/*=====================================================================
GaussianSplatRenderer.cpp
-------------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "GaussianSplatRenderer.h"


#include "IncludeOpenGL.h"
#include "OpenGLEngine.h"
#include "OpenGLMeshRenderData.h"
#include "OpenGLShader.h"
#include "OpenGLTexture.h"
#include "VAO.h"
#include "VBO.h"
#include "VertexBufferAllocator.h"
#include "../maths/Matrix4f.h"
#include "../maths/mathstypes.h"
#include "../utils/ArrayRef.h"
#include "../utils/BitUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/Exception.h"
#include "../utils/RefCounted.h"
#include "../utils/StringUtils.h"
#include "../utils/Sort.h"
#include "../utils/Task.h"
#include "../utils/TaskManager.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Vector.h"
#include <assert.h>
#include <cstring>
#include <limits>


// How far the camera has to move before a cloud's depth order is worth recomputing.  Camera rotation is deliberately
// not considered: the sort is by distance from the camera, which rotating on the spot doesn't change.
//
// The threshold scales with distance to the cloud.  Moving 10cm visibly reorders a cloud you are standing inside, but
// changes nothing about the order within one 500m away, and a single world-wide constant would re-sort every splat in
// the world for the latter.  The floor is that old constant, so a cloud you are close to behaves exactly as before.
static const float min_resort_move_threshold_ws = 0.1f;
static const float resort_threshold_dist_fraction = 0.05f;

// How many depth sorts may be in flight at once.  Each holds a scratch allocation proportional to its cloud, so this
// caps sort memory at roughly this many times the largest cloud, rather than letting it scale with the world.
static const int max_concurrent_sorts = 2;


// One registered splat object, and the range of its owning cloud's arrays that it occupies.
struct CloudMember
{
	GaussianSplatRenderer::Handle handle;
	GaussianSplatDataRef splat_data; // The original object-space data.  Kept so that a move, or a re-bake into a different cloud after a merge, starts from the source rather than accumulating error over repeated re-bakes.

	size_t offset, count; // This member's range within its cloud's arrays, and within its GPU texture and index VBO.

	js::AABBox aabb_ws; // Padded by the splat extent, not just bounding the splat centres - see bakeMember().

	// The pose, kept so that a merge can re-bake this member into a different cloud without the caller supplying it again.
	Vec4f translation_ws;
	Quat<float> rotation_ws;
	float uniform_scale_ws;
};


// One drawable cloud: a single GLObject holding one or more members' splats, with its own data texture, instance index
// VBO and depth sort.  Splat data is baked into world space, so ob->ob_to_world_matrix stays identity.
class SplatCloud : public RefCounted
{
public:
	SplatCloud()
	:	cloud_id(0), gpu_capacity_splats(0), total_splats(0), structure_generation(0), sort_in_flight(false),
		have_last_sort_cam_pos(false), last_sort_cam_pos_ws(0.f), aabb_ws(js::AABBox::emptyAABBox()), added_to_engine(false)
	{}

	uint64 cloud_id; // Stable, never reused.  Sort results carry it, so a result for a cloud that has since been merged away can be dropped.

	GLObjectRef ob;
	bool added_to_engine; // False between allocCloud() and the first member being baked in - see addCloudToEngineIfNeeded().
	Reference<VBO> instance_index_vbo;
	size_t gpu_capacity_splats; // Allocated capacity of the data texture and index VBO, in splats.  total_splats <= gpu_capacity_splats always.
	size_t total_splats;

	// World-space splat data for this cloud's members, concatenated in member order.
	js::Vector<Vec3f, 16> positions;
	js::Vector<Vec3f, 16> scales;
	js::Vector<Vec4f, 16> rotations; // (x, y, z, w)
	js::Vector<Vec4f, 16> colours;

	std::vector<CloudMember> members;
	js::AABBox aabb_ws; // The union of the members' padded bounds.  Doubles as the merge test and, via ob, the cull and draw-order box.

	uint64 structure_generation; // Bumped by anything that renumbers the cloud, which invalidates an in-flight sort's indices.
	bool sort_in_flight; // True from when a sort is kicked off until its precise result is applied.  The coarse result doesn't clear it.
	bool have_last_sort_cam_pos;
	Vec4f last_sort_cam_pos_ws; // Camera position as of the last sort kicked off (not necessarily completed).
};


// Reusable working buffers for the background depth-sorts.  At large splat counts these run to hundreds of MB, so
// they're pooled and reused rather than reallocated per sort.  Reference counted rather than owned outright, so that an
// in-flight sort keeps them alive even if the renderer is torn down.
class GaussianSplatSortScratch : public ThreadSafeRefCounted
{
public:
	struct SortItem
	{
		uint32 key;
		uint32 splat_index;
	};

	js::Vector<Vec3f, 16> positions_snapshot; // A frozen copy of one cloud's positions, taken on the main thread when a sort is kicked off.  The worker only ever reads this, never the live arrays.

	js::Vector<SortItem, 16> items; // Sort input, and the precise stage's output.
	js::Vector<SortItem, 16> working_space; // Scratch space for the sort routines, and the coarse stage's output.

	// The two stages' results, as instance draw orders ready for VBO::updateData().  Kept separate so the precise stage
	// can't overwrite a coarse result the main thread hasn't consumed yet.
	js::Vector<uint32, 16> coarse_indices;
	js::Vector<uint32, 16> precise_indices;

	js::Vector<uint32, 16> temp_counts; // Bucket counts for Sort::radixSort32BitKey().
};


namespace
{


const size_t texels_per_splat = 4; // See the texel layout in gaussian_splat_vert_shader.glsl.
const size_t splat_tex_width = 4096; // Gives ~16.7M splat capacity where GL_MAX_TEXTURE_SIZE >= 16384, which is common.
const int splat_index_attribute_loc = 1; // Forced in buildShadersIfNeeded().  Slot 1 is otherwise "normal_in", which splats have no use for.

const int coarse_key_bits = 16; // How many high bits of the sort key the coarse stage buckets on, i.e. 65536 evenly spaced depth slices.

// The world-space radius a splat covers, from its centre.  gaussian_splat_vert_shader.glsl cuts the splat off at 3
// sigma, so this matches what actually gets drawn.  Rotation is irrelevant: this bounds the splat in every direction.
const float splat_cutoff_sigmas = 3.f;


// How many texture rows, each splat_tex_width texels wide, are needed to hold num_splats splats.
size_t texHeightForSplatCount(size_t num_splats)
{
	return myMax<size_t>(1, Maths::roundedUpDivide(num_splats * texels_per_splat, splat_tex_width));
}


// Packs a range of splats into RGBA32F texels: 4 texels per splat.
void packSplatTexels(const js::Vector<Vec3f, 16>& positions, const js::Vector<Vec3f, 16>& scales, const js::Vector<Vec4f, 16>& rotations,
	const js::Vector<Vec4f, 16>& colours, size_t begin_splat, size_t end_splat, float* texel_data)
{
	for(size_t i=begin_splat; i<end_splat; ++i)
	{
		const Vec3f& pos   = positions[i];
		const Vec3f& scale = scales[i];
		const Vec4f& rot   = rotations[i];
		const Vec4f& col   = colours[i];

		float* const t = texel_data + (i - begin_splat) * texels_per_splat * 4;
		t[ 0] = pos.x;      t[ 1] = pos.y;      t[ 2] = pos.z;      t[ 3] = scale.x;
		t[ 4] = scale.y;    t[ 5] = scale.z;    t[ 6] = rot[0];     t[ 7] = rot[1];
		t[ 8] = rot[2];     t[ 9] = rot[3];     t[10] = col[0];     t[11] = col[1];
		t[12] = col[2];     t[13] = col[3];     // t[14] and t[15] are left as zero.
	}
}


// The instanced quad geometry: a local-space unit square.  The vertex shader scales and orients this per splat from the
// projected 2D covariance, so this only needs to bound [-1, 1] in both axes.
//
// Built per cloud rather than shared between them: the cloud's bounds live in mesh_data->aabb_os, which is what the
// engine derives GLObject::aabb_ws from, so a shared mesh would give every cloud one shared bounding box.
Reference<OpenGLMeshRenderData> makeInstancedQuadMeshData(VertexBufferAllocator& allocator)
{
	Reference<OpenGLMeshRenderData> mesh_data = new OpenGLMeshRenderData();
	mesh_data->setIndexType(GL_UNSIGNED_SHORT);
	mesh_data->has_uvs = false;
	mesh_data->has_shading_normals = false;
	mesh_data->num_materials_referenced = 1;
	// The quad's own bounds.  Only a placeholder: the owning cloud overwrites aabb_os with its world-space bounds in
	// rebuildCloudAABB(), before the object is ever handed to the engine.  It must still be a valid finite box rather
	// than emptyAABBox(), whose infinities produce NaNs when transformed (inf * 0 in transformedAABBFast()).
	mesh_data->aabb_os = js::AABBox(Vec4f(-1, -1, 0, 1), Vec4f(1, 1, 0, 1));

	mesh_data->batches.resize(1);
	mesh_data->batches[0].material_index = 0;
	mesh_data->batches[0].prim_start_offset_B = 0;
	mesh_data->batches[0].num_indices = 6;

	VertexAttrib pos_attrib;
	pos_attrib.enabled = true;
	pos_attrib.num_comps = 3;
	pos_attrib.type = GL_FLOAT;
	pos_attrib.normalised = false;
	pos_attrib.stride = (uint32)(sizeof(float) * 3);
	pos_attrib.offset = 0;
	mesh_data->vertex_spec.attributes.push_back(pos_attrib);

	// Placeholder for the per-instance splat index attribute: disabled, with no VBO, until ensureGpuCapacity() and
	// rebuildVAO() build the index VBO and rebuild the VAO with it enabled.  This mirrors how GLMeshBuilding.cpp adds
	// disabled instance-matrix attributes that GLObject::enableInstancing() enables later.
	VertexAttrib splat_index_attrib;
	splat_index_attrib.enabled = false;
	splat_index_attrib.num_comps = 1;
	splat_index_attrib.type = GL_UNSIGNED_INT;
	splat_index_attrib.normalised = false;
	splat_index_attrib.integer_attribute = true;
	splat_index_attrib.instancing = true;
	splat_index_attrib.stride = (uint32)sizeof(uint32);
	splat_index_attrib.offset = 0;
	assert(mesh_data->vertex_spec.attributes.size() == (size_t)splat_index_attribute_loc);
	mesh_data->vertex_spec.attributes.push_back(splat_index_attrib);

	const float quad_verts[4 * 3] = {
		-1, -1, 0,
		 1, -1, 0,
		 1,  1, 0,
		-1,  1, 0
	};
	const uint16 quad_indices[6] = { 0, 1, 2, 0, 2, 3 };

	allocator.allocateBufferSpaceAndVAO(*mesh_data, mesh_data->vertex_spec, quad_verts, sizeof(quad_verts), quad_indices, sizeof(quad_indices));

	return mesh_data;
}


// Result of a background depth-sort, handed back to the main thread.  think() checks cloud_id, to drop results for a
// cloud that has since been merged away or removed, and then generation, to drop results whose cloud has been
// renumbered since.
class GaussianSplatSortResultMsg : public ThreadMessage
{
public:
	enum Stage
	{
		Stage_Coarse, // Fast approximate order.  Splats sharing a depth slice are in arbitrary relative order, which is far finer than the splats themselves.
		Stage_Precise // Exact back-to-front order.
	};

	uint64 cloud_id;
	uint64 generation;
	Stage stage;
	Reference<GaussianSplatSortScratch> scratch; // Holds the result buffer, and keeps it alive even if the renderer was torn down while the sort ran.

	// Back-to-front (farthest first) instance order, ready to write into the cloud's instance_index_vbo.
	const js::Vector<uint32, 16>& sortedIndices() const { return (stage == Stage_Coarse) ? scratch->coarse_indices : scratch->precise_indices; }
};


// Sorts one cloud back-to-front by camera distance, entirely on a worker thread.  No GL calls here.
class GaussianSplatSortTask : public glare::Task
{
public:
	GaussianSplatSortTask(uint64 cloud_id_, uint64 generation_, const Reference<GaussianSplatSortScratch>& scratch_, const Matrix4f& world_to_cam_,
		ThreadSafeQueue<Reference<ThreadMessage> >* result_queue_)
	:	cloud_id(cloud_id_), generation(generation_), scratch(scratch_), world_to_cam(world_to_cam_), result_queue(result_queue_)
	{}

	virtual void run(size_t /*thread_index*/) override
	{
		typedef GaussianSplatSortScratch::SortItem SortItem;
		struct SortItemGetKey { inline uint32 operator () (const SortItem& item) const { return item.key; } };

		const js::Vector<Vec3f, 16>& positions = scratch->positions_snapshot; // The frozen snapshot, never the live arrays.
		const size_t num_splats = positions.size();

		js::Vector<SortItem, 16>& items = scratch->items;
		js::Vector<SortItem, 16>& working_space = scratch->working_space;
		items.resizeNoCopy(num_splats);
		working_space.resizeNoCopy(num_splats);

		// Pass 1: distance from the camera to each splat, and the range those distances span.  Using distance rather
		// than depth along the camera's forward axis is what makes the order invariant to camera rotation.  The distance
		// is stashed in the key field as raw bits, and pass 2 turns it into the integer key in place.
		float min_dist = std::numeric_limits<float>::max();
		float max_dist = 0;
		for(size_t i=0; i<num_splats; ++i)
		{
			const Vec3f& p = positions[i];
			const float dist = maskWToZero(world_to_cam * Vec4f(p.x, p.y, p.z, 1.f)).length(); // world_to_cam is a rigid transform, so this is the true world-space distance.
			items[i].key = bitCast<uint32>(dist);
			items[i].splat_index = (uint32)i;
			min_dist = myMin(min_dist, dist);
			max_dist = myMax(max_dist, dist);
		}

		// Pass 2: quantise those distances linearly over the whole uint32 key range, inverted so that ascending key
		// order is farthest-first, as back-to-front alpha blending needs.  A linear key, rather than the float's own bit
		// pattern (which sorts identically but spaces values by exponent), is what makes the coarse stage meaningful.
		const float dist_range = myMax(max_dist - min_dist, 1.0e-9f); // Guards the degenerate equidistant case, where every key ends up 0 anyway.
		const double key_scale = (double)std::numeric_limits<uint32>::max() / (double)dist_range;
		for(size_t i=0; i<num_splats; ++i)
			items[i].key = (uint32)((double)(max_dist - bitCast<float>(items[i].key)) * key_scale);

		// Stage 1: a single counting-sort pass over the top coarse_key_bits of the key.  Much cheaper than the precise
		// sort below, and already fine-grained enough to look right on its own, so it's posted immediately rather than
		// leaving the view in the pre-move order until the precise sort finishes.
		{
			struct CoarseBucketChooser { inline size_t operator () (const SortItem& item) const { return item.key >> (32 - coarse_key_bits); } };
			Sort::serialCountingSortWithNumBuckets(items.data(), working_space.data(), num_splats, (size_t)1 << coarse_key_bits, CoarseBucketChooser());

			scratch->coarse_indices.resizeNoCopy(num_splats);
			for(size_t i=0; i<num_splats; ++i)
				scratch->coarse_indices[i] = working_space[i].splat_index;

			enqueueResult(GaussianSplatSortResultMsg::Stage_Coarse);
		}

		// Stage 2: the precise sort.  Stage 1 only wrote to working_space, which radixSort32BitKey() treats as scratch
		// anyway, so items is still in its original order here.
		scratch->temp_counts.resizeNoCopy(6144); // The size Sort::radixSort32BitKey() requires.
		Sort::radixSort32BitKey(items.data(), working_space.data(), num_splats, SortItemGetKey(), scratch->temp_counts.data(), scratch->temp_counts.size());

		scratch->precise_indices.resizeNoCopy(num_splats);
		for(size_t i=0; i<num_splats; ++i)
			scratch->precise_indices[i] = items[i].splat_index;

		enqueueResult(GaussianSplatSortResultMsg::Stage_Precise);
	}

