/*=====================================================================
OpenGLEngine.h
--------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "OpenGLMeshRenderData.h"
#include "TextureLoading.h"
#include "OpenGLTexture.h"
#include "OpenGLProgram.h"
#include "GLMemUsage.h"
#include "ShadowMapping.h"
#include "VertexBufferAllocator.h"
#include "DrawIndirectBuffer.h"
#include "UniformBufOb.h"
#include "VBO.h"
#include "VAO.h"
#include "SSBO.h"
#include "OpenGLCircularBuffer.h"
#include "AsyncTextureLoader.h"
#include "TextureAllocator.h"
#include "../graphics/colour3.h"
#include "../graphics/Colour4f.h"
#include "../graphics/AnimationData.h"
#include "../graphics/BatchedMesh.h"
#include "../physics/jscol_aabbox.h"
#include "../maths/vec2.h"
#include "../maths/vec3.h"
#include "../maths/Matrix2.h"
#include "../maths/matrix3.h"
#include "../maths/plane.h"
#include "../maths/Matrix4f.h"
#include "../maths/PCG32.h"
#include "../maths/Quat.h"
#include "../utils/Timer.h"
#include "../utils/Vector.h"
#include "../utils/Reference.h"
#include "../utils/RefCounted.h"
#include "../utils/TaskManager.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/StringUtils.h"
#include "../utils/PrintOutput.h"
#include "../utils/ManagerWithCache.h"
#include "../utils/PoolAllocator.h"
#include "../utils/GeneralMemAllocator.h"
#include "../utils/ThreadManager.h"
#include "../utils/SmallArray.h"
#include "../utils/LinearIterSet.h"
#include "../physics/HashedGrid2.h"
#include <assert.h>
#include <unordered_set>
#include <set>
namespace Indigo { class Mesh; }
namespace glare { class TaskManager; }
class Map2D;
class TextureServer;
class UInt8ComponentValueTraits;
class TerrainSystem;
class RenderBuffer;
namespace glare { class BestFitAllocator; }
template <class V, class VTraits> class ImageMap;



struct OpenGLUniformVal // variant class
{
	OpenGLUniformVal() {}
	OpenGLUniformVal(int i) : intval(i) {}
	union
	{
		Vec3f vec3;
		Vec2f vec2;
		int intval;
		float floatval;
	};
};


// If UNIFORM_BUF_PER_MAT_SUPPORT is 1, we allocate a uniform buffer per material.
// This runs faster on Mac since updating a single uniform buffer each draw call is quite slow on Mac.
// Mac M1: 42 fps with UNIFORM_BUF_PER_MAT_SUPPORT, 36 fps without, standing in middle of Substrata world.
// This is only relevant if use_multi_draw_indirect is false, e.g. on old OpenGL implementations (Macs).
// UNIFORM_BUF_PER_MAT_SUPPORT is slower on Windows + RTX 3080: 60.2 fps with, 68 fps without.
//#ifdef __APPLE__
//#define UNIFORM_BUF_PER_MAT_SUPPORT 1 // This is slower on Windows + Nvidia GPU when enabled.
//#else
//#define UNIFORM_BUF_PER_MAT_SUPPORT 0
//#endif
#define UNIFORM_BUF_PER_MAT_SUPPORT 0 // Disabled for now, was getting rendering errors (imposters having incorrect colour and no texture), not worth trying to find issue.


class OpenGLMaterial
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	OpenGLMaterial()
	:	transparent(false),
		hologram(false),
		albedo_linear_rgb(0.7f, 0.7f, 0.7f),
		alpha(1.f),
		emission_linear_rgb(0.7f, 0.7f, 0.7f),
		emission_scale(0.f),
		roughness(0.5f),
		tex_matrix(1,0,0,1),
		tex_translation(0,0),
		userdata(0),
		fresnel_scale(0.5f),
		metallic_frac(0.f),
		gen_planar_uvs(false),
		draw_planar_uv_grid(false),
		convert_albedo_from_srgb(false),
		imposter(false),
		imposterable(false),
		use_wind_vert_shader(false),
		double_sided(false),
		materialise_effect(false),
		cast_shadows(true),
		geomorphing(false),
		water(false),
		terrain(false),
		imposter_tex_has_multiple_angles(false),
		decal(false),
		participating_media(false),
		draw_into_depth_buffer(false),
		auto_assign_shader(true),
		begin_fade_out_distance(100.f),
		end_fade_out_distance(120.f),
		materialise_lower_z(0),
		materialise_upper_z(1),
		materialise_start_time(-20000),
		material_data_index(-1)
	{}

	Colour3f albedo_linear_rgb; // First approximation to material colour.  Linear sRGB.
	float alpha; // Used for transparent mats.
	Colour3f emission_linear_rgb; // Linear sRGB in [0, 1]
	float emission_scale; // [0, inf).  Spectral radiance * 1.0e-9

	bool imposter; // Use imposter shader?
	bool imposterable; // Fade out with distance
	bool transparent;
	bool hologram; // E.g. just emission, no light scattering.
	bool gen_planar_uvs;
	bool draw_planar_uv_grid;
	bool convert_albedo_from_srgb;
	bool use_wind_vert_shader;
	bool double_sided;
	bool materialise_effect;
	bool cast_shadows;
	bool geomorphing;
	bool water;
	bool terrain;
	bool imposter_tex_has_multiple_angles;
	bool decal;
	bool participating_media;

	bool auto_assign_shader; // If true, assign a shader prog in assignShaderProgToMaterial(), e.g. when object is added or objectMaterialsUpdated() is called.  True by default.

	bool draw_into_depth_buffer; // Internal

	Reference<OpenGLTexture> albedo_texture;
	Reference<OpenGLTexture> metallic_roughness_texture;
	Reference<OpenGLTexture> lightmap_texture;
	Reference<OpenGLTexture> backface_albedo_texture;
	Reference<OpenGLTexture> transmission_texture;
	Reference<OpenGLTexture> emission_texture;
	Reference<OpenGLTexture> normal_map;

	Reference<OpenGLProgram> shader_prog;
	Reference<OpenGLProgram> depth_draw_shader_prog;

	Matrix2f tex_matrix;
	Vec2f tex_translation;

	float roughness;
	float fresnel_scale;
	float metallic_frac;
	float begin_fade_out_distance; // Used when imposterable is true
	float end_fade_out_distance; // Used when imposterable is true

	float materialise_lower_z;
	float materialise_upper_z;
	float materialise_start_time;
	
	uint64 userdata;

	// Kind-of user-data.  Only used in OpenGLEngine::addOpenGLTexture() and OpenGLEngine::assignLoadedTextureToObMaterials() currently, which should be removed/refactored:
	std::string tex_path;
	std::string metallic_roughness_tex_path;
	std::string lightmap_path;
	std::string emission_tex_path;
	std::string normal_map_path;

	js::Vector<OpenGLUniformVal, 16> user_uniform_vals;

	int material_data_index;

#if UNIFORM_BUF_PER_MAT_SUPPORT
	UniformBufObRef uniform_buf_ob;
#endif
};


#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable:4324) // Disable 'structure was padded due to __declspec(align())' warning.
#endif


struct GLObjectAnimNodeData
{
	GLARE_ALIGNED_16_NEW_DELETE

	GLObjectAnimNodeData() : procedural_transform(Matrix4f::identity()) {}

	Matrix4f node_hierarchical_to_object; // The overall transformation from bone space to object space, computed by walking up the node hierarchy.  Ephemeral data computed every frame.
	Matrix4f last_pre_proc_to_object; // Same as node_hierarchical_to_object, but without procedural_transform applied.
	Quatf last_rot;
	Matrix4f procedural_transform;
};


struct GlInstanceInfo
{
	Matrix4f to_world; // For imposters, this will not have rotation baked in.
	js::AABBox aabb_ws;
};


// program_index_and_flags:
// Bit 31: program supports MDI
// Bit 30: material is transparent
// Bit 29: material is water
// Bit 28: material is a decal
// Bit 27: material is participating media
// Bit 26: material has backface culling
// Bits 0-25: program index
#define PROG_SUPPORTS_MDI_BITFLAG						(1u << 31)
#define MATERIAL_TRANSPARENT_BITFLAG					(1u << 30)
#define MATERIAL_WATER_BITFLAG							(1u << 29)
#define MATERIAL_DECAL_BITFLAG							(1u << 28)
#define MATERIAL_PARTIC_MEDIA_BITFLAG					(1u << 27)
#define BACKFACE_CULLING_BITFLAG						(1u << 26)

#define ISOLATE_PROG_INDEX_MASK							0x03FFFFFF // Zero out top 6 bits
#define ISOLATE_PROG_INDEX_AND_BACKFACE_CULLING_MASK	0x07FFFFFF // Zero out top 5 bits


struct GLObjectBatchDrawInfo
{
	uint32 program_index_and_flags;
	uint32 material_data_index;
	uint32 prim_start_offset_B; // Offset in bytes from the start of the index buffer.
	uint32 num_indices;

	inline uint32 getProgramIndex()                   const { return program_index_and_flags & ISOLATE_PROG_INDEX_MASK; }
	inline uint32 getProgramIndexAndBackfaceCulling() const { return program_index_and_flags & ISOLATE_PROG_INDEX_AND_BACKFACE_CULLING_MASK; }
};


struct GLObject
{
	GLObject() noexcept;
	~GLObject();

	GLARE_ALIGNED_16_NEW_DELETE

	// For placement new in PoolAllocator:
#if __cplusplus >= 201703L
		void* operator new  (size_t size, std::align_val_t alignment, void* ptr) { return ptr; }
#else
		void* operator new  (size_t /*size*/, void* ptr) { return ptr; }
