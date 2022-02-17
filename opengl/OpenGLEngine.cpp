/*=====================================================================
OpenGLEngine.cpp
----------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "OpenGLEngine.h"


#include "OpenGLProgram.h"
#include "OpenGLShader.h"
#include "ShadowMapping.h"
#include "TextureLoading.h"
#include "GLMeshBuilding.h"
#include "MeshPrimitiveBuilding.h"
//#include "TerrainSystem.h"
#include "../dll/include/IndigoMesh.h"
#include "../graphics/ImageMap.h"
#include "../graphics/imformatdecoder.h"
#include "../graphics/SRGBUtils.h"
#include "../graphics/BatchedMesh.h"
#include "../graphics/CompressedImage.h"
#include "../graphics/PerlinNoise.h"
#include "../graphics/Voronoi.h"
#include "../graphics/EXRDecoder.h"
#include "../indigo/globals.h"
#include "../indigo/TextureServer.h"
#include "../utils/TestUtils.h"
#include "../maths/vec3.h"
#include "../maths/GeometrySampling.h"
#include "../maths/matrix3.h"
#include "../utils/Lock.h"
#include "../utils/Mutex.h"
#include "../utils/Clock.h"
#include "../utils/Timer.h"
#include "../utils/Platform.h"
#include "../utils/FileUtils.h"
#include "../utils/Reference.h"
#include "../utils/StringUtils.h"
#include "../utils/Exception.h"
#include "../utils/BitUtils.h"
#include "../utils/Vector.h"
#include "../utils/Task.h"
#include "../utils/IncludeXXHash.h"
#include "../utils/ArrayRef.h"
#include "../utils/HashMapInsertOnly2.h"
#include "../utils/IncludeHalf.h"
#include "../utils/TaskManager.h"
#include "../utils/Sort.h"
#include <algorithm>


static const bool MEM_PROFILE = false;


// https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_filter_anisotropic.txt
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT						0x84FF
#define GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT					0x8E8F

// See https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_compression_s3tc.txt
#define GL_EXT_COMPRESSED_RGB_S3TC_DXT1_EXT						0x83F0
#define GL_EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT					0x83F3


void GLObject::enableInstancing(const VBORef new_instance_matrix_vbo)
{
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

	this->vert_vao = new VAO(mesh_data->vert_vbo, vertex_spec);
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


GLMemUsage OpenGLMeshRenderData::getTotalMemUsage() const
{
	GLMemUsage usage;
	usage.geom_cpu_usage =
		vert_data.capacitySizeBytes() +
		vert_index_buffer.capacitySizeBytes() +
		vert_index_buffer_uint16.capacitySizeBytes() +
		vert_index_buffer_uint8.capacitySizeBytes() +
		vert_index_buffer_uint8.capacitySizeBytes() +
		(batched_mesh.nonNull() ? batched_mesh->getTotalMemUsage() : 0);

	usage.geom_gpu_usage = 
		(vert_vbo.nonNull() ? vert_vbo->getSize() : 0) +
		(vert_indices_buf.nonNull() ? vert_indices_buf->getSize() : 0);

	return usage;
}


size_t OpenGLMeshRenderData::GPUVertMemUsage() const
{
	return vert_vbo.nonNull() ? vert_vbo->getSize() : 0;
}


size_t OpenGLMeshRenderData::GPUIndicesMemUsage() const
{
	return vert_indices_buf.nonNull() ? vert_indices_buf->getSize() : 0;
}


size_t OpenGLMeshRenderData::getNumVerts() const
{
	if(!vertex_spec.attributes.empty() && (vertex_spec.attributes[0].stride > 0))
	{
		if(!vert_data.empty())
			return vert_data.size() / vertex_spec.attributes[0].stride;
		else if(vert_vbo.nonNull())
			return vert_vbo->getSize() / vertex_spec.attributes[0].stride;
	}
	return 0;
}


size_t OpenGLMeshRenderData::getNumTris() const
{
	if(!vert_index_buffer.empty())
		return vert_index_buffer.size() / 3;
	else if(!vert_index_buffer_uint16.empty())
		return vert_index_buffer_uint16.size() / 3;
	else if(!vert_index_buffer_uint8.empty())
		return vert_index_buffer_uint8.size() / 3;
	else if(vert_indices_buf.nonNull())
	{
		const size_t index_type_size = index_type == GL_UNSIGNED_INT ? 4 : (index_type == GL_UNSIGNED_SHORT ? 2 : 1);
		return vert_indices_buf->getSize() / index_type_size / 3;
	}
	else
		return 0;
}


OverlayObject::OverlayObject()
{
	material.albedo_rgb = Colour3f(1.f);
}


OpenGLScene::OpenGLScene()
{
	max_draw_dist = 1000;
	near_draw_dist = 0.22f;
	render_aspect_ratio = 1;
	lens_shift_up_distance = 0;
	lens_shift_right_distance = 0;
	camera_type = OpenGLScene::CameraType_Perspective;
	background_colour = Colour3f(0.1f);
	use_z_up = true;
	dest_blending_factor = GL_ONE_MINUS_SRC_ALPHA;
	bloom_strength = 0;
	wind_strength = 1.f;

	env_ob = new GLObject();
	env_ob->ob_to_world_matrix = Matrix4f::identity();
	env_ob->ob_to_world_inv_transpose_matrix = Matrix4f::identity();
	env_ob->materials.resize(1);
}


OpenGLEngine::OpenGLEngine(const OpenGLEngineSettings& settings_)
:	init_succeeded(false),
	settings(settings_),
	draw_wireframes(false),
	frame_num(0),
	current_time(0.f),
	task_manager(NULL),
	texture_server(NULL),
	outline_colour(0.43f, 0.72f, 0.95f, 1.0),
	are_8bit_textures_sRGB(true),
	outline_tex_w(0),
	outline_tex_h(0),
	last_num_obs_in_frustum(0),
	print_output(NULL),
	tex_mem_usage(0),
	last_anim_update_duration(0),
	last_depth_map_gen_GPU_time(0),
	last_render_GPU_time(0),
	profiling_enabled(false)
{
	current_scene = new OpenGLScene();
	scenes.insert(current_scene);

	viewport_w = viewport_h = 100;

	main_viewport_w = 0;
	main_viewport_h = 0;

	sun_dir = normalise(Vec4f(0.2f,0.2f,1,0));

	texture_data_manager = new TextureDataManager();
	
	max_tex_mem_usage = settings.max_tex_mem_usage;

}


OpenGLEngine::~OpenGLEngine()
{
	// The textures may outlast the OpenGLEngine, so null out the pointer to the opengl engine.
	for(auto it = opengl_textures.begin(); it != opengl_textures.end(); ++it)
		it->second.value->m_opengl_engine = NULL;
	opengl_textures.clear();

	delete task_manager;
}


static const Vec4f FORWARDS_OS(0.0f, 1.0f, 0.0f, 0.0f); // Forwards in local camera (object) space.
static const Vec4f UP_OS(0.0f, 0.0f, 1.0f, 0.0f);
static const Vec4f RIGHT_OS(1.0f, 0.0f, 0.0f, 0.0f);


void OpenGLScene::setPerspectiveCameraTransform(const Matrix4f& world_to_camera_space_matrix_, float sensor_width_, float lens_sensor_dist_, float render_aspect_ratio_, float lens_shift_up_distance_,
	float lens_shift_right_distance_, float viewport_aspect_ratio)
{
	camera_type = OpenGLScene::CameraType_Perspective;

	this->sensor_width = sensor_width_;
	this->world_to_camera_space_matrix = world_to_camera_space_matrix_;
	this->lens_sensor_dist = lens_sensor_dist_;
	this->render_aspect_ratio = render_aspect_ratio_;
	this->lens_shift_up_distance = lens_shift_up_distance_;
	this->lens_shift_right_distance = lens_shift_right_distance_;

	if(viewport_aspect_ratio > render_aspect_ratio) // if viewport has a wider aspect ratio than the render:
	{
		use_sensor_height = sensor_width / render_aspect_ratio; // Keep vertical field of view the same
		use_sensor_width = use_sensor_height * viewport_aspect_ratio; // enlarge horizontal field of view as needed
	}
	else
	{
		use_sensor_width = sensor_width; // Keep horizontal field of view the same
		use_sensor_height = sensor_width / viewport_aspect_ratio; // Enlarge vertical field of view as needed
	}


	// Make clipping planes
	const Vec4f lens_center(lens_shift_right_distance, 0, lens_shift_up_distance, 1);
	const Vec4f sensor_center(0, -lens_sensor_dist, 0, 1);

	const Vec4f sensor_bottom = (sensor_center - UP_OS    * (use_sensor_height * 0.5f));
	const Vec4f sensor_top    = (sensor_center + UP_OS    * (use_sensor_height * 0.5f));
	const Vec4f sensor_left   = (sensor_center - RIGHT_OS * (use_sensor_width  * 0.5f));
	const Vec4f sensor_right  = (sensor_center + RIGHT_OS * (use_sensor_width  * 0.5f));

	Planef planes_cs[5]; // Clipping planes in camera space

	// We won't bother with a culling plane at the near clip plane, since it near draw distance is so close to zero, so it's unlikely to cull any objects.

	planes_cs[0] = Planef(
		Vec4f(0,0,0,1) + FORWARDS_OS * this->max_draw_dist, // pos.
		FORWARDS_OS // normal
	); // far clip plane of frustum

	const Vec4f left_normal = normalise(crossProduct(UP_OS, lens_center - sensor_right));
	planes_cs[1] = Planef(
		lens_center,
		left_normal
	); // left

	const Vec4f right_normal = normalise(crossProduct(lens_center - sensor_left, UP_OS));
	planes_cs[2] = Planef(
		lens_center,
		right_normal
	); // right

	const Vec4f bottom_normal = normalise(crossProduct(lens_center - sensor_top, RIGHT_OS));
	planes_cs[3] = Planef(
		lens_center,
		bottom_normal
	); // bottom

	const Vec4f top_normal = normalise(crossProduct(RIGHT_OS, lens_center - sensor_bottom));
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
	const float shift_right_d = lens_shift_right_distance * (max_draw_dist / lens_sensor_dist); // distance verts at far end of frustum are shifted up

	const float d_w = use_sensor_width  * max_draw_dist / (2 * lens_sensor_dist);
	const float d_h = use_sensor_height * max_draw_dist / (2 * lens_sensor_dist);

	verts_cs[0] = lens_center;
	verts_cs[1] = lens_center + FORWARDS_OS * max_draw_dist + RIGHT_OS * (-d_w + shift_right_d) + UP_OS * (-d_h + shift_up_d); // bottom left
	verts_cs[2] = lens_center + FORWARDS_OS * max_draw_dist + RIGHT_OS * ( d_w + shift_right_d) + UP_OS * (-d_h + shift_up_d); // bottom right
	verts_cs[3] = lens_center + FORWARDS_OS * max_draw_dist + RIGHT_OS * ( d_w + shift_right_d) + UP_OS * ( d_h + shift_up_d); // top right
	verts_cs[4] = lens_center + FORWARDS_OS * max_draw_dist + RIGHT_OS * (-d_w + shift_right_d) + UP_OS * ( d_h + shift_up_d); // top left

	frustum_aabb = js::AABBox::emptyAABBox();
	for(int i=0; i<5; ++i)
	{
		assert(verts_cs[i][3] == 1.f);
		frustum_verts[i] = cam_to_world * verts_cs[i];
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
void OpenGLScene::calcCamFrustumVerts(float near_dist, float far_dist, Vec4f* verts_out)
{
	const Vec4f lens_center_cs(lens_shift_right_distance, 0, lens_shift_up_distance, 1);
	const Vec4f lens_center_ws = cam_to_world * lens_center_cs;

	const Vec4f forwards_ws = cam_to_world * FORWARDS_OS;
	const Vec4f up_ws = cam_to_world * UP_OS;
	const Vec4f right_ws = cam_to_world * RIGHT_OS;

	if(camera_type == OpenGLScene::CameraType_Perspective)
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


void OpenGLEngine::calcCamFrustumVerts(float near_dist, float far_dist, Vec4f* verts_out)
{
	current_scene->calcCamFrustumVerts(near_dist, far_dist, verts_out);
}


void OpenGLScene::setOrthoCameraTransform(const Matrix4f& world_to_camera_space_matrix_, float sensor_width_, float render_aspect_ratio_, float lens_shift_up_distance_,
	float lens_shift_right_distance_, float viewport_aspect_ratio)
{
	camera_type = CameraType_Orthographic;

	this->sensor_width = sensor_width_;
	this->world_to_camera_space_matrix = world_to_camera_space_matrix_;
	this->render_aspect_ratio = render_aspect_ratio_;
	this->lens_shift_up_distance = lens_shift_up_distance_;
	this->lens_shift_right_distance = lens_shift_right_distance_;

	if(viewport_aspect_ratio > render_aspect_ratio) // if viewport has a wider aspect ratio than the render:
	{
		use_sensor_height = sensor_width / render_aspect_ratio; // Keep vertical field of view the same
		use_sensor_width = use_sensor_height * viewport_aspect_ratio; // enlarge horizontal field of view as needed
	}
	else
	{
		use_sensor_width = sensor_width; // Keep horizontal field of view the same
		use_sensor_height = sensor_width / viewport_aspect_ratio; // Enlarge vertical field of view as needed
	}


	// Make camera view volume clipping planes
	const Vec4f lens_center(lens_shift_right_distance, 0, lens_shift_up_distance, 1);

	Planef planes_cs[6]; // Clipping planes in camera space

	planes_cs[0] = Planef(
		Vec4f(0,0,0,1), // pos.
		-FORWARDS_OS // normal
	); // near clip plane of frustum

	planes_cs[1] = Planef(
		Vec4f(0, 0, 0, 1) + FORWARDS_OS * this->max_draw_dist, // pos.
		FORWARDS_OS // normal
	); // far clip plane of frustum

	const Vec4f left_normal = -RIGHT_OS;
	planes_cs[2] = Planef(
		lens_center + left_normal * use_sensor_width/2,
		left_normal
	); // left

	const Vec4f right_normal = RIGHT_OS;
	planes_cs[3] = Planef(
		lens_center + right_normal * use_sensor_width/2,
		right_normal
	); // right

	const Vec4f bottom_normal = -UP_OS;
	planes_cs[4] = Planef(
		lens_center + bottom_normal * use_sensor_height/2,
		bottom_normal
	); // bottom

	const Vec4f top_normal = UP_OS;
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
	verts_cs[0] = lens_center                               + RIGHT_OS * -use_sensor_width + UP_OS * -use_sensor_height; // bottom left
	verts_cs[1] = lens_center                               + RIGHT_OS *  use_sensor_width + UP_OS * -use_sensor_height; // bottom right
	verts_cs[2] = lens_center                               + RIGHT_OS *  use_sensor_width + UP_OS *  use_sensor_height; // top right
	verts_cs[3] = lens_center                               + RIGHT_OS * -use_sensor_width + UP_OS *  use_sensor_height; // top left
	verts_cs[4] = lens_center + FORWARDS_OS * max_draw_dist + RIGHT_OS * -use_sensor_width + UP_OS * -use_sensor_height; // bottom left
	verts_cs[5] = lens_center + FORWARDS_OS * max_draw_dist + RIGHT_OS *  use_sensor_width + UP_OS * -use_sensor_height; // bottom right
	verts_cs[6] = lens_center + FORWARDS_OS * max_draw_dist + RIGHT_OS *  use_sensor_width + UP_OS *  use_sensor_height; // top right
	verts_cs[7] = lens_center + FORWARDS_OS * max_draw_dist + RIGHT_OS * -use_sensor_width + UP_OS *  use_sensor_height; // top left

	frustum_aabb = js::AABBox::emptyAABBox();
	for(int i=0; i<8; ++i)
	{
		assert(verts_cs[i][3] == 1.f);
		frustum_verts[i] = cam_to_world * verts_cs[i];
		const Vec4f frustum_vert_ws = cam_to_world * verts_cs[i];
		//conPrint("frustum_verts " + toString(i) + ": " + frustum_verts[i].toString());
		frustum_aabb.enlargeToHoldPoint(frustum_vert_ws);
	}
}


void OpenGLScene::setDiagonalOrthoCameraTransform(const Matrix4f& world_to_camera_space_matrix_, float sensor_width_, float render_aspect_ratio_, float viewport_aspect_ratio)
{
	camera_type = CameraType_DiagonalOrthographic;

	this->sensor_width = sensor_width_;
	this->world_to_camera_space_matrix = world_to_camera_space_matrix_;
	this->render_aspect_ratio = render_aspect_ratio_;
	this->lens_shift_up_distance = 0;
	this->lens_shift_right_distance = 0;

	if(viewport_aspect_ratio > render_aspect_ratio) // if viewport has a wider aspect ratio than the render:
	{
		use_sensor_height = sensor_width / render_aspect_ratio; // Keep vertical field of view the same
		use_sensor_width = use_sensor_height * viewport_aspect_ratio; // enlarge horizontal field of view as needed
	}
	else
	{
		use_sensor_width = sensor_width; // Keep horizontal field of view the same
		use_sensor_height = sensor_width / viewport_aspect_ratio; // Enlarge vertical field of view as needed
	}

	//TEMP HACK: These camera clipping planes are completely wrong for diagonal ortho.
	use_sensor_width *= 2;
	use_sensor_height *= 2;

	// Make camera view volume clipping planes
	const Vec4f lens_center(lens_shift_right_distance, 0, lens_shift_up_distance, 1);

	Planef planes_cs[6]; // Clipping planes in camera space

	planes_cs[0] = Planef(
		Vec4f(0,0,0,1), // pos.
		-FORWARDS_OS // normal
	); // near clip plane of frustum

	planes_cs[1] = Planef(
		Vec4f(0, 0, 0, 1) + FORWARDS_OS * this->max_draw_dist, // pos.
		FORWARDS_OS // normal
	); // far clip plane of frustum

	const Vec4f left_normal = -RIGHT_OS;
	planes_cs[2] = Planef(
		lens_center + left_normal * use_sensor_width/2,
		left_normal
	); // left

	const Vec4f right_normal = RIGHT_OS;
	planes_cs[3] = Planef(
		lens_center + right_normal * use_sensor_width/2,
		right_normal
	); // right

	const Vec4f bottom_normal = -UP_OS;
	planes_cs[4] = Planef(
		lens_center + bottom_normal * use_sensor_height/2,
		bottom_normal
	); // bottom

	const Vec4f top_normal = UP_OS;
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
	verts_cs[0] = lens_center                               + RIGHT_OS * -use_sensor_width + UP_OS * -use_sensor_height; // bottom left
	verts_cs[1] = lens_center                               + RIGHT_OS *  use_sensor_width + UP_OS * -use_sensor_height; // bottom right
	verts_cs[2] = lens_center                               + RIGHT_OS *  use_sensor_width + UP_OS *  use_sensor_height; // top right
	verts_cs[3] = lens_center                               + RIGHT_OS * -use_sensor_width + UP_OS *  use_sensor_height; // top left
	verts_cs[4] = lens_center + FORWARDS_OS * max_draw_dist + RIGHT_OS * -use_sensor_width + UP_OS * -use_sensor_height; // bottom left
	verts_cs[5] = lens_center + FORWARDS_OS * max_draw_dist + RIGHT_OS *  use_sensor_width + UP_OS * -use_sensor_height; // bottom right
	verts_cs[6] = lens_center + FORWARDS_OS * max_draw_dist + RIGHT_OS *  use_sensor_width + UP_OS *  use_sensor_height; // top right
	verts_cs[7] = lens_center + FORWARDS_OS * max_draw_dist + RIGHT_OS * -use_sensor_width + UP_OS *  use_sensor_height; // top left

	frustum_aabb = js::AABBox::emptyAABBox();
	for(int i=0; i<8; ++i)
	{
		assert(verts_cs[i][3] == 1.f);
		frustum_verts[i] = cam_to_world * verts_cs[i];
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


void OpenGLEngine::setSunDir(const Vec4f& d)
{
	this->sun_dir = d;
}


const Vec4f OpenGLEngine::getSunDir() const
{
	return this->sun_dir;
}


void OpenGLEngine::setEnvMapTransform(const Matrix3f& transform)
{
	this->current_scene->env_ob->ob_to_world_matrix = Matrix4f(transform, Vec3f(0.f));
	this->current_scene->env_ob->ob_to_world_matrix.getUpperLeftInverseTranspose(this->current_scene->env_ob->ob_to_world_inv_transpose_matrix);
}


void OpenGLEngine::setEnvMat(const OpenGLMaterial& env_mat_)
{
	this->current_scene->env_ob->materials[0] = env_mat_;
	if(this->current_scene->env_ob->materials[0].shader_prog.isNull())
		this->current_scene->env_ob->materials[0].shader_prog = env_prog;
}


void OpenGLEngine::setCirrusTexture(const Reference<OpenGLTexture>& tex)
{
	this->cirrus_tex = tex;
}


// Define some constants not defined on Mac for some reason.
// From https://www.khronos.org/registry/OpenGL/api/GL/glext.h
#ifndef GL_DEBUG_TYPE_ERROR
#define GL_DEBUG_TYPE_ERROR               0x824C
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR 0x824D
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR  0x824E
#define GL_DEBUG_TYPE_PORTABILITY         0x824F
#define GL_DEBUG_TYPE_PERFORMANCE         0x8250
#define GL_DEBUG_TYPE_OTHER               0x8251
#define GL_MAX_DEBUG_MESSAGE_LENGTH       0x9143
#define GL_MAX_DEBUG_LOGGED_MESSAGES      0x9144
#define GL_DEBUG_LOGGED_MESSAGES          0x9145
#define GL_DEBUG_SEVERITY_HIGH            0x9146
#define GL_DEBUG_SEVERITY_MEDIUM          0x9147
#define GL_DEBUG_SEVERITY_LOW             0x9148
#define GL_DEBUG_TYPE_MARKER              0x8268
#define GL_DEBUG_TYPE_PUSH_GROUP          0x8269
#define GL_DEBUG_TYPE_POP_GROUP           0x826A
#define GL_DEBUG_SEVERITY_NOTIFICATION    0x826B

#define GL_DEBUG_OUTPUT                   0x92E0
#endif

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
		if(engine->print_output)
			engine->print_output->print("OpenGL Engine: severity: " + severitystr + ", type: " + typestr + ", message: " + message);
	}
}





void OpenGLEngine::getUniformLocations(Reference<OpenGLProgram>& prog, bool shadow_mapping_enabled, UniformLocations& locations_out)
{
	locations_out.diffuse_colour_location			= prog->getUniformLocation("diffuse_colour");
	locations_out.have_shading_normals_location		= prog->getUniformLocation("have_shading_normals");
	locations_out.have_texture_location				= prog->getUniformLocation("have_texture");
	locations_out.diffuse_tex_location				= prog->getUniformLocation("diffuse_tex");
	locations_out.metallic_roughness_tex_location	= prog->getUniformLocation("metallic_roughness_tex");
	locations_out.backface_diffuse_tex_location		= prog->getUniformLocation("backface_diffuse_tex");
	locations_out.transmission_tex_location			= prog->getUniformLocation("transmission_tex");
	locations_out.cosine_env_tex_location			= prog->getUniformLocation("cosine_env_tex");
	locations_out.specular_env_tex_location			= prog->getUniformLocation("specular_env_tex");
	locations_out.lightmap_tex_location				= prog->getUniformLocation("lightmap_tex");
	locations_out.fbm_tex_location					= prog->getUniformLocation("fbm_tex");
	locations_out.blue_noise_tex_location			= prog->getUniformLocation("blue_noise_tex");
	locations_out.texture_matrix_location			= prog->getUniformLocation("texture_matrix");
	locations_out.sundir_cs_location				= prog->getUniformLocation("sundir_cs");
	locations_out.campos_ws_location				= prog->getUniformLocation("campos_ws");
	
	if(shadow_mapping_enabled)
	{
		locations_out.dynamic_depth_tex_location		= prog->getUniformLocation("dynamic_depth_tex");
		locations_out.static_depth_tex_location			= prog->getUniformLocation("static_depth_tex");
		locations_out.shadow_texture_matrix_location	= prog->getUniformLocation("shadow_texture_matrix");
	}

	locations_out.proj_view_model_matrix_location	= prog->getUniformLocation("proj_view_model_matrix");

	locations_out.num_blob_positions_location		= prog->getUniformLocation("num_blob_positions");
	locations_out.blob_positions_location			= prog->getUniformLocation("blob_positions");
}


struct BuildFBMNoiseTask : public glare::Task
{
	virtual void run(size_t /*thread_index*/)
	{
		js::Vector<float, 16>& data_ = *data;
		for(int y=begin_y; y<end_y; ++y)
		for(int x=0; x<W; ++x)
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

	js::Vector<float, 16>* data;
	size_t W;
	int begin_y, end_y;
};