private:
	void enqueueResult(GaussianSplatSortResultMsg::Stage stage)
	{
		Reference<GaussianSplatSortResultMsg> msg = new GaussianSplatSortResultMsg();
		msg->cloud_id = cloud_id;
		msg->generation = generation;
		msg->stage = stage;
		msg->scratch = scratch;
		result_queue->enqueue(msg);
	}

	uint64 cloud_id;
	uint64 generation;
	Reference<GaussianSplatSortScratch> scratch; // Keeps the snapshot and working buffers alive for the duration of the task.
	Matrix4f world_to_cam;
	ThreadSafeQueue<Reference<ThreadMessage> >* result_queue;
};


} // end anonymous namespace


GaussianSplatRenderer::GaussianSplatRenderer(OpenGLEngine& opengl_engine_)
:	opengl_engine(&opengl_engine_), next_handle(1), next_cloud_id(1), num_sorts_in_flight(0)
{}


GaussianSplatRenderer::~GaussianSplatRenderer()
{}


void GaussianSplatRenderer::buildShadersIfNeeded()
{
	if(shader_prog.nonNull())
		return;

	const std::string shader_dir           = opengl_engine->getShadersDir();
	const std::string version_directive    = opengl_engine->getVersionDirective();
	const std::string preprocessor_defines = opengl_engine->getPreprocessorDefines();

	// wait_for_build_to_complete is false because we need to bind our custom attribute location and relink before the
	// program is finished - see below.
	shader_prog = new OpenGLProgram(
		"gaussian splat prog",
		new OpenGLShader(shader_dir + "/gaussian_splat_vert_shader.glsl", version_directive, preprocessor_defines, GL_VERTEX_SHADER),
		new OpenGLShader(shader_dir + "/gaussian_splat_frag_shader.glsl", version_directive, preprocessor_defines, GL_FRAGMENT_SHADER),
		opengl_engine->getAndIncrNextProgramIndex(),
		/*wait_for_build_to_complete=*/false
	);

	// OpenGLProgram's constructor links the program once, binding the engine's standard attribute names.  Our
	// "splat_index_in" attribute isn't one of those, so at this point it's at whatever location the driver picked.
	// Force it to a known location and relink, so rebuildVAO() can build the instance attribute at a location we know.
	shader_prog->bindAttributeLocation(splat_index_attribute_loc, "splat_index_in");
	glLinkProgram(shader_prog->program);
	shader_prog->forceFinishLinkAndDoPostLinkCode(); // Throws glare::Exception if the relink failed.

	opengl_engine->addProgram(shader_prog);

	shader_prog->appendUserUniformInfo(UserUniformInfo::UniformType_Vec2, "viewport_dims_px");
	shader_prog->appendUserUniformInfo(UserUniformInfo::UniformType_Vec2, "focal_len_px");
	shader_prog->appendUserUniformInfo(UserUniformInfo::UniformType_Int,  "splat_tex_width");


	// Splats blend into an accumulation buffer of their own rather than straight onto the main colour buffer, so that
	// the blend happens in the display-referred space 3DGS fits them in; this program does the full-viewport pass that
	// resolves that buffer and composites it.  See OpenGLEngine::drawSplatClouds().  It has no custom attributes, so
	// unlike shader_prog above it can just be built to completion in one go.
	resolve_prog = new OpenGLProgram(
		"gaussian splat resolve prog",
		new OpenGLShader(shader_dir + "/gaussian_splat_resolve_vert_shader.glsl", version_directive, preprocessor_defines, GL_VERTEX_SHADER),
		new OpenGLShader(shader_dir + "/gaussian_splat_resolve_frag_shader.glsl", version_directive, preprocessor_defines, GL_FRAGMENT_SHADER),
		opengl_engine->getAndIncrNextProgramIndex(),
		/*wait_for_build_to_complete=*/true
	);
	opengl_engine->addProgram(resolve_prog);
	assert(resolve_prog->albedo_texture_loc >= 0);
}