#endif

	void enableInstancing(VertexBufferAllocator& allocator, const void* instance_matrix_data, size_t instance_matrix_data_size); // Enables instancing attributes, and builds vert_vao.

	void setSingleMaterial(const OpenGLMaterial& mat) { materials.resize(1); materials[0] = mat; }

	Matrix4f ob_to_world_matrix;
	Matrix4f ob_to_world_inv_transpose_matrix; // inverse transpose of upper-left part of to-world matrix.

	js::AABBox aabb_ws;

	SmallArray<GLObjectBatchDrawInfo, 4> batch_draw_info;

	uint32 vao_and_vbo_key;
	uint32 random_num;

	// Denormalised data
	VAO* vao;
	VBO* vert_vbo;
	VBO* index_vbo;
	GLenum index_type;
	GLuint instance_matrix_vbo_name;
	uint32 indices_vbo_handle_offset;
	uint32 vbo_handle_base_vertex;
	
	SmallArray<GLObjectBatchDrawInfo, 1> depth_draw_batches; // Index batches, use for depth buffer drawing for shadow mapping.  
	// We will use a SmallArray for this with N = 1, since the most likely number of batches is 1.

	Reference<OpenGLMeshRenderData> mesh_data;

	VAORef vert_vao; // Overrides mesh_data->vert_vao if non-null.  Having a vert_vao here allows us to enable instancing, by binding to the instance_matrix_vbo etc..

	//IndexBufAllocationHandle instance_matrix_vbo_handle;
	VBORef instance_matrix_vbo;
	VBORef instance_colour_vbo;
	
	bool always_visible; // For objects like the move/rotate arrows, that should be visible even when behind other objects.
	bool draw_to_mask_map; // Draw to terrain mask map?
	bool is_imposter;
	bool decal;
	int num_instances_to_draw; // e.g. num matrices built in instance_matrix_vbo.
	js::Vector<GlInstanceInfo, 16> instance_info; // Used for updating instance + imposter matrices.
	
	std::vector<OpenGLMaterial> materials;

	SmallArray<uint32, 1> depth_draw_batch_material_indices; // For the slow path where we need to get the material used by the depth draw batch

	int object_type; // 0 = tri mesh
	float line_width;

	// Current animation state:
	int current_anim_i; // Index into animations.
	int next_anim_i;
	double transition_start_time;
	double transition_end_time;
	double use_time_offset;

	float morph_start_dist; // Distance from camera at which we start morphing to the next lower LOD level, for geomorphing objects (terrain).
	float morph_end_dist;

	float depth_draw_depth_bias; // Distance to move fragment position towards sun when drawing to depth buffer.

	js::Vector<GLObjectAnimNodeData, 16> anim_node_data;
	js::Vector<Matrix4f, 16> joint_matrices;

	static const int MAX_NUM_LIGHT_INDICES = 8;
	int light_indices[MAX_NUM_LIGHT_INDICES];

	//glare::PoolAllocator* allocator;
	//int allocation_index;

	int per_ob_vert_data_index;
	int joint_matrices_base_index;
	glare::BestFitAllocator::BlockInfo* joint_matrices_block;

	// From ThreadSafeRefCounted.  Manually define this stuff here, so refcount can be defined not at the start of the structure, which wastes space due to alignment issues.
	inline glare::atomic_int decRefCount() const { return refcount.decrement(); }
	inline void incRefCount() const { refcount.increment(); }
	inline glare::atomic_int getRefCount() const { return refcount; }
	mutable glare::AtomicInt refcount;
};

typedef Reference<GLObject> GLObjectRef;


void doDestroyGLOb(GLObject* ob);

// Template specialisation of destroyAndFreeOb for GLObject.  This is called when being freed by a Reference.
// We will use this to free from the OpenGLEngine PoolAllocator if the object was allocated from there.
template <>
inline void destroyAndFreeOb<GLObject>(GLObject* ob)
{
	doDestroyGLOb(ob);
}




struct GLObjectHash
{
	size_t operator() (const GLObjectRef& ob) const
	{
		return (size_t)ob.getPointer() >> 3; // Assuming 8-byte aligned, get rid of lower zero bits.
	}
};


struct OverlayObject : public ThreadSafeRefCounted
{
	GLARE_ALIGNED_16_NEW_DELETE

	OverlayObject();

	Matrix4f ob_to_world_matrix;

	Reference<OpenGLMeshRenderData> mesh_data;
	
	OpenGLMaterial material;

	bool draw; // Should this object be drawn (or should it be skipped?).  True by default.
};
typedef Reference<OverlayObject> OverlayObjectRef;


struct OverlayObjectHash
{
	size_t operator() (const OverlayObjectRef& ob) const
	{
		return (size_t)ob.getPointer() >> 3; // Assuming 8-byte aligned, get rid of lower zero bits.
	}
};


class OpenGLEngineSettings
{
public:
	OpenGLEngineSettings() : enable_debug_output(false), shadow_mapping(false), compress_textures(false), /*use_final_image_buffer(false),*/ depth_fog(false), render_sun_and_clouds(true), render_water_caustics(true), 
		max_tex_CPU_mem_usage(1024 * 1024 * 1024ull), max_tex_GPU_mem_usage(1024 * 1024 * 1024ull), use_grouped_vbo_allocator(true), msaa_samples(4), allow_bindless_textures(true), 
		allow_multi_draw_indirect(true) {}

	bool enable_debug_output;
	bool shadow_mapping;
	bool compress_textures;
	//bool use_final_image_buffer; // Render to an off-screen buffer, which can be used for post-processing.  Required for bloom post-processing.
	bool depth_fog;
	bool render_sun_and_clouds;
	bool render_water_caustics;
	bool use_grouped_vbo_allocator; // Use the best-fit allocator to group multiple vertex buffers into one VBO.  Faster rendering but uses more GPU RAM due to unused space in the VBOs.
	int msaa_samples; // MSAA samples, used if use_final_image_buffer is true.  <= 1 to disable MSAA.

	bool allow_bindless_textures; // Allow use of bindless textures, if supported by the OpenGL implementation?   True by default.
	bool allow_multi_draw_indirect; // Allow multi-draw indirect drawing, if supported by the OpenGL implementation?   True by default.

