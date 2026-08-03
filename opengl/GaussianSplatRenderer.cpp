/*=====================================================================
GaussianSplatRenderer.cpp
-------------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "GaussianSplatRenderer.h"


#include "IncludeOpenGL.h"
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
#include "../utils/Exception.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Sort.h"
#include "../utils/StringUtils.h"
#include "../utils/Task.h"
#include "../utils/TaskManager.h"
#include "../utils/Vector.h"
#include <assert.h>
#include <cstring>
#include <limits>


// How far the camera must move, in world-space metres, since the last sort was kicked off before another one is worth
// doing.  Camera rotation is deliberately not considered: the sort is by distance from the camera, which rotating on
// the spot doesn't change.
static const float resort_move_threshold_ws = 0.1f;


// Reusable working buffers for the background depth-sorts.  At large splat counts these run to hundreds of MB, so
// they're kept and reused rather than reallocated per sort.  Reference counted rather than owned outright, so that an
// in-flight sort keeps them alive even across a shutdown().
class GaussianSplatSortScratch : public ThreadSafeRefCounted
{
public:
	struct SortItem
	{
		uint32 key;
		uint32 splat_index;
	};

	js::Vector<Vec3f, 16> positions_snapshot; // A frozen copy of the world positions, taken on the main thread when a sort is kicked off.  The worker only ever reads this, never the live arrays.

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
const int splat_index_attribute_loc = 1; // Forced in makeShaders().  Slot 1 is otherwise "normal_in", which splats have no use for.

const int coarse_key_bits = 16; // How many high bits of the sort key the coarse stage buckets on, i.e. 65536 evenly spaced depth slices.


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
Reference<OpenGLMeshRenderData> makeInstancedQuadMeshData(VertexBufferAllocator& allocator)
{
	Reference<OpenGLMeshRenderData> mesh_data = new OpenGLMeshRenderData();
	mesh_data->setIndexType(GL_UNSIGNED_SHORT);
	mesh_data->has_uvs = false;
	mesh_data->has_shading_normals = false;
	mesh_data->num_materials_referenced = 1;
	mesh_data->aabb_os = js::AABBox::emptyAABBox(); // Grown as splat clouds are added.

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


// Result of a background depth-sort, handed back to the main thread.  Checked against structure_generation in think():
// if a removeObject() happened since the sort was kicked off, the result is stale and is dropped.
class GaussianSplatSortResultMsg : public ThreadMessage
{
public:
	enum Stage
	{
		Stage_Coarse, // Fast approximate order.  Splats sharing a depth slice are in arbitrary relative order, which is far finer than the splats themselves.
		Stage_Precise // Exact back-to-front order.
	};

	uint64 generation;
	Stage stage;
	Reference<GaussianSplatSortScratch> scratch; // Holds the result buffer, and keeps it alive even if shutdown() was called while the sort ran.

	// Back-to-front (farthest first) instance order, ready to write into instance_index_vbo.
	const js::Vector<uint32, 16>& sortedIndices() const { return (stage == Stage_Coarse) ? scratch->coarse_indices : scratch->precise_indices; }
};


// Sorts the world splat cloud back-to-front by camera distance, entirely on a worker thread.  No GL calls here.
class GaussianSplatSortTask : public glare::Task
{
public:
	GaussianSplatSortTask(uint64 generation_, const Reference<GaussianSplatSortScratch>& scratch_, const Matrix4f& world_to_cam_,
		ThreadSafeQueue<Reference<ThreadMessage> >* result_queue_)
	:	generation(generation_), scratch(scratch_), world_to_cam(world_to_cam_), result_queue(result_queue_)
	{}

	virtual void run(size_t /*thread_index*/) override
	{
		//Timer timer;

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

		//conPrint("GaussianSplatSortTask task took " + timer.elapsedStringMS() + " for " + toString(num_splats) + " splats");
	}

private:
	void enqueueResult(GaussianSplatSortResultMsg::Stage stage)
	{
		Reference<GaussianSplatSortResultMsg> msg = new GaussianSplatSortResultMsg();
		msg->generation = generation;
		msg->stage = stage;
		msg->scratch = scratch;
		result_queue->enqueue(msg);
	}

	uint64 generation;
	Reference<GaussianSplatSortScratch> scratch; // Keeps the snapshot and working buffers alive for the duration of the task.
	Matrix4f world_to_cam;
	ThreadSafeQueue<Reference<ThreadMessage> >* result_queue;
};


} // end anonymous namespace


