/*=====================================================================
GaussianSplatRenderer.h
-----------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "../graphics/GaussianSplatData.h"
#include "../maths/Quat.h"
#include "../maths/Vec4f.h"
#include "../physics/jscol_aabbox.h"
#include "../utils/Platform.h"
#include "../utils/Reference.h"
#include "../utils/ThreadMessage.h"
#include "../utils/ThreadSafeQueue.h"
#include "../utils/Vector.h"
#include <map>
#include <string>
#include <vector>


class OpenGLEngine;
class OpenGLProgram;
namespace glare { class TaskManager; }
class SplatCloud; // Defined in GaussianSplatRenderer.cpp - one drawable cloud, holding one or more splat objects.
struct CloudMember; // Defined in GaussianSplatRenderer.cpp - one registered splat object within a cloud.
class GaussianSplatSortScratch; // Defined in GaussianSplatRenderer.cpp - the reusable working buffers a background depth-sort uses.


/*=====================================================================
GaussianSplatRenderer
---------------------
Renders Gaussian splat clouds.  Owned by OpenGLEngine - get at it with
OpenGLEngine::getSplatRenderer().  Callers register a cloud with addObject()
and keep the returned Handle to later move or remove it.

Splats are semi-transparent and have to be composited back-to-front, so every
cloud carries a depth sort.  Clouds are drawn by OpenGLEngine::drawSplatClouds(),
which is a pass of its own rather than part of the alpha-blended pass; see the
comment there for why.

Partitioning
------------
Each registered object usually gets a drawable cloud of its own, which is what
makes frustum culling, cheap add/remove and a per-cloud sort budget possible.
That only works while the clouds can be correctly ordered against each other.
Two clouds with disjoint AABBs always can be: a disjoint pair is separated by an
axis-aligned plane, and every ray from the camera crosses that plane at most
once, so the cloud on the camera's side is nearer along every ray that hits
both - true even where the two overlap in screen space.

Objects whose bounds *do* intersect have no such plane, so they are merged into
one cloud, where the per-splat sort orders them against each other and there is
no cross-cloud ordering left to get wrong.  Merging is by union AABB and runs to
a fixpoint, since a merged cloud's bounds can in turn intersect a third cloud.
That over-merges in some arrangements - the union of two diagonally placed boxes
contains corners neither of them occupies - which costs culling granularity but
never correctness.  Under-merging is the direction that would break, which is
why member bounds are padded by the splat extent rather than bounding the splat
centres alone.

Within a cloud, splat data is baked into world space, so the GLObject's
ob_to_world_matrix stays identity, and each member owns a [offset, count) range
of the shared arrays.

The depth sort runs on a worker thread in two stages: a fast approximate
counting sort is posted first so the view updates promptly, followed by a
precise radix sort.  Splats are sorted by distance from the camera rather than
by depth along the view axis, which makes the resulting order invariant to
camera rotation, so only camera *movement* triggers a re-sort - and the distance
a cloud's camera must move to earn one scales with how far away the cloud is.

Concurrency: a sort worker never reads the live splat arrays, since the main
thread can reallocate them.  think() copies the positions into a snapshot buffer
on the main thread when it kicks a sort off, and the worker only reads that.  A
plain append while a sort is in flight is safe without extra bookkeeping,
because the in-flight result only covers a prefix of the splats and the appended
tail is already in identity order.  Anything that renumbers a cloud bumps its
structure_generation, which every result carries and which think() checks before
applying a result; results for a cloud that has since been merged away are
dropped by cloud id.

Not handled:
 - order-independent transparency.
 - non-uniform scaling of a cloud.
 - more splats in a single cloud than one texture can address (see
   maxSplatsPerCloud()).  A merge that would exceed it is refused, leaving the
   clouds separate and their relative order approximate.
=====================================================================*/
class GaussianSplatRenderer
{
public:
	// Doesn't touch OpenGL: the shader program is built on the first addObject() call, so an engine that never renders
	// a splat cloud doesn't pay for it.
	GaussianSplatRenderer(OpenGLEngine& opengl_engine);
	~GaussianSplatRenderer();

	// Identifies a splat cloud that has been registered.  Callers keep this to later move or remove it.
	typedef uint64 Handle;
	static const Handle invalid_handle = 0;