size_t GaussianSplatRenderer::maxSplatsPerCloud() const
{
	// The texture width is fixed at splat_tex_width, which any conformant implementation supports.  The height is capped
	// at the real driver limit rather than the guaranteed minimum, since real hardware commonly supports far more.
	const size_t max_tex_h = (size_t)myMax(1, opengl_engine->max_texture_size);
	return (splat_tex_width * max_tex_h) / texels_per_splat;
}


size_t GaussianSplatRenderer::numSplatsInWorld() const
{
	size_t num = 0;
	for(size_t i=0; i<clouds.size(); ++i)
		num += clouds[i]->total_splats;
	return num;
}


size_t GaussianSplatRenderer::numObjectsInWorld() const
{
	return handle_to_cloud.size();
}


bool GaussianSplatRenderer::isValidHandle(Handle handle) const
{
	return handle_to_cloud.count(handle) != 0;
}


std::string GaussianSplatRenderer::getDiagnostics() const
{
	if(handle_to_cloud.empty() && clouds.empty())
		return std::string();

	size_t num_merged_clouds = 0, largest_cloud_splats = 0, total_splats = 0;
	uint64 tex_bytes = 0, index_vbo_bytes = 0;
	for(size_t i=0; i<clouds.size(); ++i)
	{
		const SplatCloud& cloud = *clouds[i];
		total_splats += cloud.total_splats;
		largest_cloud_splats = myMax(largest_cloud_splats, cloud.total_splats);
		if(cloud.members.size() > 1)
			num_merged_clouds++;

		// Both are sized to the cloud's capacity, not its splat count: 4 RGBA32F texels and one uint32 index per splat.
		tex_bytes       += (uint64)cloud.gpu_capacity_splats * texels_per_splat * 4 * sizeof(float);
		index_vbo_bytes += (uint64)cloud.gpu_capacity_splats * sizeof(uint32);
	}

	uint64 scratch_bytes = 0;
	for(size_t i=0; i<free_scratch.size(); ++i)
	{
		const GaussianSplatSortScratch& s = *free_scratch[i];
		scratch_bytes += (uint64)(s.positions_snapshot.size() * sizeof(Vec3f) + s.items.size() * sizeof(GaussianSplatSortScratch::SortItem) +
			s.working_space.size() * sizeof(GaussianSplatSortScratch::SortItem) + s.coarse_indices.size() * sizeof(uint32) +
			s.precise_indices.size() * sizeof(uint32) + s.temp_counts.size() * sizeof(uint32));
	}

	std::string s;
	s += "Splat objects: " + toString(handle_to_cloud.size()) + "\n";
	s += "Drawable clouds: " + toString(clouds.size()) + " (" + toString(num_merged_clouds) + " merged)\n";
	s += "Clouds drawn last frame: " + toString(opengl_engine->last_num_splat_clouds_drawn) + "\n";
	s += "Splats: " + uInt64ToStringCommaSeparated(total_splats) + " total, " + uInt64ToStringCommaSeparated(largest_cloud_splats) + " in largest cloud\n";
	s += "Splats drawn last frame: " + uInt64ToStringCommaSeparated(opengl_engine->last_num_splats_drawn) + "\n";
	s += "Sorts in flight: " + toString(num_sorts_in_flight) + " / " + toString(max_concurrent_sorts) + "\n";
	s += "GPU mem: " + getMBSizeString((size_t)tex_bytes) + " data textures, " + getMBSizeString((size_t)index_vbo_bytes) + " index VBOs\n";
	s += "Sort scratch pooled: " + toString(free_scratch.size()) + " buffers, " + getMBSizeString((size_t)scratch_bytes) + "\n";

	// The per-cloud breakdown is what shows whether the partitioning is behaving - a world of separate captures should
	// show one member per cloud.  Capped, since a world could hold many.
	const size_t max_clouds_to_list = 8;
	for(size_t i=0; i<myMin(clouds.size(), max_clouds_to_list); ++i)
	{
		const SplatCloud& cloud = *clouds[i];
		s += "  cloud " + toString(cloud.cloud_id) + ": " + toString(cloud.members.size()) + (cloud.members.size() == 1 ? " member, " : " members, ") +
			uInt64ToStringCommaSeparated(cloud.total_splats) + " splats" + (cloud.sort_in_flight ? ", sorting" : "") + "\n";
	}
	if(clouds.size() > max_clouds_to_list)
		s += "  (" + toString(clouds.size() - max_clouds_to_list) + " more)\n";

	return s;
}


