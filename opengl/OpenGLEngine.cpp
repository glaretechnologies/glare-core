/*=====================================================================
OpenGLEngine.cpp
----------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#include "OpenGLEngine.h"


#include "IncludeOpenGL.h"
#include "OpenGLProgram.h"
#include "OpenGLShader.h"
#include "RenderBuffer.h"
#include "ShadowMapping.h"
#include "OpenGLExtensions.h"
#include "GLMeshBuilding.h"
#include "MeshPrimitiveBuilding.h"
#include "OpenGLMeshRenderData.h"
#include "ShaderFileWatcherThread.h"
#include "AsyncTextureLoader.h"
#include "Query.h"
#include "TimestampQuery.h"
#include "BufferedTimeElapsedQuery.h"
#include "../graphics/TextureProcessing.h"
#include "../graphics/ImageMap.h"
#include "../graphics/SRGBUtils.h"
#include "../graphics/PerlinNoise.h"
//#include "../utils/IncludeHalf.h"
#ifndef NO_EXR_SUPPORT
#include "../graphics/EXRDecoder.h"
#endif
#include "../graphics/imformatdecoder.h"
#include "../graphics/CompressedImage.h"
#include "../indigo/TextureServer.h"
#include "../utils/TestUtils.h"
#include "../maths/vec3.h"
#include "../maths/GeometrySampling.h"
#include "../maths/matrix3.h"
#include "../utils/Lock.h"
#include "../utils/Timer.h"
#include "../utils/Platform.h"
#include "../utils/FileUtils.h"
#include "../utils/Reference.h"
#include "../utils/StringUtils.h"
#include "../utils/Exception.h"
#include "../utils/BitUtils.h"
#include "../utils/Vector.h"
#include "../utils/Task.h"
#include "../utils/ArrayRef.h"
#include "../utils/TaskManager.h"
#include "../utils/Sort.h"
#include "../utils/ContainerUtils.h"
#include "../utils/RuntimeCheck.h"
#include "../utils/BestFitAllocator.h"
#include "../utils/PlatformUtils.h"
#include <algorithm>
#include "superluminal/PerformanceAPI.h"
#include <tracy/Tracy.hpp>
#include <tracy/TracyOpenGL.hpp>
#if EMSCRIPTEN
#define GL_GLEXT_PROTOTYPES 1
#include <GLES3/gl2ext.h>
#define glClipControl glClipControlEXT
#endif


// https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_filter_anisotropic.txt
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT				0x84FF
#define GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT			0x8E8F

// https://developer.download.nvidia.com/opengl/specs/GL_NVX_gpu_memory_info.txt
#define GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX			0x9047
#define GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX		0x9048
#define GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX	0x9049

// https://www.khronos.org/registry/OpenGL/extensions/ATI/ATI_meminfo.txt
#define VBO_FREE_MEMORY_ATI								0x87FB
#define TEXTURE_FREE_MEMORY_ATI							0x87FC

// Just for Mac
#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER						0x90D2
#endif
#ifndef GL_COMPUTE_SHADER
#define GL_COMPUTE_SHADER								0x91B9
#endif

#define GL_LOWER_LEFT									0x8CA1
#define GL_ZERO_TO_ONE									0x935F
#define GL_NEGATIVE_ONE_TO_ONE							0x935E


// Use circular buffers for feeding draw commands and object indices to multi-draw-indirect?
// Currently this has terrible performance, so don't use, but should in theory be better.
// #define USE_CIRCULAR_BUFFERS_FOR_MDI	1

// If Using a core OpenGL profile, data has to come from a GPU buffer, not a client buffer.
// See https://github.com/KhronosGroup/OpenGL-API/issues/65
#define USE_DRAW_INDIRECT_BUFFER_FOR_MDI 1


// mat_common_flags values.  Should be same as in common_frag_structures.glsl
#define CLOUD_SHADOWS_FLAG					1
#define DO_SSAO_FLAG						2
#define DOING_SSAO_PREPASS_FLAG				4


#define OVERLAY_HAVE_TEXTURE_FLAG			1
#define OVERLAY_TARGET_IS_NONLINEAR_FLAG	2
#define OVERLAY_SHOW_JUST_TEX_RGB_FLAG		4
#define OVERLAY_SHOW_JUST_TEX_W_FLAG		8



// Needs to match definition in common_vert_structures.glsl
#define OB_AND_MAT_INDICES_STRIDE			3


static const size_t max_num_joint_matrices_per_ob = 256; // Max num joint matrices per object.


static const bool draw_aurora = true;

#if EMSCRIPTEN
const bool time_individual_passes = false; // Chrome doesn't support timestamp queries (https://codereview.chromium.org/1800383002), so we have to use an elapsed timer, which doesn't nest, so we cant time individual passes as well.
#else
const bool time_individual_passes = true;
#endif


// 'Standard' textures will be bound to standard texture units, for example FBM texture will always be bound to texture unit 6.
// We won't use texture unit index 0, that will be a scratch index which will get overwritten when creating new textures (which calls glBindTexture()) etc.
enum TextureUnitIndices
{
	DEPTH_TEX_TEXTURE_UNIT_INDEX = 1,
	STATIC_DEPTH_TEX_TEXTURE_UNIT_INDEX,

	COSINE_ENV_TEXTURE_UNIT_INDEX,
	SPECULAR_ENV_TEXTURE_UNIT_INDEX,
	BLUE_NOISE_TEXTURE_UNIT_INDEX,
	FBM_TEXTURE_UNIT_INDEX,

	DIFFUSE_TEXTURE_UNIT_INDEX,
	LIGHTMAP_TEXTURE_UNIT_INDEX,
	BACKFACE_ALBEDO_TEXTURE_UNIT_INDEX,
	TRANSMISSION_TEXTURE_UNIT_INDEX,
	METALLIC_ROUGHNESS_TEXTURE_UNIT_INDEX,
	EMISSION_TEXTURE_UNIT_INDEX,
	NORMAL_MAP_TEXTURE_UNIT_INDEX,
	COMBINED_ARRAY_TEXTURE_UNIT_INDEX,

	MAIN_COLOUR_COPY_TEXTURE_UNIT_INDEX,
	MAIN_NORMAL_COPY_TEXTURE_UNIT_INDEX,
	MAIN_DEPTH_COPY_TEXTURE_UNIT_INDEX,

	CIRRUS_TEX_TEXTURE_UNIT_INDEX,
	CAUSTIC_A_TEXTURE_UNIT_INDEX,
	CAUSTIC_B_TEXTURE_UNIT_INDEX,

	DETAIL_0_TEXTURE_UNIT_INDEX,
	DETAIL_1_TEXTURE_UNIT_INDEX,
	DETAIL_2_TEXTURE_UNIT_INDEX,
	DETAIL_3_TEXTURE_UNIT_INDEX,
	DETAIL_HEIGHTMAP_TEXTURE_UNIT_INDEX,

	AURORA_TEXTURE_UNIT_INDEX,
	//SNOW_ICE_NORMAL_MAP_TEXTURE_UNIT_INDEX

	SSAO_TEXTURE_UNIT_INDEX,
	SSAO_SPECULAR_TEXTURE_UNIT_INDEX,
	PREPASS_COLOUR_COPY_TEXTURE_UNIT_INDEX,
	PREPASS_NORMAL_COPY_TEXTURE_UNIT_INDEX,
	PREPASS_DEPTH_COPY_TEXTURE_UNIT_INDEX
};


static inline uint32 indexTypeSizeBytes(GLenum index_type)
{
	if(index_type == GL_UNSIGNED_INT)
		return 4;
	else if(index_type == GL_UNSIGNED_SHORT)
		return 2;
	else
	{
		assert(index_type == GL_UNSIGNED_BYTE);
		return 1;
	}
}


static inline uint32 indexTypeLog2SizeBytes(GLenum index_type)
{
	return Maths::intLogBase2(indexTypeSizeBytes(index_type));
}


// For grouping OpenGL API calls into groups in RenderDoc.
class DebugGroup
{
public:
	DebugGroup(const char* name)
	{
#if BUILD_TESTS && !defined(EMSCRIPTEN) && !defined(__APPLE__)
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, /*id=*/0, /*length=*/(GLsizei)std::strlen(name), name);
#endif
	}

	~DebugGroup()
	{
#if BUILD_TESTS && !defined(EMSCRIPTEN) && !defined(__APPLE__)
		glPopDebugGroup();
#endif
	}
};


GLObject::GLObject() noexcept
	: object_type(0), line_width(1.f), random_num(0), current_anim_i(0), next_anim_i(-1), transition_start_time(-2), transition_end_time(-1), use_time_offset(0), is_imposter(false),
	num_instances_to_draw(0), always_visible(false), draw_to_mask_map(false)/*, allocator(NULL)*/, refcount(0), per_ob_vert_data_index(-1), joint_matrices_block(NULL), joint_matrices_base_index(-1), morph_start_dist(0), morph_end_dist(0),
	depth_draw_depth_bias(0), ob_to_world_matrix_determinant(1.f)
{
	for(int i=0; i<MAX_NUM_LIGHT_INDICES; ++i)
		light_indices[i] = -1;

	// Denormalised mesh data:
	vao = NULL;
	vert_vbo = NULL;
	index_vbo = NULL;
	index_type_and_log2_size = 0;
	instance_matrix_vbo_name = 0;
	indices_vbo_handle_offset = 0;
	vbo_handle_base_vertex = 0;
}


GLObject::~GLObject()
{}


void doDestroyGLOb(GLObject* ob)
{
	/*if(ob->allocator)
	{
		glare::PoolAllocator* allocator = ob->allocator;
		const int allocation_index = ob->allocation_index; // Copy allocation_index into a local variable before we destroy the object.
		ob->~GLObject(); // Call destructor
		allocator->free(allocation_index); // Free memory
	}
	else*/
		delete ob;
}


void GLObject::enableInstancing(VertexBufferAllocator& /*vert_allocator*/, const void* instance_matrix_data, size_t instance_matrix_data_size)
{
	VBORef new_instance_matrix_vbo = new VBO(instance_matrix_data, instance_matrix_data_size, GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);

	//instance_matrix_vbo_handle = allocator.allocate(instance_matrix_data, instance_matrix_data_size
	instance_matrix_vbo = new_instance_matrix_vbo;
	num_instances_to_draw = (int)(new_instance_matrix_vbo->getSize() / sizeof(Matrix4f));

	// Make sure instance matrix attributes use the instance_matrix_vbo.

	VertexSpec vertex_spec = mesh_data->vertex_spec;

	for(size_t i = 5; i < myMin<size_t>(9, vertex_spec.attributes.size()); ++i) // For each instance matrix attribute:
	{
		if(vertex_spec.attributes[i].instancing)
		{
			vertex_spec.attributes[i].vbo = new_instance_matrix_vbo;
			vertex_spec.attributes[i].enabled = true;
		}
	}

#if DO_INDIVIDUAL_VAO_ALLOC
	// Old style VAO that binds vertex buffer and indices buffer into it.
	this->vert_vao = new VAO(mesh_data->vbo_handle.vbo, mesh_data->indices_vbo_handle.index_vbo, vertex_spec);
#else
	this->vert_vao = new VAO(vertex_spec);
#endif
}


size_t GLMemUsage::totalCPUUsage() const
{
	return texture_cpu_usage + geom_cpu_usage;
}


size_t GLMemUsage::totalGPUUsage() const
{
	return texture_gpu_usage + geom_gpu_usage;
}


void GLMemUsage::operator += (const GLMemUsage& other)
{
	texture_cpu_usage += other.texture_cpu_usage;
	texture_gpu_usage += other.texture_gpu_usage;
	geom_cpu_usage += other.geom_cpu_usage;
	geom_gpu_usage += other.geom_gpu_usage;
}


OverlayObject::OverlayObject()
:	ob_to_world_matrix(Matrix4f::uniformScaleMatrix(0.f))
{
	material.albedo_linear_rgb = Colour3f(1.f);
	clip_region = Rect2f(Vec2f(-100.f), Vec2f(100.f));
	draw = true;
}


OpenGLScene::OpenGLScene(OpenGLEngine& engine)
:	light_grid(/*cell_w=*/64.0, /*num buckets=*/1024, /*expected_num_items_per_bucket=*/4, /*empty key=*/NULL),
	objects(NULL),
	animated_objects(NULL),
	transparent_objects(NULL),
	alpha_blended_objects(NULL),
	water_objects(NULL),
	decal_objects(NULL),
	always_visible_objects(NULL),
	overlay_objects(NULL),
	overlay_world_to_camera_space_matrix(Matrix4f::identity()),
	collect_stats(true),
	draw_overlay_objects(true),
	frame_num(0)
{
	max_draw_dist = 1000;
	near_draw_dist = 0.22f;
	render_aspect_ratio = 1;
	lens_shift_up_distance = 0;
	lens_shift_right_distance = 0;
	camera_type = OpenGLScene::CameraType_Perspective;
	background_colour = Colour3f(0.1f);
	use_z_up = true;
	bloom_strength = 0;
	wind_strength = 1.f;
	water_level_z = 0.f;
	dof_blur_strength = 0.f;
	dof_blur_focus_distance = 1.0f;
	exposure_factor = 1.0f;
	saturation_multiplier = 1.0f;
	grass_pusher_sphere_pos = Vec4f(-1.0e10, -1.0e10, -1.0e10, 1);
	shadow_mapping = true;
	draw_water = true;
	draw_aurora = false;
	render_to_main_render_framebuffer = engine.settings.render_to_offscreen_renderbuffers;
	cloud_shadows = true;

	env_ob = engine.allocateObject();
	env_ob->ob_to_world_matrix = Matrix4f::identity();
	env_ob->ob_to_world_normal_matrix = Matrix4f::identity();
	env_ob->materials.resize(1);
	env_ob->mesh_data = engine.getSphereMeshData();
}


OpenGLScene::~OpenGLScene()
{
	if(!overlay_objects.empty())
	{
		conPrint("Warning: " + toString(overlay_objects.size()) + " overlay objects still in OpenGLScene upon scene destruction.");
		assert(0);
	}
}


std::string BatchDrawInfo::keyDescription() const
{
	// NOTE: out of date
	return "prog: " + toString(prog_vao_key >> 18) + 
		", vao: " + toString((prog_vao_key >> 2) & 0xFFFF) + 
		", index_type_bits: " + toString((prog_vao_key >> 0) & 0x3)
		;
}


#if !defined(OSX) && !defined(EMSCRIPTEN)

static void 
#ifdef _WIN32
// NOTE: not sure what this should be on non-windows platforms.  APIENTRY does not seem to be defined with GCC on Linux 64.
APIENTRY 
#endif
myMessageCallback(GLenum /*source*/, GLenum type, GLuint /*id*/, GLenum severity, GLsizei /*length*/, const GLchar* message, const void* userParam) 
{
	if(severity != GL_DEBUG_SEVERITY_NOTIFICATION && !StringUtils::containsString(message, "recompiled")) // Don't print out notifications by default.
	{
		// See https://www.opengl.org/sdk/docs/man/html/glDebugMessageControl.xhtml

		std::string typestr;
		switch(type)
		{
		case GL_DEBUG_TYPE_ERROR: typestr = "Error"; break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typestr = "Deprecated Behaviour"; break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: typestr = "Undefined Behaviour"; break;
		case GL_DEBUG_TYPE_PORTABILITY: typestr = "Portability"; break;
		case GL_DEBUG_TYPE_PERFORMANCE: typestr = "Performance"; break;
		case GL_DEBUG_TYPE_MARKER: typestr = "Marker"; break;
		case GL_DEBUG_TYPE_PUSH_GROUP: typestr = "Push group"; break;
		case GL_DEBUG_TYPE_POP_GROUP: typestr = "Pop group"; break;
		case GL_DEBUG_TYPE_OTHER: typestr = "Other"; break;
		default: typestr = "Unknown"; break;
		}

		std::string severitystr;
		switch(severity)
		{
		case GL_DEBUG_SEVERITY_LOW: severitystr = "low"; break;
		case GL_DEBUG_SEVERITY_MEDIUM: severitystr = "medium"; break;
		case GL_DEBUG_SEVERITY_HIGH : severitystr = "high"; break;
		case GL_DEBUG_SEVERITY_NOTIFICATION : severitystr = "notification"; break;
		case GL_DONT_CARE: severitystr = "Don't care"; break;
		default: severitystr = "Unknown"; break;
		}

		conPrint("==============================================================");
		conPrint("OpenGL msg, severity: " + severitystr + ", type: " + typestr + ":");
		conPrint(std::string(message));
		conPrint("==============================================================");

		OpenGLEngine* engine = (OpenGLEngine*)userParam;
		if(engine && engine->print_output)
			engine->print_output->print("OpenGL Engine: severity: " + severitystr + ", type: " + typestr + ", message: " + message);
	}
}

#endif // !defined(OSX)


OpenGLEngine::OpenGLEngine(const OpenGLEngineSettings& settings_)
:	init_succeeded(false),
	settings(settings_),
	draw_wireframes(false),
	shadow_mapping_frame_num(0),
	current_time(0.f),
	texture_server(NULL),
	outline_colour(0.43f, 0.72f, 0.95f, 1.0),
	outline_width_px(3.0f),
	outline_tex_w(0),
	outline_tex_h(0),
	last_num_obs_in_frustum(0),
	print_output(NULL),
	tex_CPU_mem_usage(0),
	tex_GPU_mem_usage(0),
	last_anim_update_duration(0),
	last_draw_CPU_time(0),
	last_fps(0),
	query_profiling_enabled(false),
	num_frames_since_fps_timer_reset(0),
	last_num_prog_changes(0),
	last_num_batches_bound(0),
	last_num_vao_binds(0),
	last_num_vbo_binds(0),
	last_num_index_buf_binds(0),
	last_num_indices_drawn(0),
	last_num_face_culling_changes(0),
	num_index_buf_binds(0),
	depth_draw_last_num_prog_changes(0),
	depth_draw_last_num_face_culling_changes(0),
	depth_draw_last_num_batches_bound(0),
	depth_draw_last_num_vao_binds(0),
	depth_draw_last_num_vbo_binds(0),
	depth_draw_last_num_indices_drawn(0),
	last_total_draw_GPU_time(0),
	last_dynamic_depth_draw_GPU_time(0),
	last_static_depth_draw_GPU_time(0),
	last_draw_opaque_obs_GPU_time(0),
	last_depth_pre_pass_GPU_time(0),
	last_compute_ssao_GPU_time(0),
	last_blur_ssao_GPU_time(0),
	last_copy_prepass_buffers_GPU_time(0),
	last_decal_copy_buffers_GPU_time(0),
	last_draw_overlay_obs_GPU_time(0),
	last_num_animated_obs_processed(0),
	last_num_decal_batches_drawn(0),
	next_program_index(0),
	use_bindless_textures(false),
	use_multi_draw_indirect(false),
	use_ob_and_mat_data_gpu_resident(false),
	use_reverse_z(true),
	use_scatter_shader(false),
	//object_pool_allocator(sizeof(GLObject), /*alignment=*/16, /*block capacity=*/1024),
	running_in_renderdoc(false),
	loaded_maps_for_sun_dir(false),
	add_debug_obs(false),
	async_texture_loader(NULL),
	current_bound_phong_uniform_buf_ob_index(0),
	show_ssao(true),
	num_draw_commands(0),
	reload_shaders_callback(nullptr)
{
	current_index_type = 0;
	current_bound_prog = NULL;
	current_bound_prog_index = std::numeric_limits<uint32>::max();
	current_bound_VAO = NULL;
	current_uniforms_ob = NULL;

	current_scene = new OpenGLScene(*this);
	scenes.insert(current_scene);

	viewport_w = viewport_h = 100;

	main_viewport_w = 0;
	main_viewport_h = 0;

	sun_dir = normalise(Vec4f(0.2f,0.2f,1,0));
	sun_phi = std::atan2(sun_dir[1], sun_dir[0]);

	// From AtmosphereMedium::AtmosphereMedium() (air_scattering_coeff_spectrum) in Indigo source.
	air_scattering_coeffs = Vec4f(0.000008057856f, 0.000011481005f, 0.000026190464f, 0);

	max_tex_CPU_mem_usage = settings.max_tex_CPU_mem_usage;
	max_tex_GPU_mem_usage = settings.max_tex_GPU_mem_usage;

	vert_buf_allocator = new VertexBufferAllocator(settings.use_grouped_vbo_allocator);

	animated_objects_task_group = new glare::TaskGroup();

	initial_thread_id = PlatformUtils::getCurrentThreadID();
}


OpenGLEngine::~OpenGLEngine()
{
	for(size_t i=0; i<texture_debug_preview_overlay_obs.size(); ++i)
		removeOverlayObject(texture_debug_preview_overlay_obs[i]);
	texture_debug_preview_overlay_obs.clear();

	if(large_debug_overlay_ob)
		removeOverlayObject(large_debug_overlay_ob);
	large_debug_overlay_ob = nullptr;

	if(crosshair_overlay_ob)
		removeOverlayObject(crosshair_overlay_ob);
	crosshair_overlay_ob = nullptr;

	if(async_texture_loader)
	{
		for(size_t i=0; i<loading_handles.size(); ++i)
			async_texture_loader->cancelLoadingTexture(loading_handles[i]);
	}

	// Destroy textures now, since they may use texture views
	fbm_tex = NULL;
	for(size_t i=0; i<staticArrayNumElems(detail_tex); ++i)
		detail_tex[i] = NULL;
	for(size_t i=0; i<staticArrayNumElems(detail_heightmap); ++i)
		detail_heightmap[i] = NULL;
	blue_noise_tex = NULL;
	noise_tex = NULL;
	cirrus_tex = NULL;
	aurora_tex = NULL;
	//snow_ice_normal_map = NULL;

	water_caustics_textures.clear();

	for(size_t i=0; i<debug_draw_obs.size(); ++i)
		removeObject(debug_draw_obs[i]);
	debug_draw_obs.clear();


	// The textures may outlast the OpenGLEngine, so null out the pointer to the opengl engine and set inserted_into_opengl_textures = false.
	for(auto it = opengl_textures.begin(); it != opengl_textures.end(); ++it)
	{
		OpenGLTexture* tex = it->second.value.ptr();
		if(tex->allocated_tex_view_info.texture_handle == 0) // Don't null out m_opengl_engine if we allocated a texture view from the opengl engine, is needed for freeing texture view
		{
			tex->inserted_into_opengl_textures = false;
			tex->m_opengl_engine = NULL;
		}
	}
	opengl_textures.clear();

	// Free mesh data before we free the vertex allocator.
	current_scene = NULL;
	scenes.clear();

	clear_buf_overlay_ob = NULL;
	outline_quad_meshdata = NULL;

	debug_arrow_ob = NULL;
	debug_sphere_ob = NULL;
	debug_joint_obs.clear();

	line_meshdata = NULL;
	sphere_meshdata = NULL;
	arrow_meshdata = NULL;
	cube_meshdata = NULL;
	unit_quad_meshdata = NULL;
	cylinder_meshdata = NULL;
	sprite_quad_meshdata = NULL;

	// Free texture_names
	glDeleteTextures((GLsizei)texture_names.size(), texture_names.data());

	// Update the message callback userParam to be NULL, since 'this' is being destroyed.
#if !defined(OSX) && !defined(EMSCRIPTEN)
	glDebugMessageCallback(myMessageCallback, NULL);
#endif
}


static const Vec4f FORWARDS_OS_Z_UP(0, 1, 0, 0); // Forwards in local camera (object) space.
static const Vec4f UP_OS_Z_UP(0, 0, 1, 0);
static const Vec4f RIGHT_OS_Z_UP(1, 0, 0, 0);


static const Vec4f FORWARDS_OS_Y_UP(0, 0, -1, 0); // Forwards in local camera (object) space.
static const Vec4f UP_OS_Y_UP(0, 1, 0, 0);
static const Vec4f RIGHT_OS_Y_UP(1, 0, 0, 0);


void OpenGLScene::setPerspectiveCameraTransform(const Matrix4f& world_to_camera_space_matrix_, float sensor_width_, float lens_sensor_dist_, float render_aspect_ratio_, float lens_shift_up_distance_,
	float lens_shift_right_distance_, float viewport_aspect_ratio)
{
	camera_type = OpenGLScene::CameraType_Perspective;

	this->world_to_camera_space_matrix = world_to_camera_space_matrix_;
	this->lens_sensor_dist = lens_sensor_dist_;
	this->render_aspect_ratio = render_aspect_ratio_;
	this->lens_shift_up_distance = lens_shift_up_distance_;
	this->lens_shift_right_distance = lens_shift_right_distance_;

	if(viewport_aspect_ratio > render_aspect_ratio) // if viewport has a wider aspect ratio than the render:
	{
		use_sensor_height = sensor_width_ / render_aspect_ratio; // Keep vertical field of view the same
		use_sensor_width = use_sensor_height * viewport_aspect_ratio; // enlarge horizontal field of view as needed
	}
	else
	{
		use_sensor_width = sensor_width_; // Keep horizontal field of view the same
		use_sensor_height = sensor_width_ / viewport_aspect_ratio; // Enlarge vertical field of view as needed
	}


	// Make clipping planes
	const Vec4f lens_center(lens_shift_right_distance, 0, lens_shift_up_distance, 1);
	const Vec4f sensor_center(0, -lens_sensor_dist, 0, 1);

	const Vec4f forwards_os    = this->use_z_up ? FORWARDS_OS_Z_UP : FORWARDS_OS_Y_UP;
	const Vec4f up_os          = this->use_z_up ? UP_OS_Z_UP : UP_OS_Y_UP;
	const Vec4f right_os       = this->use_z_up ? RIGHT_OS_Z_UP : RIGHT_OS_Y_UP;

	const Vec4f sensor_bottom = (sensor_center - up_os    * (use_sensor_height * 0.5f));
	const Vec4f sensor_top    = (sensor_center + up_os    * (use_sensor_height * 0.5f));
	const Vec4f sensor_left   = (sensor_center - right_os * (use_sensor_width  * 0.5f));
	const Vec4f sensor_right  = (sensor_center + right_os * (use_sensor_width  * 0.5f));

	Planef planes_cs[5]; // Clipping planes in camera space

	// We won't bother with a culling plane at the near clip plane, since the near draw distance is so close to zero, so it's unlikely to cull any objects.

	planes_cs[0] = Planef(
		Vec4f(0,0,0,1) + forwards_os * this->max_draw_dist, // pos.
		forwards_os // normal
	); // far clip plane of frustum

	const Vec4f left_normal = normalise(crossProduct(up_os, lens_center - sensor_right));
	planes_cs[1] = Planef(
		lens_center,
		left_normal
	); // left

	const Vec4f right_normal = normalise(crossProduct(lens_center - sensor_left, up_os));
	planes_cs[2] = Planef(
		lens_center,
		right_normal
	); // right

	const Vec4f bottom_normal = normalise(crossProduct(lens_center - sensor_top, right_os));
	planes_cs[3] = Planef(
		lens_center,
		bottom_normal
	); // bottom

	const Vec4f top_normal = normalise(crossProduct(right_os, lens_center - sensor_bottom));
	planes_cs[4] = Planef(
		lens_center,
		top_normal
	); // top

	// Transform clipping planes to world space
	world_to_camera_space_matrix.getInverseForAffine3Matrix(this->cam_to_world);

	for(int i=0; i<5; ++i)
	{
		// Get point on plane and plane normal in Camera space.
		const Vec4f plane_point_cs = planes_cs[i].getPointOnPlane();

		const Vec4f plane_normal_cs = planes_cs[i].getNormal();

		// Transform from camera space -> world space, then world space -> object space.
		const Vec4f plane_point_ws = cam_to_world * plane_point_cs;
		const Vec4f normal_ws = normalise(cam_to_world * plane_normal_cs);

		frustum_clip_planes[i] = Planef(plane_point_ws, normal_ws);
	}

	this->num_frustum_clip_planes = 5;


	// Calculate frustum verts
	Vec4f verts_cs[5];
	const float shift_up_d    = lens_shift_up_distance    * (max_draw_dist / lens_sensor_dist); // distance verts at far end of frustum are shifted up
	const float shift_right_d = lens_shift_right_distance * (max_draw_dist / lens_sensor_dist); // distance verts at far end of frustum are shifted right

	const float d_w = use_sensor_width  * max_draw_dist / (2 * lens_sensor_dist);
	const float d_h = use_sensor_height * max_draw_dist / (2 * lens_sensor_dist);

	verts_cs[0] = lens_center;
	verts_cs[1] = lens_center + forwards_os * max_draw_dist + right_os * (-d_w + shift_right_d) + up_os * (-d_h + shift_up_d); // bottom left
	verts_cs[2] = lens_center + forwards_os * max_draw_dist + right_os * ( d_w + shift_right_d) + up_os * (-d_h + shift_up_d); // bottom right
	verts_cs[3] = lens_center + forwards_os * max_draw_dist + right_os * ( d_w + shift_right_d) + up_os * ( d_h + shift_up_d); // top right
	verts_cs[4] = lens_center + forwards_os * max_draw_dist + right_os * (-d_w + shift_right_d) + up_os * ( d_h + shift_up_d); // top left

	frustum_aabb = js::AABBox::emptyAABBox();
	for(int i=0; i<5; ++i)
	{
		assert(verts_cs[i][3] == 1.f);
		const Vec4f frustum_vert_ws = cam_to_world * verts_cs[i];
		//conPrint("frustum_verts " + toString(i) + ": " + frustum_verts[i].toString());
		frustum_aabb.enlargeToHoldPoint(frustum_vert_ws);
	}
}


void OpenGLEngine::setPerspectiveCameraTransform(const Matrix4f& world_to_camera_space_matrix_, float sensor_width_, float lens_sensor_dist_, float render_aspect_ratio_, float lens_shift_up_distance_,
	float lens_shift_right_distance_)
{
	current_scene->setPerspectiveCameraTransform(world_to_camera_space_matrix_, sensor_width_, lens_sensor_dist_, render_aspect_ratio_, lens_shift_up_distance_, lens_shift_right_distance_, this->getViewPortAspectRatio());
}


// Calculate the corner vertices of a slice of the view frustum, where the slice is defined by near and far distances from the camera.
void OpenGLScene::calcCamFrustumVerts(float near_dist, float far_dist, Vec4f* verts_out) const
{
	const Vec4f lens_center_cs(lens_shift_right_distance, 0, lens_shift_up_distance, 1);
	const Vec4f lens_center_ws = cam_to_world * lens_center_cs;

	const Vec4f forwards_os    = this->use_z_up ? FORWARDS_OS_Z_UP : FORWARDS_OS_Y_UP;
	const Vec4f up_os          = this->use_z_up ? UP_OS_Z_UP : UP_OS_Y_UP;
	const Vec4f right_os       = this->use_z_up ? RIGHT_OS_Z_UP : RIGHT_OS_Y_UP;

	const Vec4f forwards_ws = cam_to_world * forwards_os;
	const Vec4f up_ws = cam_to_world * up_os;
	const Vec4f right_ws = cam_to_world * right_os;

	if(camera_type == OpenGLScene::CameraType_Identity)
	{
		verts_out[0] = Vec4f(-1,-1,-1,1);
		verts_out[1] = Vec4f( 1,-1,-1,1);
		verts_out[2] = Vec4f( 1,-1, 1,1);
		verts_out[3] = Vec4f(-1,-1, 1,1);
		verts_out[4] = Vec4f(-1, 1,-1,1);
		verts_out[5] = Vec4f( 1, 1,-1,1);
		verts_out[6] = Vec4f( 1, 1, 1,1);
		verts_out[7] = Vec4f(-1, 1, 1,1);
	}
	else if(camera_type == OpenGLScene::CameraType_Perspective)
	{
		// Calculate frustum verts
		const float shift_up_d    = lens_shift_up_distance    / lens_sensor_dist; // distance verts at far end of frustum are shifted up
		const float shift_right_d = lens_shift_right_distance / lens_sensor_dist; // distance verts at far end of frustum are shifted up

		const float d_w = use_sensor_width  / (2 * lens_sensor_dist);
		const float d_h = use_sensor_height / (2 * lens_sensor_dist);

		// Verts on near plane
		verts_out[0] = lens_center_ws + forwards_ws * near_dist + right_ws * (-d_w + shift_right_d) * near_dist + up_ws * (-d_h + shift_up_d) * near_dist; // bottom left
		verts_out[1] = lens_center_ws + forwards_ws * near_dist + right_ws * ( d_w + shift_right_d) * near_dist + up_ws * (-d_h + shift_up_d) * near_dist; // bottom right
		verts_out[2] = lens_center_ws + forwards_ws * near_dist + right_ws * ( d_w + shift_right_d) * near_dist + up_ws * ( d_h + shift_up_d) * near_dist; // top right
		verts_out[3] = lens_center_ws + forwards_ws * near_dist + right_ws * (-d_w + shift_right_d) * near_dist + up_ws * ( d_h + shift_up_d) * near_dist; // top left

		// Verts on far plane
		verts_out[4] = lens_center_ws + forwards_ws * far_dist + right_ws * (-d_w + shift_right_d) * far_dist + up_ws * (-d_h + shift_up_d) * far_dist; // bottom left
		verts_out[5] = lens_center_ws + forwards_ws * far_dist + right_ws * ( d_w + shift_right_d) * far_dist + up_ws * (-d_h + shift_up_d) * far_dist; // bottom right
		verts_out[6] = lens_center_ws + forwards_ws * far_dist + right_ws * ( d_w + shift_right_d) * far_dist + up_ws * ( d_h + shift_up_d) * far_dist; // top right
		verts_out[7] = lens_center_ws + forwards_ws * far_dist + right_ws * (-d_w + shift_right_d) * far_dist + up_ws * ( d_h + shift_up_d) * far_dist; // top left
	}
	else if(camera_type == OpenGLScene::CameraType_Orthographic || camera_type == OpenGLScene::CameraType_DiagonalOrthographic)
	{
		const float d_w = use_sensor_width  / 2;
		const float d_h = use_sensor_height / 2;

		// Verts on near plane
		verts_out[0] = lens_center_ws + forwards_ws * near_dist + right_ws * -d_w + up_ws * -d_h; // bottom left
		verts_out[1] = lens_center_ws + forwards_ws * near_dist + right_ws *  d_w + up_ws * -d_h; // bottom right
		verts_out[2] = lens_center_ws + forwards_ws * near_dist + right_ws *  d_w + up_ws *  d_h; // top right
		verts_out[3] = lens_center_ws + forwards_ws * near_dist + right_ws * -d_w + up_ws *  d_h; // top left

		verts_out[4] = lens_center_ws + forwards_ws * far_dist  + right_ws * -d_w + up_ws * -d_h; // bottom left
		verts_out[5] = lens_center_ws + forwards_ws * far_dist  + right_ws *  d_w + up_ws * -d_h; // bottom right
		verts_out[6] = lens_center_ws + forwards_ws * far_dist  + right_ws *  d_w + up_ws *  d_h; // top right
		verts_out[7] = lens_center_ws + forwards_ws * far_dist  + right_ws * -d_w + up_ws *  d_h; // top left
	}
	else
	{
		assert(0);
	}
}


void OpenGLEngine::calcCamFrustumVerts(float near_dist, float far_dist, Vec4f* verts_out) const
{
	current_scene->calcCamFrustumVerts(near_dist, far_dist, verts_out);
}


void OpenGLScene::setOrthoCameraTransform(const Matrix4f& world_to_camera_space_matrix_, float sensor_width_, float render_aspect_ratio_, float lens_shift_up_distance_,
	float lens_shift_right_distance_, float viewport_aspect_ratio)
{
	camera_type = CameraType_Orthographic;

	this->world_to_camera_space_matrix = world_to_camera_space_matrix_;
	this->lens_sensor_dist = 1.f;
	this->render_aspect_ratio = render_aspect_ratio_;
	this->lens_shift_up_distance = lens_shift_up_distance_;
	this->lens_shift_right_distance = lens_shift_right_distance_;

	if(viewport_aspect_ratio > render_aspect_ratio) // if viewport has a wider aspect ratio than the render:
	{
		use_sensor_height = sensor_width_ / render_aspect_ratio; // Keep vertical field of view the same
		use_sensor_width = use_sensor_height * viewport_aspect_ratio; // enlarge horizontal field of view as needed
	}
	else
	{
		use_sensor_width = sensor_width_; // Keep horizontal field of view the same
		use_sensor_height = sensor_width_ / viewport_aspect_ratio; // Enlarge vertical field of view as needed
	}


	// Make camera view volume clipping planes
	const Vec4f lens_center(lens_shift_right_distance, 0, lens_shift_up_distance, 1);

	Planef planes_cs[6]; // Clipping planes in camera space

	const Vec4f forwards_os    = this->use_z_up ? FORWARDS_OS_Z_UP : FORWARDS_OS_Y_UP;
	const Vec4f up_os          = this->use_z_up ? UP_OS_Z_UP : UP_OS_Y_UP;
	const Vec4f right_os       = this->use_z_up ? RIGHT_OS_Z_UP : RIGHT_OS_Y_UP;

	planes_cs[0] = Planef(
		Vec4f(0,0,0,1), // pos.
		-forwards_os // normal
	); // near clip plane of frustum

	planes_cs[1] = Planef(
		Vec4f(0, 0, 0, 1) + forwards_os * this->max_draw_dist, // pos.
		forwards_os // normal
	); // far clip plane of frustum

	const Vec4f left_normal = -right_os;
	planes_cs[2] = Planef(
		lens_center + left_normal * use_sensor_width/2,
		left_normal
	); // left

	const Vec4f right_normal = right_os;
	planes_cs[3] = Planef(
		lens_center + right_normal * use_sensor_width/2,
		right_normal
	); // right

	const Vec4f bottom_normal = -up_os;
	planes_cs[4] = Planef(
		lens_center + bottom_normal * use_sensor_height/2,
		bottom_normal
	); // bottom

	const Vec4f top_normal = up_os;
	planes_cs[5] = Planef(
		lens_center + top_normal * use_sensor_height/2,
		top_normal
	); // top
	
	// Transform clipping planes to world space
	world_to_camera_space_matrix.getInverseForAffine3Matrix(this->cam_to_world);
	
	for(int i=0; i<6; ++i)
	{
		// Get point on plane and plane normal in Camera space.
		const Vec4f plane_point_cs = planes_cs[i].getPointOnPlane();

		const Vec4f plane_normal_cs = planes_cs[i].getNormal();

		// Transform from camera space -> world space, then world space -> object space.
		const Vec4f plane_point_ws = cam_to_world * plane_point_cs;
		const Vec4f normal_ws = normalise(cam_to_world * plane_normal_cs);

		frustum_clip_planes[i] = Planef(plane_point_ws, normal_ws);
	}

	this->num_frustum_clip_planes = 6;

	// Calculate frustum verts
	Vec4f verts_cs[8];
	verts_cs[0] = lens_center                               + right_os * -use_sensor_width + up_os * -use_sensor_height; // bottom left
	verts_cs[1] = lens_center                               + right_os *  use_sensor_width + up_os * -use_sensor_height; // bottom right
	verts_cs[2] = lens_center                               + right_os *  use_sensor_width + up_os *  use_sensor_height; // top right
	verts_cs[3] = lens_center                               + right_os * -use_sensor_width + up_os *  use_sensor_height; // top left
	verts_cs[4] = lens_center + forwards_os * max_draw_dist + right_os * -use_sensor_width + up_os * -use_sensor_height; // bottom left
	verts_cs[5] = lens_center + forwards_os * max_draw_dist + right_os *  use_sensor_width + up_os * -use_sensor_height; // bottom right
	verts_cs[6] = lens_center + forwards_os * max_draw_dist + right_os *  use_sensor_width + up_os *  use_sensor_height; // top right
	verts_cs[7] = lens_center + forwards_os * max_draw_dist + right_os * -use_sensor_width + up_os *  use_sensor_height; // top left

	frustum_aabb = js::AABBox::emptyAABBox();
	for(int i=0; i<8; ++i)
	{
		assert(verts_cs[i][3] == 1.f);
		const Vec4f frustum_vert_ws = cam_to_world * verts_cs[i];
		//conPrint("frustum_verts " + toString(i) + ": " + frustum_verts[i].toString());
		frustum_aabb.enlargeToHoldPoint(frustum_vert_ws);
	}
}


void OpenGLScene::setDiagonalOrthoCameraTransform(const Matrix4f& world_to_camera_space_matrix_, float sensor_width_, float render_aspect_ratio_, float viewport_aspect_ratio)
{
	camera_type = CameraType_DiagonalOrthographic;

	this->world_to_camera_space_matrix = world_to_camera_space_matrix_;
	this->lens_sensor_dist = 1.f;
	this->render_aspect_ratio = render_aspect_ratio_;
	this->lens_shift_up_distance = 0;
	this->lens_shift_right_distance = 0;

	if(viewport_aspect_ratio > render_aspect_ratio) // if viewport has a wider aspect ratio than the render:
	{
		use_sensor_height = sensor_width_ / render_aspect_ratio; // Keep vertical field of view the same
		use_sensor_width = use_sensor_height * viewport_aspect_ratio; // enlarge horizontal field of view as needed
	}
	else
	{
		use_sensor_width = sensor_width_; // Keep horizontal field of view the same
		use_sensor_height = sensor_width_ / viewport_aspect_ratio; // Enlarge vertical field of view as needed
	}

	//TEMP HACK: These camera clipping planes are completely wrong for diagonal ortho.  use larger values to compensate
	const float clip_sensor_width  = use_sensor_width * 2;
	const float clip_sensor_height = use_sensor_height * 2;

	// Make camera view volume clipping planes
	const Vec4f lens_center(lens_shift_right_distance, 0, lens_shift_up_distance, 1);

	Planef planes_cs[6]; // Clipping planes in camera space

	const Vec4f forwards_os    = this->use_z_up ? FORWARDS_OS_Z_UP : FORWARDS_OS_Y_UP;
	const Vec4f up_os          = this->use_z_up ? UP_OS_Z_UP : UP_OS_Y_UP;
	const Vec4f right_os       = this->use_z_up ? RIGHT_OS_Z_UP : RIGHT_OS_Y_UP;


	planes_cs[0] = Planef(
		Vec4f(0,0,0,1), // pos.
		-forwards_os // normal
	); // near clip plane of frustum

	planes_cs[1] = Planef(
		Vec4f(0, 0, 0, 1) + forwards_os * this->max_draw_dist, // pos.
		forwards_os // normal
	); // far clip plane of frustum

	const Vec4f left_normal = -right_os;
	planes_cs[2] = Planef(
		lens_center + left_normal * clip_sensor_width/2,
		left_normal
	); // left

	const Vec4f right_normal = right_os;
	planes_cs[3] = Planef(
		lens_center + right_normal * clip_sensor_width/2,
		right_normal
	); // right

	const Vec4f bottom_normal = -up_os;
	planes_cs[4] = Planef(
		lens_center + bottom_normal * clip_sensor_height/2,
		bottom_normal
	); // bottom

	const Vec4f top_normal = up_os;
	planes_cs[5] = Planef(
		lens_center + top_normal * clip_sensor_height/2,
		top_normal
	); // top

	// Transform clipping planes to world space
	world_to_camera_space_matrix.getInverseForAffine3Matrix(this->cam_to_world);

	for(int i=0; i<6; ++i)
	{
		// Get point on plane and plane normal in Camera space.
		const Vec4f plane_point_cs = planes_cs[i].getPointOnPlane();

		const Vec4f plane_normal_cs = planes_cs[i].getNormal();

		// Transform from camera space -> world space, then world space -> object space.
		const Vec4f plane_point_ws = cam_to_world * plane_point_cs;
		const Vec4f normal_ws = normalise(cam_to_world * plane_normal_cs);

		frustum_clip_planes[i] = Planef(plane_point_ws, normal_ws);
	}

	this->num_frustum_clip_planes = 6;

	// Calculate frustum verts
	Vec4f verts_cs[8];
	verts_cs[0] = lens_center                               + right_os * -clip_sensor_width + up_os * -clip_sensor_height; // bottom left
	verts_cs[1] = lens_center                               + right_os *  clip_sensor_width + up_os * -clip_sensor_height; // bottom right
	verts_cs[2] = lens_center                               + right_os *  clip_sensor_width + up_os *  clip_sensor_height; // top right
	verts_cs[3] = lens_center                               + right_os * -clip_sensor_width + up_os *  clip_sensor_height; // top left
	verts_cs[4] = lens_center + forwards_os * max_draw_dist + right_os * -clip_sensor_width + up_os * -clip_sensor_height; // bottom left
	verts_cs[5] = lens_center + forwards_os * max_draw_dist + right_os *  clip_sensor_width + up_os * -clip_sensor_height; // bottom right
	verts_cs[6] = lens_center + forwards_os * max_draw_dist + right_os *  clip_sensor_width + up_os *  clip_sensor_height; // top right
	verts_cs[7] = lens_center + forwards_os * max_draw_dist + right_os * -clip_sensor_width + up_os *  clip_sensor_height; // top left

	frustum_aabb = js::AABBox::emptyAABBox();
	for(int i=0; i<8; ++i)
	{
		assert(verts_cs[i][3] == 1.f);
		const Vec4f frustum_vert_ws = cam_to_world * verts_cs[i];
		//conPrint("frustum_verts " + toString(i) + ": " + frustum_verts[i].toString());
		frustum_aabb.enlargeToHoldPoint(frustum_vert_ws);
	}
}


void OpenGLScene::setIdentityCameraTransform()
{
	camera_type = CameraType_Identity;
	this->world_to_camera_space_matrix = Matrix4f::identity();
	this->cam_to_world = Matrix4f::identity();

	this->num_frustum_clip_planes = 0; // TEMP: just don't do clipping currently.
	this->frustum_aabb = js::AABBox(Vec4f(-1, -1, -1, 1), Vec4f(1, 1, 1, 1));
}


void OpenGLEngine::setOrthoCameraTransform(const Matrix4f& world_to_camera_space_matrix_, float sensor_width_, float render_aspect_ratio_, float lens_shift_up_distance_,
	float lens_shift_right_distance_)
{
	current_scene->setOrthoCameraTransform(world_to_camera_space_matrix_, sensor_width_, render_aspect_ratio_, lens_shift_up_distance_, lens_shift_right_distance_, this->getViewPortAspectRatio());
}

void OpenGLEngine::setDiagonalOrthoCameraTransform(const Matrix4f& world_to_camera_space_matrix_, float sensor_width_, float render_aspect_ratio_)
{
	current_scene->setDiagonalOrthoCameraTransform(world_to_camera_space_matrix_, sensor_width_, render_aspect_ratio_, this->getViewPortAspectRatio());
}


void OpenGLEngine::setIdentityCameraTransform()
{
	current_scene->setIdentityCameraTransform();
}


void OpenGLEngine::setCurrentTime(float time)
{
	this->current_time = time;
}


// Average spectral radiances, weighted for RGB curves, from the sky model (without sun) for the sky model thetas.
// Generated with SkyModel2Generator::generateAvSpectralRadianceData().
static const float sky_av_spec_rad_data[] = {
	80390680.f, 94338770.f, 123985290.f,
	80350860.f, 94294856.f, 123960400.f,
	80267480.f, 94205390.f, 123823580.f,
	80104290.f, 94045090.f, 123622810.f,
	79887096.f, 93813600.f, 123400760.f,
	79616410.f, 93523640.f, 123037770.f,
	79258620.f, 93190240.f, 122592040.f,
	78833360.f, 92762000.f, 122113380.f,
	78354010.f, 92312370.f, 121596140.f,
	77829896.f, 91752820.f, 120949040.f,
	77246710.f, 91144590.f, 120235960.f,
	76578950.f, 90485210.f, 119479270.f,
	75857330.f, 89768070.f, 118648420.f,
	75077640.f, 88977384.f, 117687470.f,
	74245830.f, 88129960.f, 116680660.f,
	73363530.f, 87222650.f, 115627920.f,
	72426870.f, 86214810.f, 114485430.f,
	71445340.f, 85194260.f, 113205864.f,
	70361520.f, 84089250.f, 111879920.f,
	69241580.f, 82959656.f, 110501000.f,
	68086740.f, 81756320.f, 109074610.f,
	66894920.f, 80488070.f, 107520510.f,
	65632212.f, 79194050.f, 105884040.f,
	64315956.f, 77815230.f, 104123650.f,
	62934050.f, 76377390.f, 102340960.f,
	61535456.f, 74886840.f, 100538410.f,
	60089868.f, 73376260.f, 98601550.f,
	58590548.f, 71790060.f, 96594240.f,
	57049190.f, 70142230.f, 94508710.f,
	55487044.f, 68420440.f, 92284904.f,
	53873670.f, 66650984.f, 90028900.f,
	52198100.f, 64844760.f, 87613680.f,
	50513476.f, 62969190.f, 85109410.f,
	48839156.f, 61013020.f, 82662750.f,
	47071212.f, 59053516.f, 80012970.f,
	45283940.f, 57025424.f, 77295336.f,
	43452856.f, 54910596.f, 74373940.f,
	41583256.f, 52734564.f, 71245440.f,
	39692264.f, 50458010.f, 67917040.f,
	37726204.f, 48062380.f, 64467476.f,
	35671490.f, 45602970.f, 60808050.f,
	33583564.f, 42963250.f, 56828772.f,
	31324304.f, 40228810.f, 52637376.f,
	29017694.f, 37201428.f, 48032696.f,
	26489964.f, 33911652.f, 42943376.f,
	23825512.f, 30221838.f, 37419844.f,
	20817200.f, 26030846.f, 31207578.f,
	17286222.f, 21221540.f, 24417692.f,
	13440836.f, 15749220.f, 17195166.f,
	9031804.f, 9884696.f, 10332520.f,
	4487572.f, 4588253.f, 4924291.5f,
	1310750.5f, 1406245.9f, 1685797.4f,
	239037.28f, 272702.06f, 363127.75f,
	33752.348f, 39623.457f, 57423.773f,
	5899.076f, 5526.149f, 7572.385f,
	1061.1992f, 1251.5305f, 1259.0583f,
	124.07373f, 163.35097f, 37.012947f,
	4.4110293f, 7.385951f, -0.69128233f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
};
static_assert(staticArrayNumElems(sky_av_spec_rad_data) == 100 * 3, "staticArrayNumElems(sky_av_spec_rad_data) == 100 * 3");


// Sun spectral radiances, weighted for RGB curves, at the sky model thetas.
// Generated in SkyModel2Generator::makeSkyEnvMap().
static const float sun_spec_rad_data[] = {
25492523000000.f, 23473718000000.f, 21528905000000.f,
25484510000000.f, 23463434000000.f, 21513223000000.f,
25488857000000.f, 23467928000000.f, 21519642000000.f,
25482435000000.f, 23457843000000.f, 21502976000000.f,
25486932000000.f, 23460960000000.f, 21505201000000.f,
25474910000000.f, 23445945000000.f, 21483332000000.f,
25446587000000.f, 23409730000000.f, 21422810000000.f,
25431210000000.f, 23388410000000.f, 21389252000000.f,
25424497000000.f, 23374439000000.f, 21360275000000.f,
25392140000000.f, 23330290000000.f, 21289838000000.f,
25388827000000.f, 23318032000000.f, 21266644000000.f,
25357160000000.f, 23278417000000.f, 21200967000000.f,
25323524000000.f, 23234672000000.f, 21140316000000.f,
25280536000000.f, 23167687000000.f, 21038197000000.f,
25256480000000.f, 23139105000000.f, 20984816000000.f,
25217758000000.f, 23081188000000.f, 20892529000000.f,
25162515000000.f, 23006726000000.f, 20772838000000.f,
25112485000000.f, 22940842000000.f, 20664575000000.f,
25060755000000.f, 22871622000000.f, 20557643000000.f,
25002406000000.f, 22791116000000.f, 20433378000000.f,
24931025000000.f, 22695912000000.f, 20284527000000.f,
24876585000000.f, 22621301000000.f, 20153410000000.f,
24782260000000.f, 22494744000000.f, 19959032000000.f,
24694321000000.f, 22382234000000.f, 19786510000000.f,
24602540000000.f, 22250296000000.f, 19586070000000.f,
24512582000000.f, 22116854000000.f, 19381034000000.f,
24413481000000.f, 21982748000000.f, 19172665000000.f,
24299618000000.f, 21831292000000.f, 18946491000000.f,
24149353000000.f, 21641214000000.f, 18645537000000.f,
24029377000000.f, 21473413000000.f, 18391694000000.f,
23850178000000.f, 21235962000000.f, 18036703000000.f,
23668099000000.f, 20996325000000.f, 17675118000000.f,
23470748000000.f, 20737016000000.f, 17293417000000.f,
23286970000000.f, 20483899000000.f, 16923487000000.f,
23025980000000.f, 20133993000000.f, 16411908000000.f,
22751048000000.f, 19781250000000.f, 15897520000000.f,
22445254000000.f, 19384412000000.f, 15341277000000.f,
22104976000000.f, 18952097000000.f, 14734328000000.f,
21693184000000.f, 18421777000000.f, 14025746000000.f,
21231707000000.f, 17827850000000.f, 13240120000000.f,
20697156000000.f, 17151068000000.f, 12357455000000.f,
20014470000000.f, 16306796000000.f, 11298580000000.f,
19299125000000.f, 15396689000000.f, 10202650000000.f,
18352590000000.f, 14267156000000.f, 8910706000000.f,
17201104000000.f, 12932040000000.f, 7463626000000.f,
15779721000000.f, 11295045000000.f, 5838189600000.f,
13982450000000.f, 9347944000000.f, 4110298000000.f,
11685808000000.f, 7024187300000.f, 2373650200000.f,
8597981000000.f, 4311201000000.f, 873850140000.f,
4729751500000.f, 1680080600000.f, 55019454000.f,
793660100000.f, 137287600000.f, -22923274000.f,
315350.47f, 397667.25f, 515803.75f,
47948.36f, 65660.93f, 94096.984f,
4548.414f, 8537.616f, 10819.586f,
810.40015f, 746.2641f, 2220.2847f,
-194.83438f, 358.22525f, 527.6283f,
172.58765f, 58.606438f, -10.461005f,
-0.06289596f, 3.2585173f, 0.47782722f,
-0.067020684f, 0.57397836f, -0.07009879f,
-0.00016825758f, 0.0014409595f, -0.00017598132f,
-6.4087885e-10f, 5.4884928e-9f, -6.7029793e-10f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f,
0.f, 0.f, 0.f
};
static_assert(staticArrayNumElems(sun_spec_rad_data) == 100 * 3, "staticArrayNumElems(sun_spec_rad_data) == 100 * 3");


// Load two EXR maps from path_0 and path_1 and blend them together, using weights w0 and w1.
// Return blended map.
static Map2DRef loadAndBlendMaps(const std::string& path_0, const std::string& path_1, float w0, float w1)
{
#ifdef NO_EXR_SUPPORT
	throw glare::Exception("loadAndBlendMaps(): EXR support disabled.");
#else
	Map2DRef map_0 = EXRDecoder::decode(path_0);
	Map2DRef map_1 = EXRDecoder::decode(path_1);

	runtimeCheck(map_0.isType<ImageMapFloat>());
	runtimeCheck(map_1.isType<ImageMapFloat>());

	// Blend, store final result in map0
	ImageMapFloat* map_float_0 = map_0.downcastToPtr<ImageMapFloat>();
	ImageMapFloat* map_float_1 = map_1.downcastToPtr<ImageMapFloat>();
	runtimeCheck(map_float_0->getDataSize() == map_float_1->getDataSize());

	const size_t data_size = map_float_0->getDataSize();
	      float* const map0_data = map_float_0->getData();
	const float* const map1_data = map_float_1->getData();
	for(size_t z=0; z<data_size; ++z)
	{
		map0_data[z] = map0_data[z] * w0 + map1_data[z] * w1;
	}

	return map_0;
#endif
}


// Load maps for sun direction
void OpenGLEngine::loadMapsForSunDir()
{
	Timer timer;

	const float sun_theta = GeometrySampling::sphericalCoordsForDir(this->sun_dir).y;

	// const float sun_theta = NICKMATHS_PIf * (float)z / (float)SkyModel2::numThetaSteps();
	// z = sun_theta / pi * numThetaSteps
	// z = sun_theta / pi * 100
	const float z_f = sun_theta / NICKMATHS_PIf * 100;
	const int MAX_INDEX = 60; // Only include up to index 60, since images past there are just all black and so are not included.
	const int z_0 = myClamp((int)z_f, 0, MAX_INDEX);
	const int z_1 = myClamp(z_0 + 1, 0, MAX_INDEX);
	const float frac = z_f - z_0;
	const float w0 = 1 - frac;
	const float w1 = frac;


	const Vec3f sun_spec_rad_0(sun_spec_rad_data[z_0 * 3 + 0], sun_spec_rad_data[z_0 * 3 + 1], sun_spec_rad_data[z_0 * 3 + 2]);
	const Vec3f sun_spec_rad_1(sun_spec_rad_data[z_1 * 3 + 0], sun_spec_rad_data[z_1 * 3 + 1], sun_spec_rad_data[z_1 * 3 + 2]);
	const Vec3f sun_spec_rad = sun_spec_rad_0 * w0 + sun_spec_rad_1 * w1;

	const float sun_solid_angle = 0.00006780608f; // See SkyModel2Generator::makeSkyEnvMap()
	this->sun_spec_rad_times_solid_angle = sun_spec_rad.toVec4fVector() * sun_solid_angle;

	const Vec3f sky_av_spec_rad_0(sky_av_spec_rad_data[z_0 * 3 + 0], sky_av_spec_rad_data[z_0 * 3 + 1], sky_av_spec_rad_data[z_0 * 3 + 2]);
	const Vec3f sky_av_spec_rad_1(sky_av_spec_rad_data[z_1 * 3 + 0], sky_av_spec_rad_data[z_1 * 3 + 1], sky_av_spec_rad_data[z_1 * 3 + 2]);
	const Vec3f sky_av_spec_rad = sky_av_spec_rad_0 * w0 + sky_av_spec_rad_1 * w1;

	this->sun_and_sky_av_spec_rad = sky_av_spec_rad.toVec4fVector() + this->sun_spec_rad_times_solid_angle / Maths::get4Pi<float>();


	const std::string sky_gl_data_dir = data_dir + "/gl_data/sky";

	// Load diffuse irradiance cube maps (face maps are in diffuse_sky_no_sun_x_y.exr)
	// These textures are generated by the code in glare-core\trunk\opengl\EnvMapProcessing.cpp
	{
		std::vector<Map2DRef> face_maps(6);
		for(int i=0; i<6; ++i) // For each cube face:
		{
			face_maps[i] = loadAndBlendMaps(
				sky_gl_data_dir + "/diffuse_sky_no_sun_" + toString(z_0) + "_" + toString(i) + ".exr",
				sky_gl_data_dir + "/diffuse_sky_no_sun_" + toString(z_1) + "_" + toString(i) + ".exr",
				w0, w1
			);
		}

		TextureParams tex_params;
		tex_params.filtering = float_texture_filtering_support ? OpenGLTexture::Filtering_Bilinear : OpenGLTexture::Filtering_Nearest;
		tex_params.use_mipmaps = false;
		tex_params.allow_compression = false;
		this->cosine_env_tex = loadCubeMap(face_maps, tex_params);
	}

	// Load specular reflection map (specular_refl_sky_no_sun_combined_x.exr)
	// These textures are generated by the code in glare-core\trunk\opengl\EnvMapProcessing.cpp
	{
		Map2DRef specular_refl_map = loadAndBlendMaps(
			sky_gl_data_dir + "/specular_refl_sky_no_sun_combined_" + toString(z_0) + ".exr",
			sky_gl_data_dir + "/specular_refl_sky_no_sun_combined_" + toString(z_1) + ".exr",
			w0, w1
		);

		TextureParams tex_params;
		tex_params.filtering = float_texture_filtering_support ? OpenGLTexture::Filtering_Bilinear : OpenGLTexture::Filtering_Nearest;
		tex_params.use_mipmaps = false;
		tex_params.allow_compression = false;
		tex_params.wrapping = OpenGLTexture::Wrapping_MirroredRepeat; // We will use mirroring to reflect the one side of the sphere we have in the texture map.
		this->specular_env_tex = getOrLoadOpenGLTextureForMap2D(OpenGLTextureKey("specular reflection map" + toString(sun_theta)), *specular_refl_map, tex_params);
	}


	// Load hi-res env map (sky_no_sun_x.exr)
	// These textures are generated in N:\indigo\trunk\indigo\SkyModel2Generator.cpp, SkyModel2Generator::makeSkyEnvMap().
	{
		Map2DRef env_map = loadAndBlendMaps(
			sky_gl_data_dir + "/sky_no_sun_" + toString(z_0) + ".exr",
			sky_gl_data_dir + "/sky_no_sun_" + toString(z_1) + ".exr",
			w0, w1
		);

		TextureParams tex_params;
		tex_params.filtering = float_texture_filtering_support ? OpenGLTexture::Filtering_Bilinear : OpenGLTexture::Filtering_Nearest;
		tex_params.wrapping = OpenGLTexture::Wrapping_Repeat;
		tex_params.allow_compression = false;
		tex_params.use_mipmaps = false;
		tex_params.wrapping = OpenGLTexture::Wrapping_MirroredRepeat; // We will use mirroring to reflect the one side of the sphere we have in the texture map.
		this->current_scene->env_ob->materials[0].albedo_texture = getOrLoadOpenGLTextureForMap2D(OpenGLTextureKey("hi res env map" + toString(sun_theta)), *env_map, tex_params);
		this->current_scene->env_ob->materials[0].albedo_texture->setTWrappingEnabled(false); // Disable wrapping in vertical direction to avoid grey dot straight up.

		this->current_scene->env_ob->materials[0].tex_matrix = Matrix2f(-1 / Maths::get2Pi<float>(), 0, 0, 1 / Maths::pi<float>());
	}

	this->loaded_maps_for_sun_dir = true;

	conPrint("OpenGLEngine::loadMapsForSunDir took " + timer.elapsedStringNSigFigs(5));
}


void OpenGLEngine::setSunDir(const Vec4f& d)
{
	this->sun_dir = d;

	if(myMax(std::fabs(d[0]), std::fabs(d[1])) < 1.0e-5f)
		this->sun_phi = 0;
	else
		this->sun_phi = std::atan2(d[1], d[0]);

	this->loaded_maps_for_sun_dir = false; // reload maps

	setEnvMapTransform(Matrix3f::rotationAroundZAxis(sun_phi));
}


const Vec4f OpenGLEngine::getSunDir() const
{
	return this->sun_dir;
}


void OpenGLEngine::setEnvMapTransform(const Matrix3f& transform)
{
	this->current_scene->env_ob->ob_to_world_matrix = Matrix4f(transform, Vec3f(0.f));
	this->current_scene->env_ob->ob_to_world_matrix.getUpperLeftInverseTranspose(/*inverse trans out=*/this->current_scene->env_ob->ob_to_world_normal_matrix);
}


void OpenGLEngine::setEnvMat(const OpenGLMaterial& env_mat_)
{
	this->current_scene->env_ob->materials[0] = env_mat_;
	if(this->current_scene->env_ob->materials[0].shader_prog.isNull())
		this->current_scene->env_ob->materials[0].shader_prog = env_prog;

	rebuildDenormalisedDrawData(*this->current_scene->env_ob);
}


void OpenGLEngine::setCirrusTexture(const Reference<OpenGLTexture>& tex)
{
	this->cirrus_tex = tex;
}


void OpenGLEngine::setDetailTexture(int index, const Reference<OpenGLTexture>& tex)
{
	assert(index >= 0 && index < 4);
	if(index >= 0 && index < 4)
		this->detail_tex[index] = tex;
	else
		throw glare::Exception("invalid detail texture index: " + toString(index));
}


void OpenGLEngine::setDetailHeightmap(int index, const OpenGLTextureRef& tex)
{
	assert(index >= 0 && index < 4);
	if(index >= 0 && index < 4)
		this->detail_heightmap[index] = tex;
	else
		throw glare::Exception("invalid detail heightmap index: " + toString(index));
}


OpenGLTextureRef OpenGLEngine::getDetailTexture(int index) const
{
	if(index >= 0 && index < 4)
		return detail_tex[index];
	else
		return NULL;
}


OpenGLTextureRef OpenGLEngine::getDetailHeightmap(int index) const
{
	if(index >= 0 && index < 4)
		return detail_heightmap[index];
	else
		return NULL;
}


void OpenGLEngine::getUniformLocations(Reference<OpenGLProgram>& prog)
{
	prog->uniform_locations.diffuse_tex_location			= prog->getUniformLocation("diffuse_tex");
	prog->uniform_locations.metallic_roughness_tex_location	= prog->getUniformLocation("metallic_roughness_tex");
	prog->uniform_locations.emission_tex_location			= prog->getUniformLocation("emission_tex");
	prog->uniform_locations.backface_albedo_tex_location	= prog->getUniformLocation("backface_albedo_tex");
	prog->uniform_locations.transmission_tex_location		= prog->getUniformLocation("transmission_tex");
	prog->uniform_locations.normal_map_location				= prog->getUniformLocation("normal_map");
	prog->uniform_locations.combined_array_tex_location		= prog->getUniformLocation("combined_array_tex");
	prog->uniform_locations.cosine_env_tex_location			= prog->getUniformLocation("cosine_env_tex");
	prog->uniform_locations.specular_env_tex_location		= prog->getUniformLocation("specular_env_tex");
	prog->uniform_locations.lightmap_tex_location			= prog->getUniformLocation("lightmap_tex");
	prog->uniform_locations.fbm_tex_location				= prog->getUniformLocation("fbm_tex");
	prog->uniform_locations.cirrus_tex_location				= prog->getUniformLocation("cirrus_tex");
	prog->uniform_locations.main_colour_texture_location	= prog->getUniformLocation("main_colour_texture");
	prog->uniform_locations.main_normal_texture_location	= prog->getUniformLocation("main_normal_texture");
	prog->uniform_locations.main_depth_texture_location		= prog->getUniformLocation("main_depth_texture");
	prog->uniform_locations.caustic_tex_a_location			= prog->getUniformLocation("caustic_tex_a");
	prog->uniform_locations.caustic_tex_b_location			= prog->getUniformLocation("caustic_tex_b");
	prog->uniform_locations.detail_tex_0_location			= prog->getUniformLocation("detail_tex_0");
	prog->uniform_locations.detail_tex_1_location			= prog->getUniformLocation("detail_tex_1");
	prog->uniform_locations.detail_tex_2_location			= prog->getUniformLocation("detail_tex_2");
	prog->uniform_locations.detail_tex_3_location			= prog->getUniformLocation("detail_tex_3");
	prog->uniform_locations.detail_heightmap_0_location		= prog->getUniformLocation("detail_heightmap_0");
	prog->uniform_locations.blue_noise_tex_location			= prog->getUniformLocation("blue_noise_tex");
	prog->uniform_locations.aurora_tex_location				= prog->getUniformLocation("aurora_tex");
	prog->uniform_locations.ssao_tex_location				= prog->getUniformLocation("ssao_tex");
	prog->uniform_locations.ssao_specular_tex_location		= prog->getUniformLocation("ssao_specular_tex");
	prog->uniform_locations.prepass_depth_tex_location		= prog->getUniformLocation("prepass_depth_tex");
	prog->uniform_locations.prepass_normal_tex_location		= prog->getUniformLocation("prepass_normal_tex");
	//prog->uniform_locations.snow_ice_normal_map_location	= prog->getUniformLocation("snow_ice_normal_map");

	prog->uniform_locations.dynamic_depth_tex_location		= prog->getUniformLocation("dynamic_depth_tex");
	prog->uniform_locations.static_depth_tex_location		= prog->getUniformLocation("static_depth_tex");
	prog->uniform_locations.shadow_texture_matrix_location	= prog->getUniformLocation("shadow_texture_matrix");

	//prog->uniform_locations.proj_view_model_matrix_location	= prog->getUniformLocation("proj_view_model_matrix");

	prog->uniform_locations.num_blob_positions_location		= prog->getUniformLocation("num_blob_positions");
	prog->uniform_locations.blob_positions_location			= prog->getUniformLocation("blob_positions");
}


struct BuildFBMNoiseTask : public glare::Task
{
	virtual void run(size_t /*thread_index*/)
	{
		float* const data_ = imagemap->getData();
		for(int y=begin_y; y<end_y; ++y)
		for(int x=0; x<(int)W; ++x)
		{
			float px = (float)x * (4.f / W);
			float py = (float)y * (4.f / W);

			// 1024 pixels are covered by 4 perlin noise grid cells for base octave.  So each cell corresponds to 1024 / 4 = 256 pixels.
			// So we need 8 octaves to get pixel-detail noise.
			const float v =  PerlinNoise::periodicFBM(px, py, /*num octaves=*/8, /*period=*/4);
			const float normalised_v = 0.5f + 0.5f*v; // Map from [-1, 1] to [0, 1].

			//const float v = Voronoi::voronoiFBM(Vec2f(px, py), /*num octaves=*/8);
			//const float normalised_v = 0.5f*v; // Map from [-1, 1] to [0, 1].

			data_[y * W + x] = normalised_v;
		}
	}

	ImageMapFloatRef imagemap;
	size_t W;
	int begin_y, end_y;
};


// See https://www.khronos.org/opengl/wiki/Program_Introspection#Uniforms_and_blocks
static size_t getSizeOfUniformBlockInOpenGL(OpenGLProgramRef& prog, const char* block_name)
{
	const GLuint block_index = glGetUniformBlockIndex(prog->program, block_name);
	if(block_index == GL_INVALID_INDEX)
		throw glare::Exception("getSizeOfUniformBlockInOpenGL(): No such named uniform block '" + std::string(block_name) + "'");
	GLint size = 0;
	glGetActiveUniformBlockiv(prog->program, block_index, GL_UNIFORM_BLOCK_DATA_SIZE, &size);
	return size;
}


static void printFieldOffsets(OpenGLProgramRef& prog, const char* block_name)
{
#if !defined(EMSCRIPTEN)
	const GLuint block_index = glGetUniformBlockIndex(prog->program, block_name);
	if(block_index == GL_INVALID_INDEX)
		throw glare::Exception("printFieldOffsets(): No such named uniform block '" + std::string(block_name));

	GLint num_fields = 0;
	glGetActiveUniformBlockiv(prog->program, block_index, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &num_fields); // Get 'number of active uniforms within this block'.

	std::map<int, std::string> offset_to_name;
	for(int i=0; i<num_fields; ++i)
	{
		char buf[2048];
		GLsizei string_len = 0;
		glGetActiveUniformName(prog->program, i, sizeof(buf), &string_len, buf);
		std::string name(buf, buf + string_len);
		//conPrint(name);

		// Get offset:
		GLuint index = i;
		GLint offset = 0;
		glGetActiveUniformsiv(prog->program, /*uniformCount=*/1, /*indices=*/&index, GL_UNIFORM_OFFSET, &offset);
		//printVar(offset);

		offset_to_name[offset] = name;
	}
	for(auto it = offset_to_name.begin(); it != offset_to_name.end(); ++it)
	{
		conPrint("Offset " + toString(it->first) + ": " + it->second);
	}
	conPrint("Struct size: " + toString(getSizeOfUniformBlockInOpenGL(prog, block_name)));
#endif
}


void checkUniformBlockSize(OpenGLProgramRef prog, const char* block_name, size_t target_size)
{
	const size_t block_size = getSizeOfUniformBlockInOpenGL(prog, block_name);
	if(block_size != target_size)
	{
		conPrint("Uniform block '" + std::string(block_name) + "' had size " + toString(block_size) + " B, expected " + toString(target_size) + " B.");
		printFieldOffsets(prog, block_name);
		assert(0);
	}
}


static void bindUniformBlockToProgram(OpenGLProgramRef prog, const char* name, int binding_point_index)
{
	unsigned int block_index = glGetUniformBlockIndex(prog->program, name);
	assert(block_index != GL_INVALID_INDEX);
	glUniformBlockBinding(prog->program, block_index, /*binding point=*/binding_point_index);
}


// See https://registry.khronos.org/OpenGL-Refpages/gl4/html/glShaderStorageBlockBinding.xhtml
void bindShaderStorageBlockToProgram(OpenGLProgramRef prog, const char* name, int binding_point_index)
{
#if defined(OSX) || defined(EMSCRIPTEN)
	assert(0); // glShaderStorageBlockBinding is not defined on Mac. (SSBOs are not supported)
#else
	unsigned int storage_block_index = glGetProgramResourceIndex(prog->program, GL_SHADER_STORAGE_BLOCK, name);
	assert(storage_block_index != GL_INVALID_INDEX);
	glShaderStorageBlockBinding(prog->program, storage_block_index, /*binding point=*/binding_point_index);
#endif
}


static const int FINAL_IMAGING_BLOOM_STRENGTH_UNIFORM_INDEX = 0;
static const int FINAL_IMAGING_EXPOSURE_FACTOR_UNIFORM_INDEX = 1;
static const int FINAL_IMAGING_SATURATION_MULTIPLIER_UNIFORM_INDEX = 2;
static const int FINAL_IMAGING_BLUR_TEX_UNIFORM_START = 3;

static const int OIT_COMPOSITE_TRANSPARENT_ACCUM_TEX_UNIFORM_INDEX = 0;
static const int OIT_COMPOSITE_TOTAL_TRANSMITTANCE_TEX_UNIFORM_INDEX = 1;

static const int NUM_BLUR_DOWNSIZES = 8;

static const int PHONG_UBO_BINDING_POINT_INDEX = 0;
static const int SHARED_VERT_UBO_BINDING_POINT_INDEX = 1;
static const int PER_OBJECT_VERT_UBO_BINDING_POINT_INDEX = 2;
static const int DEPTH_UNIFORM_UBO_BINDING_POINT_INDEX = 3;
static const int MATERIAL_COMMON_UBO_BINDING_POINT_INDEX = 4;
static const int LIGHT_DATA_UBO_BINDING_POINT_INDEX = 5; // Just used on Mac
static const int JOINT_MATRICES_UBO_BINDING_POINT_INDEX = 6;
static const int OB_JOINT_AND_MAT_INDICES_UBO_BINDING_POINT_INDEX = 7;

static const int LIGHT_DATA_SSBO_BINDING_POINT_INDEX = 0;
static const int PER_OB_VERT_DATA_SSBO_BINDING_POINT_INDEX = 1;
static const int PHONG_DATA_SSBO_BINDING_POINT_INDEX = 2;
static const int OB_AND_MAT_INDICES_SSBO_BINDING_POINT_INDEX = 3;
static const int JOINT_MATRICES_SSBO_BINDING_POINT_INDEX = 4;
static const int DATA_UPDATES_SSBO_BINDING_POINT_INDEX = 5;


static const size_t MAX_BUFFERED_DRAW_COMMANDS = 16384;


struct DataUpdateStruct
{
	PerObjectVertUniforms data;

	uint32 write_index; // location to write data to

	uint32 padding_0;
	uint32 padding_1;
	uint32 padding_2;
};


bool OpenGLEngine::static_init_done = false;


void OpenGLEngine::staticInit()
{
	if(static_init_done)
		return;

#if !defined(OSX) && !defined(EMSCRIPTEN)
	if(gl3wInit() != 0)
	{
		conPrint("gl3wInit failed.");
		return;
	}
#endif

	OpenGLExtensions::init();

	static_init_done = true;
}


void OpenGLEngine::initialise(const std::string& data_dir_, Reference<TextureServer> texture_server_, PrintOutput* print_output_, glare::TaskManager* main_task_manager_, glare::TaskManager* high_priority_task_manager_,
	Reference<glare::Allocator> mem_allocator_)
{
	ZoneScoped; // Tracy profiler

	data_dir = data_dir_;
	texture_server = texture_server_;
	print_output = print_output_;
	main_task_manager = main_task_manager_;
	high_priority_task_manager = high_priority_task_manager_;
	mem_allocator = mem_allocator_;

	if(!static_init_done)
		staticInit();

	this->opengl_vendor		= std::string((const char*)glGetString(GL_VENDOR));
	this->opengl_renderer	= std::string((const char*)glGetString(GL_RENDERER));
	this->opengl_version	= std::string((const char*)glGetString(GL_VERSION));
	this->glsl_version		= std::string((const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

#if !defined(OSX) && !defined(EMSCRIPTEN)
	// Check to see if OpenGL 3.0 is supported, which is required for our VAO usage etc...  (See https://www.opengl.org/wiki/History_of_OpenGL#OpenGL_3.0_.282008.29 etc..)
	if(!gl3wIsSupported(3, 0))
	{
		init_succeeded = false;
		initialisation_error_msg = "OpenGL version is too old (< v3.0), version is " + std::string((const char*)glGetString(GL_VERSION));
		conPrint(initialisation_error_msg);
		return;
	}
#endif

	if(settings.enable_debug_output)
	{
#if !defined(OSX) && !defined(EMSCRIPTEN) // Apple doesn't seem to support glDebugMessageCallback: (https://stackoverflow.com/questions/24836127/gldebugmessagecallback-on-osx-xcode-5)
		// Enable error message handling,.
		// See "Porting Source to Linux: Valve's Lessons Learned": https://developer.nvidia.com/sites/default/files/akamai/gamedev/docs/Porting%20Source%20to%20Linux.pdf
		glDebugMessageCallback(myMessageCallback, this); 
		glEnable(GL_DEBUG_OUTPUT);
		// glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); // When this is enabled the offending gl call will be on the call stack when the message callback is called.
#endif
	}

	// Check if anisotropic texture filtering is available, and get max anisotropy if so.  
	// See 'Texture Mapping in OpenGL: Beyond the Basics' - http://www.informit.com/articles/article.aspx?p=770639&seqNum=2
	//if(GLEW_EXT_texture_filter_anisotropic)
	// According to https://paroj.github.io/gltut/Texturing/Tut15%20Anisotropy.html#d5e11942, while anisotropic filtering is not technically part of core OpenGL (is still an extension)
	// It's always supported in practice.
	{
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropy);
	}

	this->texture_compression_s3tc_support = false;
	this->texture_compression_ETC_support = false;
#if defined(OSX) || EMSCRIPTEN
	this->texture_compression_BC6H_support = false; // Initial value, can be enabled by EXT_texture_compression_bptc detection below.
#else
	this->texture_compression_BC6H_support = true;
#endif
	this->GL_ARB_bindless_texture_support = false;
	this->clip_control_support = false;
	this->GL_ARB_shader_storage_buffer_object_support = false;
	this->parallel_shader_compile_support = false;
	this->EXT_color_buffer_float_support = false;
#if EMSCRIPTEN
	this->float_texture_filtering_support = false; // Filtering of floating point textures is not available by default in WebGL, but can be enabled by OES_texture_float_linear extension.
#else
	this->float_texture_filtering_support = true;
#endif
	this->EXT_disjoint_timer_query_webgl2_support = false;
	this->GL_EXT_memory_object_support = false;
	this->GL_EXT_memory_object_win32_support = false;
	this->GL_EXT_win32_keyed_mutex_support = false;

	// Check OpenGL extensions
	GLint n = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &n);
	for(GLint i = 0; i < n; i++)
	{
		const char* ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
		// conPrint("Extension " + toString(i) + ": " + std::string(ext));
		if(stringEqual(ext, "GL_EXT_texture_compression_s3tc")) this->texture_compression_s3tc_support = true;
		if(stringEqual(ext, "GL_ARB_bindless_texture")) this->GL_ARB_bindless_texture_support = true;
		if(stringEqual(ext, "GL_ARB_clip_control")) this->clip_control_support = true;
		if(stringEqual(ext, "GL_ARB_shader_storage_buffer_object")) this->GL_ARB_shader_storage_buffer_object_support = true;
		if(stringEqual(ext, "GL_KHR_parallel_shader_compile")) parallel_shader_compile_support = true;
		if(stringEqual(ext, "GL_EXT_memory_object")) GL_EXT_memory_object_support = true;
		if(stringEqual(ext, "GL_EXT_memory_object_win32")) GL_EXT_memory_object_win32_support = true;
		if(stringEqual(ext, "GL_EXT_win32_keyed_mutex")) GL_EXT_win32_keyed_mutex_support = true;

#if EMSCRIPTEN
		if(stringEqual(ext, "WEBGL_compressed_texture_s3tc")) this->texture_compression_s3tc_support = true;
		if(stringEqual(ext, "WEBGL_compressed_texture_etc")) this->texture_compression_ETC_support = true; // See https://registry.khronos.org/webgl/specs/latest/2.0/#5.37, ("No ETC2 and EAC compressed texture formats")
		if(stringEqual(ext, "EXT_color_buffer_float")) this->EXT_color_buffer_float_support = true;
		if(stringEqual(ext, "EXT_clip_control")) this->clip_control_support = true;
		if(stringEqual(ext, "OES_texture_float_linear")) this->float_texture_filtering_support = true;
		if(stringEqual(ext, "EXT_texture_compression_bptc")) this->texture_compression_BC6H_support = true;
		if(stringEqual(ext, "EXT_disjoint_timer_query_webgl2")) this->EXT_disjoint_timer_query_webgl2_support = true;
#endif
	}

	// Get max texture size
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &this->max_texture_size);

#if defined(OSX)
	// Am pretty sure there is no bindless texture support on Mac (bindless texture support is opengl 4.0), set it to false anyway to make sure OpenGLTexture::getBindlessTextureHandle() is not called.
	this->GL_ARB_bindless_texture_support = false;
#endif

#if EMSCRIPTEN
	// WebGL doesn't support glBlendFunci which we need for order-independent transparency, so don't use on Web.
	this->use_order_indep_transparency = false;
#else
	this->use_order_indep_transparency = settings.render_to_offscreen_renderbuffers;
#endif

	// Query amount of GPU RAM available.  See https://stackoverflow.com/questions/4552372/determining-available-video-memory
	this->total_available_GPU_mem_B = 0;
	this->total_available_GPU_VBO_mem_B = 0;

	// For nvidia drivers:
	if(StringUtils::containsString(::toLowerCase(opengl_vendor), "nvidia"))
	{
		GLint available_kB = 0;
		glGetIntegerv(GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &available_kB);
		GLenum code = glGetError();
		if(code == GL_NO_ERROR)
		{
			this->total_available_GPU_mem_B = (uint64)available_kB * 1024;
		}
	}

	// For AMD drivers:
	if(StringUtils::containsString(::toLowerCase(opengl_vendor), "amd"))
	{
		GLint available_kB[4] = { 0, 0, 0, 0 };
		glGetIntegerv(VBO_FREE_MEMORY_ATI, available_kB); // "The query returns a 4-tuple integer where the values are in Kbyte ..."
		GLenum code = glGetError();
		if(code == GL_NO_ERROR)
		{
			this->total_available_GPU_VBO_mem_B = (uint64)available_kB[0] * 1024; // "param[0] - total memory free in the pool": https://www.khronos.org/registry/OpenGL/extensions/ATI/ATI_meminfo.txt
		}
	}

	// Work out a good size to use for VBOs.
	// If the VBO size is much too small (like 8MB), some allocations won't fit in an empty VBO.
	// If the VBO size is a little too small, then multiple VBOs will be allocated and we will have to pay the driver overhead of changing VBO bindings.
	// if the VBO size is much to big, and it can't actually fit in GPU RAM, it will be allocated in host RAM (tested on RTX 3080), which results in catastrophically bad performance.
	// Note that the VBO size will be used individually for both vertex and index data.
	if(this->total_available_GPU_mem_B != 0) // Set by NVidia drivers
	{
		vert_buf_allocator->use_VBO_size_B = this->total_available_GPU_mem_B / 16;
	}
	else if(this->total_available_GPU_VBO_mem_B != 0) // Set by AMD drivers
	{
		// NOTE: what's the relation between VBO mem available and total mem available?
		vert_buf_allocator->use_VBO_size_B = this->total_available_GPU_VBO_mem_B / 16;
	}
	else
		vert_buf_allocator->use_VBO_size_B = 64 * 1024 * 1024; // A reasonably small size, needs to work well for weaker GPUs


	// Work out if we are running in RenderDoc.  See https://renderdoc.org/docs/in_application_api.html
	// We do this so we can force the shader version directive when running in RenderDoc.
#if defined(_WIN32)
	if(GetModuleHandleA("renderdoc.dll") != NULL)
	{
		conPrint("Running in renderdoc!");
		running_in_renderdoc = true;
	}
#endif


	glClearColor(current_scene->background_colour.r, current_scene->background_colour.g, current_scene->background_colour.b, 1.f);

	glEnable(GL_DEPTH_TEST);	// Enable z-buffering
	glDisable(GL_CULL_FACE);	// Disable backface culling


	this->sphere_meshdata = MeshPrimitiveBuilding::makeSphereMesh(*vert_buf_allocator);
	this->line_meshdata = MeshPrimitiveBuilding::makeLineMesh(*vert_buf_allocator);
	this->cube_meshdata = MeshPrimitiveBuilding::makeCubeMesh(*vert_buf_allocator);
	this->unit_quad_meshdata = MeshPrimitiveBuilding::makeUnitQuadMesh(*vert_buf_allocator);
	this->sprite_quad_meshdata = MeshPrimitiveBuilding::makeSpriteQuad(*vert_buf_allocator);

	this->current_scene->env_ob->mesh_data = sphere_meshdata;

	const std::string gl_data_dir = data_dir + "/gl_data";

	try
	{
		// Make noise texture
		if(false)
		{
			Timer timer;
			const size_t W = 128;
			std::vector<float> data(W * W);
			float maxval = 0;
			float minval = 0;
			for(int y=0; y<W; ++y)
			for(int x=0; x<W; ++x)
			{
				float px = (float)x * (4.f / W);
				float py = (float)y * (4.f / W);
				const float v =  PerlinNoise::periodicNoise(px, py, /*period=*/4);
				const float normalised_v = 0.5f + 0.5f*v; // Map from [-1, 1] to [0, 1].
				minval = myMin(minval, normalised_v);
				maxval = myMax(maxval, normalised_v);
				data[y * W + x] = normalised_v;
			}
			printVar(minval);
			printVar(maxval);

			// EXRDecoder::saveImageToEXR(data.data(), W, W, 1, false, "noise.exr", "noise", EXRDecoder::SaveOptions());

			noise_tex = new OpenGLTexture(W, W, this, 
				ArrayRef<uint8>((const uint8*)data.data(), data.size() * sizeof(float)), 
				OpenGLTextureFormat::Format_Greyscale_Float, OpenGLTexture::Filtering_Fancy);

			conPrint("noise_tex creation took " + timer.elapsedString());
		}

		// Make FBM texture
		{
			Timer timer;
			const size_t W = 1024;
			fbm_imagemap = new ImageMapFloat(W, W, 1);

			glare::TaskManager& manager = *this->main_task_manager;
			const size_t num_tasks = myMax<size_t>(1, manager.getNumThreads());
			const size_t num_rows_per_task = Maths::roundedUpDivide(W, num_tasks);

			glare::TaskGroupRef group = new glare::TaskGroup();
			group->tasks.resize(num_tasks);

			for(size_t t=0; t<num_tasks; ++t)
			{
				BuildFBMNoiseTask* task = new BuildFBMNoiseTask();
				task->imagemap = fbm_imagemap;
				task->W = W;
				task->begin_y = (int)myMin(t       * num_rows_per_task, W);
				task->end_y   = (int)myMin((t + 1) * num_rows_per_task, W);
				group->tasks[t] = task;
			}
			manager.runTaskGroup(group);

			// EXRDecoder::saveImageToEXR(data.data(), W, W, 1, false, "fbm.exr", "noise", EXRDecoder::SaveOptions());

			if(float_texture_filtering_support)
			{
				conPrint("Using float FBM GPU texture");
				fbm_tex = new OpenGLTexture(W, W, this, ArrayRef<uint8>((const uint8*)fbm_imagemap->getData(), fbm_imagemap->numPixels() * sizeof(float)),
					OpenGLTextureFormat::Format_Greyscale_Float, OpenGLTexture::Filtering_Fancy);
			}
			else
			{
				conPrint("Using uint8 FBM GPU texture");

				// Convert to something we do support
				ImageMapUInt8 new_map(W, W, 1);
				for(size_t i=0; i<W*W; ++i)
					new_map.getPixel(i)[0] = (uint8)(myMax(0.f, fbm_imagemap->getPixel(i)[0] * 255.01f));

				fbm_tex = new OpenGLTexture(W, W, this, ArrayRef<uint8>(new_map.getData(), new_map.getDataSizeB()),
					OpenGLTextureFormat::Format_Greyscale_Uint8, OpenGLTexture::Filtering_Fancy);
			}

			conPrint("fbm_tex creation took " + timer.elapsedString());
		}

		// Load blue noise texture
		{
			const std::string blue_noise_map_path = gl_data_dir + "/HDR_RGBA_0.png";
			Reference<Map2D> blue_noise_map = ImFormatDecoder::decodeImage(".", blue_noise_map_path);

			// Convert to floating point. TODO: optimise
			runtimeCheck(blue_noise_map.downcastToPtr<ImageMapUInt16>());
			ImageMapFloatRef float_map = new ImageMapFloat(blue_noise_map->getMapWidth(), blue_noise_map->getMapHeight(), blue_noise_map->numChannels());
			for(size_t i=0; i<float_map->getDataSize(); ++i)
				float_map->getData()[i] = blue_noise_map.downcastToPtr<ImageMapUInt16>()->getData()[i] * (1 / 65535.f);


			TextureParams params;
			params.filtering = OpenGLTexture::Filtering_Nearest;
			params.wrapping = OpenGLTexture::Wrapping_Repeat;
			params.allow_compression = false;
			params.use_sRGB = false;
			params.use_mipmaps = false;
			params.convert_float_to_half = false;
			this->blue_noise_tex = getOrLoadOpenGLTextureForMap2D(OpenGLTextureKey(blue_noise_map_path), *float_map, params);
		}

		{
			ImageMapUInt8Ref dummy_black_tex_map = new ImageMapUInt8(1, 1, 1);
			dummy_black_tex_map->getPixel(0, 0)[0] = 0;
			this->dummy_black_tex = getOrLoadOpenGLTextureForMap2D(OpenGLTextureKey("__dummy_black_tex__"), *dummy_black_tex_map);
		}


		const bool is_intel_vendor = openglDriverVendorIsIntel();
		const bool is_ati_vendor   = openglDriverVendorIsATI();

		// Build light data buffer.  We will use a SSBO on non-mac, and a UBO on mac, since it doesn't support OpenGL 4.3 for SSBOs.
		// Intel drivers seem to choke on std430 layouts, see https://github.com/RPCS3/rpcs3/issues/12285
		// So don't try to use SSBOs (which use the std430 layout) on Intel.
		if(GL_ARB_shader_storage_buffer_object_support && !is_intel_vendor)
		{
			light_buffer = new SSBO();
			const size_t light_buf_items = 1 << 14;
			light_buffer->allocate(sizeof(LightGPUData) * light_buf_items, /*map_memory=*/false);

			for(size_t i=0; i<light_buf_items; ++i)
				light_free_indices.insert((int)i);
		}
		else
		{
			light_ubo = new UniformBufOb();
			const size_t light_buf_items = 256;
			light_ubo->allocate(sizeof(LightGPUData) * light_buf_items);

			for(size_t i=0; i<light_buf_items; ++i)
				light_free_indices.insert((int)i);
		}

#ifdef OSX
		const bool is_intel_on_mac = is_intel_vendor;
#else
		const bool is_intel_on_mac = false;
#endif
		if(is_intel_on_mac)
			settings.shadow_mapping = false; // Shadow mapping with Intel drivers on Mac just seems to cause crashes, so disable shadow mapping.

		// Don't use bindless textures with Intel drivers/GPUs, they don't seem to work properly.  (Deadstock was having issues with textures being seemingly randomly assigned)
		// On ATI/AMD, allocating and making a sufficient number of bindless textures resident causes a crash, somewhere around 2800 bindless textures.
		// Avoid this crash by just not using them.
		use_bindless_textures = settings.allow_bindless_textures && GL_ARB_bindless_texture_support && !is_intel_vendor && !is_ati_vendor;


		use_reverse_z = clip_control_support;


		use_multi_draw_indirect = false;
		use_ob_and_mat_data_gpu_resident = false;
#if !defined(OSX) && !defined(EMSCRIPTEN)
		if(settings.allow_multi_draw_indirect && gl3wIsSupported(4, 6) && use_bindless_textures)
			use_multi_draw_indirect = true; // Our multi-draw-indirect code uses bindless textures and gl_DrawID, which requires GLSL 4.60 (or ARB_shader_draw_parameters)

		if(use_bindless_textures)
			use_ob_and_mat_data_gpu_resident = true; // GPU-resident materials require bindless textures
#endif

		// We will use the 'oct24' format for encoding normals, see 'A Survey of Efficient Representations for Independent Unit Vectors', section 3.3.
		// Use an integer format, so that MSAA downsampling gives valid normals.
		// 
		// Format_RGB_Integer_Uint8 (GL_RGB8UI) isn't supported as a renderbuffer format in OpenGL ES (see https://registry.khronos.org/OpenGL-Refpages/es3.0/html/glRenderbufferStorage.xhtml),
		// so use GL_RGB8AUI, which is supported in OpenGL ES.
		// However, "GL_INVALID_OPERATION is generated if internalformat is a signed or unsigned integer format and samples is greater than 0.", so we can't use an integer format with MSAA in OpenGL ES.
		// 
		// Int formats also doesn't work properly on Apple machines. (shader doesn't output anything, just black output)
#if EMSCRIPTEN
		normal_texture_is_uint = (settings.msaa_samples <= 1); // Only use if MSAA is off.
#elif defined(__APPLE__)
		normal_texture_is_uint = false;
#else
		normal_texture_is_uint = true;
#endif

		// On OS X, we can't just not define things, we need to define them as zero or we get GLSL syntax errors.
		preprocessor_defines += "#define SHADOW_MAPPING " + (settings.shadow_mapping ? std::string("1") : std::string("0")) + "\n";
		preprocessor_defines += "#define NUM_DEPTH_TEXTURES " + (settings.shadow_mapping ? 
			toString(ShadowMapping::numDynamicDepthTextures() + shadow_mapping->numStaticDepthTextures()) : std::string("0")) + "\n";
		
		preprocessor_defines += "#define NUM_DYNAMIC_DEPTH_TEXTURES " + (settings.shadow_mapping ? 
			toString(ShadowMapping::numDynamicDepthTextures()) : std::string("0")) + "\n";
		
		preprocessor_defines += "#define NUM_STATIC_DEPTH_TEXTURES " + (settings.shadow_mapping ? 
			toString(ShadowMapping::numStaticDepthTextures()) : std::string("0")) + "\n";

		preprocessor_defines += "#define DEPTH_TEXTURE_SCALE_MULT " + (settings.shadow_mapping ? doubleLiteralString(shadow_mapping->getDynamicDepthTextureScaleMultiplier()) : std::string("1.0")) + "\n";

		preprocessor_defines += "#define DEPTH_FOG " + (settings.depth_fog ? std::string("1") : std::string("0")) + "\n";
		
		preprocessor_defines += "#define RENDER_SKY_AND_CLOUD_REFLECTIONS " + (settings.render_sun_and_clouds ? std::string("1") : std::string("0")) + "\n";
		preprocessor_defines += "#define RENDER_SUN_AND_SKY " + (settings.render_sun_and_clouds ? std::string("1") : std::string("0")) + "\n";
		preprocessor_defines += "#define RENDER_CLOUD_SHADOWS " + (settings.render_sun_and_clouds ? std::string("1") : std::string("0")) + "\n";
		preprocessor_defines += "#define UNDERWATER_CAUSTICS " + (settings.render_water_caustics ? std::string("1") : std::string("0")) + "\n";
		preprocessor_defines += "#define DRAW_AURORA " + (draw_aurora ? std::string("1") : std::string("0")) + "\n";

		// static_cascade_blending causes a white-screen error on many Intel GPUs.
		const bool static_cascade_blending = !is_intel_vendor;
		preprocessor_defines += "#define DO_STATIC_SHADOW_MAP_CASCADE_BLENDING " + (static_cascade_blending ? std::string("1") : std::string("0")) + "\n";

		preprocessor_defines += "#define USE_BINDLESS_TEXTURES " + (use_bindless_textures ? std::string("1") : std::string("0")) + "\n";
		
		preprocessor_defines += "#define USE_MULTIDRAW_ELEMENTS_INDIRECT " + (use_multi_draw_indirect ? std::string("1") : std::string("0")) + "\n";
		
		preprocessor_defines += "#define OB_AND_MAT_DATA_GPU_RESIDENT " + (use_ob_and_mat_data_gpu_resident ? std::string("1") : std::string("0")) + "\n";

		preprocessor_defines += "#define USE_SSBOS " + (light_buffer.nonNull() ? std::string("1") : std::string("0")) + "\n";

		preprocessor_defines += "#define DO_POST_PROCESSING " + (settings.render_to_offscreen_renderbuffers ? std::string("1") : std::string("0")) + "\n";
		const bool do_screenspace_refl_and_refr = settings.screenspace_refl_and_refr && settings.render_to_offscreen_renderbuffers; // Screenspace reflections require rendering to offscreen buffers.
		preprocessor_defines += "#define WATER_DO_SCREENSPACE_REFL_AND_REFR " + (do_screenspace_refl_and_refr ? std::string("1") : std::string("0")) + "\n";

		preprocessor_defines += "#define MAIN_BUFFER_MSAA_SAMPLES 1\n"; // Since we are using render buffers currently, we will resolve them down to a non-MSAA texture using blitBuffer.  So 
		// So the shaders can just consider the main buffer source textures to not have MSAA.

		preprocessor_defines += "#define USE_REVERSE_Z " + (use_reverse_z ? std::string("1") : std::string("0")) + "\n";

		preprocessor_defines += "#define ORDER_INDEPENDENT_TRANSPARENCY " + (use_order_indep_transparency ? std::string("1") : std::string("0")) + "\n";

		preprocessor_defines += "#define NORMAL_TEXTURE_IS_UINT " + (normal_texture_is_uint ? std::string("1") : std::string("0")) + "\n";

		preprocessor_defines += "#define SSAO_SUPPORT " + (settings.ssao_support ? std::string("1") : std::string("0")) + "\n";


		if(use_bindless_textures)
			preprocessor_defines += "#extension GL_ARB_bindless_texture : require\n";

#if defined(EMSCRIPTEN)
		preprocessor_defines += "precision mediump float;\n"; // WebGL needs this stuff.
		preprocessor_defines += "precision mediump sampler2DShadow;\n";
		preprocessor_defines += "precision mediump sampler2DArray;\n";
		preprocessor_defines += "precision mediump usampler2D;\n";
#endif

		int use_glsl_version = 330;
#if !defined(OSX) && !defined(EMSCRIPTEN)
		if(gl3wIsSupported(4, 3))
			use_glsl_version = 430; // 4.30 is required for SSBO
		// Not entirely sure which GLSL version the GL_ARB_bindless_texture extension requires, it seems to be 4.0.0 though. (https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_bindless_texture.txt)

		// gl_DrawID requires GLSL 4.60 or ARB_shader_draw_parameters (https://www.khronos.org/opengl/wiki/Built-in_Variable_(GLSL))
		if(use_multi_draw_indirect)
			use_glsl_version = 460;
#endif

#if defined(EMSCRIPTEN)
		this->version_directive = "#version 300 es"; // WebGL 2 uses OpenGL ES 3.0, which uses GLSL v3.00.
#else
		this->version_directive = "#version " + toString(use_glsl_version) + " core";
#endif

		const std::string use_shader_dir = data_dir + "/shaders";

		if(use_ob_and_mat_data_gpu_resident)
		{
			// Allocate per_ob_vert_data_buffer
			{
				per_ob_vert_data_buffer = new SSBO();
				const size_t num_items = 1 << 14;
				per_ob_vert_data_buffer->allocate(sizeof(PerObjectVertUniforms) * num_items, /*map_memory=*/false);

				for(size_t i=0; i<num_items; ++i)
					per_ob_vert_data_free_indices.insert((int)i);
			}

			// Allocate phong_buffer (material info)
			{
				phong_buffer = new SSBO();
				const size_t num_items = 1 << 16;
				phong_buffer->allocate(sizeof(PhongUniforms) * num_items, /*map_memory=*/false);

				for(size_t i=0; i<num_items; ++i)
					phong_buffer_free_indices.insert((int)i);
			}

			// Allocate joint matrices SSBO for MDI
			{
				joint_matrices_ssbo = new SSBO();
				const size_t num_items = 1 << 14;
				joint_matrices_ssbo->allocate(sizeof(Matrix4f) * num_items, /*map_memory=*/false);
				joint_matrices_allocator = new glare::BestFitAllocator(num_items);
			}
		}


#if MULTIPLE_PHONG_UNIFORM_BUFS_SUPPORT
		const size_t num_phong_uniform_buf_obs = settings.use_multiple_phong_uniform_bufs ? 64 : 1;
		for(size_t i=0; i<num_phong_uniform_buf_obs; ++i)
		{
			phong_uniform_buf_obs[i] = new UniformBufOb();
			phong_uniform_buf_obs[i]->allocate(sizeof(PhongUniforms));
		}
#else
		phong_uniform_buf_ob = new UniformBufOb();
		phong_uniform_buf_ob->allocate(sizeof(PhongUniforms));
#endif

		material_common_uniform_buf_ob = new UniformBufOb();
		material_common_uniform_buf_ob->allocate(sizeof(MaterialCommonUniforms));

		shared_vert_uniform_buf_ob = new UniformBufOb();
		shared_vert_uniform_buf_ob->allocate(sizeof(SharedVertUniforms));

		per_object_vert_uniform_buf_ob = new UniformBufOb();
		per_object_vert_uniform_buf_ob->allocate(sizeof(PerObjectVertUniforms));

		depth_uniform_buf_ob = new UniformBufOb();
		depth_uniform_buf_ob->allocate(sizeof(DepthUniforms));

		joint_matrices_buf_ob = new UniformBufOb();
		joint_matrices_buf_ob->allocate(sizeof(Matrix4f) * max_num_joint_matrices_per_ob);

		ob_joint_and_mat_indices_uniform_buf_ob = new UniformBufOb();
		ob_joint_and_mat_indices_uniform_buf_ob->allocate(sizeof(ObJointAndMatIndicesStruct));



#if MULTIPLE_PHONG_UNIFORM_BUFS_SUPPORT
		glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/PHONG_UBO_BINDING_POINT_INDEX, this->phong_uniform_buf_obs[0]->handle);
		this->current_bound_phong_uniform_buf_ob_index = 0;
#else
		glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/PHONG_UBO_BINDING_POINT_INDEX, this->phong_uniform_buf_ob->handle);
#endif

		glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/SHARED_VERT_UBO_BINDING_POINT_INDEX, this->shared_vert_uniform_buf_ob->handle);
		glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/PER_OBJECT_VERT_UBO_BINDING_POINT_INDEX, this->per_object_vert_uniform_buf_ob->handle);
		glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/DEPTH_UNIFORM_UBO_BINDING_POINT_INDEX, this->depth_uniform_buf_ob->handle);
		glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/MATERIAL_COMMON_UBO_BINDING_POINT_INDEX, this->material_common_uniform_buf_ob->handle);
		glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/JOINT_MATRICES_UBO_BINDING_POINT_INDEX, this->joint_matrices_buf_ob->handle);
		glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/OB_JOINT_AND_MAT_INDICES_UBO_BINDING_POINT_INDEX, this->ob_joint_and_mat_indices_uniform_buf_ob->handle);
		
		if(light_buffer.nonNull())
		{
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, /*binding point=*/LIGHT_DATA_SSBO_BINDING_POINT_INDEX, this->light_buffer->handle);
		}
		else
		{
			assert(light_ubo.nonNull());
			glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/LIGHT_DATA_UBO_BINDING_POINT_INDEX, this->light_ubo->handle);
		}

		if(use_ob_and_mat_data_gpu_resident)
		{
			// Bind per-object vert data SSBO
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, /*binding point=*/PER_OB_VERT_DATA_SSBO_BINDING_POINT_INDEX, this->per_ob_vert_data_buffer->handle);
		
			// Bind joint matrices SSBO
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, /*binding point=*/JOINT_MATRICES_SSBO_BINDING_POINT_INDEX, this->joint_matrices_ssbo->handle);

			// Bind phong data SSBO
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, /*binding point=*/PHONG_DATA_SSBO_BINDING_POINT_INDEX, this->phong_buffer->handle);

#if USE_DRAW_INDIRECT_BUFFER_FOR_MDI
			draw_indirect_buffer = new DrawIndirectBuffer();
			draw_indirect_buffer->allocate(sizeof(DrawElementsIndirectCommand) * MAX_BUFFERED_DRAW_COMMANDS/* * 3*/); // For circular buffer use, allocate an extra factor of space (3) in the buffer.
			draw_indirect_buffer->unbind();
#endif

#if USE_CIRCULAR_BUFFERS_FOR_MDI
			draw_indirect_buffer->bind();
			draw_indirect_circ_buf.init(
				glMapBufferRange(GL_DRAW_INDIRECT_BUFFER, /*offset=*/0, /*length=*/draw_indirect_buffer->byteSize(), GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT),
				draw_indirect_buffer->byteSize()
			);
			draw_indirect_buffer->unbind();
#endif

			draw_commands_buffer.resizeNoCopy(MAX_BUFFERED_DRAW_COMMANDS);

			ob_and_mat_indices_ssbo = new SSBO();
			ob_and_mat_indices_ssbo->allocate((sizeof(int) * OB_AND_MAT_INDICES_STRIDE) * MAX_BUFFERED_DRAW_COMMANDS, /*map_memory=*/false);
			//ob_and_mat_indices_ssbo->allocateForMapping((sizeof(int) * 4) * MAX_BUFFERED_DRAW_COMMANDS * 3);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, /*binding point=*/OB_AND_MAT_INDICES_SSBO_BINDING_POINT_INDEX, this->ob_and_mat_indices_ssbo->handle);

			ob_and_mat_indices_buffer.resizeNoCopy(OB_AND_MAT_INDICES_STRIDE * MAX_BUFFERED_DRAW_COMMANDS);

#if USE_CIRCULAR_BUFFERS_FOR_MDI
			ob_and_mat_indices_ssbo->bind();
			ob_and_mat_indices_circ_buf.init(
				glMapBufferRange(GL_SHADER_STORAGE_BUFFER, /*offset=*/0, /*length=*/ob_and_mat_indices_ssbo->byteSize(), GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT),
				ob_and_mat_indices_ssbo->byteSize()
			);

			GLint alignment;
			glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &alignment);
			printVar(alignment);
#endif
		}


		buildPrograms(use_shader_dir);


		//terrain_system = new TerrainSystem();
		//terrain_system->init(*this, /*campos=*/Vec3d(0,0,2));

		if(settings.shadow_mapping)
		{
			shadow_mapping = new ShadowMapping();
			shadow_mapping->init(this);

			{
				clear_buf_overlay_ob =  new OverlayObject();
				clear_buf_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(0, 0, -0.9999f);
				clear_buf_overlay_ob->material.albedo_linear_rgb = Colour3f(1.f, 0.2f, 0.2f);
				clear_buf_overlay_ob->material.shader_prog = this->clear_prog;
				clear_buf_overlay_ob->mesh_data = this->unit_quad_meshdata;
			}

			if(false)
			{
				// Add overlay quads to preview depth maps, for debugging.  NOTE: doesn't really work with PCF texture types.

				{
					OverlayObjectRef tex_preview_overlay_ob =  new OverlayObject();

					tex_preview_overlay_ob->ob_to_world_matrix = Matrix4f::uniformScaleMatrix(0.95f) * Matrix4f::translationMatrix(-1, 0, 0);

					tex_preview_overlay_ob->material.albedo_linear_rgb = Colour3f(1.f);
					tex_preview_overlay_ob->material.shader_prog = this->overlay_prog;

					tex_preview_overlay_ob->material.albedo_texture = shadow_mapping->dynamic_depth_tex; // static_depth_tex[0];

					tex_preview_overlay_ob->mesh_data = this->unit_quad_meshdata;

					addOverlayObject(tex_preview_overlay_ob);
				}
				{
					OverlayObjectRef tex_preview_overlay_ob =  new OverlayObject();

					tex_preview_overlay_ob->ob_to_world_matrix = Matrix4f::uniformScaleMatrix(0.95f) * Matrix4f::translationMatrix(0, 0, 0);

					tex_preview_overlay_ob->material.albedo_linear_rgb = Colour3f(1.f);
					tex_preview_overlay_ob->material.shader_prog = this->overlay_prog;

					tex_preview_overlay_ob->material.albedo_texture = shadow_mapping->static_depth_tex[1];

					tex_preview_overlay_ob->mesh_data = this->unit_quad_meshdata;

					addOverlayObject(tex_preview_overlay_ob);
				}
			}
		}

		// TEMP: preview prepass output (prepass_colour_copy_texture)
		if(false)
		{
			{
				OverlayObjectRef texture_debug_preview_overlay_ob =  new OverlayObject();
				texture_debug_preview_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(-0.95f, 0.4f, 0) * Matrix4f::uniformScaleMatrix(0.6f);
				texture_debug_preview_overlay_ob->mesh_data = this->unit_quad_meshdata;
				texture_debug_preview_overlay_ob->material.albedo_linear_rgb = Colour3f(1.f);
				texture_debug_preview_overlay_ob->material.shader_prog = this->overlay_prog;

				texture_debug_preview_overlay_obs.push_back(texture_debug_preview_overlay_ob);

				addOverlayObject(texture_debug_preview_overlay_ob);
			}

			{
				OverlayObjectRef texture_debug_preview_overlay_ob =  new OverlayObject();
				texture_debug_preview_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(-0.95f, -0.2f, 0) * Matrix4f::uniformScaleMatrix(0.6f);
				texture_debug_preview_overlay_ob->mesh_data = this->unit_quad_meshdata;
				texture_debug_preview_overlay_ob->material.albedo_linear_rgb = Colour3f(1.f);
				texture_debug_preview_overlay_ob->material.shader_prog = this->overlay_prog;

				texture_debug_preview_overlay_obs.push_back(texture_debug_preview_overlay_ob);

				addOverlayObject(texture_debug_preview_overlay_ob);
			}

			// Preview prepass_normal_copy_texture
			{
				OverlayObjectRef texture_debug_preview_overlay_ob =  new OverlayObject();
				texture_debug_preview_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(-0.3f, 0.4f, 0) * Matrix4f::uniformScaleMatrix(0.6f);
				texture_debug_preview_overlay_ob->mesh_data = this->unit_quad_meshdata;
				texture_debug_preview_overlay_ob->material.albedo_linear_rgb = Colour3f(1.f);
				texture_debug_preview_overlay_ob->material.shader_prog = this->overlay_prog;

				texture_debug_preview_overlay_obs.push_back(texture_debug_preview_overlay_ob);

				addOverlayObject(texture_debug_preview_overlay_ob);
			}

			{
				OverlayObjectRef texture_debug_preview_overlay_ob =  new OverlayObject();
				texture_debug_preview_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(0.35f, 0.4f, 0) * Matrix4f::uniformScaleMatrix(0.6f);
				texture_debug_preview_overlay_ob->mesh_data = this->unit_quad_meshdata;
				texture_debug_preview_overlay_ob->material.albedo_linear_rgb = Colour3f(1.f);
				texture_debug_preview_overlay_ob->material.shader_prog = this->overlay_prog;

				texture_debug_preview_overlay_obs.push_back(texture_debug_preview_overlay_ob);

				addOverlayObject(texture_debug_preview_overlay_ob);
			}

			// Create marker crosshair on second image to right
			{
				OverlayObjectRef texture_debug_preview_overlay_ob =  new OverlayObject();
				texture_debug_preview_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(-0.3f + 0.3f, 0.4f + 0.3f, -0.1f) * Matrix4f::uniformScaleMatrix(0.005f);
				texture_debug_preview_overlay_ob->mesh_data = this->unit_quad_meshdata;
				texture_debug_preview_overlay_ob->material.albedo_linear_rgb = Colour3f(1.f, 0.f, 0.f);
				texture_debug_preview_overlay_ob->material.shader_prog = this->overlay_prog;

				texture_debug_preview_overlay_obs.push_back(texture_debug_preview_overlay_ob);

				addOverlayObject(texture_debug_preview_overlay_ob);
			}

			//{
			//	OverlayObjectRef texture_debug_preview_overlay_ob =  new OverlayObject();
			//	texture_debug_preview_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(0.35f, -0.2f, 0) * Matrix4f::uniformScaleMatrix(0.6f);
			//	texture_debug_preview_overlay_ob->mesh_data = this->unit_quad_meshdata;
			//	texture_debug_preview_overlay_ob->material.albedo_linear_rgb = Colour3f(1.f);
			//	texture_debug_preview_overlay_ob->material.shader_prog = this->overlay_prog;
			//
			//	texture_debug_preview_overlay_obs.push_back(texture_debug_preview_overlay_ob);
			//
			//	addOverlayObject(texture_debug_preview_overlay_ob);
			//}
		}

		// Create crosshair marker
		if(false)
		{
			crosshair_overlay_ob =  new OverlayObject();
			crosshair_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(0, 0, 0) * Matrix4f::uniformScaleMatrix(0.005f) * Matrix4f::translationMatrix(-0.5f, -0.5f, 0);
			crosshair_overlay_ob->mesh_data = this->unit_quad_meshdata;
			crosshair_overlay_ob->material.albedo_linear_rgb = Colour3f(1.f, 0.f, 0.f);
			crosshair_overlay_ob->material.shader_prog = this->overlay_prog;

			addOverlayObject(crosshair_overlay_ob);
		}

		if(true)
		{
			// Create large tex overlay
			{
				large_debug_overlay_ob =  new OverlayObject();
				large_debug_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(-1.f, -1.f, 0) * Matrix4f::uniformScaleMatrix(2.f);
				large_debug_overlay_ob->mesh_data = this->unit_quad_meshdata;
				large_debug_overlay_ob->material.albedo_linear_rgb = Colour3f(1.f);
				large_debug_overlay_ob->material.shader_prog = this->overlay_prog;
				
				large_debug_overlay_ob->draw = false;

				addOverlayObject(large_debug_overlay_ob);
			}
		}


		// Outline stuff
		{
			outline_solid_framebuffer = new FrameBuffer();
			outline_edge_framebuffer  = new FrameBuffer();
	
			outline_edge_mat.albedo_linear_rgb = toLinearSRGB(Colour3f(0.9f, 0.2f, 0.2f));
			outline_edge_mat.shader_prog = this->overlay_prog;

			outline_quad_meshdata = this->unit_quad_meshdata;
		}


		// Change clip space z range from [-1, 1] to [0, 1], in order to improve z-buffer precision.
		// See https://nlguillemot.wordpress.com/2016/12/07/reversed-z-in-opengl/
		// and https://developer.nvidia.com/content/depth-precision-visualized
#if !defined(OSX)
		if(use_reverse_z)
			glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
#endif

#if !defined(EMSCRIPTEN) // WebGL doesn't have multisample toggling.
		if(settings.msaa_samples <= 1)
			glDisable(GL_MULTISAMPLE); // The initial value for GL_MULTISAMPLE is GL_TRUE.
#endif

#if BUILD_TESTS && !defined(EMSCRIPTEN)
		{
			std::vector<std::string> watch_dirs(1, data_dir);
			ContainerUtils::append(watch_dirs, this->additional_shader_dirs);
			thread_manager.addThread(new ShaderFileWatcherThread(watch_dirs, this));
		}
#endif

		init_succeeded = true;
	}
	catch(glare::Exception& e)
	{
		conPrint(e.what());
		this->initialisation_error_msg = e.what();
		init_succeeded = false;
	}

	TracyGpuContext
}


// Create OpenGL query objects if they have not already been created.
void OpenGLEngine::checkCreateProfilingQueries()
{
	assert(query_profiling_enabled);

	if(!this->dynamic_depth_draw_gpu_timer)
	{
		this->dynamic_depth_draw_gpu_timer = new Query();
		this->static_depth_draw_gpu_timer = new Query();
		this->draw_opaque_obs_gpu_timer = new Query();
		this->depth_pre_pass_gpu_timer = new Query();
		this->compute_ssao_gpu_timer = new Query();
		this->blur_ssao_gpu_timer = new Query();
		this->copy_prepass_buffers_gpu_timer = new Query();
		this->decal_copy_buffers_timer = new Query();
		this->draw_overlays_gpu_timer = new Query();

#if EMSCRIPTEN
		// Chrome/web doesn't support timestamp queries: https://codereview.chromium.org/1800383002
		this->buffered_total_timer = new BufferedTimeElapsedQuery();
#else
		this->start_query = new TimestampQuery();
		this->end_query   = new TimestampQuery();
#endif
	}
}


void OpenGLEngine::startAsyncLoadingData(AsyncTextureLoader* async_texture_loader_)
{
	ZoneScoped; // Tracy profiler

	async_texture_loader = async_texture_loader_;

	// Load water caustic textures
	if(settings.render_water_caustics)
	{
		//Timer timer;
		
		water_caustics_textures.resize(32);
		for(int i=0; i<32; ++i)
		{
			const std::string filename = "save." + ::leftPad(toString(1 + i), '0', 2) + ".basis";
			loading_handles.push_back(async_texture_loader->startLoadingTexture(/*local path=*/"/gl_data/caustics/" + filename, /*handler=*/this, TextureParams())); // TODO: hold onto result loading handle and cancel
		}
		
		//conPrint("Load caustics took " + timer.elapsedString());
	}
}


void OpenGLEngine::textureLoaded(Reference<OpenGLTexture> texture, const std::string& local_filename)
{
	// conPrint("OpenGLEngine::textureLoaded: " + local_filename);

	if(hasPrefix(local_filename, "/gl_data/caustics/save."))
	{
		const std::string index_str = local_filename.substr(std::string("/gl_data/caustics/save.").size(), 2); // TODO: do without alloc
		try
		{
			const int index = stringToInt(index_str) - 1; // parse, convert to 0-based index
			runtimeCheck(index >= 0 && index < (int)water_caustics_textures.size());
			water_caustics_textures[index] = texture;
		}
		catch(glare::Exception& e)
		{
			conPrint("Error parsing caustic tex index: " + e.what());
		}
	}
	else
	{
		assert(0);
		conPrint("OpenGLEngine::textureLoaded(): Unknown texture: " + local_filename);
	}
}


static std::string preprocessorDefsForKey(const ProgramKey& key)
{
	return 
		"#define ALPHA_TEST " + toString(key.alpha_test) + "\n" +
		"#define VERT_COLOURS " + toString(key.vert_colours) + "\n" +
		"#define INSTANCE_MATRICES " + toString(key.instance_matrices) + "\n" +
		"#define LIGHTMAPPING " + toString(key.lightmapping) + "\n" +
		"#define GENERATE_PLANAR_UVS " + toString(key.gen_planar_uvs) + "\n" +
		"#define DRAW_PLANAR_UV_GRID " + toString(key.draw_planar_uv_grid) + "\n" +
		"#define SKINNING " + toString(key.skinning) + "\n" +
		"#define IMPOSTER " + toString(key.imposter) + "\n" +
		"#define IMPOSTERABLE " + toString(key.imposterable) + "\n" +
		"#define USE_WIND_VERT_SHADER " + toString(key.use_wind_vert_shader) + "\n" + 
		"#define FANCY_DOUBLE_SIDED " + toString(key.fancy_double_sided) + "\n" + 
		"#define MATERIALISE_EFFECT " + toString(key.materialise_effect) + "\n" + 
		"#define BLOB_SHADOWS " + toString(1) + "\n" +
		"#define TERRAIN " + toString(key.terrain) + "\n" + 
		"#define DECAL " + toString(key.decal) + "\n" +
		"#define PARTICIPATING_MEDIA " + toString(key.participating_media) + "\n" +
		"#define VERT_TANGENTS " + toString(key.vert_tangents) + "\n" + 
		"#define SDF_TEXT " + toString(key.sdf_text) + "\n" +
		"#define COMBINED " + toString(key.combined) + "\n" +
		"#define POSITION_W_IS_OCT16_NORMAL " + toString(key.position_w_is_oct16_normal) + "\n";
}


void OpenGLEngine::buildPrograms(const std::string& use_shader_dir)
{
	ZoneScoped; // Tracy profiler

	this->vert_utils_glsl = FileUtils::readEntireFileTextMode(use_shader_dir + "/vert_utils.glsl");
	this->frag_utils_glsl = FileUtils::readEntireFileTextMode(use_shader_dir + "/frag_utils.glsl");

	this->preprocessor_defines_with_common_vert_structs = preprocessor_defines;
	preprocessor_defines_with_common_vert_structs += FileUtils::readEntireFileTextMode(use_shader_dir + "/common_vert_structures.glsl");
	preprocessor_defines_with_common_vert_structs += vert_utils_glsl;

	this->preprocessor_defines_with_common_frag_structs = preprocessor_defines;
	preprocessor_defines_with_common_frag_structs += FileUtils::readEntireFileTextMode(use_shader_dir + "/common_frag_structures.glsl");
	preprocessor_defines_with_common_frag_structs += frag_utils_glsl;

	//------------------------------------------- Build fallback progs -------------------------------------------
	// Will be used if we hit a shader compilation error later
	fallback_phong_prog       = getPhongProgram      (ProgramKey(ProgramKey::ProgramName_phong, ProgramKeyArgs()));
	fallback_transparent_prog = getTransparentProgram(ProgramKey(ProgramKey::ProgramName_transparent, ProgramKeyArgs()));

	//------------------------------------------- Build depth-draw prog -------------------------------------------
	if(settings.shadow_mapping)
		fallback_depth_prog       = getDepthDrawProgram  (ProgramKey(ProgramKey::ProgramName_depth, ProgramKeyArgs()));

	//------------------------------------------- Build env prog -------------------------------------------
	this->env_prog = buildEnvProgram(use_shader_dir);

	//------------------------------------------- Build overlay prog -------------------------------------------
	{
		const std::string key_defs = preprocessorDefsForKey(ProgramKey(ProgramKey::ProgramName_overlay, ProgramKeyArgs())); // Needed to define MATERIALISE_EFFECT to 0 etc.
		overlay_prog = new OpenGLProgram(
			"overlay",
			new OpenGLShader(use_shader_dir + "/overlay_vert_shader.glsl", version_directive, key_defs + preprocessor_defines, GL_VERTEX_SHADER),
			new OpenGLShader(use_shader_dir + "/overlay_frag_shader.glsl", version_directive, key_defs + preprocessor_defines + frag_utils_glsl, GL_FRAGMENT_SHADER),
			getAndIncrNextProgramIndex(),
			/*wait for build to complete=*/true
		);
		addProgram(overlay_prog);

		overlay_diffuse_colour_location		= overlay_prog->getUniformLocation("diffuse_colour");
		overlay_flags_location				= overlay_prog->getUniformLocation("overlay_flags");
		overlay_diffuse_tex_location		= overlay_prog->getUniformLocation("diffuse_tex");
		overlay_texture_matrix_location		= overlay_prog->getUniformLocation("texture_matrix");
		overlay_clip_min_coords_location	= overlay_prog->getUniformLocation("clip_min_coords");
		overlay_clip_max_coords_location	= overlay_prog->getUniformLocation("clip_max_coords");
	}

	//------------------------------------------- Build clear prog -------------------------------------------
	clear_prog = new OpenGLProgram(
		"clear",
		new OpenGLShader(use_shader_dir + "/clear_vert_shader.glsl", version_directive, preprocessor_defines, GL_VERTEX_SHADER),
		new OpenGLShader(use_shader_dir + "/clear_frag_shader.glsl", version_directive, preprocessor_defines, GL_FRAGMENT_SHADER),
		getAndIncrNextProgramIndex(),
		/*wait for build to complete=*/true
	);
	addProgram(clear_prog);
		
	//------------------------------------------- Build outline (no skinning) prog -------------------------------------------
	{
		outline_prog_no_skinning = new OpenGLProgram(
			"outline_prog_no_skinning",
			new OpenGLShader(use_shader_dir + "/outline_vert_shader.glsl", version_directive, preprocessor_defines_with_common_vert_structs + "#define SKINNING 0\n", GL_VERTEX_SHADER),
			new OpenGLShader(use_shader_dir + "/outline_frag_shader.glsl", version_directive, preprocessor_defines                          + "#define SKINNING 0\n", GL_FRAGMENT_SHADER),
			getAndIncrNextProgramIndex(),
			/*wait for build to complete=*/true
		);
		addProgram(outline_prog_no_skinning);
		getUniformLocations(outline_prog_no_skinning); // Make sure any unused uniforms have their locations set to -1.

		bindUniformBlockToProgram(outline_prog_no_skinning, "SharedVertUniforms",		SHARED_VERT_UBO_BINDING_POINT_INDEX);
		bindUniformBlockToProgram(outline_prog_no_skinning, "PerObjectVertUniforms",	PER_OBJECT_VERT_UBO_BINDING_POINT_INDEX);

		outline_prog_no_skinning->is_outline = true;
		outline_prog_no_skinning->uses_vert_uniform_buf_obs = true;
	}

	//------------------------------------------- Build outline (with skinning) prog -------------------------------------------
	{
		const std::string use_preprocessor_defines_vert = preprocessor_defines_with_common_vert_structs + "#define SKINNING 1\n";
		const std::string use_preprocessor_defines_frag = preprocessor_defines                          + "#define SKINNING 1\n";
		outline_prog_with_skinning = new OpenGLProgram(
			"outline_prog_with_skinning",
			new OpenGLShader(use_shader_dir + "/outline_vert_shader.glsl", version_directive, use_preprocessor_defines_vert, GL_VERTEX_SHADER),
			new OpenGLShader(use_shader_dir + "/outline_frag_shader.glsl", version_directive, use_preprocessor_defines_frag, GL_FRAGMENT_SHADER),
			getAndIncrNextProgramIndex(),
			/*wait for build to complete=*/true
		);
		addProgram(outline_prog_with_skinning);
		getUniformLocations(outline_prog_with_skinning); // Make sure any unused uniforms have their locations set to -1.

		bindUniformBlockToProgram(outline_prog_with_skinning, "SharedVertUniforms",		SHARED_VERT_UBO_BINDING_POINT_INDEX);
		bindUniformBlockToProgram(outline_prog_with_skinning, "PerObjectVertUniforms",	PER_OBJECT_VERT_UBO_BINDING_POINT_INDEX);
		bindUniformBlockToProgram(outline_prog_with_skinning, "JointMatrixUniforms",	JOINT_MATRICES_UBO_BINDING_POINT_INDEX);

		outline_prog_with_skinning->is_outline = true;
		outline_prog_with_skinning->uses_skinning = true;
		outline_prog_with_skinning->uses_vert_uniform_buf_obs = true;
	}

	//------------------------------------------- Build edge extract prog -------------------------------------------
	edge_extract_prog = new OpenGLProgram(
		"edge_extract",
		new OpenGLShader(use_shader_dir + "/edge_extract_vert_shader.glsl", version_directive, preprocessor_defines, GL_VERTEX_SHADER),
		new OpenGLShader(use_shader_dir + "/edge_extract_frag_shader.glsl", version_directive, preprocessor_defines, GL_FRAGMENT_SHADER),
		getAndIncrNextProgramIndex(),
		/*wait for build to complete=*/true
	);
	addProgram(edge_extract_prog);
	getUniformLocations(edge_extract_prog); // Make sure any unused uniforms have their locations set to -1.
	edge_extract_tex_location			= edge_extract_prog->getUniformLocation("tex");
	edge_extract_col_location			= edge_extract_prog->getUniformLocation("col");
	edge_extract_line_width_location	= edge_extract_prog->getUniformLocation("line_width");

	//------------------------------------------- Build compute-SSAO prog -------------------------------------------
	if(settings.ssao)
	{
		compute_ssao_prog = buildComputeSSAOProg(use_shader_dir);
		blur_ssao_prog = buildBlurSSAOProg(use_shader_dir);
	}
	

	if(settings.render_to_offscreen_renderbuffers)
	{
		//------------------------------------------- Build downsize prog -------------------------------------------
		{
			const std::string use_preprocessor_defines = preprocessor_defines + "#define DOWNSIZE_FROM_MAIN_BUF 0\n";
			downsize_prog = new OpenGLProgram(
				"downsize",
				new OpenGLShader(use_shader_dir + "/downsize_vert_shader.glsl", version_directive, use_preprocessor_defines  , GL_VERTEX_SHADER),
				new OpenGLShader(use_shader_dir + "/downsize_frag_shader.glsl", version_directive, use_preprocessor_defines, GL_FRAGMENT_SHADER),
				getAndIncrNextProgramIndex(),
				/*wait for build to complete=*/true
			);
			addProgram(downsize_prog);
		}
			
		//------------------------------------------- Build downsize_from_main_buf_prog -------------------------------------------
		{
			const std::string use_preprocessor_defines = preprocessor_defines + "#define DOWNSIZE_FROM_MAIN_BUF 1\n";
			downsize_from_main_buf_prog = new OpenGLProgram(
				"downsize_from_main_buf",
				new OpenGLShader(use_shader_dir + "/downsize_vert_shader.glsl", version_directive, use_preprocessor_defines, GL_VERTEX_SHADER),
				new OpenGLShader(use_shader_dir + "/downsize_frag_shader.glsl", version_directive, use_preprocessor_defines, GL_FRAGMENT_SHADER),
				getAndIncrNextProgramIndex(),
				/*wait for build to complete=*/true
			);
			addProgram(downsize_from_main_buf_prog);

			//if(use_order_indep_transparency)
			//{
			//	downsize_from_main_buf_prog->appendUserUniformInfo(UserUniformInfo::UniformType_Sampler2D, "transparent_accum_texture");
			//	assert(downsize_from_main_buf_prog->user_uniform_info.back().index == DOWNSIZE_TRANSPARENT_ACCUM_TEX_UNIFORM_INDEX);
			//	assert(downsize_from_main_buf_prog->user_uniform_info.back().loc >= 0);
			//
			//	downsize_from_main_buf_prog->appendUserUniformInfo(UserUniformInfo::UniformType_Sampler2D, "total_transmittance_texture");
			//	assert(downsize_from_main_buf_prog->user_uniform_info.back().index == DOWNSIZE_TOTAL_TRANSMITTANCE_TEX_UNIFORM_INDEX);
			//	assert(downsize_from_main_buf_prog->user_uniform_info.back().loc >= 0);
			//}
		}

		//------------------------------------------- Build gaussian_blur_prog -------------------------------------------
		gaussian_blur_prog = new OpenGLProgram(
			"gaussian_blur",
			new OpenGLShader(use_shader_dir + "/gaussian_blur_vert_shader.glsl", version_directive, preprocessor_defines, GL_VERTEX_SHADER),
			new OpenGLShader(use_shader_dir + "/gaussian_blur_frag_shader.glsl", version_directive, preprocessor_defines, GL_FRAGMENT_SHADER),
			getAndIncrNextProgramIndex(),
			/*wait for build to complete=*/true
		);
		addProgram(gaussian_blur_prog);
		gaussian_blur_prog->appendUserUniformInfo(UserUniformInfo::UniformType_Int, "x_blur");
		
		//------------------------------------------- Build final_imaging_prog -------------------------------------------
		final_imaging_prog = buildFinalImagingProg(use_shader_dir);
		
		//------------------------------------------- Build OIT_composite_prog -------------------------------------------
		{
			const std::string key_defs = preprocessorDefsForKey(ProgramKey(ProgramKey::ProgramName_OIT_composite, ProgramKeyArgs())); // Needed to define MATERIALISE_EFFECT to 0 etc.
			OIT_composite_prog = new OpenGLProgram(
				"OIT_composite",
				new OpenGLShader(use_shader_dir + "/OIT_composite_vert_shader.glsl", version_directive, key_defs + preprocessor_defines, GL_VERTEX_SHADER),
				new OpenGLShader(use_shader_dir + "/OIT_composite_frag_shader.glsl", version_directive, key_defs + preprocessor_defines + frag_utils_glsl, GL_FRAGMENT_SHADER),
				getAndIncrNextProgramIndex(),
				/*wait for build to complete=*/true
			);
			addProgram(OIT_composite_prog);

			OIT_composite_prog->appendUserUniformInfo(UserUniformInfo::UniformType_Sampler2D, "transparent_accum_texture");
			assert(OIT_composite_prog->user_uniform_info.back().index == OIT_COMPOSITE_TRANSPARENT_ACCUM_TEX_UNIFORM_INDEX);
			if(use_order_indep_transparency)
				assert(OIT_composite_prog->user_uniform_info.back().loc >= 0);

			OIT_composite_prog->appendUserUniformInfo(UserUniformInfo::UniformType_Sampler2D, "total_transmittance_texture");
			assert(OIT_composite_prog->user_uniform_info.back().index == OIT_COMPOSITE_TOTAL_TRANSMITTANCE_TEX_UNIFORM_INDEX);
			if(use_order_indep_transparency)
				assert(OIT_composite_prog->user_uniform_info.back().loc >= 0);
		}

		//------------------------------------------- Build dof_blur_prog -------------------------------------------
		{
			const std::string key_defs = preprocessorDefsForKey(ProgramKey(ProgramKey::ProgramName_dof_blur, ProgramKeyArgs())); // Needed to define MATERIALISE_EFFECT to 0 etc.
			dof_blur_prog = new OpenGLProgram(
				"dof_blur",
				new OpenGLShader(use_shader_dir + "/dof_blur_vert_shader.glsl", version_directive, key_defs + preprocessor_defines, GL_VERTEX_SHADER),
				new OpenGLShader(use_shader_dir + "/dof_blur_frag_shader.glsl", version_directive, key_defs + preprocessor_defines_with_common_frag_structs, GL_FRAGMENT_SHADER),
				getAndIncrNextProgramIndex(),
				/*wait for build to complete=*/true
			);
			addProgram(dof_blur_prog);

			dof_blur_depth_tex_location = dof_blur_prog->getUniformLocation("depth_tex");
			assert(dof_blur_depth_tex_location >= 0);
			dof_blur_strength_location = dof_blur_prog->getUniformLocation("dof_blur_strength");
			assert(dof_blur_strength_location >= 0);
			dof_blur_focus_distance_location = dof_blur_prog->getUniformLocation("focus_distance");
			assert(dof_blur_focus_distance_location >= 0);

			bindUniformBlockToProgram(dof_blur_prog, "MaterialCommonUniforms",		MATERIAL_COMMON_UBO_BINDING_POINT_INDEX);
		}
	}

	//------------------------------------------- Build scatter prog for updating data on GPU -------------------------------------------
	if(use_scatter_shader)
	{
		data_updates_ssbo = new SSBO();
		data_updates_ssbo->allocate(sizeof(DataUpdateStruct) * 64 * 1024, /*map memory=*/false);


		OpenGLShaderRef scatter_data_compute_shader = new OpenGLShader(use_shader_dir + "/scatter_data_compute_shader.glsl", version_directive, preprocessor_defines_with_common_vert_structs, GL_COMPUTE_SHADER);
		scatter_data_prog = new OpenGLProgram("scatter data", scatter_data_compute_shader, NULL, getAndIncrNextProgramIndex(), /*wait for build to complete=*/true);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, /*binding point=*/DATA_UPDATES_SSBO_BINDING_POINT_INDEX, this->data_updates_ssbo->handle);

		bindShaderStorageBlockToProgram(scatter_data_prog, "DataUpdate", DATA_UPDATES_SSBO_BINDING_POINT_INDEX);
		bindShaderStorageBlockToProgram(scatter_data_prog, "PerObjectVertUniforms", PER_OB_VERT_DATA_SSBO_BINDING_POINT_INDEX);
	}

	//------------------------------------------- Build draw_aurora_tex_prog -------------------------------------------
	draw_aurora_tex_prog = buildAuroraProgram(use_shader_dir);

	OpenGLProgram::useNoPrograms();
}


// The per-object vert data SSBO is full.  Expand it.
void OpenGLEngine::expandPerObVertDataBuffer()
{
	const size_t cur_num_items = per_ob_vert_data_buffer->byteSize() / sizeof(PerObjectVertUniforms);
	const size_t new_num_items = cur_num_items * 2;
	
	SSBORef old_buf = per_ob_vert_data_buffer;
	per_ob_vert_data_buffer = new SSBO();
	per_ob_vert_data_buffer->allocate(sizeof(PerObjectVertUniforms) * new_num_items, /*map_memory=*/false);

	// conPrint("Expanded per_ob_vert_data_buffer size to: " + toString(new_num_items) + " items (" + toString(per_ob_vert_data_buffer->byteSize()) + " B)");

	// Do an on-GPU (hopefully) copy of the old buffer to the new buffer.
	glBindBuffer(GL_COPY_READ_BUFFER, old_buf->handle);
	glBindBuffer(GL_COPY_WRITE_BUFFER, per_ob_vert_data_buffer->handle);
	glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, /*readOffset=*/0, /*writeOffset=*/0, old_buf->byteSize());

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, /*binding point=*/PER_OB_VERT_DATA_SSBO_BINDING_POINT_INDEX, this->per_ob_vert_data_buffer->handle); // Bind new buffer to binding point

	for(size_t i=cur_num_items; i<new_num_items; ++i)
		per_ob_vert_data_free_indices.insert((int)i);
}


// The material data SSBO is full.  Expand it.
void OpenGLEngine::expandPhongBuffer()
{
	const size_t cur_num_items = phong_buffer->byteSize() / sizeof(PhongUniforms);
	const size_t new_num_items = cur_num_items * 2;
	
	SSBORef old_buf = phong_buffer;
	phong_buffer = new SSBO();
	phong_buffer->allocate(sizeof(PhongUniforms) * new_num_items, /*map_memory=*/false);

	// conPrint("Expanded phong_buffer size to: " + toString(new_num_items) + " items (" + toString(phong_buffer->byteSize()) + " B)");

	// Do an on-GPU copy of the old buffer to the new buffer.
	glBindBuffer(GL_COPY_READ_BUFFER, old_buf->handle);
	glBindBuffer(GL_COPY_WRITE_BUFFER, phong_buffer->handle);
	glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, /*readOffset=*/0, /*writeOffset=*/0, old_buf->byteSize());

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, /*binding point=*/PHONG_DATA_SSBO_BINDING_POINT_INDEX, this->phong_buffer->handle); // Bind new buffer to binding point

	for(size_t i=cur_num_items; i<new_num_items; ++i)
		phong_buffer_free_indices.insert((int)i);
}


// The joint matrices SSBO is full.  Expand it.
void OpenGLEngine::expandJointMatricesBuffer(size_t min_extra_needed)
{
	const size_t cur_num_items = joint_matrices_ssbo->byteSize() / sizeof(Matrix4f);
	const size_t new_num_items = myMax<size_t>(Maths::roundToNextHighestPowerOf2(cur_num_items + min_extra_needed), cur_num_items * 2);

	SSBORef old_buf = joint_matrices_ssbo;
	joint_matrices_ssbo = new SSBO();
	joint_matrices_ssbo->allocate(sizeof(Matrix4f) * new_num_items, /*map_memory=*/false);

	// conPrint("Expanded joint_matrices_ssbo size to: " + toString(new_num_items) + " items (" + toString(joint_matrices_ssbo->byteSize()) + " B)");

	// Do an on-GPU copy of the old buffer to the new buffer.
	glBindBuffer(GL_COPY_READ_BUFFER, old_buf->handle);
	glBindBuffer(GL_COPY_WRITE_BUFFER, joint_matrices_ssbo->handle);
	glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, /*readOffset=*/0, /*writeOffset=*/0, old_buf->byteSize());

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, /*binding point=*/JOINT_MATRICES_SSBO_BINDING_POINT_INDEX, this->joint_matrices_ssbo->handle); // Bind new buffer to binding point

	joint_matrices_allocator->expand(new_num_items);
}


static const bool PRINT_PROG_BUILD_TIMES = false;


// Called when a program that has been asynchronously compiling+linking has finished.
void OpenGLEngine::finishBuildingProg(OpenGLProgram* prog)
{
	// conPrint("================== Async building OpenGL program '" + prog->prog_name + "' took " + doubleToStringNDecimalPlaces(Clock::getCurTimeRealSec() - prog->build_start_time, 4) + " s ==================");

	prog->forceFinishLinkAndDoPostLinkCode();

	if(prog->prog_name == "phong")
		doPostBuildForPhongProgram(prog);
	else if(prog->prog_name == "transparent")
		doPostBuildForTransparentProgram(prog);
	else if(prog->prog_name == "depth")
		doPostBuildForDepthDrawProgram(prog);
	else
	{
		// User-defined custom program
	}


	// Iterate over all objects, if they are using this program, update their program flags, in particular PROGRAM_FINISHED_BUILDING_BITFLAG
	for(auto si = scenes.begin(); si != scenes.end(); ++si)
	{
		const OpenGLScene* scene = si->ptr();

		const GLObjectRef* const scene_obs = scene->objects.vector.data();
		const size_t scene_obs_size        = scene->objects.vector.size();
		for(size_t i=0; i<scene_obs_size; ++i)
		{
			// Prefetch cachelines containing the variables we need for objects N places ahead in the array.
			if(i + 16 < scene_obs_size)
				_mm_prefetch((const char*)(&scene_obs[i + 16]->materials), _MM_HINT_T0);

			GLObject* const ob = scene_obs[i].ptr();
			bool need_ob_rebuild = false;

			const size_t ob_materials_size           = ob->materials.size();
			const OpenGLMaterial* const ob_materials = ob->materials.data();
			for(uint32 z = 0; z < ob_materials_size; ++z)
				if(ob_materials[z].shader_prog.ptr() == prog || ob_materials[z].depth_draw_shader_prog.ptr() == prog)
				{
					need_ob_rebuild = true;
					break;
				}

			if(need_ob_rebuild)
			{
				rebuildObjectDepthDrawBatches(*ob);
				rebuildDenormalisedDrawData(*ob);
			}
		}
	}
}


OpenGLProgramRef OpenGLEngine::buildEnvProgram(const std::string& use_shader_dir)
{
	const std::string key_defs = preprocessorDefsForKey(ProgramKey(ProgramKey::ProgramName_env, ProgramKeyArgs()));

	OpenGLProgramRef new_env_prog = new OpenGLProgram(
		"env",
		new OpenGLShader(use_shader_dir + "/env_vert_shader.glsl", version_directive, key_defs + preprocessor_defines, GL_VERTEX_SHADER),
		new OpenGLShader(use_shader_dir + "/env_frag_shader.glsl", version_directive, key_defs + preprocessor_defines_with_common_frag_structs, GL_FRAGMENT_SHADER),
		getAndIncrNextProgramIndex(),
		/*wait for build to complete=*/true
	);
	addProgram(new_env_prog);

	getUniformLocations(new_env_prog);
	setStandardTextureUnitUniformsForProgram(*new_env_prog);

	env_diffuse_colour_location		= new_env_prog->getUniformLocation("diffuse_colour");
	env_have_texture_location		= new_env_prog->getUniformLocation("have_texture");
	env_texture_matrix_location		= new_env_prog->getUniformLocation("texture_matrix");
	env_campos_ws_location			= new_env_prog->getUniformLocation("env_campos_ws");

	bindUniformBlockToProgram(new_env_prog, "MaterialCommonUniforms",		MATERIAL_COMMON_UBO_BINDING_POINT_INDEX);

	return new_env_prog;
}


OpenGLProgramRef OpenGLEngine::buildAuroraProgram(const std::string& use_shader_dir)
{
	const std::string key_defs = preprocessorDefsForKey(ProgramKey(ProgramKey::ProgramName_draw_aurora_tex, ProgramKeyArgs()));

	OpenGLProgramRef prog = new OpenGLProgram(
		"draw_aurora_tex",
		new OpenGLShader(use_shader_dir + "/draw_aurora_tex_vert_shader.glsl", version_directive, key_defs + preprocessor_defines_with_common_vert_structs, GL_VERTEX_SHADER),
		new OpenGLShader(use_shader_dir + "/draw_aurora_tex_frag_shader.glsl", version_directive, key_defs + preprocessor_defines_with_common_frag_structs, GL_FRAGMENT_SHADER),
		getAndIncrNextProgramIndex(),
		/*wait for build to complete=*/true
	);
	addProgram(prog);

	getUniformLocations(prog);
	setStandardTextureUnitUniformsForProgram(*prog);

	checkUniformBlockSize(prog, "MaterialCommonUniforms",	sizeof(MaterialCommonUniforms));

	bindUniformBlockToProgram(prog, "MaterialCommonUniforms",		MATERIAL_COMMON_UBO_BINDING_POINT_INDEX);
	bindUniformBlockToProgram(prog, "SharedVertUniforms",			SHARED_VERT_UBO_BINDING_POINT_INDEX);

	return prog;
}


OpenGLProgramRef OpenGLEngine::buildComputeSSAOProg(const std::string& use_shader_dir)
{
	const std::string key_defs = preprocessorDefsForKey(ProgramKey(ProgramKey::ProgramName_compute_ssao, ProgramKeyArgs()));

	OpenGLProgramRef prog = new OpenGLProgram(
		"compute_ssao",
		new OpenGLShader(use_shader_dir + "/compute_ssao_vert_shader.glsl", version_directive, key_defs + preprocessor_defines_with_common_vert_structs, GL_VERTEX_SHADER),
		new OpenGLShader(use_shader_dir + "/compute_ssao_frag_shader.glsl", version_directive, key_defs + preprocessor_defines_with_common_frag_structs, GL_FRAGMENT_SHADER),
		getAndIncrNextProgramIndex(),
		/*wait for build to complete=*/true
	);
	addProgram(prog);
	getUniformLocations(prog); // Make sure any unused uniforms have their locations set to -1.
	setStandardTextureUnitUniformsForProgram(*prog);

	compute_ssao_normal_tex_location = prog->getUniformLocation("normal_tex");
	compute_ssao_depth_tex_location = prog->getUniformLocation("depth_tex");

	bindUniformBlockToProgram(prog, "MaterialCommonUniforms",		MATERIAL_COMMON_UBO_BINDING_POINT_INDEX);
	bindUniformBlockToProgram(prog, "SharedVertUniforms",			SHARED_VERT_UBO_BINDING_POINT_INDEX);

	return prog;
}


OpenGLProgramRef OpenGLEngine::buildBlurSSAOProg(const std::string& use_shader_dir)
{
	const std::string key_defs = preprocessorDefsForKey(ProgramKey(ProgramKey::ProgramName_blur_ssao, ProgramKeyArgs()));

	OpenGLProgramRef prog = new OpenGLProgram(
		"blur_ssao",
		new OpenGLShader(use_shader_dir + "/blur_ssao_vert_shader.glsl", version_directive, key_defs + preprocessor_defines_with_common_vert_structs, GL_VERTEX_SHADER),
		new OpenGLShader(use_shader_dir + "/blur_ssao_frag_shader.glsl", version_directive, key_defs + preprocessor_defines_with_common_frag_structs, GL_FRAGMENT_SHADER),
		getAndIncrNextProgramIndex(),
		/*wait for build to complete=*/true
	);
	getUniformLocations(prog);
	addProgram(prog);
	setStandardTextureUnitUniformsForProgram(*prog);

	bindUniformBlockToProgram(prog, "MaterialCommonUniforms",		MATERIAL_COMMON_UBO_BINDING_POINT_INDEX);
	bindUniformBlockToProgram(prog, "SharedVertUniforms",			SHARED_VERT_UBO_BINDING_POINT_INDEX);

	assert(prog->user_uniform_info.size() == 0);
	prog->appendUserUniformInfo(UserUniformInfo::UniformType_Int, "is_ssao_blur");
	prog->appendUserUniformInfo(UserUniformInfo::UniformType_Int, "blur_x");
	
	return prog;
}


OpenGLProgramRef OpenGLEngine::buildFinalImagingProg(const std::string& use_shader_dir)
{
	const std::string key_defs = preprocessorDefsForKey(ProgramKey(ProgramKey::ProgramName_final_imaging, ProgramKeyArgs())); // Needed to define MATERIALISE_EFFECT to 0 etc.
	OpenGLProgramRef prog = new OpenGLProgram(
		"final_imaging",
		new OpenGLShader(use_shader_dir + "/final_imaging_vert_shader.glsl", version_directive, key_defs + preprocessor_defines, GL_VERTEX_SHADER),
		new OpenGLShader(use_shader_dir + "/final_imaging_frag_shader.glsl", version_directive, key_defs + preprocessor_defines + frag_utils_glsl, GL_FRAGMENT_SHADER),
		getAndIncrNextProgramIndex(),
		/*wait for build to complete=*/true
	);
	addProgram(prog);
	prog->appendUserUniformInfo(UserUniformInfo::UniformType_Float, "bloom_strength");
	assert(prog->user_uniform_info.back().index == FINAL_IMAGING_BLOOM_STRENGTH_UNIFORM_INDEX);
	assert(prog->user_uniform_info.back().loc >= 0);

	prog->appendUserUniformInfo(UserUniformInfo::UniformType_Float, "exposure_factor");
	assert(prog->user_uniform_info.back().index == FINAL_IMAGING_EXPOSURE_FACTOR_UNIFORM_INDEX);
	assert(prog->user_uniform_info.back().loc >= 0);

	prog->appendUserUniformInfo(UserUniformInfo::UniformType_Float, "saturation_multiplier");
	assert(prog->user_uniform_info.back().index == FINAL_IMAGING_SATURATION_MULTIPLIER_UNIFORM_INDEX);
	assert(prog->user_uniform_info.back().loc >= 0);

	// Note that even if use_order_indep_transparency is false, we still need to create these user uniforms, because FINAL_IMAGING_BLUR_TEX_UNIFORM_START is hard-coded and refers to user uniforms past these.
	//prog->appendUserUniformInfo(UserUniformInfo::UniformType_Sampler2D, "transparent_accum_texture");
	//assert(prog->user_uniform_info.back().index == FINAL_IMAGING_TRANSPARENT_ACCUM_TEX_UNIFORM_INDEX);
	//if(use_order_indep_transparency)
	//	assert(prog->user_uniform_info.back().loc >= 0);
	//
	//prog->appendUserUniformInfo(UserUniformInfo::UniformType_Sampler2D, "total_transmittance_texture");
	//assert(prog->user_uniform_info.back().index == FINAL_IMAGING_TOTAL_TRANSMITTANCE_TEX_UNIFORM_INDEX);
	//if(use_order_indep_transparency)
	//	assert(prog->user_uniform_info.back().loc >= 0);

	for(int i=0; i<NUM_BLUR_DOWNSIZES; ++i)
	{
		prog->appendUserUniformInfo(UserUniformInfo::UniformType_Sampler2D, "blur_tex_" + toString(i));
		assert(prog->user_uniform_info.back().index == FINAL_IMAGING_BLUR_TEX_UNIFORM_START + i);
	}

	return prog;
}


OpenGLProgramRef OpenGLEngine::getPhongProgram(const ProgramKey& key) // Throws glare::Exception on shader compilation failure.
{
	if(progs[key] == NULL)
	{
		Timer timer;

		const std::string key_defs = preprocessorDefsForKey(key);
		const std::string use_vert_defs = key_defs + preprocessor_defines_with_common_vert_structs;
		const std::string use_frag_defs = key_defs + preprocessor_defines_with_common_frag_structs;
		const std::string use_shader_dir = data_dir + "/shaders";

		OpenGLProgramRef phong_prog = new OpenGLProgram(
			"phong",
			new OpenGLShader(use_shader_dir + "/phong_vert_shader.glsl", version_directive, use_vert_defs, GL_VERTEX_SHADER),
			new OpenGLShader(use_shader_dir + "/phong_frag_shader.glsl", version_directive, use_frag_defs, GL_FRAGMENT_SHADER),
			getAndIncrNextProgramIndex(),
			/*wait for build to complete=*/!parallel_shader_compile_support
		);
		addProgram(phong_prog);
		phong_prog->uses_phong_uniforms = true;
		phong_prog->uses_vert_uniform_buf_obs = true;
		phong_prog->supports_gpu_resident = true;
		phong_prog->uses_skinning = key.skinning;

		progs[key] = phong_prog;

		if(!parallel_shader_compile_support)
		{
			phong_prog->forceFinishLinkAndDoPostLinkCode();
			doPostBuildForPhongProgram(phong_prog);
		}

		if(PRINT_PROG_BUILD_TIMES) conPrint("Built phong program.  Elapsed: " + timer.elapsedStringMSWIthNSigFigs(3) + ", key " + key.description());
	}

	return progs[key];
}


void OpenGLEngine::doPostBuildForPhongProgram(OpenGLProgramRef phong_prog)
{
	getUniformLocations(phong_prog);
	setStandardTextureUnitUniformsForProgram(*phong_prog);

	if(!use_ob_and_mat_data_gpu_resident)
	{
		// Check we got the size of our uniform blocks on the CPU side correct.
		// printFieldOffsets(phong_prog, "PhongUniforms");
		checkUniformBlockSize(phong_prog, "PhongUniforms",				sizeof(PhongUniforms));
		checkUniformBlockSize(phong_prog, "PerObjectVertUniforms",		sizeof(PerObjectVertUniforms));

		bindUniformBlockToProgram(phong_prog, "PhongUniforms",			PHONG_UBO_BINDING_POINT_INDEX);
		bindUniformBlockToProgram(phong_prog, "PerObjectVertUniforms",	PER_OBJECT_VERT_UBO_BINDING_POINT_INDEX);

		if(phong_prog->uses_skinning)
			bindUniformBlockToProgram(phong_prog, "JointMatrixUniforms",	JOINT_MATRICES_UBO_BINDING_POINT_INDEX);
	}
	else
	{
		// Bind multi-draw-indirect SSBOs to program
		bindShaderStorageBlockToProgram(phong_prog, "PerObjectVertUniforms",	PER_OB_VERT_DATA_SSBO_BINDING_POINT_INDEX);
		bindShaderStorageBlockToProgram(phong_prog, "JointMatricesStorage",		JOINT_MATRICES_SSBO_BINDING_POINT_INDEX);
		bindShaderStorageBlockToProgram(phong_prog, "PhongUniforms",			PHONG_DATA_SSBO_BINDING_POINT_INDEX);
		if(use_multi_draw_indirect)
			bindShaderStorageBlockToProgram(phong_prog, "ObAndMatIndicesStorage",	OB_AND_MAT_INDICES_SSBO_BINDING_POINT_INDEX);
		else
			bindUniformBlockToProgram(phong_prog, "ObJointAndMatIndices",		OB_JOINT_AND_MAT_INDICES_UBO_BINDING_POINT_INDEX);
	}

	checkUniformBlockSize(phong_prog, "MaterialCommonUniforms",	sizeof(MaterialCommonUniforms));
	checkUniformBlockSize(phong_prog, "SharedVertUniforms",		sizeof(SharedVertUniforms));

	bindUniformBlockToProgram(phong_prog, "MaterialCommonUniforms",		MATERIAL_COMMON_UBO_BINDING_POINT_INDEX);
	bindUniformBlockToProgram(phong_prog, "SharedVertUniforms",			SHARED_VERT_UBO_BINDING_POINT_INDEX);
		
	if(light_buffer.nonNull())
		bindShaderStorageBlockToProgram(phong_prog, "LightDataStorage", LIGHT_DATA_SSBO_BINDING_POINT_INDEX);
	else
		// Bind "LightDataStorage" uniform block in the shader to the binding point with index LIGHT_DATA_UBO_BINDING_POINT_INDEX.
		bindUniformBlockToProgram(phong_prog, "LightDataStorage",		LIGHT_DATA_UBO_BINDING_POINT_INDEX);
}


OpenGLProgramRef OpenGLEngine::getTransparentProgram(const ProgramKey& key) // Throws glare::Exception on shader compilation failure.
{
	// TODO: use buildProgram() below

	if(progs[key] == NULL)
	{
		Timer timer;

		const std::string key_defs = preprocessorDefsForKey(key);
		const std::string use_vert_defs = key_defs + preprocessor_defines_with_common_vert_structs;
		const std::string use_frag_defs = key_defs + preprocessor_defines_with_common_frag_structs;
		const std::string use_shader_dir = data_dir + "/shaders";

		OpenGLProgramRef prog = new OpenGLProgram(
			"transparent",
			new OpenGLShader(use_shader_dir + "/transparent_vert_shader.glsl", version_directive, use_vert_defs, GL_VERTEX_SHADER),
			new OpenGLShader(use_shader_dir + "/transparent_frag_shader.glsl", version_directive, use_frag_defs, GL_FRAGMENT_SHADER),
			getAndIncrNextProgramIndex(),
			/*wait for build to complete=*/!parallel_shader_compile_support
		);
		addProgram(prog);
		prog->uses_phong_uniforms = true;
		prog->uses_vert_uniform_buf_obs = true;
		prog->supports_gpu_resident = true;
		prog->uses_skinning = key.skinning;

		progs[key] = prog;

		if(!parallel_shader_compile_support)
		{
			prog->forceFinishLinkAndDoPostLinkCode();
			doPostBuildForTransparentProgram(prog);
		}

		if(PRINT_PROG_BUILD_TIMES) conPrint("Built transparent program.  Elapsed: " + timer.elapsedStringMSWIthNSigFigs(3) + ", key " + key.description());
	}

	return progs[key];
}


void OpenGLEngine::doPostBuildForTransparentProgram(OpenGLProgramRef prog)
{
	getUniformLocations(prog);
	setStandardTextureUnitUniformsForProgram(*prog);

	// Check we got the size of our uniform blocks on the CPU side correct.
	if(!use_ob_and_mat_data_gpu_resident)
	{
		checkUniformBlockSize(prog, "PhongUniforms",				sizeof(PhongUniforms));
		checkUniformBlockSize(prog, "PerObjectVertUniforms", sizeof(PerObjectVertUniforms));

		bindUniformBlockToProgram(prog, "PhongUniforms",			PHONG_UBO_BINDING_POINT_INDEX);
		bindUniformBlockToProgram(prog, "PerObjectVertUniforms",	PER_OBJECT_VERT_UBO_BINDING_POINT_INDEX);

		if(prog->uses_skinning)
			bindUniformBlockToProgram(prog, "JointMatrixUniforms",	JOINT_MATRICES_UBO_BINDING_POINT_INDEX);
	}
	else
	{
		bindShaderStorageBlockToProgram(prog, "PerObjectVertUniforms",	PER_OB_VERT_DATA_SSBO_BINDING_POINT_INDEX);
		bindShaderStorageBlockToProgram(prog, "JointMatricesStorage",	JOINT_MATRICES_SSBO_BINDING_POINT_INDEX);
		bindShaderStorageBlockToProgram(prog, "PhongUniforms",			PHONG_DATA_SSBO_BINDING_POINT_INDEX);
		if(use_multi_draw_indirect)
			bindShaderStorageBlockToProgram(prog, "ObAndMatIndicesStorage",	OB_AND_MAT_INDICES_SSBO_BINDING_POINT_INDEX);
		else
			bindUniformBlockToProgram(prog, "ObJointAndMatIndices",		OB_JOINT_AND_MAT_INDICES_UBO_BINDING_POINT_INDEX);
	}

	bindUniformBlockToProgram(prog, "MaterialCommonUniforms",		MATERIAL_COMMON_UBO_BINDING_POINT_INDEX);
	bindUniformBlockToProgram(prog, "SharedVertUniforms",			SHARED_VERT_UBO_BINDING_POINT_INDEX);

	if(light_buffer.nonNull())
		bindShaderStorageBlockToProgram(prog, "LightDataStorage",	LIGHT_DATA_SSBO_BINDING_POINT_INDEX);
	else
		bindUniformBlockToProgram(prog, "LightDataStorage",			LIGHT_DATA_UBO_BINDING_POINT_INDEX);
}


void OpenGLEngine::addProgram(OpenGLProgramRef prog)
{
	if(prog->program_index >= prog_vector.size())
		prog_vector.resize(prog->program_index + 1);

	prog_vector[prog->program_index] = prog;

	if(!prog->built_successfully)
	{
		assert(!ContainerUtils::contains(building_progs, prog.ptr()));
		building_progs.push_back(prog.ptr());
	}
}


// shader_name_prefix should be something like "water" or "participating_media"
OpenGLProgramRef OpenGLEngine::buildProgram(const string_view shader_name_prefix, const ProgramKey& key) // Throws glare::Exception on shader compilation failure.
{
	if(progs[key] == NULL)
	{
		Timer timer;

		const std::string key_defs = preprocessorDefsForKey(key);
		const std::string use_vert_defs = key_defs + preprocessor_defines_with_common_vert_structs;
		const std::string use_frag_defs = key_defs + preprocessor_defines_with_common_frag_structs;
		const std::string use_shader_dir = data_dir + "/shaders";

		OpenGLProgramRef prog = new OpenGLProgram(
			toString(shader_name_prefix),
			new OpenGLShader(use_shader_dir + "/" + toString(shader_name_prefix) + "_vert_shader.glsl", version_directive, use_vert_defs, GL_VERTEX_SHADER),
			new OpenGLShader(use_shader_dir + "/" + toString(shader_name_prefix) + "_frag_shader.glsl", version_directive, use_frag_defs, GL_FRAGMENT_SHADER),
			getAndIncrNextProgramIndex(),
			/*wait for build to complete=*/true
		);
		addProgram(prog);
		prog->uses_phong_uniforms = true;
		prog->uses_vert_uniform_buf_obs = true;
		prog->supports_gpu_resident = true;

		progs[key] = prog;

		getUniformLocations(prog);
		setStandardTextureUnitUniformsForProgram(*prog);

		// Check we got the size of our uniform blocks on the CPU side correct.
		if(!use_ob_and_mat_data_gpu_resident)
		{
			checkUniformBlockSize(prog, "PhongUniforms",				sizeof(PhongUniforms));
			checkUniformBlockSize(prog, "PerObjectVertUniforms", sizeof(PerObjectVertUniforms));

			bindUniformBlockToProgram(prog, "PhongUniforms",			PHONG_UBO_BINDING_POINT_INDEX);
			bindUniformBlockToProgram(prog, "PerObjectVertUniforms",	PER_OBJECT_VERT_UBO_BINDING_POINT_INDEX);
		}
		else
		{
			bindShaderStorageBlockToProgram(prog, "PerObjectVertUniforms",	PER_OB_VERT_DATA_SSBO_BINDING_POINT_INDEX);
			//bindShaderStorageBlockToProgram(prog, "JointMatricesStorage",	JOINT_MATRICES_SSBO_BINDING_POINT_INDEX);
			bindShaderStorageBlockToProgram(prog, "PhongUniforms",			PHONG_DATA_SSBO_BINDING_POINT_INDEX);
			if(use_multi_draw_indirect)
				bindShaderStorageBlockToProgram(prog, "ObAndMatIndicesStorage",	OB_AND_MAT_INDICES_SSBO_BINDING_POINT_INDEX);
			else
				bindUniformBlockToProgram(prog, "ObJointAndMatIndices",		OB_JOINT_AND_MAT_INDICES_UBO_BINDING_POINT_INDEX);
		}

		bindUniformBlockToProgram(prog, "MaterialCommonUniforms",		MATERIAL_COMMON_UBO_BINDING_POINT_INDEX);
		bindUniformBlockToProgram(prog, "SharedVertUniforms",			SHARED_VERT_UBO_BINDING_POINT_INDEX);

		//if(light_buffer.nonNull())
		//	bindShaderStorageBlockToProgram(prog, "LightDataStorage", LIGHT_DATA_SSBO_BINDING_POINT_INDEX);
		//else
		//	bindUniformBlockToProgram(prog, "LightDataStorage",		LIGHT_DATA_UBO_BINDING_POINT_INDEX);

		if(PRINT_PROG_BUILD_TIMES) conPrint("Built '" + toString(shader_name_prefix) + "' program.  Elapsed: " + timer.elapsedStringMSWIthNSigFigs(3) + ", key " + key.description());
	}

	return progs[key];
}


OpenGLProgramRef OpenGLEngine::getImposterProgram(const ProgramKey& key) // Throws glare::Exception on shader compilation failure.
{
	if(progs[key] == NULL)
	{
		Timer timer;

		const std::string key_defs = preprocessorDefsForKey(key);
		const std::string use_vert_defs = key_defs + preprocessor_defines_with_common_vert_structs;
		const std::string use_frag_defs = key_defs + preprocessor_defines_with_common_frag_structs;
		const std::string use_shader_dir = data_dir + "/shaders";

		OpenGLProgramRef prog = new OpenGLProgram(
			"imposter",
			new OpenGLShader(use_shader_dir + "/imposter_vert_shader.glsl", version_directive, use_vert_defs, GL_VERTEX_SHADER),
			new OpenGLShader(use_shader_dir + "/imposter_frag_shader.glsl", version_directive, use_frag_defs, GL_FRAGMENT_SHADER),
			getAndIncrNextProgramIndex(),
			/*wait for build to complete=*/true
		);
		addProgram(prog);
		prog->uses_phong_uniforms = true;
		prog->uses_vert_uniform_buf_obs = true;

		progs[key] = prog;

		getUniformLocations(prog);
		setStandardTextureUnitUniformsForProgram(*prog);

		bindUniformBlockToProgram(prog, "PhongUniforms",				PHONG_UBO_BINDING_POINT_INDEX);

		bindUniformBlockToProgram(prog, "MaterialCommonUniforms",		MATERIAL_COMMON_UBO_BINDING_POINT_INDEX);
		bindUniformBlockToProgram(prog, "SharedVertUniforms",			SHARED_VERT_UBO_BINDING_POINT_INDEX);
		bindUniformBlockToProgram(prog, "PerObjectVertUniforms",		PER_OBJECT_VERT_UBO_BINDING_POINT_INDEX);

		if(PRINT_PROG_BUILD_TIMES) conPrint("Built imposter program.  Elapsed: " + timer.elapsedStringMSWIthNSigFigs(3) + ", key " + key.description());
	}

	return progs[key];
}


OpenGLProgramRef OpenGLEngine::getDepthDrawProgram(const ProgramKey& key_) // Throws glare::Exception on shader compilation failure.
{
	// Only some fields are relevant for picking the correct depth draw prog.  Set the other options to false to avoid unnecessary state changes.
	ProgramKey key = key_;
	
	if(!key_.use_wind_vert_shader)
		key.vert_colours = false; // Vert colours will only be used for wind
	key.vert_tangents = false;
	key.lightmapping = false;
	key.gen_planar_uvs = false; // relevant?
	key.draw_planar_uv_grid = false;
	// relevant: key.skinning
	// relevant: use_wind_vert_shader
	key.fancy_double_sided = false; // for now
	key.terrain = false;
//	key.decal = false;
//	key.participating_media = false;
//	key.vert_tangents = false;
	// relevant: sdf_text
//	key.fancy_double_sided = false;
//	key.combined = false;

	key.rebuildKeyVal();


	if(progs[key] == NULL)
	{
		Timer timer;

		const std::string key_defs = preprocessorDefsForKey(key);
		const std::string use_vert_defs = key_defs + preprocessor_defines_with_common_vert_structs;
		const std::string use_frag_defs = key_defs + preprocessor_defines_with_common_frag_structs;
		const std::string use_shader_dir = data_dir + "/shaders";

		OpenGLProgramRef prog = new OpenGLProgram(
			"depth",
			new OpenGLShader(use_shader_dir + "/depth_vert_shader.glsl", version_directive, use_vert_defs, GL_VERTEX_SHADER),
			new OpenGLShader(use_shader_dir + "/depth_frag_shader.glsl", version_directive, use_frag_defs, GL_FRAGMENT_SHADER),
			getAndIncrNextProgramIndex(),
			/*wait for build to complete=*/!parallel_shader_compile_support
		);
		addProgram(prog);
		prog->is_depth_draw = true;
		prog->is_depth_draw_with_alpha_test = key.alpha_test;
		prog->uses_vert_uniform_buf_obs = true;
		prog->supports_gpu_resident = true;
		prog->uses_skinning = key.skinning;

		progs[key] = prog;

		if(!parallel_shader_compile_support)
		{
			prog->forceFinishLinkAndDoPostLinkCode();
			doPostBuildForDepthDrawProgram(prog);
		}

		if(PRINT_PROG_BUILD_TIMES) conPrint("Built depth-draw program.  Elapsed: " + timer.elapsedStringMSWIthNSigFigs(3) + ", key " + key.description());
	}

	return progs[key];
}


void OpenGLEngine::doPostBuildForDepthDrawProgram(OpenGLProgramRef prog)
{
	getUniformLocations(prog);
	setStandardTextureUnitUniformsForProgram(*prog);

	bindUniformBlockToProgram(prog, "MaterialCommonUniforms",		MATERIAL_COMMON_UBO_BINDING_POINT_INDEX);
	bindUniformBlockToProgram(prog, "SharedVertUniforms",			SHARED_VERT_UBO_BINDING_POINT_INDEX);

	if(!use_ob_and_mat_data_gpu_resident)
	{
		checkUniformBlockSize(prog, "DepthUniforms",				sizeof(DepthUniforms)); // Check we got the size of our uniform blocks on the CPU side correct.

		bindUniformBlockToProgram(prog, "DepthUniforms",			DEPTH_UNIFORM_UBO_BINDING_POINT_INDEX);
		bindUniformBlockToProgram(prog, "PerObjectVertUniforms",	PER_OBJECT_VERT_UBO_BINDING_POINT_INDEX);

		if(prog->uses_skinning)
			bindUniformBlockToProgram(prog, "JointMatrixUniforms",	JOINT_MATRICES_UBO_BINDING_POINT_INDEX);
	}
	else
	{
		bindShaderStorageBlockToProgram(prog, "PerObjectVertUniforms",	PER_OB_VERT_DATA_SSBO_BINDING_POINT_INDEX);
		bindShaderStorageBlockToProgram(prog, "JointMatricesStorage",	JOINT_MATRICES_SSBO_BINDING_POINT_INDEX);
		bindShaderStorageBlockToProgram(prog, "PhongUniforms",			PHONG_DATA_SSBO_BINDING_POINT_INDEX);
		if(use_multi_draw_indirect)
			bindShaderStorageBlockToProgram(prog, "ObAndMatIndicesStorage",	OB_AND_MAT_INDICES_SSBO_BINDING_POINT_INDEX);
		else
			bindUniformBlockToProgram(prog, "ObJointAndMatIndices",		OB_JOINT_AND_MAT_INDICES_UBO_BINDING_POINT_INDEX);
	}
}


OpenGLProgramRef OpenGLEngine::getProgramWithFallbackOnError(const ProgramKey& key) // On shader compilation failure, just returns a default phong program.
{
	try
	{
		switch(key.program_name)
		{
			case ProgramKey::ProgramName_phong:
				return getPhongProgram(key);
			case ProgramKey::ProgramName_transparent:
				return getTransparentProgram(key);
			case ProgramKey::ProgramName_imposter:
				return getImposterProgram(key);
			case ProgramKey::ProgramName_water:
				return buildProgram("water", key);
			case ProgramKey::ProgramName_participatingMedia:
				return buildProgram("participating_media", key);
			case ProgramKey::ProgramName_depth:
				return getDepthDrawProgram(key);
			default:
			{
				assert(0);
				throw glare::Exception("getProgramWithFallbackOnError(): unhandled program name " + toString(key.program_name) + ".");
			}
		}
	}
	catch(glare::Exception& e)
	{
		conPrint("ERROR: " + e.what());
		return this->fallback_phong_prog;
	}
}


OpenGLProgramRef OpenGLEngine::getDepthDrawProgramWithFallbackOnError(const ProgramKey& key) // On shader compilation failure, just returns a default phong program.
{
	try
	{
		return getDepthDrawProgram(key);
	}
	catch(glare::Exception& e)
	{
		conPrint("ERROR: " + e.what());
		return this->fallback_depth_prog;
	}
}



// We want the outline textures to have the same resolution as the viewport.
void OpenGLEngine::buildOutlineTextures()
{
	outline_tex_w = myMax(16, viewport_w);
	outline_tex_h = myMax(16, viewport_h);

	 // conPrint("buildOutlineTextures(), size: " + toString(outline_tex_w) + " x " + toString(outline_tex_h));

	js::Vector<uint8, 16> buf(outline_tex_w * outline_tex_h * 4, 0); // large enough for RGBA below

	outline_solid_tex = new OpenGLTexture(outline_tex_w, outline_tex_h, this,
		buf,
		OpenGLTextureFormat::Format_RGB_Linear_Uint8,
		OpenGLTexture::Filtering_Bilinear,
		OpenGLTexture::Wrapping_Clamp // Clamp texture reads otherwise edge outlines will wrap around to other side of frame.
	);

	outline_edge_tex = new OpenGLTexture(outline_tex_w, outline_tex_h, this,
		buf,
		OpenGLTextureFormat::Format_RGBA_Linear_Uint8,
		OpenGLTexture::Filtering_Bilinear
	);

	outline_edge_mat.albedo_texture = outline_edge_tex;

	outline_solid_framebuffer->attachTexture(*outline_solid_tex, GL_COLOR_ATTACHMENT0);
	outline_edge_framebuffer->attachTexture(*outline_edge_tex, GL_COLOR_ATTACHMENT0);

	if(false)
	{
		current_scene->overlay_objects.clear();

		// TEMP: Add overlay quad to preview texture
		Reference<OverlayObject> preview_overlay_ob = new OverlayObject();
		preview_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(-1.0, 0, -0.999f);
		preview_overlay_ob->material.shader_prog = this->overlay_prog;
		preview_overlay_ob->material.albedo_texture = outline_solid_tex;
		preview_overlay_ob->mesh_data = this->unit_quad_meshdata;
		addOverlayObject(preview_overlay_ob);

		preview_overlay_ob = new OverlayObject();
		preview_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(0.0,0,-0.999f);
		preview_overlay_ob->material.shader_prog = this->overlay_prog;
		preview_overlay_ob->material.albedo_texture = outline_edge_tex;
		preview_overlay_ob->mesh_data = this->unit_quad_meshdata;
		addOverlayObject(preview_overlay_ob);
	}
}


void OpenGLEngine::createSSAOTextures()
{
	const int xres = myMax(16, main_viewport_w);
	const int yres = myMax(16, main_viewport_h);

	const int prepass_xres = xres / 2;
	const int prepass_yres = yres / 2;
	const int prepass_msaa_samples = 1;


	conPrint("Allocating SSAO buffers and textures with width " + toString(prepass_xres) + " and height " + toString(prepass_yres));

#if 0
	prepass_colour_renderbuffer = new RenderBuffer(prepass_xres, prepass_yres, prepass_msaa_samples, col_buffer_format);
	prepass_depth_renderbuffer  = new RenderBuffer(prepass_xres, prepass_yres, prepass_msaa_samples, depth_format);
	prepass_framebuffer = new FrameBuffer();
	prepass_framebuffer->attachRenderBuffer(*prepass_colour_renderbuffer, GL_COLOR_ATTACHMENT0);
	prepass_framebuffer->attachRenderBuffer(*prepass_normal_renderbuffer, GL_COLOR_ATTACHMENT1);
	prepass_framebuffer->attachRenderBuffer(*prepass_depth_renderbuffer, GL_DEPTH_ATTACHMENT);
#else
	// We will store roughness in colour w.
	const OpenGLTextureFormat prepass_col_buffer_format = OpenGLTextureFormat::Format_RGBA_Linear_Half;

	const OpenGLTextureFormat normal_buffer_format = normal_texture_is_uint ? OpenGLTextureFormat::Format_RGBA_Integer_Uint8 : OpenGLTextureFormat::Format_RGBA_Linear_Uint8;
	const OpenGLTextureFormat depth_format = OpenGLTextureFormat::Format_Depth_Float;

	prepass_colour_renderbuffer = new RenderBuffer(prepass_xres, prepass_yres, prepass_msaa_samples, prepass_col_buffer_format);
	prepass_normal_renderbuffer = new RenderBuffer(prepass_xres, prepass_yres, prepass_msaa_samples, normal_buffer_format);
	prepass_depth_renderbuffer  = new RenderBuffer(prepass_xres, prepass_yres, prepass_msaa_samples, depth_format);
	prepass_framebuffer = new FrameBuffer();
	prepass_framebuffer->attachRenderBuffer(*prepass_colour_renderbuffer, GL_COLOR_ATTACHMENT0);
	prepass_framebuffer->attachRenderBuffer(*prepass_normal_renderbuffer, GL_COLOR_ATTACHMENT1);
	prepass_framebuffer->attachRenderBuffer(*prepass_depth_renderbuffer, GL_DEPTH_ATTACHMENT);

	prepass_colour_copy_texture = new OpenGLTexture(prepass_xres, prepass_yres, this, /*data=*/ArrayRef<uint8>(), prepass_col_buffer_format, OpenGLTexture::Filtering_Nearest, OpenGLTexture::Wrapping_Clamp, /*has_mipmaps=*/false, /*MSAA_samples=*/1);
	prepass_colour_copy_texture->setDebugName("prepass_colour_copy_texture");
	prepass_normal_copy_texture = new OpenGLTexture(prepass_xres, prepass_yres, this, /*data=*/ArrayRef<uint8>(), normal_buffer_format, OpenGLTexture::Filtering_Nearest, OpenGLTexture::Wrapping_Clamp, /*has_mipmaps=*/false, /*MSAA_samples=*/1);
	prepass_normal_copy_texture->setDebugName("prepass_normal_copy_texture");
	prepass_depth_copy_texture = new OpenGLTexture(prepass_xres, prepass_yres, this, /*data=*/ArrayRef<uint8>(), depth_format, OpenGLTexture::Filtering_Nearest, OpenGLTexture::Wrapping_Clamp, /*has_mipmaps=*/false, /*MSAA_samples=*/1);
	prepass_depth_copy_texture->setDebugName("prepass_depth_copy_texture");
	prepass_copy_framebuffer = new FrameBuffer();
	prepass_copy_framebuffer->attachTexture(*prepass_colour_copy_texture, GL_COLOR_ATTACHMENT0);
	prepass_copy_framebuffer->attachTexture(*prepass_normal_copy_texture, GL_COLOR_ATTACHMENT1);
	prepass_copy_framebuffer->attachTexture(*prepass_depth_copy_texture, GL_DEPTH_ATTACHMENT);

	ssao_texture = new OpenGLTexture(prepass_xres, prepass_yres, this, /*data=*/ArrayRef<uint8>(), 
		OpenGLTextureFormat::Format_RGBA_Linear_Half/*col_buffer_format*/, OpenGLTexture::Filtering_Nearest, OpenGLTexture::Wrapping_Clamp, /*has_mipmaps=*/false, /*MSAA_samples=*/1);
	ssao_texture->setDebugName("ssao_texture");

	blurred_ssao_texture = new OpenGLTexture(prepass_xres, prepass_yres, this, /*data=*/ArrayRef<uint8>(), 
		OpenGLTextureFormat::Format_RGBA_Linear_Half, OpenGLTexture::Filtering_Nearest, OpenGLTexture::Wrapping_Clamp, /*has_mipmaps=*/false, /*MSAA_samples=*/1);
	blurred_ssao_texture->setDebugName("blurred_ssao_texture");

	blurred_ssao_texture_x = new OpenGLTexture(prepass_xres, prepass_yres, this, /*data=*/ArrayRef<uint8>(), 
		OpenGLTextureFormat::Format_RGBA_Linear_Half, OpenGLTexture::Filtering_Nearest, OpenGLTexture::Wrapping_Clamp, /*has_mipmaps=*/false, /*MSAA_samples=*/1);
	blurred_ssao_texture_x->setDebugName("blurred_ssao_texture_x");

	// We will store the screen-space trace distance (used to determine amount of blurring) in w coord.
	ssao_specular_texture = new OpenGLTexture(prepass_xres, prepass_yres, this, /*data=*/ArrayRef<uint8>(), 
		OpenGLTextureFormat::Format_RGBA_Linear_Half/*col_buffer_format*/, OpenGLTexture::Filtering_Nearest, OpenGLTexture::Wrapping_Clamp, /*has_mipmaps=*/false, /*MSAA_samples=*/1);
	ssao_specular_texture->setDebugName("ssao_specular_texture");

	blurred_ssao_specular_texture = new OpenGLTexture(prepass_xres, prepass_yres, this, /*data=*/ArrayRef<uint8>(), 
		OpenGLTextureFormat::Format_RGB_Linear_Half, OpenGLTexture::Filtering_Nearest, OpenGLTexture::Wrapping_Clamp, /*has_mipmaps=*/false, /*MSAA_samples=*/1);
	blurred_ssao_specular_texture->setDebugName("blurred_ssao_specular_texture");

	compute_ssao_framebuffer = new FrameBuffer();
	compute_ssao_framebuffer->attachTexture(*ssao_texture, GL_COLOR_ATTACHMENT0);
	compute_ssao_framebuffer->attachTexture(*ssao_specular_texture, GL_COLOR_ATTACHMENT1);

	blurred_ssao_framebuffer = new FrameBuffer();
	blurred_ssao_framebuffer->attachTexture(*blurred_ssao_texture, GL_COLOR_ATTACHMENT0);

	blurred_ssao_specular_framebuffer = new FrameBuffer();
	blurred_ssao_specular_framebuffer->attachTexture(*blurred_ssao_specular_texture, GL_COLOR_ATTACHMENT0);

	blurred_ssao_framebuffer_x = new FrameBuffer();
	blurred_ssao_framebuffer_x->attachTexture(*blurred_ssao_texture_x, GL_COLOR_ATTACHMENT0);
#endif

	//TEMP:
	if(texture_debug_preview_overlay_obs.size() >= 4)
	{
		texture_debug_preview_overlay_obs[0]->material.albedo_texture = prepass_colour_copy_texture;

		texture_debug_preview_overlay_obs[1]->material.albedo_texture = ssao_specular_texture;//prepass_normal_copy_texture;
		texture_debug_preview_overlay_obs[1]->material.overlay_show_just_tex_rgb = true;

		texture_debug_preview_overlay_obs[2]->material.albedo_texture = ssao_texture;
		texture_debug_preview_overlay_obs[2]->material.overlay_show_just_tex_rgb = true;

		texture_debug_preview_overlay_obs[3]->material.albedo_texture = ssao_texture;
		texture_debug_preview_overlay_obs[3]->material.overlay_show_just_tex_w = true;

		//texture_debug_preview_overlay_obs[5]->material.albedo_texture = blurred_ssao_texture;
		//texture_debug_preview_overlay_obs[5]->material.overlay_show_just_tex_w = true;

		//large_debug_overlay_ob->material.albedo_texture = blurred_ssao_texture;
		//large_debug_overlay_ob->material.overlay_show_just_tex_w = true;

		//large_debug_overlay_ob2->material.albedo_texture = blurred_ssao_specular_texture;
	}
}


void OpenGLEngine::setMainViewportDims(int main_viewport_w_, int main_viewport_h_)
{
	main_viewport_w = main_viewport_w_;
	main_viewport_h = main_viewport_h_;
}


void OpenGLEngine::setViewportDims(int viewport_w_, int viewport_h_)
{
	viewport_w = viewport_w_;
	viewport_h = viewport_h_;
}


void OpenGLScene::unloadAllData()
{
	this->objects.clear();
	this->transparent_objects.clear();
	this->decal_objects.clear();
	this->alpha_blended_objects.clear();

	this->env_ob->materials[0] = OpenGLMaterial();
}


GLMemUsage OpenGLScene::getTotalMemUsage() const
{
	GLMemUsage sum;
	
	// Multiple objects can share the same mesh data.
	// To avoid double-counting the mesh data, set processed_in_mem_count flag on it when it has been counted.
	// We could also use a local set while counting, setting a flag should be faster though.

	// Clear flag
	for(auto i = objects.begin(); i != objects.end(); ++i)
		(*i)->mesh_data->processed_in_mem_count = false;

	for(auto i = objects.begin(); i != objects.end(); ++i)
	{
		OpenGLMeshRenderData* mesh_data = (*i)->mesh_data.ptr();
		if(!mesh_data->processed_in_mem_count)
		{
			sum += mesh_data->getTotalMemUsage();
			mesh_data->processed_in_mem_count = true;
		}
	}
	return sum;
}


void OpenGLEngine::unloadAllData()
{
	for(auto i = scenes.begin(); i != scenes.end(); ++i)
		(*i)->unloadAllData();

	this->selected_objects.clear();

	// null out the pointer to the opengl engine so the textures doesn't try and call textureBecameUnused() while we're clearing opengl_textures.
	for(auto it = opengl_textures.begin(); it != opengl_textures.end(); ++it)
	{
		it->second.value->inserted_into_opengl_textures = false;
		it->second.value->m_opengl_engine = NULL;
	}
	opengl_textures.clear();

	tex_CPU_mem_usage = 0;
	tex_GPU_mem_usage = 0;
}


const js::AABBox OpenGLEngine::getAABBWSForObjectWithTransform(GLObject& object, const Matrix4f& to_world)
{
	return object.mesh_data->aabb_os.transformedAABBFast(to_world);
}


struct LightDistComparator
{
	LightDistComparator(const Vec4f& use_ob_pos_) : use_ob_pos(use_ob_pos_) {}

	inline bool operator () (const GLLight* a, const GLLight* b)
	{
		return use_ob_pos.getDist2(a->gpu_data.pos) < use_ob_pos.getDist2(b->gpu_data.pos);
	}

	Vec4f use_ob_pos;
};


// Sets object ob_to_world_normal_matrix, ob_to_world_matrix_determinant and aabb_ws, then updates object data on GPU.
void OpenGLEngine::setObjectTransformData(GLObject& object)
{
	assert(::isFinite(object.ob_to_world_matrix.e[0]));

	const Matrix4f& to_world = object.ob_to_world_matrix;

	if(!object.mesh_data->pos_coords_quantised)
	{
		object.dequantise_scale = Vec4f(1.f);
		object.dequantise_translation = Vec4f(0.f);
	}
	else
	{
		object.dequantise_scale = setWToOne(div(object.mesh_data->aabb_os.span(), Vec4f(65535.f)));
		object.dequantise_translation = maskWToZero(object.mesh_data->aabb_os.min_);
	}

	// We will use sign(det(M)) adj(M)^T for the normal matrix, see
	// 'Transforming normals: adjugate transpose vs inverse transpose' - https://forwardscattering.org/post/62
	to_world.getUpperLeftAdjugateTranspose(/*result=*/object.ob_to_world_normal_matrix);

	const float upper_left_det = to_world.upperLeftDeterminant();
	object.ob_to_world_matrix_determinant = upper_left_det;

	// Apply sign(det(M)) factor
	if(upper_left_det < 0.f)
		object.ob_to_world_normal_matrix.negateInPlace();

	object.aabb_ws = object.mesh_data->aabb_os.transformedAABBFast(to_world);

	assignLightsToObject(object);

	updateObjectDataOnGPU(object);
}


void OpenGLEngine::updateObjectDataOnGPU(GLObject& object)
{
	ZoneScoped; // Tracy profiler

	if(use_ob_and_mat_data_gpu_resident)
	{
		PerObjectVertUniforms uniforms;
		uniforms.model_matrix = object.ob_to_world_matrix;
		uniforms.normal_matrix = object.ob_to_world_normal_matrix;
		for(int i=0; i<GLObject::MAX_NUM_LIGHT_INDICES; ++i)
			uniforms.light_indices[i] = object.light_indices[i];
		uniforms.depth_draw_depth_bias = object.depth_draw_depth_bias;
		uniforms.model_matrix_upper_left_det = object.ob_to_world_matrix_determinant;
		uniforms.uv0_scale = object.mesh_data->uv0_scale;
		uniforms.uv1_scale = object.mesh_data->uv1_scale;
		uniforms.dequantise_scale = object.dequantise_scale;
		uniforms.dequantise_translation = object.dequantise_translation;

		assert(object.per_ob_vert_data_index >= 0);
		if(object.per_ob_vert_data_index >= 0)
		{
			// Add update to back of data_updates_buffer, if we are using a scatter shader for updating per_ob_vert_data_buffer.
			if(use_scatter_shader)
			{
				DataUpdateStruct data_update;
				data_update.data = uniforms;
				data_update.write_index = object.per_ob_vert_data_index;

				const size_t write_i = data_updates_buffer.size();
				if(write_i < data_updates_ssbo->byteSize())
				{
					data_updates_buffer.resize(write_i + sizeof(DataUpdateStruct));
					std::memcpy(&data_updates_buffer[write_i], &data_update, sizeof(DataUpdateStruct));
				}
				else
					conPrint("Can't add data update to data_updates_buffer, is full");
			}
			else
			{
				per_ob_vert_data_buffer->updateData(/*dest offset=*/object.per_ob_vert_data_index * sizeof(PerObjectVertUniforms), /*src data=*/&uniforms, /*src size=*/sizeof(PerObjectVertUniforms));
			}
		}
	}
}


void OpenGLEngine::updateObjectLightDataOnGPU(GLObject& object)
{
	if(use_ob_and_mat_data_gpu_resident)
	{
		PerObjectVertUniforms uniforms;
		for(int i=0; i<GLObject::MAX_NUM_LIGHT_INDICES; ++i)
			uniforms.light_indices[i] = object.light_indices[i];

		static_assert(offsetof(PerObjectVertUniforms, light_indices) == sizeof(Matrix4f)*2 + sizeof(Vec4f)*2);

		assert(object.per_ob_vert_data_index >= 0);
		if(object.per_ob_vert_data_index >= 0)
		{
			assert(!use_scatter_shader);
			
			per_ob_vert_data_buffer->updateData(/*dest offset=*/object.per_ob_vert_data_index * sizeof(PerObjectVertUniforms) + offsetof(PerObjectVertUniforms, light_indices), /*src data=*/uniforms.light_indices, 
				/*src size=*/sizeof(PerObjectVertUniforms::light_indices));
		}
	}
}


// Updates object ob_to_world_inv_transpose_matrix and aabb_ws etc.
void OpenGLEngine::updateObjectTransformData(GLObject& object)
{
	// If the determinant sign changes, then we need to change back-face culling to front-face culling and vice versa.
	// That is done in rebuildDenormalisedDrawData().
	const bool old_det_positive = object.ob_to_world_matrix_determinant >= 0.f;

	setObjectTransformData(object);

	if((object.ob_to_world_matrix_determinant >= 0.f) != old_det_positive)
	{
		rebuildObjectDepthDrawBatches(object);
		rebuildDenormalisedDrawData(object);
	}
}


void OpenGLEngine::objectTransformDataChanged(GLObject& object)
{
	updateObjectDataOnGPU(object);

	// TODO: call assignLightsToObject as well here.  Investigate perf tho because this is called by evalObjectScript().
}




/*
Computing the AABB of a cone light.
Note that this does not quite capture the curved base of the cone, but should be approximately correct.
                   x 
                  /
                /
              /
            /
          /
        /         op
      /
    /
  /
/  cone angle
------------------|---------------|----> dir
       a
<----------- max_light_dist ----->



cos(cone_angle) = a/h = a/max_light_dist
a = max_light_dist * cos(cone_angle)
*/
static inline js::AABBox computelightAABB(const GLLight& light)
{
	const Matrix4f m = Matrix4f::constructFromVectorStatic(light.gpu_data.dir);
	const Vec4f v1 = m.getColumn(0);
	const Vec4f v2 = m.getColumn(1);

	const float max_light_dist = light.max_light_dist;

	const float a = max_light_dist * light.gpu_data.cone_min_cos_angle;
	const float op = max_light_dist * Maths::sinForCos(light.gpu_data.cone_min_cos_angle);

	js::AABBox aabb(light.gpu_data.pos, light.gpu_data.pos);
	aabb.enlargeToHoldPoint(light.gpu_data.pos + light.gpu_data.dir*a + v1 * op + v2 * op);
	aabb.enlargeToHoldPoint(light.gpu_data.pos + light.gpu_data.dir*a - v1 * op + v2 * op);
	aabb.enlargeToHoldPoint(light.gpu_data.pos + light.gpu_data.dir*a + v1 * op - v2 * op);
	aabb.enlargeToHoldPoint(light.gpu_data.pos + light.gpu_data.dir*a - v1 * op - v2 * op);
	aabb.enlargeToHoldPoint(light.gpu_data.pos + light.gpu_data.dir*max_light_dist);

	return aabb;
}


void OpenGLEngine::assignLightsToObject(GLObject& object)
{
	// Assign lights to object
	const js::AABBox ob_aabb_ws = object.aabb_ws;

	assert(ob_aabb_ws.min_.isFinite() && ob_aabb_ws.max_.isFinite());
	if(!ob_aabb_ws.min_.isFinite() || !ob_aabb_ws.max_.isFinite())
		return;

	const Vec4i min_bucket_i = current_scene->light_grid.bucketIndicesForPoint(ob_aabb_ws.min_);
	const Vec4i max_bucket_i = current_scene->light_grid.bucketIndicesForPoint(ob_aabb_ws.max_);

	const int MAX_LIGHTS = 128; // Max number of lights to consider
	const GLLight* lights[MAX_LIGHTS];

	const int64 x_num_buckets = (int64)max_bucket_i[0] - (int64)min_bucket_i[0] + 1;
	const int64 y_num_buckets = (int64)max_bucket_i[1] - (int64)min_bucket_i[1] + 1;
	const int64 z_num_buckets = (int64)max_bucket_i[2] - (int64)min_bucket_i[2] + 1;
	if(x_num_buckets > 256 || y_num_buckets > 256 || z_num_buckets > 256 || (x_num_buckets * y_num_buckets * z_num_buckets) > 256)
		return;

	int num_lights = 0;
	for(int x=min_bucket_i[0]; x <= max_bucket_i[0]; ++x)
	for(int y=min_bucket_i[1]; y <= max_bucket_i[1]; ++y)
	for(int z=min_bucket_i[2]; z <= max_bucket_i[2]; ++z)
	{
		const auto& bucket = current_scene->light_grid.getBucketForIndices(x, y, z);

		for(auto it = bucket.objects.begin(); it != bucket.objects.end(); ++it)
		{
			const GLLight* light = *it;
			if(light->aabb_ws.intersectsAABB(ob_aabb_ws) && num_lights < MAX_LIGHTS)
				lights[num_lights++] = light;
		}
	}

	// Sort lights based on distance from object centre
	const Vec4f use_ob_pos = ob_aabb_ws.centroid();
	LightDistComparator comparator(use_ob_pos);
	std::sort(lights, lights + num_lights, comparator);

	const GLLight* prev_light = NULL;
	int dest_i = 0;
	for(int i=0; i<num_lights; ++i)
	{
		if(lights[i] != prev_light) // Avoid adding the same light twice.
		{
			object.light_indices[dest_i++] = lights[i]->buffer_index;
			prev_light = lights[i];
			if(dest_i >= GLObject::MAX_NUM_LIGHT_INDICES)
				break;
		}
	}

	// Set remaining light indices to -1 (no light)
	for(int i=dest_i; i<GLObject::MAX_NUM_LIGHT_INDICES; ++i)
		object.light_indices[i] = -1;
}


// This is linear-time on the number of objects in the scene.
// Could make it sub-linear by using an acceleration structure for the objects.
void OpenGLEngine::rebuildObjectLightInfoForNewLight(const GLLight& light)
{
	const js::AABBox light_aabb = light.aabb_ws;

	//Timer timer;
	//int num_obs_updated = 0;
	GLObjectRef* const current_scene_obs = current_scene->objects.vector.data();
	const size_t current_scene_obs_size  = current_scene->objects.vector.size();
	for(size_t q=0; q<current_scene_obs_size; ++q)
	{
		// Prefetch cachelines containing the variables we need for objects N places ahead in the array.
		if(q + 16 < current_scene_obs_size)
			_mm_prefetch((const char*)(&current_scene_obs[q + 16]->aabb_ws), _MM_HINT_T0);
	
		GLObject* ob = current_scene_obs[q].ptr();
		if(light_aabb.intersectsAABB(ob->aabb_ws))
		{
			assignLightsToObject(*ob);

			updateObjectLightDataOnGPU(*ob);

			//num_obs_updated++;
		}
	}
	//conPrint("OpenGLEngine::rebuildObjectLightInfoForNewLight(): Assigning lights for " + toString(current_scene->objects.size()) + " objects with " + toString(num_obs_updated) + " updates took " + timer.elapsedStringMSWIthNSigFigs(3));
}


void OpenGLEngine::assignShaderProgToMaterial(OpenGLMaterial& material, bool use_vert_colours, bool uses_instancing, bool uses_skinning, bool use_vert_tangents, bool position_w_is_oct16_normal)
{
	// If the client code has already set a special non-basic shader program (like a grid shader), don't overwrite it.
	if(!material.auto_assign_shader)
	{
		assert(material.shader_prog.nonNull());
		if(material.shader_prog.nonNull())
			return;
	}

	const bool alpha_test = material.albedo_texture && material.albedo_texture->hasAlpha() && material.allow_alpha_test && !material.decal && !material.alpha_blend;

	const bool uses_lightmapping = this->texture_compression_BC6H_support && material.lightmap_texture.nonNull();

	ProgramKeyArgs key_args;
	key_args.alpha_test = alpha_test;
	key_args.vert_colours = use_vert_colours;
	key_args.vert_tangents = use_vert_tangents;
	key_args.instance_matrices = uses_instancing;
	key_args.lightmapping = uses_lightmapping;
	key_args.gen_planar_uvs = material.gen_planar_uvs;
	key_args.draw_planar_uv_grid = material.draw_planar_uv_grid;
	key_args.skinning = uses_skinning;
	key_args.imposter = material.imposter;
	key_args.imposterable = material.imposterable;
	key_args.use_wind_vert_shader = material.use_wind_vert_shader;
	key_args.fancy_double_sided = material.fancy_double_sided;
	key_args.materialise_effect = material.materialise_effect;
	key_args.terrain = material.terrain;
	key_args.decal = material.decal;
	key_args.participating_media = material.participating_media;
	key_args.sdf_text = material.sdf_text;
	key_args.combined = material.combined;
	key_args.position_w_is_oct16_normal = position_w_is_oct16_normal;

	const ProgramKey key(material.participating_media ? ProgramKey::ProgramName_participatingMedia : (material.water ? ProgramKey::ProgramName_water : 
		(material.imposter ? ProgramKey::ProgramName_imposter: (material.transparent ? ProgramKey::ProgramName_transparent : ProgramKey::ProgramName_phong))), 
		key_args);

	material.shader_prog = getProgramWithFallbackOnError(key);

	if(settings.shadow_mapping)
	{
		if(material.transparent || material.water || material.decal || material.participating_media || !material.cast_shadows) // If this material shouldn't cast shadows:
		{
			material.depth_draw_shader_prog = this->fallback_depth_prog; // Just use the default depth-draw program so we don't crash if we accidentally draw with it.
			material.draw_into_depth_buffer = false;
		}
		else
		{
			// Note that some of the depth_key values are set to false in getDepthDrawProgram() to avoid having too many depth programs.
			const ProgramKey depth_key(ProgramKey::ProgramName_depth, key_args);

			material.depth_draw_shader_prog = getDepthDrawProgramWithFallbackOnError(depth_key);
			material.draw_into_depth_buffer = true;
		}
	}

	// Alloc buffer for material
#if UNIFORM_BUF_PER_MAT_SUPPORT
	if(material.shader_prog->uses_phong_uniforms)
	{
		if(material.uniform_buf_ob.isNull())
		{
			material.uniform_buf_ob = new UniformBufOb();
			material.uniform_buf_ob->allocate(sizeof(PhongUniforms));
		}
	}
#endif
}


void OpenGLEngine::waitForAllBuildingProgramsToBuild()
{
	//================= Check if any building programs are done. =================
	while(!building_progs.empty())
	{
		for(auto it = building_progs.begin(); it != building_progs.end(); )
		{
			OpenGLProgram* prog = *it;
			if(prog->checkLinkingDone())
			{
				it = building_progs.erase(it);

				try
				{
					finishBuildingProg(prog);
				}
				catch(glare::Exception& e)
				{
					conPrint("Error building program: " + e.what());
				}
			}
			else
				++it;
		}

		if(!building_progs.empty())
			PlatformUtils::Sleep(1);
	}
}


void OpenGLEngine::bindCommonVertUniformBlocksToProgram(const Reference<OpenGLProgram>& prog)
{
	bindUniformBlockToProgram(prog, "SharedVertUniforms",		SHARED_VERT_UBO_BINDING_POINT_INDEX);
	bindUniformBlockToProgram(prog, "PerObjectVertUniforms",	PER_OBJECT_VERT_UBO_BINDING_POINT_INDEX);
}


Reference<GLObject> OpenGLEngine::allocateObject()
{
	return new GLObject();

	/*glare::PoolAllocator::AllocResult alloc_res = object_pool_allocator.alloc();

	GLObject* ob_ptr = new (alloc_res.ptr) GLObject(); // construct with placement new
	ob_ptr->allocator = &object_pool_allocator;
	ob_ptr->allocation_index = alloc_res.index;

	return ob_ptr;*/
}


int OpenGLEngine::allocPerObVertDataBufferSpot()
{
	assert(use_ob_and_mat_data_gpu_resident);

	if(per_ob_vert_data_free_indices.empty()) // If no free indices remain:
		expandPerObVertDataBuffer();
	assert(!per_ob_vert_data_free_indices.empty());

	auto it = per_ob_vert_data_free_indices.begin();
	if(it == per_ob_vert_data_free_indices.end())
	{
		assert(0); // Out of free spaces, and expansion failed somehow.
		return -1;
	}
	else
	{
		const int free_index = *it;
		per_ob_vert_data_free_indices.erase(it);

		return free_index;
	}
}


static void checkMaterial(const OpenGLMaterial& mat)
{
	if(mat.albedo_texture && mat.albedo_texture->getTextureTarget() != GL_TEXTURE_2D)
	{
		conPrint("ERROR: mat.albedo_texture texture type (target) is not a GL_TEXTURE_2D, target is " + getStringForTextureTarget(mat.albedo_texture->getTextureTarget()) + ", will not render correctly.");
	}
}


// MaterialData flag values
#define HAVE_SHADING_NORMALS_FLAG			1
#define HAVE_TEXTURE_FLAG					2
#define HAVE_METALLIC_ROUGHNESS_TEX_FLAG	4
#define HAVE_EMISSION_TEX_FLAG				8
#define IS_HOLOGRAM_FLAG					16 // e.g. no light scattering, just emission
#define IMPOSTER_TEX_HAS_MULTIPLE_ANGLES	32
#define HAVE_NORMAL_MAP_FLAG				64
#define SIMPLE_DOUBLE_SIDED_FLAG			128
#define SWIZZLE_ALBEDO_TEX_R_TO_RGB_FLAG	256 // If the texture is a single channel texture, we want to make it render as greyscale (as opposed to red), e.g. set tex_col.x,y,z = tex_col.x in the frag shader.
#define CONVERT_ALBEDO_FROM_SRGB_FLAG		512 // convert albedo texture colour from non-linear sRGB to linear sRGB in frag shader


static int computeUniformFlagsForMat(const OpenGLMaterial& opengl_mat, const OpenGLMeshRenderData& mesh_data)
{
	const bool swizzle_albedo_tex_r_to_rgb = opengl_mat.albedo_texture && (opengl_mat.albedo_texture->getInternalFormat() == GL_R8);
	
	return
		(mesh_data.has_shading_normals						? HAVE_SHADING_NORMALS_FLAG			: 0) |
		(opengl_mat.albedo_texture.nonNull()				? HAVE_TEXTURE_FLAG					: 0) |
		(opengl_mat.metallic_roughness_texture.nonNull()	? HAVE_METALLIC_ROUGHNESS_TEX_FLAG	: 0) |
		(opengl_mat.emission_texture.nonNull()				? HAVE_EMISSION_TEX_FLAG			: 0) |
		(opengl_mat.hologram								? IS_HOLOGRAM_FLAG					: 0) |
		(opengl_mat.imposter_tex_has_multiple_angles		? IMPOSTER_TEX_HAS_MULTIPLE_ANGLES	: 0) |
		(opengl_mat.normal_map.nonNull()					? HAVE_NORMAL_MAP_FLAG				: 0) |
		(opengl_mat.simple_double_sided						? SIMPLE_DOUBLE_SIDED_FLAG			: 0) |
		(swizzle_albedo_tex_r_to_rgb						? SWIZZLE_ALBEDO_TEX_R_TO_RGB_FLAG	: 0) |
		(opengl_mat.convert_albedo_from_srgb				? CONVERT_ALBEDO_FROM_SRGB_FLAG		: 0);
}


void OpenGLEngine::buildObjectData(const Reference<GLObject>& object)
{
	assert(object->mesh_data.nonNull());
	assert(!object->mesh_data->vertex_spec.attributes.empty());
	//assert(object->mesh_data->vert_vao.nonNull());

	// Check object material indices are in-bounds, so we don't have to check in inner draw loop.
	const ArrayRef<OpenGLBatch> use_src_batches = object->getUsedBatches();
	for(size_t i=0; i<use_src_batches.size(); ++i)
		if(use_src_batches[i].material_index >= object->materials.size())
			throw glare::Exception("GLObject material_index is out of bounds.");


	// Alloc spot in uniform buffer.  Note that we need to do this before updateObjectTransformData() which uses object->per_ob_vert_data_index.
	if(use_ob_and_mat_data_gpu_resident)
		object->per_ob_vert_data_index = allocPerObVertDataBufferSpot();

	object->random_num = rng.nextUInt();

	for(size_t i=0; i<object->materials.size(); ++i)
		checkMaterial(object->materials[i]);

	// Build materials
	for(size_t i=0; i<object->materials.size(); ++i)
	{
		assignShaderProgToMaterial(object->materials[i], object->mesh_data->has_vert_colours, /*uses instancing=*/object->instance_matrix_vbo.nonNull(), object->mesh_data->usesSkinning(), object->mesh_data->has_vert_tangents,
			object->mesh_data->position_w_is_oct16_normal);

		object->materials[i].uniform_flags = computeUniformFlagsForMat(object->materials[i], *object->mesh_data);
	}


	const AnimationData& anim_data = object->mesh_data->animation_data;

	// Upload material data to GPU
	if(use_ob_and_mat_data_gpu_resident)
	{
		for(size_t i=0; i<object->materials.size(); ++i)
		{
			PhongUniforms uniforms;
			setUniformsForPhongProg(object->materials[i], *object->mesh_data, uniforms);

			if(phong_buffer_free_indices.empty())
				expandPhongBuffer();
			assert(!phong_buffer_free_indices.empty());

			// Alloc spot in uniform buffer
			auto it = phong_buffer_free_indices.begin();
			if(it == phong_buffer_free_indices.end())
			{
				// Out of free spaces
				assert(0);
				conPrint("Out of space in phong uniforms buffer");
			}
			else
			{
				const int free_index = *it;
				phong_buffer_free_indices.erase(it);

				object->materials[i].material_data_index = free_index;
				phong_buffer->updateData(/*dest offset=*/free_index * sizeof(PhongUniforms), /*src data=*/&uniforms, /*src size=*/sizeof(PhongUniforms));
			}
		}

		// Alloc spot in joint matrices buffer if needed
		if(!anim_data.animations.empty() || !anim_data.joint_nodes.empty())
		{
			const size_t num_joint_matrices = anim_data.joint_nodes.size();

			glare::BestFitAllocator::BlockInfo* block = joint_matrices_allocator->alloc(num_joint_matrices, /*alignment=*/1);
			if(block == NULL)
			{
				expandJointMatricesBuffer(/*min extra needed=*/num_joint_matrices);
				block = joint_matrices_allocator->alloc(num_joint_matrices, /*alignment=*/1);
			}

			if(block == NULL)
			{
				// failed to alloc
				conPrint("Failed to alloc space in joint matrices buffer");
				assert(0);
			}
			else
			{
				object->joint_matrices_base_index = (int)block->aligned_offset;
				object->joint_matrices_block = block;
			}
		}
	}
	else
	{
#if UNIFORM_BUF_PER_MAT_SUPPORT
		for(size_t i=0; i<object->materials.size(); ++i)
		{
			if(object->materials[i].uniform_buf_ob.nonNull())
			{
				PhongUniforms uniforms;
				setUniformsForPhongProg(object->materials[i], *object->mesh_data, uniforms);
				object->materials[i].uniform_buf_ob->updateData(0, &uniforms, sizeof(PhongUniforms));
			}
		}
#endif
	}

	// Compute world space AABB of object etc.
	// setObjectTransformData() needs to go before rebuildObjectDepthDrawBatches etc. since we need to set determinant_positive.
	setObjectTransformData(*object);

	rebuildObjectDepthDrawBatches(*object);
	rebuildDenormalisedDrawData(*object);
}


void OpenGLEngine::addObject(const Reference<GLObject>& object)
{
	buildObjectData(object);

	// Don't add always_visible objects to objects set, we will draw them a special way.
	if(!object->always_visible)
		current_scene->objects.insert(object);

	bool have_transparent_mat    = false;
	bool have_materialise_effect = false;
	bool have_water_mat = false;
	bool have_partic_media_mat = false;
	bool have_alpha_blend_mat = false;
	bool have_decal_mat = false;
	for(size_t i=0; i<object->materials.size(); ++i)
	{
		const OpenGLMaterial& mat = object->materials[i];
		have_transparent_mat    = have_transparent_mat    || mat.transparent;
		have_materialise_effect = have_materialise_effect || mat.materialise_effect;
		have_water_mat          = have_water_mat          || mat.water;
		have_partic_media_mat   = have_partic_media_mat   || mat.participating_media;
		have_alpha_blend_mat    = have_alpha_blend_mat    || mat.alpha_blend;
		have_decal_mat          = have_decal_mat          || mat.decal;
	}

	if(have_transparent_mat)
		current_scene->transparent_objects.insert(object);

	if(have_materialise_effect)
		current_scene->materialise_objects.insert(object);

	if(have_water_mat)
		current_scene->water_objects.insert(object);

	if(have_partic_media_mat || have_alpha_blend_mat)
		current_scene->alpha_blended_objects.insert(object);

	if(object->always_visible)
		current_scene->always_visible_objects.insert(object);

	if(have_decal_mat)
		current_scene->decal_objects.insert(object);

	const AnimationData& anim_data = object->mesh_data->animation_data;
	if(!anim_data.animations.empty() || !anim_data.joint_nodes.empty())
		current_scene->animated_objects.insert(object);
}


static inline bool backfaceCullMat(const OpenGLMaterial& mat)
{
	return !mat.simple_double_sided && !mat.fancy_double_sided;
}


static inline uint32 faceCullBits(const GLObject& object, const OpenGLMaterial& mat)
{
	return backfaceCullMat(mat) ? ((object.ob_to_world_matrix_determinant >= 0.f) ? CULL_BACKFACE_BITS : CULL_FRONTFACE_BITS) : 0;
}


void OpenGLEngine::rebuildDenormalisedDrawData(GLObject& object)
{
	// Build misc draw info.
	object.vao_and_vbo_key = makeVAOAndVBOKey(
		object.mesh_data->vao_data_index,
		(uint32)object.mesh_data->vbo_handle.vbo_id,
		(uint32)object.mesh_data->indices_vbo_handle.vbo_id,
		object.mesh_data->index_type_bits
	);

	const ArrayRef<OpenGLBatch> use_src_batches = object.getUsedBatches();
	object.batch_draw_info.resize(use_src_batches.size());

	for(size_t i=0; i<use_src_batches.size(); ++i)
	{
		const OpenGLMaterial& mat = object.materials[use_src_batches[i].material_index];

		const uint32 face_culling_bits = faceCullBits(object, mat);

		object.batch_draw_info[i].program_index_and_flags = mat.shader_prog->program_index |
			(mat.shader_prog->supports_gpu_resident  ? PROG_SUPPORTS_GPU_RESIDENT_BITFLAG : 0) |
			(mat.transparent                ? MATERIAL_TRANSPARENT_BITFLAG      : 0) |
			(mat.water                      ? MATERIAL_WATER_BITFLAG            : 0) |
			(mat.decal                      ? MATERIAL_DECAL_BITFLAG            : 0) |
			(mat.participating_media        ? MATERIAL_ALPHA_BLEND_BITFLAG      : 0) |
			(mat.alpha_blend                ? MATERIAL_ALPHA_BLEND_BITFLAG      : 0) |
			(face_culling_bits             << MATERIAL_FACE_CULLING_BIT_INDEX)       |
			(mat.shader_prog->isBuilt()     ? PROGRAM_FINISHED_BUILDING_BITFLAG : 0);

		//assert(mat.material_data_index != -1);
		if(use_ob_and_mat_data_gpu_resident && mat.shader_prog->supports_gpu_resident)
			object.batch_draw_info[i].material_data_or_mat_index = mat.material_data_index;
		else
			object.batch_draw_info[i].material_data_or_mat_index = use_src_batches[i].material_index;
	
		object.batch_draw_info[i].prim_start_offset_B = use_src_batches[i].prim_start_offset_B;
		object.batch_draw_info[i].num_indices         = use_src_batches[i].num_indices;
	}

#if DO_INDIVIDUAL_VAO_ALLOC
	object.vao = object.vert_vao.nonNull() ? object.vert_vao.ptr() : object.mesh_data->individual_vao.ptr();
#else
	VertexBufferAllocator::VAOData& vao_data = vert_buf_allocator->vao_data[object.mesh_data->vao_data_index];
	object.vao = object.vert_vao.nonNull() ? object.vert_vao.ptr() : vao_data.vao.ptr();
#endif
	object.vert_vbo  = object.mesh_data->vbo_handle.vbo.ptr();
	object.index_vbo = object.mesh_data->indices_vbo_handle.index_vbo.ptr();
	object.index_type_and_log2_size = (uint32)object.mesh_data->getIndexType() | (indexTypeLog2SizeBytes(object.mesh_data->getIndexType()) << 16);
	object.instance_matrix_vbo_name = object.instance_matrix_vbo.nonNull() ? object.instance_matrix_vbo->bufferName() : 0;
	object.indices_vbo_handle_offset = (uint32)object.mesh_data->indices_vbo_handle.offset;
	object.vbo_handle_base_vertex = (uint32)object.mesh_data->vbo_handle.base_vertex;
}


// If the material should draw into depth buffer (is not transparent etc.), and the shader program for it has finished building, return a pointer to the program, otherwise NULL
static inline OpenGLProgram* getBuiltDepthDrawProgForMat(OpenGLMaterial& mat)
{
	if(mat.draw_into_depth_buffer && mat.depth_draw_shader_prog.nonNull() && mat.depth_draw_shader_prog->isBuilt())
		return mat.depth_draw_shader_prog.ptr();
	else
		return NULL;
}


void OpenGLEngine::rebuildObjectDepthDrawBatches(GLObject& object)
{
	// Compute shadow mapping depth-draw batches.  We can merge multiple index batches into one if they are contiguous, if they share the same depth-draw shader, and the depth-draw shader does not do alpha testing.
	// Not doing alpha testing is important because there might be different textures assigned to different materials that use the same shader program, and we need to use the correct texture.
	// This greatly reduces the number of batches (and hence draw calls) in many cases.
	if(settings.shadow_mapping)
	{
		const uint32 index_type_size_B = indexTypeSizeBytes(object.mesh_data->getIndexType());
		const ArrayRef<OpenGLBatch> use_src_batches = object.getUsedBatches();

		// Do a pass to get number of batches required
		OpenGLProgram* prev_depth_prog = NULL;
		uint32 prev_face_cull = CULL_BACKFACE_BITS;
		size_t num_batches_required = 0;
		uint32 next_contiguous_offset = 0;
		for(size_t i=0; i<use_src_batches.size(); ++i)
		{
			const uint32 mat_index = use_src_batches[i].material_index;
			OpenGLProgram* cur_depth_prog = getBuiltDepthDrawProgForMat(object.materials[mat_index]);
			const uint32 cur_face_cull = faceCullBits(object, object.materials[mat_index]);
			if((cur_depth_prog != prev_depth_prog) || // If batch depth-draw prog differs:
				(cur_face_cull != prev_face_cull) || // or face culling differs:
				(cur_depth_prog && cur_depth_prog->is_depth_draw_with_alpha_test) || // Or if the programs both do alpha testing:
				(use_src_batches[i].prim_start_offset_B != next_contiguous_offset)) // Or this batch's primitives are not immediately after the previous batch's primitives:
			{
				if(prev_depth_prog) // Will be NULL for transparent materials and when i = 0.  We don't need a batch for transparent materials.
					num_batches_required++;
				prev_depth_prog = cur_depth_prog;
				prev_face_cull = cur_face_cull;
			}
			next_contiguous_offset = use_src_batches[i].prim_start_offset_B + use_src_batches[i].num_indices * index_type_size_B;
		}
		// Finish last batch
		if(prev_depth_prog)
			num_batches_required++;

		object.depth_draw_batches.resize(num_batches_required);

		size_t dest_batch_i = 0;
		prev_depth_prog = NULL;
		prev_face_cull = CULL_BACKFACE_BITS;
		next_contiguous_offset = 0;
		OpenGLBatch current_batch;
		current_batch.material_index = 0;
		current_batch.prim_start_offset_B = 0;
		current_batch.num_indices = 0;
		for(size_t i=0; i<use_src_batches.size(); ++i)
		{
			const uint32 mat_index = use_src_batches[i].material_index;
			OpenGLProgram* cur_depth_prog = getBuiltDepthDrawProgForMat(object.materials[mat_index]);
			const uint32 cur_face_cull = faceCullBits(object, object.materials[mat_index]);
			if((cur_depth_prog != prev_depth_prog) || // If batch depth-draw prog differs:
				(cur_face_cull != prev_face_cull) || // or face culling differs:
				(cur_depth_prog && cur_depth_prog->is_depth_draw_with_alpha_test) || // Or if the programs both do alpha testing:
				(use_src_batches[i].prim_start_offset_B != next_contiguous_offset)) // Or this batch's primitives are not immediately after the previous batch's primitives:
			{
				// The depth-draw material changed.  So finish the batch.
				if(prev_depth_prog) // Will be NULL for transparent materials and when i = 0.
				{
					object.depth_draw_batches[dest_batch_i].program_index_and_flags = prev_depth_prog->program_index |
						(prev_depth_prog->supports_gpu_resident ? PROG_SUPPORTS_GPU_RESIDENT_BITFLAG : 0) |
						(prev_face_cull                        << MATERIAL_FACE_CULLING_BIT_INDEX)        |
						(prev_depth_prog->isBuilt()             ? PROGRAM_FINISHED_BUILDING_BITFLAG  : 0);

					assert((object.materials[current_batch.material_index].material_data_index != -1) || !use_ob_and_mat_data_gpu_resident);
					if(use_ob_and_mat_data_gpu_resident && prev_depth_prog->supports_gpu_resident)
						object.depth_draw_batches[dest_batch_i].material_data_or_mat_index = object.materials[current_batch.material_index].material_data_index;
					else
						object.depth_draw_batches[dest_batch_i].material_data_or_mat_index = current_batch.material_index;

					object.depth_draw_batches[dest_batch_i].prim_start_offset_B        = current_batch.prim_start_offset_B;
					object.depth_draw_batches[dest_batch_i].num_indices                = current_batch.num_indices;

					dest_batch_i++;
				}

				// Start new batch
				current_batch.material_index = mat_index;
				current_batch.prim_start_offset_B = use_src_batches[i].prim_start_offset_B;
				current_batch.num_indices = 0;

				prev_depth_prog = cur_depth_prog;
				prev_face_cull = cur_face_cull;
			}
			next_contiguous_offset = use_src_batches[i].prim_start_offset_B + use_src_batches[i].num_indices * index_type_size_B;

			current_batch.num_indices += use_src_batches[i].num_indices;
		}

		// Finish last batch
		if(prev_depth_prog)
		{
			object.depth_draw_batches[dest_batch_i].program_index_and_flags = prev_depth_prog->program_index |
				(prev_depth_prog->supports_gpu_resident ? PROG_SUPPORTS_GPU_RESIDENT_BITFLAG         : 0) |
				(prev_face_cull                        << MATERIAL_FACE_CULLING_BIT_INDEX)                |
				(prev_depth_prog->isBuilt()             ? PROGRAM_FINISHED_BUILDING_BITFLAG          : 0);

			assert((object.materials[current_batch.material_index].material_data_index != -1) || !use_ob_and_mat_data_gpu_resident);
			if(use_ob_and_mat_data_gpu_resident && prev_depth_prog->supports_gpu_resident)
				object.depth_draw_batches[dest_batch_i].material_data_or_mat_index = object.materials[current_batch.material_index].material_data_index;
			else
				object.depth_draw_batches[dest_batch_i].material_data_or_mat_index = current_batch.material_index;
			object.depth_draw_batches[dest_batch_i].prim_start_offset_B = current_batch.prim_start_offset_B;
			object.depth_draw_batches[dest_batch_i].num_indices         = current_batch.num_indices;

			dest_batch_i++;
		}
		assert(dest_batch_i == num_batches_required);


		// Check batches we made.
		for(size_t i=0; i<object.depth_draw_batches.size(); ++i)
		{
			//assert(object.depth_draw_batches[i].material_index < 10000);
			//assert(object.materials[object.depth_draw_batches[i].material_index].depth_draw_shader_prog.nonNull());
		}

		// conPrint("Collapsed " + toString(object.getUsedBatches().size()) + " batches to " + toString(object.depth_draw_batches.size()) + " depth-draw batches");
	}
}


void OpenGLEngine::updateMaterialDataOnGPU(const GLObject& ob, size_t mat_index)
{
	if(use_ob_and_mat_data_gpu_resident)
	{
		PhongUniforms uniforms;
		setUniformsForPhongProg(ob.materials[mat_index], *ob.mesh_data, uniforms);

		assert(ob.materials[mat_index].material_data_index >= 0);
		if(ob.materials[mat_index].material_data_index >= 0)
		{
			phong_buffer->updateData(/*dest offset=*/ob.materials[mat_index].material_data_index * sizeof(PhongUniforms), /*src data=*/&uniforms, /*src size=*/sizeof(PhongUniforms));
		}
	}
	else
	{
#if UNIFORM_BUF_PER_MAT_SUPPORT
		if(ob.materials[mat_index].uniform_buf_ob.nonNull())
		{
			PhongUniforms uniforms;
			setUniformsForPhongProg(ob.materials[mat_index], *ob.mesh_data, uniforms);
			ob.materials[mat_index].uniform_buf_ob->updateData(0, &uniforms, sizeof(PhongUniforms));
		}
#endif
	}
}


#if 0
void OpenGLEngine::addObjectAndLoadTexturesImmediately(const Reference<GLObject>& object)
{
	for(size_t i=0; i<object->materials.size(); ++i)
	{
		if(!object->materials[i].tex_path.empty())
			object->materials[i].albedo_texture = getTexture(object->materials[i].tex_path);
	
		if(!object->materials[i].metallic_roughness_tex_path.empty())
			object->materials[i].metallic_roughness_texture = getTexture(object->materials[i].metallic_roughness_tex_path);

		if(!object->materials[i].normal_map_path.empty())
			object->materials[i].normal_map = getTexture(object->materials[i].normal_map_path);

		if(!object->materials[i].emission_tex_path.empty())
			object->materials[i].emission_texture = getTexture(object->materials[i].emission_tex_path);
	}

	addObject(object);
}
#endif


void OpenGLEngine::addOverlayObject(const Reference<OverlayObject>& object)
{
	assert(object->ob_to_world_matrix.e[14] > -10000.0);
	assert(isFinite(object->ob_to_world_matrix.e[14]));

	// Check object material indices are in-bounds.
	for(size_t i=0; i<object->mesh_data->batches.size(); ++i)
		if(object->mesh_data->batches[i].material_index >= 1)
			throw glare::Exception("GLObject material_index is out of bounds.");

	current_scene->overlay_objects.insert(object);
	object->material.shader_prog = overlay_prog;
}


void OpenGLEngine::removeOverlayObject(const Reference<OverlayObject>& object)
{
	const size_t num_removed = current_scene->overlay_objects.erase(object);
	if(num_removed == 0)
	{
		conPrint("OpenGLEngine::removeOverlayObject(): Warning: no such object in scene, did not remove anything");
		assert(0);
	}
}


void OpenGLEngine::addLight(GLLightRef light)
{
	current_scene->lights.insert(light);

	// Alloc spot in uniform buffer
	int free_index;
	auto it = light_free_indices.begin();
	if(it == light_free_indices.end())
		free_index = -1; // Out of free spaces
	else
	{
		free_index = *it;
		light_free_indices.erase(it);
	}

	if(free_index >= 0)
	{
		if(light_buffer.nonNull())
			light_buffer->updateData(sizeof(LightGPUData) * free_index, &light->gpu_data, sizeof(LightGPUData));
		else
		{
			assert(light_ubo.nonNull());
			light_ubo->updateData(sizeof(LightGPUData) * free_index, &light->gpu_data, sizeof(LightGPUData));
		}
	}
	light->buffer_index = free_index;

	const js::AABBox light_aabb = computelightAABB(*light);
	light->aabb_ws = light_aabb;

	current_scene->light_grid.insert(light.ptr(), light_aabb);

	rebuildObjectLightInfoForNewLight(*light);

	// addDebugAABB(light_aabb, Colour4f(1.f, 0.2f, 0.2f, 0.2f));
}


void OpenGLEngine::removeLight(GLLightRef light)
{
	current_scene->lights.erase(light);

	if(light->buffer_index >= 0)
	{
		light_free_indices.insert(light->buffer_index);
		light->buffer_index = -1;
	}

	current_scene->light_grid.remove(light.ptr(), light->aabb_ws);
}


void OpenGLEngine::lightUpdated(GLLightRef light)
{
	if(light->buffer_index >= 0)
	{
		if(light_buffer.nonNull())
			light_buffer->updateData(sizeof(LightGPUData) * light->buffer_index, &light->gpu_data, sizeof(LightGPUData));
		else
		{
			assert(light_ubo.nonNull());
			light_ubo->updateData(sizeof(LightGPUData) * light->buffer_index, &light->gpu_data, sizeof(LightGPUData));
		}
	}
}


void OpenGLEngine::setLightPos(GLLightRef light, const Vec4f& new_pos)
{
	const js::AABBox old_aabb_ws = light->aabb_ws;
	light->gpu_data.pos = new_pos;
	const js::AABBox new_aabb_ws = computelightAABB(*light);
	light->aabb_ws = new_aabb_ws;

	// See if the object has changed grid cells
	const Vec4i old_min_bucket_i = current_scene->light_grid.bucketIndicesForPoint(old_aabb_ws.min_);
	const Vec4i old_max_bucket_i = current_scene->light_grid.bucketIndicesForPoint(old_aabb_ws.max_);

	const Vec4i new_min_bucket_i = current_scene->light_grid.bucketIndicesForPoint(new_aabb_ws.min_);
	const Vec4i new_max_bucket_i = current_scene->light_grid.bucketIndicesForPoint(new_aabb_ws.max_);

	if(new_min_bucket_i != old_min_bucket_i || new_max_bucket_i != old_max_bucket_i)
	{
		// cells have changed.
		current_scene->light_grid.remove(light.ptr(), old_aabb_ws);
		current_scene->light_grid.insert(light.ptr(), new_aabb_ws);
	}

	if(light->buffer_index >= 0)
	{
		if(light_buffer.nonNull())
			light_buffer->updateData(sizeof(LightGPUData) * light->buffer_index, &light->gpu_data, sizeof(LightGPUData));
		else
		{
			assert(light_ubo.nonNull());
			light_ubo->updateData(sizeof(LightGPUData) * light->buffer_index, &light->gpu_data, sizeof(LightGPUData));
		}
	}
}


void OpenGLEngine::selectObject(const Reference<GLObject>& object)
{
	this->selected_objects.insert(object.getPointer());
}


void OpenGLEngine::setSelectionOutlineColour(const Colour4f& col)
{
	outline_colour = col;
}


void OpenGLEngine::setSelectionOutlineWidth(float line_width_px)
{
	assert(line_width_px >= 0);

	outline_width_px = line_width_px;
}


void OpenGLEngine::deselectObject(const Reference<GLObject>& object)
{
	this->selected_objects.erase(object.getPointer());
}


void OpenGLEngine::deselectAllObjects()
{
	this->selected_objects.clear();
}


void OpenGLEngine::removeObject(const Reference<GLObject>& object)
{
	size_t num_removed = current_scene->objects.erase(object);
	current_scene->transparent_objects.erase(object);
	current_scene->materialise_objects.erase(object);
	num_removed += current_scene->always_visible_objects.erase(object); // Object will either be in objects or always_visible_objects.
	current_scene->animated_objects.erase(object);
	current_scene->water_objects.erase(object);
	current_scene->decal_objects.erase(object);
	current_scene->alpha_blended_objects.erase(object);
	selected_objects.erase(object.getPointer());

	if(use_ob_and_mat_data_gpu_resident)
	{
		for(size_t i=0; i<object->materials.size(); ++i)
		{
			// Deallocate material data: add material data index back to set of free indices.
			if(object->materials[i].material_data_index >= 0)
				phong_buffer_free_indices.insert(object->materials[i].material_data_index);
			object->materials[i].material_data_index = -1;
		}

		// Deallocate per-ob data: add object data index back to set of free indices.
		if(object->per_ob_vert_data_index >= 0)
		{
			this->per_ob_vert_data_free_indices.insert(object->per_ob_vert_data_index);
			object->per_ob_vert_data_index = -1;
		}

		// Deallocate joint matrices
		if(object->joint_matrices_block)
		{
			this->joint_matrices_allocator->free(object->joint_matrices_block);
			object->joint_matrices_block = NULL;
			object->joint_matrices_base_index = -1;
		}
	}

	if(num_removed == 0)
	{
		//conPrint("OpenGLEngine::removeObject(): Warning: no such object in scene, did not remove anything");
		//assert(0);
	}
}


bool OpenGLEngine::isObjectAdded(const Reference<GLObject>& object) const
{
	return 
		current_scene->objects.contains(object) || 
		current_scene->always_visible_objects.contains(object); // Always-visible objects aren't in objects set.
}


void OpenGLEngine::newMaterialUsed(OpenGLMaterial& mat, bool use_vert_colours, bool uses_instancing, bool uses_skinning, bool use_vert_tangents, bool position_w_is_oct16_normal)
{
	assignShaderProgToMaterial(mat,
		use_vert_colours,
		uses_instancing,
		uses_skinning,
		use_vert_tangents,
		position_w_is_oct16_normal
	);
}


void OpenGLEngine::objectMaterialsUpdated(GLObject& object)
{
	// Add this object to transparent_objects list if it has a transparent material and is not already in the list.
	assert(object.materials.size() > 0);

	bool have_transparent_mat = false;
	bool have_materialise_effect = false;
	for(size_t i=0; i<object.materials.size(); ++i)
	{
		OpenGLMaterial& mat = object.materials[i];

		assignShaderProgToMaterial(mat, object.mesh_data->has_vert_colours, /*uses instancing=*/object.instance_matrix_vbo.nonNull(), object.mesh_data->usesSkinning(), object.mesh_data->has_vert_tangents, 
			object.mesh_data->position_w_is_oct16_normal);
		have_transparent_mat = have_transparent_mat || mat.transparent;
		have_materialise_effect = have_materialise_effect || mat.materialise_effect;

		mat.uniform_flags = computeUniformFlagsForMat(mat, *object.mesh_data);
	}

	if(have_transparent_mat)
		current_scene->transparent_objects.insert(&object);
	else
		current_scene->transparent_objects.erase(&object); // Remove from transparent material list if it is currently in there.

	if(have_materialise_effect)
		current_scene->materialise_objects.insert(&object);
	else
		current_scene->materialise_objects.erase(&object); // Remove from materialise effect object list if it is currently in there.


	// Update material data on GPU
	{
		for(size_t i=0; i<object.materials.size(); ++i)
			updateMaterialDataOnGPU(object, i);
	}

	rebuildObjectDepthDrawBatches(object); // Since material alpha may have changed, need to recompute depth-draw batches
	rebuildDenormalisedDrawData(object);
}


void OpenGLEngine::updateAllMaterialDataOnGPU(GLObject& object) // Don't reassign shaders, just upload material data to GPU for each material
{
	for(size_t i=0; i<object.materials.size(); ++i)
		updateMaterialDataOnGPU(object, i);
}


// If a GLObject is inserted in the world, then shader progs will be assigned to the materials.
// Assigned shader progs are required for rebuildDenormalisedDrawData().
static bool areShadersAssigned(const GLObject& ob)
{
	ZoneScoped; // Tracy profiler

	for(size_t i=0; i<ob.materials.size(); ++i)
		if(ob.materials[i].shader_prog.isNull())
			return false;
	return true;
}


void OpenGLEngine::materialTextureChanged(GLObject& ob, OpenGLMaterial& mat)
{
	ZoneScoped; // Tracy profiler

	if(areShadersAssigned(ob))
	{
		mat.uniform_flags = computeUniformFlagsForMat(mat, *ob.mesh_data);

		if(mat.material_data_index >= 0) // Object may have not been inserted into engine yet, so material_data_index might not have been set yet.
		{
			assert(use_ob_and_mat_data_gpu_resident);

			PhongUniforms uniforms;
			setUniformsForPhongProg(mat, *ob.mesh_data, uniforms);

			phong_buffer->updateData(/*dest offset=*/mat.material_data_index * sizeof(PhongUniforms), /*src data=*/&uniforms, /*src size=*/sizeof(PhongUniforms));
		}

		// Since texture may have changed from one with alpha to one without, or vice-versa, we may need to assign a new shader.
		assignShaderProgToMaterial(mat, ob.mesh_data->has_vert_colours, /*uses instancing=*/ob.instance_matrix_vbo.nonNull(), ob.mesh_data->usesSkinning(), ob.mesh_data->has_vert_tangents, ob.mesh_data->position_w_is_oct16_normal);

		rebuildObjectDepthDrawBatches(ob);
		rebuildDenormalisedDrawData(ob);

#if UNIFORM_BUF_PER_MAT_SUPPORT
		if(mat.uniform_buf_ob.nonNull())
		{
			PhongUniforms uniforms;
			setUniformsForPhongProg(mat, *ob.mesh_data, uniforms);
			mat.uniform_buf_ob->updateData(0, &uniforms, sizeof(PhongUniforms));
		}
#endif
	}
}


void OpenGLEngine::objectBatchDataChanged(GLObject& object)
{
	rebuildObjectDepthDrawBatches(object);
	rebuildDenormalisedDrawData(object);
}


// Return an OpenGL texture based on tex_path.  Loads it from disk if needed.  Blocking.
// Throws glare::Exception
Reference<OpenGLTexture> OpenGLEngine::getTexture(const std::string& tex_path, const TextureParams& params)
{
	try
	{
		const std::string tex_key = this->texture_server.nonNull() ? this->texture_server->keyForPath(tex_path) : tex_path;

		const OpenGLTextureKey texture_key(tex_key);

		// If the OpenGL texture for this path has already been created, return it.
		auto res = this->opengl_textures.find(texture_key);
		if(res != this->opengl_textures.end())
		{
			// If there was only one reference to the texture, it was the reference from the opengl_textures map, so it was unused.  Mark it as used.
			if(res->second.value->getRefCount() == 1)
				this->textureBecameUsed(res->second.value.ptr());
			return res->second.value;
		}

		
		Reference<Map2D> map;
		if(texture_server.nonNull())
		{
			map = texture_server->getTexForPath(".", tex_path);// TEMP HACK: need to set base dir here
		}
		else
		{
			ImFormatDecoder::ImageDecodingOptions options;
			options.ETC_support = this->texture_compression_ETC_support;

			map = ImFormatDecoder::decodeImage(".", tex_path, options);
		}

		Reference<OpenGLTexture> opengl_tex = this->getOrLoadOpenGLTextureForMap2D(texture_key, *map, params);

		return opengl_tex;
	}
	catch(TextureServerExcep& e)
	{
		throw glare::Exception(e.what());
	}
}


// If the texture identified by key has been loaded into OpenGL, then return the OpenGL texture.
// If the texture is not loaded, return a null reference.
Reference<OpenGLTexture> OpenGLEngine::getTextureIfLoaded(const OpenGLTextureKey& texture_key)
{
	auto res = this->opengl_textures.find(texture_key);
	if(res != this->opengl_textures.end())
	{
		if(res->second.value->getRefCount() == 1)
			this->textureBecameUsed(res->second.value.ptr());
		return res->second.value;
	}
	else
		return Reference<OpenGLTexture>();
}


// Initially based on http://iquilezles.org/www/articles/frustumcorrect/frustumcorrect.htm, but improved.
static inline bool AABBIntersectsFrustum(const Planef* frustum_clip_planes, int num_frustum_clip_planes, const js::AABBox& frustum_aabb, const js::AABBox& aabb_ws)
{
	if(frustum_aabb.disjoint(aabb_ws) != 0) // If the frustum AABB is disjoint from the object AABB:
		return false;
	// Do this test first since we can cull a lot of objects cheaply (about 8 cycles for disjoint()) with this test.


	const Vec4f min_ws = aabb_ws.min_;
	const Vec4f max_ws = aabb_ws.max_;

	// For each frustum plane, check if AABB is completely on front side of plane, if so, then the AABB
	// does not intersect the frustum.
	// We'll do this by checking each vertex of the AABB against the plane (in parallel with SSE).
	for(int z=0; z<num_frustum_clip_planes; ++z) 
	{
#ifndef NDEBUG
		float refdist;
		{
			const Vec4f normal = frustum_clip_planes[z].getNormal();
			float dist = std::numeric_limits<float>::infinity();
			dist = myMin(dist, dot(normal, Vec4f(min_ws[0], min_ws[1], min_ws[2], 1.f)));
			dist = myMin(dist, dot(normal, Vec4f(min_ws[0], min_ws[1], max_ws[2], 1.f)));
			dist = myMin(dist, dot(normal, Vec4f(min_ws[0], max_ws[1], min_ws[2], 1.f)));
			dist = myMin(dist, dot(normal, Vec4f(min_ws[0], max_ws[1], max_ws[2], 1.f)));
			dist = myMin(dist, dot(normal, Vec4f(max_ws[0], min_ws[1], min_ws[2], 1.f)));
			dist = myMin(dist, dot(normal, Vec4f(max_ws[0], min_ws[1], max_ws[2], 1.f)));
			dist = myMin(dist, dot(normal, Vec4f(max_ws[0], max_ws[1], min_ws[2], 1.f)));
			dist = myMin(dist, dot(normal, Vec4f(max_ws[0], max_ws[1], max_ws[2], 1.f)));
			//if(dist >= frustum_clip_planes[z].getD())
			//	return false;
			refdist = dist;
		}
	//	const float refD = frustum_clip_planes[z].getD();
#endif
		
		const Vec4f normal = frustum_clip_planes[z].getNormal();
		const Vec4f normal_x = copyToAll<0>(normal);
		const Vec4f normal_y = copyToAll<1>(normal);
		const Vec4f normal_z = copyToAll<2>(normal);

		const Vec4f min_x(copyToAll<0>(min_ws)); // [min_ws[0], min_ws[0], min_ws[0], min_ws[0]]
		const Vec4f max_x(copyToAll<0>(max_ws)); // [max_ws[0], max_ws[0], max_ws[0], max_ws[0]]

		const Vec4f py(_mm_shuffle_ps(min_ws.v, max_ws.v, _MM_SHUFFLE(1, 1, 1, 1))); // [min_ws[1], min_ws[1], max_ws[1], max_ws[1]]
		const Vec4f pz_(_mm_shuffle_ps(min_ws.v, max_ws.v, _MM_SHUFFLE(2, 2, 2, 2))); // [min_ws[2], min_ws[2], max_ws[2], max_ws[2]]
		const Vec4f pz(_mm_shuffle_ps(pz_.v, pz_.v, _MM_SHUFFLE(2, 0, 2, 0))); // [min_ws[2], max_ws[2], min_ws[2], max_ws[2]]

		const Vec4f dot1 = mul(normal_x, min_x) + (mul(normal_y, py) + mul(normal_z, pz));
		const Vec4f dot2 = mul(normal_x, max_x) + (mul(normal_y, py) + mul(normal_z, pz));

		const Vec4f dist = min(dot1, dot2); // smallest distances
		const Vec4f greaterv = _mm_cmpge_ps(dist.v, Vec4f(frustum_clip_planes[z].getD()).v); // distances >= plane.D ?
		const int greater = _mm_movemask_ps(greaterv.v);
	//	assert((refdist >= refD) == (greater == 15));
		if(greater == 15) // if all distances >= plane.D:
			return false;
	}

	return true;
}


bool OpenGLEngine::isObjectInCameraFrustum(const GLObject& ob)
{
	return AABBIntersectsFrustum(current_scene->frustum_clip_planes, current_scene->num_frustum_clip_planes, current_scene->frustum_aabb, ob.aabb_ws);
}


Vec4f OpenGLEngine::getCameraPositionWS() const
{
	return current_scene->cam_to_world.getColumn(3);
}


bool OpenGLEngine::getWindowCoordsForWSPos(const Vec4f& pos_ws, Vec2f& coords_out) const
{
	const Vec4f pos_cs = current_scene->world_to_camera_space_matrix * pos_ws;

	if(current_scene->camera_type == OpenGLScene::CameraType_Perspective)
	{
		const float right_ratio = pos_cs[0] / pos_cs[1];
		const float up_ratio    = pos_cs[2] / pos_cs[1];

		// NOTE: ignoring lens shift here.
		const float frac_right = right_ratio / current_scene->use_sensor_width  * current_scene->lens_sensor_dist;
		const float frac_up    = up_ratio    / current_scene->use_sensor_height * current_scene->lens_sensor_dist;

		coords_out.x = (0.5f + frac_right) * main_viewport_w;
		coords_out.y = (0.5f - frac_up)    * main_viewport_h;
	}
	else
	{
		// TODO
		assert(0);
		coords_out = Vec2f(0.f);
	}

	return pos_cs[1] > 0;
}


// If prog is not the current bound/used program, then bind it, and update current_bound_prog.
// Returns true if prog was not the current bound program, and hence the currently bound program changed.
bool OpenGLEngine::checkUseProgram(const OpenGLProgram* prog)
{
	if(prog != current_bound_prog)
	{
		ZoneScopedN("changing prog"); // Tracy profiler

		// Flush existing draw commands
		if(use_multi_draw_indirect)
			submitBufferedDrawCommands();

		// conPrint("---- checkUseProgram(): Changed to program " + prog->prog_name + " ----");

		prog->useProgram();
		current_bound_prog = prog;
		current_bound_prog_index = prog->program_index;

		current_uniforms_ob = NULL; // Program has changed, so we need to set object uniforms for the current program.

		num_prog_changes++;

		return true;
	}
	else
		return false;
}


bool OpenGLEngine::checkUseProgram(uint32 prog_index)
{
	if(prog_index != current_bound_prog_index)
	{
		ZoneScopedN("changing prog"); // Tracy profiler

		// Flush existing draw commands
		if(use_multi_draw_indirect)
			submitBufferedDrawCommands();

		const OpenGLProgram* prog = this->prog_vector[prog_index].ptr();

		// conPrint("---- checkUseProgram(): Changed to program " + prog->prog_name + " ----");

		assert(prog->isBuilt());

		prog->useProgram();
		current_bound_prog = prog;
		current_bound_prog_index = prog_index;

		current_uniforms_ob = NULL; // Program has changed, so we need to set object uniforms for the current program.

		num_prog_changes++;

		return true;
	}
	else
		return false;
}


// non-instancing binding
void OpenGLEngine::bindMeshData(const OpenGLMeshRenderData& mesh_data)
{
	assert(mesh_data.vbo_handle.valid());

	// Check current_bound_VAO is correct
#ifndef NDEBUG
	{
		GLuint vao = VAO::getBoundVAO();
		if(current_bound_VAO == NULL)
			runtimeCheck(vao == 0);
		else
			runtimeCheck(current_bound_VAO->handle == vao);
	}
#endif

#if DO_INDIVIDUAL_VAO_ALLOC
	VAO* vao = mesh_data.individual_vao.ptr();

	// Bind new VAO (if changed)
	if(current_bound_VAO != vao)
	{
		// conPrint("Binding to VAO " + toString(vao->handle));

		vao->bindVertexArray();
		current_bound_VAO = vao;
		num_vao_binds++;
	}

	// Check the correct buffers are still bound
	assert(mesh_data.individual_vao->getBoundVertexBuffer(0) == mesh_data.vbo_handle.vbo->bufferName());
	assert(mesh_data.individual_vao->getBoundIndexBuffer() == mesh_data.indices_vbo_handle.index_vbo->bufferName());
#else

	VertexBufferAllocator::VAOData& vao_data = vert_buf_allocator->vao_data[mesh_data.vao_data_index];

	// Get the buffers we want to use for this batch.
	const GLenum index_type = mesh_data.getIndexType();
	      VAO* vao = vao_data.vao.ptr();
	const VBO* vert_data_vbo = mesh_data.vbo_handle.vbo.ptr();
	const VBO* index_vbo = mesh_data.indices_vbo_handle.index_vbo.ptr();

	
	if(index_type != current_index_type || vao != current_bound_VAO)
	{
		// A buffer or index type has changed.  submit queued draw commands, start queueing new commands.
		if(use_multi_draw_indirect)
			submitBufferedDrawCommands();

		current_index_type = index_type;

		// Bind new VAO (if changed)
		if(current_bound_VAO != vao)
		{
			// conPrint("Binding to VAO " + toString(vao->handle));

			vao->bindVertexArray();
			current_bound_VAO = vao;
			num_vao_binds++;
		}
	}

	assert(current_bound_VAO == vao);

	if(vao->current_bound_vert_vbo != vert_data_vbo) // If the VAO is bound to the wrong vertex buffer:
	{
		// Submit draw commands using old vertex buffer, before we bind a new one.
		if(use_multi_draw_indirect)
			submitBufferedDrawCommands();

		// Bind vertex data VBO to the VAO
		glBindVertexBuffer(
			0, // binding point index
			vert_data_vbo->bufferName(), // buffer
			0, // offset - offset of the first element within the buffer
			vao->vertex_spec.attributes[0].stride // stride
		);

		vao->current_bound_vert_vbo = vert_data_vbo;
		num_vbo_binds++;
	}

	if(vao->current_bound_index_VBO != index_vbo) // If the VAO is bound to the wrong index buffer:
	{
		// Submit draw commands using old index buffer, before we bind a new one.
		if(use_multi_draw_indirect)
			submitBufferedDrawCommands();

		// Bind index VBO to the VAO
		index_vbo->bind();

		vao->current_bound_index_VBO = index_vbo;
		num_index_buf_binds++;
	}
#endif
}


void OpenGLEngine::bindMeshData(const GLObject& ob)
{
	VAO* vao = ob.vao;

#if DO_INDIVIDUAL_VAO_ALLOC

	assert(vao == (ob.vert_vao.nonNull() ? ob.vert_vao.ptr() : ob.mesh_data->individual_vao.ptr()));

	// Bind new VAO (if changed)
	if(current_bound_VAO != vao)
	{
		// conPrint("Binding to VAO " + toString(vao->handle));

		// A buffer or index type has changed.  submit queued draw commands, start queueing new commands.
		if(use_multi_draw_indirect)
			submitBufferedDrawCommands();

		vao->bindVertexArray();
		current_bound_VAO = vao;
		num_vao_binds++;
	}

	// Check the correct buffers are still bound
#ifndef NDEBUG
	if(ob.vert_vao.nonNull())
	{
		assert(ob.vert_vao->getBoundVertexBuffer(0) == ob.mesh_data->vbo_handle.vbo->bufferName());
		assert(ob.vert_vao->getBoundIndexBuffer()   == ob.mesh_data->indices_vbo_handle.index_vbo->bufferName());
	}
	else
	{
		assert(ob.mesh_data->individual_vao->getBoundVertexBuffer(0) == ob.mesh_data->vbo_handle.vbo->bufferName());
		assert(ob.mesh_data->individual_vao->getBoundIndexBuffer()   == ob.mesh_data->indices_vbo_handle.index_vbo->bufferName());
	}
#endif

	const GLenum index_type = ob.index_type_and_log2_size & 0xFFFF;
	if(index_type != current_index_type)
	{
		// A buffer or index type has changed.  submit queued draw commands, start queueing new commands.
		if(use_multi_draw_indirect)
			submitBufferedDrawCommands();

		current_index_type = index_type;
	}
#else

	// Check to see if we should use the object's VAO that has the instance matrix stuff

	assert(vao == (ob.vert_vao.nonNull() ? ob.vert_vao.ptr() : vert_buf_allocator->vao_data[ob.mesh_data->vao_data_index].vao.ptr()));

	// Get the buffers we want to use for this batch.
	const GLenum index_type = (GLenum)(ob.index_type_and_log2_size & 0xFFFF);
	const VBO* vert_data_vbo = ob.vert_vbo;
	const VBO* index_vbo = ob.index_vbo;

	assert(index_type == ob.mesh_data->getIndexType());
	assert(vert_data_vbo == ob.mesh_data->vbo_handle.vbo.ptr());
	assert(index_vbo == ob.mesh_data->indices_vbo_handle.index_vbo.ptr());
	assert(ob.instance_matrix_vbo_name == (ob.instance_matrix_vbo.nonNull() ? ob.instance_matrix_vbo->bufferName() : 0));

	// For instancing, always just start a new draw command, for now.
	if(use_multi_draw_indirect && (ob.instance_matrix_vbo_name != 0))
		submitBufferedDrawCommands();

	if(index_type != current_index_type || vao != current_bound_VAO)
	{
		// A buffer or index type has changed.  submit queued draw commands, start queueing new commands.
		if(use_multi_draw_indirect)
			submitBufferedDrawCommands();

		current_index_type = index_type;

		// Bind new VAO (if changed)
		if(current_bound_VAO != vao)
		{
			// conPrint("Binding to VAO " + toString(vao->handle) + " with index " + toString(ob.mesh_data->vao_data_index));

			vao->bindVertexArray();
			current_bound_VAO = vao;
			num_vao_binds++;
		}

		// Check current_bound_vertex_VBO is correct etc..
#ifndef NDEBUG
		/*{
			if(current_bound_vertex_VBO != NULL)
			{
				runtimeCheck(current_bound_vertex_VBO->bufferName() == vao->getBoundVertexBuffer(0));
			}

			if(current_bound_index_VBO != NULL)
			{
				runtimeCheck(current_bound_index_VBO->bufferName() == vao->getBoundIndexBuffer());
			}
		}*/
#endif
	}

	assert(current_bound_VAO == vao);

	if(vao->current_bound_vert_vbo != vert_data_vbo) // If the VAO is bound to the wrong vertex buffer:
	{
		// conPrint("Binding to vert VBO " + toString(vert_data_vbo->bufferName()));

		// Submit draw commands using old vertex buffer, before we bind a new one.
		if(use_multi_draw_indirect)
			submitBufferedDrawCommands();

		// Bind vertex data VBO to the VAO
		glBindVertexBuffer(
			0, // binding point index
			vert_data_vbo->bufferName(), // buffer
			0, // offset - offset of the first element within the buffer
			vao->vertex_spec.attributes[0].stride // stride
		);

		vao->current_bound_vert_vbo = vert_data_vbo;
		num_vbo_binds++;
	}

	if(vao->current_bound_index_VBO != index_vbo) // If the VAO is bound to the wrong index buffer:
	{
		// conPrint("Binding to index VBO " + toString(vert_data_vbo->bufferName()));

		// Submit draw commands using old index buffer, before we bind a new one.
		if(use_multi_draw_indirect)
			submitBufferedDrawCommands();

		// Bind index VBO to the VAO
		index_vbo->bind();

		vao->current_bound_index_VBO = index_vbo;
		num_index_buf_binds++;
	}


	// Bind instancing vertex buffer, if used
	if(ob.instance_matrix_vbo.nonNull())
	{
		assert(ob.instance_matrix_vbo_name == ob.instance_matrix_vbo->bufferName());

		glBindVertexBuffer(
			1, // binding point index
			ob.instance_matrix_vbo_name, // buffer
			0, // offset - offset of the first element within the buffer
			sizeof(Matrix4f) // stride
		);
	}
#endif
}


// http://www.manpagez.com/man/3/glFrustum/
static const Matrix4f frustumMatrix(GLdouble left,
	GLdouble right,
	GLdouble bottom,
	GLdouble top,
	GLdouble zNear, // zNear, zFar: Specify the distances to the near and far depth clipping planes.  Both distances must be positive.
	GLdouble zFar)
{
	double A = (right + left) / (right - left);
	double B = (top + bottom) / (top - bottom);
	double C = - (zFar + zNear) / (zFar - zNear);
	double D = - (2 * zFar * zNear) / (zFar - zNear);

	float e[16] = {
		(float)(2*zNear / (right - left)), 0, (float)A, 0,
		0, (float)(2*zNear / (top - bottom)), (float)B, 0,
		0, 0, (float)C, (float)D,
		0, 0, -1.f, 0 };

	return Matrix4f::fromRowMajorData(e);
}


// Like frustumMatrix(), but with an infinite zFar.
// This is supposedly more accurate in terms of precision - see 'Tightening the Precision of Perspective Rendering', http://www.geometry.caltech.edu/pubs/UD12.pdf
static const Matrix4f infiniteFrustumMatrix(GLdouble left,
	GLdouble right,
	GLdouble bottom,
	GLdouble top,
	GLdouble zNear) // Distance to near clipping plane (positive)
{
	double A = (right + left) / (right - left);
	double B = (top + bottom) / (top - bottom);
	//     C = -1 = -(zFar + zNear) / (zFar - zNear) as zFar goes to infinity:
	double D = -2 * zNear; // = -(2 * zFar * zNear) / (zFar - zNear) as zFar goes to infinity:

	float e[16] = {
		(float)(2*zNear / (right - left)), 0, (float)A, 0,
		0, (float)(2*zNear / (top - bottom)), (float)B, 0,
		0, 0, -1.f, (float)D,
		0, 0, -1.f, 0 };

	return Matrix4f::fromRowMajorData(e);
}


// https://msdn.microsoft.com/en-us/library/windows/desktop/dd373965(v=vs.85).aspx
static const Matrix4f orthoMatrix(GLdouble left,
	GLdouble right,
	GLdouble bottom,
	GLdouble top,
	GLdouble zNear,
	GLdouble zFar)
{
	const float t_x = (float)(-(right + left) / (right - left));
	const float t_y = (float)(-(top + bottom) / (top - bottom));
	const float t_z = (float)(-(zFar + zNear) / (zFar - zNear));

	float e[16] = {
		(float)(2 / (right - left)), 0,                           0,                            t_x,
		0,                           (float)(2 / (top - bottom)), 0,                            t_y,
		0,                           0,                           (float)(-2 / (zFar - zNear)), t_z,
		0,                           0,                           0,                            1 
	};
	
	return Matrix4f::fromRowMajorData(e);
}


static const Matrix4f diagonalOrthoMatrix(GLdouble left,
	GLdouble right,
	GLdouble bottom,
	GLdouble top,
	GLdouble zNear, // positive
	GLdouble zFar,
	double cam_z) // positive
{
	const float t_z = (float)(-(zFar + zNear) / (zFar - zNear));

	// x' = 2x / (right - left) - slope * z  + t_x   = 2x / (right - left) - z / ( right - left) + t_x = (2x - z) / (right - left) + t_x
	// y' = 2y / (top - bottom) + slope * z  + t_y
	// z' = -2z / (far - near) + t_z

	// We want x' = 0 for x = 0 and z = -cam_z
	// x' = (2x - z) / (right - left) + t_x
	// 0 = (0 - (-cam_z)) / (right - left) + t_x
	// 0 = cam_z / (right - left) + t_x
	// t_x = -cam_z / (right - left)
	const float t_x = (float)(-cam_z / (right - left));
	const float t_y = (float)( cam_z / (top - bottom));

	const float slope = (float)(1.0 / (right - left));

	const float e[16] = {
		(float)(2 / (right - left)),  0,                           -slope,                        t_x,
		0,                            (float)(2 / (top - bottom)),  slope,                        t_y,
		0,                            0,                            (float)(-2 / (zFar - zNear)), t_z,
		0,                            0,                            0,                            1
	};

	return Matrix4f::fromRowMajorData(e);
}


void OpenGLEngine::addDebugSphere(const Vec4f& centre, float radius, const Colour4f& col)
{
	GLObjectRef ob = makeSphereObject(radius, col);
	ob->ob_to_world_matrix = Matrix4f::translationMatrix(centre) * Matrix4f::uniformScaleMatrix(radius);
	debug_draw_obs.push_back(ob);
	addObject(ob);
}


void OpenGLEngine::addDebugAABB(const js::AABBox& aabb, const Colour4f& col)
{
	GLObjectRef ob = makeAABBObject(aabb.min_, aabb.max_, col);
	debug_draw_obs.push_back(ob);
	addObject(ob);
}


void OpenGLEngine::addDebugLine(const Vec4f& start_point, const Vec4f& end_point, float radius, const Colour4f& col)
{
	GLObjectRef ob = makeCylinderObject(radius, col);
	ob->ob_to_world_matrix = Matrix4f::translationMatrix(start_point) * 
		Matrix4f::constructFromVectorStatic(normalise(end_point - start_point)) *
		Matrix4f::scaleMatrix(radius, radius, start_point.getDist(end_point));
	debug_draw_obs.push_back(ob);
	addObject(ob);
}


void OpenGLEngine::addDebugPlane(const Vec4f& point_on_plane, const Vec4f& plane_normal, float plane_draw_width, const Colour4f& col)
{
	// Add arrow
	debug_draw_obs.push_back(makeArrowObject(point_on_plane, point_on_plane + plane_normal * plane_draw_width * 0.2f, 
		Colour4f(col[0], col[1], col[2], 1), /*radius scale=*/plane_draw_width * 0.2f));
	addObject(debug_draw_obs.back());

	// Add plane quad
	GLObjectRef ob = allocateObject();
	ob->mesh_data = getUnitQuadMeshData(); // A quad from (0, 0, 0) to (1, 1, 0)
	ob->materials.resize(1);
	ob->materials[0].albedo_linear_rgb = Colour3f(col[0], col[1], col[2]);
	ob->materials[0].alpha = col[3];
	ob->materials[0].transparent = col[3] < 1.f;
	ob->materials[0].simple_double_sided = true;
	ob->ob_to_world_matrix = Matrix4f::translationMatrix(point_on_plane) * Matrix4f::constructFromVectorStatic(plane_normal) * 
		Matrix4f::uniformScaleMatrix(plane_draw_width) * Matrix4f::translationMatrix(-0.5f, -0.5f, 0);
	debug_draw_obs.push_back(ob);
	addObject(ob);
}


static inline float largestDim(const js::AABBox& aabb)
{
	return horizontalMax((aabb.max_ - aabb.min_).v);
}


// glDrawElementsBaseVertex is only in OpenGL ES 3.2: https://registry.khronos.org/OpenGL-Refpages/es3/html/glDrawElementsBaseVertex.xhtml
// However basevertex should always be zero when using Emscripten, since DO_INDIVIDUAL_VAO_ALLOC will be enabled (See VertexBufferAllocator.h), so we can just use glDrawElements.
GLARE_STRONG_INLINE static void drawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex)
{
#if defined(EMSCRIPTEN)
	assert(basevertex == 0);
	glDrawElements(mode, count, type, indices);
#else
	glDrawElementsBaseVertex(mode, count, type, indices, basevertex);
#endif
}


// glDrawElementsInstancedBaseVertex is only in OpenGL ES 3.2: https://registry.khronos.org/OpenGL-Refpages/es3/html/glDrawElementsInstancedBaseVertex.xhtml
// However basevertex should always be zero when using Emscripten, since DO_INDIVIDUAL_VAO_ALLOC will be enabled (See VertexBufferAllocator.h), so we can just use glDrawElementsInstanced.
GLARE_STRONG_INLINE static void drawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex)
{
#if defined(EMSCRIPTEN)
	assert(basevertex == 0);
	glDrawElementsInstanced(mode, count, type, indices, instancecount);
#else
	glDrawElementsInstancedBaseVertex(mode, count, type, indices, instancecount, basevertex);
#endif
}



// Draws a quad, with z value at far clip plane.  Clobbers depth func.
void OpenGLEngine::partiallyClearBuffer(const Vec2f& begin, const Vec2f& end)
{
	clear_buf_overlay_ob->ob_to_world_matrix =
		Matrix4f::translationMatrix(-1 + begin.x, -1 + begin.y, 1.f) * Matrix4f::scaleMatrix(2 * (end.x - begin.x), 2 * (end.y - begin.y), 1.f);

	glDepthFunc(GL_ALWAYS); // Do this to effectively disable z-test, but still have z writes.

	const OpenGLMaterial& opengl_mat = clear_buf_overlay_ob->material;

	checkUseProgram(opengl_mat.shader_prog.ptr());

	const OpenGLMeshRenderData& mesh_data = *clear_buf_overlay_ob->mesh_data;
	bindMeshData(mesh_data); // Bind the mesh data, which is the same for all batches.
	
	glUniformMatrix4fv(opengl_mat.shader_prog->model_matrix_loc, 1, false, clear_buf_overlay_ob->ob_to_world_matrix.e);

	drawElementsBaseVertex(GL_TRIANGLES, (GLsizei)mesh_data.batches[0].num_indices, mesh_data.getIndexType(), (void*)mesh_data.getBatch0IndicesTotalBufferOffset(), mesh_data.vbo_handle.base_vertex);
}


// Sorts objects into descending z order based on object-to-world z translation component.
struct OverlayObjectZComparator
{
	inline bool operator() (const OverlayObject* a, const OverlayObject* b) const
	{
		return a->ob_to_world_matrix.e[14] > b->ob_to_world_matrix.e[14];
	}
};


struct BatchDrawInfoGetKey
{
	inline uint32 operator () (const BatchDrawInfo& info) const { return info.prog_vao_key; }
};


void OpenGLEngine::sortBatchDrawInfos() 
{
	ZoneScopedN("sortBatchDrawInfos"); // Tracy profiler

	temp2_batch_draw_info.resizeNoCopy(temp_batch_draw_info.size());

	const int num_buckets = 6144; // As required by radixSort32BitKey().
	temp_counts.resize(num_buckets);
	
	Sort::radixSort32BitKey/*data=*/(temp_batch_draw_info.data(), /*working_space=*/temp2_batch_draw_info.data(), /*num_items=*/temp_batch_draw_info.size(), BatchDrawInfoGetKey(), temp_counts.data(), temp_counts.size());
}


void OpenGLEngine::flushDrawCommandsAndUnbindPrograms()
{
	if(use_multi_draw_indirect)
		submitBufferedDrawCommands();
	glActiveTexture(GL_TEXTURE0);
	OpenGLProgram::useNoPrograms();
	this->current_bound_prog = NULL;
	this->current_bound_prog_index = std::numeric_limits<uint32>::max();
}


struct BatchDrawInfoWithDistGetDistanceKey
{
	inline uint32 operator () (const BatchDrawInfoWithDist& info) const{ return info.dist; }
};


struct BatchDrawInfoWithDistGetKey
{
	inline uint32 operator () (const BatchDrawInfoWithDist& info) const { return info.prog_vao_key; }
};


void OpenGLEngine::sortBatchDrawInfoWithDists() 
{
	ZoneScopedN("sortBatchDrawInfoWithDists"); // Tracy profiler

	temp_batch_draw_info_dist.resizeNoCopy(batch_draw_info_dist.size());

	const int num_buckets = 6144; // As required by radixSort32BitKey().
	temp_counts.resize(num_buckets);

	// Sort first by distance
	Sort::radixSort32BitKey(/*data=*/batch_draw_info_dist.data(), /*working space=*/temp_batch_draw_info_dist.data(), /*num items=*/temp_batch_draw_info_dist.size(), 
		/*get key=*/BatchDrawInfoWithDistGetDistanceKey(), /*temp counts=*/temp_counts.data(), temp_counts.size());
	
	// Now sort by program etc.  NOTE: don't bother with this for now as currently we always use the same program
	// Sort::radixSort32BitKey(batch_draw_info_dist.data(), temp_batch_draw_info_dist.data(), temp_batch_draw_info_dist.size(), BatchDrawInfoWithDistGetKey(), temp_counts.data(), temp_counts.size());
}


// Returns num_clip_planes_used
// Because we are using a cross product to get near plane normals, we can't have multiple vertices at the same point.
static int computeShadowFrustumClipPlanes(const Vec4f frustum_verts_ws[8], const Vec4f& sun_dir, float max_shadowing_dist, Planef clip_planes_out[18])
{
	Vec4f center_p(0); // Compute point at centre of frustum
	for(int q=0; q<8; ++q)
		center_p += frustum_verts_ws[q];
	center_p *= (1 / 8.0f);

	// Compute normals of planes on each side of the view frustum
	const Vec4f top_n    = normalise(crossProduct(frustum_verts_ws[6] - frustum_verts_ws[2], frustum_verts_ws[3] - frustum_verts_ws[2]));
	const Vec4f bottom_n = normalise(crossProduct(frustum_verts_ws[0] - frustum_verts_ws[1], frustum_verts_ws[5] - frustum_verts_ws[1]));
	const Vec4f left_n   = normalise(crossProduct(frustum_verts_ws[7] - frustum_verts_ws[3], frustum_verts_ws[0] - frustum_verts_ws[3]));
	const Vec4f right_n  = normalise(crossProduct(frustum_verts_ws[5] - frustum_verts_ws[1], frustum_verts_ws[2] - frustum_verts_ws[1]));
	const Vec4f near_n   = normalise(crossProduct(frustum_verts_ws[2] - frustum_verts_ws[1], frustum_verts_ws[0] - frustum_verts_ws[1]));
	const Vec4f far_n    = normalise(crossProduct(frustum_verts_ws[4] - frustum_verts_ws[5], frustum_verts_ws[6] - frustum_verts_ws[5]));

	// Info about frustum edges
	const Vec4f edges[] = 
	{
		// normal of first adjacent plane, normal of second adjacent plane, vert on edge
		top_n,    left_n,   frustum_verts_ws[3], 
		left_n,   bottom_n, frustum_verts_ws[0],
		bottom_n, right_n,  frustum_verts_ws[1],
		right_n,  top_n,    frustum_verts_ws[2],
		near_n,   bottom_n, frustum_verts_ws[0],
		near_n,   right_n,  frustum_verts_ws[1],
		near_n,   top_n,    frustum_verts_ws[2],
		near_n,   left_n,   frustum_verts_ws[3],
		far_n,    bottom_n, frustum_verts_ws[4],
		far_n,    right_n,  frustum_verts_ws[5],
		far_n,    top_n,    frustum_verts_ws[6],
		far_n,    left_n,   frustum_verts_ws[7]
	};

	// Get clip planes on the faces of the frustum facing away from the sun, and from the faces facing the sun, which will be 
	// translated towards the sun by max_shadowing_dist.
	clip_planes_out[0] = Planef((dot(near_n,   sun_dir) > 0) ? (frustum_verts_ws[0] + sun_dir * max_shadowing_dist) : frustum_verts_ws[0], near_n);
	clip_planes_out[1] = Planef((dot(far_n,    sun_dir) > 0) ? (frustum_verts_ws[4] + sun_dir * max_shadowing_dist) : frustum_verts_ws[4], far_n);
	clip_planes_out[2] = Planef((dot(left_n,   sun_dir) > 0) ? (frustum_verts_ws[0] + sun_dir * max_shadowing_dist) : frustum_verts_ws[0], left_n);
	clip_planes_out[3] = Planef((dot(right_n,  sun_dir) > 0) ? (frustum_verts_ws[1] + sun_dir * max_shadowing_dist) : frustum_verts_ws[1], right_n);
	clip_planes_out[4] = Planef((dot(bottom_n, sun_dir) > 0) ? (frustum_verts_ws[0] + sun_dir * max_shadowing_dist) : frustum_verts_ws[0], bottom_n);
	clip_planes_out[5] = Planef((dot(top_n,    sun_dir) > 0) ? (frustum_verts_ws[3] + sun_dir * max_shadowing_dist) : frustum_verts_ws[3], top_n);

	// Get clip planes that lie on the 'extruded' edges that form the frustum silhouette as seen from the sun.
	int num_clip_planes_used = 6;
	for(int ei=0; ei<12; ++ei) // For each edge:
	{
		const Vec4f n_a = edges[ei*3 + 0]; // Normal of first face adjacent to edge
		const Vec4f n_b = edges[ei*3 + 1]; // Normal of second face adjacent to edge
		const Vec4f v   = edges[ei*3 + 2]; // Position of a vertex on edge.

		if(dot(n_a, sun_dir) * dot(n_b, sun_dir) < 0) // If edge is a silhouette edge:
		{
			const Vec4f edge_v = crossProduct(n_a, n_b); // Compute edge vector
			Vec4f plane_n = normalise(crossProduct(sun_dir, edge_v)); // Compute plane normal
			if(dot(plane_n, v - center_p) < 0) // If it's pointing inwards to the interior of the frustum, negate it.
				plane_n = -plane_n;

			clip_planes_out[num_clip_planes_used++] = Planef(v, plane_n);
		}
	}
	return num_clip_planes_used;
}


// Returns num_clip_planes_used
int OpenGLEngine::getCameraShadowMappingPlanesAndAABB(float near_dist, float far_dist, float max_shadowing_dist, Planef* shadow_clip_planes_out, js::AABBox& shadow_vol_aabb_out)
{
	assert(near_dist > 0);

	// Compute the 8 points making up the corner vertices of this slice of the view frustum
	Vec4f frustum_verts_ws[8];
	calcCamFrustumVerts(near_dist, far_dist, frustum_verts_ws);

	const int num_clip_planes_used = computeShadowFrustumClipPlanes(frustum_verts_ws, sun_dir, max_shadowing_dist, shadow_clip_planes_out);

	js::AABBox shadow_vol_aabb = js::AABBox::emptyAABBox(); // AABB of shadow rendering volume.
	for(int z=0; z<8; ++z)
	{
		shadow_vol_aabb.enlargeToHoldPoint(frustum_verts_ws[z]);
		shadow_vol_aabb.enlargeToHoldPoint(frustum_verts_ws[z] + sun_dir * max_shadowing_dist);
	}
	shadow_vol_aabb_out = shadow_vol_aabb;

	//if(add_debug_obs)
	//	addDebugVisForShadowFrustum(frustum_verts_ws, max_shadowing_dist, shadow_clip_planes_out, num_clip_planes_used);

	return num_clip_planes_used;
}


Matrix4f OpenGLEngine::getReverseZMatrixOrIdentity() const
{
	if(use_reverse_z)
	{
		// The usual OpenGL projection matrix and w divide maps z values to [-1, 1] after the w divide.  See http://www.songho.ca/opengl/gl_projectionmatrix.html
		// (Fragments on the near plane get mapped to -1, fragments on the far plane get mapped to +1).
		// Use a 'z-reversal matrix', to change this mapping to [0, 1] while reversing values.  (e.g. near plane gets mapped to +1, far plane gets mapped to 0).
		// See https://dev.theomader.com/depth-precision/ for details.
		// Maps (0, 0, -n, n) to (0, 0, n, n) (e.g. z=-n to +n, or +1 after divide by w) .  z=-n is near plane, (0, 0, -n, n) is result of proj_matrix * (0,0,-n,1)
		// Maps (0, 0, f, f) to (0, 0, 0, f) (e.g. z=+f to 0).  z=-f is far plane, (0, 0, f, f) is result of proj_matrix * (0,0,-f,1)
		const Matrix4f reverse_matrix(
			Vec4f(1, 0, 0, 0), // col 0
			Vec4f(0, 1, 0, 0), // col 1
			Vec4f(0, 0, -0.5f, 0), // col 2
			Vec4f(0, 0, 0.5f, 1) // col 3
		);
		return reverse_matrix;
	}
	else
		return Matrix4f::identity();
}

/*
Calculations for recovering depth values from depth buffer
----------------------------------------------------------
let 
raw_proj_matrix = 
(2n(r-l)    0         A       0)
(0          2n(t-b)   B       0)
(0          0         -1     -2n)
(0          0         -1      0)

[From infiniteFrustumMatrix()]

and
v' = raw_proj_matrix v

where v = (x, y, z, w), v' = (x', y', z', w')

Then
z' = -z -2nw   = -z - 2n
w' = -z


Case where we are using a reverse-z depth buffer:
-------------------------------------------------
let 
reverse_z_matrix = 
(1        0         0       0)
(0        1         0       0)
(0        0       -1/2      1/2)
(0        0         0       1)

[from getReverseZMatrixOrIdentity()]

and v'' = reverse_z_matrix v'
where v = (x'', y'', z'', w'')

Then
z'' = -1/2 z' + 1/2 w'
    = -1/2 (-z - 2n) + 1/2 (-z)
	= 1/2 z + n - 1/2 z = n
w'' = w'
   = -z

z_ndc = z'' / w'' = n / -z = -n / z

solving for z:

z = -n / z_ndc

z_01 = z_ndc            (Assuming glClipControl GL_ZERO_TO_ONE)
so

*************
depth = -z = -(-n / z_ndc) = n / z_ndc = n / z_01
*************
also
z_01 = n / depth


Examples:
Suppose z = -n             (fragment on near plane)
Then z_ndc = -n / (-n) = 1
z_01 = 1

Suppose z = -inf           (fragment on 'far plane')
Then z_ndc = -n / (-inf) = 0
z_01 = 0


Case where we are not using a reverse-z depth buffer:
-----------------------------------------------------

Let reverse_z_matrix = I
Then
z'' = z', w'' = w'.
z_ndc = z'' / w'' = z' / w' 
      = (-z - 2n) / -z 
      = (z + 2n) / z 
z_ndc = 1 + 2n/z

solving for z:
2n/z = z_ndc - 1
z = 2n / (z_ndc - 1)


if z_01 = (z_ndc + 1)/2        (Assuming glClipControl GL_NEGATIVE_ONE_TO_ONE, e.g. z_ndc e [-1. 1], and taking into account that the values read from depth buffer will be in [0, 1].)


z_01 = ((1 + 2n/z) + 1)/2

z_01 = (1 + 2n/z)/2 + 1/2
z_01 = 1/2 + n/z + 1/2 = 1 + n/z

solving for z:
n/z = z_01 - 1
z = n / (z_01 - 1)

*************
depth = -z = -n / (z_01 - 1)
*************

Examples:
Suppose z = -n             (near plane)
Then z_ndc = 1 + 2n/(-n) = 1 - 2 = -1
z_01 = (z_ndc + 1)/2 = (-1 + 1) / 2 = 0

Suppose z = -inf           (far plane)
Then z_ndc = 1 + 2n/(-inf) = 1 + 0 = 1
z_01 = (1 + 1)/2 = 1


Orthographic matrix case
=====================================================
raw_proj_matrix = 
(2(r-l)     0         A          -(r+l/(r-l))
(0          2(t-b)    B          (t-b))
(0          0         -2/(f-n)   -(f+n)/(f-n))
(0          0         0          1)

and
v' = raw_proj_matrix v

Then
z' = -2z/(f-n) - (f+n)/(f-n)*w    =    -2z(f-n) - (f+n)/(f-n)
w' = w = 1


Case where we are using a reverse-z depth buffer:
-------------------------------------------------

let 
reverse_z_matrix = 
(1        0         0       0)
(0        1         0       0)
(0        0       -1/2      1/2)
(0        0         0       1)

and v'' = reverse_z_matrix v'

Then
z'' = -1/2 z' + 1/2 w'
    = -1/2 (-2z/(f-n) - (f+n)/(f-n)) + 1/2 (1)
	= z/(f-n) + (1/2)(f+n)/(f-n) + 1/2
w'' = w'
   = 1

z_ndc = z'' / w'' = z/(f-n) + (1/2)(f+n)/(f-n) + 1/2
z_ndc = (z + (f+n)/2)/(f-n) + 1/2
let Q := (1/2)(f+n)/(f-n) + 1/2
then 
z_ndc = z/(f-n) + Q

solving for z:

z_ndc - Q = z/(f-n)
z = (z_ndc - Q)(f-n)


z_01 = z_ndc            (Assuming glClipControl GL_ZERO_TO_ONE)
so

depth = -z = -(z_ndc - Q)(f-n)
depth = -(z_01 - Q)(f-n)
depth = -(z_01 - [(1/2)(f+n)/(f-n) + 1/2])(f-n)
depth = -(z_01 - (1/2)(f+n)/(f-n) - 1/2)(f-n)
depth = -(z_01(f-n) - (1/2)(f+n) - (f-n)/2)
depth = -((z_01 - 1/2)(f-n) - (1/2)(f+n))

*************
depth = (1/2 - z_01)(f-n) + (f+n)/2
*************

Examples:
Suppose z = -n             (near plane)
#Then z_ndc = (f-n)*(-n) + (1/2)(f+n)/(f-n) + 1/2
z_ndc = (-n)/(f-n) + Q = (-n)/(f-n) + (1/2)(f+n)/(f-n) + 1/2
z_ndc = (-n + (1/2)(f + n))(f-n) + 1/2
z_ndc = (f/2 - n/2)(f-n) + 1/2 = ((f-n)/2)/(f-n) + 1/2 = 1


Suppose z = -f           (far plane)
z_ndc = (-f)/(f-n) + Q = (-f)/(f-n) + (1/2)(f+n)/(f-n) + 1/2
z_ndc = (-f + (1/2)(f + n))(f-n) + 1/2
z_ndc = (-f/2 + n/2)(f-n) + 1/2 = (-(f-n)/2)/(f-n) + 1/2 = 1
z_ndc = (-(f/2 - n/2))(f-n) + 1/2 = (-(f-n)/2)/(f-n) + 1/2 = -1/2 + 1/2 = 0
*/


// Binds texture to specified texture unit.  Doesn't reset active texture unit back to zero.
static inline void bindTextureToTextureUnitRaw(const OpenGLTexture& texture, int texture_unit_index)
{
	glActiveTexture(GL_TEXTURE0 + texture_unit_index); // Specifies which texture unit to make active
	glBindTexture(/*target=*/texture.getTextureTarget(), /*texture=*/texture.texture_handle);
}


static inline void bindTextureToTextureUnit(const OpenGLTexture& texture, int texture_unit_index)
{
	glActiveTexture(GL_TEXTURE0 + texture_unit_index); // Specifies which texture unit to make active
	glBindTexture(/*target=*/texture.getTextureTarget(), /*texture=*/texture.texture_handle);
	glActiveTexture(GL_TEXTURE0); // Change the active texture unit index back to zero, so we don't accidentally change the binding when creating new textures etc.
}


static inline void unbindTextureFromTextureUnit(GLenum texture_target, int texture_unit_index)
{
	glActiveTexture(GL_TEXTURE0 + texture_unit_index); // Specifies which texture unit to make active
	glBindTexture(/*target=*/texture_target, /*texture=*/0);
	glActiveTexture(GL_TEXTURE0);
}


// Gets the texture target (type) from texture.
static inline void unbindTextureFromTextureUnit(const OpenGLTexture& texture, int texture_unit_index)
{
	unbindTextureFromTextureUnit(/*target=*/texture.getTextureTarget(), texture_unit_index);
}


// Bind texture to texture unit, bind texture unit to sampler.
static inline void bindTextureUnitToSampler(const OpenGLTexture& texture, int texture_unit_index, GLint sampler_uniform_location)
{
	bindTextureToTextureUnit(texture, texture_unit_index);

	glUniform1i(sampler_uniform_location, texture_unit_index);
}


#ifndef NDEBUG
static int getIntUniformVal(OpenGLProgram& prog, GLint location)
{
	assert(location >= 0);

	GLint params = 0;
	glGetUniformiv(prog.program, location, &params);
	return params;
}


static int getCurrentProgram()
{
	GLint program = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &program);
	return program;
}


static GLuint getBoundTexture2D(int texture_unit_index)
{
	glActiveTexture(GL_TEXTURE0 + texture_unit_index);

	GLint tex_handle = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex_handle);

	glActiveTexture(GL_TEXTURE0);
	return tex_handle;
}


[[maybe_unused]] static int getNumBoundTextures(int max_texture_unit_index)
{
	int num_bound = 0;
	for(int i=0; i<=max_texture_unit_index; ++i)
	{
		const bool tex_unit_bound = getBoundTexture2D(i) != 0;
		if(tex_unit_bound)
			num_bound++;
	}
	return num_bound;
}
#endif


static inline void assertCurrentProgramIsZero()
{
#ifndef EMSCRIPTEN // getCurrentProgram() doesn't work in Emscripten/WebGL.
	assert(getCurrentProgram() == 0);
#endif
}


class ComputeAnimatedObJointMatricesTask : public glare::Task
{
public:
	virtual void run(size_t /*thread_index*/)
	{
		while(1)
		{
			const int64 index_to_process = next_ob_i->increment();
			if(index_to_process >= (int64)animated_obs_to_process->size())
				break;

			GLObject* const ob = (*animated_obs_to_process)[index_to_process];

			const AnimationData& anim_data = ob->mesh_data->animation_data;

			if(!anim_data.animations.empty())
			{
				ob->anim_node_data.resize(anim_data.nodes.size());

				const float DEBUG_SPEED_FACTOR = 1;

				const AnimationDatum& anim_datum_a                       = *anim_data.animations        [myClamp(ob->current_anim_i, 0, (int)anim_data.animations.size() - 1)];
				const std::vector<PerAnimationNodeData>& anim_a_node_data = anim_data.per_anim_node_data[myClamp(ob->current_anim_i, 0, (int)anim_data.animations.size() - 1)];

				const int use_next_anim_i = (ob->next_anim_i == -1) ? ob->current_anim_i : ob->next_anim_i;
				const AnimationDatum& anim_datum_b                        = *anim_data.animations       [myClamp(use_next_anim_i,    0, (int)anim_data.animations.size() - 1)];
				const std::vector<PerAnimationNodeData>& anim_b_node_data = anim_data.per_anim_node_data[myClamp(use_next_anim_i,    0, (int)anim_data.animations.size() - 1)];

				const float transition_frac = (float)Maths::smoothStep<double>(ob->transition_start_time, ob->transition_end_time, current_time);

				const float unwrapped_use_in_anim_time = (current_time + (float)ob->use_time_offset) * DEBUG_SPEED_FACTOR;

				assert(anim_datum_a.anim_len > 0);
				assert(anim_datum_b.anim_len > 0);
				const float use_in_anim_time_a = Maths::floatMod(unwrapped_use_in_anim_time, anim_datum_a.anim_len);
				const float use_in_anim_time_b = Maths::floatMod(unwrapped_use_in_anim_time, anim_datum_b.anim_len);


				// For each input accessor that the animation uses, we have an array of keyframe times.
				// For each such array, find the current and next keyframe, and interpolation fraction, based on the current time.
				// Store in a AnimationKeyFrameLocation in key_frame_locs.
				//
				// We are doing 2 important optimisations here:
				// The first is that many nodes in a skeleton may use the same input accessor.  We don't want to search the keyframe times for the correct keyframe every time
				// the input accessor is used by a node, so we do the search once for the input accessor, and store the results in key_frame_locs.
				//
				// The second optimisation is that a particular animation may only use 1 or a few input accessors.
				// For example the avatar idle animation only uses input accessor 9, and there are 33 input accessors in total.
				// So for the idle animation we only need to do the keyframe search for input accessor 9, not the 32 other accessors.

				const size_t keyframe_times_size = myMax(anim_data.keyframe_times.size(), anim_datum_a.m_keyframe_times.size(), anim_datum_b.m_keyframe_times.size());
				key_frame_locs.resizeNoCopy(keyframe_times_size * 2); // keyframe times for animation a are first, then keyframe times for animation b.

				//------------------------ Compute keyframe times for animation a -------------------------
				if(transition_frac < 1.f) // At transition_frac = 1, result is fully animation b, a is used if transition_frac < 1
				{
					const js::Vector<KeyFrameTimeInfo>& keyframe_times_a = (!anim_datum_a.m_keyframe_times.empty()) ? anim_datum_a.m_keyframe_times : anim_data.keyframe_times;

					for(size_t q=0; q<anim_datum_a.used_input_accessor_indices.size(); ++q)
					{
						const int input_accessor_i = anim_datum_a.used_input_accessor_indices[q];

						const KeyFrameTimeInfo& keyframe_time_info = keyframe_times_a[input_accessor_i];
						assert(keyframe_time_info.times_size == (int)keyframe_time_info.times.size());

						const float in_anim_time = use_in_anim_time_a;
						AnimationKeyFrameLocation& key_frame_loc = key_frame_locs[input_accessor_i];

						// If keyframe times are equally spaced, we can skip the binary search stuff, and just compute the keyframes we are between directly.
						if(keyframe_time_info.equally_spaced && (keyframe_time_info.t_back == anim_datum_a.anim_len))
						{
							if(use_in_anim_time_a < keyframe_time_info.t_0)
							{
								key_frame_loc.i_0 = 0;
								key_frame_loc.i_1 = 0;
								key_frame_loc.frac = 0;
							}
							else
							{
								const float t_minus_t_0 = use_in_anim_time_a - keyframe_time_info.t_0;
								assert(t_minus_t_0 >= 0);

								int index = (int)(t_minus_t_0 * keyframe_time_info.recip_spacing);

								const float frac = (t_minus_t_0 - (float)index * keyframe_time_info.spacing) * keyframe_time_info.recip_spacing; // Fraction of way through frame
								assert(frac >= -0.001f && frac < 1.001f);

								if(index >= keyframe_time_info.times_size)
									index = 0;

								int next_index = index + 1;
								if(next_index >= keyframe_time_info.times_size)
									next_index = 0;

								key_frame_loc.i_0 = index;
								key_frame_loc.i_1 = next_index;
								key_frame_loc.frac = frac;
							}
						}
						else
						{
							const std::vector<float>& time_vals = keyframe_time_info.times;

							if(keyframe_time_info.times_size != 0)
							{
								if(keyframe_time_info.times_size == 1)
								{
									key_frame_loc.i_0 = 0;
									key_frame_loc.i_1 = 0;
									key_frame_loc.frac = 0;
								}
								else
								{
									assert(time_vals.size() >= 2);

									// TODO: use incremental search based on the position last frame, instead of using upper_bound.  (or combine)

									/*
									frame 0                     frame 1                        frame 2                      frame 3
									|----------------------------|-----------------------------|-----------------------------|-------------------------> time
									^                            ^            ^                ^
									cur_frame_i                             in_anim_time
									index                        next_index
									*/

									// Find current frame
									auto res = std::upper_bound(time_vals.begin(), time_vals.end(), in_anim_time); // "Finds the position of the first element in an ordered range that has a value that is greater than a specified value"
									int next_index = (int)(res - time_vals.begin());
									assert(next_index >= 0 && next_index <= (int)time_vals.size());
									int index = next_index - 1;
									assert(index >= -1 && index < (int)time_vals.size());

									next_index = myMin(next_index, keyframe_time_info.times_size - 1);
									assert(next_index >= 0 && next_index < (int)time_vals.size());

									if(index < 0) // This is the case when use_in_anim_time_a < t_0.  In this case we want to clamp the output values to the keyframe 0 values.
									{
										key_frame_loc.i_0 = 0;
										key_frame_loc.i_1 = 0;
										key_frame_loc.frac = 0;
									}
									else
									{
										const float index_time = time_vals[index];

										float frac;
										frac = (in_anim_time - index_time) / (time_vals[next_index] - index_time);

										if(!(frac >= 0 && frac <= 1)) // TEMP: handle NaNs
											frac = 0;

										key_frame_loc.i_0 = index;
										key_frame_loc.i_1 = next_index;
										key_frame_loc.frac = frac;
									}
								}
							}
						}
					}
				}

				//------------------------ Compute keyframe times for animation b -------------------------
				if(transition_frac > 0.f) // At transition_frac = 0, result is fully animation a, b is used if transition_frac > 0
				{
					const js::Vector<KeyFrameTimeInfo>& keyframe_times_b = (!anim_datum_b.m_keyframe_times.empty()) ? anim_datum_b.m_keyframe_times : anim_data.keyframe_times;

					for(size_t q=0; q<anim_datum_b.used_input_accessor_indices.size(); ++q)
					{
						const int input_accessor_i = anim_datum_b.used_input_accessor_indices[q];

						const KeyFrameTimeInfo& keyframe_time_info = keyframe_times_b[input_accessor_i];
						assert(keyframe_time_info.times_size == (int)keyframe_time_info.times.size());

						const float in_anim_time = use_in_anim_time_b;
						AnimationKeyFrameLocation& key_frame_loc = key_frame_locs[keyframe_times_size + input_accessor_i]; // The keyframe_times_size offset is to get keyframe locations for animation b.

						// If keyframe times are equally spaced, we can skip the binary search stuff, and just compute the keyframes we are between directly.
						if(keyframe_time_info.equally_spaced && (keyframe_time_info.t_back == anim_datum_b.anim_len))
						{
							if(in_anim_time < keyframe_time_info.t_0)
							{
								key_frame_loc.i_0 = 0;
								key_frame_loc.i_1 = 0;
								key_frame_loc.frac = 0;
							}
							else
							{
								const float t_minus_t_0 = in_anim_time - keyframe_time_info.t_0;
								assert(t_minus_t_0 >= 0);

								int index = (int)(t_minus_t_0 * keyframe_time_info.recip_spacing);

								const float frac = (t_minus_t_0 - (float)index * keyframe_time_info.spacing) * keyframe_time_info.recip_spacing; // Fraction of way through frame
								assert(frac >= -0.001f && frac < 1.001f);

								if(index >= keyframe_time_info.times_size)
									index = 0;

								int next_index = index + 1;
								if(next_index >= keyframe_time_info.times_size)
									next_index = 0;

								key_frame_loc.i_0 = index;
								key_frame_loc.i_1 = next_index;
								key_frame_loc.frac = frac;
							}
						}
						else
						{
							const std::vector<float>& time_vals = keyframe_time_info.times;

							if(keyframe_time_info.times_size != 0)
							{
								if(keyframe_time_info.times_size == 1)
								{
									key_frame_loc.i_0 = 0;
									key_frame_loc.i_1 = 0;
									key_frame_loc.frac = 0;
								}
								else
								{
									assert(time_vals.size() >= 2);

									// Find current frame
									auto res = std::upper_bound(time_vals.begin(), time_vals.end(), in_anim_time); // "Finds the position of the first element in an ordered range that has a value that is greater than a specified value"
									int next_index = (int)(res - time_vals.begin());
									assert(next_index >= 0 && next_index <= (int)time_vals.size());
									int index = next_index - 1;
									assert(index >= -1 && index < (int)time_vals.size());

									next_index = myMin(next_index, keyframe_time_info.times_size - 1);
									assert(next_index >= 0 && next_index < (int)time_vals.size());

									if(index < 0) // This is the case when use_in_anim_time_b < t_0.  In this case we want to clamp the output values to the keyframe 0 values.
									{
										key_frame_loc.i_0 = 0;
										key_frame_loc.i_1 = 0;
										key_frame_loc.frac = 0;
									}
									else
									{
										const float index_time = time_vals[index];

										float frac;
										frac = (in_anim_time - index_time) / (time_vals[next_index] - index_time);

										if(!(frac >= 0 && frac <= 1)) // TEMP: handle NaNs
											frac = 0;

										key_frame_loc.i_0 = index;
										key_frame_loc.i_1 = next_index;
										key_frame_loc.frac = frac;
									}
								}
							}
						}
					}
				}

				node_matrices.resizeNoCopy(anim_data.sorted_nodes.size()); // A temp buffer to store node transforms that we can look up parent node transforms in.

				const js::Vector<js::Vector<Vec4f, 16> >& output_data_a = (!anim_datum_a.m_output_data.empty()) ? anim_datum_a.m_output_data : anim_data.output_data;
				const js::Vector<js::Vector<Vec4f, 16> >& output_data_b = (!anim_datum_b.m_output_data.empty()) ? anim_datum_b.m_output_data : anim_data.output_data;

				for(size_t n=0; n<anim_data.sorted_nodes.size(); ++n)
				{
					const int node_i = anim_data.sorted_nodes[n];
					const AnimationNodeData& node_data = anim_data.nodes[node_i];
					const PerAnimationNodeData& node_a = anim_a_node_data[node_i];
					const PerAnimationNodeData& node_b = anim_b_node_data[node_i];

					Vec4f trans_a = node_data.trans;
					Vec4f trans_b = node_data.trans;
					Quatf rot_a   = node_data.rot;
					Quatf rot_b   = node_data.rot;
					Vec4f scale_a = node_data.scale;
					Vec4f scale_b = node_data.scale;

					if(transition_frac < 1.f) // At transition_frac = 1, result is fully animation b, a is used if transition_frac < 1
					{
						if(node_a.translation_input_accessor >= 0)
						{
							//conPrint("anim_datum_a: " + anim_datum_a.name + "," + toString(anim_data.keyframe_times[node_a.translation_input_accessor].back()));

							const int i_0    = key_frame_locs[node_a.translation_input_accessor].i_0;
							const int i_1    = key_frame_locs[node_a.translation_input_accessor].i_1;
							const float frac = key_frame_locs[node_a.translation_input_accessor].frac;

							// read translation values from output accessor.
							const Vec4f trans_0 = (output_data_a[node_a.translation_output_accessor])[i_0];
							const Vec4f trans_1 = (output_data_a[node_a.translation_output_accessor])[i_1];
							trans_a = Maths::lerp(trans_0, trans_1, frac); // TODO: handle step interpolation, cubic lerp etc..
						}

						if(node_a.rotation_input_accessor >= 0)
						{
							const int i_0    = key_frame_locs[node_a.rotation_input_accessor].i_0;
							const int i_1    = key_frame_locs[node_a.rotation_input_accessor].i_1;
							const float frac = key_frame_locs[node_a.rotation_input_accessor].frac;

							// read rotation values from output accessor
							const Quatf rot_0 = Quatf((output_data_a[node_a.rotation_output_accessor])[i_0]);
							const Quatf rot_1 = Quatf((output_data_a[node_a.rotation_output_accessor])[i_1]);
							rot_a = Quatf::nlerp(rot_0, rot_1, frac);
						}

						if(node_a.scale_input_accessor >= 0)
						{
							const int i_0    = key_frame_locs[node_a.scale_input_accessor].i_0;
							const int i_1    = key_frame_locs[node_a.scale_input_accessor].i_1;
							const float frac = key_frame_locs[node_a.scale_input_accessor].frac;

							// read scale values from output accessor
							const Vec4f scale_0 = (output_data_a[node_a.scale_output_accessor])[i_0];
							const Vec4f scale_1 = (output_data_a[node_a.scale_output_accessor])[i_1];
							scale_a = Maths::lerp(scale_0, scale_1, frac);
						}
					}

					if(transition_frac > 0.f) // At transition_frac = 0, result is fully animation a, b is used if transition_frac > 0
					{
						if(node_b.translation_input_accessor >= 0)
						{
							const int i_0    = key_frame_locs[keyframe_times_size + node_b.translation_input_accessor].i_0; // The keyframe_times_size offset is to get keyframe locations for animation b.
							const int i_1    = key_frame_locs[keyframe_times_size + node_b.translation_input_accessor].i_1;
							const float frac = key_frame_locs[keyframe_times_size + node_b.translation_input_accessor].frac;

							// read translation values from output accessor.
							const Vec4f trans_0 = (output_data_b[node_b.translation_output_accessor])[i_0];
							const Vec4f trans_1 = (output_data_b[node_b.translation_output_accessor])[i_1];
							trans_b = Maths::lerp(trans_0, trans_1, frac); // TODO: handle step interpolation, cubic lerp etc..
						}

						if(node_b.rotation_input_accessor >= 0)
						{
							const int i_0    = key_frame_locs[keyframe_times_size + node_b.rotation_input_accessor].i_0;
							const int i_1    = key_frame_locs[keyframe_times_size + node_b.rotation_input_accessor].i_1;
							const float frac = key_frame_locs[keyframe_times_size + node_b.rotation_input_accessor].frac;

							// read rotation values from output accessor
							const Quatf rot_0 = Quatf((output_data_b[node_b.rotation_output_accessor])[i_0]);
							const Quatf rot_1 = Quatf((output_data_b[node_b.rotation_output_accessor])[i_1]);
							rot_b = Quatf::nlerp(rot_0, rot_1, frac);
						}

						if(node_b.scale_input_accessor >= 0)
						{
							const int i_0    = key_frame_locs[keyframe_times_size + node_b.scale_input_accessor].i_0;
							const int i_1    = key_frame_locs[keyframe_times_size + node_b.scale_input_accessor].i_1;
							const float frac = key_frame_locs[keyframe_times_size + node_b.scale_input_accessor].frac;

							// read scale values from output accessor
							const Vec4f scale_0 = (output_data_b[node_b.scale_output_accessor])[i_0];
							const Vec4f scale_1 = (output_data_b[node_b.scale_output_accessor])[i_1];
							scale_b = Maths::lerp(scale_0, scale_1, frac);
						}
					}

					GLObjectAnimNodeData& ob_anim_node_data_i = ob->anim_node_data[node_i];

					const Vec4f trans = Maths::lerp(trans_a, trans_b, transition_frac);
					const Quatf anim_rot = Quatf::nlerp(rot_a, rot_b, transition_frac);
					const Quatf rot = select(ob_anim_node_data_i.procedural_rot, anim_rot, bitcastToVec4f(Vec4i(ob_anim_node_data_i.procedural_rot_mask)));
					const Vec4f scale = Maths::lerp(scale_a, scale_b, transition_frac);

					const Matrix4f rot_mat = rot.toMatrix();

					const Matrix4f TRS(
						rot_mat.getColumn(0) * copyToAll<0>(scale),
						rot_mat.getColumn(1) * copyToAll<1>(scale),
						rot_mat.getColumn(2) * copyToAll<2>(scale),
						setWToOne(trans));

					const Matrix4f last_pre_proc_to_object = (node_data.parent_index == -1) ? TRS : (node_matrices[node_data.parent_index] * node_data.retarget_adjustment * TRS); // Transform without procedural_transform applied
					const Matrix4f node_transform = last_pre_proc_to_object * ob->anim_node_data[node_i].procedural_transform;

					node_matrices[node_i] = node_transform;

					ob_anim_node_data_i.last_pre_proc_to_object = last_pre_proc_to_object;
					ob_anim_node_data_i.last_rot = rot;
					ob_anim_node_data_i.node_hierarchical_to_object = node_transform;
				}
			}
			else // else if anim_data.animations.empty():
			{
				if(!anim_data.joint_nodes.empty()) // If we have a skin, but no animations, just use the default trans, rot, scales.
				{
					const size_t num_nodes = anim_data.nodes.size();
					node_matrices.resizeNoCopy(num_nodes);
					ob->anim_node_data.resize(num_nodes);

					for(size_t n=0; n<anim_data.sorted_nodes.size(); ++n)
					{
						const int node_i = anim_data.sorted_nodes[n];
						const AnimationNodeData& node_data = anim_data.nodes[node_i];
						const Vec4f trans = node_data.trans;
						const Quatf rot = node_data.rot;
						const Vec4f scale = node_data.scale;

						const Matrix4f rot_mat = rot.toMatrix();
						const Matrix4f TRS(
							rot_mat.getColumn(0) * copyToAll<0>(scale),
							rot_mat.getColumn(1) * copyToAll<1>(scale),
							rot_mat.getColumn(2) * copyToAll<2>(scale),
							setWToOne(trans));

						const Matrix4f last_pre_proc_to_object = (node_data.parent_index == -1) ? TRS : (node_matrices[node_data.parent_index] * TRS);
						const Matrix4f node_transform = last_pre_proc_to_object * ob->anim_node_data[node_i].procedural_transform;

						node_matrices[node_i] = node_transform;

						ob->anim_node_data[node_i].node_hierarchical_to_object = node_transform;
					}
				}
			}

			const size_t joint_nodes_size = anim_data.joint_nodes.size();
			if(!anim_data.animations.empty() || (joint_nodes_size > 0))
			{
				ob->joint_matrices.resizeNoCopy(joint_nodes_size);

				for(size_t i=0; i<joint_nodes_size; ++i)
				{
					const int node_i = anim_data.joint_nodes[i];

					ob->joint_matrices[i] = node_matrices[node_i] * anim_data.nodes[node_i].inverse_bind_matrix;

					//conPrint("joint_matrices[" + toString(i) + "]: (joint node: " + toString(node_i) + ", '" + anim_data.nodes[node_i].name + "')");
					//conPrint(ob->joint_matrices[i].toString());
				}
			}
		}
	}

	float current_time;
	glare::AtomicInt* next_ob_i;
	js::Vector<GLObject*, 16>* animated_obs_to_process;

	// Some temporary vectors:
	js::Vector<Matrix4f, 16> node_matrices;
	js::Vector<AnimationKeyFrameLocation, 16> key_frame_locs;
};


void OpenGLEngine::addDebugLinesForFrustum(const Vec4f* frustum_verts_ws, const Vec4f& t, float line_rad, const Colour4f& line_col)
{
	addDebugLine(frustum_verts_ws[0] + t, frustum_verts_ws[1] + t, line_rad, line_col);
	addDebugLine(frustum_verts_ws[1] + t, frustum_verts_ws[2] + t, line_rad, line_col);
	addDebugLine(frustum_verts_ws[2] + t, frustum_verts_ws[3] + t, line_rad, line_col);
	addDebugLine(frustum_verts_ws[3] + t, frustum_verts_ws[0] + t, line_rad, line_col);

	addDebugLine(frustum_verts_ws[4] + t, frustum_verts_ws[5] + t, line_rad, line_col);
	addDebugLine(frustum_verts_ws[5] + t, frustum_verts_ws[6] + t, line_rad, line_col);
	addDebugLine(frustum_verts_ws[6] + t, frustum_verts_ws[7] + t, line_rad, line_col);
	addDebugLine(frustum_verts_ws[7] + t, frustum_verts_ws[4] + t, line_rad, line_col);

	addDebugLine(frustum_verts_ws[0] + t, frustum_verts_ws[4] + t, line_rad, line_col);
	addDebugLine(frustum_verts_ws[1] + t, frustum_verts_ws[5] + t, line_rad, line_col);
	addDebugLine(frustum_verts_ws[2] + t, frustum_verts_ws[6] + t, line_rad, line_col);
	addDebugLine(frustum_verts_ws[3] + t, frustum_verts_ws[7] + t, line_rad, line_col);
}


void OpenGLEngine::addDebugVisForShadowFrustum(const Vec4f frustum_verts_ws[8], float max_shadowing_dist, const Planef clip_planes[18], int /*num_clip_planes_used*/)
{
#if BUILD_TESTS
	// Draw frustum with spheres at vertices and lines/cylinders along edges.
	// We will also draw a translated frustum along the sun direction by max_shadowing_dist.
	const float line_rad = frustum_verts_ws[0].getDist(frustum_verts_ws[4]) * 0.004f;
	const Colour4f line_col(1,0,0,1);
	addDebugLinesForFrustum(frustum_verts_ws, /*translation=*/Vec4f(0.f),                   line_rad, line_col);
	addDebugLinesForFrustum(frustum_verts_ws, /*translation=*/sun_dir * max_shadowing_dist, line_rad, line_col);

	const Colour4f extension_line_col(1,0,0,0.5f);
	for(int q=0; q<8; ++q)
		addDebugLine(frustum_verts_ws[q], frustum_verts_ws[q] + sun_dir * max_shadowing_dist, line_rad, extension_line_col);

	for(int q=0; q<8; ++q)
		addDebugSphere(frustum_verts_ws[q], /*radius*/0.1f, Colour4f(1,0,0,1));


	// Draw clip planes
	Vec4f center_p(0); // Compute point at centre of frustum
	for(int q=0; q<8; ++q)
		center_p += frustum_verts_ws[q];
	center_p *= (1 / 8.0f);

	for(int q=0; q<6/*num_clip_planes_used*/; ++q)
	{
		addDebugPlane(clip_planes[q].closestPointOnPlane(center_p), clip_planes[q].getNormal(), 
			/*plane draw width=*/frustum_verts_ws[0].getDist(frustum_verts_ws[4]) * 1.5f,
			(q < 6) ? Colour4f(0.3f, 0.8f, 0.3f, 0.3f) : Colour4f(0.3f, 0.3f, 0.8f, 0.3f));
	}
#endif
}


// glDrawBuffer does not seem to be in OpenGL ES, so use glDrawBuffers.
inline static void setSingleDrawBuffer(GLenum buffer)
{
	const GLenum buffers[1] = { buffer };
	glDrawBuffers(1, buffers);
}


inline static void setTwoDrawBuffers(GLenum buffer_0, GLenum buffer_1)
{
	const GLenum draw_buffers[] = { buffer_0, buffer_1 };
	glDrawBuffers(/*num=*/2, draw_buffers);
}


[[maybe_unused]] inline static void setThreeDrawBuffers(GLenum buffer_0, GLenum buffer_1, GLenum buffer_2)
{
	const GLenum draw_buffers[] = { buffer_0, buffer_1, buffer_2 };
	glDrawBuffers(/*num=*/3, draw_buffers);
}


void OpenGLEngine::draw()
{
	ZoneScopedC(0x33FF33); // Tracy profiler.  Set green colour for zone.
	TracyGpuZone("OpenGLEngine::draw");
	TracyGpuCollect;
	DebugGroup debug_group("draw()");

	if(!init_succeeded)
		return;

	if(query_profiling_enabled && current_scene->collect_stats)
	{
#if EMSCRIPTEN
		buffered_total_timer->recordBegin();
#else
		start_query->recordTimestamp();
#endif
	}

	Timer draw_method_timer;

	glActiveTexture(GL_TEXTURE0);
	glUseProgram(0);
	VAO::unbind();
	assertCurrentProgramIsZero();

	current_scene->frame_num++;


	//================= Process became_unused_tex_keys =================
	// This is the list of texture keys for textures that became unused on another thread.
	{
		Lock lock(became_unused_tex_keys_mutex);
		for(size_t i=0; i<became_unused_tex_keys.size(); ++i)
		{
			auto res = opengl_textures.find(became_unused_tex_keys[i]);
			if(res != opengl_textures.end())
			{
				// If the only reference to the texture is from opengl_textures:  Note that we need to check here since another reference may have been added since the texture was added to became_unused_tex_keys.
				if(res->second.value->getRefCount() == 1)
				{
					opengl_textures.itemBecameUnused(became_unused_tex_keys[i], res->second); // Add to unused list.

#if !defined(OSX) && !defined(EMSCRIPTEN)
					// Since this texture is not being used, we can make it non-resident.
					res->second.value->makeNonResidentIfResident();
#endif
				}
			}
		}
		became_unused_tex_keys.clear();
	}

	//================= Check if any building programs are done. =================
	for(auto it = building_progs.begin(); it != building_progs.end(); )
	{
		OpenGLProgram* prog = *it;
		if(prog->checkLinkingDone())
		{
			it = building_progs.erase(it);

			try
			{
				finishBuildingProg(prog);
			}
			catch(glare::Exception& e)
			{
				conPrint("Error building program: " + e.what());
			}
		}
		else
			++it;
	}


	//================= Run scatter compute shader to update data on GPU (not used currently) =================
	if(data_updates_buffer.nonEmpty())
	{
#if !defined(OSX) && !defined(EMSCRIPTEN)
		assert(data_updates_buffer.dataSizeBytes() <= data_updates_ssbo->byteSize());

		data_updates_ssbo->invalidateBufferData();

		data_updates_ssbo->updateData(/*dest offset=*/0, data_updates_buffer.data(), data_updates_buffer.dataSizeBytes());

		// Execute compute shader
		const size_t num_updates = data_updates_buffer.dataSizeBytes() / sizeof(DataUpdateStruct);

		scatter_data_prog->useProgram();
		glDispatchCompute(/*num groups x=*/(GLuint)num_updates, 1, 1);
		OpenGLProgram::useNoPrograms();

		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT); // Make sure all writes to per_object_data have finished.


		data_updates_buffer.clear();
#endif
	}


	//if(frame_num % 100 == 0)	checkMDIGPUDataCorrect();

	if(!loaded_maps_for_sun_dir && current_scene->env_ob.nonNull())
	{
		try
		{
			loadMapsForSunDir();
			assert(loaded_maps_for_sun_dir);
		}
		catch(glare::Exception& e)
		{
			conPrint("Error: " + e.what());
			if(print_output)
				print_output->print("ERROR: " + e.what());
			this->cosine_env_tex = this->fbm_tex; // Avoid crash with null cosine_env_tex
			this->specular_env_tex = this->fbm_tex;

			loaded_maps_for_sun_dir = true; // Don't try and repeatedly load the maps
		}
	}

	//================= If the ShaderFileWatcherThread has detected that a shader file has changed, reload all shaders. =================
#if BUILD_TESTS
	if(shader_file_changed)
	{
		conPrint("------------------------------------------- Reloading shaders -------------------------------------------");

		progs.clear(); // Clear built-program cache
		building_progs.clear();

		if(reload_shaders_callback)
			reload_shaders_callback->reloadShaders();

		const std::string use_shader_dir = data_dir + "/shaders";

		try
		{
			this->vert_utils_glsl = FileUtils::readEntireFileTextMode(use_shader_dir + "/vert_utils.glsl");
			this->frag_utils_glsl = FileUtils::readEntireFileTextMode(use_shader_dir + "/frag_utils.glsl");

			this->preprocessor_defines_with_common_vert_structs = preprocessor_defines;
			preprocessor_defines_with_common_vert_structs += FileUtils::readEntireFileTextMode(use_shader_dir + "/common_vert_structures.glsl");
			preprocessor_defines_with_common_vert_structs += vert_utils_glsl;

			this->preprocessor_defines_with_common_frag_structs = preprocessor_defines;
			preprocessor_defines_with_common_frag_structs += FileUtils::readEntireFileTextMode(use_shader_dir + "/common_frag_structures.glsl");
			preprocessor_defines_with_common_frag_structs += frag_utils_glsl;
		}
		catch(glare::Exception& e)
		{
			conPrint("Error while reloading vert_utils_glsl etc.: " + e.what());
		}

		// Try and reload env shader
		OpenGLProgramRef new_env_prog;
		try
		{
			new_env_prog = buildEnvProgram(use_shader_dir);
			env_prog = new_env_prog;
		}
		catch(glare::Exception& e)
		{
			conPrint("Error while reloading env prog: " + e.what());
		}
		
		// Try and reload draw-aurora shader
		try
		{
			this->draw_aurora_tex_prog = buildAuroraProgram(use_shader_dir);
		}
		catch(glare::Exception& e)
		{
			conPrint("Error while reloading aurora prog: " + e.what());
		}

		// Try and reload compute_ssao_prog
		try
		{
			if(settings.ssao)
			{
				this->compute_ssao_prog = buildComputeSSAOProg(use_shader_dir);;
				this->blur_ssao_prog = buildBlurSSAOProg(use_shader_dir);
			}
		}
		catch(glare::Exception& e)
		{
			conPrint("Error while reloading build SSAO prog: " + e.what());
		}
		
		// Try and reload final_imaging_prog
		try
		{
			this->final_imaging_prog = buildFinalImagingProg(use_shader_dir);
		}
		catch(glare::Exception& e)
		{
			conPrint("Error while reloading build final_imaging_prog: " + e.what());
		}

		// Reload shaders on all objects
		for(auto z = scenes.begin(); z != scenes.end(); ++z)
		{
			OpenGLScene* scene = z->ptr();
			for(auto it = scene->objects.begin(); it != scene->objects.end(); ++it)
			{
				GLObject* const object = it->ptr();
				for(size_t i=0; i<object->materials.size(); ++i)
				{
					assignShaderProgToMaterial(object->materials[i], object->mesh_data->has_vert_colours, /*uses instancing=*/object->instance_matrix_vbo.nonNull(), object->mesh_data->usesSkinning(), object->mesh_data->has_vert_tangents,
						object->mesh_data->position_w_is_oct16_normal);

					object->materials[i].uniform_flags = computeUniformFlagsForMat(object->materials[i], *object->mesh_data);
				}

				rebuildDenormalisedDrawData(*object);
				rebuildObjectDepthDrawBatches(*object);
			}

			// If built successfully, assign to env ob
			if(scene->env_ob.nonNull() && new_env_prog.nonNull())
				scene->env_ob->materials[0].shader_prog = new_env_prog;
		}

		shader_file_changed = 0;
	}
#endif

	//PerformanceAPI_BeginEvent("draw", NULL, PERFORMANCEAPI_DEFAULT_COLOR);

	// Some other code (e.g. Qt) may have bound some other buffer since the last draw() call.  So reset all this stuff.
	current_index_type = 0;
	current_bound_prog = NULL;
	current_bound_prog_index = std::numeric_limits<uint32>::max();
	current_bound_VAO = NULL;
	current_uniforms_ob = NULL;

	// checkForOpenGLErrors(); // Just for Mac

	this->num_indices_submitted = 0;
	//this->num_face_groups_submitted = 0;
	//this->num_aabbs_submitted = 0;


	double anim_update_duration = 0;


	// Unload unused textures if we have exceeded our texture mem usage budget.
	trimTextureUsage();


	//=============== Process materialise_objects - objects showing materialise effect ===============
	// Update materialise_lower_z and materialise_upper_z for the object.
	// Also remove any objects whose materialise effect has finished from the materialise_objects set.
	{
		ZoneScopedN("Materialise"); // Tracy profiler
		for(auto it = current_scene->materialise_objects.begin(); it != current_scene->materialise_objects.end(); )
		{
			GLObject* ob = it->ptr();
			bool materialise_still_active = false;
			for(size_t z=0; z<ob->materials.size(); ++z)
			{
				OpenGLMaterial& mat = ob->materials[z];
				if(mat.materialise_effect)
				{
					mat.materialise_lower_z = ob->aabb_ws.min_[2];
					mat.materialise_upper_z = ob->aabb_ws.max_[2];
					updateMaterialDataOnGPU(*ob, z);
					if(current_time - mat.materialise_start_time > 1.0)
					{
						// Materialise effect has finished
						mat.materialise_effect = false;
						assignShaderProgToMaterial(mat, ob->mesh_data->has_vert_colours, /*uses instancing=*/ob->instance_matrix_vbo.nonNull(), ob->mesh_data->usesSkinning(), ob->mesh_data->has_vert_tangents, ob->mesh_data->position_w_is_oct16_normal);

						mat.uniform_flags = computeUniformFlagsForMat(mat, *ob->mesh_data);

						rebuildDenormalisedDrawData(*ob);
						rebuildObjectDepthDrawBatches(*ob);
					}
					else
						materialise_still_active = true;
				}
			}
			if(!materialise_still_active)
				it = current_scene->materialise_objects.erase(it); // Erase object from set and advance iterator to next elem in set (or end if none)
			else
				++it;
		}
	}


	//=============== Set animated objects state (update bone matrices etc.) ===========

	// We will only compute updated animation data for objects (current joint matrices), if the object is in a view frustum that extends some distance from the camera,
	// or may cast a shadow into that frustum a little distance.
	Planef anim_shadow_clip_planes[18];
	int num_anim_shadow_clip_planes_used;
	js::AABBox anim_shadow_vol_aabb;
	{
		const float near_dist = 0.2f; // Needs to be > 0 for computeShadowFrustumClipPlanes() to work properly.
		const float far_dist = 200.f;
		const float max_shadowing_dist = 30.f;
		num_anim_shadow_clip_planes_used = getCameraShadowMappingPlanesAndAABB(near_dist, far_dist, max_shadowing_dist, /*shadow_clip_planes_out=*/anim_shadow_clip_planes, /*shadow_vol_aabb_out=*/anim_shadow_vol_aabb);
	}

	const Vec4f campos_ws = this->getCameraPositionWS();
	Timer anim_profile_timer;
	uint32 num_animated_obs_processed = 0;
	if(!current_scene->animated_objects.empty())
	{
		ZoneScopedN("Animated objects"); // Tracy profiler

		animated_obs_to_process.resize(0);
		for(auto it = current_scene->animated_objects.begin(); it != current_scene->animated_objects.end(); ++it)
		{
			GLObject* const ob = it->getPointer();

			bool visible_and_large_enough = AABBIntersectsFrustum(anim_shadow_clip_planes, num_anim_shadow_clip_planes_used, anim_shadow_vol_aabb, ob->aabb_ws);
			if(visible_and_large_enough)
			{
				// Don't update anim data for object if it is too small as seen from the camera.
				const float ob_w = largestDim(ob->aabb_ws);
				const float recip_dist = (ob->aabb_ws.centroid() - campos_ws).fastApproxRecipLength();
				const float proj_len = ob_w * recip_dist;
				visible_and_large_enough = proj_len > 0.01f;
			}

			const bool process = visible_and_large_enough || ob->joint_matrices.empty(); // If the joint matrices are empty we need to compute them at least once, so they don't contain garbage data that is read from.
			if(process)
				animated_obs_to_process.push_back(ob);
		}
		num_animated_obs_processed = (uint32)animated_obs_to_process.size();


		if(!animated_obs_to_process.empty())
		{
			const size_t num_animated_ob_tasks = myClamp(animated_obs_to_process.size(), (size_t)1, high_priority_task_manager->getNumThreads());

			while(animated_objects_tasks.size() < num_animated_ob_tasks)
				animated_objects_tasks.push_back(new ComputeAnimatedObJointMatricesTask());

			animated_objects_task_group->tasks.resize(num_animated_ob_tasks);

			glare::AtomicInt next_ob_i(0);

			for(size_t t=0; t<num_animated_ob_tasks; ++t)
			{
				ComputeAnimatedObJointMatricesTask* task = animated_objects_tasks[t].downcastToPtr<ComputeAnimatedObJointMatricesTask>();
				//task->processed = 0;
				task->current_time = this->current_time;
				task->next_ob_i = &next_ob_i;
				task->animated_obs_to_process = &animated_obs_to_process;

				animated_objects_task_group->tasks[t] = task;
			}

			high_priority_task_manager->runTaskGroup(animated_objects_task_group);
		}


		// Create/update debug visualisation of the joints
		if(false)
		{
			for(size_t z=0; z<animated_obs_to_process.size(); ++z)
			{
				const GLObject* ob = animated_obs_to_process[z];
				const AnimationData& anim_data = ob->mesh_data->animation_data;
				if(!anim_data.animations.empty() || !anim_data.joint_nodes.empty())
				{
					std::vector<GLObjectRef>& joint_obs = debug_joint_obs[ob];
					if(joint_obs.empty())
					{
						joint_obs.resize(anim_data.nodes.size());
						for(size_t node_i=0; node_i<anim_data.nodes.size(); ++node_i)
						{
							if(node_i == 27)
							{
								joint_obs[node_i] = new GLObject();
								joint_obs[node_i]->mesh_data = MeshPrimitiveBuilding::make3DBasisArrowMesh(*vert_buf_allocator); // Base will be at origin, tip will lie at (1, 0, 0)
								joint_obs[node_i]->materials.resize(3);
								joint_obs[node_i]->materials[0].albedo_linear_rgb = Colour3f(0.9f, 0.1f, 0.1f);
								joint_obs[node_i]->materials[1].albedo_linear_rgb = Colour3f(0.1f, 0.9f, 0.1f);
								joint_obs[node_i]->materials[2].albedo_linear_rgb = Colour3f(0.1f, 0.1f, 0.9f);
								joint_obs[node_i]->ob_to_world_matrix = Matrix4f::translationMatrix(1000, 0, 0);
					
								addObject(joint_obs[node_i]);
							}
						}
					}

					// Set location of debug joint visualisation objects
					for(size_t node_i=0; node_i<anim_data.nodes.size(); ++node_i)
						if(joint_obs[node_i])
						{
							joint_obs[node_i]->ob_to_world_matrix = ob->ob_to_world_matrix * ob->anim_node_data[node_i].node_hierarchical_to_object/*node_transform*/ * Matrix4f::uniformScaleMatrix(0.6f);//Matrix4f::translationMatrix(-0.5, 0, 0) * /*to_z_up * */Matrix4f::translationMatrix(node_transform.getColumn(3)) * Matrix4f::uniformScaleMatrix(0.02f);
							updateObjectTransformData(*joint_obs[node_i]);
						}
				}
			}
		}


		// Upload computed joint matrices to GPU
		if(use_ob_and_mat_data_gpu_resident)
		{
			for(size_t i=0; i<animated_obs_to_process.size(); ++i)
			{
				GLObject* ob = animated_obs_to_process[i];
				assert(ob->joint_matrices_base_index >= 0);
				assert(ob->joint_matrices_block);
				if(ob->joint_matrices_base_index >= 0)
					joint_matrices_ssbo->updateData(/*dest offset=*/sizeof(Matrix4f) * ob->joint_matrices_base_index, /*src data=*/ob->joint_matrices.data(), /*src size=*/ob->joint_matrices.dataSizeBytes());
				else
					conPrint("Error, ob->joint_matrices_base_index < 0");
			}
		}
	}

	this->last_num_animated_obs_processed = num_animated_obs_processed;
	anim_update_duration = anim_profile_timer.elapsed();

	//================= Compute view (world space to camera space) matrix =================
	// Indigo/Substrata camera convention is z=up, y=forwards, x=right.
	// OpenGL is y=up, x=right, -z=forwards.
	const float e[16] = { 1, 0, 0, 0,	0, 0, -1, 0,	0, 1, 0, 0,		0, 0, 0, 1 };
	const Matrix4f indigo_to_opengl_cam_matrix = current_scene->use_z_up ? Matrix4f(e) : Matrix4f::identity();

	const Matrix4f main_view_matrix = indigo_to_opengl_cam_matrix * current_scene->world_to_camera_space_matrix;
	current_scene->last_view_matrix = main_view_matrix;

	this->sun_dir_cam_space = main_view_matrix * sun_dir;

	//================= Update MaterialCommonUniforms, these values are constant for all materials for this frame. =================
	MaterialCommonUniforms common_uniforms;
	common_uniforms.frag_view_matrix = main_view_matrix;
	common_uniforms.sundir_cs = this->sun_dir_cam_space;
	common_uniforms.sundir_ws = this->sun_dir;
	common_uniforms.sun_spec_rad_times_solid_angle = this->sun_spec_rad_times_solid_angle * 1.0e-9f;
	common_uniforms.sun_and_sky_av_spec_rad = this->sun_and_sky_av_spec_rad * 1.0e-9f;
	common_uniforms.air_scattering_coeffs = this->air_scattering_coeffs;
	common_uniforms.mat_common_campos_ws = campos_ws;
	common_uniforms.near_clip_dist = this->current_scene->near_draw_dist;
	common_uniforms.far_clip_dist = this->current_scene->max_draw_dist;
	common_uniforms.time = this->current_time;
	common_uniforms.l_over_w = this->current_scene->lens_sensor_dist / this->current_scene->use_sensor_width;
	common_uniforms.l_over_h = this->current_scene->lens_sensor_dist / this->current_scene->use_sensor_height;
	common_uniforms.env_phi = this->sun_phi;
	common_uniforms.water_level_z = this->current_scene->water_level_z;
	common_uniforms.camera_type = (int)this->current_scene->camera_type;
	common_uniforms.mat_common_flags = (this->current_scene->cloud_shadows ? CLOUD_SHADOWS_FLAG : 0) | (this->settings.ssao ? DO_SSAO_FLAG : 0);
	common_uniforms.shadow_map_samples_xy_scale = this->shadow_mapping ? (2048.f / this->shadow_mapping->dynamic_w) : 1.f; // Shadow map sample pattern is scaled for 2048^2 textures.
	common_uniforms.padding_a1 = common_uniforms.padding_a2 = 0;
	this->material_common_uniform_buf_ob->updateData(/*dest offset=*/0, &common_uniforms, sizeof(MaterialCommonUniforms));



	bindStandardTexturesToTextureUnits();


	if(draw_aurora && current_scene->draw_aurora && (this->sun_dir[2] < 0.1))
		drawAuroraTex();


	num_multi_draw_indirect_calls = 0;

	//=============== Render to shadow map depth buffer if needed ===============
	if(current_scene->shadow_mapping)
		renderToShadowMapDepthBuffer();


	bindStandardShadowMappingDepthTextures(); // Rebind now that the shadow maps have been redrawn, and hence cur_static_depth_tex has changed.


#if EMSCRIPTEN
	// NOTE: EXT_color_buffer_half_float is included in WebGL 2: See https://emscripten.org/docs/optimizing/Optimizing-WebGL.html#optimizing-load-times-and-other-best-practices
	// However RGB16F is not supported as a render buffer format, just RGBA16F.  See https://developer.mozilla.org/en-US/docs/Web/API/WebGLRenderingContext/renderbufferStorage
	const OpenGLTextureFormat col_buffer_format = OpenGLTextureFormat::Format_RGBA_Linear_Half;
#else
	const OpenGLTextureFormat col_buffer_format = OpenGLTextureFormat::Format_RGB_Linear_Half;
#endif

	//=============== Allocate or reallocate buffers for drawing to ===============
	// We will render to renderbuffers (main_colour_renderbuffer, main_normal_renderbuffer) etc. attached to main_render_framebuffer.
	// We use renderbuffers as opposed to textures, as OpenGL ES 3.0 doesn't support multisample textures, whereas it does support multisample renderbuffers.
	// Also renderbuffers offer a bit more flexibility to the OpenGL implementation which could help with performance.
	//
	// To read the renderbuffers as textures, we will blit them to textures (main_colour_copy_texture etc..) using glBlitFramebuffer.
	// We will resolve the MSAA samples while doing this, so main_colour_copy_texture etc. are not MSAA textures.
	if(current_scene->render_to_main_render_framebuffer)
	{
		const int xres = myMax(16, main_viewport_w);
		const int yres = myMax(16, main_viewport_h);

		bool main_texture_size_changed = false;

		// If buffer textures are the incorrect resolution, free them, we will allocate larger ones below.
		// Free any allocated textures first, to reduce max mem usage.
		if(main_colour_copy_texture.nonNull() &&
			(((int)main_colour_copy_texture->xRes() != xres) ||
			((int)main_colour_copy_texture->yRes() != yres)))
		{
			// Free textures and render buffers before the framebuffers that they may be attached to.
			main_colour_copy_texture = NULL;
			main_normal_copy_texture = NULL;
			main_depth_copy_texture = NULL;
			transparent_accum_copy_texture = NULL;
			total_transmittance_copy_texture = NULL;

			main_colour_renderbuffer = NULL;
			main_normal_renderbuffer = NULL;
			main_depth_renderbuffer = NULL;
			transparent_accum_renderbuffer = NULL;
			total_transmittance_renderbuffer = NULL;

			main_render_framebuffer = NULL;
			main_render_copy_framebuffer = NULL;

			main_texture_size_changed = true;
		}

		if(main_colour_copy_texture.isNull())
		{
			const int msaa_samples = (settings.msaa_samples <= 1) ? -1 : settings.msaa_samples;

			const OpenGLTextureFormat normal_buffer_format = normal_texture_is_uint ? OpenGLTextureFormat::Format_RGBA_Integer_Uint8 : OpenGLTextureFormat::Format_RGBA_Linear_Uint8;

			conPrint("Allocating main render buffers and textures with width " + toString(xres) + " and height " + toString(yres));
			conPrint("MSAA samples " + toString(msaa_samples) + ", col buffer format: " + std::string(textureFormatString(col_buffer_format)));
			conPrint("normal buffer format: " + std::string(textureFormatString(normal_buffer_format)));

			main_colour_copy_texture = new OpenGLTexture(xres, yres, this,
				ArrayRef<uint8>(), // data
				col_buffer_format,
				OpenGLTexture::Filtering_Nearest,
				OpenGLTexture::Wrapping_Clamp,
				false, // has_mipmaps
				/*MSAA_samples=*/1
			);

			main_normal_copy_texture = new OpenGLTexture(xres, yres, this,
				ArrayRef<uint8>(), // data
				normal_buffer_format,
				OpenGLTexture::Filtering_Nearest,
				OpenGLTexture::Wrapping_Clamp,
				false, // has_mipmaps
				/*MSAA_samples=*/1
			);

			const OpenGLTextureFormat transparent_accum_format = col_buffer_format;
			const OpenGLTextureFormat total_transmittance_format = OpenGLTextureFormat::Format_RGBA_Linear_Half;

			if(use_order_indep_transparency)
			{
				transparent_accum_copy_texture = new OpenGLTexture(xres, yres, this,
					ArrayRef<uint8>(), // data
					transparent_accum_format,
					OpenGLTexture::Filtering_Nearest,
					OpenGLTexture::Wrapping_Clamp,
					false, // has_mipmaps
					/*MSAA_samples=*/1
				);

				total_transmittance_copy_texture = new OpenGLTexture(xres, yres, this,
					ArrayRef<uint8>(), // data
					total_transmittance_format,
					OpenGLTexture::Filtering_Nearest,
					OpenGLTexture::Wrapping_Clamp,
					false, // has_mipmaps
					/*MSAA_samples=*/1
				);
			}


			const OpenGLTextureFormat depth_format = OpenGLTextureFormat::Format_Depth_Float;

			main_depth_copy_texture = new OpenGLTexture(xres, yres, this,
				ArrayRef<uint8>(), // data
				depth_format,
				OpenGLTexture::Filtering_Nearest,
				OpenGLTexture::Wrapping_Clamp,
				false, // has_mipmaps
				/*MSAA_samples=*/1
			);


			if(texture_debug_preview_overlay_obs.size() >= 1)
				texture_debug_preview_overlay_obs[0]->material.albedo_texture = main_depth_copy_texture;

		
			// Allocate renderbuffers.  Note that these must have the same format as the textures, otherwise the glBlitFramebuffer copies will fail in WebGL.
			main_colour_renderbuffer = new RenderBuffer(xres, yres, msaa_samples, col_buffer_format);
			main_normal_renderbuffer = new RenderBuffer(xres, yres, msaa_samples, normal_buffer_format);
			main_depth_renderbuffer  = new RenderBuffer(xres, yres, msaa_samples, depth_format);
			if(use_order_indep_transparency)
			{
				transparent_accum_renderbuffer = new RenderBuffer(xres, yres, msaa_samples, transparent_accum_format);
				total_transmittance_renderbuffer  = new RenderBuffer(xres, yres, msaa_samples, total_transmittance_format);
			}

			main_render_framebuffer = new FrameBuffer();
			main_render_framebuffer->attachRenderBuffer(*main_colour_renderbuffer, GL_COLOR_ATTACHMENT0);
			main_render_framebuffer->attachRenderBuffer(*main_normal_renderbuffer, GL_COLOR_ATTACHMENT1);
			main_render_framebuffer->attachRenderBuffer(*main_depth_renderbuffer, GL_DEPTH_ATTACHMENT);
			assert(main_render_framebuffer->isComplete());


			main_render_copy_framebuffer = new FrameBuffer();
			main_render_copy_framebuffer->attachTexture(*main_colour_copy_texture, GL_COLOR_ATTACHMENT0);
			main_render_copy_framebuffer->attachTexture(*main_normal_copy_texture, GL_COLOR_ATTACHMENT1);
			main_render_copy_framebuffer->attachTexture(*main_depth_copy_texture, GL_DEPTH_ATTACHMENT);
			assert(main_render_copy_framebuffer->isComplete());


			// TODO: create as needed, not unconditionally.
			pre_dof_colour_texture = new OpenGLTexture(xres, yres, this,
				ArrayRef<uint8>(), // data
				col_buffer_format,
				OpenGLTexture::Filtering_Nearest,
				OpenGLTexture::Wrapping_Clamp,
				false, // has_mipmaps
				/*MSAA_samples=*/1
			);
			pre_dof_framebuffer = new FrameBuffer();
			pre_dof_framebuffer->attachTexture(*pre_dof_colour_texture, GL_COLOR_ATTACHMENT0);

			post_dof_colour_texture = new OpenGLTexture(xres, yres, this,
				ArrayRef<uint8>(), // data
				col_buffer_format,
				OpenGLTexture::Filtering_Nearest,
				OpenGLTexture::Wrapping_Clamp,
				false, // has_mipmaps
				/*MSAA_samples=*/1
			);
			post_dof_framebuffer = new FrameBuffer();
			post_dof_framebuffer->attachTexture(*post_dof_colour_texture, GL_COLOR_ATTACHMENT0);


			bindStandardTexturesToTextureUnits(); // Rebind textures as we have a new main_colour_copy_texture etc. that needs to get rebound.
		}

		if(settings.ssao && (!ssao_texture || main_texture_size_changed))
		{
			createSSAOTextures();
			bindStandardTexturesToTextureUnits(); // Rebind textures as we have a new main_colour_copy_texture etc. that needs to get rebound.
		}

		main_render_framebuffer->bindForDrawing();
	}
	else
	{
		// Bind requested target frame buffer as output buffer
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, this->target_frame_buffer.nonNull() ? this->target_frame_buffer->buffer_name : 0);
	}


	glViewport(0, 0, viewport_w, viewport_h); // Viewport may have been changed by shadow mapping.
	
#if !defined(OSX)
	if(use_reverse_z)
		glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
#endif
	
	glDepthFunc(use_reverse_z ? GL_GREATER : GL_LESS);
	//glDepthFunc(use_reverse_z ? GL_GEQUAL : GL_LEQUAL);

	if(current_scene->render_to_main_render_framebuffer)
	{
		// Bind normal texture as the second colour target.  Need to do this here as transparent object render pass changes this binding.
		main_render_framebuffer->attachRenderBuffer(*main_colour_renderbuffer, GL_COLOR_ATTACHMENT0);
		main_render_framebuffer->attachRenderBuffer(*main_normal_renderbuffer, GL_COLOR_ATTACHMENT1);

		// Draw to all colour buffers: colour and normal buffer.
		setTwoDrawBuffers(GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1);

		// Clear colour render buffer
		{
			const float clear_val[4] = { current_scene->background_colour.r, current_scene->background_colour.g, current_scene->background_colour.b, 1.f};
			glClearBufferfv(GL_COLOR, /*drawbuffer=*/0, clear_val);
		}

		// Clear normal render buffer.  Note that we have to use the uint version for clearing the normal buffer if it's a uint format.
		if(normal_texture_is_uint)
		{
			const GLuint clear_val[] = { 0, 0, 0, 0 };
			glClearBufferuiv(GL_COLOR, /*drawbuffer=*/1, clear_val);
		}
		else
		{
			const float clear_val[4] = { 0, 0, 0, 0 };
			glClearBufferfv(GL_COLOR, /*drawbuffer=*/1, clear_val);
		}
	}
	else
	{
		// Clear colour render buffer
		{
			const float clear_val[4] = { current_scene->background_colour.r, current_scene->background_colour.g, current_scene->background_colour.b, 1.f};
			glClearBufferfv(GL_COLOR, /*drawbuffer=*/0, clear_val);
		}
	}

	// Clear depth buffer
	{
		const float val = use_reverse_z ? 0.0f : 1.f; // For reversed-z, the 'far' z value is 0, instead of 1.
		glClearBufferfv(GL_DEPTH, /*drawbuffer=*/0, &val);
	}
	
	glLineWidth(1);

	
	
	//================= Compute projection matrix from camera settings =================
	const double z_far  = current_scene->max_draw_dist;  // Distance to far clipping plane
	const double z_near = current_scene->near_draw_dist; // Distance to near clipping plane

	// The usual OpenGL projection matrix and w divide maps z values to [-1, 1] after the w divide.  See http://www.songho.ca/opengl/gl_projectionmatrix.html
	// Use a 'z-reversal matrix', to Change this mapping to [0, 1] while reversing values.  See https://dev.theomader.com/depth-precision/ for details.
	const Matrix4f reverse_z_matrix     = getReverseZMatrixOrIdentity();

	Matrix4f proj_matrix;
	if(current_scene->camera_type == OpenGLScene::CameraType_Perspective)
	{
		const double w = 0.5 * current_scene->use_sensor_width  / current_scene->lens_sensor_dist; // Half width of camera view frustum at distance 1 from camera.
		const double h = 0.5 * current_scene->use_sensor_height / current_scene->lens_sensor_dist;

		const double unit_shift_up    = this->current_scene->lens_shift_up_distance    / this->current_scene->lens_sensor_dist;
		const double unit_shift_right = this->current_scene->lens_shift_right_distance / this->current_scene->lens_sensor_dist;

		const double x_min = z_near * (-w + unit_shift_right);
		const double x_max = z_near * ( w + unit_shift_right);
		const double y_min = z_near * (-h + unit_shift_up);
		const double y_max = z_near * ( h + unit_shift_up);

		const Matrix4f raw_proj_matrix     = infiniteFrustumMatrix(x_min, x_max, y_min, y_max, z_near);
		if(use_reverse_z)
		{
			proj_matrix = reverse_z_matrix * raw_proj_matrix;
#ifndef NDEBUG
			{
				Vec4f test_v = raw_proj_matrix * Vec4f(0, 0, -(float)z_near, 1.f);
				assert(epsEqual(test_v, Vec4f(0, 0, -(float)z_near, (float)z_near)));// Gets mapped to -1 after w_c divide.
			}
			{
				Vec4f test_v = proj_matrix * Vec4f(0, 0, -(float)z_near, 1.f);
				assert(epsEqual(test_v, Vec4f(0, 0, (float)z_near, (float)z_near))); // Gets mapped to +1 after w_c divide.
			}
#endif
			/*{
				const float far_dist = 1.0e6f;
				Vec4f test_v = raw_proj_matrix * Vec4f(0, 0, -far_dist, 1.f);
				const float ndc_z = test_v[2] / test_v[3];
				assert(epsEqual(ndc_z, 1.f));// Gets mapped to +1 after w divide.
				//assert(approxEq(test_v, Vec4f(0, 0, far_dist, far_dist)));// Gets mapped to +1 after w divide.
			}
			{
				const float far_dist = 1.0e6f;
				Vec4f test_v = proj_matrix * Vec4f(0, 0, -far_dist, 1.f);
				const float ndc_z = test_v[2] / test_v[3];
				assert(epsEqual(ndc_z, 0.f));// Gets mapped to +1 after w divide.

				//assert(approxEq(test_v, Vec4f(0, 0, 0, far_dist)));// Gets mapped to 0 after w divide.
			}*/
		}
		else
			proj_matrix = raw_proj_matrix;
	}
	else if(current_scene->camera_type == OpenGLScene::CameraType_Orthographic || current_scene->camera_type == OpenGLScene::CameraType_DiagonalOrthographic)
	{
		const double w = 0.5 * current_scene->use_sensor_width; // Half width of camera view frustum at distance 1 from camera.
		const double h = 0.5 * current_scene->use_sensor_height;

		if(current_scene->camera_type == OpenGLScene::CameraType_Orthographic)
		{
			proj_matrix = reverse_z_matrix * orthoMatrix(
				-w, // left
				w, // right
				-h, // bottom
				h, // top
				z_near,
				z_far
			);
		}
		else
		{
			const float cam_z = current_scene->cam_to_world.getColumn(3)[2];
			
			proj_matrix = reverse_z_matrix * diagonalOrthoMatrix(
				-w, // left
				w, // right
				-h, // bottom
				h, // top
				z_near,
				z_far,
				cam_z
			);
		}
	}
	else if(current_scene->camera_type == OpenGLScene::CameraType_Identity)
	{
		proj_matrix = Matrix4f::identity();
	}

	const Matrix4f view_matrix = main_view_matrix;

#if !defined(EMSCRIPTEN)
	// Draw solid polygons
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

	
	//================= Update shared vert uniforms buffer =================
	{
		SharedVertUniforms uniforms;
		uniforms.proj_matrix = proj_matrix;
		uniforms.view_matrix = view_matrix;

		Matrix4f tex_matrices[ShadowMapping::NUM_DYNAMIC_DEPTH_TEXTURES + ShadowMapping::NUM_STATIC_DEPTH_TEXTURES];

		if(shadow_mapping.nonNull())
		{
			// The first matrices in our packed array are the dynamic tex matrices
			for(int i = 0; i < ShadowMapping::NUM_DYNAMIC_DEPTH_TEXTURES; ++i)
				tex_matrices[i] = shadow_mapping->dynamic_tex_matrix[i];

			// Following those are the static tex matrices.  Use cur_static_depth_tex to select the current (complete) depth tex.
			for(int i = 0; i < ShadowMapping::NUM_STATIC_DEPTH_TEXTURES; ++i)
				tex_matrices[ShadowMapping::NUM_DYNAMIC_DEPTH_TEXTURES + i] = shadow_mapping->static_tex_matrix[shadow_mapping->cur_static_depth_tex * ShadowMapping::NUM_STATIC_DEPTH_TEXTURES + i];
		}
		else
			for(int i = 0; i < ShadowMapping::NUM_DYNAMIC_DEPTH_TEXTURES + ShadowMapping::NUM_STATIC_DEPTH_TEXTURES; ++i)
				tex_matrices[i] = Matrix4f::identity();

		for(int i = 0; i < ShadowMapping::NUM_DYNAMIC_DEPTH_TEXTURES + ShadowMapping::NUM_STATIC_DEPTH_TEXTURES; ++i)
			uniforms.shadow_texture_matrix[i] = tex_matrices[i];

		uniforms.campos_ws = current_scene->cam_to_world.getColumn(3);
		uniforms.vert_sundir_ws = this->sun_dir;
		uniforms.grass_pusher_sphere_pos = current_scene->grass_pusher_sphere_pos;
		uniforms.vert_uniforms_time = current_time;
		uniforms.wind_strength = current_scene->wind_strength;

		this->shared_vert_uniform_buf_ob->updateData(/*dest offset=*/0, &uniforms, sizeof(SharedVertUniforms));


		// Set shadow_texture_matrix in MaterialCommonUniforms
		for(int i = 0; i < ShadowMapping::NUM_DYNAMIC_DEPTH_TEXTURES + ShadowMapping::NUM_STATIC_DEPTH_TEXTURES; ++i)
			common_uniforms.shadow_texture_matrix[i] = tex_matrices[i];
		this->material_common_uniform_buf_ob->updateData(/*dest offset=*/0, &common_uniforms, sizeof(MaterialCommonUniforms));
	}


	// Each pass:
	// * binds the framebuffer the pass will draw to.
	// * Sets the buffers it will draw to, with e.g. setSingleDrawBuffer or setTwoDrawBuffers.
	// * Preserves the standard render buffer attachment bindings for main_render_framebuffer: 0 = colour, 1 = normal, also the depth attachment binding.
	// * Preserves the standard texture attachment bindings for main_render_copy_framebuffer: 0 = colour, 1 = normal, also the depth attachment binding.
	// * preserves viewport as (0, 0, viewport_w, viewport_h)
	//================= Generate outline texture =================
	generateOutlineTexture(view_matrix, proj_matrix);
	
	//================= Draw background env map =================
	// Draw background env map if there is one. (or if we are using a non-standard env shader)
	drawBackgroundEnvMap(view_matrix, proj_matrix);

	if(settings.ssao)
	{
		common_uniforms.mat_common_flags = (this->current_scene->cloud_shadows ? CLOUD_SHADOWS_FLAG : 0) | DOING_SSAO_PREPASS_FLAG; // Disable reading from SSAO output texture (DO_SSAO_FLAG) for the prepass, set DOING_SSAO_PREPASS_FLAG.
		this->material_common_uniform_buf_ob->updateData(/*dest offset=*/0, &common_uniforms, sizeof(MaterialCommonUniforms));

		// drawDepthPrePass(view_matrix, proj_matrix);
		drawColourAndDepthPrePass(view_matrix, proj_matrix);

		computeSSAO(proj_matrix);

		// Restore flags
		common_uniforms.mat_common_flags = (this->current_scene->cloud_shadows ? CLOUD_SHADOWS_FLAG : 0) | ((this->settings.ssao && show_ssao) ? DO_SSAO_FLAG : 0);
		this->material_common_uniform_buf_ob->updateData(/*dest offset=*/0, &common_uniforms, sizeof(MaterialCommonUniforms));
	}

	//================= Draw non-transparent (opaque) batches from objects =================
	drawNonTransparentMaterialBatches(view_matrix, proj_matrix);

	//================= Draw water objects =================
	drawWaterObjects(view_matrix, proj_matrix);

	//================= Draw decals =================
	drawDecals(view_matrix, proj_matrix);

	//================= Draw triangle batches with that use alpha-blending (e.g. participating media materials / particles, text objects) =================
	drawAlphaBlendedObjects(view_matrix, proj_matrix);
	
	//================= Draw triangle batches with transparent materials =================
	drawTransparentMaterialBatches(view_matrix, proj_matrix);

	//================= Draw always-visible objects =================
	drawAlwaysVisibleObjects(view_matrix, proj_matrix);
	
	//================= Draw outlines around selected objects =================
	drawOutlinesAroundSelectedObjects();

	if(current_scene->render_to_main_render_framebuffer)
	{
		ZoneScopedN("Post process"); // Tracy profiler
		TracyGpuZone("Post process");

		// We have rendered to main_render_framebuffer / main_colour_texture / main_normal_texture / main_depth_texture.

		//================================= Do post processes: bloom effect =================================

		// Generate downsized image
		/*
		This is the computation graph for computing downsized and blurred buffers:

		---------------> full buffer -------------> downsize_framebuffers[0] ------------> blur_framebuffers_x[0] -----------> blur_framebuffers[0]
		 render scene                 downsize            |                     blur x                              blur y       
		                                                  |
		                                                  |--------------> downsize_framebuffers[1] -------> blur_framebuffers_x[1] ------------>  blur_framebuffers[1]
		                                                       downsize               |             blur x                             blur y
		                                                                              |
		                                                                              -----------> downsize_framebuffers[2] -----------> blur_framebuffers_x[2] ------------>  blur_framebuffers[2]
		                                                                                  downsize              |             blur x                             blur y
		
		All blurred low res buffers are then read from and added to the resulting buffer.
		*/
		const int use_main_viewport_w = myMax(16, main_viewport_w);
		const int use_main_viewport_h = myMax(16, main_viewport_h);

		if(downsize_target_textures.empty() ||
			((int)downsize_framebuffers[0]->xRes() != (use_main_viewport_w/2) || (int)downsize_framebuffers[0]->yRes() != (use_main_viewport_h/2))
			)
		{
			conPrint("(Re)Allocing downsize_framebuffers etc..");

			// Use main viewport w + h for this.  This is because we don't want to keep reallocating these textures for random setViewport calls.
			//assert(main_viewport_w > 0);
			int w = use_main_viewport_w;
			int h = use_main_viewport_h;

			downsize_target_textures.resize(NUM_BLUR_DOWNSIZES);
			downsize_framebuffers   .resize(NUM_BLUR_DOWNSIZES);
			blur_target_textures_x  .resize(NUM_BLUR_DOWNSIZES);
			blur_framebuffers_x     .resize(NUM_BLUR_DOWNSIZES);
			blur_target_textures    .resize(NUM_BLUR_DOWNSIZES);
			blur_framebuffers       .resize(NUM_BLUR_DOWNSIZES);

			// Free any allocated textures first, to reduce max mem usage.
			for(int i=0; i<NUM_BLUR_DOWNSIZES; ++i)
			{
				downsize_target_textures[i] = NULL;
				blur_target_textures_x[i] = NULL;
				blur_target_textures[i] = NULL;

				downsize_framebuffers[i] = NULL;
				blur_framebuffers_x[i] = NULL;
				blur_framebuffers[i] = NULL;
			}

			glDepthMask(GL_FALSE); // Disable writing to depth buffer.  Do this before checking for framebuffer completeness below.

#if EMSCRIPTEN
			// Suppress 'drawElementsInstanced: Tex image TEXTURE_2D level 0 is incurring lazy initialization.' WebGL warning by passing some initial texture data.
			js::Vector<uint8, 16> temp_data((w / 2) * (h / 2) * /*num components=*/4 * /*sizeof half float=*/2);
			ArrayRef<uint8> initial_data_ref(temp_data);
#else
			ArrayRef<uint8> initial_data_ref(NULL, 0);
#endif

			for(int i=0; i<NUM_BLUR_DOWNSIZES; ++i)
			{
				w = myMax(1, w / 2);
				h = myMax(1, h / 2);

				// Clamp texture reads otherwise edge outlines will wrap around to other side of frame.
				downsize_target_textures[i] = new OpenGLTexture(w, h, this, ArrayRef<uint8>(), col_buffer_format, OpenGLTexture::Filtering_Nearest, OpenGLTexture::Wrapping_Clamp);
				downsize_framebuffers[i] = new FrameBuffer();
				downsize_framebuffers[i]->attachTexture(*downsize_target_textures[i], GL_COLOR_ATTACHMENT0);

				if(!downsize_framebuffers[i]->isComplete())
				{
					conPrint("Error: downsize_framebuffers: framebuffer is not complete.");
					assert(0);
				}


				blur_target_textures_x[i] = new OpenGLTexture(w, h, this, ArrayRef<uint8>(), col_buffer_format, OpenGLTexture::Filtering_Nearest, OpenGLTexture::Wrapping_Clamp);
				blur_framebuffers_x[i] = new FrameBuffer();
				blur_framebuffers_x[i]->attachTexture(*blur_target_textures_x[i], GL_COLOR_ATTACHMENT0);

				// Use bilinear, this tex is read from and accumulated into final buffer.
				blur_target_textures[i] = new OpenGLTexture(w, h, this, initial_data_ref, col_buffer_format, OpenGLTexture::Filtering_Bilinear, OpenGLTexture::Wrapping_Clamp);
				blur_framebuffers[i] = new FrameBuffer();
				blur_framebuffers[i]->attachTexture(*blur_target_textures[i], GL_COLOR_ATTACHMENT0);
			}
		}


		const bool do_DOF_blur = current_scene->dof_blur_strength > 0;

		//=============== Copy from renderbuffer to our framebuffer copy with textures bound, so we can access main buffer as a colour texture ============
		main_render_framebuffer->bindForReading();
		main_render_copy_framebuffer->bindForDrawing();

		// Copy colour and depth renderbuffer to depth texture
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		setSingleDrawBuffer(GL_COLOR_ATTACHMENT0);
		glBlitFramebuffer(
			/*srcX0=*/0, /*srcY0=*/0, /*srcX1=*/(int)main_render_framebuffer->xRes(), /*srcY1=*/(int)main_render_framebuffer->yRes(), 
			/*dstX0=*/0, /*dstY0=*/0, /*dstX1=*/(int)main_render_framebuffer->xRes(), /*dstY1=*/(int)main_render_framebuffer->yRes(), 
			GL_COLOR_BUFFER_BIT | (do_DOF_blur ? GL_DEPTH_BUFFER_BIT : 0), // We need the depth buffer only when doing DOF blur.
			GL_NEAREST
		);

		/*
		This is the case where we do both order-independent transparency (OIT) and DOF blur:

		main_render_copy_framebuffer (main_colour_copy_texture, main_depth_copy_texture, ...)
		  |
		   OIT compositing (OIT_composite_prog)
		  |
		  v
		pre_dof_framebuffer (pre_dof_colour_texture)
		  |
		    DOF bokeh blur (dof_blur_prog)
		  |
		  v
		post_dof_framebuffer (post_dof_colour_texture)
		  |       |
		  |       bloom downsample and blur (downsize_prog, gaussian_blur_prog)
		  |       |
		  |       v
		  |     blur_framebuffers
		   \      |
		    \     |
		      \   |
		       v  v
		        final imaging (final_imaging_prog)
		        |
		        v
		target_frame_buffer
		*/

		//================= Do order-independent transparency compositing =================
		OpenGLTexture* current_colour_tex_input = main_colour_copy_texture.ptr();
		if(use_order_indep_transparency)
		{
			doOITCompositing();

			current_colour_tex_input = pre_dof_colour_texture.ptr(); // doOITCompositing writes to pre_dof_colour_texture
		}

		//================= Do DOF blur =================
		if(do_DOF_blur)
		{
			doDOFBlur(current_colour_tex_input);

			current_colour_tex_input = post_dof_colour_texture.ptr(); // doDOFBlur() writes to post_dof_colour_texture
		}

		//================= Do postprocess bloom =================
		doBloomPostProcess(current_colour_tex_input);
		
		//================= Do final imaging =================
		// Final imaging is order-independent transparency compositing, adding together bloom results, and doing tonemapping.
		// See final_imaging_frag_shader.glsl
		doFinalImaging(current_colour_tex_input);

	} // End if(current_scene->render_to_main_render_framebuffer)


	//================= Draw UI overlay objects =================
	// We will do this after the final imaging shader runs above, so that the UI is drawn over transparent objects.
	if(current_scene->draw_overlay_objects)
		drawUIOverlayObjects(reverse_z_matrix);


	VAO::unbind(); // Unbind any bound VAO, so that its vertex and index buffers don't get accidentally overridden.
	glActiveTexture(GL_TEXTURE0); // Make sure we don't overwrite a texture binding to a non-zero texture unit (tex unit zero is the scratch texture unit), while loading data into textures or creating new textures, outside of this draw() call.

	if(query_profiling_enabled && current_scene->collect_stats)
	{
#if EMSCRIPTEN
		buffered_total_timer->recordEnd();
#else
		end_query->recordTimestamp();
#endif
	}

	if(query_profiling_enabled && current_scene->collect_stats)
	{
#if EMSCRIPTEN
		last_total_draw_GPU_time = buffered_total_timer->getLastTimeElapsed();
#else
		last_total_draw_GPU_time = end_query->getLastTimestamp() - start_query->getLastTimestamp();
#endif

		if(dynamic_depth_draw_gpu_timer->waitingForResult() && dynamic_depth_draw_gpu_timer->checkResultAvailable())
			last_dynamic_depth_draw_GPU_time = dynamic_depth_draw_gpu_timer->getTimeElapsed();

		if(static_depth_draw_gpu_timer->waitingForResult() && static_depth_draw_gpu_timer->checkResultAvailable())
			last_static_depth_draw_GPU_time = static_depth_draw_gpu_timer->getTimeElapsed();

		if(draw_opaque_obs_gpu_timer->waitingForResult() && draw_opaque_obs_gpu_timer->checkResultAvailable())
			last_draw_opaque_obs_GPU_time = draw_opaque_obs_gpu_timer->getTimeElapsed();

		if(depth_pre_pass_gpu_timer->waitingForResult() && depth_pre_pass_gpu_timer->checkResultAvailable())
			last_depth_pre_pass_GPU_time = depth_pre_pass_gpu_timer->getTimeElapsed();
		
		if(compute_ssao_gpu_timer->waitingForResult() && compute_ssao_gpu_timer->checkResultAvailable())
			last_compute_ssao_GPU_time = compute_ssao_gpu_timer->getTimeElapsed();

		if(blur_ssao_gpu_timer->waitingForResult() && blur_ssao_gpu_timer->checkResultAvailable())
			last_blur_ssao_GPU_time = blur_ssao_gpu_timer->getTimeElapsed();

		if(copy_prepass_buffers_gpu_timer->waitingForResult() && copy_prepass_buffers_gpu_timer->checkResultAvailable())
			last_copy_prepass_buffers_GPU_time = copy_prepass_buffers_gpu_timer->getTimeElapsed();

		if(decal_copy_buffers_timer->waitingForResult() && decal_copy_buffers_timer->checkResultAvailable())
			last_decal_copy_buffers_GPU_time = decal_copy_buffers_timer->getTimeElapsed();

		if(draw_overlays_gpu_timer->waitingForResult() && draw_overlays_gpu_timer->checkResultAvailable())
			last_draw_overlay_obs_GPU_time = draw_overlays_gpu_timer->getTimeElapsed();
	}

	if(current_scene->collect_stats)
	{
		last_draw_CPU_time = draw_method_timer.elapsed();
		last_anim_update_duration = anim_update_duration;

		num_frames_since_fps_timer_reset++;
		if(fps_display_timer.elapsed() > 1.0)
		{
			last_fps = num_frames_since_fps_timer_reset / fps_display_timer.elapsed();
			num_frames_since_fps_timer_reset = 0;
			fps_display_timer.reset();
		}
	}

	add_debug_obs = false;


	// DEBUG: print out texture values
	if(0)//compute_ssao_texture)
	{
		// assert(ssao_texture->getFormat() == OpenGLTextureFormat::Format_RGBA_Linear_Half);
		// ImageMap<half, HalfComponentValueTraits> image_map(ssao_texture->xRes(), ssao_texture->yRes(), 4);
		// ssao_texture->readBackTexture(/*mipmap level=*/0, ArrayRef<uint8>((uint8*)image_map.getData(), image_map.getDataSizeB()));
		// 
		// conPrint("SSAO tex centre val: " + 
		// 	doubleToStringNDecimalPlaces(image_map.getPixel(image_map.getWidth()/2, image_map.getHeight()/2)[0], 3) + ", " +
		// 	doubleToStringNDecimalPlaces(image_map.getPixel(image_map.getWidth()/2, image_map.getHeight()/2)[1], 3) + ", " +
		// 	doubleToStringNDecimalPlaces(image_map.getPixel(image_map.getWidth()/2, image_map.getHeight()/2)[2], 3) + ", " +
		// 	doubleToStringNDecimalPlaces(image_map.getPixel(image_map.getWidth()/2, image_map.getHeight()/2)[3], 3)
		// );
	}

	//PerformanceAPI_EndEvent();
}


inline static void setFaceCulling(uint32 culling_bits)
{
	if(culling_bits != 0)
	{
		assert((culling_bits == SHIFTED_CULL_BACKFACE_BITS) || (culling_bits == SHIFTED_CULL_FRONTFACE_BITS));

		glEnable(GL_CULL_FACE);
		glCullFace((culling_bits == SHIFTED_CULL_BACKFACE_BITS) ? GL_BACK : GL_FRONT);
	}
	else
		glDisable(GL_CULL_FACE);
}


[[maybe_unused]] inline static std::string faceCullingBitsDescrip(uint32 culling_bits)
{
	if(culling_bits != 0)
	{
		assert((culling_bits == SHIFTED_CULL_BACKFACE_BITS) || (culling_bits == SHIFTED_CULL_FRONTFACE_BITS));
		return (culling_bits == SHIFTED_CULL_BACKFACE_BITS) ? "GL_BACK" : "GL_FRONT";
	}
	else
		return "No culling";
}


// Input: main_colour_copy_texture, transparent_accum_copy_texture, total_transmittance_copy_texture
// Output: pre_dof_framebuffer (pre_dof_colour_texture)
void OpenGLEngine::doOITCompositing()
{
	assert(use_order_indep_transparency);

	DebugGroup debug_group("doOITCompositing()");
	TracyGpuZone("doOITCompositing");

	//----------------------------- Setup -----------------------------
	glDepthMask(GL_FALSE); // Don't write to z-buffer, depth not needed.
	glDisable(GL_DEPTH_TEST); // Don't depth test

	pre_dof_framebuffer->bindForDrawing();
	OIT_composite_prog->useProgram();
	bindMeshData(*unit_quad_meshdata);

	// Bind main_colour_texture as albedo_texture with texture unit 0
	bindTextureUnitToSampler(*main_colour_copy_texture, /*texture_unit_index=*/0, /*sampler_uniform_location=*/OIT_composite_prog->albedo_texture_loc);

	// Bind transparent_accum_texture as texture_2 with texture unit 1
	bindTextureUnitToSampler(*transparent_accum_copy_texture, /*texture_unit_index=*/1, /*sampler_uniform_location=*/OIT_composite_prog->user_uniform_info[OIT_COMPOSITE_TRANSPARENT_ACCUM_TEX_UNIFORM_INDEX].loc);

	// Bind total_transmittance_texture as texture_2 with texture unit 2
	bindTextureUnitToSampler(*total_transmittance_copy_texture, /*texture_unit_index=*/2, /*sampler_uniform_location=*/OIT_composite_prog->user_uniform_info[OIT_COMPOSITE_TOTAL_TRANSMITTANCE_TEX_UNIFORM_INDEX].loc);

	//----------------------------- Draw the quad -----------------------------
	drawElementsBaseVertex(GL_TRIANGLES, (GLsizei)unit_quad_meshdata->batches[0].num_indices, unit_quad_meshdata->getIndexType(), (void*)unit_quad_meshdata->getBatch0IndicesTotalBufferOffset(), unit_quad_meshdata->vbo_handle.base_vertex);

	//----------------------------- Cleanup -----------------------------
	OpenGLProgram::useNoPrograms();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE); // Restore writing to z-buffer.

	// Unbind textures from texture units.  Otherwise we get errors in Chrome: "GL_INVALID_OPERATION: Feedback loop formed between Framebuffer and active Texture."
	unbindTextureFromTextureUnit(*main_colour_copy_texture, /*texture_unit_index=*/0);
	if(use_order_indep_transparency)
	{
		unbindTextureFromTextureUnit(*transparent_accum_copy_texture, /*texture_unit_index=*/1);
		unbindTextureFromTextureUnit(*total_transmittance_copy_texture, /*texture_unit_index=*/2);
	}
}


// Input: colour_tex_input
// Output: post_dof_colour_texture (post_dof_colour_texture)
void OpenGLEngine::doDOFBlur(OpenGLTexture* colour_tex_input)
{
	DebugGroup debug_group("doDOFBlur()");
	TracyGpuZone("doDOFBlur");

	//----------------------------- Setup -----------------------------
	glDepthMask(GL_FALSE); // Don't write to z-buffer, depth not needed.
	glDisable(GL_DEPTH_TEST); // Don't depth test

	post_dof_framebuffer->bindForDrawing();
	dof_blur_prog->useProgram();
	bindMeshData(*unit_quad_meshdata);

	bindTextureUnitToSampler(*colour_tex_input,        /*texture_unit_index=*/0, /*sampler_uniform_location=*/dof_blur_prog->albedo_texture_loc);
	bindTextureUnitToSampler(*main_depth_copy_texture, /*texture_unit_index=*/1, /*sampler_uniform_location=*/dof_blur_depth_tex_location);

	glUniform1f(dof_blur_strength_location,       current_scene->dof_blur_strength);
	glUniform1f(dof_blur_focus_distance_location, current_scene->dof_blur_focus_distance);

	//----------------------------- Draw the quad -----------------------------
	drawElementsBaseVertex(GL_TRIANGLES, (GLsizei)unit_quad_meshdata->batches[0].num_indices, unit_quad_meshdata->getIndexType(), (void*)unit_quad_meshdata->getBatch0IndicesTotalBufferOffset(), unit_quad_meshdata->vbo_handle.base_vertex);

	//----------------------------- Cleanup -----------------------------
	OpenGLProgram::useNoPrograms();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE); // Restore writing to z-buffer.

	// Unbind textures from texture units.  Otherwise we get errors in Chrome: "GL_INVALID_OPERATION: Feedback loop formed between Framebuffer and active Texture."
	unbindTextureFromTextureUnit(*colour_tex_input,        /*texture_unit_index=*/0);
	unbindTextureFromTextureUnit(*main_depth_copy_texture, /*texture_unit_index=*/1);
}


// Input: colour_tex_input
// Output: blur_framebuffers
void OpenGLEngine::doBloomPostProcess(OpenGLTexture* colour_tex_input)
{
	// // Code to Compute gaussian blur weights:
	// float sum = 0;
	// for(int x=-2; x<=2; ++x)
	// 	sum += Maths::eval1DGaussian(x, /*standard dev=*/1.0);
	   
	// for(int x=-2; x<=2; ++x)
	// {
	// 	const float weight = Maths::eval1DGaussian(x, /*standard dev=*/1.0);
	// 	const float normed_weight = weight / sum;
	// 	conPrintStr(" " + toString(normed_weight));
	// }

	if(current_scene->bloom_strength > 0)
	{
		DebugGroup debug_group2("bloom");
		TracyGpuZone("bloom");

		glDepthMask(GL_FALSE); // Don't write to z-buffer, depth not needed.
		glDisable(GL_DEPTH_TEST); // Don't depth test

		bindMeshData(*unit_quad_meshdata);
		assert(unit_quad_meshdata->batches.size() == 1);
		const size_t unit_quad_total_buffer_offset = unit_quad_meshdata->getBatch0IndicesTotalBufferOffset();

		for(int i=0; i<(int)downsize_target_textures.size(); ++i)
		{
			//-------------------------------- Execute downsize shader --------------------------------
			// Reads from post_dof_colour_texture or downsize_target_textures[i-1], writes to downsize_framebuffers[i]

			downsize_framebuffers[i]->bindForDrawing(); // Target downsize_framebuffers[i]
			glViewport(0, 0, (int)downsize_framebuffers[i]->xRes(), (int)downsize_framebuffers[i]->yRes()); // Set viewport to target texture size

			OpenGLProgram* use_downsize_prog = (i == 0) ? downsize_from_main_buf_prog.ptr() : downsize_prog.ptr();
			use_downsize_prog->useProgram();

			OpenGLTexture* src_texture = (i == 0) ? colour_tex_input : downsize_target_textures[i - 1].ptr();
			bindTextureUnitToSampler(*src_texture, /*texture_unit_index=*/0, /*sampler_uniform_location=*/use_downsize_prog->albedo_texture_loc);

			//if((i == 0) && use_order_indep_transparency)
			//{
			//	bindTextureUnitToSampler(*transparent_accum_copy_texture,    /*texture_unit_index=*/1, /*sampler_uniform_location=*/use_downsize_prog->user_uniform_info[DOWNSIZE_TRANSPARENT_ACCUM_TEX_UNIFORM_INDEX].loc);
			//	bindTextureUnitToSampler(*total_transmittance_copy_texture,  /*texture_unit_index=*/2, /*sampler_uniform_location=*/use_downsize_prog->user_uniform_info[DOWNSIZE_TOTAL_TRANSMITTANCE_TEX_UNIFORM_INDEX].loc);
			//}
			drawElementsBaseVertex(GL_TRIANGLES, (GLsizei)unit_quad_meshdata->batches[0].num_indices, unit_quad_meshdata->getIndexType(), (void*)unit_quad_total_buffer_offset, unit_quad_meshdata->vbo_handle.base_vertex); // Draw quad


			//-------------------------------- Execute blur shader in x direction --------------------------------
			// Reads from downsize_target_textures[i], writes to blur_framebuffers_x[i]/blur_target_textures_x[i].

			blur_framebuffers_x[i]->bindForDrawing(); // Target blur_framebuffers_x[i]

			gaussian_blur_prog->useProgram();

			glUniform1i(gaussian_blur_prog->user_uniform_info[0].loc, /*val=*/1); // Set blur_x = 1

			bindTextureUnitToSampler(*downsize_target_textures[i], /*texture_unit_index=*/0, /*sampler_uniform_location=*/gaussian_blur_prog->albedo_texture_loc);

			drawElementsBaseVertex(GL_TRIANGLES, (GLsizei)unit_quad_meshdata->batches[0].num_indices, unit_quad_meshdata->getIndexType(), (void*)unit_quad_total_buffer_offset, unit_quad_meshdata->vbo_handle.base_vertex); // Draw quad


			//-------------------------------- Execute blur shader in y direction --------------------------------
			// Reads from blur_target_textures_x[i], writes to blur_framebuffers[i]/blur_target_textures[i].

			blur_framebuffers[i]->bindForDrawing(); // Target blur_framebuffers[i]

			glUniform1i(gaussian_blur_prog->user_uniform_info[0].loc, /*val=*/0); // Set blur_x = 0

			bindTextureUnitToSampler(*blur_target_textures_x[i], /*texture_unit_index=*/0, /*sampler_uniform_location=*/gaussian_blur_prog->albedo_texture_loc);

			drawElementsBaseVertex(GL_TRIANGLES, (GLsizei)unit_quad_meshdata->batches[0].num_indices, unit_quad_meshdata->getIndexType(), (void*)unit_quad_total_buffer_offset, unit_quad_meshdata->vbo_handle.base_vertex); // Draw quad
		}

		//----------------------------- Cleanup -----------------------------
		unbindTextureFromTextureUnit(GL_TEXTURE_2D, /*texture_unit_index=*/0);
		unbindTextureFromTextureUnit(GL_TEXTURE_2D, /*texture_unit_index=*/1);
		unbindTextureFromTextureUnit(GL_TEXTURE_2D, /*texture_unit_index=*/2);
		OpenGLProgram::useNoPrograms();
		glDepthMask(GL_TRUE); // Restore writing to z-buffer.
		glEnable(GL_DEPTH_TEST);
	
		glViewport(0, 0, viewport_w, viewport_h); // Restore viewport
	}
}


// Final imaging adds together bloom results, and does tonemapping
// See final_imaging_frag_shader.glsl
void OpenGLEngine::doFinalImaging(OpenGLTexture* colour_tex_input)
{
	DebugGroup debug_group3("imaging");
	TracyGpuZone("imaging");

	//----------------------------- Setup -----------------------------
	// Bind requested target frame buffer as output buffer
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, this->target_frame_buffer ? this->target_frame_buffer->buffer_name : 0);

	glDepthMask(GL_FALSE); // Don't write to z-buffer, depth not needed.
	glDisable(GL_DEPTH_TEST); // Don't depth test

	final_imaging_prog->useProgram();

	bindMeshData(*unit_quad_meshdata);
	assert(unit_quad_meshdata->batches.size() == 1);

	bindTextureUnitToSampler(*colour_tex_input, /*texture_unit_index=*/0, /*sampler_uniform_location=*/final_imaging_prog->albedo_texture_loc);

	//if(use_order_indep_transparency)
	//{
	//	// Bind transparent_accum_texture as texture_2 with texture unit 1
	//	bindTextureUnitToSampler(*transparent_accum_copy_texture, /*texture_unit_index=*/1, /*sampler_uniform_location=*/final_imaging_prog->user_uniform_info[FINAL_IMAGING_TRANSPARENT_ACCUM_TEX_UNIFORM_INDEX].loc);
	//
	//	// Bind total_transmittance_texture as texture_2 with texture unit 2
	//	bindTextureUnitToSampler(*total_transmittance_copy_texture, /*texture_unit_index=*/2, /*sampler_uniform_location=*/final_imaging_prog->user_uniform_info[FINAL_IMAGING_TOTAL_TRANSMITTANCE_TEX_UNIFORM_INDEX].loc);
	//}
			
	// Bind water_colour_texture with texture unit 3
//	bindTextureUnitToSampler(*water_colour_texture, /*texture_unit_index=*/3, /*sampler_uniform_location=*/final_imaging_prog->user_uniform_info[FINAL_IMAGING_WATER_COLOUR_TEX_UNIFORM_INDEX].loc);


	glUniform1f(final_imaging_prog->user_uniform_info[FINAL_IMAGING_BLOOM_STRENGTH_UNIFORM_INDEX].loc, current_scene->bloom_strength); // Set bloom_strength uniform
	glUniform1f(final_imaging_prog->user_uniform_info[FINAL_IMAGING_EXPOSURE_FACTOR_UNIFORM_INDEX].loc, current_scene->exposure_factor); // Set exposure_factor uniform
	glUniform1f(final_imaging_prog->user_uniform_info[FINAL_IMAGING_SATURATION_MULTIPLIER_UNIFORM_INDEX].loc, current_scene->saturation_multiplier); // Set saturation_multiplier uniform

	// Bind blur_target_textures
	// We need to bind these textures even when bloom_strength == 0, or we get runtime opengl errors.
	for(int i=0; i<NUM_BLUR_DOWNSIZES; ++i)
		bindTextureUnitToSampler(*blur_target_textures[i], /*texture_unit_index=*/4 + i, /*sampler_uniform_location=*/final_imaging_prog->user_uniform_info[FINAL_IMAGING_BLUR_TEX_UNIFORM_START + i].loc);

	//----------------------------- Draw the quad -----------------------------
	drawElementsBaseVertex(GL_TRIANGLES, (GLsizei)unit_quad_meshdata->batches[0].num_indices, unit_quad_meshdata->getIndexType(), (void*)unit_quad_meshdata->getBatch0IndicesTotalBufferOffset(), unit_quad_meshdata->vbo_handle.base_vertex);


	//----------------------------- Cleanup -----------------------------
	OpenGLProgram::useNoPrograms();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE); // Restore writing to z-buffer.


	// Unbind textures from texture units.  Otherwise we get errors in Chrome: "GL_INVALID_OPERATION: Feedback loop formed between Framebuffer and active Texture."
	unbindTextureFromTextureUnit(*colour_tex_input, /*texture_unit_index=*/0);
	//if(use_order_indep_transparency)
	//{
	//	unbindTextureFromTextureUnit(*transparent_accum_copy_texture, /*texture_unit_index=*/1);
	//	unbindTextureFromTextureUnit(*total_transmittance_copy_texture, /*texture_unit_index=*/2);
	//}
	for(int i=0; i<NUM_BLUR_DOWNSIZES; ++i)
		unbindTextureFromTextureUnit(*blur_target_textures[i], /*texture_unit_index=*/4 + i);
	

	// Now that we are done rendering, and we have run the final imaging pass that writes to target framebuffer, we don't need
	// the contents of main_render_framebuffer or main_render_copy_framebuffer any more.
	// NOTE: code disabled until it will actually be used (e.g. offscreen rendering is done on mobile)
	// main_render_framebuffer     ->discardContents(GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_DEPTH_ATTACHMENT);
	// main_render_copy_framebuffer->discardContents(GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_DEPTH_ATTACHMENT);
	// glBindFramebuffer(GL_DRAW_FRAMEBUFFER, this->target_frame_buffer.nonNull() ? this->target_frame_buffer->buffer_name : 0); // Restore framebuffer binding
}


void OpenGLEngine::renderToShadowMapDepthBuffer()
{
	assertCurrentProgramIsZero();

	if(shadow_mapping.nonNull())
	{
		DebugGroup debug_group("renderToShadowMapDepthBuffer()");
		TracyGpuZone("renderToShadowMapDepthBuffer");

		ZoneScopedN("Shadow depth draw"); // Tracy profiler

		if(query_profiling_enabled && current_scene->collect_stats && time_individual_passes && dynamic_depth_draw_gpu_timer->isIdle())
			dynamic_depth_draw_gpu_timer->beginTimerQuery();

		//-------------------- Draw dynamic depth textures ----------------
		shadow_mapping->bindDepthTexFrameBufferAsTarget();

		glDrawBuffers(/*num=*/0, NULL); // Don't draw to any colour buffers. (there are none attached to the frame buffer anyway)

		glClearDepthf(1.f);
		glClear(GL_DEPTH_BUFFER_BIT); // NOTE: not affected by current viewport dimensions.

		// Use opengl-default clip coords
#if !defined(OSX)
		if(use_reverse_z)
			glClipControl(GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE);
#endif
		glDepthFunc(GL_LESS);

		const int per_map_h = shadow_mapping->dynamic_h / shadow_mapping->numDynamicDepthTextures();

		num_prog_changes = 0;
		uint32 num_face_culling_changes = 0;
		uint32 num_batches_bound = 0;
		num_vao_binds = 0;
		num_vbo_binds = 0;

		for(int ti=0; ti<shadow_mapping->numDynamicDepthTextures(); ++ti)
		{
			DebugGroup debug_group2("dynamic depth draw");
			// Timer timer;

			glViewport(0, ti*per_map_h, shadow_mapping->dynamic_w, per_map_h);

			// Compute the 8 points making up the corner vertices of this slice of the view frustum
			float near_dist = (float)std::pow<double>(shadow_mapping->getDynamicDepthTextureScaleMultiplier(), ti);
			float far_dist = near_dist * shadow_mapping->getDynamicDepthTextureScaleMultiplier();
			if(ti == 0)
				near_dist = 0.01f;
			else
				near_dist = near_dist * 0.8f; // Corresponds to edge_dist = 0.8f * DEPTH_TEXTURE_SCALE_MULT; in phong_frag_shader.glsl.

			Vec4f frustum_verts_ws[8];
			calcCamFrustumVerts(near_dist, far_dist, frustum_verts_ws);

			// Get a basis for our distance map, where k is the direction to the sun.
			const Vec4f up(0, 0, 1, 0);
			const Vec4f k = sun_dir; // dir to sun
			const Vec4f i = normalise(crossProduct(up, sun_dir)); // right 
			const Vec4f j = crossProduct(k, i); // up

			assert(k.isUnitLength());

			const Matrix4f view_matrix = Matrix4f::fromRows(i, j, k, Vec4f(0, 0, 0, 1)); // world to sun space

			js::AABBox sun_space_bounds = js::AABBox::emptyAABBox();
			for(int z=0; z<8; ++z)
				sun_space_bounds.enlargeToHoldPoint(view_matrix * frustum_verts_ws[z]);

			// We want objects some distance outside of the view frustum to be able to cast shadows as well
			const float max_shadowing_dist = 250.f;
			const float use_max_k = sun_space_bounds.max_[2] + max_shadowing_dist; // extend k bound to encompass max_shadowing_dist from max_k.

			const float near_signed_dist = -use_max_k; // k is towards sun so negate
			const float far_signed_dist  = -sun_space_bounds.min_[2]; // k is towards sun so negate

			const Matrix4f proj_matrix = orthoMatrix(
				sun_space_bounds.min_[0], sun_space_bounds.max_[0], // left, right
				sun_space_bounds.min_[1], sun_space_bounds.max_[1], // bottom, top
				near_signed_dist, far_signed_dist // near, far
			);

			Planef clip_planes[18]; // Usually there should be <= 12 clip planes, 18 is the max possible based on the code flow in computeShadowFrustumClipPlanes.
			const int num_clip_planes_used = computeShadowFrustumClipPlanes(frustum_verts_ws, sun_dir, max_shadowing_dist, clip_planes);

			js::AABBox shadow_vol_aabb = js::AABBox::emptyAABBox(); // AABB of shadow rendering volume.
			for(int z=0; z<8; ++z)
			{
				shadow_vol_aabb.enlargeToHoldPoint(frustum_verts_ws[z]);
				shadow_vol_aabb.enlargeToHoldPoint(frustum_verts_ws[z] + sun_dir * max_shadowing_dist);
			}
			
			// We need to use a texcoord bias matrix to go from [-1, 1] to [0, 1] coord range.
			const Matrix4f texcoord_bias(
				Vec4f(0.5f, 0, 0, 0), // col 0
				Vec4f(0, 0.5f, 0, 0), // col 1
				Vec4f(0, 0, 0.5f, 0), // col 2
				Vec4f(0.5f, 0.5f, 0.5f, 1) // col 3
			);

			const float y_scale  =       1.f / shadow_mapping->numDynamicDepthTextures();
			const float y_offset = (float)ti / shadow_mapping->numDynamicDepthTextures();
			const Matrix4f cascade_selection_matrix(
				Vec4f(1, 0, 0, 0), // col 0
				Vec4f(0, y_scale, 0, 0), // col 1
				Vec4f(0, 0, 1, 0), // col 2
				Vec4f(0, y_offset, 0, 1) // col 3
			);
			// Save shadow_tex_matrix that the shaders like phong will use.
			shadow_mapping->dynamic_tex_matrix[ti] = cascade_selection_matrix * texcoord_bias * proj_matrix * view_matrix;


			// Update shared uniforms
			{
				SharedVertUniforms uniforms;
				std::memset((void*)&uniforms, 0, sizeof(SharedVertUniforms)); // Zero because we are not going to set all uniforms.
				uniforms.proj_matrix = proj_matrix;
				uniforms.view_matrix = view_matrix;
				uniforms.campos_ws = current_scene->cam_to_world.getColumn(3);
				uniforms.vert_sundir_ws = this->sun_dir;
				uniforms.vert_uniforms_time = current_time;
				uniforms.wind_strength = current_scene->wind_strength;
				this->shared_vert_uniform_buf_ob->updateData(/*dest offset=*/0, &uniforms, sizeof(SharedVertUniforms));
			}


			// Draw fully opaque batches - batches with a material that is not transparent and doesn't use alpha testing.

			temp_batch_draw_info.reserve(current_scene->objects.size());
			temp_batch_draw_info.resize(0);

			const Vec4f size_threshold_v = Vec4f((sun_space_bounds.max_[0] - sun_space_bounds.min_[0]) * 0.002f);

			//uint64 num_drawn = 0;
			//uint64 num_frustum_culled = 0;
			//uint64 num_too_small = 0;
			const GLObjectRef* const current_scene_obs = current_scene->objects.vector.data();
			const size_t current_scene_obs_size        = current_scene->objects.vector.size();
			for(size_t q=0; q<current_scene_obs_size; ++q)
			{
				// Prefetch cachelines containing the variables we need for objects N places ahead in the array.
				if(q + 16 < current_scene_obs_size)
				{	
					_mm_prefetch((const char*)(&current_scene_obs[q + 16]->aabb_ws), _MM_HINT_T0);
					_mm_prefetch((const char*)(&current_scene_obs[q + 16]->aabb_ws) + 64, _MM_HINT_T0);
				}

				const GLObject* const ob = current_scene_obs[q].ptr();
				const js::AABBox ob_aabb_ws = ob->aabb_ws;
				if(AABBIntersectsFrustum(clip_planes, num_clip_planes_used, shadow_vol_aabb, ob_aabb_ws))
				{
					if(allTrue(parallelLessThan(ob_aabb_ws.max_ - ob_aabb_ws.min_, size_threshold_v)))
					{
						//num_too_small++;
						continue;
					}
					
					const size_t ob_depth_draw_batches_size                  = ob->depth_draw_batches.size();
					const GLObjectBatchDrawInfo* const ob_depth_draw_batches = ob->depth_draw_batches.data();
					for(size_t z=0; z<ob_depth_draw_batches_size; ++z)
					{
						const uint32 prog_index_and_face_culling = ob_depth_draw_batches[z].getProgramIndexAndFaceCulling();

						BatchDrawInfo info(
							prog_index_and_face_culling,
							ob->vao_and_vbo_key,
							ob, // object ptr
							(uint32)z // batch index
						);
						temp_batch_draw_info.push_back(info);
					}
					//num_drawn++;
				}
				//else
				//	num_frustum_culled++;
			}

			sortBatchDrawInfos();

			// Draw sorted batches
			uint32 current_prog_index_and_face_culling = SHIFTED_CULL_BACKFACE_BITS | 1000000;
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);

			const BatchDrawInfo* const batch_draw_info_data = temp_batch_draw_info.data();
			const size_t batch_draw_info_size = temp_batch_draw_info.size();
			for(size_t z=0; z<batch_draw_info_size; ++z)
			{
				const BatchDrawInfo& info = batch_draw_info_data[z];
				const GLObjectBatchDrawInfo& batch = info.ob->depth_draw_batches[info.batch_i];
				const uint32 prog_index_and_face_culling = batch.getProgramIndexAndFaceCulling();
				if(prog_index_and_face_culling != current_prog_index_and_face_culling)
				{
					ZoneScopedN("changing prog or face culling"); // Tracy profiler

					if(use_multi_draw_indirect)
						submitBufferedDrawCommands(); // Flush existing draw commands

					const uint32 face_culling      = prog_index_and_face_culling         & ISOLATE_FACE_CULLING_MASK;
					const uint32 prev_face_culling = current_prog_index_and_face_culling & ISOLATE_FACE_CULLING_MASK;
					if(face_culling != prev_face_culling)
					{
						setFaceCulling(face_culling);
						num_face_culling_changes++;
					}

					const uint32 prog_index = prog_index_and_face_culling & ISOLATE_PROG_INDEX_MASK;
					if(prog_index != (current_prog_index_and_face_culling & ISOLATE_PROG_INDEX_MASK)) // If prog index changed:
					{
						const OpenGLProgram* prog = this->prog_vector[prog_index].ptr();
						// conPrint("---- Changed to program " + prog->prog_name + " (index " + toString(prog_index) + ") ----");
						prog->useProgram();
						current_bound_prog = prog;
						current_bound_prog_index = prog_index;
						current_uniforms_ob = NULL; // Program has changed, so we need to set object uniforms for the current program.
						setSharedUniformsForProg(*prog, view_matrix, proj_matrix);
						num_prog_changes++;
					}

					current_prog_index_and_face_culling = prog_index_and_face_culling;
				}

				bindMeshData(*info.ob);
				num_batches_bound++;

				drawBatchWithDenormalisedData(*info.ob, batch, info.batch_i);
			}

			flushDrawCommandsAndUnbindPrograms();

			// conPrint("Level " + toString(ti) + ": " + toString(num_drawn) + " / " + toString(current_scene_obs_size) + " obs drawn, " + toString(num_too_small) + " too small, " + toString(num_frustum_culled) + 
			//		" frustum culled (CPU time: " + timer.elapsedStringMSWIthNSigFigs(4) + ")");
		}

		shadow_mapping->unbindFrameBuffer();

		if(query_profiling_enabled && dynamic_depth_draw_gpu_timer->isRunning())
			dynamic_depth_draw_gpu_timer->endTimerQuery();
		//-------------------- End draw dynamic depth textures ----------------

		//-------------------- Draw static depth textures ----------------
		
		/*
		We will update the different static cascades only every N frames, and in a staggered fashion.

		target depth tex 0:
		buffer clear
		draw ob set 0
		draw ob set 1
		draw ob set 2
		draw ob set 3
		depth tex 1:
		ob set 0
		ob set 1
		ob set 2
		ob set 3
		depth tex 2:
		ob set 0
		ob set 1
		ob set 2
		ob set 3
		depth tex 0:
		ob set 0 etc..
		*/
		{
			DebugGroup debug_group2("static depth draw");

			if(query_profiling_enabled && current_scene->collect_stats && time_individual_passes && static_depth_draw_gpu_timer->isIdle())
				static_depth_draw_gpu_timer->beginTimerQuery();

			// Bind the non-current ('other') static depth map.  We will render to that.
			const int other_index = (shadow_mapping->cur_static_depth_tex + 1) % 2;

			shadow_mapping->bindStaticDepthTexFrameBufferAsTarget(other_index);

			glDrawBuffers(/*num=*/0, NULL); // Don't draw to any colour buffers. (there are none attached to the frame buffer anyway)

			const uint32 ti = (shadow_mapping_frame_num % 12) / 4; // Texture index, in [0, numStaticDepthTextures)
			const uint32 ob_set = shadow_mapping_frame_num % 4;    // Object set, in [0, 4)

			{
				//conPrint("At frame " + toString(shadow_mapping_frame_num) + ", drawing static cascade " + toString(ti));

				const int static_per_map_h = shadow_mapping->static_h / shadow_mapping->numStaticDepthTextures();
				glViewport(/*x=*/0, /*y=*/ti*static_per_map_h, /*width=*/shadow_mapping->static_w, /*height=*/static_per_map_h);

				if(ob_set == 0)
				{
					glDisable(GL_CULL_FACE);
					partiallyClearBuffer(Vec2f(0, 0), Vec2f(1, 1)); // Clobbers depth func
					glDepthFunc(GL_LESS); // restore depth func
				}

				// Half-width in x and y directions in metres of static depth texture at this cascade level.
				float w;
				if(current_scene->camera_type == OpenGLScene::CameraType_Perspective)
				{
					if(ti == 0)
						w = 64;
					else if(ti == 1)
						w = 256;
					else
						w = 1024;
				}
				else if(current_scene->camera_type == OpenGLScene::CameraType_Orthographic || current_scene->camera_type == OpenGLScene::CameraType_DiagonalOrthographic)
				{
					w = current_scene->use_sensor_width;
				}
				else
				{
					assert(0);
					w = 64;
				}


				const float h = 256; // Half-height (in z direction)

				// Get a bounding AABB centered on camera.
				// Quantise camera coords to avoid shimmering artifacts when moving camera.
				const Vec4f campos_ws = this->getCameraPositionWS();
				const float quant_w = 10;
				const Vec4f cur_vol_centre(
					floor(campos_ws[0] / quant_w) * quant_w, 
					floor(campos_ws[1] / quant_w) * quant_w, 
					floor(campos_ws[2] / quant_w) * quant_w, 1.f);

				// Record the volume centre, make sure we use the same one when rendering other objects
				Vec4f vol_centre;
				if(ob_set == 0)
				{
					vol_centre = cur_vol_centre;
					shadow_mapping->vol_centres[other_index * ShadowMapping::NUM_STATIC_DEPTH_TEXTURES + ti] = cur_vol_centre;
				}
				else
					vol_centre = shadow_mapping->vol_centres[other_index * ShadowMapping::NUM_STATIC_DEPTH_TEXTURES + ti]; // Use saved volume centre


				Vec4f frustum_verts_ws[8];
				frustum_verts_ws[0] = vol_centre + Vec4f(-w,-w,-h,0);
				frustum_verts_ws[1] = vol_centre + Vec4f( w,-w,-h,0);
				frustum_verts_ws[2] = vol_centre + Vec4f( w,-w, h,0);
				frustum_verts_ws[3] = vol_centre + Vec4f(-w,-w, h,0);
				frustum_verts_ws[4] = vol_centre + Vec4f(-w, w,-h,0);
				frustum_verts_ws[5] = vol_centre + Vec4f( w, w,-h,0);
				frustum_verts_ws[6] = vol_centre + Vec4f( w, w, h,0);
				frustum_verts_ws[7] = vol_centre + Vec4f(-w, w, h,0);

				const Vec4f up(0, 0, 1, 0);
				const Vec4f k = sun_dir;
				const Vec4f i = normalise(crossProduct(up, sun_dir)); // right 
				const Vec4f j = crossProduct(k, i); // up

				assert(k.isUnitLength());

				const Matrix4f view_matrix = Matrix4f::fromRows(i, j, k, Vec4f(0, 0, 0, 1));

				js::AABBox sun_space_bounds = js::AABBox::emptyAABBox();
				for(int z=0; z<8; ++z)
					sun_space_bounds.enlargeToHoldPoint(view_matrix * frustum_verts_ws[z]);


				const float max_shadowing_dist = 250.0f;
				const float use_max_k = sun_space_bounds.max_[2] + max_shadowing_dist; // extend k bound to encompass max_shadowing_dist from max_k.

				const float near_signed_dist = -use_max_k; // k is towards sun so negate
				const float far_signed_dist  = -sun_space_bounds.min_[2]; // k is towards sun so negate

				const Matrix4f proj_matrix = orthoMatrix(
					sun_space_bounds.min_[0], sun_space_bounds.max_[0], // left, right
					sun_space_bounds.min_[1], sun_space_bounds.max_[1], // bottom, top
					near_signed_dist, far_signed_dist // near, far
				);


				Planef clip_planes[18]; // Usually there should be <= 12 clip planes, 18 is the max possible based on the code flow in computeShadowFrustumClipPlanes.
				const int num_clip_planes_used = computeShadowFrustumClipPlanes(frustum_verts_ws, sun_dir, max_shadowing_dist, clip_planes);

				js::AABBox shadow_vol_aabb = js::AABBox::emptyAABBox(); // AABB of shadow rendering volume.
				for(int z=0; z<8; ++z)
				{
					shadow_vol_aabb.enlargeToHoldPoint(frustum_verts_ws[z]);
					shadow_vol_aabb.enlargeToHoldPoint(frustum_verts_ws[z] + sun_dir * max_shadowing_dist);
				}

				// We need to use a texcoord bias matrix to go from [-1, 1] to [0, 1] coord range.
				const Matrix4f texcoord_bias(
					Vec4f(0.5f, 0, 0, 0), // col 0
					Vec4f(0, 0.5f, 0, 0), // col 1
					Vec4f(0, 0, 0.5f, 0), // col 2
					Vec4f(0.5f, 0.5f, 0.5f, 1) // col 3
				);

				const float y_scale =        1.f / shadow_mapping->numStaticDepthTextures();
				const float y_offset = (float)ti / shadow_mapping->numStaticDepthTextures();
				const Matrix4f cascade_selection_matrix(
					Vec4f(1, 0, 0, 0), // col 0
					Vec4f(0, y_scale, 0, 0), // col 1
					Vec4f(0, 0, 1, 0), // col 2
					Vec4f(0, y_offset, 0, 1) // col 3
				);

				// Save shadow_tex_matrix that the shaders like phong will use.
				if(ob_set == 0)
					shadow_mapping->static_tex_matrix[ShadowMapping::NUM_STATIC_DEPTH_TEXTURES * other_index + ti] = cascade_selection_matrix * texcoord_bias * proj_matrix * view_matrix;

				// Draw fully opaque batches - batches with a material that is not transparent and doesn't use alpha testing.
				Timer timer3;
				
				// Update shared uniforms
				{
					SharedVertUniforms uniforms;
					std::memset((void*)&uniforms, 0, sizeof(SharedVertUniforms)); // Zero because we are not going to set all uniforms.
					uniforms.proj_matrix = proj_matrix;
					uniforms.view_matrix = view_matrix;
					uniforms.campos_ws = current_scene->cam_to_world.getColumn(3);
					uniforms.vert_sundir_ws = this->sun_dir;
					uniforms.vert_uniforms_time = current_time;
					uniforms.wind_strength = current_scene->wind_strength;
					this->shared_vert_uniform_buf_ob->updateData(/*dest offset=*/0, &uniforms, sizeof(SharedVertUniforms));
				}


				temp_batch_draw_info.reserve(current_scene->objects.size());
				temp_batch_draw_info.resize(0);

				const Vec4f size_threshold_v = Vec4f((sun_space_bounds.max_[0] - sun_space_bounds.min_[0]) * 0.001f);
				
				//uint64 num_drawn = 0;
				//uint64 num_frustum_culled = 0;
				//uint64 num_too_small = 0;
				const GLObjectRef* const current_scene_obs = current_scene->objects.vector.data();
				const size_t current_scene_obs_size        = current_scene->objects.vector.size();
				for(size_t q=0; q<current_scene_obs_size; ++q)
				{
					// Prefetch cachelines containing the variables we need for objects N places ahead in the array.
					if(q + 16 < current_scene_obs_size)
					{	
						_mm_prefetch((const char*)(&current_scene_obs[q + 16]->aabb_ws), _MM_HINT_T0);
						_mm_prefetch((const char*)(&current_scene_obs[q + 16]->aabb_ws) + 64, _MM_HINT_T0);
					}

					const GLObject* const ob = current_scene_obs[q].ptr();
					if((ob->random_num & 0x3) == ob_set) // Only draw objects in ob_set (check by comparing lowest 2 bits with ob_set)
					{
						const js::AABBox ob_aabb_ws = ob->aabb_ws;
						if(AABBIntersectsFrustum(clip_planes, num_clip_planes_used, shadow_vol_aabb, ob_aabb_ws))
						{
							if(allTrue(parallelLessThan(ob_aabb_ws.max_ - ob_aabb_ws.min_, size_threshold_v)))
							{
								//num_too_small++;
								continue;
							}

							const size_t ob_depth_draw_batches_size                  = ob->depth_draw_batches.size();
							const GLObjectBatchDrawInfo* const ob_depth_draw_batches = ob->depth_draw_batches.data();
							for(size_t z = 0; z < ob_depth_draw_batches_size; ++z)
							{
								const uint32 prog_index_and_face_culling = ob_depth_draw_batches[z].getProgramIndexAndFaceCulling();

								BatchDrawInfo info(
									prog_index_and_face_culling,
									ob->vao_and_vbo_key,
									ob, // object ptr
									(uint32)z // batch index
								);
								temp_batch_draw_info.push_back(info);
							}
							//num_drawn++;
						}
						//else
						//	num_frustum_culled++;
					}
				}

				sortBatchDrawInfos();

				// Draw sorted batches
				uint32 current_prog_index_and_face_culling = SHIFTED_CULL_BACKFACE_BITS | 1000000;
				glEnable(GL_CULL_FACE);
				glCullFace(GL_BACK);

				const BatchDrawInfo* const batch_draw_info_data = temp_batch_draw_info.data();
				const size_t batch_draw_info_size = temp_batch_draw_info.size();
				for(size_t z=0; z<batch_draw_info_size; ++z)
				{
					const BatchDrawInfo& info = batch_draw_info_data[z];
					const GLObjectBatchDrawInfo& batch = info.ob->depth_draw_batches[info.batch_i];
					const uint32 prog_index_and_face_culling = batch.getProgramIndexAndFaceCulling();
					if(prog_index_and_face_culling != current_prog_index_and_face_culling)
					{
						ZoneScopedN("changing prog"); // Tracy profiler

						if(use_multi_draw_indirect)
							submitBufferedDrawCommands(); // Flush existing draw commands

						const uint32 face_culling      = prog_index_and_face_culling         & ISOLATE_FACE_CULLING_MASK;
						const uint32 prev_face_culling = current_prog_index_and_face_culling & ISOLATE_FACE_CULLING_MASK;
						if(face_culling != prev_face_culling)
						{
							setFaceCulling(face_culling);
							num_face_culling_changes++;
						}

						const uint32 prog_index = prog_index_and_face_culling & ISOLATE_PROG_INDEX_MASK;
						if(prog_index != (current_prog_index_and_face_culling & ISOLATE_PROG_INDEX_MASK)) // If prog index changed:
						{
							const OpenGLProgram* prog = this->prog_vector[prog_index].ptr();
							// conPrint("---- Changed to program " + prog->prog_name + " (index " + toString(prog_index) + ") ----");
							prog->useProgram();
							current_bound_prog = prog;
							current_bound_prog_index = prog_index;
							current_uniforms_ob = NULL; // Program has changed, so we need to set object uniforms for the current program.
							setSharedUniformsForProg(*prog, view_matrix, proj_matrix);
							num_prog_changes++;
						}

						current_prog_index_and_face_culling = prog_index_and_face_culling;
					}

					bindMeshData(*info.ob);
					num_batches_bound++;
					
					drawBatchWithDenormalisedData(*info.ob, batch, info.batch_i);
				}

				flushDrawCommandsAndUnbindPrograms();

				//conPrint("Static shadow map level " + toString(ti) + ": ob set: " + toString(ob_set) + " " + toString(num_drawn) + " / " + toString(current_scene->objects.size()) + " drawn. " + 
				//	"num_frustum_culled: " + toString(num_frustum_culled) + ", num_too_small: " + toString(num_too_small ) + " (CPU time: " + timer3.elapsedStringNSigFigs(3) + ")");
			}

			shadow_mapping->unbindFrameBuffer();

			if(shadow_mapping_frame_num % 12 == 11) // If we just finished drawing to our 'other' depth map, swap cur and other
				shadow_mapping->setCurStaticDepthTex((shadow_mapping->cur_static_depth_tex + 1) % 2); // Swap cur and other

			if(query_profiling_enabled && static_depth_draw_gpu_timer->isRunning())
				static_depth_draw_gpu_timer->endTimerQuery();
		}
		//-------------------- End draw static depth textures ----------------

		this->depth_draw_last_num_prog_changes = num_prog_changes;
		this->depth_draw_last_num_face_culling_changes = num_face_culling_changes;
		this->depth_draw_last_num_batches_bound = num_batches_bound;
		this->depth_draw_last_num_vao_binds = num_vao_binds;
		this->depth_draw_last_num_vbo_binds = num_vbo_binds;
		this->depth_draw_last_num_indices_drawn = num_indices_submitted;
		this->num_indices_submitted = 0;


		glDisable(GL_CULL_FACE);

		// Restore clip coord range and depth comparison func
#if !defined(OSX)
		if(use_reverse_z)
		{
			glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
			glDepthFunc(GL_GREATER);
		}
#endif

		shadow_mapping_frame_num++;

	} // End if(shadow_mapping.nonNull())
}


// Draw background env map if there is one. (or if we are using a non-standard env shader)
void OpenGLEngine::drawBackgroundEnvMap(const Matrix4f& view_matrix, const Matrix4f& proj_matrix)
{
	DebugGroup debug_group("drawBackgroundEnvMap()");
	TracyGpuZone("drawBackgroundEnvMap");

	if(this->current_scene->env_ob.nonNull() && this->current_scene->env_ob->materials[0].shader_prog.nonNull())
	{
		if((this->current_scene->env_ob->materials[0].shader_prog.ptr() != this->env_prog.ptr()) || this->current_scene->env_ob->materials[0].albedo_texture.nonNull())
		{
			if(current_scene->render_to_main_render_framebuffer)
			{
				main_render_framebuffer->bindForDrawing();
				assert(main_render_framebuffer->getAttachedRenderBufferName(GL_COLOR_ATTACHMENT0) == main_colour_renderbuffer->buffer_name); // Check main colour renderbuffer is attached at GL_COLOR_ATTACHMENT0.
				setSingleDrawBuffer(GL_COLOR_ATTACHMENT0); // Just draw to colour buffer, not normal buffer
			}
			else
			{
				if(this->target_frame_buffer)
				{
					this->target_frame_buffer->bindForDrawing();
					setSingleDrawBuffer(GL_COLOR_ATTACHMENT0); // Just draw to colour buffer, not normal buffer.
				}
				else
					glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // Bind to default frame buffer and use the draw buffer already set already for it.
			}
			
		
			ZoneScopedN("Draw env"); // Tracy profiler
			assertCurrentProgramIsZero();

			Matrix4f world_to_camera_space_no_translation = view_matrix;
			world_to_camera_space_no_translation.e[12] = 0;
			world_to_camera_space_no_translation.e[13] = 0;
			world_to_camera_space_no_translation.e[14] = 0;

			glDepthMask(GL_FALSE); // Disable writing to depth buffer.

			Matrix4f use_proj_mat;
			if(current_scene->camera_type == OpenGLScene::CameraType_Orthographic)
			{
				// Use a perspective transformation for rendering the env sphere, with a narrow field of view, to provide just a hint of texture detail.
				const float w = 0.01f;
				use_proj_mat = frustumMatrix(-w, w, -w, w, 0.5, 100);
			}
			else
				use_proj_mat = proj_matrix;

			const bool program_changed = checkUseProgram(current_scene->env_ob->materials[0].shader_prog.ptr());
			if(program_changed)
				setSharedUniformsForProg(*current_scene->env_ob->materials[0].shader_prog, world_to_camera_space_no_translation, use_proj_mat);
			bindMeshData(*current_scene->env_ob->mesh_data);
			drawBatch(*current_scene->env_ob, current_scene->env_ob->materials[0], *current_scene->env_ob->materials[0].shader_prog, *current_scene->env_ob->mesh_data, current_scene->env_ob->mesh_data->batches[0], /*batch_index=*/0);
			
			flushDrawCommandsAndUnbindPrograms();

			glDepthMask(GL_TRUE); // Re-enable writing to depth buffer.
		}
	}
}


// Draw triangle batches with alpha-blended materials, such as participating media materials (i.e. particles), and fonts.
// These batches will be sorted by distance from camera and drawn in far to near order.
void OpenGLEngine::drawAlphaBlendedObjects(const Matrix4f& view_matrix, const Matrix4f& proj_matrix)
{
	DebugGroup debug_group("drawAlphaBlendedObjects()");
	TracyGpuZone("drawAlphaBlendedObjects");

	if(!current_scene->alpha_blended_objects.empty())
	{
		ZoneScopedN("Draw participating media obs"); // Tracy profiler
		assertCurrentProgramIsZero();

		if(current_scene->render_to_main_render_framebuffer)
		{
			main_render_framebuffer->bindForDrawing();
			assert(main_render_framebuffer->getAttachedRenderBufferName(GL_COLOR_ATTACHMENT0) == main_colour_renderbuffer->buffer_name); // Check main colour renderbuffer is attached at GL_COLOR_ATTACHMENT0.
			setSingleDrawBuffer(GL_COLOR_ATTACHMENT0); // Just draw to colour buffer (not normal buffer)
		}
		else
		{
			if(this->target_frame_buffer)
			{
				this->target_frame_buffer->bindForDrawing();
				setSingleDrawBuffer(GL_COLOR_ATTACHMENT0); // Just draw to colour buffer, not normal buffer.
			}
			else
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // Bind to default frame buffer and use the draw buffer already set already for it.
		}
		

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE); // Disable writing to depth buffer - we don't want to occlude other transparent objects.

		const Vec4f campos_ws = this->getCameraPositionWS();

		batch_draw_info_dist.resize(0);

		const GLObjectRef* const current_scene_trans_obs = current_scene->alpha_blended_objects.vector.data();
		const size_t current_scene_trans_obs_size        = current_scene->alpha_blended_objects.vector.size();
		for(size_t q=0; q<current_scene_trans_obs_size; ++q)
		{
			// Prefetch cachelines containing the variables we need for objects N places ahead in the array.
			if(q + 16 < current_scene_trans_obs_size)
			{	
				_mm_prefetch((const char*)(&current_scene_trans_obs[q + 16]->aabb_ws), _MM_HINT_T0);
				_mm_prefetch((const char*)(&current_scene_trans_obs[q + 16]->aabb_ws) + 64, _MM_HINT_T0);
			}

			const GLObject* const ob = current_scene_trans_obs[q].ptr();
			if(AABBIntersectsFrustum(current_scene->frustum_clip_planes, current_scene->num_frustum_clip_planes, current_scene->frustum_aabb, ob->aabb_ws))
			{
				const uint32 z = 0; // Just assume we always have one triangle batch (e.g. one material)
				assert(ob->batch_draw_info.size() == 1);

				const GLObjectBatchDrawInfo* const ob_batch_draw_info = ob->batch_draw_info.data();
				{
					const uint32 prog_index_and_flags = ob_batch_draw_info[z].program_index_and_flags;
					const uint32 prog_index_and_face_culling_flag = prog_index_and_flags & ISOLATE_PROG_INDEX_AND_FACE_CULLING_MASK;

#ifndef NDEBUG
					const bool face_culling = !ob->materials[ob->getUsedBatches()[z].material_index].simple_double_sided && !ob->materials[ob->getUsedBatches()[z].material_index].fancy_double_sided;
					const uint32 face_culling_bits = face_culling ? ((ob->ob_to_world_matrix_determinant >= 0.f) ? SHIFTED_CULL_BACKFACE_BITS : SHIFTED_CULL_FRONTFACE_BITS) : 0;
					assert(prog_index_and_face_culling_flag == (ob->materials[ob->getUsedBatches()[z].material_index].shader_prog->program_index | face_culling_bits));
					assert(BitUtils::isBitSet(prog_index_and_flags, MATERIAL_ALPHA_BLEND_BITFLAG) == ob->materials[ob->getUsedBatches()[z].material_index].participating_media || ob->materials[ob->getUsedBatches()[z].material_index].alpha_blend);
#endif
					if(BitUtils::areBitsSet(prog_index_and_flags, MATERIAL_ALPHA_BLEND_BITFLAG | PROGRAM_FINISHED_BUILDING_BITFLAG)) // If alpha-blend bit is set, and program has finished building:
					{
						// We want to sort into descending depth order, because we want to draw far objects first.
						// We also want to convert to a uint32.  So convert to uint32 first then negate.
						//const float dist_f = campos_ws.getDist2(ob->aabb_ws.centroid());
						const float dist_f = ob->aabb_ws.distanceToPoint(campos_ws); // This works better for long, thin AABBs like text AABBs
						const uint32 dist_i = bitCast<uint32>(dist_f);
						const uint32 distval = std::numeric_limits<uint32>::max() - dist_i;
						BatchDrawInfoWithDist info(
							prog_index_and_face_culling_flag,
							ob->vao_and_vbo_key,
							distval,
							ob // object ptr
							//(uint32)z // batch_i
						);
						batch_draw_info_dist.push_back(info);
					}
				}
			}
		}

		sortBatchDrawInfoWithDists();

		// Draw sorted batches
		const BatchDrawInfoWithDist* const batch_draw_info_data = batch_draw_info_dist.data();
		const size_t batch_draw_info_size = batch_draw_info_dist.size();
		for(size_t i=0; i<batch_draw_info_size; ++i)
		{
			const BatchDrawInfoWithDist& info = batch_draw_info_data[i];
			const uint32 batch_i = 0;
			const uint32 prog_index = info.ob->batch_draw_info[batch_i].getProgramIndex();
			const bool program_changed = checkUseProgram(prog_index);
			if(program_changed)
			{
				const OpenGLProgram* prog = prog_vector[prog_index].ptr();
			// conPrint("---- Changed to program " + prog->prog_name + " (index " + toString(prog_index) + ") ----");
				setSharedUniformsForProg(*prog, view_matrix, proj_matrix);
			}

			bindMeshData(*info.ob);
			drawBatchWithDenormalisedData(*info.ob, info.ob->batch_draw_info[batch_i], batch_i);
		}

		flushDrawCommandsAndUnbindPrograms();

		glDepthMask(GL_TRUE); // Re-enable writing to depth buffer.
		glDisable(GL_BLEND);

#if !defined(EMSCRIPTEN)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif
	}
}


void OpenGLEngine::drawDecals(const Matrix4f& view_matrix, const Matrix4f& proj_matrix)
{
	DebugGroup debug_group("drawDecals()");
	TracyGpuZone("drawDecals");

	this->last_num_decal_batches_drawn = 0;

	// We will need to copy the depth buffer and normal buffer again, to capture the results of drawing the water.
	if(!current_scene->decal_objects.empty() && current_scene->render_to_main_render_framebuffer)
	{
		assertCurrentProgramIsZero();

		ZoneScopedN("Draw decal obs"); // Tracy profiler

		//Timer timer;
		temp_batch_draw_info.resize(0);

		{
			const Planef* frustum_clip_planes = current_scene->frustum_clip_planes;
			const int num_frustum_clip_planes = current_scene->num_frustum_clip_planes;
			const js::AABBox frustum_aabb = current_scene->frustum_aabb;
			const GLObjectRef* const current_decal_obs = current_scene->decal_objects.vector.data();
			const size_t current_decal_obs_size        = current_scene->decal_objects.vector.size();
			for(size_t i=0; i<current_decal_obs_size; ++i)
			{
				// Prefetch cachelines containing the variables we need for objects N places ahead in the array.
				if(i + 16 < current_decal_obs_size)
				{	
					_mm_prefetch((const char*)(&current_decal_obs[i + 16]->aabb_ws), _MM_HINT_T0);
					_mm_prefetch((const char*)(&current_decal_obs[i + 16]->aabb_ws) + 64, _MM_HINT_T0);
				}

				const GLObject* const ob = current_decal_obs[i].ptr();
				if(AABBIntersectsFrustum(frustum_clip_planes, num_frustum_clip_planes, frustum_aabb, ob->aabb_ws))
				{
					const size_t ob_batch_draw_info_size                  = ob->batch_draw_info.size();
					const GLObjectBatchDrawInfo* const ob_batch_draw_info = ob->batch_draw_info.data();
					for(uint32 z = 0; z < ob_batch_draw_info_size; ++z)
					{
						const uint32 prog_index_and_flags = ob_batch_draw_info[z].program_index_and_flags;
						const uint32 prog_index_and_face_culling_flag = prog_index_and_flags & ISOLATE_PROG_INDEX_AND_FACE_CULLING_MASK;

#ifndef NDEBUG
						const bool face_culling = !ob->materials[ob->getUsedBatches()[z].material_index].simple_double_sided && !ob->materials[ob->getUsedBatches()[z].material_index].fancy_double_sided;
						const uint32 face_culling_bits = face_culling ? ((ob->ob_to_world_matrix_determinant >= 0.f) ? SHIFTED_CULL_BACKFACE_BITS : SHIFTED_CULL_FRONTFACE_BITS) : 0;
						assert(prog_index_and_face_culling_flag == (ob->materials[ob->getUsedBatches()[z].material_index].shader_prog->program_index | face_culling_bits));
						assert(BitUtils::isBitSet(prog_index_and_flags, MATERIAL_TRANSPARENT_BITFLAG) == ob->materials[ob->getUsedBatches()[z].material_index].transparent);

						// Check the denormalised vao_and_vbo_key is correct
						const uint32 vao_id = ob->mesh_data->vao_data_index;
						const uint32 vbo_id = (uint32)ob->mesh_data->vbo_handle.vbo_id;
						const uint32 indices_vbo_id = (uint32)ob->mesh_data->indices_vbo_handle.vbo_id;
						const uint32 index_type_bits = ob->mesh_data->index_type_bits;
						assert(ob->vao_and_vbo_key == makeVAOAndVBOKey(vao_id, vbo_id, indices_vbo_id, index_type_bits));
#endif
						// Draw primitives for the given material
						//if((prog_index_and_flags & (MATERIAL_TRANSPARENT_BITFLAG | MATERIAL_WATER_BITFLAG)) == 0) // NOTE: check for decal bitflag? or just assume all decal object materials are decals.
						if(BitUtils::areBitsSet(prog_index_and_flags, MATERIAL_DECAL_BITFLAG | PROGRAM_FINISHED_BUILDING_BITFLAG)) // If decal bit is set and program is finished building:
						{
							BatchDrawInfo info(
								prog_index_and_face_culling_flag,
								ob->vao_and_vbo_key,
								ob, // object ptr
								(uint32)z // batch_i
							);
							temp_batch_draw_info.push_back(info);
						}
					}
				}
			} // End for each object in scene
		}
		//conPrint("Draw opaque make batch loop took " + timer.elapsedStringNSigFigs(4));

		if(temp_batch_draw_info.nonEmpty()) // Only do buffer copying if we have some decals to draw.
		{
			if(query_profiling_enabled && current_scene->collect_stats && time_individual_passes && decal_copy_buffers_timer->isIdle())
				decal_copy_buffers_timer->beginTimerQuery();

			main_render_framebuffer->bindForReading();

			assert(main_render_framebuffer->getAttachedRenderBufferName(GL_DEPTH_ATTACHMENT) == this->main_depth_renderbuffer->buffer_name);
			assert(main_render_framebuffer->getAttachedRenderBufferName(GL_COLOR_ATTACHMENT1) == this->main_normal_renderbuffer->buffer_name);
		
			main_render_copy_framebuffer->bindForDrawing();

			assert(main_render_copy_framebuffer->getAttachedTextureName(GL_DEPTH_ATTACHMENT) == this->main_depth_copy_texture->texture_handle);
			assert(main_render_copy_framebuffer->getAttachedTextureName(GL_COLOR_ATTACHMENT1) == this->main_normal_copy_texture->texture_handle);

			//---------------------- Copy depth renderbuffer to depth texture ----------------------
			glReadBuffer(GL_COLOR_ATTACHMENT0);
			setSingleDrawBuffer(GL_COLOR_ATTACHMENT0);

			glBlitFramebuffer(
				/*srcX0=*/0, /*srcY0=*/0, /*srcX1=*/(int)main_render_framebuffer->xRes(), /*srcY1=*/(int)main_render_framebuffer->yRes(), 
				/*dstX0=*/0, /*dstY0=*/0, /*dstX1=*/(int)main_render_framebuffer->xRes(), /*dstY1=*/(int)main_render_framebuffer->yRes(), 
				GL_DEPTH_BUFFER_BIT,
				GL_NEAREST
			);

			//---------------------- Copy normal buffer.  Do this just to resolve the MSAA samples. ----------------------
			glReadBuffer(GL_COLOR_ATTACHMENT1);
			setTwoDrawBuffers(GL_NONE, GL_COLOR_ATTACHMENT1); // In OpenGL ES, GL_COLOR_ATTACHMENT1 must be specified as buffer 1 (can't be buffer 0), so use glDrawBuffers with GL_NONE as buffer 0.
	
			glBlitFramebuffer(
				/*srcX0=*/0, /*srcY0=*/0, /*srcX1=*/(int)main_render_framebuffer->xRes(), /*srcY1=*/(int)main_render_framebuffer->yRes(), 
				/*dstX0=*/0, /*dstY0=*/0, /*dstX1=*/(int)main_render_framebuffer->xRes(), /*dstY1=*/(int)main_render_framebuffer->yRes(), 
				GL_COLOR_BUFFER_BIT,
				GL_NEAREST
			);

			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0); // Unbind any framebuffer from readback operations.

			// Stop timer
			if(query_profiling_enabled && decal_copy_buffers_timer->isRunning())
				decal_copy_buffers_timer->endTimerQuery();



			// Restore main render buffer binding
			main_render_framebuffer->bindForDrawing();
			assert(main_render_framebuffer->getAttachedRenderBufferName(GL_COLOR_ATTACHMENT0) == this->main_colour_renderbuffer->buffer_name);
			setSingleDrawBuffer(GL_COLOR_ATTACHMENT0); // Only write to colour buffer for decal shader (don't write to normal buffer).




			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			glDepthMask(GL_FALSE); // Disable writing to depth buffer, or nearby decals will occlude each other.


			sortBatchDrawInfos();


			// Draw sorted batches
			uint32 current_prog_index_and_face_culling = SHIFTED_CULL_BACKFACE_BITS | 1000000;
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);

#if !defined(EMSCRIPTEN)
			if(draw_wireframes)
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif

			//Timer timer3;
			const BatchDrawInfo* const batch_draw_info_data = temp_batch_draw_info.data();
			const size_t batch_draw_info_size = temp_batch_draw_info.size();
			for(size_t i=0; i<batch_draw_info_size; ++i)
			{
				const BatchDrawInfo& info = batch_draw_info_data[i];
				const uint32 prog_index_and_face_culling = info.ob->batch_draw_info[info.batch_i].getProgramIndexAndFaceCulling();
				if(prog_index_and_face_culling != current_prog_index_and_face_culling)
				{
					ZoneScopedN("changing prog"); // Tracy profiler

					if(use_multi_draw_indirect)
						submitBufferedDrawCommands(); // Flush existing draw commands

					const uint32 face_culling      = prog_index_and_face_culling         & ISOLATE_FACE_CULLING_MASK;
					const uint32 prev_face_culling = current_prog_index_and_face_culling & ISOLATE_FACE_CULLING_MASK;
					if(face_culling != prev_face_culling)
					{
						setFaceCulling(face_culling);
					}

					const uint32 prog_index = prog_index_and_face_culling & ISOLATE_PROG_INDEX_MASK;
					if(prog_index != (current_prog_index_and_face_culling & ISOLATE_PROG_INDEX_MASK)) // If prog index changed:
					{
						const OpenGLProgram* prog = this->prog_vector[prog_index].ptr();
						// conPrint("---- Changed to program " + prog->prog_name + " (index " + toString(prog_index) + ") ----");
						prog->useProgram();
						current_bound_prog = prog;
						current_bound_prog_index = prog_index;
						current_uniforms_ob = NULL; // Program has changed, so we need to set object uniforms for the current program.
						setSharedUniformsForProg(*prog, view_matrix, proj_matrix);
						num_prog_changes++;
					}

					current_prog_index_and_face_culling = prog_index_and_face_culling;
				}

				bindMeshData(*info.ob);

				drawBatchWithDenormalisedData(*info.ob, info.ob->batch_draw_info[info.batch_i], info.batch_i);
			}

			flushDrawCommandsAndUnbindPrograms();

			glDisable(GL_CULL_FACE); // Restore
			glDisable(GL_BLEND); // Restore

			glDepthMask(GL_TRUE); // Restore writing to depth buffer.

#if !defined(EMSCRIPTEN)
			if(draw_wireframes)
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // Restore normal fill mode
#endif

			// main_render_copy_framebuffer->discardContents(GL_DEPTH_ATTACHMENT, GL_DEPTH_ATTACHMENT);
			// main_render_framebuffer->bindForDrawing(); // Restore framebuffer binding
		}

		this->last_num_decal_batches_drawn = (uint32)temp_batch_draw_info.size();
		//conPrint("Draw decal batches took " + timer3.elapsedStringNSigFigs(4) + " for " + toString(num_batches_bound) + " batches");
	}
}


void OpenGLEngine::drawWaterObjects(const Matrix4f& view_matrix, const Matrix4f& proj_matrix)
{
	DebugGroup debug_group("drawWaterObjects()");
	TracyGpuZone("drawWaterObjects");

	if(current_scene->draw_water)
	{
		if(current_scene->render_to_main_render_framebuffer)
		{
			// Copy framebuffer textures from main render framebuffer to main render copy framebuffer.
			// This will copy main_colour_texture to main_colour_copy_texture, and main_depth_texture to main_depth_copy_texture.
			//
			// From https://www.khronos.org/opengl/wiki/Framebuffer#Blitting:
			// "the only colors read will come from the read color buffer in the read FBO, specified by glReadBuffer.
			// The colors written will only go to the draw color buffers in the write FBO, specified by glDrawBuffers.
			// If multiple draw buffers are specified, then multiple color buffers are updated with the same data."
			//
			// 
			main_render_framebuffer->bindForReading();

			assert(main_render_framebuffer->getAttachedRenderBufferName(GL_COLOR_ATTACHMENT0) == this->main_colour_renderbuffer->buffer_name);
			assert(main_render_framebuffer->getAttachedRenderBufferName(GL_COLOR_ATTACHMENT1) == this->main_normal_renderbuffer->buffer_name);
			assert(main_render_framebuffer->getAttachedRenderBufferName(GL_DEPTH_ATTACHMENT)  == this->main_depth_renderbuffer->buffer_name);

			main_render_copy_framebuffer->bindForDrawing();

			assert(main_render_copy_framebuffer->getAttachedTextureName(GL_COLOR_ATTACHMENT0) == this->main_colour_copy_texture->texture_handle);
			assert(main_render_copy_framebuffer->getAttachedTextureName(GL_COLOR_ATTACHMENT1) == this->main_normal_copy_texture->texture_handle);
			assert(main_render_copy_framebuffer->getAttachedTextureName(GL_DEPTH_ATTACHMENT)  == this->main_depth_copy_texture->texture_handle);

			//--------------- Copy colour buffer (attachment 0) and depth buffer. ---------------
			glReadBuffer(GL_COLOR_ATTACHMENT0);
			setSingleDrawBuffer(GL_COLOR_ATTACHMENT0);

			glBlitFramebuffer(
				/*srcX0=*/0, /*srcY0=*/0, /*srcX1=*/(int)main_render_framebuffer->xRes(), /*srcY1=*/(int)main_render_framebuffer->yRes(), 
				/*dstX0=*/0, /*dstY0=*/0, /*dstX1=*/(int)main_render_framebuffer->xRes(), /*dstY1=*/(int)main_render_framebuffer->yRes(), 
				GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
				GL_NEAREST
			);

			//--------------- Copy normal buffer.  Do this just to resolve the MSAA samples. ---------------
			glReadBuffer(GL_COLOR_ATTACHMENT1);
			setTwoDrawBuffers(GL_NONE, GL_COLOR_ATTACHMENT1); // In OpenGL ES, GL_COLOR_ATTACHMENT1 must be specified as buffer 1 (can't be buffer 0), so use glDrawBuffers with GL_NONE as buffer 0.
	
			glBlitFramebuffer(
				/*srcX0=*/0, /*srcY0=*/0, /*srcX1=*/(int)main_render_framebuffer->xRes(), /*srcY1=*/(int)main_render_framebuffer->yRes(), 
				/*dstX0=*/0, /*dstY0=*/0, /*dstX1=*/(int)main_render_framebuffer->xRes(), /*dstY1=*/(int)main_render_framebuffer->yRes(), 
				GL_COLOR_BUFFER_BIT,
				GL_NEAREST
			);

			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0); // Unbind any framebuffer from readback operations.

			// Restore main render buffer binding for drawing
			main_render_framebuffer->bindForDrawing();
			assert(main_render_framebuffer->getAttachedRenderBufferName(GL_COLOR_ATTACHMENT0) == this->main_colour_renderbuffer->buffer_name);
			assert(main_render_framebuffer->getAttachedRenderBufferName(GL_COLOR_ATTACHMENT1) == this->main_normal_renderbuffer->buffer_name);
		
			//glDrawBuffer(GL_COLOR_ATTACHMENT0); // Only write to colour buffer for water shader (don't write to normal buffer).
		
			// Bind normal texture as the second colour target
			//main_render_framebuffer->bindTextureAsTarget(*main_normal_texture, GL_COLOR_ATTACHMENT1);

			// Draw to all colour buffers: colour and normal buffer.
			setTwoDrawBuffers(GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1);
		}
		else
		{
			if(this->target_frame_buffer)
			{
				this->target_frame_buffer->bindForDrawing();
				setSingleDrawBuffer(GL_COLOR_ATTACHMENT0); // Just draw to colour buffer, not normal buffer.
			}
			else
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // Bind to default frame buffer and use the draw buffer already set already for it.
		}

		glDepthMask(GL_TRUE);

		ZoneScopedN("Draw water obs"); // Tracy profiler

		temp_batch_draw_info.reserve(current_scene->objects.size());
		temp_batch_draw_info.resize(0);

		{
			ZoneScopedN("frustum culling"); // Tracy profiler

			const Planef* frustum_clip_planes = current_scene->frustum_clip_planes;
			const int num_frustum_clip_planes = current_scene->num_frustum_clip_planes;
			const js::AABBox frustum_aabb = current_scene->frustum_aabb;
			const GLObjectRef* const water_obs = current_scene->water_objects.vector.data();
			const size_t water_obs_size        = current_scene->water_objects.vector.size();
			for(size_t i=0; i<water_obs_size; ++i)
			{
				if(i + 16 < water_obs_size)
				{	
					_mm_prefetch((const char*)(&water_obs[i + 16]->aabb_ws), _MM_HINT_T0);
					_mm_prefetch((const char*)(&water_obs[i + 16]->aabb_ws) + 64, _MM_HINT_T0);
				}

				const GLObject* const ob = water_obs[i].ptr();
				if(AABBIntersectsFrustum(frustum_clip_planes, num_frustum_clip_planes, frustum_aabb, ob->aabb_ws))
				{
					const size_t ob_batch_draw_info_size                  = ob->batch_draw_info.size();
					const GLObjectBatchDrawInfo* const ob_batch_draw_info = ob->batch_draw_info.data();
					for(uint32 z = 0; z < ob_batch_draw_info_size; ++z)
					{
						const uint32 prog_index_and_flags = ob_batch_draw_info[z].program_index_and_flags;
						const uint32 prog_index_and_face_culling_flag = prog_index_and_flags & ISOLATE_PROG_INDEX_AND_FACE_CULLING_MASK;

#ifndef NDEBUG
						const bool face_culling = !ob->materials[ob->getUsedBatches()[z].material_index].simple_double_sided && !ob->materials[ob->getUsedBatches()[z].material_index].fancy_double_sided;
						const uint32 face_culling_bits = face_culling ? ((ob->ob_to_world_matrix_determinant >= 0.f) ? SHIFTED_CULL_BACKFACE_BITS : SHIFTED_CULL_FRONTFACE_BITS) : 0;
						assert(prog_index_and_face_culling_flag == (ob->materials[ob->getUsedBatches()[z].material_index].shader_prog->program_index | face_culling_bits));
						assert(BitUtils::isBitSet(prog_index_and_flags, MATERIAL_WATER_BITFLAG) == ob->materials[ob->getUsedBatches()[z].material_index].water);

						// Check the denormalised vao_and_vbo_key is correct
						const uint32 vao_id = ob->mesh_data->vao_data_index;
						const uint32 vbo_id = (uint32)ob->mesh_data->vbo_handle.vbo_id;
						const uint32 indices_vbo_id = (uint32)ob->mesh_data->indices_vbo_handle.vbo_id;
						const uint32 index_type_bits = ob->mesh_data->index_type_bits;
						assert(ob->vao_and_vbo_key == makeVAOAndVBOKey(vao_id, vbo_id, indices_vbo_id, index_type_bits));
#endif
						// Draw primitives for the given material
						if(BitUtils::areBitsSet(prog_index_and_flags, MATERIAL_WATER_BITFLAG | PROGRAM_FINISHED_BUILDING_BITFLAG)) // If water bit is set and program is finished building:
						{
							BatchDrawInfo info(
								prog_index_and_face_culling_flag,
								ob->vao_and_vbo_key,
								ob, // object ptr
								(uint32)z // batch_i
							);
							temp_batch_draw_info.push_back(info);
						}
					}
				}
			} // End for each object in scene
		}

		sortBatchDrawInfos();

		// Draw sorted batches
#if !defined(EMSCRIPTEN)
		if(draw_wireframes)
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif

		//Timer timer3;
		const BatchDrawInfo* const batch_draw_info_data = temp_batch_draw_info.data();
		const size_t batch_draw_info_size = temp_batch_draw_info.size();
		for(size_t i=0; i<batch_draw_info_size; ++i)
		{
			const BatchDrawInfo& info = batch_draw_info_data[i];
			const uint32 prog_index = info.ob->batch_draw_info[info.batch_i].getProgramIndex();
			if(prog_index != current_bound_prog_index)
			{
				ZoneScopedN("changing prog"); // Tracy profiler

				if(use_multi_draw_indirect)
					submitBufferedDrawCommands(); // Flush existing draw commands

				// conPrint("---- Changed to program " + prog->prog_name + " ----");
				const OpenGLProgram* prog = this->prog_vector[prog_index].ptr();

				prog->useProgram();
				// conPrint("---- Changed to program " + prog->prog_name + " (index " + toString(prog_index) + ") ----");
				current_bound_prog = prog;
				current_bound_prog_index = prog_index;
				current_uniforms_ob = NULL; // Program has changed, so we need to set object uniforms for the current program.
				setSharedUniformsForProg(*prog, view_matrix, proj_matrix);
				num_prog_changes++;
			}

			bindMeshData(*info.ob);

			drawBatchWithDenormalisedData(*info.ob, info.ob->batch_draw_info[info.batch_i], info.batch_i);
		}

		flushDrawCommandsAndUnbindPrograms();

#if !defined(EMSCRIPTEN)
		if(draw_wireframes)
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // Restore normal fill mode
#endif

		if(current_scene->render_to_main_render_framebuffer)
		{
			// main_render_copy_framebuffer->discardContents(GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_DEPTH_ATTACHMENT);
			// main_render_framebuffer->bindForDrawing(); // Restore framebuffer binding
		}

		//glDepthMask(GL_TRUE); // Restore writing to depth buffer.

		//conPrint("Draw water objects batches took " + timer3.elapsedStringNSigFigs(4) + " for " + toString(num_batches_bound) + " batches");
	}
}


// Draw non-transparent (opaque) batches from objects
void OpenGLEngine::drawNonTransparentMaterialBatches(const Matrix4f& view_matrix, const Matrix4f& proj_matrix)
{
	ZoneScopedN("Draw opaque obs"); // Tracy profiler
	DebugGroup debug_group("Draw opaque obs");
	TracyGpuZone("Draw opaque obs");

	if(query_profiling_enabled && current_scene->collect_stats && time_individual_passes && draw_opaque_obs_gpu_timer->isIdle())
		draw_opaque_obs_gpu_timer->beginTimerQuery();

	//conPrint("-----------------------------------------------drawNonTransparentMaterialBatches--------------------------------------");

	//glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
	//glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	assertCurrentProgramIsZero();

	if(current_scene->render_to_main_render_framebuffer)
	{
		main_render_framebuffer->bindForDrawing();
		assert(main_render_framebuffer->getAttachedRenderBufferName(GL_COLOR_ATTACHMENT0) == main_colour_renderbuffer->buffer_name); // Check main colour renderbuffer is attached at GL_COLOR_ATTACHMENT0.
		assert(main_render_framebuffer->getAttachedRenderBufferName(GL_COLOR_ATTACHMENT1) == main_normal_renderbuffer->buffer_name); // Check main normal renderbuffer is attached at GL_COLOR_ATTACHMENT1.
		setTwoDrawBuffers(GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1); // Draw to colour and normal buffers.
	}
	else
	{
		if(this->target_frame_buffer)
		{
			this->target_frame_buffer->bindForDrawing();
			setSingleDrawBuffer(GL_COLOR_ATTACHMENT0); // Just draw to colour buffer, not normal buffer.
		}
		else
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // Bind to default frame buffer and use the draw buffer already set already for it.
	}
	

	//Timer timer;
	temp_batch_draw_info.reserve(current_scene->objects.size());
	temp_batch_draw_info.resize(0);

	uint64 num_frustum_culled = 0;
	{
		ZoneScopedN("frustum culling"); // Tracy profiler

		const Planef* frustum_clip_planes = current_scene->frustum_clip_planes;
		const int num_frustum_clip_planes = current_scene->num_frustum_clip_planes;
		const js::AABBox frustum_aabb = current_scene->frustum_aabb;
		const GLObjectRef* const current_scene_obs = current_scene->objects.vector.data();
		const size_t current_scene_obs_size        = current_scene->objects.vector.size();
		for(size_t i=0; i<current_scene_obs_size; ++i)
		{
			// Prefetch cachelines containing the variables we need for objects N places ahead in the array.
			if(i + 16 < current_scene_obs_size)
			{	
				_mm_prefetch((const char*)(&current_scene_obs[i + 16]->aabb_ws), _MM_HINT_T0);
				_mm_prefetch((const char*)(&current_scene_obs[i + 16]->aabb_ws) + 64, _MM_HINT_T0);
			}

			const GLObject* const ob = current_scene_obs[i].ptr();
			if(AABBIntersectsFrustum(frustum_clip_planes, num_frustum_clip_planes, frustum_aabb, ob->aabb_ws))
			{
				const size_t ob_batch_draw_info_size                  = ob->batch_draw_info.size();
				const GLObjectBatchDrawInfo* const ob_batch_draw_info = ob->batch_draw_info.data();
				for(uint32 z = 0; z < ob_batch_draw_info_size; ++z)
				{
					const uint32 prog_index_and_flags = ob_batch_draw_info[z].program_index_and_flags;
					const uint32 prog_index_and_face_culling_flag = prog_index_and_flags & ISOLATE_PROG_INDEX_AND_FACE_CULLING_MASK;

#ifndef NDEBUG
					const bool face_culling = !ob->materials[ob->getUsedBatches()[z].material_index].simple_double_sided && !ob->materials[ob->getUsedBatches()[z].material_index].fancy_double_sided;
					const uint32 face_culling_bits = face_culling ? ((ob->ob_to_world_matrix_determinant >= 0.f) ? SHIFTED_CULL_BACKFACE_BITS : SHIFTED_CULL_FRONTFACE_BITS) : 0;
					assert(prog_index_and_face_culling_flag == (ob->materials[ob->getUsedBatches()[z].material_index].shader_prog->program_index | face_culling_bits));
					assert(BitUtils::isBitSet(prog_index_and_flags, MATERIAL_TRANSPARENT_BITFLAG) == ob->materials[ob->getUsedBatches()[z].material_index].transparent);

					// Check the denormalised vao_and_vbo_key is correct
					const uint32 vao_id = ob->mesh_data->vao_data_index;
					const uint32 vbo_id = (uint32)ob->mesh_data->vbo_handle.vbo_id;
					const uint32 indices_vbo_id = (uint32)ob->mesh_data->indices_vbo_handle.vbo_id;
					const uint32 index_type_bits = ob->mesh_data->index_type_bits;
					assert(ob->vao_and_vbo_key == makeVAOAndVBOKey(vao_id, vbo_id, indices_vbo_id, index_type_bits));
#endif
					// Draw primitives for the given material
					// If transparent bit is not set, and water bit is not set, and decal bit is not set, and the program has finished building:
					if((prog_index_and_flags & (PROGRAM_FINISHED_BUILDING_BITFLAG | MATERIAL_TRANSPARENT_BITFLAG | MATERIAL_WATER_BITFLAG | MATERIAL_DECAL_BITFLAG | MATERIAL_ALPHA_BLEND_BITFLAG)) == PROGRAM_FINISHED_BUILDING_BITFLAG)
					{
						BatchDrawInfo info(
							prog_index_and_face_culling_flag,
							ob->vao_and_vbo_key,
							ob, // object ptr
							(uint32)z // batch_i
						);
						assert(((info.prog_vao_key << 10) >> 10) == ob->vao_and_vbo_key);
						temp_batch_draw_info.push_back(info);
					}
				}
			}
			else
				num_frustum_culled++;
		} // End for each object in scene

		if(current_scene->collect_stats)
			this->last_num_obs_in_frustum = current_scene->objects.size() - num_frustum_culled;
	}
	//conPrint("Draw opaque make batch loop took " + timer.elapsedStringMSWIthNSigFigs(4) + " for " + toString(current_scene->objects.vector.size()) + " objects");

	sortBatchDrawInfos();

	// Draw sorted batches
	num_prog_changes = 0;
	uint32 num_batches_bound = 0;
	num_vao_binds = 0;
	num_vbo_binds = 0;
	num_index_buf_binds = 0;
	uint32 num_face_culling_changes = 0;

	assertCurrentProgramIsZero();

	uint32 current_prog_index_and_face_culling = SHIFTED_CULL_BACKFACE_BITS | 1000000;
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

#if !defined(EMSCRIPTEN)
	if(draw_wireframes)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif

	// conPrint("================================================================================");
	// conPrint("OpenGLEngine::drawNonTransparentMaterialBatches()");

	//Timer timer3;
	const BatchDrawInfo* const batch_draw_info_data = temp_batch_draw_info.data();
	const size_t batch_draw_info_size = temp_batch_draw_info.size();
	for(size_t i=0; i<batch_draw_info_size; ++i)
	{
		const BatchDrawInfo& info = batch_draw_info_data[i];
		const GLObjectBatchDrawInfo& batch = info.ob->batch_draw_info[info.batch_i];
		const uint32 prog_index_and_face_culling = batch.getProgramIndexAndFaceCulling();

		[[maybe_unused]] const uint32 info_culling_bits = (info.prog_vao_key & ((1 << 22u) | (1 << 23u))) >> 22u;
		[[maybe_unused]] const uint32 batch_culling_bits = (prog_index_and_face_culling & ISOLATE_FACE_CULLING_MASK) >> MATERIAL_FACE_CULLING_BIT_INDEX;
		assert(info_culling_bits == batch_culling_bits);

		if(prog_index_and_face_culling != current_prog_index_and_face_culling)
		{
			ZoneScopedN("changing prog or face culling"); // Tracy profiler

			if(use_multi_draw_indirect)
				submitBufferedDrawCommands(); // Flush existing draw commands

			const uint32 face_culling = prog_index_and_face_culling & ISOLATE_FACE_CULLING_MASK;
			if(face_culling != (current_prog_index_and_face_culling & ISOLATE_FACE_CULLING_MASK)) // If face culling changed:
			{
				//conPrint("Setting face culling to " + faceCullingBitsDescrip(face_culling));
				setFaceCulling(face_culling);
				num_face_culling_changes++;
			}

			const uint32 prog_index = prog_index_and_face_culling & ISOLATE_PROG_INDEX_MASK;
			if(prog_index != (current_prog_index_and_face_culling & ISOLATE_PROG_INDEX_MASK)) // If program index changed:
			{
				const OpenGLProgram* prog = this->prog_vector[prog_index].ptr();

				// conPrint("---- Changed to program " + prog->prog_name + " (index " + toString(prog_index) + ") ----");

				prog->useProgram();
				current_bound_prog = prog;
				current_bound_prog_index = prog_index;
				current_uniforms_ob = NULL; // Program has changed, so we need to set object uniforms for the current program.
				setSharedUniformsForProg(*prog, view_matrix, proj_matrix);
				num_prog_changes++;
			}

			current_prog_index_and_face_culling = prog_index_and_face_culling;
		}

		bindMeshData(*info.ob);
		num_batches_bound++;

		drawBatchWithDenormalisedData(*info.ob, batch, info.batch_i);
	}

	if(current_scene->collect_stats)
	{
		last_num_prog_changes = num_prog_changes;
		last_num_batches_bound = num_batches_bound;
		last_num_vao_binds = num_vao_binds;
		last_num_vbo_binds = num_vbo_binds;
		last_num_index_buf_binds = num_index_buf_binds;
		last_num_indices_drawn = this->num_indices_submitted;
		last_num_face_culling_changes = num_face_culling_changes;
	}
	this->num_indices_submitted = 0;

	flushDrawCommandsAndUnbindPrograms();

	glDisable(GL_CULL_FACE); // Restore

#if !defined(EMSCRIPTEN)
	if(draw_wireframes)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // Restore normal fill mode
#endif

	if(query_profiling_enabled && draw_opaque_obs_gpu_timer->isRunning())
		draw_opaque_obs_gpu_timer->endTimerQuery();

	//conPrint("Draw opaque batches took " + timer3.elapsedStringMSWIthNSigFigs(4) + " for " + toString(num_batches_bound) + " batches");
}


void OpenGLEngine::drawTransparentMaterialBatches(const Matrix4f& view_matrix, const Matrix4f& proj_matrix)
{
	ZoneScopedN("Draw transparent obs"); // Tracy profiler
	DebugGroup debug_group("Draw transparent obs");
	TracyGpuZone("Draw transparent obs");
	assertCurrentProgramIsZero();

	if(current_scene->render_to_main_render_framebuffer)
	{
		main_render_framebuffer->bindForDrawing();
		assert(main_render_framebuffer->getAttachedRenderBufferName(GL_COLOR_ATTACHMENT0) == main_colour_renderbuffer->buffer_name); // Check main colour renderbuffer is attached at GL_COLOR_ATTACHMENT0.
		if(use_order_indep_transparency)
		{
			main_render_framebuffer->attachRenderBuffer(*total_transmittance_renderbuffer, GL_COLOR_ATTACHMENT0); // Replaces color buffer as GL_COLOR_ATTACHMENT0
			main_render_framebuffer->attachRenderBuffer(*transparent_accum_renderbuffer, GL_COLOR_ATTACHMENT1); // Replaces normal buffer as GL_COLOR_ATTACHMENT1
			setTwoDrawBuffers(GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1); // Draw to two colour buffers:  total_transmittance, transparent_accum.
		}
		else
		{
			setSingleDrawBuffer(GL_COLOR_ATTACHMENT0); // Just draw to colour buffer
		}
	}
	else
	{
		if(this->target_frame_buffer)
		{
			this->target_frame_buffer->bindForDrawing();
			setSingleDrawBuffer(GL_COLOR_ATTACHMENT0); // Just draw to colour buffer, not normal buffer.
		}
		else
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // Bind to default frame buffer and use the draw buffer already set already for it.
	}

	if(use_order_indep_transparency && current_scene->render_to_main_render_framebuffer)
	{
		// Clear total_transmittance_texture (GL_COLOR_ATTACHMENT0).
		// NOTE that glClearBufferfv uses draw buffer indices, so glDrawBuffers() needs to be called first.
		const float col_one[4] = { 1, 1, 1, 1 };
		glClearBufferfv(GL_COLOR, /*drawBuffer=*/0, col_one);

		// Clear transparent_accum_texture buffer (GL_COLOR_ATTACHMENT1)
		const float col_zero[4] = { 0, 0, 0, 0 };
		glClearBufferfv(GL_COLOR, /*drawBuffer=*/1, col_zero);
	}


	glEnable(GL_BLEND);

	if(use_order_indep_transparency && current_scene->render_to_main_render_framebuffer)
	{
		// glBlendFunci is only in OpenGL ES 3.2+ so we can't use it in Emscripten.
#if defined(EMSCRIPTEN)
		assert(0); // Shouldn't be using order_indep_transparency in Emscripten.
#else
		// For colour buffer 0 (total_transmittance_texture), transparent shader outputs transmittance as output 0.  We multiply the existing buffer colour with that transmittance.
		// res = source * 0 + dest * source = source * dest
		// res = transmittance * dest
		glBlendFunci(/*buf=*/0, /*source factor=*/GL_ZERO, /*destination factor=*/GL_SRC_COLOR);
	
		// For colour buffer 1 (transparent_accum_texture), accumulate the scattered and emitted light by the material (accum_out).
		// res = source * 1 + dest * 1 = source + dest
		// res = accum_out + dest
		glBlendFunci(/*buf=*/1, /*source factor=*/GL_ONE, /*destination factor=*/GL_ONE);
#endif
	}
	else
	{
		// We will use source factor = 1  (source factor is the blending factor for incoming colour).
		// To apply an alpha factor to the source colour if desired, we can just multiply by alpha in the fragment shader.
		// For completely additive blending (hologram shader), we don't multiply by alpha in the fragment shader, and set a fragment colour with alpha = 0, so dest factor = 1 - 0 = 1.
		// See https://gamedev.stackexchange.com/a/143117

		glBlendFunc(/*source factor=*/GL_ONE, /*destination factor=*/GL_ONE_MINUS_SRC_ALPHA);
	}


	glDepthMask(GL_FALSE); // Disable writing to depth buffer - we don't want to occlude other transparent objects.

	temp_batch_draw_info.resize(0);

	const GLObjectRef* const current_scene_trans_obs = current_scene->transparent_objects.vector.data();
	const size_t current_scene_trans_obs_size        = current_scene->transparent_objects.vector.size();
	for(size_t q=0; q<current_scene_trans_obs_size; ++q)
	{
		// Prefetch cachelines containing the variables we need for objects N places ahead in the array.
		if(q + 16 < current_scene_trans_obs_size)
		{	
			_mm_prefetch((const char*)(&current_scene_trans_obs[q + 16]->aabb_ws), _MM_HINT_T0);
			_mm_prefetch((const char*)(&current_scene_trans_obs[q + 16]->aabb_ws) + 64, _MM_HINT_T0);
		}

		const GLObject* const ob = current_scene_trans_obs[q].ptr();
		if(AABBIntersectsFrustum(current_scene->frustum_clip_planes, current_scene->num_frustum_clip_planes, current_scene->frustum_aabb, ob->aabb_ws))
		{
			const size_t ob_batch_draw_info_size                  = ob->batch_draw_info.size();
			const GLObjectBatchDrawInfo* const ob_batch_draw_info = ob->batch_draw_info.data();
			for(uint32 z = 0; z < ob_batch_draw_info_size; ++z)
			{
				const uint32 prog_index_and_flags = ob_batch_draw_info[z].program_index_and_flags;
				const uint32 prog_index_and_face_culling_flag = prog_index_and_flags & ISOLATE_PROG_INDEX_AND_FACE_CULLING_MASK;

#ifndef NDEBUG
				const bool face_culling = !ob->materials[ob->getUsedBatches()[z].material_index].simple_double_sided && !ob->materials[ob->getUsedBatches()[z].material_index].fancy_double_sided;
				const uint32 face_culling_bits = face_culling ? ((ob->ob_to_world_matrix_determinant >= 0.f) ? SHIFTED_CULL_BACKFACE_BITS : SHIFTED_CULL_FRONTFACE_BITS) : 0;
				assert(prog_index_and_face_culling_flag == (ob->materials[ob->getUsedBatches()[z].material_index].shader_prog->program_index | face_culling_bits));
				assert(BitUtils::isBitSet(prog_index_and_flags, MATERIAL_TRANSPARENT_BITFLAG) == ob->materials[ob->getUsedBatches()[z].material_index].transparent);
#endif
				if(BitUtils::areBitsSet(prog_index_and_flags, PROGRAM_FINISHED_BUILDING_BITFLAG | MATERIAL_TRANSPARENT_BITFLAG)) // If transparent bit is set, and program has finished building:
				{
					BatchDrawInfo info(
						prog_index_and_face_culling_flag,
						ob->vao_and_vbo_key,
						ob, // object ptr
						(uint32)z // batch_i
					);
					temp_batch_draw_info.push_back(info);
				}
			}
		}
	}

	sortBatchDrawInfos();

	// Draw sorted batches
	const BatchDrawInfo* const batch_draw_info_data = temp_batch_draw_info.data();
	const size_t batch_draw_info_size = temp_batch_draw_info.size();
	for(size_t i=0; i<batch_draw_info_size; ++i)
	{
		const BatchDrawInfo& info = batch_draw_info_data[i];
		const uint32 prog_index = info.ob->batch_draw_info[info.batch_i].getProgramIndex();
		const bool program_changed = checkUseProgram(prog_index);
		if(program_changed)
		{
			const OpenGLProgram* prog = prog_vector[prog_index].ptr();
			//conPrint("---- Changed to program " + prog->prog_name + " (index " + toString(prog_index) + ") ----");
			setSharedUniformsForProg(*prog, view_matrix, proj_matrix);
		}

		bindMeshData(*info.ob);
		drawBatchWithDenormalisedData(*info.ob, info.ob->batch_draw_info[info.batch_i], info.batch_i);
	}

	flushDrawCommandsAndUnbindPrograms();

	glDepthMask(GL_TRUE); // Re-enable writing to depth buffer.
	glDisable(GL_BLEND);

#if !defined(EMSCRIPTEN)
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif
	


	if(use_order_indep_transparency && current_scene->render_to_main_render_framebuffer)
	{
		//----------------------- Copy total_transmittance render buffer to total_transmittance_copy_texture -----------------------
		main_render_framebuffer->bindForReading(); // Set copy source framebuffer
		main_render_copy_framebuffer->bindForDrawing(); // Set copy destination framebuffer
		main_render_copy_framebuffer->attachTexture(*total_transmittance_copy_texture,  GL_COLOR_ATTACHMENT0);
		main_render_copy_framebuffer->attachTexture(*transparent_accum_copy_texture, GL_COLOR_ATTACHMENT1);

		glReadBuffer(GL_COLOR_ATTACHMENT0);
		setSingleDrawBuffer(GL_COLOR_ATTACHMENT0);
	
		glBlitFramebuffer(
			/*srcX0=*/0, /*srcY0=*/0, /*srcX1=*/(int)main_render_framebuffer->xRes(), /*srcY1=*/(int)main_render_framebuffer->yRes(), 
			/*dstX0=*/0, /*dstY0=*/0, /*dstX1=*/(int)main_render_framebuffer->xRes(), /*dstY1=*/(int)main_render_framebuffer->yRes(), 
			GL_COLOR_BUFFER_BIT,
			GL_NEAREST
		);

		//----------------------- Copy transparent_accum render buffer to transparent_accum_copy_texture -----------------------		
		glReadBuffer(GL_COLOR_ATTACHMENT1);
		setTwoDrawBuffers(GL_NONE, GL_COLOR_ATTACHMENT1); // In OpenGL ES, GL_COLOR_ATTACHMENT1 must be specified as buffer 1 (can't be buffer 0), so use glDrawBuffers with GL_NONE as buffer 0.

		glBlitFramebuffer(
			/*srcX0=*/0, /*srcY0=*/0, /*srcX1=*/(int)main_render_framebuffer->xRes(), /*srcY1=*/(int)main_render_framebuffer->yRes(), 
			/*dstX0=*/0, /*dstY0=*/0, /*dstX1=*/(int)main_render_framebuffer->xRes(), /*dstY1=*/(int)main_render_framebuffer->yRes(), 
			GL_COLOR_BUFFER_BIT,
			GL_NEAREST
		);


		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0); // Restore to no framebuffers bound for reading.

		// main_render_framebuffer->discardContents(GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1); // Discard total_transmittance and transparent_accum now it has been copied out of

		// Restore render buffer bindings for main_render_framebuffer and main_render_copy_framebuffer
		main_render_framebuffer->attachRenderBuffer(*main_colour_renderbuffer, GL_COLOR_ATTACHMENT0);
		main_render_framebuffer->attachRenderBuffer(*main_normal_renderbuffer, GL_COLOR_ATTACHMENT1);
		main_render_copy_framebuffer->attachTexture(*main_colour_copy_texture, GL_COLOR_ATTACHMENT0);
		main_render_copy_framebuffer->attachTexture(*main_normal_copy_texture, GL_COLOR_ATTACHMENT1);
	}
}


void OpenGLEngine::drawColourAndDepthPrePass(const Matrix4f& view_matrix, const Matrix4f& proj_matrix)
{
	if(current_scene->render_to_main_render_framebuffer)
	{
		ZoneScopedN("colour and depth pre-pass"); // Tracy profiler
		DebugGroup debug_group("colour and depth pre-pass");
		TracyGpuZone("colour and depth pre-pass");

		if(query_profiling_enabled && current_scene->collect_stats && time_individual_passes && depth_pre_pass_gpu_timer->isIdle())
			depth_pre_pass_gpu_timer->beginTimerQuery();

		assertCurrentProgramIsZero();

		prepass_framebuffer->bindForDrawing();
		assert(prepass_framebuffer->getAttachedRenderBufferName(GL_COLOR_ATTACHMENT0) == prepass_colour_renderbuffer->buffer_name);
		assert(prepass_framebuffer->getAttachedRenderBufferName(GL_COLOR_ATTACHMENT1) == prepass_normal_renderbuffer->buffer_name);
		assert(prepass_framebuffer->getAttachedRenderBufferName(GL_DEPTH_ATTACHMENT) == prepass_depth_renderbuffer->buffer_name);
		setTwoDrawBuffers(GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1); // Draw to colour and normal buffer


		glViewport(0, 0, (GLsizei)prepass_framebuffer->xRes(), (GLsizei)prepass_framebuffer->yRes());
		glClearColor(current_scene->background_colour.r, current_scene->background_colour.g, current_scene->background_colour.b, 1.f);
		//glClearDepthf(use_reverse_z ? 0.0f : 1.f); // For reversed-z, the 'far' z value is 0, instead of 1.
		glClearDepthf(use_reverse_z ? 0.000001f :0.999999f); // For reversed-z, the 'far' z value is 0, instead of 1.
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);



		const Vec4f campos_ws = this->getCameraPositionWS();

		//Timer timer;
		temp_batch_draw_info.reserve(current_scene->objects.size());
		temp_batch_draw_info.resize(0);

		uint64 num_frustum_culled = 0;
		{
			ZoneScopedN("frustum culling"); // Tracy profiler

			const Planef* frustum_clip_planes = current_scene->frustum_clip_planes;
			const int num_frustum_clip_planes = current_scene->num_frustum_clip_planes;
			const js::AABBox frustum_aabb = current_scene->frustum_aabb;
			const GLObjectRef* const current_scene_obs = current_scene->objects.vector.data();
			const size_t current_scene_obs_size        = current_scene->objects.vector.size();
			for(size_t i=0; i<current_scene_obs_size; ++i)
			{
				// Prefetch cachelines containing the variables we need for objects N places ahead in the array.
				if(i + 16 < current_scene_obs_size)
				{	
					_mm_prefetch((const char*)(&current_scene_obs[i + 16]->aabb_ws), _MM_HINT_T0);
					_mm_prefetch((const char*)(&current_scene_obs[i + 16]->aabb_ws) + 64, _MM_HINT_T0);
				}

				const GLObject* const ob = current_scene_obs[i].ptr();
				if(AABBIntersectsFrustum(frustum_clip_planes, num_frustum_clip_planes, frustum_aabb, ob->aabb_ws))
				{
					// Only draw objects near to camera
					if(ob->aabb_ws.distanceToPoint(campos_ws) > 80.f)
						continue;

					const size_t ob_batch_draw_info_size                  = ob->batch_draw_info.size();
					const GLObjectBatchDrawInfo* const ob_batch_draw_info = ob->batch_draw_info.data();
					for(uint32 z = 0; z < ob_batch_draw_info_size; ++z)
					{
						const uint32 prog_index_and_flags = ob_batch_draw_info[z].program_index_and_flags;
						const uint32 prog_index_and_face_culling_flag = prog_index_and_flags & ISOLATE_PROG_INDEX_AND_FACE_CULLING_MASK;

	#ifndef NDEBUG
						//const bool face_culling = !ob->materials[ob->depth_draw_batches[z].material_data_or_mat_index].simple_double_sided && !ob->materials[ob->depth_draw_batches[z].material_data_or_mat_index].fancy_double_sided;
						//assert(prog_index_and_face_culling_flag == (ob->materials[ob->getUsedBatches()[z].material_index].shader_prog->program_index | (face_culling ? BACKFACE_CULLING_BITFLAG : 0)));
						//assert(BitUtils::isBitSet(prog_index_and_flags, MATERIAL_TRANSPARENT_BITFLAG) == ob->materials[ob->getUsedBatches()[z].material_index].transparent);

						// Check the denormalised vao_and_vbo_key is correct
						const uint32 vao_id = ob->mesh_data->vao_data_index;
						const uint32 vbo_id = (uint32)ob->mesh_data->vbo_handle.vbo_id;
						const uint32 indices_vbo_id = (uint32)ob->mesh_data->indices_vbo_handle.vbo_id;
						const uint32 index_type_bits = ob->mesh_data->index_type_bits;
						assert(ob->vao_and_vbo_key == makeVAOAndVBOKey(vao_id, vbo_id, indices_vbo_id, index_type_bits));
	#endif
						// Draw primitives for the given material
						// If transparent bit is not set, and water bit is not set, and decal bit is not set, and the program has finished building:
						if((prog_index_and_flags & (PROGRAM_FINISHED_BUILDING_BITFLAG | MATERIAL_TRANSPARENT_BITFLAG | MATERIAL_WATER_BITFLAG | MATERIAL_DECAL_BITFLAG | MATERIAL_ALPHA_BLEND_BITFLAG)) == PROGRAM_FINISHED_BUILDING_BITFLAG)
						{
							BatchDrawInfo info(
								prog_index_and_face_culling_flag,
								ob->vao_and_vbo_key,
								ob, // object ptr
								(uint32)z // batch_i
							);
							temp_batch_draw_info.push_back(info);
						}
					}
				}
				else
					num_frustum_culled++;
			} // End for each object in scene

			if(current_scene->collect_stats)
				this->last_num_obs_in_frustum = current_scene->objects.size() - num_frustum_culled;
		}
		//conPrint("Draw opaque make batch loop took " + timer.elapsedStringNSigFigs(4));

		sortBatchDrawInfos();

		// Draw sorted batches
		num_prog_changes = 0;
		uint32 num_batches_bound = 0;
		num_vao_binds = 0;
		num_vbo_binds = 0;
		num_index_buf_binds = 0;
		uint32 num_face_culling_changes = 0;

		assertCurrentProgramIsZero();

		uint32 current_prog_index_and_face_culling = SHIFTED_CULL_BACKFACE_BITS | 1000000;
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);

	#if !defined(EMSCRIPTEN)
		if(draw_wireframes)
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	#endif

		//Timer timer3;
		const BatchDrawInfo* const batch_draw_info_data = temp_batch_draw_info.data();
		const size_t batch_draw_info_size = temp_batch_draw_info.size();
		for(size_t i=0; i<batch_draw_info_size; ++i)
		{
			const BatchDrawInfo& info = batch_draw_info_data[i];
			const GLObjectBatchDrawInfo& batch = info.ob->batch_draw_info[info.batch_i];
			const uint32 prog_index_and_face_culling = batch.getProgramIndexAndFaceCulling();
			if(prog_index_and_face_culling != current_prog_index_and_face_culling)
			{
				ZoneScopedN("changing prog"); // Tracy profiler

				if(use_multi_draw_indirect)
					submitBufferedDrawCommands(); // Flush existing draw commands

				const uint32 face_culling      = prog_index_and_face_culling         & ISOLATE_FACE_CULLING_MASK;
				const uint32 prev_face_culling = current_prog_index_and_face_culling & ISOLATE_FACE_CULLING_MASK;
				if(face_culling != prev_face_culling)
				{
					setFaceCulling(face_culling);
					num_face_culling_changes++;
				}

				const uint32 prog_index = prog_index_and_face_culling & ISOLATE_PROG_INDEX_MASK;
				if(prog_index != (current_prog_index_and_face_culling & ISOLATE_PROG_INDEX_MASK)) // If prog index changed:
				{
					const OpenGLProgram* prog = this->prog_vector[prog_index].ptr();
					//conPrint("---- Changed to program " + prog->prog_name + " (index " + toString(prog_index) + ") ----");
					prog->useProgram();
					current_bound_prog = prog;
					current_bound_prog_index = prog_index;
					current_uniforms_ob = NULL; // Program has changed, so we need to set object uniforms for the current program.
					setSharedUniformsForProg(*prog, view_matrix, proj_matrix);
					num_prog_changes++;
				}

				current_prog_index_and_face_culling = prog_index_and_face_culling;
			}

			bindMeshData(*info.ob);
			num_batches_bound++;

			drawBatchWithDenormalisedData(*info.ob, batch, info.batch_i);
		}

		/*if(current_scene->collect_stats)
		{
			last_num_prog_changes = num_prog_changes;
			last_num_batches_bound = num_batches_bound;
			last_num_vao_binds = num_vao_binds;
			last_num_vbo_binds = num_vbo_binds;
			last_num_index_buf_binds = num_index_buf_binds;
			last_num_indices_drawn = this->num_indices_submitted;
			last_num_face_culling_changes = num_face_culling_changes;
		}
		this->num_indices_submitted = 0;*/

		flushDrawCommandsAndUnbindPrograms();

		glDisable(GL_CULL_FACE); // Restore

	#if !defined(EMSCRIPTEN)
		if(draw_wireframes)
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // Restore normal fill mode
	#endif

		if(query_profiling_enabled && depth_pre_pass_gpu_timer->isRunning())
			depth_pre_pass_gpu_timer->endTimerQuery();


		// Copy depth buffer from main render framebuffer to render copy framebuffer.
		// This will copy main_depth_texture to main_depth_copy_texture.
		//if(current_scene->render_to_main_render_framebuffer)
		//{
		//	// Copy framebuffer textures from main render framebuffer to main render copy framebuffer.
		//	// This will copy main_colour_texture to main_colour_copy_texture, and main_depth_texture to main_depth_copy_texture.
		//	//
		//	// From https://www.khronos.org/opengl/wiki/Framebuffer#Blitting:
		//	// " the only colors read will come from the read color buffer in the read FBO, specified by glReadBuffer. 
		//	// The colors written will only go to the draw color buffers in the write FBO, specified by glDrawBuffers. 
		//	// If multiple draw buffers are specified, then multiple color buffers are updated with the same data."
		//	//
		//	// 
		//	main_render_framebuffer->bindForReading();

		//	main_render_copy_framebuffer->bindForDrawing();

		//	//--------------- Copy depth buffer. ---------------
		//	//glReadBuffer(GL_COLOR_ATTACHMENT0);
		//	//setSingleDrawBuffer(GL_COLOR_ATTACHMENT0);
		//	glReadBuffer(GL_COLOR_ATTACHMENT0); // Shouldn't be used.
		//	glDrawBuffers(0, nullptr);

		//	glBlitFramebuffer(
		//		/*srcX0=*/0, /*srcY0=*/0, /*srcX1=*/(int)main_render_framebuffer->xRes(), /*srcY1=*/(int)main_render_framebuffer->yRes(), 
		//		/*dstX0=*/0, /*dstY0=*/0, /*dstX1=*/(int)main_render_framebuffer->xRes(), /*dstY1=*/(int)main_render_framebuffer->yRes(), 
		//		GL_DEPTH_BUFFER_BIT,
		//		GL_NEAREST
		//	);
		//}

		// Restore viewport
		glViewport(0, 0, viewport_w, viewport_h);
	}
}


void OpenGLEngine::drawDepthPrePass(const Matrix4f& view_matrix, const Matrix4f& proj_matrix)
{
	ZoneScopedN("Depth pre-pass"); // Tracy profiler
	DebugGroup debug_group("Depth pre-pass");
	TracyGpuZone("Depth pre-pass");

	if(query_profiling_enabled && current_scene->collect_stats && time_individual_passes && depth_pre_pass_gpu_timer->isIdle())
		depth_pre_pass_gpu_timer->beginTimerQuery();

	assertCurrentProgramIsZero();

/*	if(current_scene->render_to_main_render_framebuffer)
	{
		main_render_framebuffer->bindForDrawing();
		//assert(main_render_framebuffer->getAttachedRenderBufferName(GL_COLOR_ATTACHMENT0) == main_colour_renderbuffer->buffer_name); // Check main colour renderbuffer is attached at GL_COLOR_ATTACHMENT0.
		//assert(main_render_framebuffer->getAttachedRenderBufferName(GL_COLOR_ATTACHMENT1) == main_normal_renderbuffer->buffer_name); // Check main normal renderbuffer is attached at GL_COLOR_ATTACHMENT1.
		//setTwoDrawBuffers(GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1); // Draw to colour and normal buffers.
	}
	else
	{
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, this->target_frame_buffer.nonNull() ? this->target_frame_buffer->buffer_name : 0);
		//setSingleDrawBuffer(this->target_frame_buffer.nonNull() ? GL_COLOR_ATTACHMENT0 : GL_BACK); // Just draw to colour buffer, not normal buffer. GL_BACK is required for targetting default framebuffer
	}*/

	prepass_framebuffer->bindForDrawing();

	glDrawBuffers(/*num=*/0, NULL); // Don't draw to any colour buffers. (there are none attached to the frame buffer anyway)

	glViewport(0, 0, (GLsizei)prepass_framebuffer->xRes(), (GLsizei)prepass_framebuffer->yRes());
	
	glClearColor(current_scene->background_colour.r, current_scene->background_colour.g, current_scene->background_colour.b, 1.f);
	glClearDepthf(use_reverse_z ? 0.0f : 1.f); // For reversed-z, the 'far' z value is 0, instead of 1.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//Timer timer;
	temp_batch_draw_info.reserve(current_scene->objects.size());
	temp_batch_draw_info.resize(0);

	const Vec4f campos_ws = this->getCameraPositionWS();

	uint64 num_frustum_culled = 0;
	{
		ZoneScopedN("frustum culling"); // Tracy profiler

		const Planef* frustum_clip_planes = current_scene->frustum_clip_planes;
		const int num_frustum_clip_planes = current_scene->num_frustum_clip_planes;
		const js::AABBox frustum_aabb = current_scene->frustum_aabb;
		const GLObjectRef* const current_scene_obs = current_scene->objects.vector.data();
		const size_t current_scene_obs_size        = current_scene->objects.vector.size();
		for(size_t i=0; i<current_scene_obs_size; ++i)
		{
			// Prefetch cachelines containing the variables we need for objects N places ahead in the array.
			if(i + 16 < current_scene_obs_size)
			{	
				_mm_prefetch((const char*)(&current_scene_obs[i + 16]->aabb_ws), _MM_HINT_T0);
				_mm_prefetch((const char*)(&current_scene_obs[i + 16]->aabb_ws) + 64, _MM_HINT_T0);
			}

			const GLObject* const ob = current_scene_obs[i].ptr();
			if(AABBIntersectsFrustum(frustum_clip_planes, num_frustum_clip_planes, frustum_aabb, ob->aabb_ws))
			{
				if(ob->aabb_ws.distanceToPoint(campos_ws) > 80.f)
					continue;

				const size_t ob_batch_draw_info_size                  = ob->depth_draw_batches/*batch_draw_info*/.size();
				const GLObjectBatchDrawInfo* const ob_batch_draw_info = ob->depth_draw_batches/*batch_draw_info*/.data();
				for(uint32 z = 0; z < ob_batch_draw_info_size; ++z)
				{
					const uint32 prog_index_and_flags = ob_batch_draw_info[z].program_index_and_flags;
					const uint32 prog_index_and_face_culling_flag = prog_index_and_flags & ISOLATE_PROG_INDEX_AND_FACE_CULLING_MASK;

#ifndef NDEBUG
					//const bool face_culling = !ob->materials[ob->depth_draw_batches[z].material_data_or_mat_index].simple_double_sided && !ob->materials[ob->depth_draw_batches[z].material_data_or_mat_index].fancy_double_sided;
					//assert(prog_index_and_face_culling_flag == (ob->materials[ob->getUsedBatches()[z].material_index].shader_prog->program_index | (face_culling ? BACKFACE_CULLING_BITFLAG : 0)));
					//assert(BitUtils::isBitSet(prog_index_and_flags, MATERIAL_TRANSPARENT_BITFLAG) == ob->materials[ob->getUsedBatches()[z].material_index].transparent);

					// Check the denormalised vao_and_vbo_key is correct
					const uint32 vao_id = ob->mesh_data->vao_data_index;
					const uint32 vbo_id = (uint32)ob->mesh_data->vbo_handle.vbo_id;
					const uint32 indices_vbo_id = (uint32)ob->mesh_data->indices_vbo_handle.vbo_id;
					const uint32 index_type_bits = ob->mesh_data->index_type_bits;
					assert(ob->vao_and_vbo_key == makeVAOAndVBOKey(vao_id, vbo_id, indices_vbo_id, index_type_bits));
#endif
					// Draw primitives for the given material
					// If transparent bit is not set, and water bit is not set, and decal bit is not set, and the program has finished building:
					if((prog_index_and_flags & (PROGRAM_FINISHED_BUILDING_BITFLAG | MATERIAL_TRANSPARENT_BITFLAG | MATERIAL_WATER_BITFLAG | MATERIAL_DECAL_BITFLAG | MATERIAL_ALPHA_BLEND_BITFLAG)) == PROGRAM_FINISHED_BUILDING_BITFLAG)
					{
						BatchDrawInfo info(
							prog_index_and_face_culling_flag,
							ob->vao_and_vbo_key,
							ob, // object ptr
							(uint32)z // batch_i
						);
						temp_batch_draw_info.push_back(info);
					}
				}
			}
			else
				num_frustum_culled++;
		} // End for each object in scene

		if(current_scene->collect_stats)
			this->last_num_obs_in_frustum = current_scene->objects.size() - num_frustum_culled;
	}
	//conPrint("Draw opaque make batch loop took " + timer.elapsedStringNSigFigs(4));

	sortBatchDrawInfos();

	// Draw sorted batches
	num_prog_changes = 0;
	uint32 num_batches_bound = 0;
	num_vao_binds = 0;
	num_vbo_binds = 0;
	num_index_buf_binds = 0;
	uint32 num_face_culling_changes = 0;

	assertCurrentProgramIsZero();

	uint32 current_prog_index_and_face_culling = SHIFTED_CULL_BACKFACE_BITS | 1000000;
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

#if !defined(EMSCRIPTEN)
	if(draw_wireframes)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif

	//Timer timer3;
	const BatchDrawInfo* const batch_draw_info_data = temp_batch_draw_info.data();
	const size_t batch_draw_info_size = temp_batch_draw_info.size();
	for(size_t i=0; i<batch_draw_info_size; ++i)
	{
		const BatchDrawInfo& info = batch_draw_info_data[i];
		const GLObjectBatchDrawInfo& batch = info.ob->depth_draw_batches/*batch_draw_info*/[info.batch_i];
		const uint32 prog_index_and_face_culling = batch.getProgramIndexAndFaceCulling();
		if(prog_index_and_face_culling != current_prog_index_and_face_culling)
		{
			ZoneScopedN("changing prog"); // Tracy profiler

			if(use_multi_draw_indirect)
				submitBufferedDrawCommands(); // Flush existing draw commands

			const uint32 face_culling      = prog_index_and_face_culling         & ISOLATE_FACE_CULLING_MASK;
			const uint32 prev_face_culling = current_prog_index_and_face_culling & ISOLATE_FACE_CULLING_MASK;
			if(face_culling != prev_face_culling)
			{
				setFaceCulling(face_culling);
				num_face_culling_changes++;
			}

			const uint32 prog_index = prog_index_and_face_culling & ISOLATE_PROG_INDEX_MASK;
			if(prog_index != (current_prog_index_and_face_culling & ISOLATE_PROG_INDEX_MASK)) // If prog index changed:
			{
				const OpenGLProgram* prog = this->prog_vector[prog_index].ptr();
				//conPrint("---- Changed to program " + prog->prog_name + " (index " + toString(prog_index) + ") ----");
				prog->useProgram();
				current_bound_prog = prog;
				current_bound_prog_index = prog_index;
				current_uniforms_ob = NULL; // Program has changed, so we need to set object uniforms for the current program.
				setSharedUniformsForProg(*prog, view_matrix, proj_matrix);
				num_prog_changes++;
			}

			current_prog_index_and_face_culling = prog_index_and_face_culling;
		}

		bindMeshData(*info.ob);
		num_batches_bound++;

		drawBatchWithDenormalisedData(*info.ob, batch, info.batch_i);
	}

	//printVar(num_face_culling_changes);
	//printVar(num_prog_changes);
	/*if(current_scene->collect_stats)
	{
		last_num_prog_changes = num_prog_changes;
		last_num_batches_bound = num_batches_bound;
		last_num_vao_binds = num_vao_binds;
		last_num_vbo_binds = num_vbo_binds;
		last_num_index_buf_binds = num_index_buf_binds;
		last_num_indices_drawn = this->num_indices_submitted;
		last_num_face_culling_changes = num_face_culling_changes;
	}
	this->num_indices_submitted = 0;*/

	flushDrawCommandsAndUnbindPrograms();

	glDisable(GL_CULL_FACE); // Restore

#if !defined(EMSCRIPTEN)
	if(draw_wireframes)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // Restore normal fill mode
#endif

	if(query_profiling_enabled && depth_pre_pass_gpu_timer->isRunning())
		depth_pre_pass_gpu_timer->endTimerQuery();


	// Copy depth buffer from main render framebuffer to render copy framebuffer.
	// This will copy main_depth_texture to main_depth_copy_texture.
	if(current_scene->render_to_main_render_framebuffer)
	{
		// Copy framebuffer textures from main render framebuffer to main render copy framebuffer.
		// This will copy main_colour_texture to main_colour_copy_texture, and main_depth_texture to main_depth_copy_texture.
		//
		// From https://www.khronos.org/opengl/wiki/Framebuffer#Blitting:
		// " the only colors read will come from the read color buffer in the read FBO, specified by glReadBuffer. 
		// The colors written will only go to the draw color buffers in the write FBO, specified by glDrawBuffers. 
		// If multiple draw buffers are specified, then multiple color buffers are updated with the same data."
		//
		// 
		main_render_framebuffer->bindForReading();

		main_render_copy_framebuffer->bindForDrawing();

		//--------------- Copy depth buffer. ---------------
		//glReadBuffer(GL_COLOR_ATTACHMENT0);
		//setSingleDrawBuffer(GL_COLOR_ATTACHMENT0);
		glReadBuffer(GL_COLOR_ATTACHMENT0); // Shouldn't be used.
		glDrawBuffers(0, nullptr);

		glBlitFramebuffer(
			/*srcX0=*/0, /*srcY0=*/0, /*srcX1=*/(int)main_render_framebuffer->xRes(), /*srcY1=*/(int)main_render_framebuffer->yRes(), 
			/*dstX0=*/0, /*dstY0=*/0, /*dstX1=*/(int)main_render_framebuffer->xRes(), /*dstY1=*/(int)main_render_framebuffer->yRes(), 
			GL_DEPTH_BUFFER_BIT,
			GL_NEAREST
		);
	}
}


// Blits from prepass_framebuffer to prepass_copy_framebuffer
// Computes SSAO, writes output to compute_ssao_framebuffer / compute_ssao_texture
void OpenGLEngine::computeSSAO(const Matrix4f& /*proj_matrix*/)
{
	if(current_scene->render_to_main_render_framebuffer)
	{
		assertCurrentProgramIsZero();

		{
			ZoneScopedN("computeSSAO"); // Tracy profiler
			TracyGpuZone("computeSSAO");
			DebugGroup debug_group("computeSSAO");
		
			
			//---------------------- Blit from prepass_framebuffer to prepass_copy_framebuffer ----------------------
			if(query_profiling_enabled && current_scene->collect_stats && time_individual_passes && copy_prepass_buffers_gpu_timer->isIdle())
				copy_prepass_buffers_gpu_timer->beginTimerQuery();

			prepass_framebuffer->bindForReading();
			prepass_copy_framebuffer->bindForDrawing();

			// Copy colour and depth buffer
			glReadBuffer(GL_COLOR_ATTACHMENT0);
			setSingleDrawBuffer(GL_COLOR_ATTACHMENT0);

			glBlitFramebuffer(
				/*srcX0=*/0, /*srcY0=*/0, /*srcX1=*/(int)prepass_framebuffer->xRes(), /*srcY1=*/(int)prepass_framebuffer->yRes(), 
				/*dstX0=*/0, /*dstY0=*/0, /*dstX1=*/(int)prepass_framebuffer->xRes(), /*dstY1=*/(int)prepass_framebuffer->yRes(), 
				GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
				GL_NEAREST
			);

			// Copy normal buffer.
			glReadBuffer(GL_COLOR_ATTACHMENT1);
			setTwoDrawBuffers(GL_NONE, GL_COLOR_ATTACHMENT1); // In OpenGL ES, GL_COLOR_ATTACHMENT1 must be specified as buffer 1 (can't be buffer 0), so use glDrawBuffers with GL_NONE as buffer 0.
	
			glBlitFramebuffer(
				/*srcX0=*/0, /*srcY0=*/0, /*srcX1=*/(int)prepass_framebuffer->xRes(), /*srcY1=*/(int)prepass_framebuffer->yRes(), 
				/*dstX0=*/0, /*dstY0=*/0, /*dstX1=*/(int)prepass_framebuffer->xRes(), /*dstY1=*/(int)prepass_framebuffer->yRes(), 
				GL_COLOR_BUFFER_BIT,
				GL_NEAREST
			);

			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0); // Unbind any framebuffer from readback operations.

			if(query_profiling_enabled && copy_prepass_buffers_gpu_timer->isRunning())
				copy_prepass_buffers_gpu_timer->endTimerQuery();
			//-----------------------------------------------------------------------------------------------------


			if(query_profiling_enabled && current_scene->collect_stats && time_individual_passes && compute_ssao_gpu_timer->isIdle())
				compute_ssao_gpu_timer->beginTimerQuery();

			glViewport(0, 0, (GLsizei)prepass_framebuffer->xRes(), (GLsizei)prepass_framebuffer->yRes());

			compute_ssao_framebuffer->bindForDrawing();
			setTwoDrawBuffers(GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1);


			glDepthMask(GL_FALSE); // Don't write to z-buffer
			glDisable(GL_DEPTH_TEST); // Disable depth testing

			// Position quad to cover viewport
			const float use_z = 0.5f; // Use a z value that will be in the clip volume for both default and reverse z.
			const Matrix4f ob_to_world_matrix = Matrix4f::scaleMatrix(2.f, 2.f, 1.f) * Matrix4f::translationMatrix(Vec4f(-0.5, -0.5, use_z, 0));

			const OpenGLMeshRenderData& mesh_data = *outline_quad_meshdata;
			bindMeshData(mesh_data);
			for(uint32 z = 0; z < mesh_data.batches.size(); ++z)
			{
				checkUseProgram(compute_ssao_prog.ptr());

				glUniformMatrix4fv(compute_ssao_prog->model_matrix_loc, 1, false, /*ob->*/ob_to_world_matrix.e);

				bindTextureUnitToSampler(*prepass_colour_copy_texture, PREPASS_COLOUR_COPY_TEXTURE_UNIT_INDEX, compute_ssao_prog->uniform_locations.diffuse_tex_location);
				bindTextureUnitToSampler(*prepass_normal_copy_texture, PREPASS_NORMAL_COPY_TEXTURE_UNIT_INDEX, compute_ssao_normal_tex_location);
				bindTextureUnitToSampler(*prepass_depth_copy_texture, PREPASS_DEPTH_COPY_TEXTURE_UNIT_INDEX, compute_ssao_depth_tex_location);

				drawElementsBaseVertex(GL_TRIANGLES, (GLsizei)mesh_data.batches[0].num_indices, mesh_data.getIndexType(), (void*)mesh_data.getBatch0IndicesTotalBufferOffset(), mesh_data.vbo_handle.base_vertex);
			}

			flushDrawCommandsAndUnbindPrograms();


			if(query_profiling_enabled && compute_ssao_gpu_timer->isRunning())
				compute_ssao_gpu_timer->endTimerQuery();
		}


		//-------------------------------- Execute blur SSAO shader --------------------------------
		{
			ZoneScopedN("blurSSAO"); // Tracy profiler
			TracyGpuZone("blurSSAO");
			DebugGroup debug_group("blurSSAO");
			
			if(query_profiling_enabled && current_scene->collect_stats && time_individual_passes && blur_ssao_gpu_timer->isIdle())
				blur_ssao_gpu_timer->beginTimerQuery();

			blur_ssao_prog->useProgram();
			glViewport(0, 0, (int)blurred_ssao_framebuffer_x->xRes(), (int)blurred_ssao_framebuffer_x->yRes()); // Set viewport to target texture size
			assert(blurred_ssao_framebuffer->xRes() == blurred_ssao_framebuffer_x->xRes() && blurred_ssao_framebuffer->xRes() == blurred_ssao_specular_framebuffer->xRes());

			// Blur irradiance + AO texture

			// First blur in x direction
			// Input: ssao_texture, prepass_colour_copy_texture (for roughness) prepass_depth_copy_texture, prepass_normal_copy_texture
			// Output: blurred_ssao_texture_x (via blurred_ssao_framebuffer_x)
			{
				blurred_ssao_framebuffer_x->bindForDrawing();

				bindTextureUnitToSampler(*ssao_texture, /*texture_unit_index=*/SSAO_TEXTURE_UNIT_INDEX, /*sampler_uniform_location=*/blur_ssao_prog->albedo_texture_loc);
				assert(blur_ssao_prog->uniform_locations.main_depth_texture_location >= 0);
				bindTextureUnitToSampler(*prepass_depth_copy_texture,  /*texture_unit_index=*/PREPASS_DEPTH_COPY_TEXTURE_UNIT_INDEX, /*sampler_uniform_location=*/blur_ssao_prog->uniform_locations.main_depth_texture_location);
				bindTextureUnitToSampler(*prepass_normal_copy_texture, /*texture_unit_index=*/PREPASS_NORMAL_COPY_TEXTURE_UNIT_INDEX, /*sampler_uniform_location=*/blur_ssao_prog->uniform_locations.main_normal_texture_location);
				bindTextureUnitToSampler(*prepass_colour_copy_texture, /*texture_unit_index=*/PREPASS_COLOUR_COPY_TEXTURE_UNIT_INDEX, /*sampler_uniform_location=*/blur_ssao_prog->uniform_locations.main_colour_texture_location);

				glUniform1i(blur_ssao_prog->user_uniform_info[0].loc, /*val=*/1); // set is_ssao_blur = 1
				glUniform1i(blur_ssao_prog->user_uniform_info[1].loc, /*val=*/1); // set blur_x = 1

				// Draw quad
				drawElementsBaseVertex(GL_TRIANGLES, (GLsizei)unit_quad_meshdata->batches[0].num_indices, unit_quad_meshdata->getIndexType(), (void*)unit_quad_meshdata->getBatch0IndicesTotalBufferOffset(), 
					unit_quad_meshdata->vbo_handle.base_vertex); 
			}

			// Do blur in y direction
			// Input: blurred_ssao_texture_x, prepass textures
			// Output: blurred_ssao_texture (via blurred_ssao_framebuffer)
			{
				blurred_ssao_framebuffer->bindForDrawing();

				bindTextureUnitToSampler(*blurred_ssao_texture_x, /*texture_unit_index=*/SSAO_TEXTURE_UNIT_INDEX, /*sampler_uniform_location=*/blur_ssao_prog->albedo_texture_loc);

				assert(blur_ssao_prog->uniform_locations.main_depth_texture_location >= 0);
				bindTextureUnitToSampler(*prepass_depth_copy_texture,  /*texture_unit_index=*/PREPASS_DEPTH_COPY_TEXTURE_UNIT_INDEX, /*sampler_uniform_location=*/blur_ssao_prog->uniform_locations.main_depth_texture_location);
				bindTextureUnitToSampler(*prepass_normal_copy_texture, /*texture_unit_index=*/PREPASS_NORMAL_COPY_TEXTURE_UNIT_INDEX, /*sampler_uniform_location=*/blur_ssao_prog->uniform_locations.main_normal_texture_location);
				bindTextureUnitToSampler(*prepass_colour_copy_texture, /*texture_unit_index=*/PREPASS_COLOUR_COPY_TEXTURE_UNIT_INDEX, /*sampler_uniform_location=*/blur_ssao_prog->uniform_locations.main_colour_texture_location);

				glUniform1i(blur_ssao_prog->user_uniform_info[0].loc, /*val=*/1); // set is_ssao_blur = 1
				glUniform1i(blur_ssao_prog->user_uniform_info[1].loc, /*val=*/0); // set blur_x = 0

				// Draw quad
				drawElementsBaseVertex(GL_TRIANGLES, (GLsizei)unit_quad_meshdata->batches[0].num_indices, unit_quad_meshdata->getIndexType(), (void*)unit_quad_meshdata->getBatch0IndicesTotalBufferOffset(), 
					unit_quad_meshdata->vbo_handle.base_vertex); 
			}


			// Blur specular texture
			
			// First do blur in x direction
			// Input: ssao_specular_texture, prepass textures
			// Output: blurred_ssao_texture_x (via blurred_ssao_framebuffer_x)
			{
				blurred_ssao_framebuffer_x->bindForDrawing();

				bindTextureUnitToSampler(*ssao_specular_texture, /*texture_unit_index=*/SSAO_TEXTURE_UNIT_INDEX, /*sampler_uniform_location=*/blur_ssao_prog->albedo_texture_loc);
				assert(blur_ssao_prog->uniform_locations.main_depth_texture_location >= 0);
				bindTextureUnitToSampler(*prepass_depth_copy_texture, /*texture_unit_index=*/PREPASS_DEPTH_COPY_TEXTURE_UNIT_INDEX, /*sampler_uniform_location=*/blur_ssao_prog->uniform_locations.main_depth_texture_location);
				bindTextureUnitToSampler(*prepass_normal_copy_texture, /*texture_unit_index=*/PREPASS_NORMAL_COPY_TEXTURE_UNIT_INDEX, /*sampler_uniform_location=*/blur_ssao_prog->uniform_locations.main_normal_texture_location);
				bindTextureUnitToSampler(*prepass_colour_copy_texture, /*texture_unit_index=*/PREPASS_COLOUR_COPY_TEXTURE_UNIT_INDEX, /*sampler_uniform_location=*/blur_ssao_prog->uniform_locations.main_colour_texture_location);

				glUniform1i(blur_ssao_prog->user_uniform_info[0].loc, /*val=*/0); // set is_ssao_blur = 0
				glUniform1i(blur_ssao_prog->user_uniform_info[1].loc, /*val=*/1); // set blur_x = 1

				// Draw quad
				drawElementsBaseVertex(GL_TRIANGLES, (GLsizei)unit_quad_meshdata->batches[0].num_indices, unit_quad_meshdata->getIndexType(), (void*)unit_quad_meshdata->getBatch0IndicesTotalBufferOffset(), 
					unit_quad_meshdata->vbo_handle.base_vertex); 
			}

			// Do blur in y direction
			// Input: blurred_ssao_texture_x, prepass textures
			// Output: blurred_ssao_specular_texture (via blurred_ssao_specular_framebuffer)
			{
				blurred_ssao_specular_framebuffer->bindForDrawing();

				bindTextureUnitToSampler(*blurred_ssao_texture_x, /*texture_unit_index=*/SSAO_TEXTURE_UNIT_INDEX, /*sampler_uniform_location=*/blur_ssao_prog->albedo_texture_loc);
				assert(blur_ssao_prog->uniform_locations.main_depth_texture_location >= 0);
				bindTextureUnitToSampler(*prepass_depth_copy_texture, /*texture_unit_index=*/PREPASS_DEPTH_COPY_TEXTURE_UNIT_INDEX, /*sampler_uniform_location=*/blur_ssao_prog->uniform_locations.main_depth_texture_location);
				bindTextureUnitToSampler(*prepass_normal_copy_texture, /*texture_unit_index=*/PREPASS_NORMAL_COPY_TEXTURE_UNIT_INDEX, /*sampler_uniform_location=*/blur_ssao_prog->uniform_locations.main_normal_texture_location);
				bindTextureUnitToSampler(*prepass_colour_copy_texture, /*texture_unit_index=*/PREPASS_COLOUR_COPY_TEXTURE_UNIT_INDEX, /*sampler_uniform_location=*/blur_ssao_prog->uniform_locations.main_colour_texture_location);

				glUniform1i(blur_ssao_prog->user_uniform_info[0].loc, /*val=*/0); // set is_ssao_blur = 0
				glUniform1i(blur_ssao_prog->user_uniform_info[1].loc, /*val=*/0); // set blur_x = 0

				// Draw quad
				drawElementsBaseVertex(GL_TRIANGLES, (GLsizei)unit_quad_meshdata->batches[0].num_indices, unit_quad_meshdata->getIndexType(), (void*)unit_quad_meshdata->getBatch0IndicesTotalBufferOffset(), 
					unit_quad_meshdata->vbo_handle.base_vertex); 
			}

			if(query_profiling_enabled && blur_ssao_gpu_timer->isRunning())
				blur_ssao_gpu_timer->endTimerQuery();


			// unbind textures that aren't used by standard materials.
			unbindTextureFromTextureUnit(*prepass_colour_copy_texture, PREPASS_COLOUR_COPY_TEXTURE_UNIT_INDEX);

			// restore bindings
			bindTextureToTextureUnit(*blurred_ssao_texture, /*texture_unit_index=*/SSAO_TEXTURE_UNIT_INDEX);

			flushDrawCommandsAndUnbindPrograms();
		}

		glEnable(GL_DEPTH_TEST); // Restore depth testing
		glDepthMask(GL_TRUE); // Restore
		glViewport(0, 0, viewport_w, viewport_h); // Restore viewport
	}
}


void OpenGLEngine::drawAlwaysVisibleObjects(const Matrix4f& view_matrix, const Matrix4f& proj_matrix)
{
	// These are objects like the move/rotate arrows, that should be visible even when behind other objects.
	// The drawing strategy for these will be:
	// Draw once without depth testing, and without depth writes, but with blending, so they are always partially visible.
	// Then draw again with depth testing, so they look proper when not occluded by another object.

	assertCurrentProgramIsZero();
	DebugGroup debug_group("drawAlwaysVisibleObjects()");
	TracyGpuZone("drawAlwaysVisibleObjects");

	if(!current_scene->always_visible_objects.empty())
	{
		ZoneScopedN("Draw always visible"); // Tracy profiler

		if(current_scene->render_to_main_render_framebuffer)
		{
			main_render_framebuffer->bindForDrawing();
			assert(main_render_framebuffer->getAttachedRenderBufferName(GL_COLOR_ATTACHMENT0) == main_colour_renderbuffer->buffer_name); // Check main colour renderbuffer is attached at GL_COLOR_ATTACHMENT0.			
			setSingleDrawBuffer(GL_COLOR_ATTACHMENT0); // Just write to colour buffer
		}
		else
		{
			if(this->target_frame_buffer)
			{
				this->target_frame_buffer->bindForDrawing();
				setSingleDrawBuffer(GL_COLOR_ATTACHMENT0); // Just draw to colour buffer, not normal buffer.
			}
			else
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // Bind to default frame buffer and use the draw buffer already set already for it.
		}


		glDisable(GL_DEPTH_TEST); // Turn off depth testing
		glDepthMask(GL_FALSE); // Disable writing to depth buffer.

		glEnable(GL_BLEND);
		glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
		glBlendColor(1, 1, 1, 0.5f);
		

		glEnable(GL_CULL_FACE); // We will cull backfaces, since we are not doing depth testing, to make it looks slightly less weird.
		glCullFace(GL_BACK);

		for(auto it = current_scene->always_visible_objects.begin(); it != current_scene->always_visible_objects.end(); ++it)
		{
			const GLObject* const ob = it->getPointer();
			if(AABBIntersectsFrustum(current_scene->frustum_clip_planes, current_scene->num_frustum_clip_planes, current_scene->frustum_aabb, ob->aabb_ws))
			{
				const OpenGLMeshRenderData& mesh_data = *ob->mesh_data;
				bindMeshData(*ob); // Bind the mesh data, which is the same for all batches.
				for(uint32 z = 0; z < mesh_data.batches.size(); ++z)
				{
					const uint32 mat_index = mesh_data.batches[z].material_index;
					const bool program_changed = checkUseProgram(ob->materials[mat_index].shader_prog.ptr()); // TODO: use sorting for these draws
					if(program_changed)
						setSharedUniformsForProg(*ob->materials[mat_index].shader_prog, view_matrix, proj_matrix);
					drawBatch(*ob, ob->materials[mat_index], *ob->materials[mat_index].shader_prog, mesh_data, mesh_data.batches[z], /*batch_index=*/z); // Draw primitives for the given material
				}
			}
		}

		flushDrawCommandsAndUnbindPrograms();

		glDisable(GL_CULL_FACE);

		glDepthMask(GL_TRUE); // Re-enable writing to depth buffer.
		glDisable(GL_BLEND);

		glEnable(GL_DEPTH_TEST); // Restore depth testing

		// Re-draw objects over the top where not occluded.
		for(auto it = current_scene->always_visible_objects.begin(); it != current_scene->always_visible_objects.end(); ++it)
		{
			const GLObject* const ob = it->getPointer();
			if(AABBIntersectsFrustum(current_scene->frustum_clip_planes, current_scene->num_frustum_clip_planes, current_scene->frustum_aabb, ob->aabb_ws))
			{
				const OpenGLMeshRenderData& mesh_data = *ob->mesh_data;
				bindMeshData(*ob); // Bind the mesh data, which is the same for all batches.
				for(uint32 z = 0; z < mesh_data.batches.size(); ++z)
				{
					const uint32 mat_index = mesh_data.batches[z].material_index;
					const bool program_changed = checkUseProgram(ob->materials[mat_index].shader_prog.ptr()); // TODO: use sorting for these draws
					if(program_changed)
						setSharedUniformsForProg(*ob->materials[mat_index].shader_prog, view_matrix, proj_matrix);
					drawBatch(*ob, ob->materials[mat_index], *ob->materials[mat_index].shader_prog, mesh_data, mesh_data.batches[z], /*batch_index=*/z); // Draw primitives for the given material
				}
			}
		}

		flushDrawCommandsAndUnbindPrograms();
	}
}


// To Draw outlines around selected object(s):
// * Draw object with flat uniform shading to a render frame buffer.
// * Make render buffer a texture
// * Draw a quad over the main viewport, using a shader that does a Sobel filter, to detect the edges.  write to another frame buffer.
// * Draw another quad using the edge texture, blending a green colour into the final frame buffer on the edge.
void OpenGLEngine::generateOutlineTexture(const Matrix4f& view_matrix, const Matrix4f& proj_matrix)
{
	if(!selected_objects.empty() && current_scene->render_to_main_render_framebuffer) // Only do when render_to_main_render_framebuffer is true, to avoid constantly reallocing outline textures as viewport size changes.
	{
		assertCurrentProgramIsZero();
		DebugGroup debug_group("generateOutlineTexture()");
	
		// Make outline textures if they have not been created, or are the wrong size.
		if(outline_tex_w != myMax<size_t>(16, viewport_w) || outline_tex_h != myMax<size_t>(16, viewport_h))
		{
			buildOutlineTextures();
		}

		setSingleDrawBuffer(GL_COLOR_ATTACHMENT0); // Just draw to colour buffer (not normal buffer)

		// -------------------------- Stage 1: draw flat selected objects. --------------------
		outline_solid_framebuffer->bindForDrawing();
		glViewport(0, 0, (GLsizei)outline_tex_w, (GLsizei)outline_tex_h); // Make viewport same size as texture.
		glClearColor(0.f, 0.f, 0.f, 1.f);
		glClearDepthf(use_reverse_z ? 0.0f : 1.f); // For reversed-z, the 'far' z value is 0, instead of 1.
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		for(auto i = selected_objects.begin(); i != selected_objects.end(); ++i)
		{
			const GLObject* const ob = *i;
			if(AABBIntersectsFrustum(current_scene->frustum_clip_planes, current_scene->num_frustum_clip_planes, current_scene->frustum_aabb, ob->aabb_ws))
			{
				const OpenGLMeshRenderData& mesh_data = *ob->mesh_data;

				const OpenGLProgram* use_prog = mesh_data.usesSkinning() ? outline_prog_with_skinning.ptr() : outline_prog_no_skinning.ptr();

				const bool program_changed = checkUseProgram(use_prog);
				if(program_changed)
					setSharedUniformsForProg(*use_prog, view_matrix, proj_matrix);
				bindMeshData(*ob); // Bind the mesh data, which is the same for all batches.
				for(uint32 z = 0; z < mesh_data.batches.size(); ++z)
					drawBatch(*ob, outline_solid_mat, *use_prog, mesh_data, mesh_data.batches[z], /*batch_index=*/z); // Draw object with outline_mat.
			}
		}

		// ------------------- Stage 2: Extract edges with Sobel filter---------------------
		// Shader reads from outline_solid_tex, writes to outline_edge_tex.
		outline_edge_framebuffer->bindForDrawing();
		glDepthMask(GL_FALSE); // Don't write to z-buffer, depth not needed.

		checkUseProgram(edge_extract_prog.ptr());

		// Position quad to cover viewport
		const Matrix4f ob_to_world_matrix = Matrix4f::scaleMatrix(2.f, 2.f, 1.f) * Matrix4f::translationMatrix(Vec4f(-0.5f, -0.5f, 0, 0));

		bindMeshData(*outline_quad_meshdata);
		assert(outline_quad_meshdata->batches.size() == 1);

		{
			bindTextureUnitToSampler(*outline_solid_tex, /*texture_unit_index=*/0, /*sampler_uniform_location=*/edge_extract_tex_location);

			glUniformMatrix4fv(edge_extract_prog->model_matrix_loc, 1, false, ob_to_world_matrix.e);
			glUniform4fv(edge_extract_col_location, 1, outline_colour.x);
			glUniform1f(edge_extract_line_width_location, this->outline_width_px);
				
			drawElementsBaseVertex(GL_TRIANGLES, (GLsizei)outline_quad_meshdata->batches[0].num_indices, outline_quad_meshdata->getIndexType(), (void*)outline_quad_meshdata->getBatch0IndicesTotalBufferOffset(), 
				outline_quad_meshdata->vbo_handle.base_vertex);

			unbindTextureFromTextureUnit(*outline_solid_tex, /*texture_unit_index=*/0);
		}

		OpenGLProgram::useNoPrograms();

		glDepthMask(GL_TRUE); // Restore writing to z-buffer.

		glViewport(0, 0, viewport_w, viewport_h); // Restore viewport
	}
}


void OpenGLEngine::drawOutlinesAroundSelectedObjects()
{
	// At this stage the outline texture has been generated in outline_edge_tex.  So we will just blend it over the current frame.
	if(!selected_objects.empty() && current_scene->render_to_main_render_framebuffer)
	{
		assertCurrentProgramIsZero();
		DebugGroup debug_group("drawOutlinesAroundSelectedObjects()");

		main_render_framebuffer->bindForDrawing();
		assert(main_render_framebuffer->getAttachedRenderBufferName(GL_COLOR_ATTACHMENT0) == this->main_colour_renderbuffer->buffer_name);
		setSingleDrawBuffer(GL_COLOR_ATTACHMENT0); // Just draw to colour buffer (not normal buffer)

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE); // Don't write to z-buffer
		glDisable(GL_DEPTH_TEST); // Disable depth testing

		// Position quad to cover viewport
		const float use_z = 0.5f; // Use a z value that will be in the clip volume for both default and reverse z.
		const Matrix4f ob_to_world_matrix = Matrix4f::scaleMatrix(2.f, 2.f, 1.f) * Matrix4f::translationMatrix(Vec4f(-0.5, -0.5, use_z, 0));

		const OpenGLMeshRenderData& mesh_data = *outline_quad_meshdata;
		bindMeshData(mesh_data); // Bind the mesh data, which is the same for all batches.
		for(uint32 z = 0; z < mesh_data.batches.size(); ++z)
		{
			const OpenGLMaterial& opengl_mat = outline_edge_mat;

			assert(opengl_mat.shader_prog.getPointer() == this->overlay_prog.getPointer());

			checkUseProgram(opengl_mat.shader_prog.ptr());

			glUniform4f(overlay_diffuse_colour_location, 1, 1, 1, 1);
			glUniformMatrix4fv(opengl_mat.shader_prog->model_matrix_loc, 1, false, /*ob->*/ob_to_world_matrix.e);
			const int overlay_flags = 
				(opengl_mat.albedo_texture              ? OVERLAY_HAVE_TEXTURE_FLAG        : 0) |
				(opengl_mat.overlay_target_is_nonlinear ? OVERLAY_TARGET_IS_NONLINEAR_FLAG : 0) |
				(opengl_mat.overlay_show_just_tex_rgb   ? OVERLAY_SHOW_JUST_TEX_RGB_FLAG   : 0) |
				(opengl_mat.overlay_show_just_tex_w     ? OVERLAY_SHOW_JUST_TEX_W_FLAG     : 0);
			glUniform1i(this->overlay_flags_location, overlay_flags);

			const Matrix3f identity = Matrix3f::identity();
			glUniformMatrix3fv(overlay_texture_matrix_location, /*count=*/1, /*transpose=*/false, identity.e);

			bindTextureUnitToSampler(*opengl_mat.albedo_texture, /*texture_unit_index=*/0, /*sampler_uniform_location=*/overlay_diffuse_tex_location);
				
			drawElementsBaseVertex(GL_TRIANGLES, (GLsizei)mesh_data.batches[0].num_indices, mesh_data.getIndexType(), (void*)mesh_data.getBatch0IndicesTotalBufferOffset(), mesh_data.vbo_handle.base_vertex);

			unbindTextureFromTextureUnit(*opengl_mat.albedo_texture, /*texture_unit_index=*/0);
		}

		flushDrawCommandsAndUnbindPrograms();

		glEnable(GL_DEPTH_TEST); // Restore depth testing
		glDepthMask(GL_TRUE); // Restore
		glDisable(GL_BLEND);
	}
}


void OpenGLEngine::drawUIOverlayObjects(const Matrix4f& reverse_z_matrix)
{
	assertCurrentProgramIsZero();
	DebugGroup debug_group("drawUIOverlayObjects()");
	TracyGpuZone("drawUIOverlayObjects");

	if(query_profiling_enabled && current_scene->collect_stats && time_individual_passes && draw_overlays_gpu_timer->isIdle())
		draw_overlays_gpu_timer->beginTimerQuery();

	// Bind requested target frame buffer as output buffer
	if(this->target_frame_buffer)
	{
		this->target_frame_buffer->bindForDrawing();
		setSingleDrawBuffer(GL_COLOR_ATTACHMENT0); // Just draw to colour buffer, not normal buffer.
	}
	else
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // Bind to default frame buffer and use the draw buffer already set already for it.

	glEnable(GL_BLEND);

	// For web rendering, where we have a framebuffer with alpha, and the OpenGL canvas is composited over another element, the standard glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// doesn't work well because you end up with a framebuffer with alpha < 1 where the UI is rendered over an opaque element, whereas we want alpha to be 1 in that case.
	// So use glBlendFuncSeparate

	//glBlendFunc(/*source (incoming) factor=*/GL_SRC_ALPHA, /*destination factor=*/GL_ONE_MINUS_SRC_ALPHA);
	glBlendFuncSeparate(/*source (incoming) RGB factor=*/GL_SRC_ALPHA, /*destination RGB factor=*/GL_ONE_MINUS_SRC_ALPHA, /*src alpha factor=*/GL_ONE, /*dest alpha factor=*/GL_ONE);

	// Instead of using the depth buffer, use the Painter's algorithm - draw objects from far to near.
	glDisable(GL_DEPTH_TEST); // Disable depth testing
	glDepthMask(GL_FALSE); // Don't write to depth buffer

	// Sort overlay objects into descending z order, then we draw from back to front (descending z order).
	// Note that z=-1 is the near clip plane, z=1 is the far clip plane
	temp_obs.resizeNoCopy(current_scene->overlay_objects.size());
	size_t q = 0;
	for(auto it = current_scene->overlay_objects.begin(); it != current_scene->overlay_objects.end(); ++it)
	{
		const OverlayObject* ob = it->ptr();
		if(ob->draw) // Only sort objects we are actually drawing.
			temp_obs[q++] = ob;
	}
	temp_obs.resize(q);
	std::sort(temp_obs.begin(), temp_obs.end(), OverlayObjectZComparator());

	/*for(size_t i=0; i<temp_obs.size(); ++i)
	{
		conPrint("overlay temp_obs[" + toString(i) + "]: z: " + toString(temp_obs[i]->ob_to_world_matrix.e[14]));
		assert(isFinite(temp_obs[i]->ob_to_world_matrix.e[14]));
	}*/

	for(size_t i=0; i<temp_obs.size(); ++i)
	{
		const OverlayObject* const ob = temp_obs[i];
		const OpenGLMeshRenderData& mesh_data = *ob->mesh_data;
		bindMeshData(mesh_data); // Bind the mesh data, which is the same for all batches.
		for(uint32 z = 0; z < mesh_data.batches.size(); ++z)
		{
			const OpenGLMaterial& opengl_mat = ob->material;

			assert(opengl_mat.shader_prog.getPointer() == this->overlay_prog.getPointer());

			checkUseProgram(opengl_mat.shader_prog.ptr());

			glUniform4f(overlay_diffuse_colour_location, opengl_mat.albedo_linear_rgb.r, opengl_mat.albedo_linear_rgb.g, opengl_mat.albedo_linear_rgb.b, opengl_mat.alpha);

			const Matrix4f ob_to_cam = reverse_z_matrix * current_scene->overlay_world_to_camera_space_matrix * ob->ob_to_world_matrix;

			glUniformMatrix4fv(opengl_mat.shader_prog->model_matrix_loc, 1, false, ob_to_cam.e);
			const int overlay_flags = 
				(opengl_mat.albedo_texture              ? OVERLAY_HAVE_TEXTURE_FLAG        : 0) |
				(opengl_mat.overlay_target_is_nonlinear ? OVERLAY_TARGET_IS_NONLINEAR_FLAG : 0) |
				(opengl_mat.overlay_show_just_tex_rgb   ? OVERLAY_SHOW_JUST_TEX_RGB_FLAG   : 0) |
				(opengl_mat.overlay_show_just_tex_w     ? OVERLAY_SHOW_JUST_TEX_W_FLAG     : 0);
			glUniform1i(this->overlay_flags_location, overlay_flags);

			glUniform2f(overlay_clip_min_coords_location, ob->clip_region.getMin().x, ob->clip_region.getMin().y);
			glUniform2f(overlay_clip_max_coords_location, ob->clip_region.getMax().x, ob->clip_region.getMax().y);

			if(opengl_mat.albedo_texture)
			{
				const GLfloat tex_elems[9] = {
					opengl_mat.tex_matrix.e[0], opengl_mat.tex_matrix.e[2], 0,
					opengl_mat.tex_matrix.e[1], opengl_mat.tex_matrix.e[3], 0,
					opengl_mat.tex_translation.x, opengl_mat.tex_translation.y, 1
				};
				glUniformMatrix3fv(overlay_texture_matrix_location, /*count=*/1, /*transpose=*/false, tex_elems);

				bindTextureUnitToSampler(*opengl_mat.albedo_texture, /*texture_unit_index=*/0, /*sampler_uniform_location=*/overlay_diffuse_tex_location);
			}
				
			drawElementsBaseVertex(GL_TRIANGLES, (GLsizei)mesh_data.batches[0].num_indices, mesh_data.getIndexType(), (void*)mesh_data.getBatch0IndicesTotalBufferOffset(), mesh_data.vbo_handle.base_vertex);

			if(opengl_mat.albedo_texture)
				unbindTextureFromTextureUnit(*opengl_mat.albedo_texture, /*texture_unit_index=*/0);
		}
	}

	flushDrawCommandsAndUnbindPrograms();

	glEnable(GL_DEPTH_TEST); // Restore depth testing
	glDepthMask(GL_TRUE); // Restore depth buffer writes
	glDisable(GL_BLEND);

	if(query_profiling_enabled && draw_overlays_gpu_timer->isRunning())
		draw_overlays_gpu_timer->endTimerQuery();
}


void OpenGLEngine::drawAuroraTex()
{
	DebugGroup debug_group("drawAuroraTex()");
	TracyGpuZone("drawAuroraTex");

	OpenGLProgram::useNoPrograms(); // Unbind any programs before we start changing framebuffer and z-buffer options.

	const int AURORA_TEX_W = 512;
	if(aurora_tex.isNull())
	{
		aurora_tex = new OpenGLTexture(AURORA_TEX_W, AURORA_TEX_W, /*opengl_engine=*/this,
			ArrayRef<uint8>(NULL, 0),
			OpenGLTextureFormat::Format_RGB_Linear_Uint8,
			OpenGLTexture::Filtering_Bilinear
		);

		bindTextureToTextureUnit(*this->aurora_tex, /*texture_unit_index=*/AURORA_TEXTURE_UNIT_INDEX);
	}

	if(aurora_tex_frame_buffer.isNull())
	{
		aurora_tex_frame_buffer = new FrameBuffer();
		aurora_tex_frame_buffer->attachTexture(*aurora_tex, GL_COLOR_ATTACHMENT0);

		GLenum is_complete = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if(is_complete != GL_FRAMEBUFFER_COMPLETE)
		{
			conPrint("Error: drawAuroraTex(): framebuffer is not complete.");
			assert(0);
		}

		// Add overlay quad showing texture, for debugging
		if(false)
		{
			OverlayObjectRef tex_preview_overlay_ob =  new OverlayObject();
			tex_preview_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(-1, -1, -0.999f) * Matrix4f::scaleMatrix(0.5f, 0.5f, 1);
			tex_preview_overlay_ob->material.albedo_linear_rgb = Colour3f(1.f);
			tex_preview_overlay_ob->material.albedo_texture = aurora_tex;
			tex_preview_overlay_ob->mesh_data = this->unit_quad_meshdata;
			addOverlayObject(tex_preview_overlay_ob);
		}
	}

	glDepthMask(GL_FALSE); // Don't write to z-buffer, depth not needed.
	glDisable(GL_DEPTH_TEST); // Don't depth test

	bindMeshData(*unit_quad_meshdata);
	assert(unit_quad_meshdata->batches.size() == 1);

	aurora_tex_frame_buffer->bindForDrawing();
		
	glViewport(0, 0, AURORA_TEX_W, AURORA_TEX_W); // Set viewport to target texture size

	draw_aurora_tex_prog->useProgram();

#if EMSCRIPTEN
	// There seems to be an Emscripten bug where sometimes the uniform value gets changed.  Just set it every frame as a workaround.
	glUniform1i(draw_aurora_tex_prog->uniform_locations.fbm_tex_location, FBM_TEXTURE_UNIT_INDEX);
#endif
	bindTextureToTextureUnit(*this->fbm_tex, FBM_TEXTURE_UNIT_INDEX);

	drawElementsBaseVertex(GL_TRIANGLES, (GLsizei)unit_quad_meshdata->batches[0].num_indices, unit_quad_meshdata->getIndexType(), (void*)unit_quad_meshdata->getBatch0IndicesTotalBufferOffset(), 
		unit_quad_meshdata->vbo_handle.base_vertex); // Draw quad


	OpenGLProgram::useNoPrograms();

	glEnable(GL_DEPTH_TEST); // Restore depth testing
	glDepthMask(GL_TRUE); // Restore depth buffer writes
}


Reference<OpenGLMeshRenderData> OpenGLEngine::getLineMeshData() { return line_meshdata; } // A line from (0, 0, 0) to (1, 0, 0)
Reference<OpenGLMeshRenderData> OpenGLEngine::getSphereMeshData() { return sphere_meshdata; }
Reference<OpenGLMeshRenderData> OpenGLEngine::getCubeMeshData() { return cube_meshdata; } // Bottom left corner will be at origin, opposite corner will lie at (1, 1, 1)
Reference<OpenGLMeshRenderData> OpenGLEngine::getUnitQuadMeshData() { return unit_quad_meshdata; } // A quad from (0, 0, 0) to (1, 1, 0)


Reference<OpenGLMeshRenderData> OpenGLEngine::getCylinderMesh() // A cylinder from (0,0,0), to (0,0,1) with radius 1;
{
	if(cylinder_meshdata.isNull())
		cylinder_meshdata = MeshPrimitiveBuilding::makeCylinderMesh(*vert_buf_allocator, /*end caps=*/true);
	return cylinder_meshdata;
}


Matrix4f OpenGLEngine::arrowObjectTransform(const Vec4f& startpos, const Vec4f& endpos, float radius_scale)
{
	// We want to map the vector (1,0,0) to endpos-startpos.
	// We want to map the point (0,0,0) to startpos.
	assert(startpos[3] == 1.f);
	assert(endpos[3] == 1.f);

	const Vec4f dir = endpos - startpos;
	const Vec4f col1 = normalise(std::fabs(normalise(dir)[2]) > 0.5f ? crossProduct(dir, Vec4f(1,0,0,0)) : crossProduct(dir, Vec4f(0,0,1,0))) * radius_scale;
	const Vec4f col2 = normalise(crossProduct(dir, col1)) * radius_scale;

	Matrix4f m;
	m.setColumn(0, dir);
	m.setColumn(1, col1);
	m.setColumn(2, col2);
	m.setColumn(3, startpos);
	return m;
}


GLObjectRef OpenGLEngine::makeArrowObject(const Vec4f& startpos, const Vec4f& endpos, const Colour4f& col, float radius_scale)
{
	GLObjectRef ob = allocateObject();

	ob->ob_to_world_matrix = arrowObjectTransform(startpos, endpos, radius_scale);

	if(arrow_meshdata.isNull())
		arrow_meshdata = MeshPrimitiveBuilding::make3DArrowMesh(*vert_buf_allocator);
	ob->mesh_data = arrow_meshdata;
	ob->materials.resize(1);
	ob->materials[0].albedo_linear_rgb = toLinearSRGB(Colour3f(col[0], col[1], col[2]));
	return ob;
}


Matrix4f OpenGLEngine::AABBObjectTransform(const Vec4f& min_os, const Vec4f& max_os)
{
	const Vec4f span = max_os - min_os;

	Matrix4f m;
	m.setColumn(0, Vec4f(span[0], 0, 0, 0));
	m.setColumn(1, Vec4f(0, span[1], 0, 0));
	m.setColumn(2, Vec4f(0, 0, span[2], 0));
	m.setColumn(3, min_os); // set origin

	return m;
}


GLObjectRef OpenGLEngine::makeAABBObject(const Vec4f& min_, const Vec4f& max_, const Colour4f& col)
{
	GLObjectRef ob = allocateObject();

	ob->ob_to_world_matrix = AABBObjectTransform(min_, max_);

	ob->mesh_data = cube_meshdata;
	ob->materials.resize(1);
	ob->materials[0].albedo_linear_rgb = toLinearSRGB(Colour3f(col[0], col[1], col[2]));
	ob->materials[0].alpha = col[3];
	ob->materials[0].transparent = col[3] < 1.f;
	return ob;
}


GLObjectRef OpenGLEngine::makeCuboidEdgeAABBObject(const Vec4f& min_, const Vec4f& max_, const Colour4f& col, float edge_width_scale)
{
	GLObjectRef ob = allocateObject();

	ob->ob_to_world_matrix = Matrix4f::translationMatrix(min_);

	const Vec4f span = max_ - min_;
	const float edge_width = myMin(1.f, span[0] * edge_width_scale);
	ob->mesh_data = MeshPrimitiveBuilding::makeCuboidEdgeAABBMesh(*vert_buf_allocator, span, edge_width);
	ob->materials.resize(1);
	ob->materials[0].albedo_linear_rgb = toLinearSRGB(Colour3f(col[0], col[1], col[2]));
	ob->materials[0].alpha = col[3];
	ob->materials[0].transparent = col[3] < 1.f;
	return ob;
}


GLObjectRef OpenGLEngine::makeSphereObject(float radius, const Colour4f& col)
{
	GLObjectRef ob = allocateObject();

	ob->ob_to_world_matrix = Matrix4f::uniformScaleMatrix(radius);

	ob->mesh_data = sphere_meshdata;
	ob->materials.resize(1);
	ob->materials[0].albedo_linear_rgb = toLinearSRGB(Colour3f(col[0], col[1], col[2]));
	ob->materials[0].alpha = col[3];
	ob->materials[0].transparent = col[3] < 1.f;
	ob->materials[0].simple_double_sided = true;
	return ob;
}


GLObjectRef OpenGLEngine::makeCylinderObject(float radius, const Colour4f& col)
{
	GLObjectRef ob = allocateObject();

	ob->ob_to_world_matrix = Matrix4f::uniformScaleMatrix(radius);

	ob->mesh_data = getCylinderMesh();
	ob->materials.resize(1);
	ob->materials[0].albedo_linear_rgb = toLinearSRGB(Colour3f(col[0], col[1], col[2]));
	ob->materials[0].alpha = col[3];
	ob->materials[0].transparent = col[3] < 1.f;
	return ob;
}


Reference<OpenGLMeshRenderData> OpenGLEngine::buildMeshRenderData(VertexBufferAllocator& allocator, const js::Vector<Vec3f, 16>& vertices, const js::Vector<Vec3f, 16>& normals, const js::Vector<Vec2f, 16>& uvs, const js::Vector<uint32, 16>& indices)
{
	return GLMeshBuilding::buildMeshRenderData(allocator, vertices, normals, uvs, indices);
}


std::string MeshDataLoadingProgress::summaryString() const
{ 
	return "vert data: " + toString(vert_next_i) + " / " + toString(vert_total_size_B) + " B, index data: " + toString(index_next_i) + "/" + toString(index_total_size_B) + " B";
}


void OpenGLEngine::initialiseMeshDataLoadingProgress(OpenGLMeshRenderData& data, MeshDataLoadingProgress& loading_progress)
{
	ZoneScoped; // Tracy profiler
	loading_progress.vert_next_i = 0;
	loading_progress.index_next_i = 0;

	if(data.batched_mesh.nonNull())
	{
		loading_progress.vert_total_size_B = data.batched_mesh->vertex_data.dataSizeBytes();
		loading_progress.index_total_size_B = data.batched_mesh->index_data.dataSizeBytes();
	}
	else
	{
		loading_progress.vert_total_size_B = data.vert_data.dataSizeBytes();

		if(!data.vert_index_buffer_uint8.empty())
			loading_progress.index_total_size_B = data.vert_index_buffer_uint8.dataSizeBytes();
		else if(!data.vert_index_buffer_uint16.empty())
			loading_progress.index_total_size_B = data.vert_index_buffer_uint16.dataSizeBytes();
		else
			loading_progress.index_total_size_B = data.vert_index_buffer.dataSizeBytes();
	}

	assert(loading_progress.vert_total_size_B > 0);
	assert(loading_progress.index_total_size_B > 0);
}


void OpenGLEngine::partialLoadOpenGLMeshDataIntoOpenGL(VertexBufferAllocator& allocator, OpenGLMeshRenderData& data, MeshDataLoadingProgress& loading_progress, size_t& total_bytes_uploaded_in_out, size_t max_total_upload_bytes)
{
	ZoneScoped; // Tracy profiler
	assert(!loading_progress.done());

	const size_t max_chunk_size = max_total_upload_bytes;

	if(!data.vbo_handle.valid())
		data.vbo_handle = allocator.allocateVertexDataSpace(data.vertex_spec.vertStride(), /*data=*/NULL, loading_progress.vert_total_size_B); // Just allocate the buffer, don't upload

	if(!data.indices_vbo_handle.valid()) // loading_progress.index_next_i == 0)
	{
		data.indices_vbo_handle = allocator.allocateIndexDataSpace(/*data=*/NULL, loading_progress.index_total_size_B); // Just allocate the buffer, don't upload

		allocator.getOrCreateAndAssignVAOForMesh(data, data.vertex_spec);
	}
		

	if(data.batched_mesh.nonNull())
	{
		assert(loading_progress.vert_total_size_B == data.batched_mesh->vertex_data.dataSizeBytes());
		assert(loading_progress.index_total_size_B == data.batched_mesh->index_data.dataSizeBytes());

		// Upload a chunk of vertex data, if not finished uploading it already.
		if(loading_progress.vert_next_i < loading_progress.vert_total_size_B)
		{
			const size_t chunk_size = myMin(max_chunk_size, loading_progress.vert_total_size_B - loading_progress.vert_next_i);
			data.vbo_handle.vbo->updateData(/*offset=*/data.vbo_handle.offset + loading_progress.vert_next_i, /*data=*/data.batched_mesh->vertex_data.data() + loading_progress.vert_next_i, 
				/*data_size=*/chunk_size);
			loading_progress.vert_next_i += chunk_size;
			total_bytes_uploaded_in_out += chunk_size;
		}
		else
		{
			// Upload a chunk of index data, if not finished uploading it already.
			if(loading_progress.index_next_i < loading_progress.index_total_size_B)
			{
				const size_t chunk_size = myMin(max_chunk_size, loading_progress.index_total_size_B - loading_progress.index_next_i);
				data.indices_vbo_handle.index_vbo->updateData(/*offset=*/data.indices_vbo_handle.offset + loading_progress.index_next_i, /*data=*/data.batched_mesh->index_data.data() + loading_progress.index_next_i, 
					/*data_size=*/chunk_size);
				loading_progress.index_next_i += chunk_size;
				total_bytes_uploaded_in_out += chunk_size;
			}
		}
	}
	else
	{
		// Upload a chunk of vertex data, if not finished uploading it already.
		if(loading_progress.vert_next_i < loading_progress.vert_total_size_B)
		{
			const size_t chunk_size = myMin(max_chunk_size, loading_progress.vert_total_size_B - loading_progress.vert_next_i);
			data.vbo_handle.vbo->updateData(/*offset=*/data.vbo_handle.offset + loading_progress.vert_next_i, data.vert_data.data() + loading_progress.vert_next_i, chunk_size);
			loading_progress.vert_next_i += chunk_size;
		}
		else
		{
			const uint8* src_data;
			if(!data.vert_index_buffer_uint8.empty())
			{
				src_data = data.vert_index_buffer_uint8.data();
			}
			else if(!data.vert_index_buffer_uint16.empty())
			{
				src_data = (const uint8*)data.vert_index_buffer_uint16.data();
			}
			else
			{
				src_data = (const uint8*)data.vert_index_buffer.data();
			}

			// Upload a chunk of index data, if not finished uploading it already.
			if(loading_progress.index_next_i < loading_progress.index_total_size_B)
			{
				const size_t chunk_size = myMin(max_chunk_size, loading_progress.index_total_size_B - loading_progress.index_next_i);
				data.indices_vbo_handle.index_vbo->updateData(/*offset=*/data.indices_vbo_handle.offset + loading_progress.index_next_i, src_data + loading_progress.index_next_i, chunk_size);
				loading_progress.index_next_i += chunk_size;
				total_bytes_uploaded_in_out += chunk_size;
			}
		}
	}

	if(loading_progress.done())
	{
		// Now that data has been uploaded, free the host buffers.
		data.vert_data.clearAndFreeMem();
		data.vert_index_buffer_uint8.clearAndFreeMem();
		data.vert_index_buffer_uint16.clearAndFreeMem();
		data.vert_index_buffer.clearAndFreeMem();

		data.batched_mesh = NULL;
	}
}


void OpenGLEngine::loadOpenGLMeshDataIntoOpenGL(VertexBufferAllocator& allocator, OpenGLMeshRenderData& data)
{
	// If we have a ref to a BatchedMesh, upload directly from it:
	if(data.batched_mesh.nonNull())
	{
		data.vbo_handle = allocator.allocateVertexDataSpace(data.vertex_spec.vertStride(), data.batched_mesh->vertex_data.data(), data.batched_mesh->vertex_data.dataSizeBytes());

		data.indices_vbo_handle = allocator.allocateIndexDataSpace(data.batched_mesh->index_data.data(), data.batched_mesh->index_data.dataSizeBytes());
	}
	else
	{
		data.vbo_handle = allocator.allocateVertexDataSpace(data.vertex_spec.vertStride(), data.vert_data.data(), data.vert_data.dataSizeBytes());

		if(!data.vert_index_buffer_uint8.empty())
		{
			data.indices_vbo_handle = allocator.allocateIndexDataSpace(data.vert_index_buffer_uint8.data(), data.vert_index_buffer_uint8.dataSizeBytes());
			assert(data.getIndexType() == GL_UNSIGNED_BYTE);
		}
		else if(!data.vert_index_buffer_uint16.empty())
		{
			data.indices_vbo_handle = allocator.allocateIndexDataSpace(data.vert_index_buffer_uint16.data(), data.vert_index_buffer_uint16.dataSizeBytes());
			assert(data.getIndexType() == GL_UNSIGNED_SHORT);
		}
		else
		{
			data.indices_vbo_handle = allocator.allocateIndexDataSpace(data.vert_index_buffer.data(), data.vert_index_buffer.dataSizeBytes());
			assert(data.getIndexType() == GL_UNSIGNED_INT);
		}
	}

	allocator.getOrCreateAndAssignVAOForMesh(data, data.vertex_spec);


	// Now that data has been uploaded, free the buffers.
	data.vert_data.clearAndFreeMem();
	data.vert_index_buffer_uint8.clearAndFreeMem();
	data.vert_index_buffer_uint16.clearAndFreeMem();
	data.vert_index_buffer.clearAndFreeMem();

	data.batched_mesh = NULL;
}


void OpenGLEngine::setStandardTextureUnitUniformsForProgram(const OpenGLProgram& program)
{
	program.useProgram(); // Bind program

	doSetStandardTextureUnitUniformsForBoundProgram(program);

	OpenGLProgram::useNoPrograms();
}


// Assumes program is bound.
void OpenGLEngine::doSetStandardTextureUnitUniformsForBoundProgram(const OpenGLProgram& program)
{
	glUniform1i(program.uniform_locations.diffuse_tex_location, DIFFUSE_TEXTURE_UNIT_INDEX);

	glUniform1i(program.uniform_locations.dynamic_depth_tex_location, DEPTH_TEX_TEXTURE_UNIT_INDEX);
	glUniform1i(program.uniform_locations.static_depth_tex_location, STATIC_DEPTH_TEX_TEXTURE_UNIT_INDEX);

	glUniform1i(program.uniform_locations.cosine_env_tex_location, COSINE_ENV_TEXTURE_UNIT_INDEX);
	glUniform1i(program.uniform_locations.specular_env_tex_location, SPECULAR_ENV_TEXTURE_UNIT_INDEX);
	glUniform1i(program.uniform_locations.blue_noise_tex_location, BLUE_NOISE_TEXTURE_UNIT_INDEX);
	glUniform1i(program.uniform_locations.fbm_tex_location, FBM_TEXTURE_UNIT_INDEX);


	glUniform1i(program.uniform_locations.lightmap_tex_location, LIGHTMAP_TEXTURE_UNIT_INDEX);
	glUniform1i(program.uniform_locations.metallic_roughness_tex_location, METALLIC_ROUGHNESS_TEXTURE_UNIT_INDEX);
	glUniform1i(program.uniform_locations.emission_tex_location, EMISSION_TEXTURE_UNIT_INDEX);
	glUniform1i(program.uniform_locations.backface_albedo_tex_location, BACKFACE_ALBEDO_TEXTURE_UNIT_INDEX);
	glUniform1i(program.uniform_locations.transmission_tex_location, TRANSMISSION_TEXTURE_UNIT_INDEX);
	glUniform1i(program.uniform_locations.normal_map_location, NORMAL_MAP_TEXTURE_UNIT_INDEX);
	glUniform1i(program.uniform_locations.combined_array_tex_location, COMBINED_ARRAY_TEXTURE_UNIT_INDEX);

	glUniform1i(program.uniform_locations.main_colour_texture_location, MAIN_COLOUR_COPY_TEXTURE_UNIT_INDEX);
	glUniform1i(program.uniform_locations.main_normal_texture_location, MAIN_NORMAL_COPY_TEXTURE_UNIT_INDEX);
	glUniform1i(program.uniform_locations.main_depth_texture_location, MAIN_DEPTH_COPY_TEXTURE_UNIT_INDEX);

	glUniform1i(program.uniform_locations.cirrus_tex_location, CIRRUS_TEX_TEXTURE_UNIT_INDEX);

	glUniform1i(program.uniform_locations.caustic_tex_a_location, CAUSTIC_A_TEXTURE_UNIT_INDEX);
	glUniform1i(program.uniform_locations.caustic_tex_b_location, CAUSTIC_B_TEXTURE_UNIT_INDEX);

	glUniform1i(program.uniform_locations.detail_tex_0_location, DETAIL_0_TEXTURE_UNIT_INDEX);
	glUniform1i(program.uniform_locations.detail_tex_1_location, DETAIL_1_TEXTURE_UNIT_INDEX);
	glUniform1i(program.uniform_locations.detail_tex_2_location, DETAIL_2_TEXTURE_UNIT_INDEX);
	glUniform1i(program.uniform_locations.detail_tex_3_location, DETAIL_3_TEXTURE_UNIT_INDEX);

	glUniform1i(program.uniform_locations.detail_heightmap_0_location, DETAIL_HEIGHTMAP_TEXTURE_UNIT_INDEX);
	
	glUniform1i(program.uniform_locations.aurora_tex_location, AURORA_TEXTURE_UNIT_INDEX);

	glUniform1i(program.uniform_locations.ssao_tex_location, SSAO_TEXTURE_UNIT_INDEX);
	glUniform1i(program.uniform_locations.ssao_specular_tex_location, SSAO_SPECULAR_TEXTURE_UNIT_INDEX);

	glUniform1i(program.uniform_locations.prepass_depth_tex_location, PREPASS_DEPTH_COPY_TEXTURE_UNIT_INDEX);
	glUniform1i(program.uniform_locations.prepass_normal_tex_location, PREPASS_NORMAL_COPY_TEXTURE_UNIT_INDEX);

	//glUniform1i(program.uniform_locations.snow_ice_normal_map_location, SNOW_ICE_NORMAL_MAP_TEXTURE_UNIT_INDEX);
}


void OpenGLEngine::bindStandardShadowMappingDepthTextures()
{
	if(shadow_mapping)
	{
		bindTextureToTextureUnit(*this->shadow_mapping->dynamic_depth_tex,                                            /*texture_unit_index=*/DEPTH_TEX_TEXTURE_UNIT_INDEX);
		bindTextureToTextureUnit(*this->shadow_mapping->static_depth_tex[this->shadow_mapping->cur_static_depth_tex], /*texture_unit_index=*/STATIC_DEPTH_TEX_TEXTURE_UNIT_INDEX);
	}
}


void OpenGLEngine::bindStandardTexturesToTextureUnits()
{
	bindStandardShadowMappingDepthTextures();

	if(cosine_env_tex)
	{
		bindTextureToTextureUnit(*this->cosine_env_tex,    /*texture_unit_index=*/COSINE_ENV_TEXTURE_UNIT_INDEX);
		bindTextureToTextureUnit(*this->specular_env_tex,  /*texture_unit_index=*/SPECULAR_ENV_TEXTURE_UNIT_INDEX);
	}
	bindTextureToTextureUnit(*this->blue_noise_tex,    /*texture_unit_index=*/BLUE_NOISE_TEXTURE_UNIT_INDEX);
	bindTextureToTextureUnit(*this->fbm_tex,           /*texture_unit_index=*/FBM_TEXTURE_UNIT_INDEX);


	if(main_colour_copy_texture)
	{
		bindTextureToTextureUnit(*main_colour_copy_texture, /*texture_unit_index=*/MAIN_COLOUR_COPY_TEXTURE_UNIT_INDEX);
		bindTextureToTextureUnit(*main_normal_copy_texture, /*texture_unit_index=*/MAIN_NORMAL_COPY_TEXTURE_UNIT_INDEX);
		bindTextureToTextureUnit(*main_depth_copy_texture,  /*texture_unit_index=*/MAIN_DEPTH_COPY_TEXTURE_UNIT_INDEX);
	}

	if(cirrus_tex)
		bindTextureToTextureUnit(*this->cirrus_tex, /*texture_unit_index=*/CIRRUS_TEX_TEXTURE_UNIT_INDEX);

	if(settings.render_water_caustics && !water_caustics_textures.empty())
	{
		const int current_caustic_index   = Maths::intMod((int)(this->current_time * 24.0f)    , (int)water_caustics_textures.size());
		const int current_caustic_index_1 = Maths::intMod((int)(this->current_time * 24.0f) + 1, (int)water_caustics_textures.size());

		if(water_caustics_textures[current_caustic_index  ].nonNull())
			bindTextureToTextureUnit(*water_caustics_textures[current_caustic_index  ], /*texture_unit_index=*/CAUSTIC_A_TEXTURE_UNIT_INDEX);
		if(water_caustics_textures[current_caustic_index_1  ].nonNull())
			bindTextureToTextureUnit(*water_caustics_textures[current_caustic_index_1], /*texture_unit_index=*/CAUSTIC_B_TEXTURE_UNIT_INDEX);
	}


	//if(this->detail_tex[0])
	//	bindTextureToTextureUnit(*this->detail_tex[0], /*texture_unit_index=*/DETAIL_0_TEXTURE_UNIT_INDEX); // Not used in fragment shader currently
	if(this->detail_tex[1])
		bindTextureToTextureUnit(*this->detail_tex[1], /*texture_unit_index=*/DETAIL_1_TEXTURE_UNIT_INDEX);
	if(this->detail_tex[2])
		bindTextureToTextureUnit(*this->detail_tex[2], /*texture_unit_index=*/DETAIL_2_TEXTURE_UNIT_INDEX);
	//if(this->detail_tex[3])
	//	bindTextureToTextureUnit(*this->detail_tex[3], /*texture_unit_index=*/DETAIL_3_TEXTURE_UNIT_INDEX); // Not used in fragment shader currently
	
	// NOTE: for now we will only use 1 detail heightmap (rock) in shader
	//if(this->detail_heightmap[0])
	//	bindTextureToTextureUnit(*this->detail_heightmap[0], /*texture_unit_index=*/DETAIL_HEIGHTMAP_TEXTURE_UNIT_INDEX); // Not used in fragment shader currently

	bindTextureToTextureUnit(aurora_tex ? *aurora_tex : *dummy_black_tex, /*texture_unit_index=*/AURORA_TEXTURE_UNIT_INDEX);

	// 15 textures bound before here
	
	bindTextureToTextureUnit(blurred_ssao_texture ? *blurred_ssao_texture : *dummy_black_tex, /*texture_unit_index=*/SSAO_TEXTURE_UNIT_INDEX);
	bindTextureToTextureUnit(blurred_ssao_specular_texture ? *blurred_ssao_specular_texture : *dummy_black_tex, /*texture_unit_index=*/SSAO_SPECULAR_TEXTURE_UNIT_INDEX);

	//if(prepass_colour_copy_texture)
	//	bindTextureToTextureUnit(*prepass_colour_copy_texture, PREPASS_COLOUR_COPY_TEXTURE_UNIT_INDEX);
	if(prepass_normal_copy_texture)
		bindTextureToTextureUnit(*prepass_normal_copy_texture, PREPASS_NORMAL_COPY_TEXTURE_UNIT_INDEX);
	if(prepass_depth_copy_texture)
		bindTextureToTextureUnit(*prepass_depth_copy_texture, PREPASS_DEPTH_COPY_TEXTURE_UNIT_INDEX);

	//if(snow_ice_normal_map.nonNull())
	//	bindTextureToTextureUnit(*snow_ice_normal_map, /*texture_unit_index=*/SNOW_ICE_NORMAL_MAP_TEXTURE_UNIT_INDEX);

	//const int num_bound = getNumBoundTextures(/*max_texture_unit_index=*/PREPASS_DEPTH_COPY_TEXTURE_UNIT_INDEX);
	//printVar(num_bound);
}



void OpenGLEngine::setUniformsForPhongProg(const OpenGLMaterial& opengl_mat, const OpenGLMeshRenderData& mesh_data, PhongUniforms& uniforms) const
{
	ZoneScoped; // Tracy profiler

	uniforms.diffuse_colour      = Colour4f(opengl_mat.albedo_linear_rgb.r, opengl_mat.albedo_linear_rgb.g, opengl_mat.albedo_linear_rgb.b, opengl_mat.alpha);
	uniforms.transmission_colour = opengl_mat.transmission_albedo_linear_rgb;
	uniforms.emission_colour     = opengl_mat.emission_linear_rgb * opengl_mat.emission_scale;

	uniforms.texture_upper_left_matrix_col0.x = opengl_mat.tex_matrix.e[0];
	uniforms.texture_upper_left_matrix_col0.y = opengl_mat.tex_matrix.e[2];
	uniforms.texture_upper_left_matrix_col1.x = opengl_mat.tex_matrix.e[1];
	uniforms.texture_upper_left_matrix_col1.y = opengl_mat.tex_matrix.e[3];
	uniforms.texture_matrix_translation = opengl_mat.tex_translation;

	if(this->use_bindless_textures)
	{
		uniforms.diffuse_tex            = opengl_mat.albedo_texture             ? opengl_mat.albedo_texture            ->getBindlessTextureHandle() : 0;
		uniforms.metallic_roughness_tex = opengl_mat.metallic_roughness_texture ? opengl_mat.metallic_roughness_texture->getBindlessTextureHandle() : 0;
		uniforms.lightmap_tex           = opengl_mat.lightmap_texture           ? opengl_mat.lightmap_texture          ->getBindlessTextureHandle() : 0;
		uniforms.emission_tex           = opengl_mat.emission_texture           ? opengl_mat.emission_texture          ->getBindlessTextureHandle() : 0;
		uniforms.backface_albedo_tex    = opengl_mat.backface_albedo_texture    ? opengl_mat.backface_albedo_texture   ->getBindlessTextureHandle() : 0;
		uniforms.transmission_tex       = opengl_mat.transmission_texture       ? opengl_mat.transmission_texture      ->getBindlessTextureHandle() : 0;
		uniforms.normal_map             = opengl_mat.normal_map                 ? opengl_mat.normal_map                ->getBindlessTextureHandle() : 0;
		uniforms.combined_array_tex     = opengl_mat.combined_array_texture     ? opengl_mat.combined_array_texture    ->getBindlessTextureHandle() : 0;
	}

	
	assert(opengl_mat.uniform_flags == computeUniformFlagsForMat(opengl_mat, mesh_data)); // Check our cached flags are correct
	uniforms.flags						= opengl_mat.uniform_flags;

	uniforms.roughness					= opengl_mat.roughness;
	uniforms.fresnel_scale				= opengl_mat.fresnel_scale;
	uniforms.metallic_frac				= opengl_mat.metallic_frac;
	uniforms.begin_fade_out_distance	= opengl_mat.begin_fade_out_distance;
	uniforms.end_fade_out_distance		= opengl_mat.end_fade_out_distance;
	uniforms.materialise_lower_z		= opengl_mat.materialise_lower_z;
	uniforms.materialise_upper_z		= opengl_mat.materialise_upper_z;
	uniforms.materialise_start_time		= opengl_mat.materialise_start_time;
	uniforms.dopacity_dt				= opengl_mat.dopacity_dt;
}


void OpenGLEngine::bindTexturesForPhongProg(const OpenGLMaterial& opengl_mat) const
{
	assert(!this->use_bindless_textures);

	if(opengl_mat.albedo_texture.nonNull())
		bindTextureToTextureUnitRaw(*opengl_mat.albedo_texture, DIFFUSE_TEXTURE_UNIT_INDEX);

	if(opengl_mat.lightmap_texture.nonNull())
		bindTextureToTextureUnitRaw(*opengl_mat.lightmap_texture, LIGHTMAP_TEXTURE_UNIT_INDEX);

	if(opengl_mat.backface_albedo_texture.nonNull())
		bindTextureToTextureUnitRaw(*opengl_mat.backface_albedo_texture, BACKFACE_ALBEDO_TEXTURE_UNIT_INDEX);

	if(opengl_mat.transmission_texture.nonNull())
		bindTextureToTextureUnitRaw(*opengl_mat.transmission_texture, TRANSMISSION_TEXTURE_UNIT_INDEX);

	if(opengl_mat.metallic_roughness_texture.nonNull())
		bindTextureToTextureUnitRaw(*opengl_mat.metallic_roughness_texture, METALLIC_ROUGHNESS_TEXTURE_UNIT_INDEX);

	if(opengl_mat.emission_texture.nonNull())
		bindTextureToTextureUnitRaw(*opengl_mat.emission_texture, EMISSION_TEXTURE_UNIT_INDEX);

	if(opengl_mat.normal_map.nonNull())
		bindTextureToTextureUnitRaw(*opengl_mat.normal_map, NORMAL_MAP_TEXTURE_UNIT_INDEX);

	if(opengl_mat.combined_array_texture.nonNull())
		bindTextureToTextureUnitRaw(*opengl_mat.combined_array_texture, COMBINED_ARRAY_TEXTURE_UNIT_INDEX);
}


#define doCheck(v) assert(v)
// Something like assert() that we can use in non-debug mode when needed.
/*#define doCheck(v) _doCheck((v), (#v))
static void _doCheck(bool b, const char* message)
{
	if(!b)
		conPrint("!!!!!!!!!!!!!!!!! ERROR: " + std::string(message));
	assert(b);
}*/


// Set uniforms that are the same for every triangle batch this frame.
void OpenGLEngine::setSharedUniformsForProg(const OpenGLProgram& shader_prog, const Matrix4f& view_mat, const Matrix4f& proj_mat)
{
	ZoneScoped; // Tracy profiler

	if(shader_prog.view_matrix_loc >= 0)
	{
		glUniformMatrix4fv(shader_prog.view_matrix_loc, 1, false, view_mat.e);
		glUniformMatrix4fv(shader_prog.proj_matrix_loc, 1, false, proj_mat.e);
	}

	if(shader_prog.uses_phong_uniforms)
	{
		// Set blob shadows location data
		glUniform1i(shader_prog.uniform_locations.num_blob_positions_location, settings.ssao ? 0 : (int)current_scene->blob_shadow_locations.size()); // When SSAO is enabled, don't do blob shadows.

		if(current_scene->blob_shadow_locations.size() > 0) // Avoid call with count=0.  (this causes an error in Emscripten, and is unneeded)
			glUniform4fv(shader_prog.uniform_locations.blob_positions_location, (int)current_scene->blob_shadow_locations.size(), (const float*)current_scene->blob_shadow_locations.data());
	}

	// There seems to be an Emscripten bug where sometimes the uniform values gets changed.  Just set it every frame as a workaround.
#if EMSCRIPTEN
	doSetStandardTextureUnitUniformsForBoundProgram(shader_prog);
#endif

	// Check standard textures are correctly bound to standard texture units
	if(shadow_mapping.nonNull())
	{
		assert(getBoundTexture2D(DEPTH_TEX_TEXTURE_UNIT_INDEX)        == this->shadow_mapping->dynamic_depth_tex->texture_handle);
		assert(getBoundTexture2D(STATIC_DEPTH_TEX_TEXTURE_UNIT_INDEX) == this->shadow_mapping->static_depth_tex[this->shadow_mapping->cur_static_depth_tex]->texture_handle);
	}

	if(main_colour_copy_texture.nonNull())
	{
		assert(getBoundTexture2D(MAIN_COLOUR_COPY_TEXTURE_UNIT_INDEX) == main_colour_copy_texture->texture_handle);
		assert(getBoundTexture2D(MAIN_NORMAL_COPY_TEXTURE_UNIT_INDEX) == main_normal_copy_texture->texture_handle);
		assert(getBoundTexture2D(MAIN_DEPTH_COPY_TEXTURE_UNIT_INDEX)  == main_depth_copy_texture->texture_handle);
	}

	if(cirrus_tex.nonNull())
		assert(getBoundTexture2D(CIRRUS_TEX_TEXTURE_UNIT_INDEX) == cirrus_tex->texture_handle);
	if(aurora_tex.nonNull())
		assert(getBoundTexture2D(AURORA_TEXTURE_UNIT_INDEX)     == aurora_tex->texture_handle);
	assert(getBoundTexture2D(BLUE_NOISE_TEXTURE_UNIT_INDEX) == blue_noise_tex->texture_handle);
	assert(getBoundTexture2D(FBM_TEXTURE_UNIT_INDEX)        == fbm_tex->texture_handle);

	if(blurred_ssao_texture)
		assert(getBoundTexture2D(SSAO_TEXTURE_UNIT_INDEX) == blurred_ssao_texture->texture_handle);

	if(blurred_ssao_specular_texture)
		assert(getBoundTexture2D(SSAO_SPECULAR_TEXTURE_UNIT_INDEX) == blurred_ssao_specular_texture->texture_handle);
}


void OpenGLEngine::drawBatch(const GLObject& ob, const OpenGLMaterial& opengl_mat, const OpenGLProgram& shader_prog_, const OpenGLMeshRenderData& mesh_data, const OpenGLBatch& batch, uint32 batch_index)
{
	const uint32 prim_start_offset_B = batch.prim_start_offset_B;
	const uint32 num_indices = batch.num_indices;

	if(num_indices == 0)
		return;

	const OpenGLProgram* shader_prog = &shader_prog_;

	// Set uniforms.  NOTE: Setting the uniforms manually in this way (switching on shader program) is obviously quite hacky.  Improve.

	// Set per-object vert uniforms.  Only do this if the current object has changed.
	if(&ob != current_uniforms_ob)
	{
		if(use_ob_and_mat_data_gpu_resident && shader_prog->supports_gpu_resident)
		{
			
		}
		else
		{
			if(shader_prog->uses_vert_uniform_buf_obs)
			{
				PerObjectVertUniforms uniforms;
				uniforms.model_matrix = ob.ob_to_world_matrix;
				uniforms.normal_matrix = ob.ob_to_world_normal_matrix;
				for(int i=0; i<GLObject::MAX_NUM_LIGHT_INDICES; ++i)
					uniforms.light_indices[i] = ob.light_indices[i];
				uniforms.depth_draw_depth_bias = ob.depth_draw_depth_bias;
				uniforms.model_matrix_upper_left_det = ob.ob_to_world_matrix_determinant;
				uniforms.uv0_scale = ob.mesh_data->uv0_scale;
				uniforms.uv1_scale = ob.mesh_data->uv1_scale;
				uniforms.dequantise_scale = ob.dequantise_scale;
				uniforms.dequantise_translation = ob.dequantise_translation;
				this->per_object_vert_uniform_buf_ob->updateData(/*dest offset=*/0, &uniforms, sizeof(PerObjectVertUniforms));
			}
			else
			{
				glUniformMatrix4fv(shader_prog->model_matrix_loc, 1, false, ob.ob_to_world_matrix.e);
				glUniformMatrix4fv(shader_prog->normal_matrix_loc, 1, false, ob.ob_to_world_normal_matrix.e);
			}

			if(shader_prog->uses_skinning)
			{
				assert(mesh_data.usesSkinning());
				assert(ob.joint_matrices.size() > 0);

				// The joint_matrix uniform array has 256 elems, don't upload more than that.
				const size_t num_joint_matrices_to_upload = myMin<size_t>(max_num_joint_matrices_per_ob, ob.joint_matrices.size()); 
				this->joint_matrices_buf_ob->updateData(/*dest offset=*/0, /*src data=*/ob.joint_matrices[0].e, /*src size=*/num_joint_matrices_to_upload * sizeof(Matrix4f));
			}
		}

		current_uniforms_ob = &ob;
	}

	if(use_ob_and_mat_data_gpu_resident && shader_prog->supports_gpu_resident)
	{
		// Nothing to do here if using MDI

		if(!use_multi_draw_indirect)
		{
			ObJointAndMatIndicesStruct indices;
			indices.per_ob_data_index = ob.per_ob_vert_data_index;
			indices.material_index = opengl_mat.material_data_index;
			indices.joints_base_index = ob.joint_matrices_base_index;
			this->ob_joint_and_mat_indices_uniform_buf_ob->updateData(/*dest offset=*/0, &indices, sizeof(ObJointAndMatIndicesStruct));
		}
	}
	else if(shader_prog->uses_phong_uniforms)
	{
#if UNIFORM_BUF_PER_MAT_SUPPORT

		glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/PHONG_UBO_BINDING_POINT_INDEX, opengl_mat.uniform_buf_ob->handle);

#else

		PhongUniforms uniforms;
		setUniformsForPhongProg(opengl_mat, mesh_data, uniforms);

	#if MULTIPLE_PHONG_UNIFORM_BUFS_SUPPORT
		const uint32 use_batch_index = settings.use_multiple_phong_uniform_bufs ? myMin(batch_index, 63u) : 0;

		this->phong_uniform_buf_obs[use_batch_index]->updateData(/*dest offset=*/0, &uniforms, sizeof(PhongUniforms));

		if(use_batch_index != current_bound_phong_uniform_buf_ob_index)
		{
			glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/PHONG_UBO_BINDING_POINT_INDEX, this->phong_uniform_buf_obs[use_batch_index]->handle);
			current_bound_phong_uniform_buf_ob_index = use_batch_index;
		}
	#else
		GLARE_DECLARE_USED(batch_index); // Suppress warning about unused var
		this->phong_uniform_buf_ob->updateData(/*dest offset=*/0, &uniforms, sizeof(PhongUniforms));
	#endif

#endif
		if(!use_bindless_textures)
			bindTexturesForPhongProg(opengl_mat);
	}
	else if(shader_prog == this->env_prog.getPointer())
	{
		glUniform4f(this->env_diffuse_colour_location, opengl_mat.albedo_linear_rgb.r, opengl_mat.albedo_linear_rgb.g, opengl_mat.albedo_linear_rgb.b, 1.f);
		glUniform1i(this->env_have_texture_location, opengl_mat.albedo_texture.nonNull() ? 1 : 0);
		const Vec4f campos_ws = current_scene->cam_to_world.getColumn(3);
		glUniform3fv(this->env_campos_ws_location, 1, campos_ws.x);
		if(shader_prog->time_loc >= 0)
			glUniform1f(shader_prog->time_loc, this->current_time);

		if(opengl_mat.albedo_texture.nonNull())
		{
			const GLfloat tex_elems[9] = {
				opengl_mat.tex_matrix.e[0], opengl_mat.tex_matrix.e[2], 0,
				opengl_mat.tex_matrix.e[1], opengl_mat.tex_matrix.e[3], 0,
				opengl_mat.tex_translation.x, opengl_mat.tex_translation.y, 1
			};
			glUniformMatrix3fv(this->env_texture_matrix_location, /*count=*/1, /*transpose=*/false, tex_elems);

			bindTextureToTextureUnit(*opengl_mat.albedo_texture, DIFFUSE_TEXTURE_UNIT_INDEX);
		}

		// There seems to be an Emscripten bug where sometimes the uniform values gets changed.  Just set it every frame as a workaround.
#if EMSCRIPTEN
		doSetStandardTextureUnitUniformsForBoundProgram(*this->env_prog);
#endif

		//assert(getIntUniformVal(*env_prog, this->env_prog->uniform_locations.blue_noise_tex_location) == BLUE_NOISE_TEXTURE_UNIT_INDEX);
		assert(getIntUniformVal(*env_prog, this->env_prog->uniform_locations.diffuse_tex_location) == DIFFUSE_TEXTURE_UNIT_INDEX);
		if(settings.render_sun_and_clouds)
		{
			assert(getIntUniformVal(*env_prog, this->env_prog->uniform_locations.fbm_tex_location) == FBM_TEXTURE_UNIT_INDEX);
			if(this->cirrus_tex.nonNull())
				assert(getIntUniformVal(*env_prog, this->env_prog->uniform_locations.cirrus_tex_location) == CIRRUS_TEX_TEXTURE_UNIT_INDEX);
		}

		assert(getBoundTexture2D(BLUE_NOISE_TEXTURE_UNIT_INDEX) == blue_noise_tex->texture_handle);
		assert(getBoundTexture2D(FBM_TEXTURE_UNIT_INDEX) == fbm_tex->texture_handle);

		if(this->cirrus_tex.nonNull())
			assert(getBoundTexture2D(CIRRUS_TEX_TEXTURE_UNIT_INDEX) == cirrus_tex->texture_handle);
		if(this->aurora_tex.nonNull())
			assert(getBoundTexture2D(AURORA_TEXTURE_UNIT_INDEX) == aurora_tex->texture_handle);
	}
	else if(shader_prog->is_depth_draw)
	{
		assert(false); // Done in drawBatchWithDenormalisedData()
	}
	else if(shader_prog->is_outline)
	{

	}
	else // Else shader created by user code:
	{
		if(shader_prog->time_loc >= 0)
			glUniform1f(shader_prog->time_loc, this->current_time);
		if(shader_prog->colour_loc >= 0)
			glUniform3fv(shader_prog->colour_loc, 1, &opengl_mat.albedo_linear_rgb.r);
		if(shader_prog->campos_os_loc >= 0)
		{
			Matrix4f world_to_ob;
			ob.ob_to_world_matrix.getInverseForAffine3Matrix(world_to_ob);
			const Vec4f campos_ws = this->current_scene->cam_to_world.getColumn(3);
			const Vec4f campos_os = world_to_ob * campos_ws;
			glUniform3fv(shader_prog->campos_os_loc, 1, campos_os.x);
		}
		if(shader_prog->ob_random_num_loc >= 0)
			glUniform1i(shader_prog->ob_random_num_loc, ob.random_num);

		if(shader_prog->albedo_texture_loc >= 0 && opengl_mat.albedo_texture.nonNull())
			bindTextureUnitToSampler(*opengl_mat.albedo_texture, /*texture_unit_index=*/DIFFUSE_TEXTURE_UNIT_INDEX, /*sampler_uniform_location=*/shader_prog->albedo_texture_loc);

		// Set user uniforms
		for(size_t i=0; i<shader_prog->user_uniform_info.size(); ++i)
		{
			switch(shader_prog->user_uniform_info[i].uniform_type)
			{
			case UserUniformInfo::UniformType_Vec2:
				glUniform2fv(shader_prog->user_uniform_info[i].loc, 1, (const float*)&opengl_mat.user_uniform_vals[i].vec2);
				break;
			case UserUniformInfo::UniformType_Vec3:
				glUniform3fv(shader_prog->user_uniform_info[i].loc, 1, (const float*)&opengl_mat.user_uniform_vals[i].vec3);
				break;
			case UserUniformInfo::UniformType_Int:
				glUniform1i(shader_prog->user_uniform_info[i].loc, opengl_mat.user_uniform_vals[i].intval);
				break;
			case UserUniformInfo::UniformType_Float:
				glUniform1f(shader_prog->user_uniform_info[i].loc, opengl_mat.user_uniform_vals[i].floatval);
				break;
			case UserUniformInfo::UniformType_Sampler2D:
				assert(0); // TODO
				break;
			}
		}
	}

	GLenum draw_mode;
	if(ob.object_type == 0)
		draw_mode = GL_TRIANGLES;
	else
	{
		draw_mode = GL_LINES;
		glLineWidth(ob.line_width);
	}

	if(use_multi_draw_indirect && shader_prog->supports_gpu_resident)
	{
		int index_type_size_B = 1;
		if(mesh_data.getIndexType() == GL_UNSIGNED_BYTE)
			index_type_size_B = 1;
		else if(mesh_data.getIndexType() == GL_UNSIGNED_SHORT)
			index_type_size_B = 2;
		else
			index_type_size_B = 4;

		// A mesh might have indices 0, 1, 2.  
		/*
		indices
		----------
		mesh_0 indices                                       mesh_1 indices
		[0, 1, 2, 0, 2, 3, ....                              , 0, 1, 2, 0, 2, 3, ...                          ]


		vertex data
		-----------
		mesh 0 vertex data                                          mesh 1 vertex data
		[0.2343, 0.1243, 0.645, ...                                 , 0.865, 2.343, 0.454, ....                   ]

		So we need to add a constant to the index values for mesh_1 - this is the offset into the vertex buffer at which mesh 1 vertex data is at.
		This is the baseVertex argument.
		*/

		const size_t instance_count = ob.instance_matrix_vbo.nonNull() ? ob.num_instances_to_draw : 1;

		const size_t total_buffer_offset = mesh_data.indices_vbo_handle.offset + prim_start_offset_B;
		assert(total_buffer_offset % index_type_size_B == 0);

		assert(num_draw_commands < MAX_BUFFERED_DRAW_COMMANDS);
		DrawElementsIndirectCommand& command = this->draw_commands_buffer[num_draw_commands];
		command.count = (GLsizei)num_indices;
		command.instanceCount = (uint32)instance_count;
		command.firstIndex = (uint32)(total_buffer_offset / index_type_size_B); // First index is not a byte offset, but a number-of-indices offset.
		command.baseVertex = mesh_data.vbo_handle.base_vertex; // "Specifies a constant that should be added to each element of indices when chosing elements from the enabled vertex arrays."
		command.baseInstance = 0;

		// Push back per-ob-vert-data and material indices to mat_and_ob_indices_buffer.
		doCheck((ob.per_ob_vert_data_index >= 0) && ((size_t)ob.per_ob_vert_data_index < this->per_ob_vert_data_buffer->byteSize() / sizeof(PerObjectVertUniforms)));
		doCheck((opengl_mat.material_data_index >= 0) && ((size_t)opengl_mat.material_data_index < this->phong_buffer->byteSize() / sizeof(PhongUniforms)));
		if(ob.mesh_data->usesSkinning())
			doCheck(ob.joint_matrices_base_index >= 0 && (size_t)ob.joint_matrices_base_index < this->joint_matrices_ssbo->byteSize() / sizeof(Matrix4f));


		uint32* const ob_and_mat_indices_buffer_write_ptr = &this->ob_and_mat_indices_buffer[num_draw_commands * OB_AND_MAT_INDICES_STRIDE];
		ob_and_mat_indices_buffer_write_ptr[0] = ob.per_ob_vert_data_index;
		ob_and_mat_indices_buffer_write_ptr[1] = ob.joint_matrices_base_index;
		ob_and_mat_indices_buffer_write_ptr[2] = opengl_mat.material_data_index;

		num_draw_commands++;

		//conPrint("   appended draw call.");

		//const size_t max_contiguous_draws_remaining_a = draw_indirect_circ_buf.contiguousBytesRemaining() / sizeof(DrawElementsIndirectCommand);
		//const size_t max_contiguous_draws_remaining_b = ob_and_mat_indices_circ_buf.contiguousBytesRemaining() / (sizeof(int) * 4);
		//assert(max_contiguous_draws_remaining_a == max_contiguous_draws_remaining_b);
		//assert(max_contiguous_draws_remaining_b >= 1);

		if((num_draw_commands == MAX_BUFFERED_DRAW_COMMANDS) /*(draw_commands.size() >= max_contiguous_draws_remaining_b)*/ || (instance_count > 1))
			submitBufferedDrawCommands();
	}
	else // else if not using multi-draw-indirect for this shader:
	{
		if(ob.instance_matrix_vbo.nonNull() && ob.num_instances_to_draw > 0)
		{
			const size_t total_buffer_offset = mesh_data.indices_vbo_handle.offset + prim_start_offset_B;
			drawElementsInstancedBaseVertex(draw_mode, (GLsizei)num_indices, mesh_data.getIndexType(), (void*)total_buffer_offset, (uint32)ob.num_instances_to_draw, mesh_data.vbo_handle.base_vertex);
		}
		else
		{
			const size_t total_buffer_offset = mesh_data.indices_vbo_handle.offset + prim_start_offset_B;
			drawElementsBaseVertex(draw_mode, (GLsizei)num_indices, mesh_data.getIndexType(), (void*)total_buffer_offset, mesh_data.vbo_handle.base_vertex);
		}
	}

	this->num_indices_submitted += num_indices;
	//this->num_face_groups_submitted++;
}


void OpenGLEngine::drawBatchWithDenormalisedData(const GLObject& ob, const GLObjectBatchDrawInfo& batch, [[maybe_unused]] uint32 batch_index)
{
	const uint32 prim_start_offset = batch.prim_start_offset_B;
	const uint32 num_indices = batch.num_indices;

	if(num_indices == 0)
		return;

	const bool prog_supports_GPU_resident = BitUtils::isBitSet(batch.program_index_and_flags, PROG_SUPPORTS_GPU_RESIDENT_BITFLAG);
	assert(prog_supports_GPU_resident == this->prog_vector[batch.getProgramIndex()]->supports_gpu_resident);

	const bool use_MDI_for_batch = use_multi_draw_indirect && prog_supports_GPU_resident;

	const GLenum draw_mode = GL_TRIANGLES;

	const GLenum index_type = (GLenum)(ob.index_type_and_log2_size & 0xFFFF);
	const uint32 index_type_log2_size_B = ob.index_type_and_log2_size >> 16;
	assert(index_type == ob.mesh_data->getIndexType());
	assert((index_type_log2_size_B == indexTypeLog2SizeBytes(index_type)) && ((1u << index_type_log2_size_B) == indexTypeSizeBytes(index_type)));

	
	if(!use_MDI_for_batch)
	{
		// non-MDI path:
		if(use_ob_and_mat_data_gpu_resident && prog_supports_GPU_resident)
		{
			ObJointAndMatIndicesStruct indices;
			indices.per_ob_data_index = ob.per_ob_vert_data_index;
			indices.material_index = batch.material_data_or_mat_index;
			indices.joints_base_index = ob.joint_matrices_base_index;
			this->ob_joint_and_mat_indices_uniform_buf_ob->updateData(/*dest offset=*/0, &indices, sizeof(ObJointAndMatIndicesStruct));
		}
		else
		{
			// Set per-object vert uniforms.  Only do this if the current object has changed.
			if(&ob != current_uniforms_ob)
			{
				const OpenGLProgram* shader_prog = this->prog_vector[batch.getProgramIndex()].ptr();
				if(shader_prog->uses_vert_uniform_buf_obs)
				{
					PerObjectVertUniforms uniforms;
					uniforms.model_matrix = ob.ob_to_world_matrix;
					uniforms.normal_matrix = ob.ob_to_world_normal_matrix;
					for(int i=0; i<GLObject::MAX_NUM_LIGHT_INDICES; ++i)
						uniforms.light_indices[i] = ob.light_indices[i];
					uniforms.depth_draw_depth_bias = ob.depth_draw_depth_bias;
					uniforms.model_matrix_upper_left_det = ob.ob_to_world_matrix_determinant;
					uniforms.uv0_scale = ob.mesh_data->uv0_scale;
					uniforms.uv1_scale = ob.mesh_data->uv1_scale;
					uniforms.dequantise_scale = ob.dequantise_scale;
					uniforms.dequantise_translation = ob.dequantise_translation;
					this->per_object_vert_uniform_buf_ob->updateData(/*dest offset=*/0, &uniforms, sizeof(PerObjectVertUniforms));
				}
				else
				{
					glUniformMatrix4fv(shader_prog->model_matrix_loc, 1, false, ob.ob_to_world_matrix.e);
					glUniformMatrix4fv(shader_prog->normal_matrix_loc, 1, false, ob.ob_to_world_normal_matrix.e);
				}

				if(shader_prog->uses_skinning)
				{
					assert(ob.mesh_data->usesSkinning());
					assert(ob.joint_matrices.size() > 0);

					// The joint_matrix uniform array has 256 elems, don't upload more than that.
					const size_t num_joint_matrices_to_upload = myMin<size_t>(max_num_joint_matrices_per_ob, ob.joint_matrices.size()); 
					this->joint_matrices_buf_ob->updateData(/*dest offset=*/0, /*src data=*/ob.joint_matrices[0].e, /*src size=*/num_joint_matrices_to_upload * sizeof(Matrix4f));
				}

				current_uniforms_ob = &ob;
			}

		
			// Set material uniforms.  NOTE: Setting the uniforms manually in this way (switching on shader program) is obviously quite hacky.  Improve.
			const OpenGLProgram* const prog = this->prog_vector[batch.getProgramIndex()].ptr();
			if(prog->uses_phong_uniforms)
			{
				assert(batch.material_data_or_mat_index == ob.getUsedBatches()[batch_index].material_index);
				const OpenGLMaterial& opengl_mat = ob.materials[batch.material_data_or_mat_index]; // This is the non-MDI case, so material_data_or_mat_index is the index into ob.materials

#if UNIFORM_BUF_PER_MAT_SUPPORT
	
				glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/PHONG_UBO_BINDING_POINT_INDEX, opengl_mat.uniform_buf_ob->handle);

#else

				PhongUniforms uniforms;
				setUniformsForPhongProg(opengl_mat, *ob.mesh_data, uniforms);
			
			#if MULTIPLE_PHONG_UNIFORM_BUFS_SUPPORT
				const uint32 use_batch_index = settings.use_multiple_phong_uniform_bufs ? myMin(batch_index, 63u) : 0;

				this->phong_uniform_buf_obs[use_batch_index]->updateData(/*dest offset=*/0, &uniforms, sizeof(PhongUniforms));

				if(use_batch_index != current_bound_phong_uniform_buf_ob_index)
				{
					glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/PHONG_UBO_BINDING_POINT_INDEX, this->phong_uniform_buf_obs[use_batch_index]->handle);
					current_bound_phong_uniform_buf_ob_index = use_batch_index;
				}
			#else
				this->phong_uniform_buf_ob->updateData(/*dest offset=*/0, &uniforms, sizeof(PhongUniforms));
			#endif

#endif
				if(!use_bindless_textures)
					bindTexturesForPhongProg(opengl_mat);
			}
			else if(prog == this->env_prog.getPointer())
			{
				assert(false); // env_prog drawing is done in drawBatch()
			}
			else if(prog->is_depth_draw)
			{
				assert(!use_multi_draw_indirect); // If multi-draw-indirect was enabled, depth-draw mats would be handled in (use_MDI_and_prog_supports_MDI) branch.

				// Slow, non-MDI path:
				assert(batch.material_data_or_mat_index == ob.depth_draw_batches[batch_index].material_data_or_mat_index);
				const OpenGLMaterial& opengl_mat = ob.materials[batch.material_data_or_mat_index]; // This is the non-MDI case, so material_data_or_mat_index is the index into ob.materials

				//const Matrix4f proj_view_model_matrix = proj_mat * view_mat * ob.ob_to_world_matrix;
				//glUniformMatrix4fv(shader_prog->uniform_locations.proj_view_model_matrix_location, 1, false, proj_view_model_matrix.e);
				if(prog->is_depth_draw_with_alpha_test)
				{
					assert(opengl_mat.albedo_texture.nonNull()); // We should only be using the depth shader with alpha test if there is a texture with alpha.

					DepthUniforms uniforms;

					uniforms.texture_upper_left_matrix_col0.x = opengl_mat.tex_matrix.e[0];
					uniforms.texture_upper_left_matrix_col0.y = opengl_mat.tex_matrix.e[2];
					uniforms.texture_upper_left_matrix_col1.x = opengl_mat.tex_matrix.e[1];
					uniforms.texture_upper_left_matrix_col1.y = opengl_mat.tex_matrix.e[3];
					uniforms.texture_matrix_translation = opengl_mat.tex_translation;

					if(this->use_bindless_textures)
					{
						if(opengl_mat.albedo_texture.nonNull())
							uniforms.diffuse_tex = opengl_mat.albedo_texture->getBindlessTextureHandle();
					}
					else
					{
						assert(prog->uniform_locations.diffuse_tex_location >= 0);
						bindTextureToTextureUnitRaw(*opengl_mat.albedo_texture, /*texture_unit_index=*/DIFFUSE_TEXTURE_UNIT_INDEX);
					}

					// Just set IMPOSTER_TEX_HAS_MULTIPLE_ANGLES flag which is the only one used in depth_frag_shader.glsl
					uniforms.flags                   = opengl_mat.imposter_tex_has_multiple_angles ? IMPOSTER_TEX_HAS_MULTIPLE_ANGLES : 0;
			
					uniforms.begin_fade_out_distance = opengl_mat.begin_fade_out_distance;
					uniforms.end_fade_out_distance   = opengl_mat.end_fade_out_distance;
					uniforms.materialise_lower_z	 = opengl_mat.materialise_lower_z;
					uniforms.materialise_upper_z	 = opengl_mat.materialise_upper_z;
					uniforms.materialise_start_time  = opengl_mat.materialise_start_time;

					this->depth_uniform_buf_ob->updateData(/*dest offset=*/0, &uniforms, sizeof(DepthUniforms));
				}
				else
				{
					DepthUniforms uniforms;

					uniforms.flags                   = 0;
					uniforms.begin_fade_out_distance = opengl_mat.begin_fade_out_distance;
					uniforms.end_fade_out_distance   = opengl_mat.end_fade_out_distance;
					uniforms.materialise_lower_z	 = opengl_mat.materialise_lower_z;
					uniforms.materialise_upper_z	 = opengl_mat.materialise_upper_z;
					uniforms.materialise_start_time	 = opengl_mat.materialise_start_time;

					this->depth_uniform_buf_ob->updateData(/*dest offset=*/0, &uniforms, sizeof(DepthUniforms));
				}
			}
			else if(prog->is_outline)
			{

			}
			else // Else shader created by user code:
			{
				// Slow path:
				assert(batch.material_data_or_mat_index == ob.getUsedBatches()[batch_index].material_index);
				const OpenGLMaterial& opengl_mat = ob.materials[batch.material_data_or_mat_index];

				if(prog->time_loc >= 0)
					glUniform1f(prog->time_loc, this->current_time);
				if(prog->colour_loc >= 0)
					glUniform3fv(prog->colour_loc, 1, &opengl_mat.albedo_linear_rgb.r);
				if(prog->campos_os_loc >= 0)
				{
					Matrix4f world_to_ob;
					ob.ob_to_world_matrix.getInverseForAffine3Matrix(world_to_ob);
					const Vec4f campos_ws = this->current_scene->cam_to_world.getColumn(3);
					const Vec4f campos_os = world_to_ob * campos_ws;
					glUniform3fv(prog->campos_os_loc, 1, campos_os.x);
				}
				if(prog->ob_random_num_loc >= 0)
					glUniform1ui(prog->ob_random_num_loc, ob.random_num);

				if(prog->albedo_texture_loc >= 0 && opengl_mat.albedo_texture)
					bindTextureUnitToSampler(*opengl_mat.albedo_texture, /*texture_unit_index=*/DIFFUSE_TEXTURE_UNIT_INDEX, /*sampler_uniform_location=*/prog->albedo_texture_loc);

				assert(getBoundTexture2D(FBM_TEXTURE_UNIT_INDEX)        == fbm_tex->texture_handle);

				// Set user uniforms
				for(size_t i=0; i<prog->user_uniform_info.size(); ++i)
				{
					switch(prog->user_uniform_info[i].uniform_type)
					{
					case UserUniformInfo::UniformType_Vec2:
						glUniform2fv(prog->user_uniform_info[i].loc, 1, (const float*)&opengl_mat.user_uniform_vals[i].vec2);
						break;
					case UserUniformInfo::UniformType_Vec3:
						glUniform3fv(prog->user_uniform_info[i].loc, 1, (const float*)&opengl_mat.user_uniform_vals[i].vec3);
						break;
					case UserUniformInfo::UniformType_Int:
						glUniform1i(prog->user_uniform_info[i].loc, opengl_mat.user_uniform_vals[i].intval);
						break;
					case UserUniformInfo::UniformType_Float:
						glUniform1f(prog->user_uniform_info[i].loc, opengl_mat.user_uniform_vals[i].floatval);
						break;
					case UserUniformInfo::UniformType_Sampler2D:
						assert(0); // TODO
						break;
					}
				}
			}
		}

		assert(ob.indices_vbo_handle_offset == ob.mesh_data->indices_vbo_handle.offset);
		assert(ob.vbo_handle_base_vertex == (uint32)ob.mesh_data->vbo_handle.base_vertex);

		if(ob.instance_matrix_vbo.nonNull() && ob.num_instances_to_draw > 0)
		{
			const size_t total_buffer_offset = ob.indices_vbo_handle_offset + prim_start_offset;
			drawElementsInstancedBaseVertex(draw_mode, (GLsizei)num_indices, index_type, (void*)total_buffer_offset, (uint32)ob.num_instances_to_draw, ob.vbo_handle_base_vertex);
		}
		else
		{
			const size_t total_buffer_offset = ob.indices_vbo_handle_offset + prim_start_offset;
			drawElementsBaseVertex(draw_mode, (GLsizei)num_indices, index_type, (void*)total_buffer_offset, ob.vbo_handle_base_vertex);
		}
	}
	else // else if use_MDI_for_batch:
	{
		// A mesh might have indices 0, 1, 2.  
		/*
		indices
		----------
		mesh_0 indices                                       mesh_1 indices
		[0, 1, 2, 0, 2, 3, ....                              , 0, 1, 2, 0, 2, 3, ...                          ]
		
		
		vertex data
		-----------
		mesh 0 vertex data                                          mesh 1 vertex data
		[0.2343, 0.1243, 0.645, ...                                 , 0.865, 2.343, 0.454, ....                   ]

		So we need to add a constant to the index values for mesh_1 - this is the offset into the vertex buffer at which mesh 1 vertex data is at.
		This is the baseVertex argument.
		*/

		const uint32 instance_count = (ob.instance_matrix_vbo_name != 0) ? (uint32)ob.num_instances_to_draw : (uint32)1;

		assert(ob.indices_vbo_handle_offset == ob.mesh_data->indices_vbo_handle.offset);
		const uint32 total_buffer_offset = ob.indices_vbo_handle_offset + prim_start_offset;
		assert(total_buffer_offset % (1 << index_type_log2_size_B) == 0);

		assert(ob.vbo_handle_base_vertex == (uint32)ob.mesh_data->vbo_handle.base_vertex);

		assert(num_draw_commands < MAX_BUFFERED_DRAW_COMMANDS);
		DrawElementsIndirectCommand& command = this->draw_commands_buffer[num_draw_commands];
		command.count = num_indices;
		command.instanceCount = instance_count;
		command.firstIndex = total_buffer_offset >> index_type_log2_size_B; // = total_buffer_offset / index_type_size_B.  First index is not a byte offset, but a number-of-indices offset.
		command.baseVertex = ob.vbo_handle_base_vertex; // "Specifies a constant that should be added to each element of indices when choosing elements from the enabled vertex arrays."
		command.baseInstance = 0;


		// Push back per-ob-vert-data and material indices to mat_and_ob_indices_buffer.
		doCheck((ob.per_ob_vert_data_index >= 0) && ((size_t)ob.per_ob_vert_data_index < this->per_ob_vert_data_buffer->byteSize() / sizeof(PerObjectVertUniforms)));
		doCheck((batch.material_data_or_mat_index >= 0) && ((size_t)batch.material_data_or_mat_index < this->phong_buffer->byteSize() / sizeof(PhongUniforms)));
#ifndef NDEBUG
		//const OpenGLMaterial& opengl_mat = ob.materials[is_depth_draw ? ob.depth_draw_batch_material_indices[batch_index] : ob.mesh_data->batches[batch_index].material_index];
		//assert(batch.material_data_index == opengl_mat.material_data_index);

		if(ob.mesh_data->usesSkinning())
			doCheck(ob.joint_matrices_base_index >= 0 && (size_t)ob.joint_matrices_base_index < this->joint_matrices_ssbo->byteSize() / sizeof(Matrix4f));
#endif

		uint32* const ob_and_mat_indices_buffer_write_ptr = &this->ob_and_mat_indices_buffer[num_draw_commands * OB_AND_MAT_INDICES_STRIDE];
		ob_and_mat_indices_buffer_write_ptr[0] = ob.per_ob_vert_data_index;
		ob_and_mat_indices_buffer_write_ptr[1] = ob.joint_matrices_base_index;
		ob_and_mat_indices_buffer_write_ptr[2] = batch.material_data_or_mat_index;

		num_draw_commands++;

		//conPrint("   appended draw call.");

		//const size_t max_contiguous_draws_remaining_a = draw_indirect_circ_buf.contiguousBytesRemaining() / sizeof(DrawElementsIndirectCommand);
		//const size_t max_contiguous_draws_remaining_b = ob_and_mat_indices_circ_buf.contiguousBytesRemaining() / (sizeof(int) * 4);
		//assert(max_contiguous_draws_remaining_a == max_contiguous_draws_remaining_b);
		//assert(max_contiguous_draws_remaining_b >= 1);

		if((num_draw_commands == MAX_BUFFERED_DRAW_COMMANDS) /*(draw_commands.size() >= max_contiguous_draws_remaining_b)*/ || (instance_count > 1))
			submitBufferedDrawCommands();
	}

	this->num_indices_submitted += num_indices;
	//this->num_face_groups_submitted++;
}


void OpenGLEngine::submitBufferedDrawCommands()
{
	assert(use_multi_draw_indirect);

	//conPrint("Flushing " + toString(this->draw_commands.size()) + " draw commands");

	if(num_draw_commands > 0)
	{
		//const size_t max_contiguous_draws = draw_indirect_circ_buf.contiguousBytesRemaining() / sizeof(DrawElementsIndirectCommand);
		/*{
			const size_t max_contiguous_draws_remaining_a = draw_indirect_circ_buf.contiguousBytesRemaining()      / sizeof(DrawElementsIndirectCommand);
			const size_t max_contiguous_draws_remaining_b = ob_and_mat_indices_circ_buf.contiguousBytesRemaining() / (sizeof(int) * 4);
			assert(max_contiguous_draws_remaining_a == max_contiguous_draws_remaining_b);
			assert(max_contiguous_draws_remaining_b >= 1);
		}*/

#if USE_CIRCULAR_BUFFERS_FOR_MDI
		void* ob_and_mat_ind_write_ptr = ob_and_mat_indices_circ_buf.getRangeForCPUWriting(ob_and_mat_indices_buffer.dataSizeBytes());
		std::memcpy(ob_and_mat_ind_write_ptr, ob_and_mat_indices_buffer.data(), ob_and_mat_indices_buffer.dataSizeBytes());

		void* draw_cmd_write_ptr = draw_indirect_circ_buf.getRangeForCPUWriting(draw_commands.dataSizeBytes());
		std::memcpy(draw_cmd_write_ptr, draw_commands.data(), draw_commands.dataSizeBytes());

		draw_indirect_buffer->bind();

		const size_t ob_and_mat_offset = (uint8*)ob_and_mat_ind_write_ptr - (uint8*)ob_and_mat_indices_circ_buf.buffer;
		glBindBufferRange(GL_SHADER_STORAGE_BUFFER, /*index=*/OB_AND_MAT_INDICES_SSBO_BINDING_POINT_INDEX, ob_and_mat_indices_ssbo->handle, /*offset=*/ob_and_mat_offset, ob_and_mat_indices_buffer.dataSizeBytes());

		glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
#else

		// Upload mat_and_ob_indices_buffer to GPU
		
		// Invalidate ob_and_mat_indices SSBO, as we will probably just be updating and using part of the buffer below.
		// This hopefully allows the driver some leeway in discarding the rest of the buffer, thereby increasing efficiency.
		ob_and_mat_indices_ssbo->invalidateBufferData(); // Binds and then invalidates

		assert(num_draw_commands * (sizeof(uint32) * OB_AND_MAT_INDICES_STRIDE) <= ob_and_mat_indices_ssbo->byteSize());
		ob_and_mat_indices_ssbo->updateData(/*dest offset=*/0, ob_and_mat_indices_buffer.data(), num_draw_commands * (sizeof(uint32) * OB_AND_MAT_INDICES_STRIDE), /*bind needed=*/false);


#if USE_DRAW_INDIRECT_BUFFER_FOR_MDI
		draw_indirect_buffer->invalidateBufferData();
		assert(num_draw_commands * sizeof(DrawElementsIndirectCommand) <= draw_indirect_buffer->byteSize());
		draw_indirect_buffer->updateData(/*offset=*/0, this->draw_commands_buffer.data(), num_draw_commands * sizeof(DrawElementsIndirectCommand));
#endif

#endif
		
#if !defined(OSX) && !defined(EMSCRIPTEN)
		glMultiDrawElementsIndirect(
			GL_TRIANGLES,
			this->current_index_type, // index type
#if USE_DRAW_INDIRECT_BUFFER_FOR_MDI
			0, // indirect - bytes into the GL_DRAW_INDIRECT_BUFFER.
#elif USE_CIRCULAR_BUFFERS_FOR_MDI
			(void*)((uint8*)draw_cmd_write_ptr - (uint8*)draw_indirect_circ_buf.buffer), // indirect - bytes into the GL_DRAW_INDIRECT_BUFFER.
#else
			this->draw_commands.data(), // Read draw command structures from CPU memory
#endif
			(GLsizei)num_draw_commands, // drawcount
			0 // stride - use 0 to mean tightly packed.
		);
#endif // end ifndef OSX

#if USE_CIRCULAR_BUFFERS_FOR_MDI
		ob_and_mat_indices_circ_buf.finishedCPUWriting(ob_and_mat_ind_write_ptr, ob_and_mat_indices_buffer.dataSizeBytes());
		draw_indirect_circ_buf.finishedCPUWriting(draw_cmd_write_ptr, draw_commands.dataSizeBytes());
#endif

		this->num_multi_draw_indirect_calls++;

		// Now that we have submitted the multi-draw call, start writing to the front of ob_and_mat_indices_buffer and draw_commands again.
		num_draw_commands = 0;
	}
}


Reference<OpenGLTexture> OpenGLEngine::loadCubeMap(const std::vector<Reference<Map2D> >& face_maps, const TextureParams& params)
{
	if(dynamic_cast<const ImageMapFloat*>(face_maps[0].getPointer()))
	{
		std::vector<const void*> tex_data(6);
		for(int i=0; i<6; ++i)
		{
			const ImageMapFloat* imagemap = static_cast<const ImageMapFloat*>(face_maps[i].getPointer());

			if(imagemap->getN() != 3)
				throw glare::Exception("Texture has unhandled number of components: " + toString(imagemap->getN()));

			tex_data[i] = imagemap->getData();
		}

		const size_t tex_xres = face_maps[0]->getMapWidth();
		const size_t tex_yres = face_maps[0]->getMapHeight();
		Reference<OpenGLTexture> opengl_tex = new OpenGLTexture();
		opengl_tex->createCubeMap(tex_xres, tex_yres, this, tex_data, OpenGLTextureFormat::Format_RGB_Linear_Float, params.filtering);

		//this->opengl_textures.insert(std::make_pair(key, opengl_tex)); // Store

		return opengl_tex;
	}
	/*else if(dynamic_cast<const ImageMap<half, HalfComponentValueTraits>*>(&map2d))
	{
		const ImageMap<half, HalfComponentValueTraits>* imagemap = static_cast<const ImageMap<half, HalfComponentValueTraits>*>(&map2d);

		// Compute hash of map
		const uint64 key = XXH64(imagemap->getData(), imagemap->getDataSize() * sizeof(half), 1);

		auto res = this->opengl_textures.find(key);
		if(res == this->opengl_textures.end())
		{
			// Load texture
			unsigned int tex_xres = map2d.getMapWidth();
			unsigned int tex_yres = map2d.getMapHeight();

			if(imagemap->getN() == 3)
			{
				Reference<OpenGLTexture> opengl_tex = new OpenGLTexture();
				opengl_tex->load(tex_xres, tex_yres, (uint8*)imagemap->getData(), this, target, GL_RGB, GL_RGB, GL_HALF_FLOAT,
					OpenGLTexture::Filtering_Fancy
				);

				this->opengl_textures.insert(std::make_pair(key, opengl_tex)); // Store

				return opengl_tex;
			}
			else
				throw glare::Exception("Texture has unhandled number of components: " + toString(imagemap->getN()));

		}
		else // Else if this map has already been loaded into an OpenGL Texture:
		{
			return res->second;
		}
	}*/
	else
	{
		throw glare::Exception("Unhandled texture type.");
	}
}


// If the texture identified by key has been loaded into OpenGL, then return the OpenGL texture.
// Otherwise load the texture from map2d into OpenGL immediately.
Reference<OpenGLTexture> OpenGLEngine::getOrLoadOpenGLTextureForMap2D(const OpenGLTextureKey& key, const Map2D& map2d, const TextureParams& params)
{
	ZoneScoped; // Tracy profiler

	auto res = this->opengl_textures.find(key);
	if(res != this->opengl_textures.end())// if this map has already been loaded into an OpenGL Texture:
	{
		if(res->second.value->getRefCount() == 1)
			this->textureBecameUsed(res->second.value.ptr());
		return res->second.value;
	}

	if(params.allow_compression)
	{
		assert(params.use_mipmaps);
	}

	if(params.filtering != OpenGLTexture::Filtering_Fancy)
	{
		assert(params.use_mipmaps == false);
	}

	// Build or get texture data
	Reference<TextureData> texture_data;
	if(const CompressedImage* compressed_img = dynamic_cast<const CompressedImage*>(&map2d))
	{
		texture_data = compressed_img->texture_data; // If the image map just holds already compressed data, use it.
	}
	else
	{
		const bool use_compression = params.allow_compression && this->DXTTextureCompressionSupportedAndEnabled() && params.use_mipmaps && OpenGLTexture::areTextureDimensionsValidForCompression(map2d); // The non mip-mapping code-path doesn't allow compression
		texture_data = TextureProcessing::buildTextureData(&map2d, this->mem_allocator.ptr(), this->main_task_manager, use_compression, params.use_mipmaps, params.convert_float_to_half);
	}

	OpenGLTextureLoadingProgress loading_progress;
	TextureLoading::initialiseTextureLoadingProgress(this, key, params, texture_data, loading_progress);

	const int MAX_ITERS = 1000;
	int i = 0;
	for(; i<MAX_ITERS; ++i)
	{
		testAssert(loading_progress.loadingInProgress());
		const size_t MAX_UPLOAD_SIZE_B = 1000000000ull;
		size_t total_bytes_uploaded = 0;
		TextureLoading::partialLoadTextureIntoOpenGL(loading_progress, total_bytes_uploaded, MAX_UPLOAD_SIZE_B);
		if(loading_progress.done())
			break;
	}
	runtimeCheck(i < MAX_ITERS);

	addOpenGLTexture(loading_progress.opengl_tex->key, loading_progress.opengl_tex);
	return loading_progress.opengl_tex;
}


void OpenGLEngine::addOpenGLTexture(const OpenGLTextureKey& key, const Reference<OpenGLTexture>& opengl_tex)
{
	ZoneScoped; // Tracy profiler

	opengl_tex->key = key;
	opengl_tex->m_opengl_engine = this;
	opengl_tex->inserted_into_opengl_textures = true;
	this->opengl_textures.set(key, opengl_tex);

	if(opengl_tex->texture_data.nonNull())
		this->tex_CPU_mem_usage += opengl_tex->texture_data->totalCPUMemUsage();

	this->tex_GPU_mem_usage += opengl_tex->getTotalStorageSizeB();

	textureBecameUsed(opengl_tex.ptr());

	trimTextureUsage();
}


void OpenGLEngine::removeOpenGLTexture(const OpenGLTextureKey& key)
{
	auto it = this->opengl_textures.find(key);

	if(it != this->opengl_textures.end())
	{
		OpenGLTexture* tex = it->second.value.ptr();

		if(tex->texture_data.nonNull())
		{
			assert(this->tex_CPU_mem_usage >= tex->texture_data->totalCPUMemUsage());
			this->tex_CPU_mem_usage -= tex->texture_data->totalCPUMemUsage();
		}

		tex->inserted_into_opengl_textures = false;

		assert(this->tex_GPU_mem_usage >= tex->getTotalStorageSizeB());
		this->tex_GPU_mem_usage -= tex->getTotalStorageSizeB();

		this->opengl_textures.erase(it);
	}
}


bool OpenGLEngine::isOpenGLTextureInsertedForKey(const OpenGLTextureKey& key) const
{
	return this->opengl_textures.isInserted(key);
}


float OpenGLEngine::getPixelDepth(int pixel_x, int pixel_y)
{
	// TODO: update with new infinite perspective projection matrix code.

	float depth;
	glReadPixels(pixel_x, pixel_y, 
		1, 1,  // width, height
		GL_DEPTH_COMPONENT, GL_FLOAT, 
		&depth);

	const double z_far  = current_scene->max_draw_dist;
	const double z_near = current_scene->near_draw_dist;

	// From http://learnopengl.com/#!Advanced-OpenGL/Depth-testing
	float z = depth * 2.0f - 1.0f; 
	float linear_depth = (float)((2.0 * z_near * z_far) / (z_far + z_near - z * (z_far - z_near)));
	return linear_depth;
}


Reference<ImageMap<uint8, UInt8ComponentValueTraits> > OpenGLEngine::getRenderedColourBuffer(size_t xres, size_t yres, bool buffer_has_alpha)
{
	const bool capture_alpha = buffer_has_alpha;

	// Get current byte alignment for the start of each row in memory, this is needed for reading as GL_RGB since the row size may not be a multiple of 4 bytes (which GL_PACK_ALIGNMENT probably is).
	GLint alignment;
	glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);

	const int N = capture_alpha ? 4 : 3;
	ImageMapUInt8Ref map = new ImageMapUInt8(xres, yres, N);

	runtimeCheck((alignment > 0) && Maths::isPowerOfTwo(alignment));
	const size_t row_B = Maths::roundUpToMultipleOfPowerOf2(xres * N, (size_t)alignment);
	js::Vector<uint8, 16> data(yres * row_B);

	glReadPixels(0, 0, // x, y
		(GLsizei)xres, (GLsizei)yres,  // width, height
		capture_alpha ? GL_RGBA : GL_RGB,
		GL_UNSIGNED_BYTE,
		data.data()
	);
	
	// Copy from data to map, while flipping image upside down.
	for(int dy=0; dy<yres; ++dy)
	{
		const int sy = (int)yres - dy - 1;
		std::memcpy(/*dest=*/map->getPixel(0, dy), /*src=*/&data[sy * row_B], /*size=*/xres * N);
	}

	return map;
}


// Creates some off-screen render buffers, sets as target_frame_buffer, draws the current scene.
// Then captures the draw result using glReadPixels() in getRenderedColourBuffer() and returns as an ImageMapUInt8.
Reference<ImageMap<uint8, UInt8ComponentValueTraits>> OpenGLEngine::drawToBufferAndReturnImageMap()
{
	// Save target_frame_buffer
	Reference<FrameBuffer> old_target_frame_buffer = this->target_frame_buffer;

	// Use an 8-bit format, as float formats don't seem to work with glReadPixels in WebGL.
	// And we are reading to an 8-bit image format so it makes sense.
	const OpenGLTextureFormat col_buffer_format = OpenGLTextureFormat::Format_RGBA_Linear_Uint8;

	const OpenGLTextureFormat depth_format = OpenGLTextureFormat::Format_Depth_Float;

	const int xres = myMax(16, main_viewport_w);
	const int yres = myMax(16, main_viewport_h);
	const int msaa_samples = (settings.msaa_samples <= 1) ? -1 : settings.msaa_samples;

	// Allocate renderbuffers.  Note that these must have the same format as the textures, otherwise the glBlitFramebuffer copies will fail in WebGL.
	RenderBufferRef colour_renderbuffer = new RenderBuffer(xres, yres, msaa_samples, col_buffer_format);
	RenderBufferRef depth_renderbuffer  = new RenderBuffer(xres, yres, msaa_samples, depth_format);

	FrameBufferRef render_framebuffer = new FrameBuffer();
	render_framebuffer->attachRenderBuffer(*colour_renderbuffer, GL_COLOR_ATTACHMENT0);
	render_framebuffer->attachRenderBuffer(*depth_renderbuffer, GL_DEPTH_ATTACHMENT);
	assert(render_framebuffer->isComplete());

	// Create render_copy_framebuffer.  This is similar to render_framebuffer except it doesn't use MSAA (is not clear to me how glReadPixels works with MSAA)
	// So we will blit from render_framebuffer to colour_copy_texture to resolve the MSAA.
	OpenGLTextureRef colour_copy_texture = new OpenGLTexture(xres, yres, this,
		ArrayRef<uint8>(), // data
		col_buffer_format,
		OpenGLTexture::Filtering_Nearest,
		OpenGLTexture::Wrapping_Clamp,
		false, // has_mipmaps
		/*MSAA_samples=*/1
	);

	OpenGLTextureRef depth_copy_texture = new OpenGLTexture(xres, yres, this,
		ArrayRef<uint8>(), // data
		depth_format,
		OpenGLTexture::Filtering_Nearest,
		OpenGLTexture::Wrapping_Clamp,
		false, // has_mipmaps
		/*MSAA_samples=*/1
	);

	FrameBufferRef render_copy_framebuffer = new FrameBuffer();
	render_copy_framebuffer->attachTexture(*colour_copy_texture, GL_COLOR_ATTACHMENT0);
	render_copy_framebuffer->attachTexture(*depth_copy_texture, GL_DEPTH_ATTACHMENT);
	assert(render_copy_framebuffer->isComplete());



	this->target_frame_buffer = render_framebuffer;

	this->draw(); // Render the scene


	//=============== Copy from render_framebuffer to render_copy_framebuffer ============
	render_framebuffer->bindForReading();
	render_copy_framebuffer->bindForDrawing();

	glReadBuffer(GL_COLOR_ATTACHMENT0);
	setSingleDrawBuffer(GL_COLOR_ATTACHMENT0);
	glBlitFramebuffer(
		/*srcX0=*/0, /*srcY0=*/0, /*srcX1=*/(int)render_framebuffer->xRes(), /*srcY1=*/(int)render_framebuffer->yRes(), 
		/*dstX0=*/0, /*dstY0=*/0, /*dstX1=*/(int)render_framebuffer->xRes(), /*dstY1=*/(int)render_framebuffer->yRes(), 
		GL_COLOR_BUFFER_BIT, // We need the depth buffer only when doing DOF blur.
		GL_NEAREST
	);

	// Capture render_copy_framebuffer as an ImageMapUInt8
	render_copy_framebuffer->bindForReading();
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	ImageMapUInt8Ref image = getRenderedColourBuffer(xres, yres, /*buffer has alpha=*/numChannels(col_buffer_format) == 4); // Capture the framebuffer

	// Free the framebuffers we just made
	render_framebuffer = nullptr;
	render_copy_framebuffer = nullptr;
	this->target_frame_buffer = nullptr; 
	
	// Restore target_frame_buffer
	this->target_frame_buffer = old_target_frame_buffer;

	return image;
}


void OpenGLEngine::setTargetFrameBufferAndViewport(const Reference<FrameBuffer> frame_buffer)
{ 
	target_frame_buffer = frame_buffer;
	setViewportDims((int)frame_buffer->xRes(), (int)frame_buffer->yRes());
}


void OpenGLEngine::setReadFrameBufferToDefault()
{
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0); // Restore read framebuffer back to default framebuffer
}


// Copies material and per-object vert data (matrices) on the GPU back to CPU mem, checks that it is the same as the current CPU state.
// For debugging.
void OpenGLEngine::checkMDIGPUDataCorrect()
{
	for(auto z = scenes.begin(); z != scenes.end(); ++z)
	{
		const OpenGLScene* scene = z->ptr();
		for(auto it = scene->objects.begin(); it != scene->objects.end(); ++it)
		{
			const GLObject* ob = it->ptr();
			for(size_t i=0; i<ob->materials.size(); ++i)
			{
				if(ob->per_ob_vert_data_index >= 0)
				{
					// Read data back from GPU
					PerObjectVertUniforms uniforms;
					per_ob_vert_data_buffer->readData(ob->per_ob_vert_data_index * sizeof(PerObjectVertUniforms), &uniforms, sizeof(PerObjectVertUniforms));

					doCheck(uniforms.model_matrix  == ob->ob_to_world_matrix);
					doCheck(uniforms.normal_matrix == ob->ob_to_world_normal_matrix);
				}


				if(ob->materials[i].material_data_index >= 0)
				{
					// Read back to CPU
					PhongUniforms phong_uniforms;
					phong_buffer->readData(ob->materials[i].material_data_index * sizeof(PhongUniforms), &phong_uniforms, sizeof(PhongUniforms));

					PhongUniforms ref_phong_uniforms;
					setUniformsForPhongProg(ob->materials[i], *ob->mesh_data, ref_phong_uniforms);

					doCheck(phong_uniforms.diffuse_tex				== ref_phong_uniforms.diffuse_tex);
					doCheck(phong_uniforms.metallic_roughness_tex	== ref_phong_uniforms.metallic_roughness_tex);
					doCheck(phong_uniforms.lightmap_tex				== ref_phong_uniforms.lightmap_tex);
					doCheck(phong_uniforms.emission_tex				== ref_phong_uniforms.emission_tex);
				}
			}
		}
	}
}


void OpenGLEngine::textureBecameUnused(OpenGLTexture* tex)
{
	assert(tex->inserted_into_opengl_textures && tex->m_opengl_engine == this);

	// opengl_textures is not designed to be used by multiple threads (not mutex protected), also making a bindless texture non-resident needs to be done on the main OpenGL context.
	// So if this method is called from off the main thread, add to a queue to process later on the main thread.
	if(PlatformUtils::getCurrentThreadID() == this->initial_thread_id)
	{
		opengl_textures.itemBecameUnused(tex->key);

#if !defined(OSX) && !defined(EMSCRIPTEN)
		// Since this texture is not being used, we can make it non-resident.
		tex->makeNonResidentIfResident();
#endif

		// conPrint("Texture " + tex->key.path + " became unused.");
		// conPrint("textures: " + toString(opengl_textures.numUsedItems()) + " used, " + toString(opengl_textures.numUnusedItems()) + " unused");
	}
	else
	{
		Lock lock(became_unused_tex_keys_mutex);
		became_unused_tex_keys.push_back(tex->key);
	}
}


void OpenGLEngine::textureBecameUsed(const OpenGLTexture* tex)
{
	assert(tex->key != "");
	assert(tex->inserted_into_opengl_textures && tex->m_opengl_engine == this);
	runtimeCheck(PlatformUtils::getCurrentThreadID() == this->initial_thread_id);

	opengl_textures.itemBecameUsed(tex->key);

	// conPrint("Texture " + tex->key.path + " became used.");
	// conPrint("textures: " + toString(opengl_textures.numUsedItems()) + " used, " + toString(opengl_textures.numUnusedItems()) + " unused");
}


static glare::AtomicInt total_gpu_mem_allocated;


void OpenGLEngine::GPUMemAllocated(size_t size)
{
	total_gpu_mem_allocated += size;

	// conPrint("Allocated " + uInt64ToStringCommaSeparated(size) + " B of GPU mem.    Total allocated: " + uInt64ToStringCommaSeparated(total_gpu_mem_allocated));
}


void OpenGLEngine::GPUMemFreed(size_t size)
{
	assert(total_gpu_mem_allocated >= (int64)size);
	total_gpu_mem_allocated -= size;

	// conPrint("Freed " + uInt64ToStringCommaSeparated(size) + " B of GPU mem.    Total allocated: " + uInt64ToStringCommaSeparated(total_gpu_mem_allocated));
}


// glGenTextures() can be really slow, like 6ms, apparently due to doing some kind of flush or synchronisation.  So allocate a bunch of names up-front and just use one from that list.
GLuint OpenGLEngine::allocTextureName()
{
	Lock lock(texture_names_mutex);

	if(texture_names.empty())
	{
		//Timer timer;
		texture_names.resize(2048);
		glGenTextures(2048, texture_names.data());
		//conPrint("\n\n\n*********************** glGenTextures took " + timer.elapsedStringMSWIthNSigFigs());
	}

	GLuint name = texture_names.back();
	texture_names.pop_back();
	return name;
}


size_t OpenGLEngine::getTotalGPUMemAllocated()
{
	return (size_t)total_gpu_mem_allocated;
}


void OpenGLEngine::trimTextureUsage()
{
	ZoneScoped; // Tracy profiler

	// Remove textures from unused texture list until we are using <= max_tex_mem_usage
	while(((tex_CPU_mem_usage > max_tex_CPU_mem_usage) || (tex_GPU_mem_usage > max_tex_GPU_mem_usage)) && (opengl_textures.numUnusedItems() > 0))
	{
		OpenGLTextureKey removed_key;
		OpenGLTextureRef removed_tex;
		const bool removed = opengl_textures.removeLRUUnusedItem(removed_key, removed_tex);
		assert(removed);
		if(removed)
		{
			if(removed_tex->texture_data.nonNull())
			{
				assert(this->tex_CPU_mem_usage >= removed_tex->texture_data->totalCPUMemUsage());
				this->tex_CPU_mem_usage -= removed_tex->texture_data->totalCPUMemUsage();
			}

			assert(this->tex_GPU_mem_usage >= removed_tex->getTotalStorageSizeB());
			this->tex_GPU_mem_usage -= removed_tex->getTotalStorageSizeB();

			removed_tex->inserted_into_opengl_textures = false;
		}
	}
}


void OpenGLEngine::addScene(const Reference<OpenGLScene>& scene)
{
	scenes.insert(scene);
}


void OpenGLEngine::removeScene(const Reference<OpenGLScene>& scene)
{
	scenes.erase(scene);
}


void OpenGLEngine::setCurrentScene(const Reference<OpenGLScene>& scene)
{
	current_scene = scene;
}


//void OpenGLEngine::setMSAAEnabled(bool enabled)
//{
//	if(enabled)
//		glEnable(GL_MULTISAMPLE);
//	else
//		glDisable(GL_MULTISAMPLE);
//}


void OpenGLEngine::setSSAOEnabled(bool ssao_enabled)
{
	settings.ssao = ssao_enabled;

	if(settings.ssao && !compute_ssao_prog)
	{
		const std::string use_shader_dir = data_dir + "/shaders";
		compute_ssao_prog = buildComputeSSAOProg(use_shader_dir);
		blur_ssao_prog = buildBlurSSAOProg(use_shader_dir);
	}
}


bool OpenGLEngine::openglDriverVendorIsIntel() const
{
	return StringUtils::containsString(::toLowerCase(opengl_vendor), "intel");
}


bool OpenGLEngine::openglDriverVendorIsATI() const
{
	return StringUtils::containsString(opengl_vendor, "ATI");
}


void OpenGLEngine::toggleShowTexDebug(int index)
{
	if(large_debug_overlay_ob)
	{
		large_debug_overlay_ob->draw = !large_debug_overlay_ob->draw;

		large_debug_overlay_ob->material.overlay_show_just_tex_rgb = false;
		large_debug_overlay_ob->material.overlay_show_just_tex_w = false;

		if(index == 0)
		{
			// AO
			conPrint("Showing AO");
			large_debug_overlay_ob->material.albedo_texture = this->ssao_texture;
			large_debug_overlay_ob->material.overlay_show_just_tex_w = true;
		}
		else if(index == 1)
		{
			// blurred AO
			conPrint("Showing blurred AO");
			large_debug_overlay_ob->material.albedo_texture = this->blurred_ssao_texture;
			large_debug_overlay_ob->material.overlay_show_just_tex_w = true;
		}
		else if(index == 2)
		{
			// indirect illum
			conPrint("Showing indirect illum");
			large_debug_overlay_ob->material.albedo_texture = this->ssao_texture;
			large_debug_overlay_ob->material.overlay_show_just_tex_rgb = true;
		}
		else if(index == 3)
		{
			// blurred indirect illum
			conPrint("Showing blurred indirect illum");
			large_debug_overlay_ob->material.albedo_texture = this->blurred_ssao_texture;
			large_debug_overlay_ob->material.overlay_show_just_tex_rgb = true;
		}
		else if(index == 4)
		{
			// specular refl
			conPrint("Showing specular");
			large_debug_overlay_ob->material.albedo_texture = this->ssao_specular_texture;
			large_debug_overlay_ob->material.overlay_show_just_tex_rgb = true;
		}
		else if(index == 5)
		{
			// specular refl roughness * trace dist
			conPrint("showing refl roughness * trace dist");
			large_debug_overlay_ob->material.albedo_texture = this->ssao_specular_texture;
			large_debug_overlay_ob->material.overlay_show_just_tex_w = true;

			// blurred specular refl
			//large_debug_overlay_ob->material.albedo_texture = this->blurred_ssao_specular_texture;
		}
	}
}


bool OpenGLEngine::shouldUseSharedTextures() const
{
	// Shared textures seem to crash using Intel drivers.
	return GL_EXT_memory_object_win32_support && GL_EXT_memory_object_win32_support && !openglDriverVendorIsIntel();
}


void OpenGLEngine::shaderFileChanged() // Called by ShaderFileWatcherThread, from another thread.
{
	shader_file_changed = 1;
}


// Render objects with ob->draw_to_mask_map set, with a top-down camera view.
// botleft_pos is the coordinates of the bottom left of the view, and the view has width world_capture_width.
// mask_map_texture must be valid for binding to a framebuffer.
// We will render without depth testing or writing.
// This is used for rendering a vegetation map.
void OpenGLEngine::renderMaskMap(OpenGLTexture& mask_map_texture, const Vec2f& botleft_pos, float world_capture_width)
{
	OpenGLProgram::useNoPrograms(); // Unbind any programs before we start changing framebuffer and z-buffer options.

	// Some other code (e.g. Qt) may have bound some other buffer since the last draw() call.  So reset all this stuff.
	current_index_type = 0;
	current_bound_prog = NULL;
	current_bound_prog_index = std::numeric_limits<uint32>::max();
	current_bound_VAO = NULL;
	current_uniforms_ob = NULL;

	bindStandardTexturesToTextureUnits();

	if(mask_map_frame_buffer.isNull())
	{
		mask_map_frame_buffer = new FrameBuffer();
		mask_map_frame_buffer->attachTexture(mask_map_texture, GL_COLOR_ATTACHMENT0);

		GLenum is_complete = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if(is_complete != GL_FRAMEBUFFER_COMPLETE)
		{
			conPrint("Error: renderMaskMap(): framebuffer is not complete.");
			assert(0);
		}

		// Add overlay quad showing texture, for debugging
		/*{
			OverlayObjectRef tex_preview_overlay_ob =  new OverlayObject();
			tex_preview_overlay_ob->ob_to_world_matrix = Matrix4f::uniformScaleMatrix(0.95f) * Matrix4f::translationMatrix(-1, 0, 0);
			tex_preview_overlay_ob->material.albedo_linear_rgb = Colour3f(1.f);
			tex_preview_overlay_ob->material.albedo_texture = mask_map_tex;
			tex_preview_overlay_ob->mesh_data = this->unit_quad_meshdata;
			addOverlayObject(tex_preview_overlay_ob);
		}*/
	}

	mask_map_frame_buffer->attachTexture(mask_map_texture, GL_COLOR_ATTACHMENT0);

	setSingleDrawBuffer(GL_COLOR_ATTACHMENT0);

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE); // Disable backface culling
	glDepthMask(GL_FALSE); // Don't write to z-buffer, depth not needed.
	
	const Matrix4f reverse_z_matrix = getReverseZMatrixOrIdentity(); // Needed to avoid clipping
	Matrix4f proj_matrix = reverse_z_matrix * orthoMatrix(
		botleft_pos.x, // left
		botleft_pos.x + world_capture_width, // right
		botleft_pos.y, // bottom
		botleft_pos.y + world_capture_width, // top
		-100000.0, // z_near,
		 100000.0  // z_far
	);

	// Because we are rendering from a top-down view, world space coords match view/camera space (ground plane is the x-y plane)
	const Matrix4f view_matrix = Matrix4f::identity();

	const js::AABBox frustum_aabb(
		Vec4f(botleft_pos.x, botleft_pos.y, -100000.0, 1),
		Vec4f(botleft_pos.x + world_capture_width, botleft_pos.y + world_capture_width, 100000.0, 1));

	const Planef clip_planes[4] = { // in world space
		Planef(Vec4f(-1,0,0,0), -botleft_pos.x), // left plane.  Note that dist is along vector (with negative component).
		Planef(Vec4f( 1,0,0,0), botleft_pos.x + world_capture_width), // right
		Planef(Vec4f(0,-1,0,0), -botleft_pos.y), // bottom plane..  Note that dist is along vector (with negative component).
		Planef(Vec4f(0, 1,0,0), botleft_pos.y + world_capture_width) // top
	};

	//================= Update shared vert uniforms buffer =================
	// Just set the uniforms that the outline shaders need. (don't need shadow_texture_matrix)
	{
		SharedVertUniforms uniforms;
		uniforms.proj_matrix = proj_matrix;
		uniforms.view_matrix = view_matrix;

		uniforms.campos_ws = current_scene->cam_to_world.getColumn(3);
		uniforms.vert_sundir_ws = this->sun_dir;
		uniforms.grass_pusher_sphere_pos = current_scene->grass_pusher_sphere_pos;
		uniforms.vert_uniforms_time = current_time;
		uniforms.wind_strength = current_scene->wind_strength;

		this->shared_vert_uniform_buf_ob->updateData(/*dest offset=*/0, &uniforms, sizeof(SharedVertUniforms));;
	}


	// Use outline shader (the shader that draws the flat colour for the object, before edge detection)
	glViewport(0, 0, (int)mask_map_texture.xRes(), (int)mask_map_texture.yRes()); // Make viewport same size as texture.

	const GLObjectRef* const current_scene_obs = current_scene->objects.vector.data();
	const size_t current_scene_obs_size        = current_scene->objects.vector.size();
	for(size_t q=0; q<current_scene_obs_size; ++q)
	{
		// Prefetch cachelines containing the variables we need for objects N places ahead in the array.
		if(q + 16 < current_scene_obs_size)
		{	
			_mm_prefetch((const char*)(&current_scene_obs[q + 16]->aabb_ws), _MM_HINT_T0);
			_mm_prefetch((const char*)(&current_scene_obs[q + 16]->aabb_ws) + 64, _MM_HINT_T0);
		}

		const GLObject* const ob = current_scene_obs[q].ptr();
		if(ob->draw_to_mask_map && AABBIntersectsFrustum(clip_planes, staticArrayNumElems(clip_planes), frustum_aabb, ob->aabb_ws))
		{
			const OpenGLMeshRenderData& mesh_data = *ob->mesh_data;

			const OpenGLProgram* use_prog = mesh_data.usesSkinning() ? outline_prog_with_skinning.ptr() : outline_prog_no_skinning.ptr();

			const bool program_changed = checkUseProgram(use_prog);
			if(program_changed)
				setSharedUniformsForProg(*use_prog, view_matrix, proj_matrix);
			bindMeshData(*ob); // Bind the mesh data, which is the same for all batches.
			for(uint32 z = 0; z < mesh_data.batches.size(); ++z)
				drawBatch(*ob, outline_solid_mat, *use_prog, mesh_data, mesh_data.batches[z], /*batch_index=*/z); // Draw object with outline_mat.
		}
	}


	VAO::unbind(); // Unbind any bound VAO, so that it's vertex and index buffers don't get accidentally overridden.
	OpenGLProgram::useNoPrograms();

	glDepthMask(GL_TRUE); // Restore writing to z-buffer.
	glEnable(GL_DEPTH_TEST); // Restore
	glViewport(0, 0, viewport_w, viewport_h); // Restore viewport
}


static void addMemUsageForTexture(GLMemUsage& usage, const OpenGLTexture* tex)
{
	usage.texture_gpu_usage += tex->getTotalStorageSizeB();

	if(tex->texture_data)
		usage.texture_cpu_usage += tex->texture_data->totalCPUMemUsage();
}


GLMemUsage OpenGLEngine::getTotalMemUsage() const
{
	GLMemUsage sum;

	for(auto it=scenes.begin(); it != scenes.end(); ++it)
	{
		Reference<OpenGLScene> scene = *it;
		sum += scene->getTotalMemUsage();
	}

	//----------- opengl_textures  ----------
	for(auto i = opengl_textures.begin(); i != opengl_textures.end(); ++i)
	{
		const OpenGLTexture* tex = i->second.value.ptr();
		
		addMemUsageForTexture(sum, tex);
	}

	// Get sum memory usage of unused (cached) textures.
	sum.sum_unused_tex_gpu_usage = 0;
	sum.sum_unused_tex_cpu_usage = 0;
	for(auto i = opengl_textures.begin(); i != opengl_textures.end(); ++i) // For all items (used and unused):
	{
		if(!opengl_textures.isItemUsed(i->second)) // If item is not used:
		{
			const OpenGLTexture* tex = i->second.value.ptr();

			const size_t tex_gpu_usage = tex->getTotalStorageSizeB();
			sum.sum_unused_tex_gpu_usage += tex_gpu_usage;

			if(tex->texture_data.nonNull())
				sum.sum_unused_tex_cpu_usage += tex->texture_data->totalCPUMemUsage();
		}
	}

	return sum;
}


std::string OpenGLEngine::getDiagnostics() const
{
	std::string s;
	s += "Objects: " + toString(current_scene->objects.size()) + "\n";
	s += "Transparent objects: " + toString(current_scene->transparent_objects.size()) + "\n";
	s += "Lights: " + toString(current_scene->lights.size()) + "\n";

	s += "Num obs in view frustum: " + toString(last_num_obs_in_frustum) + "\n";
	s += "\n";
	s += "Num prog changes: " + toString(last_num_prog_changes) + "\n";
	s += "Num VAO binds: " + toString(last_num_vao_binds) + "\n";
	s += "Num VBO binds: " + toString(last_num_vbo_binds) + "\n";
	s += "Num face culling changes: " + toString(last_num_face_culling_changes) + "\n";
	s += "Num index buf binds: " + toString(last_num_index_buf_binds) + "\n";
	s += "Num batches bound: " + toString(last_num_batches_bound) + "\n";
	s += "tris drawn: " + uInt32ToStringCommaSeparated(last_num_indices_drawn / 3) + "\n";
	s += "\n";
	s += "Num multi-draw-indirect calls: " + toString(num_multi_draw_indirect_calls) + "\n";
	s += "\n";
	s += "depth draw num prog changes: " + toString(depth_draw_last_num_prog_changes) + "\n";
	s += "depth draw num face culling changes: " + toString(depth_draw_last_num_face_culling_changes) + "\n";
	s += "depth draw num VAO binds: " + toString(depth_draw_last_num_vao_binds) + "\n";
	s += "depth draw num VBO binds: " + toString(depth_draw_last_num_vbo_binds) + "\n";
	s += "depth draw num batches bound: " + toString(depth_draw_last_num_batches_bound) + "\n";
	s += "depth draw tris drawn: " + uInt32ToStringCommaSeparated(depth_draw_last_num_indices_drawn / 3) + "\n";
	s += "\n";
	s += "decal batches drawn: " + toString(last_num_decal_batches_drawn) + "\n";
	s += "\n";

	s += "FPS: " + doubleToStringNDecimalPlaces(last_fps, 1) + "\n";
	s += "last_anim_update_duration: " + doubleToStringNSigFigs(last_anim_update_duration * 1.0e3, 4) + " ms\n";
	s += "Processed " + toString(last_num_animated_obs_processed) + " / " + toString(current_scene->animated_objects.size()) + " animated obs\n";
	s += "draw_CPU_time: " + doubleToStringNSigFigs(last_draw_CPU_time * 1.0e3, 4) + " ms\n"; 
	s += "\n";
	s += "----GPU times----\n";
	s += "dynamic depth draw: " + doubleToStringNSigFigs(last_dynamic_depth_draw_GPU_time * 1.0e3, 4) + " ms\n";
	s += "static depth draw : " + doubleToStringNSigFigs(last_static_depth_draw_GPU_time * 1.0e3, 4) + " ms\n";
	s += "pre-pass          : " + doubleToStringNSigFigs(last_depth_pre_pass_GPU_time * 1.0e3, 4) + " ms\n";
	s += "copy pre-pass bufs: " + doubleToStringNSigFigs(last_copy_prepass_buffers_GPU_time * 1.0e3, 4) + " ms\n";
	s += "compute SSAO      : " + doubleToStringNSigFigs(last_compute_ssao_GPU_time * 1.0e3, 4) + " ms\n";
	s += "blur SSAO         : " + doubleToStringNSigFigs(last_blur_ssao_GPU_time * 1.0e3, 4) + " ms\n";
	s += "draw opaque obs   : " + doubleToStringNSigFigs(last_draw_opaque_obs_GPU_time * 1.0e3, 4) + " ms\n";
	s += "decal copy buffers: " + doubleToStringNSigFigs(last_decal_copy_buffers_GPU_time * 1.0e3, 4) + " ms\n";
	s += "overlay obs       : " + doubleToStringNSigFigs(last_draw_overlay_obs_GPU_time * 1.0e3, 4) + " ms\n";
	s += "total             : " + doubleToStringNSigFigs(last_total_draw_GPU_time * 1.0e3, 4) + " ms\n";
	s += "\n";

	s += "Total GPU mem usage: " + getMBSizeString(this->getTotalGPUMemAllocated()) + "\n";

	const GLMemUsage mem_usage = this->getTotalMemUsage();
	s += "geometry CPU mem usage: " + getNiceByteSize(mem_usage.geom_cpu_usage) + "\n";
	s += "geometry GPU mem usage: " + getNiceByteSize(mem_usage.geom_gpu_usage) + "\n";

	const size_t tex_usage_GPU_active = mem_usage.texture_gpu_usage - mem_usage.sum_unused_tex_gpu_usage;
	const size_t tex_usage_CPU_active = mem_usage.texture_cpu_usage - mem_usage.sum_unused_tex_cpu_usage;

	s += "Textures: " + toString(opengl_textures.numUsedItems()) + " / " + toString(opengl_textures.numUnusedItems()) + " / " + toString(opengl_textures.size()) + " (active / cached / total)\n";
	s += "Textures CPU mem: " + getMBSizeString(tex_usage_CPU_active) + " / " + getMBSizeString(mem_usage.sum_unused_tex_cpu_usage) + " / " + getMBSizeString(mem_usage.texture_cpu_usage) + " (active / cached / total)\n";
	s += "Textures GPU mem: " + getMBSizeString(tex_usage_GPU_active) + " / " + getMBSizeString(mem_usage.sum_unused_tex_gpu_usage) + " / " + getMBSizeString(mem_usage.texture_gpu_usage) + " (active / cached / total)\n";

	
	if(false)
	{
		// Print out textures with mem usage.
		s += "------------------------ texture GPU usage ------------------------\n";
		for(auto i = opengl_textures.begin(); i != opengl_textures.end(); ++i)
		{
			const OpenGLTexture* tex = i->second.value.ptr();
		
			s += ::rightSpacePad(FileUtils::getFilename(std::string(tex->key)).substr(0, 40) + ": ", 40) + uInt32ToStringCommaSeparated((uint32)tex->getTotalStorageSizeB()) + " B\n";
		}
		s += "------------------------------------------------------------------\n";
	}

	s += "num bindless resident: " + toString(OpenGLTexture::getNumResidentTextures()) + "\n";

	//s += "---- VBO pool ----\n";
	//s += getMBSizeString(vbo_pool.getAllocatedSpace()) + " / " + getMBSizeString(vbo_pool.getFreeSpace()) + " / " + getMBSizeString(vbo_pool.getAllocatedSpace() + vbo_pool.getFreeSpace()) + " (used / free / total)\n";
	//s += "---- PBO pool ----\n";
	//s += getMBSizeString(pbo_pool.getAllocatedSpace()) + " / " + getMBSizeString(pbo_pool.getFreeSpace()) + " / " + getMBSizeString(pbo_pool.getAllocatedSpace() + pbo_pool.getFreeSpace()) + " (used / free / total)\n";
	
	if(per_ob_vert_data_buffer.nonNull())
		s += "per-ob vert buf size: " + toString(per_ob_vert_data_buffer->byteSize() / sizeof(PerObjectVertUniforms)) + " (" + getNiceByteSize(per_ob_vert_data_buffer->byteSize()) + ")\n";
	if(phong_buffer.nonNull())
		s += "phong_buffer size: " + toString(phong_buffer->byteSize() / sizeof(PhongUniforms)) + " (" + getNiceByteSize(phong_buffer->byteSize()) + ")\n";
	if(joint_matrices_ssbo.nonNull())
		s += "joint matrices size: " + toString(joint_matrices_ssbo->byteSize() / sizeof(Matrix4f)) + " (" + getNiceByteSize(joint_matrices_ssbo->byteSize()) + "\n";
	
	s += "OpenGL vendor: " + opengl_vendor + "\n";
	s += "OpenGL renderer: " + opengl_renderer + "\n";
	s += "OpenGL version: " + opengl_version + "\n";
	s += "GLSL version: " + glsl_version + "\n";
	s += "texture s3tc support: " + boolToString(texture_compression_s3tc_support) + "\n";
	s += "texture ETC support: " + boolToString(texture_compression_ETC_support) + "\n";
	s += "texture BC6H support: " + boolToString(texture_compression_BC6H_support) + "\n";
	s += "GL_KHR_parallel_shader_compile: " + boolToString(parallel_shader_compile_support) + "\n";
#if EMSCRIPTEN
	s += "EXT_color_buffer_float_support: " + boolToString(EXT_color_buffer_float_support) + "\n";
#endif
	s += "max_texture_size: " + toString(max_texture_size) + "\n";
	s += "using bindless textures: " + boolToString(use_bindless_textures) + "\n";
	s += "using multi-draw-indirect: " + boolToString(use_multi_draw_indirect) + "\n";
	s += "using ob and mat data GPU-resident: " + boolToString(use_ob_and_mat_data_gpu_resident) + "\n";
	s += "using reverse z: " + boolToString(use_reverse_z) + "\n";
	s += "SSBO support: " + boolToString(GL_ARB_shader_storage_buffer_object_support) + "\n";
	s += "total available GPU mem (nvidia): " + getNiceByteSize(total_available_GPU_mem_B) + "\n";
	s += "total available GPU VBO mem (amd): " + getNiceByteSize(total_available_GPU_VBO_mem_B) + "\n";
	s += "settings.ssao: " + boolToString(settings.ssao) + "\n";

	if(main_colour_renderbuffer.nonNull())
		s += "main_colour_renderbuffer (offscreen): " + toString(main_colour_renderbuffer->xRes()) + " x " + toString(main_colour_renderbuffer->yRes()) + ", MSAA samples: " + toString(main_colour_renderbuffer->MSAASamples()) + "\n";
	else
		s += "main_colour_renderbuffer (offscreen): Not used\n";
	s += "main viewport dims: " + toString(main_viewport_w) + " x " + toString(main_viewport_h) + ", MSAA samples: " + toString(settings.msaa_samples) + "\n";

	s += "Programs: " + toString(next_program_index) + "\n";

	s += vert_buf_allocator->getDiagnostics();

	return s;
}


void OpenGLEngine::setProfilingEnabled(bool enable)
{
	if(enable)
	{
#if defined(OSX)
		// Queries not supported
		conPrint("GPU profiling not available on Mac.");
#elif EMSCRIPTEN
		if(EXT_disjoint_timer_query_webgl2_support)
			query_profiling_enabled = true;
		else
			conPrint("GPU profiling not available in Emscripten without EXT_disjoint_timer_query_webgl2 support.");
#else
		query_profiling_enabled = true;
#endif

		if(query_profiling_enabled)
			checkCreateProfilingQueries();
	}
	else
	{
		query_profiling_enabled = false;
	}
}


static std::string errorCodeString(GLenum code)
{
	switch(code)
	{
		case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
		case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
		case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
		case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
		case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
		//case GL_STACK_UNDERFLOW: return "GL_STACK_UNDERFLOW";
		//case GL_STACK_OVERFLOW: return "GL_STACK_OVERFLOW";
		default: return "[Unknown]";
	}
}


void doCheckForOpenGLErrorsAtLocation(long line, const char* file)
{
	GLenum code = glGetError();
	if(code != GL_NO_ERROR)
	{
		conPrint("OpenGL error detected at " + std::string(file) + ", line " + toString((int)line) + ": " + errorCodeString(code));
		assert(0);
	}
}


void checkForOpenGLErrors()
{
	GLenum code = glGetError();
	if(code != GL_NO_ERROR)
	{
		conPrint("OpenGL error detected: " + errorCodeString(code));
		assert(0);
	}
}