	uint64 max_tex_CPU_mem_usage; // If total CPU RAM usage of used and cached textures exceeds this value, textures will be removed from cache.  Default: 1GB
	uint64 max_tex_GPU_mem_usage; // If total GPU RAM usage of used and cached textures exceeds this value, textures will be removed from cache.  Default: 1GB
};


// Should match LightData struct in common_frag_structures.glsl etc.
struct LightGPUData
{
	LightGPUData() : light_type(0), cone_cos_angle_start(0.8f), cone_cos_angle_end(0.85f) {}
	Vec4f pos;
	Vec4f dir;
	Colour4f col;
	int light_type; // 0 = point light, 1 = spotlight
	float cone_cos_angle_start;
	float cone_cos_angle_end;
};


struct GLLight : public ThreadSafeRefCounted
{
	LightGPUData gpu_data;
	int buffer_index;
};

typedef Reference<GLLight> GLLightRef;


// The OpenGLEngine contains one or more OpenGLScenes.
// An OpenGLScene is a set of objects, plus a camera transform, and associated information.
// The current scene being rendered by the OpenGLEngine can be set with OpenGLEngine::setCurrentScene().
class OpenGLScene : public ThreadSafeRefCounted
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	OpenGLScene(OpenGLEngine& engine);

	friend class OpenGLEngine;

	void setPerspectiveCameraTransform(const Matrix4f& world_to_camera_space_matrix_, float sensor_width_, float lens_sensor_dist_, float render_aspect_ratio_, float lens_shift_up_distance_,
		float lens_shift_right_distance_, float viewport_aspect_ratio);
	void setOrthoCameraTransform(const Matrix4f& world_to_camera_space_matrix_, float sensor_width_, float render_aspect_ratio_, float lens_shift_up_distance_,
		float lens_shift_right_distance_, float viewport_aspect_ratio);
	void setDiagonalOrthoCameraTransform(const Matrix4f& world_to_camera_space_matrix_, float sensor_width_, float render_aspect_ratio_, float viewport_aspect_ratio);
	void setIdentityCameraTransform();

	void calcCamFrustumVerts(float near_dist, float far_dist, Vec4f* verts_out) const;

	void unloadAllData();

	GLMemUsage getTotalMemUsage() const;

	Colour3f background_colour;

	bool use_z_up; // Should the +z axis be up (for cameras, sun lighting etc..).  True by default.  If false, y is up.
	// NOTE: Use only with CameraType_Identity currently, clip planes won't be computed properly otherwise.

	bool shadow_mapping; // True by default
	bool draw_water; // True by default

	// If true, the scene is rendered to an offscreen and internally allocated framebuffer.  This allows for bloom effects etc.
	// If false, the scene is rendered directly to the framebuffer set by setTargetFrameBuffer().  Only some object types can be draw in this case (not transparent objects etc.)
	bool use_main_render_framebuffer; // True by default

	bool cloud_shadows; // True by default

	float bloom_strength; // [0-1].  Strength 0 turns off bloom.  0 by default.

	float wind_strength; // Default = 1.

	float water_level_z; // Default = 0.  Controls drawing of underwater caustics.

	js::Vector<Vec4f, 16> blob_shadow_locations;
	Vec4f grass_pusher_sphere_pos;
private:
	float use_sensor_width;
	float use_sensor_height;
	
	float lens_sensor_dist;
	float render_aspect_ratio;
	float lens_shift_up_distance;
	float lens_shift_right_distance;

	// These enum values should match the defines in common_frag_structures.glsl
	enum CameraType
	{
		CameraType_Identity = 0, // Identity camera transform.
		CameraType_Perspective = 1,
		CameraType_Orthographic = 2,
		CameraType_DiagonalOrthographic = 3
	};

	CameraType camera_type;

	Matrix4f world_to_camera_space_matrix; // Maps world space to a camera space where (1,0,0) is right, (0,1,0) is forwards and (0,0,1) is up.
	Matrix4f cam_to_world;
public:
	Matrix4f overlay_world_to_camera_space_matrix; // Identity by default

	glare::LinearIterSet<Reference<GLObject>, GLObjectHash> objects;
	glare::LinearIterSet<Reference<GLObject>, GLObjectHash> animated_objects; // Objects for which we need to update the animation data (bone matrices etc.) every frame.
	glare::LinearIterSet<Reference<GLObject>, GLObjectHash> transparent_objects;
	glare::LinearIterSet<Reference<GLObject>, GLObjectHash> participating_media_objects;
	glare::LinearIterSet<Reference<GLObject>, GLObjectHash> water_objects;
	glare::LinearIterSet<Reference<GLObject>, GLObjectHash> decal_objects;
	std::set<Reference<GLObject>> always_visible_objects; // For objects like the move/rotate arrows, that should be visible even when behind other objects.
	std::set<Reference<OverlayObject>> overlay_objects; // UI overlays
	std::set<Reference<GLObject>> materialise_objects; // Objects currently playing materialise effect
	
	GLObjectRef env_ob;

	std::set<Reference<GLLight>> lights;

	HashedGrid2<GLLight*, std::hash<GLLight*>> light_grid;

private:
	float max_draw_dist;
	float near_draw_dist;
public:
	Planef frustum_clip_planes[6];
	int num_frustum_clip_planes; // <= 6
	js::AABBox frustum_aabb;
};


typedef Reference<OpenGLScene> OpenGLSceneRef;


struct OpenGLSceneHash
{
	size_t operator() (const OpenGLSceneRef& scene) const
	{
		return (size_t)scene.getPointer() >> 3; // Assuming 8-byte aligned, get rid of lower zero bits.
	}
};


struct FrameBufTextures
{
	Reference<OpenGLTexture> colour_texture;
	Reference<OpenGLTexture> emission_texture;
	Reference<OpenGLTexture> depth_texture;
};


/*
We will pack various IDs for each batch to draw together into a 32-bit uint, then radix sort the uints.
We will allocate a certain number of bits to each type of ID.
The more expensive the state change, the more significant bit position we allocate the IDs.
Index type is in here so we can group together more calls for multi-draw-indirect.
If the actual ID exceeds the allocated number of bits, rendering will still be correct, we just will do more state changes than strictly needed.

             bits allocated    bit index (0 = least significant bit)
program_index:     8 bits      24
backface_cull:     1 bit       23
VAO id:	           9 bits      14
vert VBO id:       6 bits      8
index VBO id:      6 bits      2
index type bits:   2 bits      0
*/

inline uint32 makeVAOAndVBOKey(uint32 vao_id, uint32 vert_vbo_id, uint32 idx_vbo_id, uint32 index_type_bits)
{
	return ((vao_id & 511u) << 14) | ((vert_vbo_id & 63u) << 8) | ((idx_vbo_id & 63u) << 2) | index_type_bits;
}

struct BatchDrawInfo
{
	BatchDrawInfo() {}
	BatchDrawInfo(uint32 prog_index_and_backface_culling_flag, uint32 vao_and_vbo_key, const GLObject* ob_, uint32 batch_i_) 
	:	prog_vao_key(((prog_index_and_backface_culling_flag & 511u) << 23) | vao_and_vbo_key),
		batch_i(batch_i_), ob(ob_)
	{}

	BatchDrawInfo(uint32 prog_index_and_backface_culling_flag, uint32 vao_id, uint32 vert_vbo_id, uint32 idx_vbo_id, uint32 index_type_bits, const GLObject* ob_, uint32 batch_i_) 
	:	prog_vao_key(((prog_index_and_backface_culling_flag & 511u) << 23) | makeVAOAndVBOKey(vao_id, vert_vbo_id, idx_vbo_id, index_type_bits)),
		batch_i(batch_i_), ob(ob_)
	{
		assert(index_type_bits <= 2);
	}
	std::string keyDescription() const;

	uint32 prog_vao_key;
	uint32 batch_i;
	const GLObject* ob;
	