Reference<SplatCloud> GaussianSplatRenderer::allocCloud()
{
	Reference<SplatCloud> cloud = new SplatCloud();
	cloud->cloud_id = next_cloud_id++;

	cloud->ob = new GLObject();
	cloud->ob->ob_to_world_matrix = Matrix4f::identity(); // Never changes: splat positions are baked into world space.
	cloud->ob->mesh_data = makeInstancedQuadMeshData(*opengl_engine->vert_buf_allocator);
	cloud->ob->num_instances_to_draw = 0;

	cloud->ob->materials.resize(1);
	OpenGLMaterial& mat = cloud->ob->materials[0];
	mat.shader_prog = shader_prog;
	mat.auto_assign_shader = false;

	// Route the object through OpenGLEngine::drawSplatClouds(), which orders whole clouds back-to-front against each
	// other.  Neither of the engine's general-purpose passes will do: the transparent pass uses order-independent
	// transparency on native, which accumulates colour additively and so has no notion of one splat occluding another -
	// fine for a few glass surfaces, but splat clouds put hundreds of overlapping splats on every pixel, where it
	// saturates to white.  The alpha-blended pass does blend back-to-front, but orders objects by distance to their
	// AABB, which isn't an exact ordering for clouds that overlap in screen space.
	mat.splat_cloud = true;

	// Also set alpha_blend, even though drawAlphaBlendedObjects() won't draw this object: the flag is what sets
	// MATERIAL_ALPHA_BLEND_BITFLAG on the batch, which is how the opaque pass, the depth pre-pass and the shadow passes
	// know to skip it.  OpenGLEngine::addObject() keys off splat_cloud to put the object in exactly one of the two sets.
	mat.alpha_blend = true;
	mat.user_uniform_vals.resize(3); // viewport_dims_px and focal_len_px are set by think().
	mat.user_uniform_vals[2].intval = (int)splat_tex_width;

	// Build a real (if minimal) texture and VAO up front: adding the object to the engine before it has those would
	// leave the engine caching a stale, non-instanced VAO reference that a later VAO rebuild can't retroactively fix.
	ensureGpuCapacity(*cloud, /*needed_splats=*/0);

	// Deliberately not added to the engine here.  An empty cloud has no meaningful bounds, and handing the engine an
	// object whose aabb_os is still the placeholder would have it compute and cache a bogus aabb_ws.  The first member
	// to be baked in gives the cloud real bounds and an instance count, and adds it - see addCloudToEngineIfNeeded().
	clouds.push_back(cloud);
	return cloud;
}