GaussianSplatRenderer::GaussianSplatRenderer()
:	gpu_capacity_splats(0), total_splats(0), next_handle(1), structure_generation(0), sort_in_flight(false), have_last_sort_cam_pos(false)
{}


GaussianSplatRenderer::~GaussianSplatRenderer()
{}


void GaussianSplatRenderer::makeShaders(OpenGLEngine& opengl_engine, const std::string& shader_dir)
{
	const std::string version_directive    = opengl_engine.getVersionDirective();
	const std::string preprocessor_defines = opengl_engine.getPreprocessorDefines();

	// wait_for_build_to_complete is false because we need to bind our custom attribute location and relink before the
	// program is finished - see below.
	shader_prog = new OpenGLProgram(
		"gaussian splat prog",
		new OpenGLShader(shader_dir + "/gaussian_splat_vert_shader.glsl", version_directive, preprocessor_defines, GL_VERTEX_SHADER),
		new OpenGLShader(shader_dir + "/gaussian_splat_frag_shader.glsl", version_directive, preprocessor_defines, GL_FRAGMENT_SHADER),
		opengl_engine.getAndIncrNextProgramIndex(),
		/*wait_for_build_to_complete=*/false
	);

	// OpenGLProgram's constructor links the program once, binding the engine's standard attribute names.  Our
	// "splat_index_in" attribute isn't one of those, so at this point it's at whatever location the driver picked.
	// Force it to a known location and relink, so rebuildVAO() can build the instance attribute at a location we know.
	shader_prog->bindAttributeLocation(splat_index_attribute_loc, "splat_index_in");
	glLinkProgram(shader_prog->program);
	shader_prog->forceFinishLinkAndDoPostLinkCode(); // Throws glare::Exception if the relink failed.

	opengl_engine.addProgram(shader_prog);

	shader_prog->appendUserUniformInfo(UserUniformInfo::UniformType_Vec2, "viewport_dims_px");
	shader_prog->appendUserUniformInfo(UserUniformInfo::UniformType_Vec2, "focal_len_px");
	shader_prog->appendUserUniformInfo(UserUniformInfo::UniformType_Int,  "splat_tex_width");
}


size_t GaussianSplatRenderer::maxSupportedSplats(int gl_max_texture_size)
{
	// The texture width is fixed at splat_tex_width, which any conformant implementation supports.  The height is capped
	// at the real driver limit rather than the guaranteed minimum, since real hardware commonly supports far more.
	const size_t max_tex_h = (size_t)myMax(1, gl_max_texture_size);
	return (splat_tex_width * max_tex_h) / texels_per_splat;
}


void GaussianSplatRenderer::rebuildVAO()
{
	VertexSpec vertex_spec = world_ob->mesh_data->vertex_spec;
	vertex_spec.attributes[splat_index_attribute_loc].vbo = instance_index_vbo;
	vertex_spec.attributes[splat_index_attribute_loc].enabled = true;
#if DO_INDIVIDUAL_VAO_ALLOC
	world_ob->vert_vao = new VAO(world_ob->mesh_data->vbo_handle.vbo, world_ob->mesh_data->indices_vbo_handle.index_vbo, vertex_spec);
#else
	world_ob->vert_vao = new VAO(vertex_spec);
#endif
	// Bind the index VBO as the object's per-instance buffer.  Note that on the shared-VAO path (i.e. everywhere except
	// Mac and Emscripten) the VertexAttrib::vbo set above is ignored, and the engine instead binds instance_matrix_vbo
	// to binding point 1 at draw time - hence instance_vbo_stride_B, without which the engine would assume the stride of
	// an instance matrix rather than of our uint32 indices.
	world_ob->instance_matrix_vbo = instance_index_vbo;
	world_ob->instance_vbo_stride_B = (uint32)sizeof(uint32);
}