	bool operator < (const BatchDrawInfo& other) const
	{
		if(prog_vao_key < other.prog_vao_key)
			return true;
		else if(prog_vao_key > other.prog_vao_key)
			return false;
		else
			return ob->mesh_data.ptr() < other.ob->mesh_data.ptr();
	}
};

// Similar to BatchDrawInfo, but with a distance field, so we can sort from far to near.
// Also has no batch_i as we will assume we are always drawing batch 0.
struct BatchDrawInfoWithDist
{
	BatchDrawInfoWithDist() {}
	BatchDrawInfoWithDist(uint32 prog_index_and_backface_culling_flag, uint32 vao_and_vbo_key, uint32 dist_, const GLObject* ob_)
	:	prog_vao_key(((prog_index_and_backface_culling_flag & 511u) << 23) | vao_and_vbo_key),
		dist(dist_),
		ob(ob_)
	{}

	uint32 prog_vao_key;
	uint32 dist;
	const GLObject* ob;
};



// Matches MaterialData defined in common_frag_structures.glsl
// Used for transparent mats also.
struct PhongUniforms
{
	Colour4f diffuse_colour; // linear sRGB
	Colour4f emission_colour; // linear sRGB, spectral radiance * 1.0e-9
	Vec2f texture_upper_left_matrix_col0;
	Vec2f texture_upper_left_matrix_col1;
	Vec2f texture_matrix_translation;

	uint64 diffuse_tex; // Bindless texture handle
	uint64 metallic_roughness_tex; // Bindless texture handle
	uint64 lightmap_tex; // Bindless texture handle
	uint64 emission_tex; // Bindless texture handle
	uint64 backface_albedo_tex; // Bindless texture handle
	uint64 transmission_tex; // Bindless texture handle
	uint64 normal_map; // Bindless texture handle

	int flags;
	float roughness;
	float fresnel_scale;
	float metallic_frac;
	float begin_fade_out_distance;
	float end_fade_out_distance;

	float materialise_lower_z;
	float materialise_upper_z;
	float materialise_start_time;

	float padding_b0;
	float padding_b1;
	float padding_b2;
};


// Should be the same layout as in common_frag_structures.glsl
struct MaterialCommonUniforms
{
	Matrix4f frag_view_matrix;
	Vec4f sundir_cs;
	Vec4f sundir_ws;
	Vec4f sun_spec_rad_times_solid_angle; // spectral rad * 1.0e-9 * solid angle
	Vec4f sun_and_sky_av_spec_rad; // spectral rad * 1.0e-9
	Vec4f air_scattering_coeffs;
	float near_clip_dist;
	float far_clip_dist;
	float time;
	float l_over_w; // lens_sensor_dist / sensor width
	float l_over_h; // lens_sensor_dist / sensor height
	float env_phi;
	float water_level_z;
	int camera_type; // OpenGLScene::CameraType

	int mat_common_flags;
	float padding_a0;
	float padding_a1;
	float padding_a2;
};


// Matches DepthUniforms defined in depth_frag_shader.glsl
SSE_CLASS_ALIGN DepthUniforms
{
public:
	Vec2f texture_upper_left_matrix_col0;
	Vec2f texture_upper_left_matrix_col1;
	Vec2f texture_matrix_translation;

	uint64 diffuse_tex; // Bindless texture handle

	int flags;
	float begin_fade_out_distance;
	float end_fade_out_distance;

	float materialise_lower_z;
	float materialise_upper_z;
	float materialise_start_time;
	
	float padding_d0;
	float padding_d1;
};


// Should be the same layout as in common_vert_structures.glsl
struct SharedVertUniforms
{
	Matrix4f proj_matrix; // same for all objects
	Matrix4f view_matrix; // same for all objects
	//#if NUM_DEPTH_TEXTURES > 0
	Matrix4f shadow_texture_matrix[ShadowMapping::NUM_DYNAMIC_DEPTH_TEXTURES + ShadowMapping::NUM_STATIC_DEPTH_TEXTURES]; // same for all objects
	//#endif
	Vec4f campos_ws; // same for all objects
	Vec4f vert_sundir_ws;
	Vec4f grass_pusher_sphere_pos;

	float vert_uniforms_time;
	float wind_strength;

	float padding_0; // AMD drivers have different opinions on if structures are padded than Nvidia drivers, so explicitly pad.
	float padding_1;
};


// Should be the same layout as PerObjectVertUniformsStruct in common_vert_structures.glsl
struct PerObjectVertUniforms
{
	Matrix4f model_matrix; // ob_to_world_matrix
	Matrix4f normal_matrix; // ob_to_world_inv_transpose_matrix

	int light_indices[8];

	float depth_draw_depth_bias;

	float padding_po1;
	float padding_po2;
	float padding_po3;
};


// See https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glMultiDrawElementsIndirect.xhtml
struct DrawElementsIndirectCommand
{
	uint32 count;
	uint32 instanceCount;
	uint32 firstIndex;
	uint32 baseVertex;
	uint32 baseInstance;
};


struct MeshDataLoadingProgress
{
	MeshDataLoadingProgress() : vert_total_size_B(0), vert_next_i(0), index_total_size_B(0), index_next_i(0) {}

	bool done() const { return vert_next_i >= vert_total_size_B && index_next_i >= index_total_size_B; }

	std::string summaryString() const;

	size_t vert_total_size_B;
	size_t vert_next_i;
	size_t index_total_size_B;
	size_t index_next_i;
};