	// Bakes splat_data's positions/scales/rotations into world space with the given pose, and registers the result.
	// Note that non-uniform scaling isn't supported.
	//
	// Throws glare::Exception if splat_data has more splats than maxSplatsPerCloud(), or if the shader fails to build.
	Handle addObject(const GaussianSplatDataRef& splat_data, const Vec4f& translation_ws, const Quat<float>& rotation_ws, float uniform_scale_ws);

	// Re-bakes a cloud with a new pose.  Returns false if the handle isn't valid.
	bool updateObjectTransform(Handle handle, const Vec4f& translation_ws, const Quat<float>& rotation_ws, float uniform_scale_ws);

	// Returns false if the handle isn't valid.
	bool removeObject(Handle handle);

	// Removes every object, invalidating all outstanding handles.
	void removeAllObjects();

	bool isValidHandle(Handle handle) const;

	// The most splats one drawable cloud can hold, given the real GL_MAX_TEXTURE_SIZE.  Note that this is a per-cloud
	// limit, not a world-wide one: several clouds of this size can coexist, so long as they don't have to be merged.
	size_t maxSplatsPerCloud() const;

	size_t numSplatsInWorld() const;
	size_t numObjectsInWorld() const;
	size_t numDrawableClouds() const { return clouds.size(); } // How many clouds the partition has settled on.

	// Multi-line summary of the partition, the sort state and GPU/CPU memory use, for the diagnostics display.
	// Returns an empty string if no splat object is registered, so it costs nothing in a world without any.
	std::string getDiagnostics() const;

	// Per-frame update: refreshes the uniforms the shader needs for the covariance projection, applies any completed
	// background sorts, and kicks off new ones for clouds whose camera has moved far enough.  Called by
	// OpenGLEngine::draw(), after the frame's camera transform has been set.
	void think();

private:
	GLARE_DISABLE_COPY(GaussianSplatRenderer);

	void buildShadersIfNeeded();

	Reference<SplatCloud> allocCloud(); // Builds an empty cloud with its GLObject.  Not added to the engine until it holds a member - see addCloudToEngineIfNeeded().
	void addCloudToEngineIfNeeded(SplatCloud& cloud); // Adds the cloud's GLObject to the engine, once it has real bounds and an instance count for the engine to cache.
	void destroyCloud(const Reference<SplatCloud>& cloud); // Removes the cloud's GLObject from the engine and drops the cloud.

	void ensureGpuCapacity(SplatCloud& cloud, size_t needed_splats); // Grows the data texture and instance index VBO if needed.
	void rebuildVAO(SplatCloud& cloud); // Rebuilds vert_vao against the current instance index VBO - needed whenever that VBO is replaced.
	void uploadTexelRowsForSplatRange(SplatCloud& cloud, size_t first_splat, size_t num_splats_to_upload); // Repacks and re-uploads just the texture rows spanning the given splat range.
	void writeIdentityIndices(SplatCloud& cloud, size_t first_splat, size_t num_splats); // Writes an identity draw order over the given range of the instance index VBO.
	void rebuildCloudAABB(SplatCloud& cloud); // Recomputes the cloud AABB as the union of its members' bounds.  O(num members), not O(num splats).

	void appendMemberToCloud(SplatCloud& cloud, const CloudMember& member); // Fast path: bakes one member onto the tail and uploads only the affected rows.  member.offset is assigned here.
	void rebuildCloud(SplatCloud& cloud); // Re-bakes every member from its stored pose.  Used after a merge or a removal, where offsets change.
	void mergeIntersectingClouds(SplatCloud& seed_cloud); // Merges any cloud whose AABB intersects seed_cloud into it, to a fixpoint.

	void drainSortResults();
	void kickOffSorts();

	Reference<OpenGLProgram> shader_prog; // Shared by every cloud.  Null until the first addObject().

	OpenGLEngine* opengl_engine;

	std::vector<Reference<SplatCloud> > clouds;
	std::map<Handle, SplatCloud*> handle_to_cloud; // Kept in step with the member lists, since a merge moves members between clouds.
	Handle next_handle;
	uint64 next_cloud_id;

	// Sort scratch is pooled rather than per-cloud: at large splat counts these buffers run to hundreds of MB, so N
	// clouds must not mean N copies of them.  Borrowed for the duration of a sort, returned when its precise result lands.
	std::vector<Reference<GaussianSplatSortScratch> > free_scratch;
	int num_sorts_in_flight;

	ThreadSafeQueue<Reference<ThreadMessage> > sort_result_queue; // Shared by every cloud; results carry the cloud id they belong to.
	js::Vector<Reference<ThreadMessage>, 16> completed_msgs;
};