void GaussianSplatRenderer::uploadTexelRowsForSplatRange(size_t first_splat, size_t num_splats_to_upload)
{
	if(num_splats_to_upload == 0)
		return;

	// Repack whole texture rows spanning the given splat range.  An arbitrary range doesn't align to row boundaries, so
	// this may re-pack a few splats belonging to a neighbouring entry too.  That's harmless: their data is already
	// correct, and we just re-derive the same texels for them.
	const size_t first_texel     = first_splat * texels_per_splat;
	const size_t last_texel_excl = (first_splat + num_splats_to_upload) * texels_per_splat;
	const size_t start_row       = first_texel / splat_tex_width;
	const size_t end_row         = Maths::roundedUpDivide(last_texel_excl, splat_tex_width); // Exclusive.
	const size_t num_rows        = end_row - start_row;

	const size_t row_start_splat    = (start_row * splat_tex_width) / texels_per_splat;
	const size_t row_end_splat_excl = myMin(total_splats, (end_row * splat_tex_width) / texels_per_splat);

	js::Vector<float, 16> texel_data(splat_tex_width * num_rows * 4, 0.f);
	packSplatTexels(world_positions, world_scales, world_rotations, world_colours, row_start_splat, row_end_splat_excl, texel_data.data());

	world_ob->materials[0].albedo_texture->loadRegionIntoExistingTexture(/*mipmap_level=*/0, /*x=*/0, /*y=*/start_row, /*z=*/0,
		/*region_w=*/splat_tex_width, /*region_h=*/num_rows, /*region_d=*/1, /*src_row_stride_B=*/splat_tex_width * 4 * sizeof(float),
		ArrayRef<uint8>((const uint8*)texel_data.data(), texel_data.size() * sizeof(float)), /*bind_needed=*/true);
}


void GaussianSplatRenderer::ensureGpuCapacity(size_t needed_splats, OpenGLEngine& opengl_engine, bool ob_already_in_engine)
{
	// The albedo_texture check covers the case of a first addObject() call with zero splats, where needed_splats (0)
	// <= gpu_capacity_splats (0) would otherwise skip creating a texture at all.
	if(needed_splats <= gpu_capacity_splats && world_ob->materials[0].albedo_texture.nonNull())
		return;

	size_t new_tex_h = texHeightForSplatCount(needed_splats);
	const size_t cur_tex_h = texHeightForSplatCount(myMax<size_t>(1, gpu_capacity_splats));
	new_tex_h = myMax(new_tex_h, cur_tex_h * 2); // Geometric growth, so repeated small appends don't reallocate every time.

	const size_t new_capacity_splats = (splat_tex_width * new_tex_h) / texels_per_splat;

	// Repack every splat into a fresh, bigger texture.  Growth is rare, so this cost isn't paid on every addObject().
	js::Vector<float, 16> texel_data(splat_tex_width * new_tex_h * 4, 0.f);
	packSplatTexels(world_positions, world_scales, world_rotations, world_colours, 0, total_splats, texel_data.data());

	world_ob->materials[0].albedo_texture = new OpenGLTexture(splat_tex_width, new_tex_h, &opengl_engine,
		ArrayRef<uint8>((const uint8*)texel_data.data(), texel_data.size() * sizeof(float)),
		OpenGLTextureFormat::Format_RGBA_Linear_Float,
		OpenGLTexture::Filtering_Nearest, // Must be Nearest: this is a data texture, and filtering would blend unrelated splats' attributes together.
		OpenGLTexture::Wrapping_Clamp,
		/*has_mipmaps=*/false);

	// Grow the index VBO to match, in identity order.  Any in-flight sort's result will simply be reapplied, or dropped
	// if a removeObject() also happened, once it lands.
	js::Vector<uint32, 16> identity_indices(new_capacity_splats);
	for(size_t i=0; i<new_capacity_splats; ++i)
		identity_indices[i] = (uint32)i;
	instance_index_vbo = new VBO(identity_indices.data(), identity_indices.size() * sizeof(uint32), GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);

	rebuildVAO();

	// world_ob->vert_vao was just replaced.  If world_ob is already in the engine, the engine cached a draw-time VAO
	// reference when it was added that is now stale, and would keep drawing the old, freed VAO.
	// objectMaterialsUpdated() recomputes it from the current vert_vao.  It must not be called on the very first call,
	// since world_ob hasn't been through OpenGLEngine::addObject()'s buildObjectData() yet at that point.
	if(ob_already_in_engine)
		opengl_engine.objectMaterialsUpdated(*world_ob);

	gpu_capacity_splats = new_capacity_splats;
}


void GaussianSplatRenderer::bakeRangeToWorldSpace(const GaussianSplatData& splat_data, size_t dest_offset, const Vec4f& translation_ws,
	const Quat<float>& rotation_ws, float uniform_scale_ws, js::AABBox& aabb_ws_out)
{
	aabb_ws_out = js::AABBox::emptyAABBox();

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

		world_positions[dest_offset + i] = toVec3f(world_pos);
		world_scales   [dest_offset + i] = os_scale * uniform_scale_ws;
		world_rotations[dest_offset + i] = world_quat.v; // Quat::v is already (x, y, z, w), matching our storage convention.

		aabb_ws_out.enlargeToHoldPoint(world_pos);
	}
}