void GaussianSplatRenderer::addCloudToEngineIfNeeded(SplatCloud& cloud)
{
	if(cloud.added_to_engine)
		return;

	assert(cloud.total_splats > 0); // Otherwise the bounds are still the placeholder.
	opengl_engine->addObject(cloud.ob); // Computes aabb_ws from the bounds set by rebuildCloudAABB().
	cloud.added_to_engine = true;
}


void GaussianSplatRenderer::destroyCloud(const Reference<SplatCloud>& cloud)
{
	if(cloud->added_to_engine)
		opengl_engine->removeObject(cloud->ob);

	for(size_t i=0; i<clouds.size(); ++i)
		if(clouds[i].ptr() == cloud.ptr())
		{
			clouds.erase(clouds.begin() + i);
			break;
		}

	// Any sort in flight for this cloud is left to run.  Its result carries the cloud id, which no longer matches a
	// live cloud, so drainSortResults() drops it and returns the scratch to the pool.
}


void GaussianSplatRenderer::rebuildVAO(SplatCloud& cloud)
{
	VertexSpec vertex_spec = cloud.ob->mesh_data->vertex_spec;
	vertex_spec.attributes[splat_index_attribute_loc].vbo = cloud.instance_index_vbo;
	vertex_spec.attributes[splat_index_attribute_loc].enabled = true;
#if DO_INDIVIDUAL_VAO_ALLOC
	cloud.ob->vert_vao = new VAO(cloud.ob->mesh_data->vbo_handle.vbo, cloud.ob->mesh_data->indices_vbo_handle.index_vbo, vertex_spec);
#else
	cloud.ob->vert_vao = new VAO(vertex_spec);
#endif
	// Bind the index VBO as the object's per-instance buffer.  Note that on the shared-VAO path (i.e. everywhere except
	// Mac and Emscripten) the VertexAttrib::vbo set above is ignored, and the engine instead binds instance_matrix_vbo
	// to binding point 1 at draw time - hence instance_vbo_stride_B, without which the engine would assume the stride of
	// an instance matrix rather than of our uint32 indices.
	cloud.ob->instance_matrix_vbo = cloud.instance_index_vbo;
	cloud.ob->instance_vbo_stride_B = (uint32)sizeof(uint32);
}


void GaussianSplatRenderer::uploadTexelRowsForSplatRange(SplatCloud& cloud, size_t first_splat, size_t num_splats_to_upload)
{
	if(num_splats_to_upload == 0)
		return;

	// Repack whole texture rows spanning the given splat range.  An arbitrary range doesn't align to row boundaries, so
	// this may re-pack a few splats belonging to a neighbouring member too.  That's harmless: their data is already
	// correct, and we just re-derive the same texels for them.
	const size_t first_texel     = first_splat * texels_per_splat;
	const size_t last_texel_excl = (first_splat + num_splats_to_upload) * texels_per_splat;
	const size_t start_row       = first_texel / splat_tex_width;
	const size_t end_row         = Maths::roundedUpDivide(last_texel_excl, splat_tex_width); // Exclusive.
	const size_t num_rows        = end_row - start_row;

	const size_t row_start_splat    = (start_row * splat_tex_width) / texels_per_splat;
	const size_t row_end_splat_excl = myMin(cloud.total_splats, (end_row * splat_tex_width) / texels_per_splat);

	js::Vector<float, 16> texel_data(splat_tex_width * num_rows * 4, 0.f);
	packSplatTexels(cloud.positions, cloud.scales, cloud.rotations, cloud.colours, row_start_splat, row_end_splat_excl, texel_data.data());

	cloud.ob->materials[0].albedo_texture->loadRegionIntoExistingTexture(/*mipmap_level=*/0, /*x=*/0, /*y=*/start_row, /*z=*/0,
		/*region_w=*/splat_tex_width, /*region_h=*/num_rows, /*region_d=*/1, /*src_row_stride_B=*/splat_tex_width * 4 * sizeof(float),
		ArrayRef<uint8>((const uint8*)texel_data.data(), texel_data.size() * sizeof(float)), /*bind_needed=*/true);
}


void GaussianSplatRenderer::writeIdentityIndices(SplatCloud& cloud, size_t first_splat, size_t num_splats)
{
	if(num_splats == 0)
		return;

	js::Vector<uint32, 16> indices(num_splats);
	for(size_t i=0; i<num_splats; ++i)
		indices[i] = (uint32)(first_splat + i);
	cloud.instance_index_vbo->updateData(first_splat * sizeof(uint32), indices.data(), indices.size() * sizeof(uint32));
}


void GaussianSplatRenderer::ensureGpuCapacity(SplatCloud& cloud, size_t needed_splats)
{
	// The albedo_texture check covers the case of a newly allocated cloud, where needed_splats (0) <=
	// gpu_capacity_splats (0) would otherwise skip creating a texture at all.
	if(needed_splats <= cloud.gpu_capacity_splats && cloud.ob->materials[0].albedo_texture.nonNull())
		return;

	size_t new_tex_h = texHeightForSplatCount(needed_splats);
	const size_t cur_tex_h = texHeightForSplatCount(myMax<size_t>(1, cloud.gpu_capacity_splats));
	new_tex_h = myMax(new_tex_h, cur_tex_h * 2); // Geometric growth, so repeated small appends don't reallocate every time.

	const size_t new_capacity_splats = (splat_tex_width * new_tex_h) / texels_per_splat;

	// Repack every splat into a fresh, bigger texture.  Growth is rare, so this cost isn't paid on every append.
	js::Vector<float, 16> texel_data(splat_tex_width * new_tex_h * 4, 0.f);
	packSplatTexels(cloud.positions, cloud.scales, cloud.rotations, cloud.colours, 0, cloud.total_splats, texel_data.data());

	cloud.ob->materials[0].albedo_texture = new OpenGLTexture(splat_tex_width, new_tex_h, opengl_engine,
		ArrayRef<uint8>((const uint8*)texel_data.data(), texel_data.size() * sizeof(float)),
		OpenGLTextureFormat::Format_RGBA_Linear_Float,
		OpenGLTexture::Filtering_Nearest, // Must be Nearest: this is a data texture, and filtering would blend unrelated splats' attributes together.
		OpenGLTexture::Wrapping_Clamp,
		/*has_mipmaps=*/false);

	// Grow the index VBO to match, in identity order.  Any in-flight sort's result will simply be reapplied, or dropped
	// if the cloud has also been renumbered, once it lands.
	js::Vector<uint32, 16> identity_indices(new_capacity_splats);
	for(size_t i=0; i<new_capacity_splats; ++i)
		identity_indices[i] = (uint32)i;
	cloud.instance_index_vbo = new VBO(identity_indices.data(), identity_indices.size() * sizeof(uint32), GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);

	rebuildVAO(cloud);

	// vert_vao was just replaced.  If the object is already in the engine, the engine cached a draw-time VAO reference
	// when it was added that is now stale, and would keep drawing the old, freed VAO.  objectMaterialsUpdated()
	// recomputes it from the current vert_vao.  It must not be called before the object has been through
	// OpenGLEngine::addObject()'s buildObjectData(), which is why the flag is checked rather than assumed.
	if(cloud.added_to_engine)
		opengl_engine->objectMaterialsUpdated(*cloud.ob);

	cloud.gpu_capacity_splats = new_capacity_splats;
}