class OpenGLEngine final : public ThreadSafeRefCounted, public AsyncTextureLoadedHandler
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	OpenGLEngine(const OpenGLEngineSettings& settings);
	~OpenGLEngine();

	friend class TextureLoading;
	friend class TerrainSystem;

	//---------------------------- Initialisation/deinitialisation --------------------------
	// data_dir should contain 'gl_data' and 'shaders' dirs.
	void initialise(const std::string& data_dir, Reference<TextureServer> texture_server, PrintOutput* print_output, glare::TaskManager* main_task_manager, glare::TaskManager* high_priority_task_manager,
		Reference<glare::Allocator> mem_allocator); // data_dir should have 'shaders' and 'gl_data' in it.  texture_server can be NULL.
	bool initSucceeded() const { return init_succeeded; }
	std::string getInitialisationErrorMsg() const { return initialisation_error_msg; }

	void startAsyncLoadingData(AsyncTextureLoader* async_texture_loader);
	virtual void textureLoaded(Reference<OpenGLTexture> texture, const std::string& local_filename) override;

	void unloadAllData();

	const std::string& getPreprocessorDefines() const { return preprocessor_defines; } // for compiling shader programs
	const std::string& getPreprocessorDefinesWithCommonVertStructs() const { return preprocessor_defines_with_common_vert_structs; } // for compiling shader programs
	const std::string& getPreprocessorDefinesWithCommonFragStructs() const { return preprocessor_defines_with_common_frag_structs; } // for compiling shader programs
	const std::string& getVersionDirective() const { return version_directive; } // for compiling shader programs

	const std::string& getDataDir() const { return data_dir; }
	//----------------------------------------------------------------------------------------


	//---------------------------- Adding and removing objects -------------------------------

	Reference<GLObject> allocateObject();

	// Add object to world.  Doesn't load textures for object.
	void addObject(const Reference<GLObject>& object);
	void addObjectAndLoadTexturesImmediately(const Reference<GLObject>& object);
	void removeObject(const Reference<GLObject>& object);
	bool isObjectAdded(const Reference<GLObject>& object) const;

	// Overlay objects are considered to be in default (z -1 to 1) OpenGL clip-space coordinates.  (See https://www.songho.ca/opengl/gl_projectionmatrix.html)
	// The x axis is to the right, y up, and positive z away from the camera into the scene.
	// x=-1 is the left edge of the viewport, x=+1 is the right edge.
	// y=-1 is the bottom edge of the viewport, y=+1 is the top edge.
	// z=-1 is the near clip plane, z=1 is the far clip plane
	//
	// Overlay objects are drawn back-to-front, without z-testing, using the overlay shaders.
	void addOverlayObject(const Reference<OverlayObject>& object);
	void removeOverlayObject(const Reference<OverlayObject>& object);
	//----------------------------------------------------------------------------------------


	//---------------------------- Lights ----------------------------------------------------
	void addLight(GLLightRef light);
	void removeLight(GLLightRef light);
	void lightUpdated(GLLightRef light);
	void setLightPos(GLLightRef light, const Vec4f& new_pos);
	//----------------------------------------------------------------------------------------

	//---------------------------- Updating objects ------------------------------------------
	void updateObjectTransformData(GLObject& object); // Updates object ob_to_world_inv_transpose_matrix and aabb_ws, then updates object data on GPU.
	void objectTransformDataChanged(GLObject& object); // Just update object data on GPU.
	const js::AABBox getAABBWSForObjectWithTransform(GLObject& object, const Matrix4f& to_world);

	void newMaterialUsed(OpenGLMaterial& mat, bool use_vert_colours, bool uses_instancing, bool uses_skinning, bool use_vert_tangents);
	void objectMaterialsUpdated(GLObject& object);
	void updateAllMaterialDataOnGPU(GLObject& object); // Don't reassign shaders, just upload material data to GPU for each material
	void materialTextureChanged(GLObject& object, OpenGLMaterial& mat);  // Update material data on GPU

	void objectBatchDataChanged(GLObject& object);
	//----------------------------------------------------------------------------------------


	//---------------------------- Object selection ------------------------------------------
	void selectObject(const Reference<GLObject>& object);
	void deselectObject(const Reference<GLObject>& object);
	void deselectAllObjects();
	void setSelectionOutlineColour(const Colour4f& col);
	void setSelectionOutlineWidth(float line_width_px);
	//----------------------------------------------------------------------------------------


	//---------------------------- Object queries --------------------------------------------
	bool isObjectInCameraFrustum(const GLObject& object);
	//----------------------------------------------------------------------------------------


	//---------------------------- Camera queries --------------------------------------------
	Vec4f getCameraPositionWS() const;

	// Return window coordinates - e.g. coordinates in the viewport, for a given world space position.  (0,0) is the top left of the viewport.
	bool getWindowCoordsForWSPos(const Vec4f& pos_ws, Vec2f& coords_out) const; // Returns true if in front of camera
	//----------------------------------------------------------------------------------------


	//---------------------------- Texture loading -------------------------------------------
	// Return an OpenGL texture based on tex_path.  Loads it from disk if needed.  Blocking.
	// Throws glare::Exception if texture could not be loaded.
	Reference<OpenGLTexture> getTexture(const std::string& tex_path, const TextureParams& params = TextureParams());

	// If the texture identified by key has been loaded into OpenGL, then return the OpenGL texture.
	// If the texture is not loaded, return a null reference.
	Reference<OpenGLTexture> getTextureIfLoaded(const OpenGLTextureKey& key);

	Reference<OpenGLTexture> loadCubeMap(const std::vector<Reference<Map2D> >& face_maps, const TextureParams& params = TextureParams());

	// If the texture identified by key has been loaded into OpenGL, then return the OpenGL texture.
	// Otherwise load the texure from map2d into OpenGL immediately.
	Reference<OpenGLTexture> getOrLoadOpenGLTextureForMap2D(const OpenGLTextureKey& key, const Map2D& map2d, const TextureParams& params = TextureParams());

	void addOpenGLTexture(const OpenGLTextureKey& key, const Reference<OpenGLTexture>& tex); // Adds to opengl_textures.  Assigns texture to all inserted objects that are using it according to opengl_mat.tex_path.

	void removeOpenGLTexture(const OpenGLTextureKey& key); // Erases from opengl_textures.

	bool isOpenGLTextureInsertedForKey(const OpenGLTextureKey& key) const;

	Reference<TextureServer>& getTextureServer() { return texture_server; } // May be NULL

	bool textureCompressionSupportedAndEnabled() const { return texture_compression_s3tc_support && settings.compress_textures; }

	TextureAllocator& getTextureAllocator() { return texture_allocator; }
	//------------------------------- End texture loading ------------------------------------

	//---------------------------- Built-in texture access -------------------------------------------
	OpenGLTexture& getFBMTex() const { return *fbm_tex; }
	//----------------------------------------------------------------------------------------


	//------------------------------- Camera management --------------------------------------
	void setPerspectiveCameraTransform(const Matrix4f& world_to_camera_space_matrix, float sensor_width, float lens_sensor_dist, float render_aspect_ratio, float lens_shift_up_distance,
		float lens_shift_right_distance);

	// If world_to_camera_space_matrix is the identity, results in a camera, with forwards = +y, right = +x, up = +z. 
	// Left clip plane will be at camera position x - sensor_width/2, right clip plane at camera position x + sensor_width/2
	// Cam space width = sensor_width.
	void setOrthoCameraTransform(const Matrix4f& world_to_camera_space_matrix, float sensor_width, float render_aspect_ratio, float lens_shift_up_distance,
		float lens_shift_right_distance);

	void setDiagonalOrthoCameraTransform(const Matrix4f& world_to_camera_space_matrix, float sensor_width, float render_aspect_ratio);

	void setIdentityCameraTransform(); // See also use_z_up to use z-up like opengl.
	//----------------------------------------------------------------------------------------


	//------------------------------- Environment material/map management --------------------
	void setSunDir(const Vec4f& d);
	const Vec4f getSunDir() const;

	void setEnvMapTransform(const Matrix3f& transform);

	void setEnvMat(const OpenGLMaterial& env_mat);
	const OpenGLMaterial& getEnvMat() const { return current_scene->env_ob->materials[0]; }

	void setCirrusTexture(const Reference<OpenGLTexture>& tex);

	//void setSnowIceTexture(const Reference<OpenGLTexture>& tex) { this->snow_ice_normal_map = tex; }
	//----------------------------------------------------------------------------------------

	//------------------------------- Terrain --------------------
	void setDetailTexture(int index, const OpenGLTextureRef& tex);
	void setDetailHeightmap(int index, const OpenGLTextureRef& tex);
	OpenGLTextureRef getDetailTexture(int index) const;
	OpenGLTextureRef getDetailHeightmap(int index) const;
	//----------------------------------------------------------------------------------------


	//--------------------------------------- Drawing ----------------------------------------
	void setDrawWireFrames(bool draw_wireframes_) { draw_wireframes = draw_wireframes_; }
	void setMaxDrawDistance(float d) { current_scene->max_draw_dist = d; } // Set far draw distance
	void setNearDrawDistance(float d) { current_scene->near_draw_dist = d; } // Set near draw distance
	void setCurrentTime(float time);
	float getCurrentTime() const { return current_time; }

	void draw();
	//----------------------------------------------------------------------------------------


	//--------------------------------------- Viewport ----------------------------------------
	// This will be the resolution at which the main render framebuffer is allocated, for which res bloom textures are allocated etc..
	void setMainViewportDims(int main_viewport_w_, int main_viewport_h_);
	int getMainViewPortWidth()  const { return main_viewport_w; } // Return viewport width, in pixels.
	int getMainViewPortHeight() const { return main_viewport_h; }

	void setViewportDims(int viewport_w_, int viewport_h_);
	void setViewportDims(const Vec2i& viewport_dims) { setViewportDims(viewport_dims.x, viewport_dims.y); }
	Vec2i getViewportDims() const { return Vec2i(viewport_w, viewport_h); }
	int getViewPortWidth()  const { return viewport_w; } // Return viewport width, in pixels.
	int getViewPortHeight() const { return viewport_h; }
	float getViewPortAspectRatio() const { return (float)getViewPortWidth() / (float)(getViewPortHeight()); } // Viewport width / viewport height.
	//----------------------------------------------------------------------------------------


	//----------------------------------- Mesh functions -------------------------------------
	Reference<OpenGLMeshRenderData> getLineMeshData(); // A line from (0, 0, 0) to (1, 0, 0)
	Reference<OpenGLMeshRenderData> getSphereMeshData();
	Reference<OpenGLMeshRenderData> getCubeMeshData(); // Bottom left corner will be at origin, opposite corner will lie at (1, 1, 1)
	Reference<OpenGLMeshRenderData> getUnitQuadMeshData(); // A quad from (0, 0, 0) to (1, 1, 0)
	Reference<OpenGLMeshRenderData> getCylinderMesh(); // A cylinder from (0,0,0), to (0,0,1) with radius 1;
	Reference<OpenGLMeshRenderData> getSpriteQuadMeshData() { return sprite_quad_meshdata; }

	GLObjectRef makeAABBObject(const Vec4f& min_, const Vec4f& max_, const Colour4f& col);
	GLObjectRef makeArrowObject(const Vec4f& startpos, const Vec4f& endpos, const Colour4f& col, float radius_scale);
	GLObjectRef makeSphereObject(float radius, const Colour4f& col);
	GLObjectRef makeCylinderObject(float radius, const Colour4f& col); // A cylinder from (0,0,0), to (0,0,1) with radius 1;

	static Matrix4f AABBObjectTransform(const Vec4f& min_os, const Vec4f& max_os);
	static Matrix4f arrowObjectTransform(const Vec4f& startpos, const Vec4f& endpos, float radius_scale);
	

	static Reference<OpenGLMeshRenderData> buildMeshRenderData(VertexBufferAllocator& allocator, const js::Vector<Vec3f, 16>& vertices, const js::Vector<Vec3f, 16>& normals, const js::Vector<Vec2f, 16>& uvs, const js::Vector<uint32, 16>& indices);

	static void initialiseMeshDataLoadingProgress(OpenGLMeshRenderData& data, MeshDataLoadingProgress& loading_progress);
	static void partialLoadOpenGLMeshDataIntoOpenGL(VertexBufferAllocator& allocator, OpenGLMeshRenderData& data, MeshDataLoadingProgress& loading_progress, size_t& total_bytes_uploaded_in_out, size_t max_total_upload_bytes);

	static void loadOpenGLMeshDataIntoOpenGL(VertexBufferAllocator& allocator, OpenGLMeshRenderData& data);
	//---------------------------- End mesh functions ----------------------------------------


	//----------------------------------- Readback -------------------------------------------
	float getPixelDepth(int pixel_x, int pixel_y);

	Reference<ImageMap<uint8, UInt8ComponentValueTraits> > getRenderedColourBuffer();
	//----------------------------------------------------------------------------------------


	//----------------------------------- Target framebuffer ---------------------------------
	// Set the primary render target frame buffer.  Can be NULL in which case framebuffer 0 is drawn to.
	void setTargetFrameBuffer(const Reference<FrameBuffer> frame_buffer) { target_frame_buffer = frame_buffer; }
	void setTargetFrameBufferAndViewport(const Reference<FrameBuffer> frame_buffer); // Set target framebuffer, also set viewport to the whole framebuffer.

	Reference<FrameBuffer> getTargetFrameBuffer() const { return target_frame_buffer; }
	//----------------------------------------------------------------------------------------
	

	//----------------------------------- Scene management -----------------------------------
	void addScene(const Reference<OpenGLScene>& scene);
	void removeScene(const Reference<OpenGLScene>& scene);
	void setCurrentScene(const Reference<OpenGLScene>& scene);
	OpenGLScene* getCurrentScene() { return current_scene.ptr(); }
	//----------------------------------------------------------------------------------------


	//----------------------------------- Diagnostics ----------------------------------------
	GLMemUsage getTotalMemUsage() const;

	std::string getDiagnostics() const;

	void setProfilingEnabled(bool enabled) { query_profiling_enabled = enabled; }
	//----------------------------------------------------------------------------------------

	//----------------------------------- Settings ----------------------------------------
	//void setMSAAEnabled(bool enabled);

	bool openglDriverVendorIsIntel() const; // Works after opengl_vendor is set in initialise().
	bool openglDriverVendorIsATI() const; // Works after opengl_vendor is set in initialise().
	//----------------------------------------------------------------------------------------


	void renderMaskMap(OpenGLTexture& mask_map_texture, const Vec2f& botleft_pos, float world_capture_width);


	void shaderFileChanged(); // Called by ShaderFileWatcherThread, from another thread.