static const int FINAL_IMAGING_BLOOM_STRENGTH_UNIFORM_INDEX = 0;
static const int FINAL_IMAGING_BLUR_TEX_UNIFORM_START = 1;

static const int NUM_BLUR_DOWNSIZES = 8;

void OpenGLEngine::initialise(const std::string& data_dir_, TextureServer* texture_server_, PrintOutput* print_output_)
{
	data_dir = data_dir_;
	texture_server = texture_server_;
	print_output = print_output_;

#if !defined(OSX)
	if(gl3wInit() != 0)
	{
		conPrint("gl3wInit failed.");
		init_succeeded = false;
		initialisation_error_msg = "gl3wInit failed.";
		return;
	}
#endif

	this->opengl_vendor		= std::string((const char*)glGetString(GL_VENDOR));
	this->opengl_renderer	= std::string((const char*)glGetString(GL_RENDERER));
	this->opengl_version	= std::string((const char*)glGetString(GL_VERSION));
	this->glsl_version		= std::string((const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

#if !defined(OSX)
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
#if !defined(OSX) // Apple doesn't seem to support glDebugMessageCallback: (https://stackoverflow.com/questions/24836127/gldebugmessagecallback-on-osx-xcode-5)
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

	this->GL_EXT_texture_sRGB_support = false;
	this->GL_EXT_texture_compression_s3tc_support = false;

	// Check OpenGL extensions
	GLint n = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &n);
	for(GLint i = 0; i < n; i++)
	{
		const char* ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
		if(stringEqual(ext, "GL_EXT_texture_sRGB")) this->GL_EXT_texture_sRGB_support = true;
		if(stringEqual(ext, "GL_EXT_texture_compression_s3tc")) this->GL_EXT_texture_compression_s3tc_support = true;
		// if(stringEqual(ext, "GL_ARB_texture_compression_bptc")) conPrint("GL_ARB_texture_compression_bptc supported");
	}
	

	// Init TextureLoading (in particular stb_compress_dxt lib) before it's called from multiple threads
	TextureLoading::init();


	// Set up the rendering context, define display lists etc.:
	glClearColor(current_scene->background_colour.r, current_scene->background_colour.g, current_scene->background_colour.b, 1.f);

	glEnable(GL_DEPTH_TEST);	// Enable z-buffering
	glDisable(GL_CULL_FACE);	// Disable backface culling

#if !defined(OSX)
	if(profiling_enabled)
		glGenQueries(1, &timer_query_id);
#endif

	this->sphere_meshdata = MeshPrimitiveBuilding::makeSphereMesh();
	this->line_meshdata = MeshPrimitiveBuilding::makeLineMesh();
	this->cube_meshdata = MeshPrimitiveBuilding::makeCubeMesh();
	this->unit_quad_meshdata = MeshPrimitiveBuilding::makeUnitQuadMesh();

	this->current_scene->env_ob->mesh_data = sphere_meshdata;

	const std::string gl_data_dir = data_dir + "/gl_data";

	try
	{
		// On OS X, we can't just not define things, we need to define them as zero or we get GLSL syntax errors.
		preprocessor_defines += "#define SHADOW_MAPPING " + (settings.shadow_mapping ? std::string("1") : std::string("0")) + "\n";
		preprocessor_defines += "#define NUM_DEPTH_TEXTURES " + (settings.shadow_mapping ? 
			toString(shadow_mapping->numDynamicDepthTextures() + shadow_mapping->numStaticDepthTextures()) : std::string("0")) + "\n";
		
		preprocessor_defines += "#define NUM_DYNAMIC_DEPTH_TEXTURES " + (settings.shadow_mapping ? 
			toString(shadow_mapping->numDynamicDepthTextures()) : std::string("0")) + "\n";
		
		preprocessor_defines += "#define NUM_STATIC_DEPTH_TEXTURES " + (settings.shadow_mapping ? 
			toString(shadow_mapping->numStaticDepthTextures()) : std::string("0")) + "\n";

		preprocessor_defines += "#define DEPTH_TEXTURE_SCALE_MULT " + (settings.shadow_mapping ? toString(shadow_mapping->getDynamicDepthTextureScaleMultiplier()) : std::string("1.0")) + "\n";

		preprocessor_defines += "#define DEPTH_FOG " + (settings.depth_fog ? std::string("1") : std::string("0")) + "\n";

		preprocessor_defines += "#define USE_LOGARITHMIC_DEPTH_BUFFER " + (settings.use_logarithmic_depth_buffer ? std::string("1") : std::string("0")) + "\n";

		// static_cascade_blending causes a white-screen error on many Intel GPUs.
		const bool is_intel_vendor = openglDriverVendorIsIntel();
		const bool static_cascade_blending = !is_intel_vendor;
		preprocessor_defines += "#define DO_STATIC_SHADOW_MAP_CASCADE_BLENDING " + (static_cascade_blending ? std::string("1") : std::string("0")) + "\n";
		

		const std::string use_shader_dir = data_dir + "/shaders";


		phong_uniform_buf_ob = new UniformBufOb();
		phong_uniform_buf_ob->allocate(sizeof(PhongUniforms));

		shared_vert_uniform_buf_ob = new UniformBufOb();
		shared_vert_uniform_buf_ob->allocate(sizeof(SharedVertUniforms));

		per_object_vert_uniform_buf_ob = new UniformBufOb();
		per_object_vert_uniform_buf_ob->allocate(sizeof(PerObjectVertUniforms));

		// Will be used if we hit a shader compilation error later
		fallback_phong_prog       = getPhongProgram      (ProgramKey("phong",       false, false, false, false, false, false, false, false, false, false, false));
		fallback_transparent_prog = getTransparentProgram(ProgramKey("transparent", false, false, false, false, false, false, false, false, false, false, false));

		if(settings.shadow_mapping)
			fallback_depth_prog       = getDepthDrawProgram  (ProgramKey("depth",       false, false, false, false, false, false, false, false, false, false, false));


		env_prog = new OpenGLProgram(
			"env",
			new OpenGLShader(use_shader_dir + "/env_vert_shader.glsl", preprocessor_defines, GL_VERTEX_SHADER),
			new OpenGLShader(use_shader_dir + "/env_frag_shader.glsl", preprocessor_defines, GL_FRAGMENT_SHADER)
		);
		env_diffuse_colour_location		= env_prog->getUniformLocation("diffuse_colour");
		env_have_texture_location		= env_prog->getUniformLocation("have_texture");
		env_diffuse_tex_location		= env_prog->getUniformLocation("diffuse_tex");
		env_texture_matrix_location		= env_prog->getUniformLocation("texture_matrix");
		env_sundir_cs_location			= env_prog->getUniformLocation("sundir_cs");
		//env_noise_tex_location			= env_prog->getUniformLocation("noise_tex");
		env_fbm_tex_location			= env_prog->getUniformLocation("fbm_tex");
		env_cirrus_tex_location			= env_prog->getUniformLocation("cirrus_tex");

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

			noise_tex = new OpenGLTexture(W, W, this, OpenGLTexture::Format_Greyscale_Float, OpenGLTexture::Filtering_Fancy);
			noise_tex->load(W, W, W * sizeof(float), ArrayRef<uint8>((const uint8*)data.data(), data.size() * sizeof(float)));
			conPrint("noise_tex creation took " + timer.elapsedString());
		}

		// Make FBM texture
		{
			Timer timer;
			const size_t W = 1024;
			js::Vector<float, 16> data(W * W);

			glare::TaskManager& manager = getTaskManager();
			const size_t num_tasks = myMax<size_t>(1, manager.getNumThreads());
			const size_t num_rows_per_task = Maths::roundedUpDivide(W, num_tasks);
			for(size_t t=0; t<num_tasks; ++t)
			{
				BuildFBMNoiseTask* task = new BuildFBMNoiseTask();
				task->data = &data;
				task->W = W;
				task->begin_y = (int)myMin(t       * num_rows_per_task, W);
				task->end_y   = (int)myMin((t + 1) * num_rows_per_task, W);
				manager.addTask(task);
			}
			manager.waitForTasksToComplete();

			// EXRDecoder::saveImageToEXR(data.data(), W, W, 1, false, "fbm.exr", "noise", EXRDecoder::SaveOptions());

			fbm_tex = new OpenGLTexture(W, W, this, OpenGLTexture::Format_Greyscale_Float, OpenGLTexture::Filtering_Fancy);
			fbm_tex->load(W, W, W * sizeof(float), ArrayRef<uint8>((const uint8*)data.data(), data.size() * sizeof(float)));
			conPrint("fbm_tex creation took " + timer.elapsedString());
		}


		overlay_prog = new OpenGLProgram(
			"overlay",
			new OpenGLShader(use_shader_dir + "/overlay_vert_shader.glsl", preprocessor_defines, GL_VERTEX_SHADER),
			new OpenGLShader(use_shader_dir + "/overlay_frag_shader.glsl", preprocessor_defines, GL_FRAGMENT_SHADER)
		);
		overlay_diffuse_colour_location		= overlay_prog->getUniformLocation("diffuse_colour");
		overlay_have_texture_location		= overlay_prog->getUniformLocation("have_texture");
		overlay_diffuse_tex_location		= overlay_prog->getUniformLocation("diffuse_tex");
		overlay_texture_matrix_location		= overlay_prog->getUniformLocation("texture_matrix");

		clear_prog = new OpenGLProgram(
			"clear",
			new OpenGLShader(use_shader_dir + "/clear_vert_shader.glsl", preprocessor_defines, GL_VERTEX_SHADER),
			new OpenGLShader(use_shader_dir + "/clear_frag_shader.glsl", preprocessor_defines, GL_FRAGMENT_SHADER)
		);
		
		outline_prog = new OpenGLProgram(
			"outline",
			new OpenGLShader(use_shader_dir + "/outline_vert_shader.glsl", preprocessor_defines, GL_VERTEX_SHADER),
			new OpenGLShader(use_shader_dir + "/outline_frag_shader.glsl", preprocessor_defines, GL_FRAGMENT_SHADER)
		);

		edge_extract_prog = new OpenGLProgram(
			"edge_extract",
			new OpenGLShader(use_shader_dir + "/edge_extract_vert_shader.glsl", preprocessor_defines, GL_VERTEX_SHADER),
			new OpenGLShader(use_shader_dir + "/edge_extract_frag_shader.glsl", preprocessor_defines, GL_FRAGMENT_SHADER)
		);
		edge_extract_tex_location		= edge_extract_prog->getUniformLocation("tex");
		edge_extract_col_location		= edge_extract_prog->getUniformLocation("col");

		if(settings.use_final_image_buffer)
		{
			downsize_prog = new OpenGLProgram(
				"downsize",
				new OpenGLShader(use_shader_dir + "/downsize_vert_shader.glsl", preprocessor_defines, GL_VERTEX_SHADER),
				new OpenGLShader(use_shader_dir + "/downsize_frag_shader.glsl", preprocessor_defines, GL_FRAGMENT_SHADER)
			);

			gaussian_blur_prog = new OpenGLProgram(
				"gaussian_blur",
				new OpenGLShader(use_shader_dir + "/gaussian_blur_vert_shader.glsl", preprocessor_defines, GL_VERTEX_SHADER),
				new OpenGLShader(use_shader_dir + "/gaussian_blur_frag_shader.glsl", preprocessor_defines, GL_FRAGMENT_SHADER)
			);
			gaussian_blur_prog->appendUserUniformInfo(UserUniformInfo::UniformType_Int, "x_blur");
		
			final_imaging_prog = new OpenGLProgram(
				"final_imaging",
				new OpenGLShader(use_shader_dir + "/final_imaging_vert_shader.glsl", preprocessor_defines, GL_VERTEX_SHADER),
				new OpenGLShader(use_shader_dir + "/final_imaging_frag_shader.glsl", preprocessor_defines, GL_FRAGMENT_SHADER)
			);
			final_imaging_prog->appendUserUniformInfo(UserUniformInfo::UniformType_Float, "bloom_strength");
			assert(final_imaging_prog->user_uniform_info.back().index == FINAL_IMAGING_BLOOM_STRENGTH_UNIFORM_INDEX);

			for(int i=0; i<NUM_BLUR_DOWNSIZES; ++i)
			{
				final_imaging_prog->appendUserUniformInfo(UserUniformInfo::UniformType_Sampler2D, "blur_tex_" + toString(i));
				assert(final_imaging_prog->user_uniform_info.back().index == FINAL_IMAGING_BLUR_TEX_UNIFORM_START + i);
			}
		}


		//terrain_system = new TerrainSystem();
		//terrain_system->init(*this, Vec3d(0,0,2));

		if(settings.shadow_mapping)
		{
			shadow_mapping = new ShadowMapping();
			shadow_mapping->init();

			{
				clear_buf_overlay_ob =  new OverlayObject();
				clear_buf_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(0, 0, -0.9999f);
				clear_buf_overlay_ob->material.albedo_rgb = Colour3f(1.f, 0.2f, 0.2f);
				clear_buf_overlay_ob->material.shader_prog = this->clear_prog;
				clear_buf_overlay_ob->mesh_data = this->unit_quad_meshdata;
			}

			if(false)
			{
				// Add overlay quad to preview depth map

				{
					OverlayObjectRef tex_preview_overlay_ob =  new OverlayObject();

					tex_preview_overlay_ob->ob_to_world_matrix = Matrix4f::uniformScaleMatrix(0.95f) * Matrix4f::translationMatrix(-1, 0, 0);

					tex_preview_overlay_ob->material.albedo_rgb = Colour3f(1.f);
					tex_preview_overlay_ob->material.shader_prog = this->overlay_prog;

					tex_preview_overlay_ob->material.albedo_texture = shadow_mapping->depth_tex; // static_depth_tex[0];

					tex_preview_overlay_ob->mesh_data = this->unit_quad_meshdata;

					addOverlayObject(tex_preview_overlay_ob);
				}
				{
					OverlayObjectRef tex_preview_overlay_ob =  new OverlayObject();

					tex_preview_overlay_ob->ob_to_world_matrix = Matrix4f::uniformScaleMatrix(0.95f) * Matrix4f::translationMatrix(0, 0, 0);

					tex_preview_overlay_ob->material.albedo_rgb = Colour3f(1.f);
					tex_preview_overlay_ob->material.shader_prog = this->overlay_prog;

					tex_preview_overlay_ob->material.albedo_texture = shadow_mapping->static_depth_tex[1];

					tex_preview_overlay_ob->mesh_data = this->unit_quad_meshdata;

					addOverlayObject(tex_preview_overlay_ob);
				}
			}
		}

		// Outline stuff
		{
			outline_solid_framebuffer = new FrameBuffer();
			outline_edge_framebuffer  = new FrameBuffer();
	
			outline_solid_mat.shader_prog = outline_prog;

			outline_edge_mat.albedo_rgb = Colour3f(0.9f, 0.2f, 0.2f);
			outline_edge_mat.shader_prog = this->overlay_prog;

			outline_quad_meshdata = this->unit_quad_meshdata;
		}

		// Load diffuse irradiance maps
		{
			std::vector<Map2DRef> face_maps(6);
			for(int i=0; i<6; ++i)
			{
				face_maps[i] = ImFormatDecoder::decodeImage(".", gl_data_dir + "/diffuse_sky_no_sun_" + toString(i) + ".exr");

				if(!face_maps[i].isType<ImageMapFloat>())
					throw glare::Exception("cosine env map Must be ImageMapFloat");
			}

			this->cosine_env_tex = loadCubeMap(face_maps, OpenGLTexture::Filtering_Bilinear);
		}

		// Load specular-reflection env tex
		{
			const std::string path = gl_data_dir + "/specular_refl_sky_no_sun_combined.exr";
			Map2DRef specular_env = ImFormatDecoder::decodeImage(".", path);

			if(!specular_env.isType<ImageMapFloat>())
				throw glare::Exception("specular env map Must be ImageMapFloat");

			this->specular_env_tex = getOrLoadOpenGLTexture(OpenGLTextureKey(path), *specular_env, /*state, */OpenGLTexture::Filtering_Bilinear);
		}

		// Load blue noise texture
		{
			Reference<Map2D> blue_noise_map = texture_server->getTexForPath(".", gl_data_dir + "/LDR_LLL1_0.png");
			this->blue_noise_tex = getOrLoadOpenGLTexture(OpenGLTextureKey(gl_data_dir + "/LDR_LLL1_0.png"), *blue_noise_map, OpenGLTexture::Filtering_Nearest, 
				OpenGLTexture::Wrapping_Repeat, /*allow compression=*/false, /*use_sRGB=*/false);
		}

		init_succeeded = true;
	}
	catch(glare::Exception& e)
	{
		conPrint(e.what());
		this->initialisation_error_msg = e.what();
		init_succeeded = false;
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
		"#define CONVERT_ALBEDO_FROM_SRGB " + toString(key.convert_albedo_from_srgb) + "\n" +
		"#define SKINNING " + toString(key.skinning) + "\n" +
		"#define IMPOSTERABLE " + toString(key.imposterable) + "\n" +
		"#define USE_WIND_VERT_SHADER " + toString(key.use_wind_vert_shader) + "\n" + 
		"#define DOUBLE_SIDED " + toString(key.double_sided) + "\n" + 
		"#define BLOB_SHADOWS " + toString(1) + "\n"; // TEMP
}


OpenGLProgramRef OpenGLEngine::getPhongProgram(const ProgramKey& key) // Throws glare::Exception on shader compilation failure.
{
	if(progs[key] == NULL)
	{
		//Timer timer;

		const std::string use_defs = preprocessor_defines + preprocessorDefsForKey(key);
		const std::string use_shader_dir = data_dir + "/shaders";

		OpenGLProgramRef phong_prog = new OpenGLProgram(
			"phong",
			new OpenGLShader(use_shader_dir + "/phong_vert_shader.glsl", use_defs, GL_VERTEX_SHADER),
			new OpenGLShader(use_shader_dir + "/phong_frag_shader.glsl", use_defs, GL_FRAGMENT_SHADER)
		);
		phong_prog->is_phong = true;
		phong_prog->uses_vert_uniform_buf_obs = true;

		progs[key] = phong_prog;

		getUniformLocations(phong_prog, settings.shadow_mapping, phong_prog->uniform_locations);

		unsigned int phong_uniforms_index = glGetUniformBlockIndex(phong_prog->program, "PhongUniforms");
		glUniformBlockBinding(phong_prog->program, phong_uniforms_index, /*binding point=*/0);
		glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/0, this->phong_uniform_buf_ob->handle);

		unsigned int phong_shared_vert_uniforms_index = glGetUniformBlockIndex(phong_prog->program, "SharedVertUniforms");
		glUniformBlockBinding(phong_prog->program, phong_shared_vert_uniforms_index, /*binding point=*/1);
		glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/1, this->shared_vert_uniform_buf_ob->handle);

		unsigned int phong_per_object_vert_uniforms_index = glGetUniformBlockIndex(phong_prog->program, "PerObjectVertUniforms");
		glUniformBlockBinding(phong_prog->program, phong_per_object_vert_uniforms_index, /*binding point=*/2);
		glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/2, this->per_object_vert_uniform_buf_ob->handle);
		

		//conPrint("Built phong program for key " + key.description() + ", Elapsed: " + timer.elapsedStringNSigFigs(3));
	}

	return progs[key];
}


OpenGLProgramRef OpenGLEngine::getTransparentProgram(const ProgramKey& key) // Throws glare::Exception on shader compilation failure.
{
	// TODO: use buildProgram() below

	if(progs[key] == NULL)
	{
		Timer timer;

		const std::string use_defs = preprocessor_defines + preprocessorDefsForKey(key);
		const std::string use_shader_dir = data_dir + "/shaders";

		OpenGLProgramRef prog = new OpenGLProgram(
			"transparent",
			new OpenGLShader(use_shader_dir + "/transparent_vert_shader.glsl", use_defs, GL_VERTEX_SHADER),
			new OpenGLShader(use_shader_dir + "/transparent_frag_shader.glsl", use_defs, GL_FRAGMENT_SHADER)
		);
		prog->is_transparent = true;
		prog->uses_vert_uniform_buf_obs = true;

		progs[key] = prog;

		getUniformLocations(prog, settings.shadow_mapping, prog->uniform_locations);

		unsigned int shared_vert_uniforms_index = glGetUniformBlockIndex(prog->program, "SharedVertUniforms");
		glUniformBlockBinding(prog->program, shared_vert_uniforms_index, /*binding point=*/1);
		glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/1, this->shared_vert_uniform_buf_ob->handle);

		unsigned int per_object_vert_uniforms_index = glGetUniformBlockIndex(prog->program, "PerObjectVertUniforms");
		glUniformBlockBinding(prog->program, per_object_vert_uniforms_index, /*binding point=*/2);
		glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/2, this->per_object_vert_uniform_buf_ob->handle);


		conPrint("Built transparent program for key " + key.description() + ", Elapsed: " + timer.elapsedStringNSigFigs(3));
	}

	return progs[key];
}


// shader_name_prefix should be something like "water" or "transparent"
OpenGLProgramRef OpenGLEngine::buildProgram(const std::string& shader_name_prefix, const ProgramKey& key) // Throws glare::Exception on shader compilation failure.
{
	if(progs[key] == NULL)
	{
		Timer timer;

		const std::string use_defs = preprocessor_defines + preprocessorDefsForKey(key);
		const std::string use_shader_dir = data_dir + "/shaders";

		OpenGLProgramRef prog = new OpenGLProgram(
			shader_name_prefix,
			new OpenGLShader(use_shader_dir + "/" + shader_name_prefix + "_vert_shader.glsl", use_defs, GL_VERTEX_SHADER),
			new OpenGLShader(use_shader_dir + "/" + shader_name_prefix + "_frag_shader.glsl", use_defs, GL_FRAGMENT_SHADER)
		);
		prog->is_transparent = true;
		prog->uses_vert_uniform_buf_obs = true;

		progs[key] = prog;

		getUniformLocations(prog, settings.shadow_mapping, prog->uniform_locations);

		unsigned int shared_vert_uniforms_index = glGetUniformBlockIndex(prog->program, "SharedVertUniforms");
		glUniformBlockBinding(prog->program, shared_vert_uniforms_index, /*binding point=*/1);
		glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/1, this->shared_vert_uniform_buf_ob->handle);

		unsigned int per_object_vert_uniforms_index = glGetUniformBlockIndex(prog->program, "PerObjectVertUniforms");
		glUniformBlockBinding(prog->program, per_object_vert_uniforms_index, /*binding point=*/2);
		glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/2, this->per_object_vert_uniform_buf_ob->handle);


		conPrint("Built transparent program for key " + key.description() + ", Elapsed: " + timer.elapsedStringNSigFigs(3));
	}

	return progs[key];
}


OpenGLProgramRef OpenGLEngine::getImposterProgram(const ProgramKey& key) // Throws glare::Exception on shader compilation failure.
{
	if(progs[key] == NULL)
	{
		//Timer timer;

		const std::string use_defs = preprocessor_defines + preprocessorDefsForKey(key);
		const std::string use_shader_dir = data_dir + "/shaders";

		OpenGLProgramRef prog = new OpenGLProgram(
			"imposter",
			new OpenGLShader(use_shader_dir + "/imposter_vert_shader.glsl", use_defs, GL_VERTEX_SHADER),
			new OpenGLShader(use_shader_dir + "/imposter_frag_shader.glsl", use_defs, GL_FRAGMENT_SHADER)
		);
		prog->is_phong = true; // Just set to true to use PhongUniforms block
		prog->uses_vert_uniform_buf_obs = true;

		progs[key] = prog;

		getUniformLocations(prog, settings.shadow_mapping, prog->uniform_locations);

		unsigned int phong_uniforms_index = glGetUniformBlockIndex(prog->program, "PhongUniforms");
		glUniformBlockBinding(prog->program, phong_uniforms_index, /*binding point=*/0);
		glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/0, this->phong_uniform_buf_ob->handle);

		unsigned int shared_vert_uniforms_index = glGetUniformBlockIndex(prog->program, "SharedVertUniforms");
		glUniformBlockBinding(prog->program, shared_vert_uniforms_index, /*binding point=*/1);
		glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/1, this->shared_vert_uniform_buf_ob->handle);

		unsigned int per_object_vert_uniforms_index = glGetUniformBlockIndex(prog->program, "PerObjectVertUniforms");
		glUniformBlockBinding(prog->program, per_object_vert_uniforms_index, /*binding point=*/2);
		glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/2, this->per_object_vert_uniform_buf_ob->handle);


		//conPrint("Built imposter program for key " + key.description() + ", Elapsed: " + timer.elapsedStringNSigFigs(3));
	}

	return progs[key];
}