GaussianSplatRenderer::Handle GaussianSplatRenderer::addObject(const GaussianSplatDataRef& splat_data, const Vec4f& translation_ws,
	const Quat<float>& rotation_ws, float uniform_scale_ws, OpenGLEngine& opengl_engine)
{
	if(shader_prog.isNull())
		throw glare::Exception("GaussianSplatRenderer::addObject(): makeShaders() must be called first.");

	const bool creating_world_ob = world_ob.isNull();
	if(creating_world_ob)
	{
		// The first splat cloud added: create the shared world object.  Its ob_to_world_matrix stays identity forever,
		// since every splat position is baked into world space in the data itself.  It's deliberately not added to the
		// engine yet - see below, once ensureGpuCapacity() has built a real VAO and texture for it.
		world_ob = new GLObject();
		world_ob->ob_to_world_matrix = Matrix4f::identity();
		world_ob->mesh_data = makeInstancedQuadMeshData(*opengl_engine.vert_buf_allocator);

		world_ob->materials.resize(1);
		OpenGLMaterial& mat = world_ob->materials[0];
		mat.shader_prog = shader_prog;
		mat.auto_assign_shader = false;

		// Route the object through drawAlphaBlendedObjects() rather than the transparent pass.  The transparent pass
		// uses order-independent transparency on native, which accumulates colour additively and so has no notion of
		// one splat occluding another.  That's a reasonable approximation for a few glass surfaces, but splat clouds put
		// hundreds of overlapping splats on every pixel, where it saturates to white.  Splats need real back-to-front
		// blending, which is what the depth sort in think() provides and what the alpha-blended pass does.
		mat.alpha_blend = true;
		mat.user_uniform_vals.resize(3); // viewport_dims_px and focal_len_px are set by think().
		mat.user_uniform_vals[2].intval = (int)splat_tex_width;
		// mat.albedo_texture is set by ensureGpuCapacity() below.
	}

	const size_t num_new_splats = splat_data->numSplats();
	const size_t old_total = total_splats;
	const size_t new_total = old_total + num_new_splats;

	world_positions.resize(new_total);
	world_scales   .resize(new_total);
	world_rotations.resize(new_total);
	world_colours  .resize(new_total);

	js::AABBox new_range_aabb_ws;
	bakeRangeToWorldSpace(*splat_data, old_total, translation_ws, rotation_ws, uniform_scale_ws, new_range_aabb_ws);

	for(size_t i=0; i<num_new_splats; ++i)
		world_colours[old_total + i] = splat_data->colours[i]; // Colour and opacity aren't affected by the cloud's pose.

	total_splats = new_total;

	// Set the instance count and enlarge the world AABB before possibly adding world_ob to the engine below, so that
	// call never sees a stale instance count or the placeholder empty box.
	world_ob->num_instances_to_draw = (int)new_total;
	world_ob->mesh_data->aabb_os.enlargeToHoldAABBox(new_range_aabb_ws); // Only ever enlarged here; removeObject() recomputes it exactly.

	ensureGpuCapacity(new_total, opengl_engine, /*ob_already_in_engine=*/!creating_world_ob);

	// Only now does world_ob have a real VAO (with the instance attribute bound) and a real data texture.  Adding it to
	// the engine any earlier would leave the engine caching a stale, non-instanced VAO reference that a later VAO
	// rebuild can't retroactively fix.
	if(creating_world_ob)
		opengl_engine.addObject(world_ob);

	uploadTexelRowsForSplatRange(old_total, num_new_splats); // If ensureGpuCapacity() just repacked everything, this re-uploads the same correct data, which is harmless.

	// Extend the index VBO's identity-order tail to cover the newly appended splats.  Again redundant but harmless if
	// ensureGpuCapacity() just rebuilt the whole thing.
	{
		js::Vector<uint32, 16> new_indices(num_new_splats);
		for(size_t i=0; i<num_new_splats; ++i)
			new_indices[i] = (uint32)(old_total + i);
		instance_index_vbo->updateData(old_total * sizeof(uint32), new_indices.data(), new_indices.size() * sizeof(uint32));
	}

	opengl_engine.updateObjectTransformData(*world_ob); // Refreshes aabb_ws from the aabb_os enlarged above.  ob_to_world_matrix itself never changes.

	WorldSplatEntry entry;
	entry.handle = next_handle++;
	entry.splat_data = splat_data;
	entry.offset = old_total;
	entry.count = num_new_splats;
	entry.aabb_ws = new_range_aabb_ws;
	entries.push_back(entry);

	have_last_sort_cam_pos = false; // Force a fresh sort: the appended splats are in identity order relative to the rest.

	return entry.handle;
}