private:
	void loadMapsForSunDir();
	void buildObjectData(const Reference<GLObject>& object);
	void rebuildDenormalisedDrawData(GLObject& ob);
	void rebuildObjectDepthDrawBatches(GLObject& ob);
	void updateMaterialDataOnGPU(const GLObject& ob, size_t mat_index);
	void calcCamFrustumVerts(float near_dist, float far_dist, Vec4f* verts_out) const;
	void assignLightsToObject(GLObject& ob);
	void assignLightsToAllObjects();
public:
	void assignShaderProgToMaterial(OpenGLMaterial& material, bool use_vert_colours, bool uses_instancing, bool uses_skinning, bool use_vert_tangents);
private:
	// Set uniforms that are the same for every batch for the duration of this frame.
	void setSharedUniformsForProg(const OpenGLProgram& shader_prog, const Matrix4f& view_mat, const Matrix4f& proj_mat);
	void drawBatch(const GLObject& ob, const OpenGLMaterial& opengl_mat, const OpenGLProgram& shader_prog, const OpenGLMeshRenderData& mesh_data, const OpenGLBatch& batch);
	void drawBatchWithDenormalisedData(const GLObject& ob, const GLObjectBatchDrawInfo& batch, uint32 batch_index);
	void buildOutlineTextures();
private:
	void addDebugSphere(const Vec4f& centre, float radius, const Colour4f& col);
	void addDebugLine(const Vec4f& start_point, const Vec4f& end_point, float radius, const Colour4f& col);
	void addDebugPlane(const Vec4f& point_on_plane, const Vec4f& plane_normal, float plane_draw_width, const Colour4f& col);
	void addDebugLinesForFrustum(const Vec4f* frustum_verts_ws, const Vec4f& t, float line_rad, const Colour4f& line_col);

public:
	static void getUniformLocations(Reference<OpenGLProgram>& prog);
private:
	static void setStandardTextureUnitUniformsForProgram(const OpenGLProgram& program);
	static void doSetStandardTextureUnitUniformsForBoundProgram(const OpenGLProgram& program);
	void setUniformsForPhongProg(const OpenGLMaterial& opengl_mat, const OpenGLMeshRenderData& mesh_data, PhongUniforms& uniforms) const;
	void bindTexturesForPhongProg(const OpenGLMaterial& opengl_mat) const;
	void partiallyClearBuffer(const Vec2f& begin, const Vec2f& end);
	Matrix4f getReverseZMatrixOrIdentity() const;

	OpenGLProgramRef getProgramWithFallbackOnError(const ProgramKey& key);

	OpenGLProgramRef getPhongProgram(const ProgramKey& key); // Throws glare::Exception on shader compilation failure.
	OpenGLProgramRef getTransparentProgram(const ProgramKey& key); // Throws glare::Exception on shader compilation failure.
	OpenGLProgramRef getImposterProgram(const ProgramKey& key); // Throws glare::Exception on shader compilation failure.
	OpenGLProgramRef getDepthDrawProgram(const ProgramKey& key); // Throws glare::Exception on shader compilation failure.
	OpenGLProgramRef getDepthDrawProgramWithFallbackOnError(const ProgramKey& key);
	OpenGLProgramRef buildEnvProgram(const std::string& use_shader_dir);
	OpenGLProgramRef buildAuroraProgram(const std::string& use_shader_dir);
public:
	OpenGLProgramRef buildProgram(const std::string& shader_name_prefix, const ProgramKey& key); // Throws glare::Exception on shader compilation failure.
	uint32 getAndIncrNextProgramIndex() { return next_program_index++; }
	void addProgram(OpenGLProgramRef);

	glare::TaskManager* getMainTaskManager() { return main_task_manager; }

	void textureBecameUnused(const OpenGLTexture* tex);
	void textureBecameUsed(const OpenGLTexture* tex);