OpenGLProgramRef OpenGLEngine::getDepthDrawProgram(const ProgramKey& key_) // Throws glare::Exception on shader compilation failure.
{
	// Only some fields are relevant for picking the correct depth draw prog.  Set the other options to false to avoid unnecessary state changes.
	ProgramKey key = key_;
	
	if(!key_.use_wind_vert_shader)
		key.vert_colours = false; // Vert colours will only be used for wind
	key.lightmapping = false;
	key.gen_planar_uvs = false; // relevant?
	key.draw_planar_uv_grid = false;
	key.convert_albedo_from_srgb = false;
	// relevant: key.skinning
	key.imposterable = false; // for now
	// relevant: use_wind_vert_shader
	key.double_sided = false; // for now

	if(progs[key] == NULL)
	{
		Timer timer;

		const std::string use_defs = preprocessor_defines + preprocessorDefsForKey(key);
		const std::string use_shader_dir = data_dir + "/shaders";

		OpenGLProgramRef prog = new OpenGLProgram(
			"depth",
			new OpenGLShader(use_shader_dir + "/depth_vert_shader.glsl", use_defs, GL_VERTEX_SHADER),
			new OpenGLShader(use_shader_dir + "/depth_frag_shader.glsl", use_defs, GL_FRAGMENT_SHADER)
		);
		prog->key = key;
		prog->is_depth_draw = true;

		progs[key] = prog;

		getUniformLocations(prog, settings.shadow_mapping, prog->uniform_locations);

		if(key.instance_matrices || key.skinning || key.use_wind_vert_shader) // SharedVertUniforms are only used in depth_vert_shader.glsl when INSTANCE_MATRICES is defined.
		{
			unsigned int shared_vert_uniforms_index = glGetUniformBlockIndex(prog->program, "SharedVertUniforms");
			assert(shared_vert_uniforms_index != GL_INVALID_INDEX);
			
			glUniformBlockBinding(prog->program, shared_vert_uniforms_index, /*binding point=*/1);
			glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/1, this->shared_vert_uniform_buf_ob->handle);


			unsigned int per_object_vert_uniforms_index = glGetUniformBlockIndex(prog->program, "PerObjectVertUniforms");
			glUniformBlockBinding(prog->program, per_object_vert_uniforms_index, /*binding point=*/2);
			glBindBufferBase(GL_UNIFORM_BUFFER, /*binding point=*/2, this->per_object_vert_uniform_buf_ob->handle);

			prog->uses_vert_uniform_buf_obs = true;
		}

		conPrint("Built depth draw program for key " + key.description() + ", Elapsed: " + timer.elapsedStringNSigFigs(3));
	}

	return progs[key];
}


