/*=====================================================================
GaussianSplatRenderer.h
-----------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "OpenGLEngine.h"
#include "OpenGLProgram.h"
#include "../graphics/GaussianSplatData.h"
#include "../maths/Quat.h"
#include "../physics/jscol_aabbox.h"
#include "../utils/Platform.h"
#include "../utils/Reference.h"
#include "../utils/ThreadMessage.h"
#include "../utils/ThreadSafeQueue.h"
#include <string>
#include <vector>

namespace glare { class TaskManager; }
class GaussianSplatSortScratch; // Defined in GaussianSplatRenderer.cpp - the reusable working buffers the background depth-sort uses.


/*=====================================================================
GaussianSplatRenderer
---------------------
Renders any number of Gaussian splat clouds as a single OpenGL object - "the
world splat cloud" - drawn through the engine's normal transparent pass.

Splat clouds have to be drawn back-to-front, so they need a depth sort.  The
reason everything goes into one shared object, rather than each cloud getting
its own, is that per-object sorting can't give correct results between
objects: merging per-object sorted lists at draw time is far too slow at
realistic splat counts, and the obvious optimisation (only interleaving
objects whose bounds overlap) is simply wrong, since two objects whose 3D
bounds are disjoint can still overlap on screen.  With one shared cloud there
is no "between objects" left to get wrong.  Each cloud just owns a
[offset, count) range within the shared arrays, and positions/scales/rotations
are baked into world space when the cloud is added.

GPU representation:
 - all splat attributes (position, scale, rotation, colour and opacity) are
   packed into one RGBA32F texture, 4 texels per splat, read back with
   texelFetch() in the vertex shader.  A single packed texture is what the
   engine's custom-shader draw path allows, since it only binds
   material.albedo_texture for app-supplied shaders.
 - the geometry is one instanced quad, with draw order controlled by a
   dedicated per-splat uint32 index VBO ("splat_index_in", forced to attribute
   location 1), not by gl_InstanceID directly.
 - the texture and index VBO are grown geometrically as clouds are added, so a
   small world doesn't pay for a large allocation.  Growth reuploads
   everything; ordinary appends only upload the newly added texture rows.

The depth sort runs on a worker thread in two stages: a fast approximate
counting sort is posted first so the view updates promptly, followed by a
precise radix sort.  Splats are sorted by distance from the camera rather than
by depth along the view axis, which makes the resulting order invariant to
camera rotation, so only camera *movement* triggers a re-sort.

Concurrency: the sort worker never reads the live splat arrays, since the main
thread can reallocate them.  think() copies the positions into a snapshot
buffer on the main thread when it kicks a sort off, and the worker only reads
that.  A plain append while a sort is in flight is safe without any extra
bookkeeping, because the in-flight result only covers a prefix of the splats
and the appended tail is already in identity order.  removeObject() is the one
operation that invalidates in-flight results, since it renumbers everything -
hence structure_generation, which every result carries and which think() checks
before applying a result.

Not handled:
 - order-independent transparency.
 - non-uniform scaling of a cloud.
 - more splats than a single texture can address (see maxSupportedSplats()).
=====================================================================*/
class GaussianSplatRenderer
{
public:
	GaussianSplatRenderer();
	~GaussianSplatRenderer();

	// Identifies a splat cloud that has been added to the world cloud.  Callers keep this to later move or remove it.
	typedef uint64 Handle;
	static const Handle invalid_handle = 0;

	// Builds the shared shader program.  Call once, after the OpenGL context exists.
	void makeShaders(OpenGLEngine& opengl_engine, const std::string& shader_dir);

	// Bakes splat_data's positions/scales/rotations into world space with the given pose, and appends the result to the
	// shared world splat cloud.  Note that non-uniform scaling isn't supported.
	//
	// The first call creates the shared GLObject and adds it to the engine.  Use getWorldGLObject() to get at it - callers
	// must not add it to, or remove it from, the engine themselves.
	//
	// Callers are responsible for checking numSplatsInWorld() + splat_data->numSplats() against maxSupportedSplats()
	// first: this will otherwise just try to grow GPU storage to fit.
	Handle addObject(const GaussianSplatDataRef& splat_data, const Vec4f& translation_ws, const Quat<float>& rotation_ws,
		float uniform_scale_ws, OpenGLEngine& opengl_engine);

	// Re-bakes a cloud with a new pose and re-uploads just the affected texture rows.  Returns false if the handle isn't valid.
	bool updateObjectTransform(Handle handle, const Vec4f& translation_ws, const Quat<float>& rotation_ws, float uniform_scale_ws, OpenGLEngine& opengl_engine);