private:
	void trimTextureUsage();
	void bindMeshData(const OpenGLMeshRenderData& mesh_data);
	void bindMeshData(const GLObject& ob);
	bool checkUseProgram(const OpenGLProgram* prog);
	bool checkUseProgram(uint32 prog_index);
	void submitBufferedDrawCommands();
	void sortBatchDrawInfos();
	void flushDrawCommandsAndUnbindPrograms();
	void sortBatchDrawInfoWithDists();
	int getCameraShadowMappingPlanesAndAABB(float near_dist, float far_dist, float max_shadowing_dist, Planef* shadow_clip_planes_out, js::AABBox& shadow_vol_aabb_out);
	void assignLoadedTextureToObMaterials(const std::string& path, Reference<OpenGLTexture> opengl_texture);
	void expandPerObVertDataBuffer();
	void expandPhongBuffer();
	void expandJointMatricesBuffer(size_t min_extra_needed);
	void checkMDIGPUDataCorrect();
	int allocPerObVertDataBufferSpot();
	void addDebugVisForShadowFrustum(const Vec4f frustum_verts_ws[8], float max_shadowing_dist, const Planef clip_planes[18], int num_clip_planes_used);
	void renderToShadowMapDepthBuffer(uint64& shadow_depth_drawing_elapsed_ns);
	void drawUIOverlayObjects(const Matrix4f& reverse_z_matrix);
	void generateOutlineTexture(const Matrix4f& view_matrix, const Matrix4f& proj_matrix);
	void drawOutlinesAroundSelectedObjects();
	void drawAlwaysVisibleObjects(const Matrix4f& view_matrix, const Matrix4f& proj_matrix);
	void drawTransparentMaterialBatches(const Matrix4f& view_matrix, const Matrix4f& proj_matrix);
	void drawNonTransparentMaterialBatches(const Matrix4f& view_matrix, const Matrix4f& proj_matrix);
	void drawWaterObjects(const Matrix4f& view_matrix, const Matrix4f& proj_matrix);
	void drawDecals(const Matrix4f& view_matrix, const Matrix4f& proj_matrix);
	void drawParticipatingMediaObjects(const Matrix4f& view_matrix, const Matrix4f& proj_matrix);
	void drawBackgroundEnvMap(const Matrix4f& view_matrix, const Matrix4f& proj_matrix);
	void drawAuroraTex();
	void buildPrograms(const std::string& use_shader_dir);
	void bindStandardTexturesToTextureUnits();
	void bindStandardShadowMappingDepthTextures();

	bool init_succeeded;
	std::string initialisation_error_msg;
	
	Vec4f sun_dir; // Dir to sun.
	Vec4f sun_dir_cam_space;
	Vec4f sun_spec_rad_times_solid_angle;
	Vec4f sun_and_sky_av_spec_rad;
	Vec4f air_scattering_coeffs;
	float sun_phi;

	bool loaded_maps_for_sun_dir;

	Reference<OpenGLMeshRenderData> line_meshdata;
	Reference<OpenGLMeshRenderData> sphere_meshdata;
	Reference<OpenGLMeshRenderData> arrow_meshdata;
	Reference<OpenGLMeshRenderData> cube_meshdata;
	Reference<OpenGLMeshRenderData> unit_quad_meshdata;
	Reference<OpenGLMeshRenderData> cylinder_meshdata;
	Reference<OpenGLMeshRenderData> sprite_quad_meshdata;


	int main_viewport_w, main_viewport_h;

	int viewport_w, viewport_h;
	
	js::Vector<OverlayObject*, 16> temp_obs;

#if !defined(OSX)
	GLuint timer_query_id;
#endif

	//uint64 num_face_groups_submitted;
	uint32 num_indices_submitted;
	//uint64 num_aabbs_submitted;

	std::string preprocessor_defines;
	std::string preprocessor_defines_with_common_vert_structs;
	std::string preprocessor_defines_with_common_frag_structs;
	std::string version_directive;

	// Map from preprocessor defs to built program.
	std::map<ProgramKey, OpenGLProgramRef> progs;
	std::vector<OpenGLProgramRef> prog_vector; // prog_vector[i]->program_index == i.
	OpenGLProgramRef fallback_phong_prog;
	OpenGLProgramRef fallback_transparent_prog;
	OpenGLProgramRef fallback_depth_prog;
	uint32 next_program_index;

	Reference<OpenGLProgram> env_prog;
	int env_diffuse_colour_location;
	int env_have_texture_location;
	int env_texture_matrix_location;
	int env_campos_ws_location;

	ImageMapFloatRef fbm_imagemap;
	Reference<OpenGLTexture> fbm_tex;
	Reference<OpenGLTexture> detail_tex[4];
	Reference<OpenGLTexture> detail_heightmap[4];
	Reference<OpenGLTexture> blue_noise_tex;
	Reference<OpenGLTexture> noise_tex;
	Reference<OpenGLTexture> cirrus_tex; // May be NULL, set by setCirrusTexture().
	Reference<OpenGLTexture> aurora_tex;
	//Reference<OpenGLTexture> snow_ice_normal_map;

	std::vector<Reference<OpenGLTexture>> water_caustics_textures;

	Reference<OpenGLProgram> overlay_prog;
	int overlay_diffuse_colour_location;
	int overlay_have_texture_location;
	int overlay_diffuse_tex_location;
	int overlay_texture_matrix_location;

	Reference<OpenGLProgram> clear_prog;

	Reference<OpenGLProgram> outline_prog_no_skinning; // Used for drawing constant flat shaded pixels currently.
	Reference<OpenGLProgram> outline_prog_with_skinning; // Used for drawing constant flat shaded pixels currently.

	Reference<OpenGLProgram> edge_extract_prog;
	int edge_extract_tex_location;
	int edge_extract_col_location;
	int edge_extract_line_width_location;

	Colour4f outline_colour;
	float outline_width_px;

	Reference<OpenGLProgram> downsize_prog;
	Reference<OpenGLProgram> downsize_from_main_buf_prog;
	Reference<OpenGLProgram> gaussian_blur_prog;

	Reference<OpenGLProgram> final_imaging_prog; // Adds bloom, tonemaps

	Reference<OpenGLProgram> draw_aurora_tex_prog;

	//size_t vert_mem_used; // B
	//size_t index_mem_used; // B

	std::string data_dir;

	Reference<ShadowMapping> shadow_mapping;
public:
	//Reference<TerrainSystem> terrain_system;
private:
	OverlayObjectRef clear_buf_overlay_ob;

	double draw_time;
	Timer draw_timer;

	ManagerWithCache<OpenGLTextureKey, Reference<OpenGLTexture>, OpenGLTextureKeyHash> opengl_textures;