// Bakes member.splat_data into cloud's arrays at member.offset, using member's stored pose, and sets member.aabb_ws.
//
// The bounds are grown by each splat's own radius, rather than just holding the splat centres.  A splat is drawn as a
// quad extending well beyond its centre, so centre-only bounds would let two clouds whose bounds are marginally
// disjoint still have their fringe splats interpenetrating - and the partitioning would then leave them in separate
// clouds with no separating plane between them, which is the one failure that produces a wrong compositing order.
static void bakeMember(SplatCloud& cloud, CloudMember& member)
{
	const GaussianSplatData& splat_data = *member.splat_data;
	const Vec4f translation_ws = member.translation_ws;
	const Quat<float>& rotation_ws = member.rotation_ws;
	const float uniform_scale_ws = member.uniform_scale_ws;

	js::AABBox aabb_ws = js::AABBox::emptyAABBox();

	const size_t num_splats = splat_data.numSplats();
	for(size_t i=0; i<num_splats; ++i)
	{
		const Vec3f& os_pos   = splat_data.positions[i];
		const Vec3f& os_scale = splat_data.scales[i];
		const Vec4f& os_rot   = splat_data.rotations[i]; // (x, y, z, w)

		const Vec4f rotated = rotation_ws.rotateVector(Vec4f(uniform_scale_ws * os_pos.x, uniform_scale_ws * os_pos.y, uniform_scale_ws * os_pos.z, 0.f));
		const Vec4f world_pos = translation_ws + rotated; // translation_ws.w == 1 and rotated.w == 0, so world_pos.w == 1, as a point should be.

		const Quat<float> os_quat(os_rot[0], os_rot[1], os_rot[2], os_rot[3]);
		const Quat<float> world_quat = rotation_ws * os_quat;

		const Vec3f world_scale = os_scale * uniform_scale_ws;

		const size_t dest = member.offset + i;
		cloud.positions[dest] = toVec3f(world_pos);
		cloud.scales   [dest] = world_scale;
		cloud.rotations[dest] = world_quat.v; // Quat::v is already (x, y, z, w), matching our storage convention.
		cloud.colours  [dest] = splat_data.colours[i]; // Colour and opacity aren't affected by the cloud's pose, but re-deriving them keeps this the single place a member's data is written.

		const float radius = splat_cutoff_sigmas * myMax(world_scale.x, myMax(world_scale.y, world_scale.z));
		aabb_ws.enlargeToHoldPoint(world_pos - Vec4f(radius, radius, radius, 0.f));
		aabb_ws.enlargeToHoldPoint(world_pos + Vec4f(radius, radius, radius, 0.f));
	}

	member.aabb_ws = aabb_ws;
}


void GaussianSplatRenderer::rebuildCloudAABB(SplatCloud& cloud)
{
	js::AABBox aabb = js::AABBox::emptyAABBox();
	for(size_t m=0; m<cloud.members.size(); ++m)
		aabb.enlargeToHoldAABBox(cloud.members[m].aabb_ws);

	cloud.aabb_ws = aabb;
	cloud.ob->mesh_data->aabb_os = aabb; // ob_to_world_matrix is identity, so object space is world space here.

	// Refreshes ob->aabb_ws, which drawSplatClouds() culls and orders on.  Skipped for a cloud that hasn't been added to
	// the engine yet: OpenGLEngine::addObject() derives aabb_ws itself, from the bounds just set above.
	if(cloud.added_to_engine)
		opengl_engine->updateObjectTransformData(*cloud.ob);
}


void GaussianSplatRenderer::appendMemberToCloud(SplatCloud& cloud, const CloudMember& member_in)
{
	const size_t old_total = cloud.total_splats;
	const size_t new_total = old_total + member_in.count;

	cloud.positions.resize(new_total);
	cloud.scales   .resize(new_total);
	cloud.rotations.resize(new_total);
	cloud.colours  .resize(new_total);

	cloud.members.push_back(member_in);
	CloudMember& member = cloud.members.back();
	member.offset = old_total;
	bakeMember(cloud, member);

	cloud.total_splats = new_total;
	cloud.ob->num_instances_to_draw = (int)new_total;

	ensureGpuCapacity(cloud, new_total);

	uploadTexelRowsForSplatRange(cloud, old_total, member_in.count); // If ensureGpuCapacity() just repacked everything, this re-uploads the same correct data, which is harmless.
	writeIdentityIndices(cloud, old_total, member_in.count); // Again redundant but harmless if ensureGpuCapacity() just rebuilt the whole VBO.

	rebuildCloudAABB(cloud);
	addCloudToEngineIfNeeded(cloud); // For the first member: only now does the cloud have real bounds and an instance count for the engine to cache.

	// The appended splats are in identity order relative to the rest, so the cloud needs a fresh sort.  This doesn't
	// bump structure_generation: a pure append leaves existing indices meaningful, so an in-flight sort's result still
	// applies to the prefix it covers.
	cloud.have_last_sort_cam_pos = false;
}


void GaussianSplatRenderer::rebuildCloud(SplatCloud& cloud)
{
	size_t total = 0;
	for(size_t m=0; m<cloud.members.size(); ++m)
	{
		cloud.members[m].offset = total;
		total += cloud.members[m].count;
	}

	cloud.total_splats = total;
	cloud.positions.resize(total);
	cloud.scales   .resize(total);
	cloud.rotations.resize(total);
	cloud.colours  .resize(total);

	for(size_t m=0; m<cloud.members.size(); ++m)
		bakeMember(cloud, cloud.members[m]);

	cloud.ob->num_instances_to_draw = (int)total;

	ensureGpuCapacity(cloud, total);

	uploadTexelRowsForSplatRange(cloud, 0, total);
	writeIdentityIndices(cloud, 0, total);

	rebuildCloudAABB(cloud);
	addCloudToEngineIfNeeded(cloud);

	cloud.structure_generation++; // Every member was renumbered, so any in-flight sort's indices no longer mean the same splats.
	cloud.have_last_sort_cam_pos = false;
}