OpenGLProgramRef OpenGLEngine::getProgramWithFallbackOnError(const ProgramKey& key) // On shader compilation failure, just returns a default phong program.
{
	try
	{
		if(key.program_name == "phong")
			return getPhongProgram(key);
		else if(key.program_name == "transparent")
			return getTransparentProgram(key);
		else if(key.program_name == "imposter")
			return getImposterProgram(key);
		else if(key.program_name == "depth")
			return getDepthDrawProgram(key);
		else
		{
			assert(0);
			throw glare::Exception("Invalid program name '" + key.program_name + ".");
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

	outline_solid_tex = new OpenGLTexture(outline_tex_w, outline_tex_h, this,
		OpenGLTexture::Format_RGB_Linear_Uint8,
		OpenGLTexture::Filtering_Bilinear,
		OpenGLTexture::Wrapping_Clamp // Clamp texture reads otherwise edge outlines will wrap around to other side of frame.
	);

	outline_edge_tex = new OpenGLTexture(outline_tex_w, outline_tex_h, this,
		OpenGLTexture::Format_RGBA_Linear_Uint8,
		OpenGLTexture::Filtering_Bilinear
	);

	outline_edge_mat.albedo_texture = outline_edge_tex;

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


void OpenGLEngine::setMainViewport(int main_viewport_w_, int main_viewport_h_)
{
	main_viewport_w = main_viewport_w_;
	main_viewport_h = main_viewport_h_;
}


void OpenGLEngine::setViewport(int viewport_w_, int viewport_h_)
{
	viewport_w = viewport_w_;
	viewport_h = viewport_h_;

	glViewport(0, 0, viewport_w, viewport_h);
}


void OpenGLScene::unloadAllData()
{
	this->objects.clear();
	this->transparent_objects.clear();

	this->env_ob->materials[0] = OpenGLMaterial();
}


GLMemUsage OpenGLScene::getTotalMemUsage() const
{
	GLMemUsage sum;
	for(auto i = objects.begin(); i != objects.end(); ++i)
	{
		sum += (*i)->mesh_data->getTotalMemUsage();
	}
	return sum;
}


void OpenGLEngine::unloadAllData()
{
	for(auto i = scenes.begin(); i != scenes.end(); ++i)
		(*i)->unloadAllData();

	this->selected_objects.clear();

	opengl_textures.clear();
	texture_data_manager->clear();

	tex_mem_usage = 0;
}


const js::AABBox OpenGLEngine::getAABBWSForObjectWithTransform(GLObject& object, const Matrix4f& to_world)
{
	return object.mesh_data->aabb_os.transformedAABB(to_world);
}


// Updates object ob_to_world_inv_transpose_matrix and aabb_ws.
void OpenGLEngine::updateObjectTransformData(GLObject& object)
{
	assert(::isFinite(object.ob_to_world_matrix.e[0]));

	const Matrix4f& to_world = object.ob_to_world_matrix;

	const bool invertible = to_world.getUpperLeftInverseTranspose(/*result=*/object.ob_to_world_inv_transpose_matrix); // Compute inverse matrix
	if(!invertible)
		object.ob_to_world_inv_transpose_matrix = object.ob_to_world_matrix; // If not invertible, just use to-world matrix.
	// Hopefully we won't encounter non-invertible matrices here anyway.

	object.aabb_ws = object.mesh_data->aabb_os.transformedAABBFast(to_world);
}


void OpenGLEngine::assignShaderProgToMaterial(OpenGLMaterial& material, bool use_vert_colours, bool uses_instancing, bool uses_skinning)
{
	// If the client code has already set a special non-basic shader program (like a grid shader), don't overwrite it.
	if(material.shader_prog.nonNull() && 
		!(material.shader_prog->is_transparent || material.shader_prog->is_phong)
		)
		return;

	const bool alpha_test = material.albedo_texture.nonNull() && material.albedo_texture->hasAlpha();

	// Lightmapping doesn't work properly on Mac currently, because lightmaps use the BC6H format, which isn't supported on mac.
	// This results in lightmaps rendering black, so it's better to just not use lightmaps for now.
#ifdef OSX
	const bool uses_lightmapping = false;
#else
	const bool uses_lightmapping = material.lightmap_texture.nonNull();
#endif

	// If we do not support converting textures from sRGB to linear in opengl, then we need to do it in the shader.
	// we only want to do this when we have a texture.
	const bool need_convert_albedo_from_srgb = !this->GL_EXT_texture_sRGB_support;// && material.albedo_texture.nonNull();

	const ProgramKey key(material.imposter ? "imposter" : (material.transparent ? "transparent" : "phong"), /*alpha_test=*/alpha_test, /*vert_colours=*/use_vert_colours, /*instance_matrices=*/uses_instancing, uses_lightmapping,
		material.gen_planar_uvs, material.draw_planar_uv_grid, material.convert_albedo_from_srgb || need_convert_albedo_from_srgb, uses_skinning, material.imposterable, material.use_wind_vert_shader, material.double_sided);

	material.shader_prog = getProgramWithFallbackOnError(key);

	if(settings.shadow_mapping)
	{
		const ProgramKey depth_key("depth", /*alpha_test=*/alpha_test, /*vert_colours=*/use_vert_colours, /*instance_matrices=*/uses_instancing, uses_lightmapping,
			material.gen_planar_uvs, material.draw_planar_uv_grid, material.convert_albedo_from_srgb || need_convert_albedo_from_srgb, uses_skinning, material.imposterable, material.use_wind_vert_shader, material.double_sided);

		material.depth_draw_shader_prog = getDepthDrawProgramWithFallbackOnError(depth_key);
	}
}


void OpenGLEngine::addObject(const Reference<GLObject>& object)
{
	assert(object->mesh_data.nonNull());
	assert(object->mesh_data->vert_vao.nonNull());

	// Compute world space AABB of object
	updateObjectTransformData(*object.getPointer());
	
	// Don't add always_visible objects to objects set, we will draw them a special way.
	if(!object->always_visible)
		current_scene->objects.insert(object);

	object->random_num = rng.nextUInt();

	// Build the mesh used by the object if it's not built already.
	//if(mesh_render_data.find(object->mesh) == mesh_render_data.end())
	//	this->buildMesh(object->mesh);

	// Build materials
	bool have_transparent_mat = false;
	for(size_t i=0; i<object->materials.size(); ++i)
	{
		assignShaderProgToMaterial(object->materials[i], object->mesh_data->has_vert_colours, /*uses instancing=*/object->instance_matrix_vbo.nonNull(), object->mesh_data->usesSkinning());
		have_transparent_mat = have_transparent_mat || object->materials[i].transparent;
	}

	if(have_transparent_mat)
		current_scene->transparent_objects.insert(object);

	if(object->always_visible)
		current_scene->always_visible_objects.insert(object);

	if(object->is_instanced_ob_with_imposters)
		current_scene->objects_with_imposters.insert(object);

	const AnimationData& anim_data = object->mesh_data->animation_data;
	if(!anim_data.animations.empty() || !anim_data.joint_nodes.empty())
		current_scene->animated_objects.insert(object);
}


void OpenGLEngine::addOverlayObject(const Reference<OverlayObject>& object)
{
	current_scene->overlay_objects.insert(object);
	object->material.shader_prog = overlay_prog;
}


void OpenGLEngine::removeOverlayObject(const Reference<OverlayObject>& object)
{
	current_scene->overlay_objects.erase(object);
}


// Notify the OpenGL engine that a texture has been loaded.
void OpenGLEngine::textureLoaded(const std::string& path, const OpenGLTextureKey& key, bool use_sRGB)
{
	// conPrint("textureLoaded(): path: " + path);

	// Load the texture into OpenGL.
	// Note that at this point there may actually be no OpenGL objects inserted that use the texture yet!
	Reference<OpenGLTexture> opengl_texture;

	// If the OpenGL texture for this path has already been created, return it.
	auto res = this->opengl_textures.find(key);
	if(res != this->opengl_textures.end())
	{
		// conPrint("\tFound in opengl_textures.");
		opengl_texture = res->second.value;
	}
	else
	{
		// See if we have 8-bit texture data loaded for this texture
		Reference<TextureData> tex_data = this->texture_data_manager->getTextureData(key.path); // returns null ref if not present.
		if(tex_data.nonNull())
		{
			// Avoid using trilinear filtering with animated textures, if we haven't computed the mipmap levels ourselves (while doing compression).
			// Otherwise the opengl driver will have to repeatedly generate mipmaps as the texture is updated to the current frame.
			const bool animated = tex_data->frames.size() > 1;
			const bool compressed = !tex_data->frames[0].compressed_data.empty();
			const OpenGLTexture::Filtering filtering = (animated && !compressed) ? OpenGLTexture::Filtering_Bilinear : OpenGLTexture::Filtering_Fancy;

			opengl_texture = this->loadOpenGLTextureFromTexData(key, tex_data, filtering, OpenGLTexture::Wrapping_Repeat, use_sRGB);
			assert(opengl_texture.nonNull());
			// conPrint("\tLoaded from tex data.");
		}
		else
		{
			Reference<Map2D> map = texture_server->getTexForRawNameIfLoaded(key.path);
			if(map.nonNull())
			{
				opengl_texture = this->getOrLoadOpenGLTexture(key, *map, OpenGLTexture::Filtering_Fancy, OpenGLTexture::Wrapping_Repeat, /*allow compression=*/true, use_sRGB);
				assert(opengl_texture.nonNull());
				// conPrint("\tLoaded from map.");
			}
		}
	}

	assert(opengl_texture.nonNull());
	
	for(auto z = scenes.begin(); z != scenes.end(); ++z)
	{
		OpenGLScene* scene = z->ptr();

		for(auto it = scene->objects.begin(); it != scene->objects.end(); ++it)
		{
			GLObject* const object = it->ptr();

			for(size_t i=0; i<object->materials.size(); ++i)
			{
				OpenGLMaterial& mat = object->materials[i];
				if((mat.albedo_texture.isNull() || mat.albedo_tex_is_placeholder) && mat.tex_path == path)
				{
					// conPrint("\tOpenGLEngine::textureLoaded(): Found object using tex '" + path + "'.");

					mat.albedo_texture = opengl_texture;
					mat.albedo_tex_is_placeholder = false;

					// Texture may have an alpha channel, in which case we want to assign a different shader.
					assignShaderProgToMaterial(mat, object->mesh_data->has_vert_colours, /*uses instancing=*/object->instance_matrix_vbo.nonNull(), object->mesh_data->usesSkinning());
				}

				if(object->materials[i].metallic_roughness_tex_path == path)
				{
					mat.metallic_roughness_texture = opengl_texture;
				}

				if(/*mat.lightmap_texture.isNull() && */object->materials[i].lightmap_path == path)
				{
					// conPrint("\tOpenGLEngine::textureLoaded(): Found object using lightmap '" + path + "'.");

					mat.lightmap_texture = opengl_texture;

					// Now that we have a lightmap, assign a different shader.
					assignShaderProgToMaterial(mat, object->mesh_data->has_vert_colours, /*uses instancing=*/object->instance_matrix_vbo.nonNull(), object->mesh_data->usesSkinning());
				}
			}
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
	current_scene->objects.erase(object);
	current_scene->transparent_objects.erase(object);
	current_scene->always_visible_objects.erase(object);
	if(object->is_instanced_ob_with_imposters)
		current_scene->objects_with_imposters.erase(object);
	current_scene->animated_objects.erase(object);
	selected_objects.erase(object.getPointer());
}


bool OpenGLEngine::isObjectAdded(const Reference<GLObject>& object) const
{
	return current_scene->objects.find(object) != current_scene->objects.end();
}


void OpenGLEngine::newMaterialUsed(OpenGLMaterial& mat, bool use_vert_colours, bool uses_instancing, bool uses_skinning)
{
	assignShaderProgToMaterial(mat,
		use_vert_colours,
		uses_instancing,
		uses_skinning
	);
}


void OpenGLEngine::objectMaterialsUpdated(const Reference<GLObject>& object)
{
	// Add this object to transparent_objects list if it has a transparent material and is not already in the list.

	bool have_transparent_mat = false;
	for(size_t i=0; i<object->materials.size(); ++i)
	{
		assignShaderProgToMaterial(object->materials[i], object->mesh_data->has_vert_colours, /*uses instancing=*/object->instance_matrix_vbo.nonNull(), object->mesh_data->usesSkinning());
		have_transparent_mat = have_transparent_mat || object->materials[i].transparent;
	}

	if(have_transparent_mat)
		current_scene->transparent_objects.insert(object);
	else
		current_scene->transparent_objects.erase(object); // Remove from transparent material list if it is currently in there.
}


// Return an OpenGL texture based on tex_path.  Loads it from disk if needed.  Blocking.
// Throws glare::Exception
Reference<OpenGLTexture> OpenGLEngine::getTexture(const std::string& tex_path, bool allow_compression)
{
	try
	{
		const std::string tex_key = this->texture_server->keyForPath(tex_path);

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

		// TEMP HACK: need to set base dir here
		Reference<Map2D> map = texture_server->getTexForPath(".", tex_path);

		return this->getOrLoadOpenGLTexture(texture_key, *map, OpenGLTexture::Filtering_Fancy, OpenGLTexture::Wrapping_Repeat, allow_compression);
	}
	catch(TextureServerExcep& e)
	{
		throw glare::Exception(e.what());
	}
}


// If the texture identified by tex_path has been loaded and processed, load into OpenGL if needed, then return the OpenGL texture.
// If the texture is not loaded or not processed yet, return a null reference.
// Throws glare::Exception
Reference<OpenGLTexture> OpenGLEngine::getTextureIfLoaded(const OpenGLTextureKey& texture_key, bool use_sRGB)
{
	// conPrint("getTextureIfLoaded(), tex_path: " + tex_path);
	try
	{
		// If the OpenGL texture for this path has already been created, return it.
		auto res = this->opengl_textures.find(texture_key);
		if(res != this->opengl_textures.end())
		{
			// conPrint("\tFound in opengl_textures.");
			if(res->second.value->getRefCount() == 1)
				this->textureBecameUsed(res->second.value.ptr());
			return res->second.value;
		}

		// If we have processed texture data for this texture, load it
		Reference<TextureData> tex_data = this->texture_data_manager->getTextureData(texture_key.path);
		if(tex_data.nonNull())
		{
			// conPrint("\tFound in tex_data.");
			Reference<OpenGLTexture> gl_tex = this->loadOpenGLTextureFromTexData(texture_key, tex_data, OpenGLTexture::Filtering_Fancy, OpenGLTexture::Wrapping_Repeat, use_sRGB); // Load into OpenGL and return it.
			assert(gl_tex.nonNull());
			return gl_tex;
		}

		Reference<Map2D> map = texture_server->getTexForRawNameIfLoaded(texture_key.path);
		if(map.nonNull()) // If the texture has been loaded from disk already:
		{
			if(dynamic_cast<const ImageMapUInt8*>(map.ptr())) // If UInt8 map, which needs processing:
				return Reference<OpenGLTexture>(); // Consider this texture not loaded yet.  (Needs to be processed first)
			else
			{
				// Not an UInt8 map so doesn't need processing, so load it.
				// conPrint("\tloading from map.");
				Reference<OpenGLTexture> gl_tex = this->getOrLoadOpenGLTexture(texture_key, *map, OpenGLTexture::Filtering_Fancy, OpenGLTexture::Wrapping_Repeat, /*allow_compression=*/true, use_sRGB); 
				assert(gl_tex.nonNull());
				return gl_tex;
			}
		}
		else
		{
			// conPrint("\tNot found.");
			return Reference<OpenGLTexture>();
		}
	}
	catch(TextureServerExcep& e)
	{
		throw glare::Exception(e.what());
	}
}


// http://iquilezles.org/www/articles/frustumcorrect/frustumcorrect.htm
static bool AABBIntersectsFrustum(const Planef* frustum_clip_planes, int num_frustum_clip_planes, const js::AABBox& frustum_aabb, const js::AABBox& aabb_ws)
{
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
		const float refD = frustum_clip_planes[z].getD();
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
		assert((refdist >= refD) == (greater == 15));
		if(greater == 15) // if all distances >= plane.D:
			return false;
	}

	if(frustum_aabb.disjoint(aabb_ws) != 0) // If the frustum AABB is disjoint from the object AABB:
		return false;

	return true;
}


static void bindMeshData(const OpenGLMeshRenderData& mesh_data)
{
	mesh_data.vert_vao->bind();
	mesh_data.vert_indices_buf->bind();
}


static void unbindMeshData(const OpenGLMeshRenderData& mesh_data)
{
	mesh_data.vert_indices_buf->unbind();
	mesh_data.vert_vao->unbind();
}


static void bindMeshData(const GLObject& ob)
{
	if(ob.vert_vao.nonNull())
		ob.vert_vao->bind();
	else
		ob.mesh_data->vert_vao->bind();
	ob.mesh_data->vert_indices_buf->bind();
}


static void unbindMeshData(const GLObject& ob)
{
	ob.mesh_data->vert_indices_buf->unbind();
	VAO::unbind();
}



// http://www.manpagez.com/man/3/glFrustum/
static const Matrix4f frustumMatrix(GLdouble left,
                       GLdouble right,
                       GLdouble bottom,
                       GLdouble top,
                       GLdouble zNear,
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
	Matrix4f t;
	Matrix4f(e).getTranspose(t);
	return t;
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
		(float)(2 / (right - left)),  0, 0, t_x,
		0, (float)(2 / (top - bottom)),  0, t_y,
		0, 0, (float)(-2 / (zFar - zNear)), t_z,
		0, 0, 0, 1 };
	Matrix4f t;
	Matrix4f(e).getTranspose(t);
	return t;
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
	const double t_x = -cam_z / (right - left);
	const double t_y =  cam_z / (top - bottom);

	const double slope = 1.0 / (right - left);

	const float e[16] = {
		(float)(2 / (right - left)),  0, -(float)slope, (float)t_x,
		0, (float)(2 / (top - bottom)),  (float)slope, (float)t_y,
		0, 0, (float)(-2 / (zFar - zNear)), (float)t_z,
		0, 0, 0, 1 };
	return Matrix4f(e).getTranspose();
}


void OpenGLEngine::drawDebugPlane(const Vec3f& point_on_plane, const Vec3f& plane_normal, 
	const Matrix4f& view_matrix, const Matrix4f& proj_matrix, float plane_draw_half_width)
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE); // Disable writing to depth buffer.

	Matrix4f rot;
	rot.constructFromVector(plane_normal.toVec4fVector());

	// Draw a quad on the plane
	{
		outline_edge_mat.albedo_rgb = Colour3f(0.8f, 0.2f, 0.2f);
		outline_edge_mat.alpha = 0.5f;
		outline_edge_mat.shader_prog = this->fallback_transparent_prog;

		Matrix4f quad_to_world = Matrix4f::translationMatrix(point_on_plane.toVec4fPoint()) * rot *
			Matrix4f::uniformScaleMatrix(2*plane_draw_half_width) * Matrix4f::translationMatrix(Vec4f(-0.5f, -0.5f, 0.f, 1.f));

		GLObject ob;
		ob.ob_to_world_matrix = quad_to_world;

		bindMeshData(*outline_quad_meshdata); // Bind the mesh data, which is the same for all batches.
		drawBatch(ob, view_matrix, proj_matrix, outline_edge_mat, *outline_edge_mat.shader_prog, *outline_quad_meshdata, outline_quad_meshdata->batches[0]);
		unbindMeshData(*outline_quad_meshdata);
	}

	glDepthMask(GL_TRUE); // Re-enable writing to depth buffer.
	glDisable(GL_BLEND);

	// Draw an arrow to show the plane normal
	if(debug_arrow_ob.isNull())
	{
		debug_arrow_ob = new GLObject();
		if(arrow_meshdata.isNull())
			arrow_meshdata = MeshPrimitiveBuilding::make3DArrowMesh(); // tip lies at (1,0,0).
		debug_arrow_ob->mesh_data = arrow_meshdata;
		debug_arrow_ob->materials.resize(1);
		debug_arrow_ob->materials[0].albedo_rgb = Colour3f(0.5f, 0.9f, 0.3f);
		debug_arrow_ob->materials[0].shader_prog = getProgramWithFallbackOnError(ProgramKey("phong", /*alpha_test=*/false, /*vert_colours=*/false, /*instance_matrices=*/false, /*lightmapping=*/false,
			/*gen_planar_uvs=*/false, /*draw_planar_uv_grid=*/false, /*convert_albedo_from_srgb=*/false, false, false, false, false));
	}

	Matrix4f arrow_to_world = Matrix4f::translationMatrix(point_on_plane.toVec4fPoint()) * rot *
		Matrix4f::scaleMatrix(2, 2, 2) * Matrix4f::rotationMatrix(Vec4f(0,1,0,0), -Maths::pi_2<float>()); // rot x axis to z axis
	
	debug_arrow_ob->ob_to_world_matrix = arrow_to_world;
	bindMeshData(*debug_arrow_ob); // Bind the mesh data, which is the same for all batches.
	drawBatch(*debug_arrow_ob, view_matrix, proj_matrix, debug_arrow_ob->materials[0], *debug_arrow_ob->materials[0].shader_prog, *debug_arrow_ob->mesh_data, debug_arrow_ob->mesh_data->batches[0]);
	unbindMeshData(*debug_arrow_ob);
}


static inline float largestDim(const js::AABBox& aabb)
{
	return horizontalMax((aabb.max_ - aabb.min_).v);
}


// Draws a quad, with z value at far clip plane.
void OpenGLEngine::partiallyClearBuffer(const Vec2f& begin, const Vec2f& end)
{
	clear_buf_overlay_ob->ob_to_world_matrix =
		Matrix4f::translationMatrix(-1 + begin.x, -1 + begin.y, 1.f) * Matrix4f::scaleMatrix(2 * (end.x - begin.x), 2 * (end.y - begin.y), 1.f);

	glDepthFunc(GL_ALWAYS); // Do this to effectively disable z-test, but still have z writes.

	const OpenGLMeshRenderData& mesh_data = *clear_buf_overlay_ob->mesh_data;
	bindMeshData(mesh_data); // Bind the mesh data, which is the same for all batches.
	const OpenGLMaterial& opengl_mat = clear_buf_overlay_ob->material;
	opengl_mat.shader_prog->useProgram();
	glUniformMatrix4fv(opengl_mat.shader_prog->model_matrix_loc, 1, false, clear_buf_overlay_ob->ob_to_world_matrix.e);
	glDrawElements(GL_TRIANGLES, (GLsizei)mesh_data.batches[0].num_indices, mesh_data.index_type, (void*)(uint64)mesh_data.batches[0].prim_start_offset);
	opengl_mat.shader_prog->useNoPrograms();
	unbindMeshData(mesh_data);

	glDepthFunc(GL_LESS); // restore
}


// Sorts objects into ascending z order based on object-to-world z translation component.
struct OverlayObjectZComparator
{
	inline bool operator() (OverlayObject* a, OverlayObject* b) const
	{
		return a->ob_to_world_matrix.e[14] < b->ob_to_world_matrix.e[14];
	}
};


// This code needs to be reasonably fast as it is executed twice per frame.
void OpenGLEngine::updateInstanceMatricesForObWithImposters(GLObject& ob, bool for_shadow_mapping)
{
	const GlInstanceInfo* const instance_info = ob.instance_info.data();
	const size_t instance_info_size = ob.instance_info.size();

	temp_matrices.resize(0);
	temp_matrices.reserve(ob.instance_info.size());

	const Vec4f campos = this->current_scene->cam_to_world.getColumn(3);

	const float IMPOSTER_FADE_DIST_START = 100.f;
	const float IMPOSTER_FADE_DIST_END   = 120.f;

	if(for_shadow_mapping)
	{
		if(!ob.is_imposter) // If not imposter:
		{
			// For each object, add transform to list of instance matrices to draw, if the object is close enough.
			for(size_t i=0; i<instance_info_size; ++i)
			{
				const Vec4f pos = instance_info[i].to_world.getColumn(3);
				const float dist2 = pos.getDist2(campos);
				if(dist2 < Maths::square(IMPOSTER_FADE_DIST_START + 5.f)) // a little crossover with shadows
					temp_matrices.push_back(instance_info[i].to_world);
			}
		}
		else // else if is imposter:
		{
			// We want to rotate the quad towards the sun.
			const Vec4f axis_k = Vec4f(0, 0, 1, 0);
			const Vec4f axis_i = -normalise(crossProduct(axis_k, sun_dir)); // Negate, as only backfaces cast shadows currently.
			const Vec4f axis_j = crossProduct(axis_k, axis_i);
			Matrix4f rot(axis_i, axis_j, axis_k, Vec4f(0,0,0,1));

			for(size_t i=0; i<instance_info_size; ++i)
			{
				const Vec4f pos = instance_info[i].to_world.getColumn(3);
				const float dist2 = pos.getDist2(campos);
				if(dist2 > Maths::square(IMPOSTER_FADE_DIST_START))
					temp_matrices.push_back(leftTranslateAffine3(sun_dir * -2.f, instance_info[i].to_world * rot)); // Nudge the shadow casting quad away from the old position, to avoid self-shadowing of the imposter quad
				// once it is rotated towards the camera
			}

			ob.materials[0].tex_matrix = Matrix2f(0.25f, 0, 0, -1.f); // Adjust texture matrix to pick just one of the texture sprites
		}
	}
	else
	{
		if(!ob.is_imposter) // If not imposter:
		{
			for(size_t i=0; i<instance_info_size; ++i)
			{
				const Vec4f pos = instance_info[i].to_world.getColumn(3);
				const float dist2 = pos.getDist2(campos);
				if(dist2 < Maths::square(IMPOSTER_FADE_DIST_END))
				{
					temp_matrices.push_back(instance_info[i].to_world);
				}
			}
		}
		else // else if is imposter:
		{
			for(size_t i=0; i<instance_info_size; ++i)
			{
				const Vec4f pos = instance_info[i].to_world.getColumn(3);
				const float dist2 = pos.getDist2(campos);
				if(dist2 > Maths::square(IMPOSTER_FADE_DIST_START))
				{
					// We want to rotate the imposter towards the camera.
					const Vec4f axis_k = Vec4f(0, 0, 1, 0);
					const Vec4f axis_i = -normalise(crossProduct(axis_k, pos - campos)); // TEMP NEGATING, needed for some reason or leftleft looks like rightlit
					const Vec4f axis_j = crossProduct(axis_k, axis_i);

					Matrix4f rot(axis_i, axis_j, axis_k, Vec4f(0,0,0,1));

					temp_matrices.push_back(instance_info[i].to_world * rot);
				}
			}

			ob.materials[0].tex_matrix = Matrix2f(1.f, 0, 0, -1.f); // Restore texture matrix
		}
	}

	ob.num_instances_to_draw = (int)temp_matrices.size();
	ob.instance_matrix_vbo->updateData(temp_matrices.data(), temp_matrices.dataSizeBytes());
}


void OpenGLEngine::draw()
{
	if(!init_succeeded)
		return;

	// Unload unused textures if we have exceeded our texture mem usage budget.
	trimTextureUsage();

	this->num_indices_submitted = 0;
	this->num_face_groups_submitted = 0;
	this->num_aabbs_submitted = 0;
	Timer profile_timer;

	this->draw_time = draw_timer.elapsed();
	uint64 shadow_depth_drawing_elapsed_ns = 0;
	double anim_update_duration = 0;


	//=============== Set animated objects state (update bone matrices etc.)===========
	Timer anim_profile_timer;
	for(auto it = current_scene->animated_objects.begin(); it != current_scene->animated_objects.end(); ++it)
	{
		GLObject* const ob = it->getPointer();

		AnimationData& anim_data = ob->mesh_data->animation_data;


		if(!anim_data.animations.empty() || !anim_data.joint_nodes.empty())
		{
			//TEMP create debug visualisation of the joints
			if(false)
			if(debug_joint_obs.empty())
			{
				debug_joint_obs.resize(256);//anim_data.sorted_nodes.size());
				for(size_t i=0; i<debug_joint_obs.size(); ++i)
				{
					debug_joint_obs[i] = new GLObject();
					debug_joint_obs[i]->mesh_data = MeshPrimitiveBuilding::make3DBasisArrowMesh(); // Base will be at origin, tip will lie at (1, 0, 0)
					debug_joint_obs[i]->materials.resize(3);
					debug_joint_obs[i]->materials[0].albedo_rgb = Colour3f(0.9f, 0.5f, 0.3f);
					debug_joint_obs[i]->materials[1].albedo_rgb = Colour3f(0.5f, 0.9f, 0.5f);
					debug_joint_obs[i]->materials[2].albedo_rgb = Colour3f(0.3f, 0.5f, 0.9f);
					//debug_joint_obs[i]->materials[0].shader_prog = getProgramWithFallbackOnError(ProgramKey("phong", /*alpha_test=*/false, /*vert_colours=*/false, /*instance_matrices=*/false, /*lightmapping=*/false,
					//	/*gen_planar_uvs=*/false, /*draw_planar_uv_grid=*/false, /*convert_albedo_from_srgb=*/false, false));
					debug_joint_obs[i]->ob_to_world_matrix = Matrix4f::translationMatrix(1000, 0, 0);

					addObject(debug_joint_obs[i]);
				}
			}
		}


		if(!anim_data.animations.empty())
		{
			ob->anim_node_data.resize(anim_data.nodes.size());

			const float DEBUG_SPEED_FACTOR = 1;
			//const float use_time = current_time * DEBUG_SPEED_FACTOR;
			
			const AnimationDatum& anim_datum_a = *anim_data.animations[myClamp(ob->current_anim_i, 0, (int)anim_data.animations.size() - 1)];
			
			const int use_next_anim_i = (ob->next_anim_i == -1) ? ob->current_anim_i : ob->next_anim_i;
			const AnimationDatum& anim_datum_b = *anim_data.animations[myClamp(use_next_anim_i,    0, (int)anim_data.animations.size() - 1)];

			const float transition_frac = (float)Maths::smoothStep<double>(ob->transition_start_time, ob->transition_end_time, current_time/*use_time*/);
			
			const float use_in_anim_time = (current_time + (float)ob->use_time_offset) * DEBUG_SPEED_FACTOR;

			/*
			For each node,
			if node translation is animated
			get keyframe i_0, i_1 and frac for input time values for sampler input accessor index.  (this computaton should be shared.)
			read translation values from output accessor
			interpolate values based on sample interpolation type
			*/
			const size_t num_nodes = anim_data.sorted_nodes.size();
			// assert(num_nodes <= 256); 
			// We only support 256 joint matrices for now.  Currently we just don't upload more than 256 matrices.
			node_matrices.resizeNoCopy(num_nodes);

			// const Matrix4f to_z_up(Vec4f(1,0,0,0), Vec4f(0, 0, 1, 0), Vec4f(0, -1, 0, 0), Vec4f(0,0,0,1));

			// Iterate over each array of keyframe times.  (each array corresponds to an input accessor)
			// For each array, find the current and next keyframe, and interpolation fraction, based on the current time.

			// TODO: not all of these input accessors will be used, only compute data for used ones.
			key_frame_locs.resize(anim_data.keyframe_times.size());
			for(int z=0; z<(int)anim_data.keyframe_times.size(); ++z) // For each input accessor:
			{
				const std::vector<float>& time_vals = anim_data.keyframe_times[z];

				if(!time_vals.empty())
				{
					if(time_vals.size() == 1)
					{
						key_frame_locs[z].i_0 = 0;
						key_frame_locs[z].i_1 = 0;
						key_frame_locs[z].frac = 0;
					}
					else
					{
						// TODO: use incremental search based on the position last frame, instead of using upper_bound.  (or combine)

						/*
						frame 0                     frame 1                        frame 2                      frame 3
						|----------------------------|-----------------------------|-----------------------------|-------------------------> time
						^                                         ^
						cur_frame_i                             in_anim_time
						*/

						assert(time_vals.size() >= 2);
						const float in_anim_time = time_vals[0] + Maths::floatMod(use_in_anim_time, time_vals.back() - time_vals[0]);

						// Find current frame
						auto res = std::upper_bound(time_vals.begin(), time_vals.end(), in_anim_time); // "Finds the position of the first element in an ordered range that has a value that is greater than a specified value"
						int next_index = (int)(res - time_vals.begin());
						int index = next_index - 1;

						next_index = myMin(next_index, (int)time_vals.size() - 1);

						float index_time;
						if(index < 0)
						{
							index = (int)time_vals.size() - 1; // Use last keyframe value
							index_time = 0;
						}
						else
							index_time = time_vals[index];

						//if(index >= 0 && next_index < time_vals.size())
						//{
							float frac;
							frac = (in_anim_time - index_time) / (time_vals[next_index] - index_time);
							
							if(!(frac >= 0 && frac <= 1)) // TEMP: handle NaNs
								frac = 0;
							//assert(frac >= 0 && frac <= 1);
						//}

						key_frame_locs[z].i_0 = index;
						key_frame_locs[z].i_1 = next_index;
						key_frame_locs[z].frac = frac;
					}
				}
			}

			for(int n=0; n<anim_data.sorted_nodes.size(); ++n)
			{
				const int node_i = anim_data.sorted_nodes[n];
				const AnimationNodeData& node_data = anim_data.nodes[node_i];
				const PerAnimationNodeData& node_a = anim_datum_a.per_anim_node_data[node_i];
				const PerAnimationNodeData& node_b = anim_datum_b.per_anim_node_data[node_i];

				Vec4f trans_a;
				if(node_a.translation_input_accessor >= 0)
				{
					//conPrint("anim_datum_a: " + anim_datum_a.name + "," + toString(anim_data.keyframe_times[node_a.translation_input_accessor].back()));

					const int i_0    = key_frame_locs[node_a.translation_input_accessor].i_0;
					const int i_1    = key_frame_locs[node_a.translation_input_accessor].i_1;
					const float frac = key_frame_locs[node_a.translation_input_accessor].frac;

					// read translation values from output accessor.
					const Vec4f trans_0 = (anim_data.output_data[node_a.translation_output_accessor])[i_0];
					const Vec4f trans_1 = (anim_data.output_data[node_a.translation_output_accessor])[i_1];
					trans_a = Maths::lerp(trans_0, trans_1, frac); // TODO: handle step interpolation, cubic lerp etc..
				}
				else
					trans_a = node_data.trans;

				Vec4f trans_b;
				if(node_b.translation_input_accessor >= 0)
				{
					const int i_0    = key_frame_locs[node_b.translation_input_accessor].i_0;
					const int i_1    = key_frame_locs[node_b.translation_input_accessor].i_1;
					const float frac = key_frame_locs[node_b.translation_input_accessor].frac;

					// read translation values from output accessor.
					const Vec4f trans_0 = (anim_data.output_data[node_b.translation_output_accessor])[i_0];
					const Vec4f trans_1 = (anim_data.output_data[node_b.translation_output_accessor])[i_1];
					trans_b = Maths::lerp(trans_0, trans_1, frac); // TODO: handle step interpolation, cubic lerp etc..
				}
				else
					trans_b = node_data.trans;

				const Vec4f trans = Maths::lerp(trans_a, trans_b, transition_frac);



				Quatf rot_a;
				if(node_a.rotation_input_accessor >= 0)
				{
					const int i_0    = key_frame_locs[node_a.rotation_input_accessor].i_0;
					const int i_1    = key_frame_locs[node_a.rotation_input_accessor].i_1;
					const float frac = key_frame_locs[node_a.rotation_input_accessor].frac;

					// read rotation values from output accessor
					const Quatf rot_0 = Quatf((anim_data.output_data[node_a.rotation_output_accessor])[i_0]);
					const Quatf rot_1 = Quatf((anim_data.output_data[node_a.rotation_output_accessor])[i_1]);
					rot_a = Quatf::nlerp(rot_0, rot_1, frac);
				}
				else
					rot_a = node_data.rot;

				Quatf rot_b;
				if(node_b.rotation_input_accessor >= 0)
				{
					const int i_0    = key_frame_locs[node_b.rotation_input_accessor].i_0;
					const int i_1    = key_frame_locs[node_b.rotation_input_accessor].i_1;
					const float frac = key_frame_locs[node_b.rotation_input_accessor].frac;

					// read rotation values from output accessor
					const Quatf rot_0 = Quatf((anim_data.output_data[node_b.rotation_output_accessor])[i_0]);
					const Quatf rot_1 = Quatf((anim_data.output_data[node_b.rotation_output_accessor])[i_1]);
					rot_b = Quatf::nlerp(rot_0, rot_1, frac);
				}
				else
					rot_b = node_data.rot;

				const Quatf rot = Quatf::nlerp(rot_a, rot_b, transition_frac);

				Vec4f scale_a;
				if(node_a.scale_input_accessor >= 0)
				{
					const int i_0    = key_frame_locs[node_a.scale_input_accessor].i_0;
					const int i_1    = key_frame_locs[node_a.scale_input_accessor].i_1;
					const float frac = key_frame_locs[node_a.scale_input_accessor].frac;

					// read scale values from output accessor
					const Vec4f scale_0 = (anim_data.output_data[node_a.scale_output_accessor])[i_0];
					const Vec4f scale_1 = (anim_data.output_data[node_a.scale_output_accessor])[i_1];
					scale_a = Maths::lerp(scale_0, scale_1, frac);
				}
				else
					scale_a = node_data.scale;

				Vec4f scale_b;
				if(node_b.scale_input_accessor >= 0)
				{
					const int i_0    = key_frame_locs[node_b.scale_input_accessor].i_0;
					const int i_1    = key_frame_locs[node_b.scale_input_accessor].i_1;
					const float frac = key_frame_locs[node_b.scale_input_accessor].frac;

					// read scale values from output accessor
					const Vec4f scale_0 = (anim_data.output_data[node_b.scale_output_accessor])[i_0];
					const Vec4f scale_1 = (anim_data.output_data[node_b.scale_output_accessor])[i_1];
					scale_b = Maths::lerp(scale_0, scale_1, frac);
				}
				else
					scale_b = node_data.scale;

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

				ob->anim_node_data[node_i].last_pre_proc_to_object = last_pre_proc_to_object;
				ob->anim_node_data[node_i].last_rot = rot;
				ob->anim_node_data[node_i].node_hierarchical_to_object = node_transform;

				// Set location of debug joint visualisation objects
				//debug_joint_obs[node_i]->ob_to_world_matrix = Matrix4f::translationMatrix(-0.5, 0, 0) * /*to_z_up * */Matrix4f::translationMatrix(node_transform.getColumn(3)) * Matrix4f::uniformScaleMatrix(0.02f);
				if(!debug_joint_obs.empty())
				{
					if(node_i == 4)
						debug_joint_obs[node_i]->ob_to_world_matrix = ob->ob_to_world_matrix * node_transform * Matrix4f::uniformScaleMatrix(0.6f);//Matrix4f::translationMatrix(-0.5, 0, 0) * /*to_z_up * */Matrix4f::translationMatrix(node_transform.getColumn(3)) * Matrix4f::uniformScaleMatrix(0.02f);
					else
						debug_joint_obs[node_i]->ob_to_world_matrix = Matrix4f::translationMatrix(1000, 0, 0);
					updateObjectTransformData(*debug_joint_obs[node_i]);
				}
			}
		}
		else // else if anim_data.animations.empty():
		{
			if(!anim_data.joint_nodes.empty()) // If we have a skin, but no animations, just use the default trans, rot, scales.
			{
				const size_t num_nodes = anim_data.sorted_nodes.size();
				node_matrices.resizeNoCopy(num_nodes);
				ob->anim_node_data.resize(num_nodes);

				for(int n=0; n<anim_data.sorted_nodes.size(); ++n)
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

					const Matrix4f node_transform = (node_data.parent_index == -1) ? TRS : (node_matrices[node_data.parent_index] * TRS);

					node_matrices[node_i] = node_transform;

					ob->anim_node_data[node_i].node_hierarchical_to_object = node_transform;


					// Set location of debug joint visualisation objects
					//debug_joint_obs[node_i]->ob_to_world_matrix = Matrix4f::translationMatrix(-0.5, 0, 0) * /*to_z_up * */Matrix4f::translationMatrix(node_transform.getColumn(3)) * Matrix4f::uniformScaleMatrix(0.02f);
					if(!debug_joint_obs.empty())
					{
						if(node_i == 27 || node_i == 28)
							debug_joint_obs[node_i]->ob_to_world_matrix = ob->ob_to_world_matrix * node_transform * Matrix4f::uniformScaleMatrix(0.2f);//Matrix4f::translationMatrix(-0.5, 0, 0) * /*to_z_up * */Matrix4f::translationMatrix(node_transform.getColumn(3)) * Matrix4f::uniformScaleMatrix(0.02f);
						else
							debug_joint_obs[node_i]->ob_to_world_matrix = Matrix4f::translationMatrix(1000, 0, 0);
						updateObjectTransformData(*debug_joint_obs[node_i]);
					}
				}
			}
		}
	}
	anim_update_duration = anim_profile_timer.elapsed();


	//=============== Render to shadow map depth buffer if needed ===========
	if(shadow_mapping.nonNull())
	{
		// Set instance matrices for imposters.
		for(auto it = current_scene->objects_with_imposters.begin(); it != current_scene->objects_with_imposters.end(); ++it)
		{
			GLObject* const ob = it->getPointer();
			updateInstanceMatricesForObWithImposters(*ob, /*for_shadow_mapping=*/true);
		}

#if !defined(OSX)
		if(profiling_enabled) glBeginQuery(GL_TIME_ELAPSED, timer_query_id);
#endif
		//-------------------- Draw dynamic depth textures ----------------
		shadow_mapping->bindDepthTexFrameBufferAsTarget();

		glClear(GL_DEPTH_BUFFER_BIT);

		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);

		const int per_map_h = shadow_mapping->dynamic_h / shadow_mapping->numDynamicDepthTextures();

		for(int ti=0; ti<shadow_mapping->numDynamicDepthTextures(); ++ti)
		{
			glViewport(0, ti*per_map_h, shadow_mapping->dynamic_w, per_map_h);

// Code before here works
#if 1
// Code after here works too
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
			const Vec4f k = sun_dir;
			const Vec4f i = normalise(crossProduct(up, sun_dir)); // right 
			const Vec4f j = crossProduct(k, i); // up

			assert(k.isUnitLength());

			// Get bounds of the view frustum slice along i, j, k vectors
			float min_i = std::numeric_limits<float>::max();
			float min_j = std::numeric_limits<float>::max();
			float min_k = std::numeric_limits<float>::max();
			float max_i = -std::numeric_limits<float>::max();
			float max_j = -std::numeric_limits<float>::max();
			float max_k = -std::numeric_limits<float>::max();
			
			for(int z=0; z<8; ++z)
			{
				assert(i[3] == 0 && j[3] == 0 && k[3] == 0);
				const float dot_i = dot(i, frustum_verts_ws[z]);
				const float dot_j = dot(j, frustum_verts_ws[z]);
				const float dot_k = dot(k, frustum_verts_ws[z]);
				min_i = myMin(min_i, dot_i);
				min_j = myMin(min_j, dot_j);
				min_k = myMin(min_k, dot_k);
				max_i = myMax(max_i, dot_i);
				max_j = myMax(max_j, dot_j);
				max_k = myMax(max_k, dot_k);
			}

			// We want objects some distance outside of the view frustum to be able to cast shadows as well
			const float max_shadowing_dist = 300.0f;

			const float use_max_k = myMax(max_k - min_k, min_k + max_shadowing_dist); // extend k bound to encompass max_shadowing_dist from min_k.

			const float near_signed_dist = -use_max_k; // k is towards sun so negate
			const float far_signed_dist = -min_k;

			Matrix4f proj_matrix = orthoMatrix(
				min_i, max_i, // left, right
				min_j, max_j, // bottom, top
				near_signed_dist, far_signed_dist // near, far
			);

			
			// TEMP: compute verts of shadow volume
			Vec4f shadow_vol_verts[8];
			shadow_vol_verts[0] = Vec4f(0,0,0,1) + i*min_i + j*max_j + k*use_max_k;
			shadow_vol_verts[1] = Vec4f(0,0,0,1) + i*max_i + j*max_j + k*use_max_k;
			shadow_vol_verts[2] = Vec4f(0,0,0,1) + i*max_i + j*min_j + k*use_max_k;
			shadow_vol_verts[3] = Vec4f(0,0,0,1) + i*min_i + j*min_j + k*use_max_k;
			shadow_vol_verts[4] = Vec4f(0,0,0,1) + i*min_i + j*max_j + k*min_k;
			shadow_vol_verts[5] = Vec4f(0,0,0,1) + i*max_i + j*max_j + k*min_k;
			shadow_vol_verts[6] = Vec4f(0,0,0,1) + i*max_i + j*min_j + k*min_k;
			shadow_vol_verts[7] = Vec4f(0,0,0,1) + i*min_i + j*min_j + k*min_k;

			Matrix4f view_matrix;
			view_matrix.setRow(0, i);
			view_matrix.setRow(1, j);
			view_matrix.setRow(2, k);
			view_matrix.setRow(3, Vec4f(0, 0, 0, 1));

			// Compute clipping planes for shadow mapping
			shadow_clip_planes[0] = Planef( i, max_i);
			shadow_clip_planes[1] = Planef(-i, -min_i);
			shadow_clip_planes[2] = Planef( j, max_j);
			shadow_clip_planes[3] = Planef(-j, -min_j);
			shadow_clip_planes[4] = Planef( k, use_max_k);
			shadow_clip_planes[5] = Planef(-k, -min_k);

			js::AABBox shadow_vol_aabb = js::AABBox::emptyAABBox(); // AABB of shadow rendering volume.
			for(int z=0; z<8; ++z)
				shadow_vol_aabb.enlargeToHoldPoint(shadow_vol_verts[z]);

			
			// We need to a texcoord bias matrix to go from [-1, 1] to [0, 1] coord range.
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
				std::memset(&uniforms, 0, sizeof(SharedVertUniforms)); // Zero because we are not going to set all uniforms.
				uniforms.proj_matrix = proj_matrix;
				uniforms.view_matrix = view_matrix;
				uniforms.vert_uniforms_time = current_time;
				uniforms.wind_strength = current_scene->wind_strength;
				this->shared_vert_uniform_buf_ob->updateData(/*dest offset=*/0, &uniforms, sizeof(SharedVertUniforms));
			}


			// Draw fully opaque batches - batches with a material that is not transparent and doesn't use alpha testing.
			//uint64 num_drawn = 0;
			//uint64 num_in_frustum = 0;

			batch_draw_info.reserve(current_scene->objects.size());
			batch_draw_info.resize(0);

			uint64 num_frustum_culled = 0;
			for(auto it = current_scene->objects.begin(); it != current_scene->objects.end(); ++it)
			{
				const GLObject* const ob = it->getPointer();
				if(AABBIntersectsFrustum(shadow_clip_planes, /*num clip planes=*/6, shadow_vol_aabb, ob->aabb_ws))
				{
					if(largestDim(ob->aabb_ws) < (max_i - min_i) * 0.002f)
						continue;

					const OpenGLMeshRenderData& mesh_data = *ob->mesh_data;
					for(uint32 z = 0; z < mesh_data.batches.size(); ++z)
					{
						const uint32 mat_index = mesh_data.batches[z].material_index;
						if(mat_index < ob->materials.size()) // This can happen for some dodgy meshes.  TODO: Filter such batches out in mesh creation stage.
						{
							if(!ob->materials[mat_index].transparent) // Don't draw shadows from transparent obs
							{
								BatchDrawInfo info;
								info.ob = ob;
								info.batch = &mesh_data.batches[z];
								info.mat = &ob->materials[mat_index];
								info.prog = ob->materials[mat_index].depth_draw_shader_prog.ptr();

								batch_draw_info.push_back(info);
							}
						}
					}
				}
				else
					num_frustum_culled++;
			}

			// Sort by shader program
			std::sort(batch_draw_info.begin(), batch_draw_info.end());

			// Draw sorted batches
			num_prog_changes = 0;
			//const OpenGLMeshRenderData* last_mesh_data = NULL;
			for(size_t z = 0; z < batch_draw_info.size(); ++z)
			{
				const BatchDrawInfo& info = batch_draw_info[z];
				bindMeshData(*info.ob);
				drawBatch(*info.ob, view_matrix, proj_matrix, *info.mat, *info.prog, *info.ob->mesh_data, *info.batch);
			}
			//if(last_mesh_data) unbindMeshData(*last_mesh_data);
			OpenGLProgram::useNoPrograms();
			current_bound_prog = NULL;

			//conPrint("Level " + toString(ti) + ": " + toString(num_drawn) + " / " + toString(current_scene->objects.size()/*num_in_frustum*/) + " drawn.");
#endif
			// Code after here works
		}

		shadow_mapping->unbindFrameBuffer();
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
			// Bind the non-current ('other') static depth map.  We will render to that.
			const int other_index = (shadow_mapping->cur_static_depth_tex + 1) % 2;

			shadow_mapping->bindStaticDepthTexFrameBufferAsTarget(other_index);

			const uint32 ti = (frame_num % 12) / 4; // Texture index, in [0, numStaticDepthTextures)
			const uint32 ob_set = frame_num % 4;    // Object set, in [0, 4)

			{
				//conPrint("At frame " + toString(frame_num) + ", drawing static cascade " + toString(ti));

				const int static_per_map_h = shadow_mapping->static_h / shadow_mapping->numStaticDepthTextures();
				glViewport(/*x=*/0, /*y=*/ti*static_per_map_h, /*width=*/shadow_mapping->static_w, /*height=*/static_per_map_h);

				// Code before here works
#if 1
				if(ob_set == 0)
				{
					glDisable(GL_CULL_FACE);
					partiallyClearBuffer(Vec2f(0, 0), Vec2f(1, 1));
					glEnable(GL_CULL_FACE);
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
				const Vec4f campos_ws = current_scene->cam_to_world * Vec4f(0, 0, 0, 1);
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
				js::getAABBCornerVerts(js::AABBox(
					vol_centre - Vec4f(w, w, h, 0),
					vol_centre + Vec4f(w, w, h, 0)
				), frustum_verts_ws);

				const Vec4f up(0, 0, 1, 0);
				const Vec4f k = sun_dir;
				const Vec4f i = normalise(crossProduct(up, sun_dir)); // right 
				const Vec4f j = crossProduct(k, i); // up

				assert(k.isUnitLength());

				// Get bounds along i, j, k vectors
				float min_i = std::numeric_limits<float>::max();
				float min_j = std::numeric_limits<float>::max();
				float min_k = std::numeric_limits<float>::max();
				float max_i = -std::numeric_limits<float>::max();
				float max_j = -std::numeric_limits<float>::max();
				float max_k = -std::numeric_limits<float>::max();

				for(int z=0; z<8; ++z)
				{
					assert(i[3] == 0 && j[3] == 0 && k[3] == 0);
					const float dot_i = dot(i, frustum_verts_ws[z]);
					const float dot_j = dot(j, frustum_verts_ws[z]);
					const float dot_k = dot(k, frustum_verts_ws[z]);
					min_i = myMin(min_i, dot_i);
					min_j = myMin(min_j, dot_j);
					min_k = myMin(min_k, dot_k);
					max_i = myMax(max_i, dot_i);
					max_j = myMax(max_j, dot_j);
					max_k = myMax(max_k, dot_k);
				}

				const float max_shadowing_dist = 300.0f;

				float use_max_k = myMax(max_k - min_k, min_k + max_shadowing_dist);

				const float near_signed_dist = -use_max_k;
				const float far_signed_dist = -min_k;

				const Matrix4f proj_matrix = orthoMatrix(
					min_i, max_i, // left, right
					min_j, max_j, // bottom, top
					near_signed_dist, far_signed_dist // near, far
				);


				// TEMP: compute verts of shadow volume
				Vec4f shadow_vol_verts[8];
				shadow_vol_verts[0] = Vec4f(0, 0, 0, 1) + i*min_i + j*max_j + k*use_max_k;
				shadow_vol_verts[1] = Vec4f(0, 0, 0, 1) + i*max_i + j*max_j + k*use_max_k;
				shadow_vol_verts[2] = Vec4f(0, 0, 0, 1) + i*max_i + j*min_j + k*use_max_k;
				shadow_vol_verts[3] = Vec4f(0, 0, 0, 1) + i*min_i + j*min_j + k*use_max_k;
				shadow_vol_verts[4] = Vec4f(0, 0, 0, 1) + i*min_i + j*max_j + k*min_k;
				shadow_vol_verts[5] = Vec4f(0, 0, 0, 1) + i*max_i + j*max_j + k*min_k;
				shadow_vol_verts[6] = Vec4f(0, 0, 0, 1) + i*max_i + j*min_j + k*min_k;
				shadow_vol_verts[7] = Vec4f(0, 0, 0, 1) + i*min_i + j*min_j + k*min_k;

				Matrix4f view_matrix;
				view_matrix.setRow(0, i);
				view_matrix.setRow(1, j);
				view_matrix.setRow(2, k);
				view_matrix.setRow(3, Vec4f(0, 0, 0, 1));

				// Compute clipping planes for shadow mapping
				shadow_clip_planes[0] = Planef( i, max_i);
				shadow_clip_planes[1] = Planef(-i, -min_i);
				shadow_clip_planes[2] = Planef( j, max_j);
				shadow_clip_planes[3] = Planef(-j, -min_j);
				shadow_clip_planes[4] = Planef( k, use_max_k);
				shadow_clip_planes[5] = Planef(-k, -min_k);

				js::AABBox shadow_vol_aabb = js::AABBox::emptyAABBox(); // AABB of shadow rendering volume.
				for(int z=0; z<8; ++z)
					shadow_vol_aabb.enlargeToHoldPoint(shadow_vol_verts[z]);

				// We need to a texcoord bias matrix to go from [-1, 1] to [0, 1] coord range.
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
				//uint64 num_drawn = 0;
				//uint64 num_in_frustum = 0;

				// Update shared uniforms
				{
					SharedVertUniforms uniforms;
					std::memset(&uniforms, 0, sizeof(SharedVertUniforms)); // Zero because we are not going to set all uniforms.
					uniforms.proj_matrix = proj_matrix;
					uniforms.view_matrix = view_matrix;
					uniforms.vert_uniforms_time = current_time;
					uniforms.wind_strength = current_scene->wind_strength;
					this->shared_vert_uniform_buf_ob->updateData(/*dest offset=*/0, &uniforms, sizeof(SharedVertUniforms));
				}


				batch_draw_info.reserve(current_scene->objects.size());
				batch_draw_info.resize(0);

				uint64 num_frustum_culled = 0;
				for(auto it = current_scene->objects.begin(); it != current_scene->objects.end(); ++it)
				{
					const GLObject* const ob = it->getPointer();
					if((ob->random_num & 0x3) == ob_set) // Only draw objects in ob_set (check by comparing lowest 2 bits with ob_set)
					{
						if(AABBIntersectsFrustum(shadow_clip_planes, /*num clip planes=*/6, shadow_vol_aabb, ob->aabb_ws))
						{
							if(largestDim(ob->aabb_ws) < (max_i - min_i) * 0.001f)
								continue;

							const OpenGLMeshRenderData& mesh_data = *ob->mesh_data;
							for(uint32 z = 0; z < mesh_data.batches.size(); ++z)
							{
								const uint32 mat_index = mesh_data.batches[z].material_index;
								if(mat_index < ob->materials.size()) // This can happen for some dodgy meshes.  TODO: Filter such batches out in mesh creation stage.
								{
									if(!ob->materials[mat_index].transparent) // Don't draw shadows from transparent obs
									{
										BatchDrawInfo info;
										info.ob = ob;
										info.batch = &mesh_data.batches[z];
										info.mat = &ob->materials[mat_index];
										info.prog = ob->materials[mat_index].depth_draw_shader_prog.ptr();
										batch_draw_info.push_back(info);
									}
								}
							}
						}
						else
							num_frustum_culled++;
					}
				}

				// Sort by shader program
				std::sort(batch_draw_info.begin(), batch_draw_info.end());

				// Draw sorted batches
				num_prog_changes = 0;
				//const OpenGLMeshRenderData* last_mesh_data = NULL;
				for(size_t z = 0; z < batch_draw_info.size(); ++z)
				{
					const BatchDrawInfo& info = batch_draw_info[z];
					bindMeshData(*info.ob);
					drawBatch(*info.ob, view_matrix, proj_matrix, *info.mat, *info.prog, *info.ob->mesh_data, *info.batch);
					//unbindMeshData(*info.ob); // TEMP
				}
				//if(last_mesh_data) unbindMeshData(*last_mesh_data);
				OpenGLProgram::useNoPrograms();
				this->current_bound_prog = NULL;

				//conPrint("Static shadow map Level " + toString(ti) + ": ob set: " + toString(ob_set) + " " + toString(num_drawn) + " / " + toString(current_scene->objects.size()/*num_in_frustum*/) + " drawn. (CPU time: " + timer3.elapsedStringNSigFigs(3) + ")");
#endif
// Code after here works
			}

			shadow_mapping->unbindFrameBuffer();

			if(frame_num % 12 == 11) // If we just finished drawing to our 'other' depth map, swap cur and other
				shadow_mapping->setCurStaticDepthTex((shadow_mapping->cur_static_depth_tex + 1) % 2); // Swap cur and other
		}
		//-------------------- End draw static depth textures ----------------

		if(this->target_frame_buffer.nonNull())
			glBindFramebuffer(GL_FRAMEBUFFER, this->target_frame_buffer->buffer_name);

		glDisable(GL_CULL_FACE);