private:
	size_t outline_tex_w, outline_tex_h;
	Reference<FrameBuffer> outline_solid_framebuffer;
	Reference<FrameBuffer> outline_edge_framebuffer;
	Reference<OpenGLTexture> outline_solid_tex;
	Reference<OpenGLTexture> outline_edge_tex;
	OpenGLMaterial outline_solid_mat; // Material for drawing selected objects with a flat, unshaded colour.
	Reference<OpenGLMeshRenderData> outline_quad_meshdata;
	OpenGLMaterial outline_edge_mat; // Material for drawing the actual edge overlay.

	std::vector<Reference<FrameBuffer> > downsize_framebuffers;
	std::vector<Reference<OpenGLTexture> > downsize_target_textures;

	std::vector<Reference<FrameBuffer> > blur_framebuffers_x; // Blurred in x direction
	std::vector<Reference<OpenGLTexture> > blur_target_textures_x;

	std::vector<Reference<FrameBuffer> > blur_framebuffers; // Blurred in both directions:
	std::vector<Reference<OpenGLTexture> > blur_target_textures;

	Reference<FrameBuffer> main_render_framebuffer;
	Reference<FrameBuffer> main_render_copy_framebuffer;

	Reference<RenderBuffer> main_colour_renderbuffer;
	Reference<RenderBuffer> main_normal_renderbuffer;
	Reference<RenderBuffer> main_depth_renderbuffer;
	Reference<RenderBuffer> transparent_accum_renderbuffer;
	Reference<RenderBuffer> av_transmittance_renderbuffer;

	OpenGLTextureRef main_colour_copy_texture;
	OpenGLTextureRef main_depth_copy_texture;
	OpenGLTextureRef main_normal_copy_texture;
	OpenGLTextureRef transparent_accum_copy_texture;
	OpenGLTextureRef av_transmittance_copy_texture;

	std::unordered_set<GLObject*> selected_objects;

	bool draw_wireframes;

	GLObjectRef debug_arrow_ob;
	GLObjectRef debug_sphere_ob;

	std::vector<GLObjectRef> debug_joint_obs;

	std::vector<GLObjectRef> debug_draw_obs;

	Reference<OpenGLTexture> cosine_env_tex;
	Reference<OpenGLTexture> specular_env_tex;

	float current_time;

	Reference<TextureServer> texture_server;

	Reference<FrameBuffer> target_frame_buffer;

	glare::TaskManager* main_task_manager; // Used for building 8-bit texture data (DXT compression, mip-map data building).
	glare::TaskManager* high_priority_task_manager; // For short, processor intensive tasks that the main thread depends on, such as computing animation data for the current frame
public:
	std::string opengl_vendor;
	std::string opengl_renderer;
	std::string opengl_version;
	std::string glsl_version;

	bool texture_sRGB_support;
	bool texture_compression_s3tc_support;
	bool GL_ARB_bindless_texture_support;
	bool GL_ARB_clip_control_support;
	bool GL_ARB_shader_storage_buffer_object_support;
	bool parallel_shader_compile_support;
	float max_anisotropy;
	int max_texture_size;
	bool use_bindless_textures;
	bool use_multi_draw_indirect;
	bool use_reverse_z;
	bool use_scatter_shader; // Use scatter shader for data updates
	bool use_order_indep_transparency;

	OpenGLEngineSettings settings;

	uint64 shadow_mapping_frame_num;

	bool are_8bit_textures_sRGB; // If true, load textures as sRGB, otherwise treat them as in an 8-bit colour space.  Defaults to true.
	// Currently compressed textures are always treated as sRGB.

	bool add_debug_obs;
private:
	std::unordered_set<Reference<OpenGLScene>, OpenGLSceneHash> scenes;
	Reference<OpenGLScene> current_scene;

	PCG32 rng;

	uint64 last_num_obs_in_frustum;

	js::Vector<BatchDrawInfo, 16> batch_draw_info;
	js::Vector<BatchDrawInfo, 16> temp_batch_draw_info;
	js::Vector<BatchDrawInfoWithDist, 16> batch_draw_info_dist;
	js::Vector<BatchDrawInfoWithDist, 16> temp_batch_draw_info_dist;
	std::vector<uint32> temp_counts;
	uint32 num_prog_changes;
	uint32 num_vao_binds;
	uint32 num_vbo_binds;
	uint32 num_index_buf_binds;
	
	uint32 last_num_prog_changes;
	uint32 last_num_batches_bound;
	uint32 last_num_vao_binds;
	uint32 last_num_vbo_binds;
	uint32 last_num_index_buf_binds;
	uint32 last_num_indices_drawn;
	uint32 last_num_backface_culling_changes;

	uint32 depth_draw_last_num_prog_changes;
	uint32 depth_draw_last_num_batches_bound;
	uint32 depth_draw_last_num_vao_binds;
	uint32 depth_draw_last_num_vbo_binds;
	uint32 depth_draw_last_num_indices_drawn;

	uint32 last_num_animated_obs_processed;

	uint32 num_multi_draw_indirect_calls;

	Timer fps_display_timer;
	int num_frames_since_fps_timer_reset;


	double last_anim_update_duration;
	double last_depth_map_gen_GPU_time;
	double last_render_GPU_time;
	double last_draw_CPU_time;
	double last_fps;

	bool query_profiling_enabled;


	UniformBufObRef phong_uniform_buf_ob; // Used for transparent mats also.
	UniformBufObRef material_common_uniform_buf_ob;
	UniformBufObRef depth_uniform_buf_ob;
	UniformBufObRef shared_vert_uniform_buf_ob;
	UniformBufObRef per_object_vert_uniform_buf_ob;
	UniformBufObRef joint_matrices_buf_ob;

	// Some temporary vectors:
	js::Vector<Matrix4f, 16> temp_matrices;


	// For MDI:
	js::Vector<uint32, 16> ob_and_mat_indices_buffer;
	SSBORef ob_and_mat_indices_ssbo;
	OpenGLCircularBuffer ob_and_mat_indices_circ_buf;

	DrawIndirectBufferRef draw_indirect_buffer;
	OpenGLCircularBuffer draw_indirect_circ_buf;

	js::Vector<DrawElementsIndirectCommand, 16> draw_commands;
	
	GLenum current_index_type;
	const OpenGLProgram* current_bound_prog;
	uint32 current_bound_prog_index;
	const VAO* current_bound_VAO;
	const GLObject* current_uniforms_ob; // Last object for which we uploaded joint matrices, and set the per_object_vert_uniform_buf_ob.  Used to stop uploading the same uniforms repeatedly for the same object.

	
public:
	PrintOutput* print_output; // May be NULL

	uint64 tex_CPU_mem_usage; // Running sum of CPU RAM used by inserted textures.
	uint64 tex_GPU_mem_usage; // Running sum of GPU RAM used by inserted textures.

	uint64 max_tex_CPU_mem_usage;
	uint64 max_tex_GPU_mem_usage;

	VertexBufferAllocatorRef vert_buf_allocator;

	uint64 total_available_GPU_mem_B; // Set by NVidia drivers
	uint64 total_available_GPU_VBO_mem_B; // Set by AMD drivers

	Reference<glare::Allocator> mem_allocator;

private:
	glare::PoolAllocator object_pool_allocator;

	SSBORef light_buffer; // SSBO
	UniformBufObRef light_ubo; // UBO for Mac.

	std::set<int> light_free_indices;

	std::set<int> per_ob_vert_data_free_indices;
	SSBORef per_ob_vert_data_buffer; // SSBO

	std::set<int> phong_buffer_free_indices;
	SSBORef phong_buffer; // SSBO

	SSBORef joint_matrices_ssbo;
	Reference<glare::BestFitAllocator> joint_matrices_allocator;

	glare::AtomicInt shader_file_changed;

	ThreadManager thread_manager; // For ShaderFileWatcherThread

	bool running_in_renderdoc;

	js::Vector<GLObject*, 16> animated_obs_to_process;
	std::vector<glare::TaskRef> animated_objects_tasks;
	glare::TaskGroupRef animated_objects_task_group;


	js::Vector<uint8, 16> data_updates_buffer;
	SSBORef data_updates_ssbo;
	OpenGLProgramRef scatter_data_prog;

	TextureAllocator texture_allocator;
public:
	Reference<FrameBuffer> mask_map_frame_buffer;
private:
	Reference<FrameBuffer> aurora_tex_frame_buffer;

	AsyncTextureLoader* async_texture_loader;
	std::vector<AsyncTextureLoadingHandle> loading_handles;
};


#ifdef _WIN32
#pragma warning(pop)
#endif


#define checkForOpenGLErrorsAtLocation() (doCheckForOpenGLErrorsAtLocation((__LINE__), (__FILE__)))


void checkForOpenGLErrors();
void doCheckForOpenGLErrorsAtLocation(long line, const char* file);
void checkUniformBlockSize(OpenGLProgramRef prog, const char* block_name, size_t target_size);
void bindShaderStorageBlockToProgram(OpenGLProgramRef prog, const char* name, int binding_point_index);