void GaussianSplatRenderer::mergeIntersectingClouds(SplatCloud& seed_cloud)
{
	const size_t max_splats = maxSplatsPerCloud();

	size_t merged_total = seed_cloud.total_splats;
	bool merged_any = false;
	bool changed = true;
	while(changed)
	{
		changed = false;

		// Absorbing a cloud grows seed_cloud's bounds, which can bring a cloud that was previously disjoint - including
		// one already passed over in this scan - into contact.  Hence the outer loop: this runs to a fixpoint.
		for(size_t i=0; i<clouds.size(); )
		{
			SplatCloud* const other = clouds[i].ptr();
			if(other == &seed_cloud || !seed_cloud.aabb_ws.intersectsAABB(other->aabb_ws))
			{
				++i;
				continue;
			}

			if(merged_total + other->total_splats > max_splats)
			{
				// The merged cloud wouldn't fit in one data texture.  Leave the two separate: their relative draw order
				// becomes approximate, which is a far better failure than not rendering one of them at all.
				conPrint("GaussianSplatRenderer: not merging two intersecting splat clouds, as the result would exceed the " +
					toString(max_splats) + " splat per-cloud limit.  Their relative draw order will be approximate.");
				++i;
				continue;
			}

			for(size_t m=0; m<other->members.size(); ++m)
			{
				seed_cloud.members.push_back(other->members[m]);
				handle_to_cloud[other->members[m].handle] = &seed_cloud;
			}
			merged_total += other->total_splats;

			// Union the bounds now rather than waiting for the rebuild below, since the fixpoint test above needs them.
			seed_cloud.aabb_ws.enlargeToHoldAABBox(other->aabb_ws);

			const Reference<SplatCloud> other_ref = clouds[i]; // Keep alive across the erase inside destroyCloud().
			destroyCloud(other_ref);

			merged_any = true;
			changed = true;
			// Don't advance i: clouds[] shifted down over the erased entry.
		}
	}

	// One rebuild for the whole merge, rather than one per absorbed cloud.
	if(merged_any)
		rebuildCloud(seed_cloud);
}


GaussianSplatRenderer::Handle GaussianSplatRenderer::addObject(const GaussianSplatDataRef& splat_data, const Vec4f& translation_ws,
	const Quat<float>& rotation_ws, float uniform_scale_ws)
{
	buildShadersIfNeeded();

	const size_t max_splats = maxSplatsPerCloud();
	if(splat_data->numSplats() > max_splats)
		throw glare::Exception("Can't render a splat cloud with " + toString(splat_data->numSplats()) + " splats: the per-cloud limit is " + toString(max_splats) + ".");

	CloudMember member;
	member.handle = next_handle++;
	member.splat_data = splat_data;
	member.offset = 0; // Assigned by appendMemberToCloud().
	member.count = splat_data->numSplats();
	member.translation_ws = translation_ws;
	member.rotation_ws = rotation_ws;
	member.uniform_scale_ws = uniform_scale_ws;

	// Start the member in a cloud of its own and then let the partitioning merge it, rather than deciding up front
	// which cloud it belongs in.  Baking is what produces the member's bounds, and the bounds are what the merge test
	// needs, so this way the common case - a capture that intersects nothing - bakes exactly once.
	Reference<SplatCloud> cloud = allocCloud();
	appendMemberToCloud(*cloud, member);
	handle_to_cloud[member.handle] = cloud.ptr();

	mergeIntersectingClouds(*cloud); // Absorbs any cloud this one now touches.  cloud itself always survives.

	return member.handle;
}


bool GaussianSplatRenderer::updateObjectTransform(Handle handle, const Vec4f& translation_ws, const Quat<float>& rotation_ws, float uniform_scale_ws)
{
	const std::map<Handle, SplatCloud*>::iterator res = handle_to_cloud.find(handle);
	if(res == handle_to_cloud.end())
		return false;

	SplatCloud& cloud = *res->second;

	for(size_t m=0; m<cloud.members.size(); ++m)
	{
		CloudMember& member = cloud.members[m];
		if(member.handle == handle)
		{
			member.translation_ws = translation_ws;
			member.rotation_ws = rotation_ws;
			member.uniform_scale_ws = uniform_scale_ws;

			bakeMember(cloud, member); // Re-bakes in place: offsets and counts are unchanged.
			uploadTexelRowsForSplatRange(cloud, member.offset, member.count);
			rebuildCloudAABB(cloud);

			// This member's splats may now be in the wrong depth order relative to the rest of the cloud.  This doesn't
			// bump structure_generation: offsets and counts are unchanged, so an in-flight sort's indices stay meaningful.
			cloud.have_last_sort_cam_pos = false;

			// The cloud's bounds have moved, so it may now touch clouds it didn't before.  Note that the reverse is not
			// checked: a cloud is never split back apart once merged.  An over-merged cloud draws correctly, just with
			// coarser culling, whereas splitting one that shouldn't be split is what breaks the ordering.
			mergeIntersectingClouds(cloud);

			return true;
		}
	}

	assert(0); // handle_to_cloud pointed at a cloud that doesn't hold this member.
	return false;
}


bool GaussianSplatRenderer::removeObject(Handle handle)
{
	const std::map<Handle, SplatCloud*>::iterator res = handle_to_cloud.find(handle);
	if(res == handle_to_cloud.end())
		return false;

	SplatCloud* const cloud = res->second;
	handle_to_cloud.erase(res);

	for(size_t m=0; m<cloud->members.size(); ++m)
		if(cloud->members[m].handle == handle)
		{
			cloud->members.erase(cloud->members.begin() + m);
			break;
		}

	if(cloud->members.empty())
	{
		// The common case, since most objects end up in a cloud of their own.
		Reference<SplatCloud> cloud_ref = cloud; // Keep alive across the erase inside destroyCloud().
		destroyCloud(cloud_ref);
	}
	else
		rebuildCloud(*cloud); // Removing a member renumbers everything after it, so the whole cloud is re-baked and re-uploaded.

	return true;
}