#if !defined(OSX)
		if(profiling_enabled)
		{
			glEndQuery(GL_TIME_ELAPSED);
			glGetQueryObjectui64v(timer_query_id, GL_QUERY_RESULT, &shadow_depth_drawing_elapsed_ns); // Blocks
		}
#endif
	} // End if(shadow_mapping.nonNull())

#if !defined(OSX)
	if(profiling_enabled) glBeginQuery(GL_TIME_ELAPSED, timer_query_id); // Start measuring everything else after depth buffer drawing:
#endif

	//------------------------------ water ----------------------------------------
#if 0
	if(pre_water_framebuffer.isNull())
		pre_water_framebuffer = new FrameBuffer();

	if(pre_water_colour_tex.isNull())
	{
		conPrint("Allocing pre_water_colour_tex with width " + toString(viewport_w) + " and height " + toString(viewport_h));
		pre_water_colour_tex = new OpenGLTexture(viewport_w, viewport_h, this,
			OpenGLTexture::Format_RGB_Linear_Float,
			OpenGLTexture::Filtering_Bilinear,
			OpenGLTexture::Wrapping_Clamp // Clamp texture reads otherwise edge outlines will wrap around to other side of frame.
		);

		pre_water_depth_tex = new OpenGLTexture(viewport_w, viewport_h, this,
			OpenGLTexture::Format_Depth_Float,
			OpenGLTexture::Filtering_Bilinear,
			OpenGLTexture::Wrapping_Clamp // Clamp texture reads otherwise edge outlines will wrap around to other side of frame.
		);
	}

	pre_water_framebuffer->bindTextureAsTarget(*pre_water_colour_tex, GL_COLOR_ATTACHMENT0);
	pre_water_framebuffer->bindTextureAsTarget(*pre_water_depth_tex,  GL_DEPTH_ATTACHMENT);
	pre_water_framebuffer->bind();