bool GaussianSplatRenderer::updateObjectTransform(Handle handle, const Vec4f& translation_ws, const Quat<float>& rotation_ws,
	float uniform_scale_ws, OpenGLEngine& opengl_engine)
{
	for(size_t e=0; e<entries.size(); ++e)
	{
		if(entries[e].handle == handle)
		{
			WorldSplatEntry& entry = entries[e];

			bakeRangeToWorldSpace(*entry.splat_data, entry.offset, translation_ws, rotation_ws, uniform_scale_ws, entry.aabb_ws);
			// Colours are unchanged: re-baking never touches world_colours.

			uploadTexelRowsForSplatRange(entry.offset, entry.count);

			world_ob->mesh_data->aabb_os.enlargeToHoldAABBox(entry.aabb_ws); // Conservative: only grows.  removeObject() does the exact recompute.
			opengl_engine.updateObjectTransformData(*world_ob);

			// This entry's splats may now be in the wrong depth order relative to the rest of the world.  This doesn't
			// bump structure_generation: offsets and counts are unchanged, so an in-flight sort's indices stay meaningful.
			have_last_sort_cam_pos = false;

			return true;
		}
	}
	return false;
}


bool GaussianSplatRenderer::isValidHandle(Handle handle) const
{
	for(size_t e=0; e<entries.size(); ++e)
		if(entries[e].handle == handle)
			return true;
	return false;
}


bool GaussianSplatRenderer::removeObject(Handle handle)
{
	for(size_t e=0; e<entries.size(); ++e)
	{
		if(entries[e].handle == handle)
		{
			const size_t offset = entries[e].offset;
			const size_t count  = entries[e].count;
			const size_t num_tail_splats = total_splats - (offset + count); // Splats after the removed range, which shift down to close the gap.

			if(num_tail_splats > 0)
			{
				std::memmove(&world_positions[offset], &world_positions[offset + count], num_tail_splats * sizeof(Vec3f));
				std::memmove(&world_scales   [offset], &world_scales   [offset + count], num_tail_splats * sizeof(Vec3f));
				std::memmove(&world_rotations[offset], &world_rotations[offset + count], num_tail_splats * sizeof(Vec4f));
				std::memmove(&world_colours  [offset], &world_colours  [offset + count], num_tail_splats * sizeof(Vec4f));
			}

			total_splats -= count;
			world_positions.resize(total_splats);
			world_scales   .resize(total_splats);
			world_rotations.resize(total_splats);
			world_colours  .resize(total_splats);

			entries.erase(entries.begin() + e);
			for(size_t j=0; j<entries.size(); ++j)
				if(entries[j].offset > offset)
					entries[j].offset -= count;

			structure_generation++; // Invalidates any in-flight sort's result.

			// Full re-upload of what's left.  The texture and VBO capacity is unchanged: removal never shrinks GPU storage.
			uploadTexelRowsForSplatRange(0, total_splats);

			if(total_splats > 0)
			{
				js::Vector<uint32, 16> identity_indices(total_splats);
				for(size_t i=0; i<total_splats; ++i)
					identity_indices[i] = (uint32)i;
				instance_index_vbo->updateData(0, identity_indices.data(), identity_indices.size() * sizeof(uint32));
			}

			world_ob->num_instances_to_draw = (int)total_splats;

			rebuildWorldAABB();

			have_last_sort_cam_pos = false; // Force a fresh sort of the now-renumbered world.

			return true;
		}
	}
	return false;
}


void GaussianSplatRenderer::removeAllObjects()
{
	if(entries.empty())
		return;

	entries.clear();
	world_positions.clear();
	world_scales   .clear();
	world_rotations.clear();
	world_colours  .clear();
	total_splats = 0;

	structure_generation++; // Invalidates any in-flight sort's result.

	// Note that we deliberately don't touch shader_prog, world_ob, instance_index_vbo or gpu_capacity_splats: the GPU
	// storage is kept for reuse, exactly as removeObject() does.  Nothing is drawn while num_instances_to_draw is 0, so
	// the now-stale texture and index VBO contents don't matter.
	if(world_ob.nonNull())
	{
		world_ob->num_instances_to_draw = 0;
		rebuildWorldAABB();
	}

	have_last_sort_cam_pos = false;
}