void GaussianSplatRenderer::removeAllObjects()
{
	for(size_t i=0; i<clouds.size(); ++i)
		opengl_engine->removeObject(clouds[i]->ob);

	clouds.clear();
	handle_to_cloud.clear();

	// Any sorts still in flight are left to run: their results carry cloud ids that no longer match a live cloud, so
	// drainSortResults() drops them and returns their scratch to the pool.
}


void GaussianSplatRenderer::drainSortResults()
{
	sort_result_queue.dequeueAnyQueuedItems(completed_msgs);

	for(size_t i=0; i<completed_msgs.size(); ++i)
	{
		const GaussianSplatSortResultMsg* const msg = static_cast<const GaussianSplatSortResultMsg*>(completed_msgs[i].ptr());

		SplatCloud* cloud = NULL;
		for(size_t c=0; c<clouds.size(); ++c)
			if(clouds[c]->cloud_id == msg->cloud_id)
			{
				cloud = clouds[c].ptr();
				break;
			}

		if(msg->stage == GaussianSplatSortResultMsg::Stage_Precise)
		{
			// Bookkeeping first, and unconditionally: the sort has finished and its scratch is free to reuse whether or
			// not the cloud it was for still exists.
			num_sorts_in_flight--;
			free_scratch.push_back(msg->scratch);
			if(cloud)
				cloud->sort_in_flight = false;
		}

		// Drop results for a cloud that has since been merged away or removed, and results computed before a
		// renumbering, whose indices no longer mean the same splats.
		if(!cloud || msg->generation != cloud->structure_generation)
			continue;

		// If a later message in this same batch is for the same cloud and generation, that one supersedes this one.
		// Both stages can land in the same frame on a fast-sorting cloud, and uploading the coarse order only to
		// overwrite it with the precise order in the same frame is a pointless buffer upload.
		bool superseded_this_frame = false;
		for(size_t j=i + 1; j<completed_msgs.size(); ++j)
		{
			const GaussianSplatSortResultMsg* const later = static_cast<const GaussianSplatSortResultMsg*>(completed_msgs[j].ptr());
			if(later->cloud_id == msg->cloud_id && later->generation == msg->generation)
			{
				superseded_this_frame = true;
				break;
			}
		}

		if(!superseded_this_frame)
		{
			// The snapshot this was computed from may be a strict prefix of the current, possibly since-grown cloud.
			// Only write as many bytes as the result actually covers: any appended tail beyond it already holds valid
			// identity-order indices written by appendMemberToCloud().
			const js::Vector<uint32, 16>& sorted_indices = msg->sortedIndices();
			cloud->instance_index_vbo->updateData(0, sorted_indices.data(), sorted_indices.size() * sizeof(uint32));
		}
	}

	completed_msgs.clear(); // Drop the references, so a scratch just returned to the pool isn't kept alive by a stale message.
}


void GaussianSplatRenderer::kickOffSorts()
{
	glare::TaskManager* const task_manager = opengl_engine->getMainTaskManager();
	if(task_manager == NULL)
		return;

	const OpenGLScene* const scene = opengl_engine->getCurrentScene();
	const Vec4f cam_pos_ws = scene->cam_to_world.getColumn(3);

	while(num_sorts_in_flight < max_concurrent_sorts)
	{
		// Pick the cloud most overdue for a sort, measured as how far the camera has moved relative to that cloud's own
		// threshold.  With a cap on concurrent sorts, a world of many clouds would otherwise service them in an
		// arbitrary order, and the nearby cloud whose order actually looks wrong could be starved by distant ones.
		SplatCloud* best_cloud = NULL;
		float best_ratio = 1.f; // A cloud has to be past its own threshold to be worth sorting at all.
		for(size_t i=0; i<clouds.size(); ++i)
		{
			SplatCloud* const cloud = clouds[i].ptr();
			if(cloud->total_splats == 0 || cloud->sort_in_flight)
				continue;

			float ratio;
			if(!cloud->have_last_sort_cam_pos)
				ratio = std::numeric_limits<float>::max(); // Never sorted, or invalidated by a change to the cloud.
			else
			{
				const float threshold = myMax(min_resort_move_threshold_ws, cloud->aabb_ws.distanceToPoint(cam_pos_ws) * resort_threshold_dist_fraction);
				ratio = cam_pos_ws.getDist(cloud->last_sort_cam_pos_ws) / threshold;
			}

			if(ratio > best_ratio)
			{
				best_ratio = ratio;
				best_cloud = cloud;
			}
		}

		if(best_cloud == NULL)
			break;

		Reference<GaussianSplatSortScratch> scratch;
		if(free_scratch.empty())
			scratch = new GaussianSplatSortScratch();
		else
		{
			scratch = free_scratch.back();
			free_scratch.pop_back();
		}

		// Freeze a snapshot of the cloud's current positions on the main thread before handing off to the worker, so the
		// worker never touches the live, growable arrays.
		scratch->positions_snapshot.resizeNoCopy(best_cloud->total_splats);
		std::memcpy(scratch->positions_snapshot.data(), best_cloud->positions.data(), best_cloud->total_splats * sizeof(Vec3f));

		best_cloud->sort_in_flight = true;
		best_cloud->have_last_sort_cam_pos = true;
		best_cloud->last_sort_cam_pos_ws = cam_pos_ws;
		num_sorts_in_flight++;

		Matrix4f world_to_cam;
		scene->cam_to_world.getInverseForAffine3Matrix(world_to_cam);

		task_manager->addTask(new GaussianSplatSortTask(best_cloud->cloud_id, best_cloud->structure_generation, scratch, world_to_cam, &sort_result_queue));
	}
}


void GaussianSplatRenderer::think()
{
	// Applying a completed sort is the only GL call in the whole depth-sort pipeline, which is why it happens here on
	// the main thread rather than in the worker task.
	drainSortResults();

	if(clouds.empty())
		return;

	const Vec2i viewport_dims = opengl_engine->getViewportDims();
	const OpenGLScene* const scene = opengl_engine->getCurrentScene();

	// Focal length in pixels, derived the same way as the engine's own screen-space projections:
	// focal_px = viewport_px * (lens_sensor_dist / sensor_size).
	const float focal_x = (float)viewport_dims.x * scene->lens_sensor_dist / scene->use_sensor_width;
	const float focal_y = (float)viewport_dims.y * scene->lens_sensor_dist / scene->use_sensor_height;

	for(size_t i=0; i<clouds.size(); ++i)
	{
		OpenGLMaterial& mat = clouds[i]->ob->materials[0];
		mat.user_uniform_vals[0].vec2 = Vec2f((float)viewport_dims.x, (float)viewport_dims.y);
		mat.user_uniform_vals[1].vec2 = Vec2f(focal_x, focal_y);
		// user_uniform_vals[2] (splat_tex_width) is constant, and was set in allocCloud().
	}

	kickOffSorts();
}