#endif
	//------------------------------ end water ----------------------------------------


	// We will render to main_render_framebuffer / main_render_texture / main_depth_texture
	FrameBufTextures current_framebuf_textures;
	if(settings.use_final_image_buffer)
	{
		if(main_render_framebuffer.isNull())
			main_render_framebuffer = new FrameBuffer();

		// Allocate a framebuffer for this viewport width and height
		// We support multiple framebuffer sizes so viewport can be changed at runtime without constant reallocation.
		// TODO: improve: use max of all sizes seen maybe?  then need to adjust read UVs

		auto res = main_render_textures.find(Vec2i(viewport_w, viewport_h));
		if(res == main_render_textures.end())
		{
			conPrint("Allocing main_render_texture with width " + toString(viewport_w) + " and height " + toString(viewport_h));
			OpenGLTextureRef main_render_texture = new OpenGLTexture(viewport_w, viewport_h, this,
				OpenGLTexture::Format_RGB_Linear_Float,
				OpenGLTexture::Filtering_Bilinear,
				OpenGLTexture::Wrapping_Clamp // Clamp texture reads otherwise edge outlines will wrap around to other side of frame.
			);

			OpenGLTextureRef main_depth_texture = new OpenGLTexture(viewport_w, viewport_h, this,
				OpenGLTexture::Format_Depth_Float,
				OpenGLTexture::Filtering_Bilinear,
				OpenGLTexture::Wrapping_Clamp // Clamp texture reads otherwise edge outlines will wrap around to other side of frame.
			);

			FrameBufTextures textures;
			textures.colour_texture = main_render_texture;
			textures.depth_texture = main_depth_texture;

			main_render_textures.insert(std::make_pair(Vec2i(viewport_w, viewport_h), textures));

			current_framebuf_textures = textures;
		}
		else
		{
			current_framebuf_textures = res->second;
		}

		main_render_framebuffer->bindTextureAsTarget(*current_framebuf_textures.colour_texture, GL_COLOR_ATTACHMENT0);
		main_render_framebuffer->bindTextureAsTarget(*current_framebuf_textures.depth_texture,  GL_DEPTH_ATTACHMENT);
		main_render_framebuffer->bind();
	}
	else
	{
		// Bind requested target frame buffer as output buffer
		glBindFramebuffer(GL_FRAMEBUFFER, this->target_frame_buffer.nonNull() ? this->target_frame_buffer->buffer_name : 0);
	}


	glViewport(0, 0, viewport_w, viewport_h); // Viewport may have been changed by shadow mapping.
	
	// NOTE: We want to clear here first, even if the scene node is null.
	// Clearing here fixes the bug with the OpenGL widget buffer not being initialised properly and displaying garbled mem on OS X.
	glClearColor(current_scene->background_colour.r, current_scene->background_colour.g, current_scene->background_colour.b, 1.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	glLineWidth(1);

	const double unit_shift_up    = this->current_scene->lens_shift_up_distance    / this->current_scene->lens_sensor_dist;
	const double unit_shift_right = this->current_scene->lens_shift_right_distance / this->current_scene->lens_sensor_dist;

	Matrix4f proj_matrix;
	
	// Initialise projection matrix from Indigo camera settings
	const double z_far  = current_scene->max_draw_dist;
	const double z_near = current_scene->near_draw_dist;

	const float viewport_aspect_ratio = this->getViewPortAspectRatio();

	if(current_scene->camera_type == OpenGLScene::CameraType_Perspective)
	{
		double w, h;
		if(viewport_aspect_ratio > current_scene->render_aspect_ratio)
		{
			h = 0.5 * (current_scene->sensor_width / current_scene->lens_sensor_dist) / current_scene->render_aspect_ratio;
			w = h * viewport_aspect_ratio;
		}
		else
		{
			w = 0.5 * current_scene->sensor_width / current_scene->lens_sensor_dist;
			h = w / viewport_aspect_ratio;
		}

		const double x_min = z_near * (-w + unit_shift_right);
		const double x_max = z_near * ( w + unit_shift_right);
		const double y_min = z_near * (-h + unit_shift_up);
		const double y_max = z_near * ( h + unit_shift_up);
		proj_matrix = frustumMatrix(x_min, x_max, y_min, y_max, z_near, z_far);
	}
	else if(current_scene->camera_type == OpenGLScene::CameraType_Orthographic)
	{
		const double sensor_height = current_scene->sensor_width / current_scene->render_aspect_ratio;

		if(viewport_aspect_ratio > current_scene->render_aspect_ratio)
		{
			// Match on the vertical clip planes.
			proj_matrix = orthoMatrix(
				-sensor_height * 0.5 * viewport_aspect_ratio, // left
				 sensor_height * 0.5 * viewport_aspect_ratio, // right
				-sensor_height * 0.5, // bottom
				 sensor_height * 0.5, // top
				z_near,
				z_far
			);
		}
		else
		{
			// Match on the horizontal clip planes.
			proj_matrix = orthoMatrix(
				-current_scene->sensor_width * 0.5, // left
				 current_scene->sensor_width * 0.5, // right
				-current_scene->sensor_width * 0.5 / viewport_aspect_ratio, // bottom
				 current_scene->sensor_width * 0.5 / viewport_aspect_ratio, // top
				z_near,
				z_far
			);
		}
	}
	else if(current_scene->camera_type == OpenGLScene::CameraType_DiagonalOrthographic)
	{
		const double sensor_height = current_scene->sensor_width / current_scene->render_aspect_ratio;

		const float cam_z = current_scene->cam_to_world.getColumn(3)[2];

		if(viewport_aspect_ratio > current_scene->render_aspect_ratio)
		{
			// Match on the vertical clip planes.
			proj_matrix = diagonalOrthoMatrix(
				-sensor_height * 0.5 * viewport_aspect_ratio, // left
				sensor_height * 0.5 * viewport_aspect_ratio, // right
				-sensor_height * 0.5, // bottom
				sensor_height * 0.5, // top
				z_near,
				z_far,
				cam_z
			);
		}
		else
		{
			// Match on the horizontal clip planes.
			proj_matrix = diagonalOrthoMatrix(
				-current_scene->sensor_width * 0.5, // left
				current_scene->sensor_width * 0.5, // right
				-current_scene->sensor_width * 0.5 / viewport_aspect_ratio, // bottom
				current_scene->sensor_width * 0.5 / viewport_aspect_ratio, // top
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

	const float e[16] = { 1, 0, 0, 0,	0, 0, -1, 0,	0, 1, 0, 0,		0, 0, 0, 1 };
	const Matrix4f indigo_to_opengl_cam_matrix = current_scene->use_z_up ? Matrix4f(e) : Matrix4f::identity();

	const Matrix4f view_matrix = indigo_to_opengl_cam_matrix * current_scene->world_to_camera_space_matrix;

	this->sun_dir_cam_space = indigo_to_opengl_cam_matrix * (current_scene->world_to_camera_space_matrix * sun_dir);

	// Draw solid polygons
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);


	
	// Draw outlines around selected object(s).
	// * Draw object with flat uniform shading to a render frame buffer.
	// * Make render buffer a texture
	// * Draw a quad over the main viewport, using a shader that does a Sobel filter, to detect the edges.  write to another frame buffer.
	// * Draw another quad using the edge texture, blending a green colour into the final frame buffer on the edge.

	//================= Generate outline texture =================
	if(!selected_objects.empty())
	{
		// Make outline textures if they have not been created, or are the wrong size.
		if(outline_tex_w != myMax<size_t>(16, viewport_w) || outline_tex_h != myMax<size_t>(16, viewport_h))
		{
			buildOutlineTextures();
		}

		// -------------------------- Stage 1: draw flat selected objects. --------------------
		outline_solid_framebuffer->bind();
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, outline_solid_tex->texture_handle, 0);
		glViewport(0, 0, (GLsizei)outline_tex_w, (GLsizei)outline_tex_h); // Make viewport same size as texture.
		glClearColor(0.f, 0.f, 0.f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		for(auto i = selected_objects.begin(); i != selected_objects.end(); ++i)
		{
			const GLObject* const ob = *i;
			if(AABBIntersectsFrustum(current_scene->frustum_clip_planes, current_scene->num_frustum_clip_planes, current_scene->frustum_aabb, ob->aabb_ws))
			{
				const OpenGLMeshRenderData& mesh_data = *ob->mesh_data;
				bindMeshData(*ob); // Bind the mesh data, which is the same for all batches.
				for(uint32 z = 0; z < mesh_data.batches.size(); ++z)
					drawBatch(*ob, view_matrix, proj_matrix, outline_solid_mat, *outline_solid_mat.shader_prog, mesh_data, mesh_data.batches[z]); // Draw object with outline_mat.
				unbindMeshData(*ob);
			}
		}

		outline_solid_framebuffer->unbind();
	
		// ------------------- Stage 2: Extract edges with Sobel filter---------------------
		// Shader reads from outline_solid_tex, writes to outline_edge_tex.
	
		outline_edge_framebuffer->bind();
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, outline_edge_tex->texture_handle, 0);
		glDepthMask(GL_FALSE); // Don't write to z-buffer, depth not needed.

		edge_extract_prog->useProgram();

		// Position quad to cover viewport
		const Matrix4f ob_to_world_matrix = Matrix4f::scaleMatrix(2.f, 2.f, 1.f) * Matrix4f::translationMatrix(Vec4f(-0.5, -0.5, 0, 0));

		bindMeshData(*outline_quad_meshdata); // Bind the mesh data, which is the same for all batches.
		for(uint32 z = 0; z < outline_quad_meshdata->batches.size(); ++z)
		{
			glUniformMatrix4fv(edge_extract_prog->model_matrix_loc, 1, false, ob_to_world_matrix.e);

			glActiveTexture(GL_TEXTURE0 + 0);
			glBindTexture(GL_TEXTURE_2D, outline_solid_tex->texture_handle);
			glUniform1i(edge_extract_tex_location, 0);

			glUniform4fv(edge_extract_col_location, 1, outline_colour.x);
				
			glDrawElements(GL_TRIANGLES, (GLsizei)outline_quad_meshdata->batches[0].num_indices, outline_quad_meshdata->index_type, (void*)(uint64)outline_quad_meshdata->batches[0].prim_start_offset);
		}
		unbindMeshData(*outline_quad_meshdata);

		edge_extract_prog->useNoPrograms();

		glDepthMask(GL_TRUE); // Restore writing to z-buffer.
		outline_edge_framebuffer->unbind();

		if(settings.use_final_image_buffer)
			main_render_framebuffer->bind(); // Restore main render framebuffer binding.
		else
			glBindFramebuffer(GL_FRAMEBUFFER, this->target_frame_buffer.nonNull() ? this->target_frame_buffer->buffer_name : 0);

		glViewport(0, 0, viewport_w, viewport_h); // Restore viewport
	}


	// Draw background env map if there is one. (or if we are using a non-standard env shader)
	if((this->current_scene->env_ob->materials[0].shader_prog.nonNull() && (this->current_scene->env_ob->materials[0].shader_prog.ptr() != this->env_prog.ptr())) || this->current_scene->env_ob->materials[0].albedo_texture.nonNull())
	{
		Matrix4f world_to_camera_space_no_translation = view_matrix;
		world_to_camera_space_no_translation.e[12] = 0;
		world_to_camera_space_no_translation.e[13] = 0;
		world_to_camera_space_no_translation.e[14] = 0;

		glDepthMask(GL_FALSE); // Disable writing to depth buffer.

		Matrix4f use_proj_mat;
		if(current_scene->camera_type == OpenGLScene::CameraType_Orthographic)
		{
			// Use a perpective transformation for rendering the env sphere, with a few narrow field of view, to provide just a hint of texture detail.
			const float w = 0.01f;
			use_proj_mat = frustumMatrix(-w, w, -w, w, 0.5, 100);
		}
		else
			use_proj_mat = proj_matrix;

		bindMeshData(*current_scene->env_ob);
		drawBatch(*current_scene->env_ob, world_to_camera_space_no_translation, use_proj_mat, current_scene->env_ob->materials[0], *current_scene->env_ob->materials[0].shader_prog, *current_scene->env_ob->mesh_data, current_scene->env_ob->mesh_data->batches[0]);
		unbindMeshData(*current_scene->env_ob);
			
		glDepthMask(GL_TRUE); // Re-enable writing to depth buffer.
	}
	
	//================= Draw non-transparent batches from objects =================

	//glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
	//glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Required for logarithmic depth buffer to avoid clipping artifacts.
	if(settings.use_logarithmic_depth_buffer)
		glEnable(GL_DEPTH_CLAMP);

	// Update shared phong uniforms
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
		uniforms.vert_uniforms_time = current_time;
		uniforms.wind_strength = current_scene->wind_strength;

		this->shared_vert_uniform_buf_ob->updateData(/*dest offset=*/0, &uniforms, sizeof(SharedVertUniforms));
	}

#if 1
	// Set instance matrices for imposters.
	for(auto it = current_scene->objects_with_imposters.begin(); it != current_scene->objects_with_imposters.end(); ++it)
	{
		GLObject* const ob = it->getPointer();
		if(AABBIntersectsFrustum(current_scene->frustum_clip_planes, current_scene->num_frustum_clip_planes, current_scene->frustum_aabb, ob->aabb_ws))
			updateInstanceMatricesForObWithImposters(*ob, /*for_shadow_mapping=*/false);
	}


#if 0
	if(terrain_system.nonNull())
	{
		//terrain_system->render(*this);

		for(auto it = terrain_system->chunks.begin(); it != terrain_system->chunks.end(); ++it)
		{
			TerrainChunk& chunk = it->second;

			//------------------------------ Draw terrain ------------------------------
			{
				OpenGLMeshRenderData* meshdata = chunk.mesh_data_LODs[0].ptr();
				bindMeshData(*meshdata);

				const OpenGLProgram* shader_prog = terrain_system->mat.shader_prog.ptr();
				shader_prog->useProgram();

				// Set uniforms.  NOTE: Setting the uniforms manually in this way (switching on shader program) is obviously quite hacky.  Improve.

				// Set per-object vert uniforms
				if(shader_prog->uses_vert_uniform_buf_obs)
				{
					Matrix4f ob_to_world_matrix = Matrix4f::identity();
					Matrix4f ob_to_world_inv_transpose_matrix = Matrix4f::identity();

					PerObjectVertUniforms uniforms;
					uniforms.model_matrix = ob_to_world_matrix;
					uniforms.normal_matrix = ob_to_world_inv_transpose_matrix;

					this->per_object_vert_uniform_buf_ob->updateData(/*dest offset=*/0, &uniforms, sizeof(PerObjectVertUniforms));
				}
				else
				{
					Matrix4f ob_to_world_matrix = Matrix4f::identity();
					Matrix4f ob_to_world_inv_transpose_matrix = Matrix4f::identity();
					glUniformMatrix4fv(shader_prog->model_matrix_loc, 1, false, ob_to_world_matrix.e);
					glUniformMatrix4fv(shader_prog->normal_matrix_loc, 1, false, ob_to_world_inv_transpose_matrix.e); // inverse transpose model matrix

					glUniformMatrix4fv(shader_prog->view_matrix_loc, 1, false, view_matrix.e);
					glUniformMatrix4fv(shader_prog->proj_matrix_loc, 1, false, proj_matrix.e);
				}

				//if(shader_prog->is_phong)
				{
					setUniformsForPhongProg(terrain_system->mat, *meshdata, shader_prog->uniform_locations, /*prog_changed=*/true);
				}
			
				GLenum draw_mode = GL_TRIANGLES;

				glDrawElements(draw_mode, (GLsizei)meshdata->batches[0].num_indices, meshdata->index_type, (void*)(uint64)meshdata->batches[0].prim_start_offset);

				this->num_indices_submitted += meshdata->batches[0].num_indices;
				this->num_face_groups_submitted++;
			}

			//------------------------------ Draw water ------------------------------
			{
				OpenGLMeshRenderData* meshdata = chunk.water_mesh_data_LODs[0].ptr();
				bindMeshData(*meshdata);

				const OpenGLProgram* shader_prog = terrain_system->water_mat.shader_prog.ptr();
				shader_prog->useProgram();


				// Set uniforms.  NOTE: Setting the uniforms manually in this way (switching on shader program) is obviously quite hacky.  Improve.

				// Set per-object vert uniforms
				if(shader_prog->uses_vert_uniform_buf_obs)
				{
					Matrix4f ob_to_world_matrix = Matrix4f::identity();
					Matrix4f ob_to_world_inv_transpose_matrix = Matrix4f::identity();

					PerObjectVertUniforms uniforms;
					uniforms.model_matrix = ob_to_world_matrix;
					uniforms.normal_matrix = ob_to_world_inv_transpose_matrix;

					this->per_object_vert_uniform_buf_ob->updateData(/*dest offset=*/0, &uniforms, sizeof(PerObjectVertUniforms));
				}
				else
				{
					Matrix4f ob_to_world_matrix = Matrix4f::identity();
					Matrix4f ob_to_world_inv_transpose_matrix = Matrix4f::identity();
					glUniformMatrix4fv(shader_prog->model_matrix_loc, 1, false, ob_to_world_matrix.e);
					glUniformMatrix4fv(shader_prog->normal_matrix_loc, 1, false, ob_to_world_inv_transpose_matrix.e); // inverse transpose model matrix

					glUniformMatrix4fv(shader_prog->view_matrix_loc, 1, false, view_matrix.e);
					glUniformMatrix4fv(shader_prog->proj_matrix_loc, 1, false, proj_matrix.e);
				}

				if(this->fbm_tex.nonNull())
				{
					glActiveTexture(GL_TEXTURE0 + 2);
					glBindTexture(GL_TEXTURE_2D, this->fbm_tex->texture_handle);
					glUniform1i(terrain_system->water_fbm_tex_location, 2);
				}
				if(this->cirrus_tex.nonNull())
				{
					glActiveTexture(GL_TEXTURE0 + 3);
					glBindTexture(GL_TEXTURE_2D, this->cirrus_tex->texture_handle);
					glUniform1i(terrain_system->water_cirrus_tex_location, 3);
				}

				if(shader_prog->time_loc >= 0)
					glUniform1f(shader_prog->time_loc, this->current_time);

				glUniform4fv(terrain_system->water_sundir_cs_location, /*count=*/1, this->sun_dir_cam_space.x);

				//if(shader_prog->is_phong)
				{
					setUniformsForPhongProg(terrain_system->mat, *meshdata, shader_prog->uniform_locations, /*prog_changed=*/true);
				}

				GLenum draw_mode = GL_TRIANGLES;

				glDrawElements(draw_mode, (GLsizei)meshdata->batches[0].num_indices, meshdata->index_type, (void*)(uint64)meshdata->batches[0].prim_start_offset);

				this->num_indices_submitted += meshdata->batches[0].num_indices;
				this->num_face_groups_submitted++;

				unbindMeshData(*meshdata);
			}
		}
	}
#endif


	batch_draw_info.reserve(current_scene->objects.size());
	batch_draw_info.resize(0);

	uint64 num_frustum_culled = 0;
	for(auto it = current_scene->objects.begin(); it != current_scene->objects.end(); ++it)
	{
		const GLObject* const ob = it->getPointer();
		if(AABBIntersectsFrustum(current_scene->frustum_clip_planes, current_scene->num_frustum_clip_planes, current_scene->frustum_aabb, ob->aabb_ws))
		{
			const OpenGLMeshRenderData& mesh_data = *ob->mesh_data;
			for(uint32 z = 0; z < mesh_data.batches.size(); ++z)
			{
				const uint32 mat_index = mesh_data.batches[z].material_index;
				if(mat_index < ob->materials.size()) // This can happen for some dodgy meshes.  TODO: Filter such batches out in mesh creation stage.
				{
					// Draw primitives for the given material
					if(!ob->materials[mat_index].transparent)
					{
						BatchDrawInfo info;
						info.ob = ob;
						info.batch = &mesh_data.batches[z];
						info.mat = &ob->materials[mat_index];
						info.prog = ob->materials[mat_index].shader_prog.ptr();
						batch_draw_info.push_back(info);
					}
				}
			}
		}
		else
			num_frustum_culled++;

		this->last_num_obs_in_frustum = current_scene->objects.size() - num_frustum_culled;
	}

	// Sort by shader program
	std::sort(batch_draw_info.begin(), batch_draw_info.end());

	// Draw sorted batches
	num_prog_changes = 0;
	num_batches_bound = 0;

	//const OpenGLMeshRenderData* last_mesh_data = NULL;
	//VAO* last_VAO = NULL;
	//VBO* last_indices = NULL;
	for(size_t i=0; i<batch_draw_info.size(); ++i)
	{
		const BatchDrawInfo& info = batch_draw_info[i];
		
		// This technique crashes for some reason:
		/*const GLObject& ob = *info.ob;
		VAO* vao = ob.vert_vao.nonNull() ? ob.vert_vao.ptr() : ob.mesh_data->vert_vao.ptr();
		VBO* indices = ob.mesh_data->vert_indices_buf.ptr();

		if(info.prog != current_bound_prog.ptr())
		{
			last_VAO = NULL;
			last_indices = NULL;
		}

		if(vao != last_VAO)
		{
			vao->bind();
			last_VAO = vao;
		}
		if(indices != last_indices)
		{
			indices->bind();
			last_indices = indices;
			num_batches_bound++;
		}*/
		

		bindMeshData(*info.ob);
		num_batches_bound++;


		//if(last_mesh_data != info.ob->mesh_data.ptr()) // NOTE: need ot handle instancing on off here. should check ob is the same as well.  or vert_vao is the same
		//{
		//	bindMeshData(*info.ob); // Bind the mesh data, which is the same for all batches.
		//	last_mesh_data = info.ob->mesh_data.ptr();
		//	num_batches_bound++;
		//}

		drawBatch(*info.ob, view_matrix, proj_matrix, *info.mat, *info.prog, *info.ob->mesh_data, *info.batch);
	}
	//if(last_mesh_data) unbindMeshData(*last_mesh_data);
	//OpenGLProgram::useNoPrograms(); // NOTE: seems slightly faster without this
	last_num_prog_changes = num_prog_changes;
	last_num_batches_bound = num_batches_bound;
#else
	uint64 num_frustum_culled = 0;
	num_prog_changes = 0;
	for(auto it = current_scene->objects.begin(); it != current_scene->objects.end(); ++it)
	{
		const GLObject* const ob = it->getPointer();
		if(AABBIntersectsFrustum(current_scene->frustum_clip_planes, current_scene->num_frustum_clip_planes, current_scene->frustum_aabb, ob->aabb_ws))
		{
			const OpenGLMeshRenderData& mesh_data = *ob->mesh_data;
			bindMeshData(*ob); // Bind the mesh data, which is the same for all batches.
			for(uint32 z = 0; z < mesh_data.batches.size(); ++z)
			{
				const uint32 mat_index = mesh_data.batches[z].material_index;
				if(mat_index < ob->materials.size()) // This can happen for some dodgy meshes.  TODO: Filter such batches out in mesh creation stage.
				{
					// Draw primitives for the given material
					if(!ob->materials[mat_index].transparent)
						drawBatch(*ob, view_matrix, proj_matrix, ob->materials[mat_index], *ob->materials[mat_index].shader_prog, mesh_data, mesh_data.batches[z]);
				}
			}
			unbindMeshData(*ob);
		}
		else
			num_frustum_culled++;

		this->last_num_obs_in_frustum = current_scene->objects.size() - num_frustum_culled;
	}
	last_num_prog_changes = num_prog_changes;
#endif

	OpenGLProgram::useNoPrograms();
	this->current_bound_prog = NULL;

	//================= Draw wireframes if required =================
	if(draw_wireframes)
	{
		// Use outline shaders for now as they just generate white fragments, which is what we want.
		OpenGLMaterial wire_mat;
		wire_mat.shader_prog = outline_prog;

		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		
		// Offset the lines so they draw in front of the filled polygons.
		glEnable(GL_POLYGON_OFFSET_LINE);
		glPolygonOffset(0.f, -1.0f);

		for(auto it = current_scene->objects.begin(); it != current_scene->objects.end(); ++it)
		{
			const GLObject* const ob = it->getPointer();
			if(AABBIntersectsFrustum(current_scene->frustum_clip_planes, current_scene->num_frustum_clip_planes, current_scene->frustum_aabb, ob->aabb_ws))
			{
				const OpenGLMeshRenderData& mesh_data = *ob->mesh_data;
				bindMeshData(*ob); // Bind the mesh data, which is the same for all batches.
				for(uint32 z = 0; z < mesh_data.batches.size(); ++z)
					drawBatch(*ob, view_matrix, proj_matrix, wire_mat, *wire_mat.shader_prog, mesh_data, mesh_data.batches[z]);
				unbindMeshData(*ob);
			}
		}

		glDisable(GL_POLYGON_OFFSET_LINE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	//================= Draw transparent batches =================
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, current_scene->dest_blending_factor);
	glDepthMask(GL_FALSE); // Disable writing to depth buffer.

	for(auto it = current_scene->transparent_objects.begin(); it != current_scene->transparent_objects.end(); ++it)
	{
		const GLObject* const ob = it->getPointer();
		if(AABBIntersectsFrustum(current_scene->frustum_clip_planes, current_scene->num_frustum_clip_planes, current_scene->frustum_aabb, ob->aabb_ws))
		{
			const OpenGLMeshRenderData& mesh_data = *ob->mesh_data;
			bindMeshData(*ob); // Bind the mesh data, which is the same for all batches.
			for(uint32 z = 0; z < mesh_data.batches.size(); ++z)
			{
				const uint32 mat_index = mesh_data.batches[z].material_index;
				if(mat_index < ob->materials.size()) // This can happen for some dodgy meshes.  TODO: Filter such batches out in mesh creation stage.
				{
					// Draw primitives for the given material
					if(ob->materials[mat_index].transparent)
						drawBatch(*ob, view_matrix, proj_matrix, ob->materials[mat_index], *ob->materials[mat_index].shader_prog, mesh_data, mesh_data.batches[z]);
				}
			}
			unbindMeshData(*ob);
		}
	}
	glDepthMask(GL_TRUE); // Re-enable writing to depth buffer.
	glDisable(GL_BLEND);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);


	//================= Draw always-visible objects =================
	// These are objects like the move/rotate arrows, that should be visible even when behind other objects.
	// The drawing strategy for these will be:
	// Draw once without depth testing, and without depth writes, but with blending, so they are always partially visible.
	// Then draw again with depth testing, so they look proper when not occluded by another object.

	if(!current_scene->always_visible_objects.empty())
	{
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
					if(mat_index < ob->materials.size()) // This can happen for some dodgy meshes.  TODO: Filter such batches out in mesh creation stage.
						drawBatch(*ob, view_matrix, proj_matrix, ob->materials[mat_index], *ob->materials[mat_index].shader_prog, mesh_data, mesh_data.batches[z]); // Draw primitives for the given material
				}
				unbindMeshData(*ob);
			}
		}

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
					if(mat_index < ob->materials.size()) // This can happen for some dodgy meshes.  TODO: Filter such batches out in mesh creation stage.
						drawBatch(*ob, view_matrix, proj_matrix, ob->materials[mat_index], *ob->materials[mat_index].shader_prog, mesh_data, mesh_data.batches[z]); // Draw primitives for the given material
				}
				unbindMeshData(*ob);
			}
		}
	}

	//================= Draw outlines around selected objects =================
	// At this stage the outline texture has been generated in outline_edge_tex.  So we will just blend it over the current frame.
	if(!selected_objects.empty())
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE); // Don't write to z-buffer

		// Position quad to cover viewport
		const Matrix4f ob_to_world_matrix = Matrix4f::scaleMatrix(2.f, 2.f, 1.f) * Matrix4f::translationMatrix(Vec4f(-0.5, -0.5, -0.999f, 0));

		const OpenGLMeshRenderData& mesh_data = *outline_quad_meshdata;
		bindMeshData(mesh_data); // Bind the mesh data, which is the same for all batches.
		for(uint32 z = 0; z < mesh_data.batches.size(); ++z)
		{
			const OpenGLMaterial& opengl_mat = outline_edge_mat;

			assert(opengl_mat.shader_prog.getPointer() == this->overlay_prog.getPointer());

			opengl_mat.shader_prog->useProgram();

			glUniform4f(overlay_diffuse_colour_location, 1, 1, 1, 1);
			glUniformMatrix4fv(opengl_mat.shader_prog->model_matrix_loc, 1, false, /*ob->*/ob_to_world_matrix.e);
			glUniform1i(this->overlay_have_texture_location, opengl_mat.albedo_texture.nonNull() ? 1 : 0);

			glActiveTexture(GL_TEXTURE0 + 0);
			glBindTexture(GL_TEXTURE_2D, opengl_mat.albedo_texture->texture_handle);

			const Matrix3f identity = Matrix3f::identity();
			glUniformMatrix3fv(overlay_texture_matrix_location, /*count=*/1, /*transpose=*/false, identity.e);
			glUniform1i(overlay_diffuse_tex_location, 0);
				
			glDrawElements(GL_TRIANGLES, (GLsizei)mesh_data.batches[0].num_indices, mesh_data.index_type, (void*)(uint64)mesh_data.batches[0].prim_start_offset);

			opengl_mat.shader_prog->useNoPrograms();
		}
		unbindMeshData(mesh_data);

		glDepthMask(GL_TRUE); // Restore
		glDisable(GL_BLEND);
	}

	//================= Draw UI overlay objects =================
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE); // Don't write to z-buffer

		// Sort overlay objects into ascending z order, then we draw from back to front (ascending z order).
		temp_obs.resize(current_scene->overlay_objects.size());
		size_t q = 0;
		for(auto it = current_scene->overlay_objects.begin(); it != current_scene->overlay_objects.end(); ++it)
			temp_obs[q++] = it->ptr();

		std::sort(temp_obs.begin(), temp_obs.end(), OverlayObjectZComparator());

		for(size_t i=0; i<temp_obs.size(); ++i)
		{
			const OverlayObject* const ob = temp_obs[i];
			const OpenGLMeshRenderData& mesh_data = *ob->mesh_data;
			bindMeshData(mesh_data); // Bind the mesh data, which is the same for all batches.
			for(uint32 z = 0; z < mesh_data.batches.size(); ++z)
			{
				const OpenGLMaterial& opengl_mat = ob->material;

				assert(opengl_mat.shader_prog.getPointer() == this->overlay_prog.getPointer());

				opengl_mat.shader_prog->useProgram();

				glUniform4f(overlay_diffuse_colour_location, opengl_mat.albedo_rgb.r, opengl_mat.albedo_rgb.g, opengl_mat.albedo_rgb.b, opengl_mat.alpha);
				glUniformMatrix4fv(opengl_mat.shader_prog->model_matrix_loc, 1, false, ob->ob_to_world_matrix.e);
				glUniform1i(this->overlay_have_texture_location, opengl_mat.albedo_texture.nonNull() ? 1 : 0);

				if(opengl_mat.albedo_texture.nonNull())
				{
					glActiveTexture(GL_TEXTURE0 + 0);
					glBindTexture(GL_TEXTURE_2D, opengl_mat.albedo_texture->texture_handle);

					const GLfloat tex_elems[9] = {
						opengl_mat.tex_matrix.e[0], opengl_mat.tex_matrix.e[2], 0,
						opengl_mat.tex_matrix.e[1], opengl_mat.tex_matrix.e[3], 0,
						opengl_mat.tex_translation.x, opengl_mat.tex_translation.y, 1
					};
					glUniformMatrix3fv(overlay_texture_matrix_location, /*count=*/1, /*transpose=*/false, tex_elems);
					glUniform1i(overlay_diffuse_tex_location, 0);
				}
				
				glDrawElements(GL_TRIANGLES, (GLsizei)mesh_data.batches[0].num_indices, mesh_data.index_type, (void*)(uint64)mesh_data.batches[0].prim_start_offset);

				opengl_mat.shader_prog->useNoPrograms();
			}
			unbindMeshData(mesh_data);
		}

		glDepthMask(GL_TRUE); // Restore
		glDisable(GL_BLEND);
	}

	if(settings.use_final_image_buffer)
	{
		// We have rendered to main_render_framebuffer / main_render_texture / main_depth_texture.

		//================================= Do post processes: bloom effect =================================

		// Generate downsized image
		/*
		This is the computation graph for computing downsized and blurred buffers:

		---------------> full buffer -------------> downsize_framebuffers[0] ------------> blur_framebuffers_x[0] -----------> blur_framebuffers[0]
		 render scene                 downsize            |                     blur x                              blur y       
		                                                  |
		                                                  |------------------> low res buffer 1 -----------> blur_framebuffers_x[1] ------------>  blur_framebuffers[1]
		                                                       downsize               |             blur x                             blur y
		                                                                              |
		                                                                              ------------------> low res buffer 2
		                                                                                  downsize              |
		
		All blurred low res buffers are then read from and added to the resulting buffer.
		*/
		if(downsize_target_textures.empty() ||
			(downsize_framebuffers[0]->xRes() != (main_viewport_w/2) || downsize_framebuffers[0]->yRes() != (main_viewport_h/2))
			)
		{
			conPrint("(Re)Allocing downsize_framebuffers etc..");

			// Use main viewport w + h for this.  This is because we don't want to keep reallocating these textures for random setViewport calls.
			assert(main_viewport_w > 0);
			int w = main_viewport_w;
			int h = main_viewport_h;

			downsize_target_textures.resize(NUM_BLUR_DOWNSIZES);
			downsize_framebuffers   .resize(NUM_BLUR_DOWNSIZES);
			blur_target_textures_x  .resize(NUM_BLUR_DOWNSIZES);
			blur_framebuffers_x     .resize(NUM_BLUR_DOWNSIZES);
			blur_target_textures    .resize(NUM_BLUR_DOWNSIZES);
			blur_framebuffers       .resize(NUM_BLUR_DOWNSIZES);

			for(int i=0; i<NUM_BLUR_DOWNSIZES; ++i)
			{
				w /= 2;
				h /= 2;

				// Clamp texture reads otherwise edge outlines will wrap around to other side of frame.
				downsize_target_textures[i] = new OpenGLTexture(w, h, this, OpenGLTexture::Format_RGB_Linear_Float, OpenGLTexture::Filtering_Bilinear, OpenGLTexture::Wrapping_Clamp);
				downsize_framebuffers[i] = new FrameBuffer();
				downsize_framebuffers[i]->bindTextureAsTarget(*downsize_target_textures[i], GL_COLOR_ATTACHMENT0);

				blur_target_textures_x[i] = new OpenGLTexture(w, h, this, OpenGLTexture::Format_RGB_Linear_Float, OpenGLTexture::Filtering_Bilinear, OpenGLTexture::Wrapping_Clamp);
				blur_framebuffers_x[i] = new FrameBuffer();
				blur_framebuffers_x[i]->bindTextureAsTarget(*blur_target_textures_x[i], GL_COLOR_ATTACHMENT0);

				blur_target_textures[i] = new OpenGLTexture(w, h, this, OpenGLTexture::Format_RGB_Linear_Float, OpenGLTexture::Filtering_Bilinear, OpenGLTexture::Wrapping_Clamp);
				blur_framebuffers[i] = new FrameBuffer();
				blur_framebuffers[i]->bindTextureAsTarget(*blur_target_textures[i], GL_COLOR_ATTACHMENT0);
			}
		}

		//------------------- Do postprocess bloom ---------------------
	
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
			glDepthMask(GL_FALSE); // Don't write to z-buffer, depth not needed.
			glDisable(GL_DEPTH_TEST); // Don't depth test

			bindMeshData(*unit_quad_meshdata);
			assert(unit_quad_meshdata->batches.size() == 1);

			for(int i=0; i<(int)downsize_target_textures.size(); ++i)
			{
				//-------------------------------- Execute downsize shader --------------------------------
				// Reads from current_framebuf_textures or downsize_target_textures[i-1], writes to downsize_framebuffers[i]

				OpenGLTextureRef src_texture = (i == 0) ? current_framebuf_textures.colour_texture : downsize_target_textures[i - 1];

				downsize_framebuffers[i]->bind(); // Target downsize_framebuffers[i]
		
				glViewport(0, 0, (int)downsize_framebuffers[i]->xRes(), (int)downsize_framebuffers[i]->yRes()); // Set viewport to target texture size

				downsize_prog->useProgram();

				glActiveTexture(GL_TEXTURE0 + 0);
				glBindTexture(GL_TEXTURE_2D, src_texture->texture_handle);  // Set source texture
				glUniform1i(downsize_prog->albedo_texture_loc, 0);

				glDrawElements(GL_TRIANGLES, (GLsizei)unit_quad_meshdata->batches[0].num_indices, unit_quad_meshdata->index_type, (void*)(uint64)unit_quad_meshdata->batches[0].prim_start_offset); // Draw quad


				//-------------------------------- Execute blur shader in x direction --------------------------------
				// Reads from downsize_target_textures[i], writes to blur_framebuffers_x[i]/blur_target_textures_x[i].

				blur_framebuffers_x[i]->bind(); // Target blur_framebuffers_x[i]

				gaussian_blur_prog->useProgram();

				glUniform1i(gaussian_blur_prog->user_uniform_info[0].loc, /*val=*/1); // Set blur_x = 1

				glActiveTexture(GL_TEXTURE0 + 0);
				glBindTexture(GL_TEXTURE_2D, downsize_target_textures[i]->texture_handle); // Set source texture
				glUniform1i(gaussian_blur_prog->albedo_texture_loc, 0);

				glDrawElements(GL_TRIANGLES, (GLsizei)unit_quad_meshdata->batches[0].num_indices, unit_quad_meshdata->index_type, (void*)(uint64)unit_quad_meshdata->batches[0].prim_start_offset); // Draw quad


				//-------------------------------- Execute blur shader in y direction --------------------------------
				// Reads from blur_target_textures_x[i], writes to blur_framebuffers[i]/blur_target_textures[i].

				blur_framebuffers[i]->bind(); // Target blur_framebuffers[i]

				glUniform1i(gaussian_blur_prog->user_uniform_info[0].loc, /*val=*/0); // Set blur_x = 0

				glActiveTexture(GL_TEXTURE0 + 0);
				glBindTexture(GL_TEXTURE_2D, blur_target_textures_x[i]->texture_handle); // Set source texture
				glUniform1i(gaussian_blur_prog->albedo_texture_loc, 0);

				glDrawElements(GL_TRIANGLES, (GLsizei)unit_quad_meshdata->batches[0].num_indices, unit_quad_meshdata->index_type, (void*)(uint64)unit_quad_meshdata->batches[0].prim_start_offset); // Draw quad
			}

			OpenGLProgram::useNoPrograms();

			unbindMeshData(*unit_quad_meshdata);

			glDepthMask(GL_TRUE); // Restore writing to z-buffer.
			glEnable(GL_DEPTH_TEST);
	
			glViewport(0, 0, viewport_w, viewport_h); // Restore viewport
		}

		// Do imaging, which reads from main_render_framebuffer and writes to the output framebuffer

		// Bind requested target frame buffer as output buffer
		glBindFramebuffer(GL_FRAMEBUFFER, this->target_frame_buffer.nonNull() ? this->target_frame_buffer->buffer_name : 0);

		{
			glDepthMask(GL_FALSE); // Don't write to z-buffer, depth not needed.
			glDisable(GL_DEPTH_TEST); // Don't depth test

			final_imaging_prog->useProgram();

			bindMeshData(*unit_quad_meshdata);
			assert(unit_quad_meshdata->batches.size() == 1);

			glActiveTexture(GL_TEXTURE0 + 0);
			glBindTexture(GL_TEXTURE_2D, current_framebuf_textures.colour_texture->texture_handle);
			glUniform1i(final_imaging_prog->albedo_texture_loc, 0);

			glUniform1f(final_imaging_prog->user_uniform_info[FINAL_IMAGING_BLOOM_STRENGTH_UNIFORM_INDEX].loc, current_scene->bloom_strength); // Set bloom_strength uniform

			// Bind downsize_target_textures
			if(current_scene->bloom_strength > 0)
			{
				for(int i=0; i<NUM_BLUR_DOWNSIZES; ++i)
				{
					glActiveTexture(GL_TEXTURE0 + 1 + i); // Bind after current_framebuf_textures above
					glBindTexture(GL_TEXTURE_2D, blur_target_textures[i]->texture_handle);
					glUniform1i(final_imaging_prog->user_uniform_info[FINAL_IMAGING_BLUR_TEX_UNIFORM_START + i].loc, /*val=*/1 + i);
				}
			}

			glDrawElements(GL_TRIANGLES, (GLsizei)unit_quad_meshdata->batches[0].num_indices, unit_quad_meshdata->index_type, (void*)(uint64)unit_quad_meshdata->batches[0].prim_start_offset); // Draw quad
			unbindMeshData(*unit_quad_meshdata);

			OpenGLProgram::useNoPrograms();

			glEnable(GL_DEPTH_TEST);
			glDepthMask(GL_TRUE); // Restore writing to z-buffer.
		}
	} // End if(settings.use_final_image_buffer)
	

	if(profiling_enabled)
	{
		//const double cpu_time = profile_timer.elapsed();
		uint64 elapsed_ns = 0;
#if !defined(OSX)
		glEndQuery(GL_TIME_ELAPSED);
		glGetQueryObjectui64v(timer_query_id, GL_QUERY_RESULT, &elapsed_ns); // Blocks
#endif
		/*conPrint("anim_update_duration: " + doubleToStringNDecimalPlaces(anim_update_duration * 1.0e3, 4) + " ms");
		conPrint("Frame times: CPU: " + doubleToStringNDecimalPlaces(cpu_time * 1.0e3, 4) + 
			" ms, depth map gen on GPU: " + doubleToStringNDecimalPlaces(shadow_depth_drawing_elapsed_ns * 1.0e-6, 4) + 
			" ms, GPU: " + doubleToStringNDecimalPlaces(elapsed_ns * 1.0e-6, 4) + " ms.");
		conPrint("Submitted: face groups: " + toString(num_face_groups_submitted) + ", faces: " + toString(num_indices_submitted / 3) + ", aabbs: " + toString(num_aabbs_submitted) + ", " + 
			toString(current_scene->objects.size() - num_frustum_culled) + "/" + toString(current_scene->objects.size()) + " obs");*/

		last_anim_update_duration = anim_update_duration;
		last_depth_map_gen_GPU_time = shadow_depth_drawing_elapsed_ns * 1.0e-9;
		last_render_GPU_time = elapsed_ns * 1.0e-9;
	}

	frame_num++;
}