void GaussianSplatRenderer::rebuildWorldAABB()
{
	js::AABBox aabb = js::AABBox::emptyAABBox();
	for(size_t e=0; e<entries.size(); ++e)
		aabb.enlargeToHoldAABBox(entries[e].aabb_ws);
	world_ob->mesh_data->aabb_os = aabb;
}


void GaussianSplatRenderer::think(OpenGLEngine& opengl_engine, glare::TaskManager& task_manager)
{
	if(world_ob.isNull())
		return;

	// Drain any completed sort result and write it to the index VBO.  This is the only GL call in the whole depth-sort
	// pipeline, which is why it happens here on the main thread rather than in the worker task.
	sort_result_queue.dequeueAnyQueuedItems(completed_msgs);
	for(size_t i=0; i<completed_msgs.size(); ++i)
	{
		const GaussianSplatSortResultMsg* msg = static_cast<const GaussianSplatSortResultMsg*>(completed_msgs[i].ptr());

		// If a later message in this same batch is for the same generation, that one supersedes this one.  Both
		// stages can land in the same frame on a fast-sorting world, and uploading the coarse order only to
		// overwrite it with the precise order in the same frame is a pointless buffer upload.
		bool superseded_this_frame = false;
		for(size_t j=i + 1; j<completed_msgs.size(); ++j)
			if(static_cast<const GaussianSplatSortResultMsg*>(completed_msgs[j].ptr())->generation == msg->generation)
			{
				superseded_this_frame = true;
				break;
			}

		if(msg->stage == GaussianSplatSortResultMsg::Stage_Precise)
			sort_in_flight = false;

		// A removeObject() since this sort was kicked off has renumbered the world, so this result's indices no
		// longer mean the same splats.  Drop it.
		if(msg->generation != structure_generation)
			continue;

		if(!superseded_this_frame)
		{
			// The snapshot this was computed from may be a strict prefix of the current, possibly since-grown
			// world.  Only write as many bytes as the result actually covers: any appended tail beyond it already
			// holds valid identity-order indices written by addObject().
			const js::Vector<uint32, 16>& sorted_indices = msg->sortedIndices();
			instance_index_vbo->updateData(0, sorted_indices.data(), sorted_indices.size() * sizeof(uint32));
		}
	}

	const Vec2i viewport_dims = opengl_engine.getViewportDims();
	const OpenGLScene* scene = opengl_engine.getCurrentScene();

	// Focal length in pixels, derived the same way as the engine's own screen-space projections:
	// focal_px = viewport_px * (lens_sensor_dist / sensor_size).
	const float focal_x = (float)viewport_dims.x * scene->lens_sensor_dist / scene->use_sensor_width;
	const float focal_y = (float)viewport_dims.y * scene->lens_sensor_dist / scene->use_sensor_height;

	OpenGLMaterial& mat = world_ob->materials[0];
	mat.user_uniform_vals[0].vec2 = Vec2f((float)viewport_dims.x, (float)viewport_dims.y);
	mat.user_uniform_vals[1].vec2 = Vec2f(focal_x, focal_y);
	// user_uniform_vals[2] (splat_tex_width) is constant, and was set in addObject().

	// Kick off a background re-sort, but only if one isn't already in flight and the camera has moved far enough since
	// the last one to be worth it.
	const Vec4f cam_pos_ws = scene->cam_to_world.getColumn(3);
	const bool moved_enough = !have_last_sort_cam_pos || cam_pos_ws.getDist(last_sort_cam_pos_ws) >= resort_move_threshold_ws;
	if(total_splats > 0 && !sort_in_flight && moved_enough)
	{
		Matrix4f world_to_cam;
		scene->cam_to_world.getInverseForAffine3Matrix(world_to_cam);

		if(sort_scratch.isNull())
			sort_scratch = new GaussianSplatSortScratch();

		// Freeze a snapshot of the current positions on the main thread before handing off to the worker, so the worker
		// never touches the live, growable arrays.
		sort_scratch->positions_snapshot.resizeNoCopy(total_splats);
		std::memcpy(sort_scratch->positions_snapshot.data(), world_positions.data(), total_splats * sizeof(Vec3f));

		sort_in_flight = true;
		have_last_sort_cam_pos = true;
		last_sort_cam_pos_ws = cam_pos_ws;

		task_manager.addTask(new GaussianSplatSortTask(structure_generation, sort_scratch, world_to_cam, &sort_result_queue));
	}
}