	// Removes a cloud from the world cloud.  This rebuilds and re-uploads everything, on the basis that removal is a
	// rare, user-driven event.  Returns false if the handle isn't valid.
	bool removeObject(Handle handle);

	// Removes every cloud, invalidating all outstanding handles, but keeps the shader and GPU storage so the renderer can be
	// used again immediately.  For tearing a whole world down, where removing clouds one at a time would re-upload
	// everything once per cloud.
	void removeAllObjects();

	bool isValidHandle(Handle handle) const;

	// The shared GLObject holding every splat in the world, or null if no cloud has been added yet.
	GLObjectRef getWorldGLObject() const { return world_ob; }

	// The maximum number of splats the world can hold, given the real GL_MAX_TEXTURE_SIZE (OpenGLEngine::max_texture_size).
	static size_t maxSupportedSplats(int gl_max_texture_size);

	size_t numSplatsInWorld() const { return total_splats; }
	size_t numObjectsInWorld() const { return entries.size(); }

	// Per-frame update: refreshes the viewport and focal-length uniforms the shader needs for the covariance projection,
	// applies any completed background sort, and kicks off a new sort if the camera has moved far enough.  Call once per
	// frame, after the frame's camera transform has been set on opengl_engine.
	void think(OpenGLEngine& opengl_engine, glare::TaskManager& task_manager);

	//void shutdown();

private:
	GLARE_DISABLE_COPY(GaussianSplatRenderer);

	// Grows the data texture and instance index VBO if needed_splats exceeds the current capacity.  ob_already_in_engine
	// must be false only for the very first call, made while addObject() is still building world_ob.
	void ensureGpuCapacity(size_t needed_splats, OpenGLEngine& opengl_engine, bool ob_already_in_engine);
	void rebuildVAO(); // Rebuilds world_ob->vert_vao against the current instance index VBO - needed whenever that VBO is replaced.
	void uploadTexelRowsForSplatRange(size_t first_splat, size_t num_splats_to_upload); // Repacks and re-uploads just the texture rows spanning the given splat range.
	void rebuildWorldAABB(); // Recomputes the world AABB as the union of each entry's bounds.  O(num entries), not O(num splats).
	void bakeRangeToWorldSpace(const GaussianSplatData& splat_data, size_t dest_offset, const Vec4f& translation_ws, const Quat<float>& rotation_ws,
		float uniform_scale_ws, js::AABBox& aabb_ws_out);

	Reference<OpenGLProgram> shader_prog;

	GLObjectRef world_ob; // The single persistent world splat cloud object, created lazily by the first addObject() call.  Its ob_to_world_matrix stays identity: splat positions are already in world space.
	Reference<VBO> instance_index_vbo;
	size_t gpu_capacity_splats; // Allocated capacity of the data texture and index VBO, in splats.  total_splats <= gpu_capacity_splats always.
	size_t total_splats;

	// World-space splat data for the whole world, concatenated in registration order.
	js::Vector<Vec3f, 16> world_positions;
	js::Vector<Vec3f, 16> world_scales;
	js::Vector<Vec4f, 16> world_rotations; // (x, y, z, w)
	js::Vector<Vec4f, 16> world_colours; // Never changes once added, since a cloud's pose doesn't affect its colours.

	struct WorldSplatEntry
	{
		Handle handle;
		GaussianSplatDataRef splat_data; // The original object-space data, kept so that a move can re-bake from scratch rather than accumulating error over repeated re-bakes.
		size_t offset, count; // This entry's range within the arrays above, and within the GPU texture and VBO.
		js::AABBox aabb_ws;
	};
	std::vector<WorldSplatEntry> entries;
	Handle next_handle;

	uint64 structure_generation; // Bumped by removeObject(), which renumbers everything.  Not bumped by addObject(), since a pure append leaves existing indices meaningful.

	bool sort_in_flight; // True from when a sort is kicked off until its precise result is applied.  The coarse result doesn't clear it.
	Vec4f last_sort_cam_pos_ws; // Camera position as of the last sort kicked off (not necessarily completed).
	bool have_last_sort_cam_pos;
	Reference<GaussianSplatSortScratch> sort_scratch; // Working buffers, reused across sorts.  Reference counted so an in-flight sort keeps them alive across a shutdown().

	ThreadSafeQueue<Reference<ThreadMessage> > sort_result_queue; // Written by the sort task, drained by think().

	js::Vector<Reference<ThreadMessage>, 16> completed_msgs;
};