Reference<OpenGLMeshRenderData> OpenGLEngine::getCylinderMesh() // A cylinder from (0,0,0), to (0,0,1) with radius 1;
{
	if(cylinder_meshdata.isNull())
		cylinder_meshdata = MeshPrimitiveBuilding::makeCylinderMesh();
	return cylinder_meshdata;
}


Matrix4f OpenGLEngine::arrowObjectTransform(const Vec4f& startpos, const Vec4f& endpos, float radius_scale)
{
	// We want to map the vector (1,0,0) to endpos-startpos.
	// We want to map the point (0,0,0) to startpos.

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
	GLObjectRef ob = new GLObject();

	ob->ob_to_world_matrix = arrowObjectTransform(startpos, endpos, radius_scale);

	if(arrow_meshdata.isNull())
		arrow_meshdata = MeshPrimitiveBuilding::make3DArrowMesh();
	ob->mesh_data = arrow_meshdata;
	ob->materials.resize(1);
	ob->materials[0].albedo_rgb = Colour3f(col[0], col[1], col[2]);
	return ob;
}


GLObjectRef OpenGLEngine::makeAABBObject(const Vec4f& min_, const Vec4f& max_, const Colour4f& col)
{
	GLObjectRef ob = new GLObject();

	const Vec4f span = max_ - min_;

	ob->ob_to_world_matrix.setColumn(0, Vec4f(span[0], 0, 0, 0));
	ob->ob_to_world_matrix.setColumn(1, Vec4f(0, span[1], 0, 0));
	ob->ob_to_world_matrix.setColumn(2, Vec4f(0, 0, span[2], 0));
	ob->ob_to_world_matrix.setColumn(3, min_); // set origin

	ob->mesh_data = cube_meshdata;
	ob->materials.resize(1);
	ob->materials[0].albedo_rgb = Colour3f(col[0], col[1], col[2]);
	ob->materials[0].alpha = col[3];
	ob->materials[0].transparent = col[3] < 1.f;
	return ob;
}


void OpenGLEngine::buildMeshRenderData(OpenGLMeshRenderData& meshdata, const js::Vector<Vec3f, 16>& vertices, const js::Vector<Vec3f, 16>& normals, const js::Vector<Vec2f, 16>& uvs, const js::Vector<uint32, 16>& indices)
{
	GLMeshBuilding::buildMeshRenderData(meshdata, vertices, normals, uvs, indices);
}


void OpenGLEngine::loadOpenGLMeshDataIntoOpenGL(OpenGLMeshRenderData& data)
{
	// If we have a ref to a BatchedMesh, upload directly from it:
	if(data.batched_mesh.nonNull())
	{
		data.vert_vbo = new VBO(data.batched_mesh->vertex_data.data(), data.batched_mesh->vertex_data.dataSizeBytes());

		data.vert_indices_buf = new VBO(data.batched_mesh->index_data.data(), data.batched_mesh->index_data.dataSizeBytes(), GL_ELEMENT_ARRAY_BUFFER);
	}
	else
	{
		data.vert_vbo = new VBO(data.vert_data.data(), data.vert_data.dataSizeBytes());

		if(!data.vert_index_buffer_uint8.empty())
		{
			data.vert_indices_buf = new VBO(data.vert_index_buffer_uint8.data(), data.vert_index_buffer_uint8.dataSizeBytes(), GL_ELEMENT_ARRAY_BUFFER);
			assert(data.index_type == GL_UNSIGNED_BYTE);
		}
		else if(!data.vert_index_buffer_uint16.empty())
		{
			data.vert_indices_buf = new VBO(data.vert_index_buffer_uint16.data(), data.vert_index_buffer_uint16.dataSizeBytes(), GL_ELEMENT_ARRAY_BUFFER);
			assert(data.index_type == GL_UNSIGNED_SHORT);
		}
		else
		{
			data.vert_indices_buf = new VBO(data.vert_index_buffer.data(), data.vert_index_buffer.dataSizeBytes(), GL_ELEMENT_ARRAY_BUFFER);
			assert(data.index_type == GL_UNSIGNED_INT);
		}
	}

	
	data.vert_vao = new VAO(data.vert_vbo, data.vertex_spec);


	// Now that data has been uploaded, free the buffers.
	data.vert_data.clearAndFreeMem();
	data.vert_index_buffer_uint8.clearAndFreeMem();
	data.vert_index_buffer_uint16.clearAndFreeMem();
	data.vert_index_buffer.clearAndFreeMem();

	data.batched_mesh = NULL;
}


void OpenGLEngine::setUniformsForPhongProg(const OpenGLMaterial& opengl_mat, const OpenGLMeshRenderData& mesh_data, const UniformLocations& locations, bool prog_changed)
{
	const Colour4f col_nonlinear(opengl_mat.albedo_rgb.r, opengl_mat.albedo_rgb.g, opengl_mat.albedo_rgb.b, 1.f);
	const Colour4f col_linear = fastApproxSRGBToLinearSRGB(col_nonlinear);

	PhongUniforms uniforms;
	uniforms.sundir_cs = this->sun_dir_cam_space;
	uniforms.diffuse_colour = col_linear;

	/*
	Matrix2f layout:
	0 1
	2 3
	*/
	const GLfloat tex_elems[12] = {
			opengl_mat.tex_matrix.e[0], opengl_mat.tex_matrix.e[2], 0, 0,
			opengl_mat.tex_matrix.e[1], opengl_mat.tex_matrix.e[3], 0, 0,
			opengl_mat.tex_translation.x, opengl_mat.tex_translation.y, 1, 0
	};
	std::memcpy(uniforms.texture_matrix, tex_elems, sizeof(float) * 12);
	uniforms.have_shading_normals = mesh_data.has_shading_normals ? 1 : 0;
	uniforms.have_texture = opengl_mat.albedo_texture.nonNull() ? 1 : 0;
	uniforms.have_metallic_roughness_tex = opengl_mat.metallic_roughness_texture.nonNull() ? 1 : 0;
	uniforms.roughness = opengl_mat.roughness;
	uniforms.fresnel_scale = opengl_mat.fresnel_scale;
	uniforms.metallic_frac = opengl_mat.metallic_frac;
	uniforms.time = this->current_time;
	uniforms.begin_fade_out_distance = opengl_mat.begin_fade_out_distance;
	uniforms.end_fade_out_distance = opengl_mat.end_fade_out_distance;



	//if(locations.sundir_cs_location >= 0)            glUniform4fv(locations.sundir_cs_location, /*count=*/1, this->sun_dir_cam_space.x);
	//if(locations.diffuse_colour_location >= 0)       glUniform4f(locations.diffuse_colour_location, col_linear[0], col_linear[1], col_linear[2], 1.f);
	//if(locations.have_shading_normals_location >= 0) glUniform1i(locations.have_shading_normals_location, mesh_data.has_shading_normals ? 1 : 0);
	//if(locations.have_texture_location >= 0)         glUniform1i(locations.have_texture_location, opengl_mat.albedo_texture.nonNull() ? 1 : 0);
	//if(locations.roughness_location >= 0)            glUniform1f(locations.roughness_location, opengl_mat.roughness);
	//if(locations.fresnel_scale_location >= 0)        glUniform1f(locations.fresnel_scale_location, opengl_mat.fresnel_scale);
	//if(locations.metallic_frac_location >= 0)        glUniform1f(locations.metallic_frac_location, opengl_mat.metallic_frac);

	if(prog_changed)
	{
		if(this->cosine_env_tex.nonNull())
		{
			glActiveTexture(GL_TEXTURE0 + 3);
			glBindTexture(GL_TEXTURE_CUBE_MAP, this->cosine_env_tex->texture_handle);
			glUniform1i(locations.cosine_env_tex_location, 3);
		}
			
		if(this->specular_env_tex.nonNull())
		{
			glActiveTexture(GL_TEXTURE0 + 4);
			glBindTexture(GL_TEXTURE_2D, this->specular_env_tex->texture_handle);
			glUniform1i(locations.specular_env_tex_location, 4);
		}

		if(this->fbm_tex.nonNull())
		{
			glActiveTexture(GL_TEXTURE0 + 5);
			glBindTexture(GL_TEXTURE_2D, this->fbm_tex->texture_handle);
			glUniform1i(locations.fbm_tex_location, 5);
		}
	
		if(this->blue_noise_tex.nonNull())
		{
			glActiveTexture(GL_TEXTURE0 + 6);
			glBindTexture(GL_TEXTURE_2D, this->blue_noise_tex->texture_handle);
			glUniform1i(locations.blue_noise_tex_location, 6);
		}
	}

	if(opengl_mat.albedo_texture.nonNull())
	{
		glActiveTexture(GL_TEXTURE0 + 0);
		glBindTexture(GL_TEXTURE_2D, opengl_mat.albedo_texture->texture_handle);

		//const GLfloat tex_elems[9] ={
		//	opengl_mat.tex_matrix.e[0], opengl_mat.tex_matrix.e[2], 0,
		//	opengl_mat.tex_matrix.e[1], opengl_mat.tex_matrix.e[3], 0,
		//	opengl_mat.tex_translation.x, opengl_mat.tex_translation.y, 1
		//};
		//glUniformMatrix3fv(locations.texture_matrix_location, /*count=*/1, /*transpose=*/false, tex_elems);
		glUniform1i(locations.diffuse_tex_location, 0);
	}

	if(opengl_mat.lightmap_texture.nonNull())
	{
		glActiveTexture(GL_TEXTURE0 + 7);
		glBindTexture(GL_TEXTURE_2D, opengl_mat.lightmap_texture->texture_handle);
		glUniform1i(locations.lightmap_tex_location, 7);
	}

	// for double-sided mat, check required textures are set
	if(opengl_mat.double_sided)
	{
		if(opengl_mat.albedo_texture.nonNull())
		{
			assert(opengl_mat.backface_albedo_texture.nonNull());
			assert(opengl_mat.transmission_texture.nonNull());
		}

		// Set backface_albedo_texture
		if(opengl_mat.backface_albedo_texture.nonNull())
		{
			glActiveTexture(GL_TEXTURE0 + 8);
			glBindTexture(GL_TEXTURE_2D, opengl_mat.backface_albedo_texture->texture_handle);
			glUniform1i(locations.backface_diffuse_tex_location, 8);
			assert(locations.backface_diffuse_tex_location >= 0);
		}

		// Set transmission_texture
		if(opengl_mat.transmission_texture.nonNull())
		{
			glActiveTexture(GL_TEXTURE0 + 9);
			glBindTexture(GL_TEXTURE_2D, opengl_mat.transmission_texture->texture_handle);
			glUniform1i(locations.transmission_tex_location, 9);
			assert(locations.transmission_tex_location >= 0);
		}
	}

	if(opengl_mat.metallic_roughness_texture.nonNull())
	{
		glActiveTexture(GL_TEXTURE0 + 10);
		glBindTexture(GL_TEXTURE_2D, opengl_mat.metallic_roughness_texture->texture_handle);

		glUniform1i(locations.metallic_roughness_tex_location, 10);
	}

	// Set shadow mapping uniforms
	if(prog_changed && shadow_mapping.nonNull())
	{
		glActiveTexture(GL_TEXTURE0 + 1);

		glBindTexture(GL_TEXTURE_2D, this->shadow_mapping->depth_tex->texture_handle);
		glUniform1i(locations.dynamic_depth_tex_location, 1); // Texture unit 1 is for shadow maps

		glActiveTexture(GL_TEXTURE0 + 2);

		glBindTexture(GL_TEXTURE_2D, this->shadow_mapping->static_depth_tex[this->shadow_mapping->cur_static_depth_tex]->texture_handle);
		glUniform1i(locations.static_depth_tex_location, 2);
	}

	// Set blob shadows location
	if(prog_changed)
	{
		glUniform1i(locations.num_blob_positions_location, (int)current_scene->blob_shadow_locations.size());
		glUniform4fv(locations.blob_positions_location, (int)current_scene->blob_shadow_locations.size(), (const float*)current_scene->blob_shadow_locations.data());
	}

	this->phong_uniform_buf_ob->updateData(/*dest offset=*/0, &uniforms, sizeof(PhongUniforms));
}


void OpenGLEngine::drawBatch(const GLObject& ob, const Matrix4f& view_mat, const Matrix4f& proj_mat, 
	const OpenGLMaterial& opengl_mat, const OpenGLProgram& shader_prog, const OpenGLMeshRenderData& mesh_data, const OpenGLBatch& batch)
{
	drawPrimitives(ob, view_mat, proj_mat, opengl_mat, shader_prog, mesh_data, batch.prim_start_offset, batch.num_indices);
}


void OpenGLEngine::drawPrimitives(const GLObject& ob, const Matrix4f& view_mat, const Matrix4f& proj_mat, const OpenGLMaterial& opengl_mat,
		const OpenGLProgram& shader_prog_, const OpenGLMeshRenderData& mesh_data, uint32 prim_start_offset, uint32 num_indices)
{
	if(num_indices == 0)
		return;

	const OpenGLProgram* shader_prog = &shader_prog_;

	bool prog_changed = false;
	if(shader_prog != current_bound_prog.ptr())
	{
		current_bound_prog = shader_prog;
		shader_prog->useProgram();
		num_prog_changes++;
		prog_changed = true;
	}
	//shader_prog->useProgram();
	//num_prog_changes++;

	// Set uniforms.  NOTE: Setting the uniforms manually in this way (switching on shader program) is obviously quite hacky.  Improve.

	// Set per-object vert uniforms
	if(shader_prog->uses_vert_uniform_buf_obs)
	{
		PerObjectVertUniforms uniforms;
		uniforms.model_matrix = ob.ob_to_world_matrix;
		uniforms.normal_matrix = ob.ob_to_world_inv_transpose_matrix;

		this->per_object_vert_uniform_buf_ob->updateData(/*dest offset=*/0, &uniforms, sizeof(PerObjectVertUniforms));
	}
	else
	{
		glUniformMatrix4fv(shader_prog->model_matrix_loc, 1, false, ob.ob_to_world_matrix.e);
		glUniformMatrix4fv(shader_prog->normal_matrix_loc, 1, false, ob.ob_to_world_inv_transpose_matrix.e); // inverse transpose model matrix

		if(prog_changed) // THese uniforms are the same for every object, so we can just set this once for this program, for this frame.
		{
			glUniformMatrix4fv(shader_prog->view_matrix_loc, 1, false, view_mat.e);
			glUniformMatrix4fv(shader_prog->proj_matrix_loc, 1, false, proj_mat.e);
		}
	}

	if(mesh_data.usesSkinning())
	{
		temp_joint_matrices.resizeNoCopy(mesh_data.animation_data.joint_nodes.size());

		// NOTE: we should maybe just store the nodes in the joint_nodes order, to avoid this indirection.
		for(size_t i=0; i<mesh_data.animation_data.joint_nodes.size(); ++i)
		{
			const int node_i = mesh_data.animation_data.joint_nodes[i];

			temp_joint_matrices[i] = /*mesh_data.animation_data.skeleton_root_transform * */ob.anim_node_data[node_i].node_hierarchical_to_object * 
				mesh_data.animation_data.nodes[node_i].inverse_bind_matrix;

			//conPrint("matrices[" + toString(i) + "]:");
			//conPrint(matrices[i].toString());
		}

		const size_t num_joint_matrices_to_upload = myMin<size_t>(256, temp_joint_matrices.size()); // The joint_matrix uniform array has 256 elems, don't upload more than that.
		glUniformMatrix4fv(shader_prog->joint_matrix_loc, (GLsizei)num_joint_matrices_to_upload, /*transpose=*/false, temp_joint_matrices[0].e);
	}


	if(shader_prog->is_phong)
	{
		setUniformsForPhongProg(opengl_mat, mesh_data, shader_prog->uniform_locations, prog_changed);
	}
	else if(shader_prog->is_transparent)
	{
		const Colour4f col_nonlinear(opengl_mat.albedo_rgb.r, opengl_mat.albedo_rgb.g, opengl_mat.albedo_rgb.b, 1.f);
		const Colour4f col_linear = fastApproxSRGBToLinearSRGB(col_nonlinear);

		glUniform4f(shader_prog->uniform_locations.diffuse_colour_location, col_linear[0], col_linear[1], col_linear[2], opengl_mat.alpha);
		glUniform1i(shader_prog->uniform_locations.have_shading_normals_location, mesh_data.has_shading_normals ? 1 : 0);
		
		if(prog_changed)
		{
			glUniform4fv(shader_prog->uniform_locations.sundir_cs_location, /*count=*/1, this->sun_dir_cam_space.x);

			if(this->specular_env_tex.nonNull())
			{
				glActiveTexture(GL_TEXTURE0 + 4);
				glBindTexture(GL_TEXTURE_2D, this->specular_env_tex->texture_handle);
				glUniform1i(shader_prog->uniform_locations.specular_env_tex_location, 4);
			}

			const Vec4f campos_ws = current_scene->cam_to_world.getColumn(3);
			glUniform3fv(shader_prog->uniform_locations.campos_ws_location, 1, campos_ws.x);
		}
	}
	else if(shader_prog == this->env_prog.getPointer())
	{
		glUniform4fv(this->env_sundir_cs_location, /*count=*/1, this->sun_dir_cam_space.x);
		glUniform4f(this->env_diffuse_colour_location, opengl_mat.albedo_rgb.r, opengl_mat.albedo_rgb.g, opengl_mat.albedo_rgb.b, 1.f);
		glUniform1i(this->env_have_texture_location, opengl_mat.albedo_texture.nonNull() ? 1 : 0);
		const Vec4f campos_ws = current_scene->cam_to_world.getColumn(3);
		glUniform3fv(shader_prog->campos_ws_loc, 1, campos_ws.x);
		if(shader_prog->time_loc >= 0)
			glUniform1f(shader_prog->time_loc, this->current_time);

		if(opengl_mat.albedo_texture.nonNull())
		{
			glActiveTexture(GL_TEXTURE0 + 0);
			glBindTexture(GL_TEXTURE_2D, opengl_mat.albedo_texture->texture_handle);

			const GLfloat tex_elems[9] = {
				opengl_mat.tex_matrix.e[0], opengl_mat.tex_matrix.e[2], 0,
				opengl_mat.tex_matrix.e[1], opengl_mat.tex_matrix.e[3], 0,
				opengl_mat.tex_translation.x, opengl_mat.tex_translation.y, 1
			};
			glUniformMatrix3fv(this->env_texture_matrix_location, /*count=*/1, /*transpose=*/false, tex_elems);
			glUniform1i(this->env_diffuse_tex_location, 0);
		}

		/*if(this->noise_tex.nonNull())
		{
			glActiveTexture(GL_TEXTURE0 + 1);
			glBindTexture(GL_TEXTURE_2D, this->noise_tex->texture_handle);
			glUniform1i(this->env_noise_tex_location, 1);
		}*/
		if(this->fbm_tex.nonNull())
		{
			glActiveTexture(GL_TEXTURE0 + 2);
			glBindTexture(GL_TEXTURE_2D, this->fbm_tex->texture_handle);
			glUniform1i(this->env_fbm_tex_location, 2);
		}
		if(this->cirrus_tex.nonNull())
		{
			glActiveTexture(GL_TEXTURE0 + 3);
			glBindTexture(GL_TEXTURE_2D, this->cirrus_tex->texture_handle);
			glUniform1i(this->env_cirrus_tex_location, 3);
		}
	}
	else if(shader_prog->is_depth_draw)
	{
		const Matrix4f proj_view_model_matrix = proj_mat * view_mat * ob.ob_to_world_matrix;
		glUniformMatrix4fv(shader_prog->uniform_locations.proj_view_model_matrix_location, 1, false, proj_view_model_matrix.e);

		if(shader_prog->key.alpha_test)
		{
			assert(opengl_mat.albedo_texture.nonNull()); // We should only be using the depth shader with alpha test if there is a texture with alpha.
			if(opengl_mat.albedo_texture.nonNull())
			{
				glActiveTexture(GL_TEXTURE0 + 0);
				glBindTexture(GL_TEXTURE_2D, opengl_mat.albedo_texture->texture_handle);

				const GLfloat tex_elems[9] = {
					opengl_mat.tex_matrix.e[0], opengl_mat.tex_matrix.e[2], 0,
					opengl_mat.tex_matrix.e[1], opengl_mat.tex_matrix.e[3], 0,
					opengl_mat.tex_translation.x, opengl_mat.tex_translation.y, 1
				};
				glUniformMatrix3fv(shader_prog->uniform_locations.texture_matrix_location, /*count=*/1, /*transpose=*/false, tex_elems);
				glUniform1i(shader_prog->uniform_locations.diffuse_tex_location, 0);
			}
		}
	}
	else if(shader_prog == this->outline_prog.getPointer())
	{

	}
	else // Else shader created by user code:
	{
		if(shader_prog->campos_ws_loc >= 0)
		{
			const Vec4f campos_ws = current_scene->cam_to_world.getColumn(3);
			glUniform3fv(shader_prog->campos_ws_loc, 1, campos_ws.x);
		}
		if(shader_prog->time_loc >= 0)
			glUniform1f(shader_prog->time_loc, this->current_time);
		if(shader_prog->colour_loc >= 0)
			glUniform3fv(shader_prog->colour_loc, 1, &opengl_mat.albedo_rgb.r);

		if(shader_prog->albedo_texture_loc >= 0 && opengl_mat.albedo_texture.nonNull())
		{
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, opengl_mat.albedo_texture->texture_handle);
			glUniform1i(shader_prog->albedo_texture_loc, 0);
		}

		if(shader_prog->texture_2_loc >= 0 && opengl_mat.texture_2.nonNull())
		{
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, opengl_mat.texture_2->texture_handle);
			glUniform1i(shader_prog->texture_2_loc, 1);
		}

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

	if(ob.instance_matrix_vbo.nonNull())
	{
		const size_t num_instances = ob.num_instances_to_draw;
		glDrawElementsInstanced(draw_mode, (GLsizei)num_indices, mesh_data.index_type, (void*)(uint64)prim_start_offset, (uint32)num_instances);
	}
	else
		glDrawElements(draw_mode, (GLsizei)num_indices, mesh_data.index_type, (void*)(uint64)prim_start_offset);

	this->num_indices_submitted += num_indices;
	this->num_face_groups_submitted++;
}


Reference<OpenGLTexture> OpenGLEngine::loadCubeMap(const std::vector<Reference<Map2D> >& face_maps,
	OpenGLTexture::Filtering /*filtering*/, OpenGLTexture::Wrapping /*wrapping*/)
{
	Reference<OpenGLTexture> opengl_tex = new OpenGLTexture();

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
		opengl_tex->loadCubeMap(tex_xres, tex_yres, tex_data, OpenGLTexture::Format_RGB_Linear_Float);

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



Reference<OpenGLTexture> OpenGLEngine::loadOpenGLTextureFromTexData(const OpenGLTextureKey& key, Reference<TextureData> texture_data,
	OpenGLTexture::Filtering filtering, OpenGLTexture::Wrapping wrapping, bool use_sRGB)
{
	auto res = this->opengl_textures.find(key);
	if(res == this->opengl_textures.end())
	{
		// Load into OpenGL
		Reference<OpenGLTexture> opengl_tex = TextureLoading::loadTextureIntoOpenGL(*texture_data, this, filtering, wrapping, use_sRGB);

		this->opengl_textures.insert(std::make_pair(key, opengl_tex)); // Store

		// Now that the data has been loaded into OpenGL, we can erase the texture data.  Don't remove data with more than 1 frame, as we will need it for animated textures.
		if(texture_data->frames.size() == 1)
			this->texture_data_manager->removeTextureData(key.path);

		opengl_tex->key = key;
		opengl_tex->m_opengl_engine = this;
		this->textureBecameUsed(opengl_tex.ptr());
		this->tex_mem_usage += opengl_tex->getByteSize();

		return opengl_tex;
	}
	else // Else if this map has already been loaded into an OpenGL Texture:
	{
		if(res->second.value->getRefCount() == 1)
			this->textureBecameUsed(res->second.value.ptr());

		// conPrint("\tTexture already loaded.");
		return res->second.value;
	}
}


static Reference<ImageMapUInt8> convertToUInt8ImageMap(const ImageMap<uint16, UInt16ComponentValueTraits>& map)
{
	Reference<ImageMapUInt8> new_map = new ImageMapUInt8(map.getWidth(), map.getHeight(), map.getN());
	for(size_t i=0; i<map.getDataSize(); ++i)
		new_map->getData()[i] = (uint8)(map.getData()[i] / 256);
	return new_map;
}


Reference<OpenGLTexture> OpenGLEngine::getOrLoadOpenGLTexture(const OpenGLTextureKey& key, const Map2D& map2d, /*BuildUInt8MapTextureDataScratchState& state,*/
	OpenGLTexture::Filtering filtering, OpenGLTexture::Wrapping wrapping, bool allow_compression, bool use_sRGB)
{
	if(dynamic_cast<const ImageMap<uint16, UInt16ComponentValueTraits>*>(&map2d))
	{
		// Convert to 8-bit, then recurse.
		Reference<ImageMapUInt8> im_map_uint8 = convertToUInt8ImageMap(static_cast<const ImageMap<uint16, UInt16ComponentValueTraits>&>(map2d));

		return getOrLoadOpenGLTexture(key, *im_map_uint8, filtering, wrapping, allow_compression, use_sRGB);
	}
	else if(dynamic_cast<const ImageMapUInt8*>(&map2d))
	{
		const ImageMapUInt8* imagemap = static_cast<const ImageMapUInt8*>(&map2d);

		//conPrint("key: " + key.path);
		//conPrint("&map2d: " + toString((uint64)&map2d));
		auto res = this->opengl_textures.find(key);
		if(res == this->opengl_textures.end())
		{
			// Get processed texture data
			Reference<TextureData> texture_data = this->texture_data_manager->getOrBuildTextureData(key.path, imagemap, this/*, state*/, allow_compression);

			// Load into OpenGL
			Reference<OpenGLTexture> opengl_tex = TextureLoading::loadTextureIntoOpenGL(*texture_data, this, filtering, wrapping, use_sRGB);

			// Now that the data has been loaded into OpenGL, we can erase the texture data.
			this->texture_data_manager->removeTextureData(key.path);

			this->opengl_textures.insert(std::make_pair(key, opengl_tex)); // Store

			opengl_tex->key = key;
			opengl_tex->m_opengl_engine = this;
			this->textureBecameUsed(opengl_tex.ptr());
			this->tex_mem_usage += opengl_tex->getByteSize();

			return opengl_tex;
		}
		else // Else if this map has already been loaded into an OpenGL Texture:
		{
			if(res->second.value->getRefCount() == 1)
				this->textureBecameUsed(res->second.value.ptr());

			// conPrint("\tTexture already loaded.");
			return res->second.value;
		}
	}
	else if(dynamic_cast<const ImageMapFloat*>(&map2d))
	{
		const ImageMapFloat* imagemap = static_cast<const ImageMapFloat*>(&map2d);

		auto res = this->opengl_textures.find(key);
		if(res == this->opengl_textures.end())
		{
			// Load texture
			if(imagemap->getN() == 1)
			{
				Reference<OpenGLTexture> opengl_tex = new OpenGLTexture();
				opengl_tex->load(map2d.getMapWidth(), map2d.getMapHeight(), ArrayRef<uint8>((uint8*)imagemap->getData(), imagemap->getDataSize()), this, OpenGLTexture::Format_Greyscale_Float,
					filtering, wrapping
				);

				this->opengl_textures.insert(std::make_pair(key, opengl_tex)); // Store

				opengl_tex->key = key;
				opengl_tex->m_opengl_engine = this;
				this->textureBecameUsed(opengl_tex.ptr());
				this->tex_mem_usage += opengl_tex->getByteSize();

				return opengl_tex;
			}
			else if(imagemap->getN() == 3)
			{
				Reference<OpenGLTexture> opengl_tex = new OpenGLTexture();
				opengl_tex->load(map2d.getMapWidth(), map2d.getMapHeight(), ArrayRef<uint8>((uint8*)imagemap->getData(), imagemap->getDataSize()), this, OpenGLTexture::Format_RGB_Linear_Float,
					filtering, wrapping
				);

				this->opengl_textures.insert(std::make_pair(key, opengl_tex)); // Store

				opengl_tex->key = key;
				opengl_tex->m_opengl_engine = this;
				this->textureBecameUsed(opengl_tex.ptr());
				this->tex_mem_usage += opengl_tex->getByteSize();

				return opengl_tex;
			}
			else
				throw glare::Exception("Texture has unhandled number of components: " + toString(imagemap->getN()));

		}
		else // Else if this map has already been loaded into an OpenGL Texture:
		{
			if(res->second.value->getRefCount() == 1)
				this->textureBecameUsed(res->second.value.ptr());

			return res->second.value;
		}
	}
	else if(dynamic_cast<const ImageMap<half, HalfComponentValueTraits>*>(&map2d))
	{
		const ImageMap<half, HalfComponentValueTraits>* imagemap = static_cast<const ImageMap<half, HalfComponentValueTraits>*>(&map2d);

		auto res = this->opengl_textures.find(key);
		if(res == this->opengl_textures.end())
		{
			// Load texture
			if(imagemap->getN() == 1)
			{
				Reference<OpenGLTexture> opengl_tex = new OpenGLTexture();
				opengl_tex->load(map2d.getMapWidth(), map2d.getMapHeight(), ArrayRef<uint8>((uint8*)imagemap->getData(), imagemap->getDataSize()), this, OpenGLTexture::Format_Greyscale_Half,
					OpenGLTexture::Filtering_Fancy
				);

				this->opengl_textures.insert(std::make_pair(key, opengl_tex)); // Store

				opengl_tex->key = key;
				opengl_tex->m_opengl_engine = this;
				this->textureBecameUsed(opengl_tex.ptr());
				this->tex_mem_usage += opengl_tex->getByteSize();

				return opengl_tex;
			}
			else if(imagemap->getN() == 3)
			{
				Reference<OpenGLTexture> opengl_tex = new OpenGLTexture();
				opengl_tex->load(map2d.getMapWidth(), map2d.getMapHeight(), ArrayRef<uint8>((uint8*)imagemap->getData(), imagemap->getDataSize()), this, OpenGLTexture::Format_RGB_Linear_Half,
					OpenGLTexture::Filtering_Fancy
				);

				this->opengl_textures.insert(std::make_pair(key, opengl_tex)); // Store

				opengl_tex->key = key;
				opengl_tex->m_opengl_engine = this;
				this->textureBecameUsed(opengl_tex.ptr());
				this->tex_mem_usage += opengl_tex->getByteSize();

				return opengl_tex;
			}
			else
				throw glare::Exception("Texture has unhandled number of components: " + toString(imagemap->getN()));

		}
		else // Else if this map has already been loaded into an OpenGL Texture:
		{
			if(res->second.value->getRefCount() == 1)
				this->textureBecameUsed(res->second.value.ptr());

			return res->second.value;
		}
	}
	else if(dynamic_cast<const CompressedImage*>(&map2d))
	{
		const CompressedImage* compressed_image = static_cast<const CompressedImage*>(&map2d);

		auto res = this->opengl_textures.find(key);
		if(res == this->opengl_textures.end())
		{
			// Load texture
			//printVar(compressed_image->gl_internal_format);
			//printVar(GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT);

			Reference<OpenGLTexture> opengl_tex = new OpenGLTexture();

			size_t bytes_per_block;
			if(compressed_image->gl_base_internal_format == GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT)
			{
				opengl_tex->makeGLTexture(OpenGLTexture::Format_Compressed_BC6); // Binds texture
				bytes_per_block = 16; // "Both formats use 4x4 pixel blocks, and each block in both compression format is 128-bits in size" - See https://www.khronos.org/opengl/wiki/BPTC_Texture_Compression
			}
			else if(compressed_image->gl_base_internal_format == GL_EXT_COMPRESSED_RGB_S3TC_DXT1_EXT)
			{
				opengl_tex->makeGLTexture(GL_EXT_texture_sRGB_support ? OpenGLTexture::Format_Compressed_SRGB_Uint8 : OpenGLTexture::Format_Compressed_RGB_Uint8); // Binds texture
				bytes_per_block = 8;
			}
			else if(compressed_image->gl_base_internal_format == GL_EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT)
			{
				opengl_tex->makeGLTexture(GL_EXT_texture_sRGB_support ? OpenGLTexture::Format_Compressed_SRGBA_Uint8 : OpenGLTexture::Format_Compressed_RGBA_Uint8); // Binds texture
				bytes_per_block = 16;
			}
			else
				throw glare::Exception("Unhandled internal format for compressed image.");

			opengl_tex->setTexParams(this, filtering);

			for(size_t i=0; i<compressed_image->mipmap_level_data.size(); ++i)
			{
				const size_t level_w = myMax((size_t)1, compressed_image->getMapWidth()  >> i);
				const size_t level_h = myMax((size_t)1, compressed_image->getMapHeight() >> i);

				// Check mipmap_level_data is the correct size for this mipmap level.
				const size_t num_xblocks = Maths::roundedUpDivide(level_w, (size_t)4);
				const size_t num_yblocks = Maths::roundedUpDivide(level_h, (size_t)4);
				const size_t expected_size_B = num_xblocks * num_yblocks * bytes_per_block;
				if(expected_size_B != compressed_image->mipmap_level_data[i].size())
					throw glare::Exception("Compressed image data was wrong size.");

				opengl_tex->setMipMapLevelData((int)i, level_w, level_h, ArrayRef<uint8>(compressed_image->mipmap_level_data[i].data(), compressed_image->mipmap_level_data[i].size()));
			}

			this->opengl_textures.insert(std::make_pair(key, opengl_tex)); // Store

			opengl_tex->key = key;
			opengl_tex->m_opengl_engine = this;
			this->textureBecameUsed(opengl_tex.ptr());
			this->tex_mem_usage += opengl_tex->getByteSize();

			return opengl_tex;
		}
		else // Else if this map has already been loaded into an OpenGL Texture:
		{
			if(res->second.value->getRefCount() == 1)
				this->textureBecameUsed(res->second.value.ptr());

			return res->second.value;
		}
	}
	else
	{
		throw glare::Exception("Unhandled texture type for texture '" + key.path + "': (B/pixel: " + toString(map2d.getBytesPerPixel()) + ", numChannels: " + toString(map2d.numChannels()) + ")");
	}
}


void OpenGLEngine::addOpenGLTexture(const OpenGLTextureKey& key, const Reference<OpenGLTexture>& tex)
{
	tex->key = key;
	tex->m_opengl_engine = this;
	this->opengl_textures.set(key, tex);

	this->tex_mem_usage += tex->getByteSize();

	trimTextureUsage();
}


void OpenGLEngine::removeOpenGLTexture(const OpenGLTextureKey& key)
{
	auto it = this->opengl_textures.find(key);

	if(it != this->opengl_textures.end())
	{
		OpenGLTexture* tex = it->second.value.ptr();

		assert(this->tex_mem_usage >= tex->getByteSize());
		this->tex_mem_usage -= tex->getByteSize();

		this->opengl_textures.erase(it);
	}
}


bool OpenGLEngine::isOpenGLTextureInsertedForKey(const OpenGLTextureKey& key) const
{
	return this->opengl_textures.isInserted(key);
}


float OpenGLEngine::getPixelDepth(int pixel_x, int pixel_y)
{
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


Reference<ImageMap<uint8, UInt8ComponentValueTraits> > OpenGLEngine::getRenderedColourBuffer()
{
	const bool capture_alpha = false;

	const int N = capture_alpha ? 4 : 3;
	Reference<ImageMap<uint8, UInt8ComponentValueTraits> > map = new ImageMap<uint8, UInt8ComponentValueTraits>(viewport_w, viewport_h, N);
	js::Vector<uint8, 16> data(viewport_w * viewport_h * N);
	glReadPixels(0, 0, // x, y
		viewport_w, viewport_h,  // width, height
		capture_alpha ? GL_RGBA : GL_RGB,
		GL_UNSIGNED_BYTE,
		data.data()
	);
	
	// Flip image upside down.
	for(int dy=0; dy<viewport_h; ++dy)
	{
		const int sy = viewport_h - dy - 1;
		std::memcpy(/*dest=*/map->getPixel(0, dy), /*src=*/&data[sy*viewport_w*N], /*size=*/viewport_w*N);
	}

	return map;
}


void OpenGLEngine::setTargetFrameBufferAndViewport(const Reference<FrameBuffer> frame_buffer)
{ 
	target_frame_buffer = frame_buffer;
	setViewport((int)frame_buffer->xRes(), (int)frame_buffer->yRes());
}


glare::TaskManager& OpenGLEngine::getTaskManager()
{
	Lock lock(task_manager_mutex);
	if(!task_manager)
	{
		task_manager = new glare::TaskManager();
		task_manager->setThreadPriorities(MyThread::Priority_Lowest);
		// conPrint("OpenGLEngine::getTaskManager(): created task manager.");
	}
	return *task_manager;
}


void OpenGLEngine::textureBecameUnused(const OpenGLTexture* tex)
{
	opengl_textures.itemBecameUnused(tex->key);

	// conPrint("Texture " + tex->key.path + " became unused.");
	// conPrint("textures: " + toString(opengl_textures.numUsedItems()) + " used, " + toString(opengl_textures.numUnusedItems()) + " unused");
}


void OpenGLEngine::textureBecameUsed(const OpenGLTexture* tex)
{
	assert(tex->key.path != "");

	opengl_textures.itemBecameUsed(tex->key);

	// conPrint("Texture " + tex->key.path + " became used.");
	// conPrint("textures: " + toString(opengl_textures.numUsedItems()) + " used, " + toString(opengl_textures.numUnusedItems()) + " unused");
}


void OpenGLEngine::trimTextureUsage()
{
	if(tex_mem_usage > max_tex_mem_usage)
	{
		// Remove textures from unused texture list until we are using <= max_tex_mem_usage
		while((tex_mem_usage > max_tex_mem_usage) && (opengl_textures.numUnusedItems() > 0))
		{
			OpenGLTextureKey removed_key;
			OpenGLTextureRef removed_tex;
			const bool removed = opengl_textures.removeLRUUnusedItem(removed_key, removed_tex);
			assert(removed);
			if(removed)
			{
				assert(this->tex_mem_usage >= removed_tex->getByteSize());
				this->tex_mem_usage -= removed_tex->getByteSize();
			}
		}
	}
}


void OpenGLEngine::addScene(const Reference<OpenGLScene>& scene)
{
	scenes.insert(scene);

	scene->env_ob->mesh_data = sphere_meshdata;
}


void OpenGLEngine::removeScene(const Reference<OpenGLScene>& scene)
{
	scenes.erase(scene);
}


void OpenGLEngine::setCurrentScene(const Reference<OpenGLScene>& scene)
{
	current_scene = scene;
}


void OpenGLEngine::setMSAAEnabled(bool enabled)
{
	if(enabled)
		glEnable(GL_MULTISAMPLE);
	else
		glDisable(GL_MULTISAMPLE);
}


bool OpenGLEngine::openglDriverVendorIsIntel() const
{
	return StringUtils::containsString(::toLowerCase(opengl_vendor), "intel");
}


GLMemUsage OpenGLEngine::getTotalMemUsage() const
{
	GLMemUsage sum;

	for(auto it=scenes.begin(); it != scenes.end(); ++it)
	{
		Reference<OpenGLScene> scene = *it;
		sum += scene->getTotalMemUsage();
	}

	//----------- texture_data_manager  ----------
	const size_t tex_cpu_usage = texture_data_manager->getTotalMemUsage();
	sum.texture_cpu_usage += tex_cpu_usage;

	//----------- opengl_textures  ----------
	for(auto i = opengl_textures.begin(); i != opengl_textures.end(); ++i)
	{
		const size_t tex_gpu_usage = i->second.value->getByteSize();
		sum.texture_gpu_usage += tex_gpu_usage;
	}

	// Get sum memory usage of unused (cached) textures.
	// NOTE: this could be a bit slow with the map lookups.
	sum.sum_unused_tex_gpu_usage = 0;
	for(auto i = opengl_textures.unused_items.begin(); i != opengl_textures.unused_items.end(); ++i)
	{
		const auto res = opengl_textures.find(*i);
		if(res != opengl_textures.end())
		{
			const size_t tex_gpu_usage = res->second.value->getByteSize();
			sum.sum_unused_tex_gpu_usage += tex_gpu_usage;
		}
	}

	return sum;
}


std::string OpenGLEngine::getDiagnostics() const
{
	std::string s;
	s += "Objects: " + toString(current_scene->objects.size()) + "\n";
	s += "Transparent objects: " + toString(current_scene->transparent_objects.size()) + "\n";

	s += "Num obs in view frustum: " + toString(last_num_obs_in_frustum) + "\n";
	s += "Num prog changes: " + toString(last_num_prog_changes) + "\n";
	s += "Num batches bound: " + toString(last_num_batches_bound) + "\n";

	s += "last_anim_update_duration: " + doubleToStringNSigFigs(last_anim_update_duration * 1.0e3, 4) + " ms\n";
	s += "last_depth_map_gen_GPU_time: " + doubleToStringNSigFigs(last_depth_map_gen_GPU_time * 1.0e3, 4) + " ms\n";
	s += "last_render_GPU_time: " + doubleToStringNSigFigs(last_render_GPU_time * 1.0e3, 4) + " ms\n";

	const GLMemUsage mem_usage = this->getTotalMemUsage();
	s += "geometry CPU mem usage: " + getNiceByteSize(mem_usage.geom_cpu_usage) + "\n";
	s += "geometry GPU mem usage: " + getNiceByteSize(mem_usage.geom_gpu_usage) + "\n";
	s += "texture CPU mem usage: " + getNiceByteSize(mem_usage.texture_cpu_usage) + "\n";
	s += "texture GPU mem usage total: " + getNiceByteSize(tex_mem_usage) + "\n";
	//s += "texture GPU mem usage (sum)    : " + getNiceByteSize(mem_usage.texture_gpu_usage) + "\n";
	const size_t tex_usage_used = mem_usage.texture_gpu_usage - mem_usage.sum_unused_tex_gpu_usage;
	s += "textures GPU active: " + toString(opengl_textures.numUsedItems()) + " (" + getNiceByteSize(tex_usage_used) + ")\n";
	s += "textures GPU cached: " + toString(opengl_textures.numUnusedItems()) + " (" + getNiceByteSize(mem_usage.sum_unused_tex_gpu_usage) + ")\n";
	//s += "textures: " + toString(opengl_textures.numUsedItems()) + " used (getNiceByteSize" + , " + toString(opengl_textures.numUnusedItems()) + " unused\n";
	s += "num textures: " + toString(opengl_textures.size()) + "\n";
	s += "OpenGL vendor: " + opengl_vendor + "\n";
	s += "OpenGL renderer: " + opengl_renderer + "\n";
	s += "OpenGL version: " + opengl_version + "\n";
	s += "GLSL version: " + glsl_version + "\n";
	s += "texture sRGB support: " + boolToString(GL_EXT_texture_sRGB_support) + "\n";
	s += "texture s3tc support: " + boolToString(GL_EXT_texture_compression_s3tc_support);

	return s;
}
