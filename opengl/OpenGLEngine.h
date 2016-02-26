/*=====================================================================
OpenGLEngine.h
--------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "../utils/IncludeWindows.h" // This needs to go first for NOMINMAX.
#include "../graphics/colour3.h"
#include "../physics/jscol_aabbox.h"
#include "../opengl/OpenGLTexture.h"
#include "../opengl/OpenGLProgram.h"
#include "../opengl/VBO.h"
#include "../maths/vec2.h"
#include "../maths/vec3.h"
#include "../maths/Matrix2.h"
#include "../maths/matrix3.h"
#include "../maths/plane.h"
#include "../maths/Matrix4f.h"
#include "../utils/Timer.h"
#include "../utils/Reference.h"
#include "../utils/RefCounted.h"
#include "../utils/ThreadSafeRefCounted.h"
namespace Indigo { class Mesh; }


// Data for a bunch of primitives from a given mesh, that all share the same material.
class OpenGLBatch
{
public:
	uint32 material_index;
	uint32 prim_start_offset;
	uint32 num_indices;//num_prims; // Number of triangles or quads.
	uint32 num_verts_per_prim; // 3 or 4.
};


// OpenGL data for a given mesh.
class OpenGLMeshRenderData : public RefCounted
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	js::AABBox aabb_os; // Should go first as is aligned.
	VBORef vert_vbo;
	size_t normal_offset;
	size_t uv_offset;
	VBORef vert_indices_buf;
	std::vector<OpenGLBatch> batches;
	bool has_uvs;
	bool has_shading_normals;
};


class OpenGLMaterial
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	OpenGLMaterial()
	:	transparent(false),
		albedo_rgb(0.85f, 0.25f, 0.85f),
		specular_rgb(0.f),
		alpha(1.f),
		phong_exponent(100.f),
		tex_matrix(1,0,0,1),
		tex_translation(0,0),
		userdata(0),
		fresnel_scale(0.5f)
	{}

	std::string albedo_tex_path;

	Colour3f albedo_rgb; // First approximation to material colour
	Colour3f specular_rgb; // Used for OpenGL specular colour
	float alpha; // Used for transparent mats.

	bool transparent;

	Reference<OpenGLTexture> albedo_texture;

	Reference<OpenGLProgram> shader_prog;

	Matrix2f tex_matrix;
	Vec2f tex_translation;

	float phong_exponent;
	float fresnel_scale;

	uint64 userdata;
};


struct GLObject : public RefCounted
{
	INDIGO_ALIGNED_NEW_DELETE

	Matrix4f ob_to_world_matrix;

	js::AABBox aabb_ws;

	Reference<OpenGLMeshRenderData> mesh_data;
	
	std::vector<OpenGLMaterial> materials;
};
typedef Reference<GLObject> GLObjectRef;


#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable:4324) // Disable 'structure was padded due to __declspec(align())' warning.
#endif
class OpenGLEngine : public ThreadSafeRefCounted
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	OpenGLEngine();
	~OpenGLEngine();

	void initialise(const std::string& shader_dir);

	void unloadAllData();

	void addObject(const Reference<GLObject>& object);

	void newMaterialUsed(OpenGLMaterial& mat);

	void objectMaterialsUpdated(const Reference<GLObject>& object);

	void draw();

	void setCameraTransform(const Matrix4f& world_to_camera_space_matrix_, float sensor_width, float lens_sensor_dist, float render_aspect_ratio);

	void setSunDir(const Vec4f& d);

	void setEnvMapTransform(const Matrix3f& transform);

	void setEnvMat(const OpenGLMaterial& env_mat);
	const OpenGLMaterial& getEnvMat() const { return env_mat; }

	void setViewportAspectRatio(float r) { viewport_aspect_ratio = r; }

	void setMaxDrawDistance(float d) { max_draw_dist = d; }

	bool initSucceeded() const { return init_succeeded; }
	std::string getInitialisationErrorMsg() const { return initialisation_error_msg; }


	// Built OpenGLMeshRenderData from an Indigo::Mesh.
	// Throws Indigo::Exception on failure
	static Reference<OpenGLMeshRenderData> buildIndigoMesh(const Reference<Indigo::Mesh>& mesh_);

private:
	void buildMaterial(OpenGLMaterial& mat);
	void drawPrimitives(const OpenGLMaterial& opengl_mat, const OpenGLMeshRenderData& mesh_data, const OpenGLBatch& batch/*, int num_verts_per_primitive*/);
	void drawPrimitivesWireframe(const OpenGLBatch& pass_data, int num_verts_per_primitive);


	bool init_succeeded;
	std::string initialisation_error_msg;
	
	OpenGLMaterial env_mat;
	Matrix3f env_mat_transform;

	Vec4f sun_dir;
	Vec4f sun_dir_cam_space;

	//OpenGLMeshRenderData aabb_meshdata;
	OpenGLMeshRenderData sphere_meshdata;

	float sensor_width;
	float viewport_aspect_ratio;
	float lens_sensor_dist;
	float render_aspect_ratio;

	Matrix4f world_to_camera_space_matrix;
public:
	std::vector<Reference<GLObject> > objects;
	std::vector<Reference<GLObject> > transparent_objects;
private:
	float max_draw_dist;

	GLuint timer_query_id;


	uint64 num_face_groups_submitted;
	uint64 num_faces_submitted;
	uint64 num_aabbs_submitted;

	Plane<float> frustum_clip_planes[5];
	Vec4f frustum_verts[5];
	js::AABBox frustum_aabb;

	Reference<OpenGLProgram> phong_untextured_prog;
	int diffuse_colour_location;
	int have_shading_normals_location;
	int have_texture_location;
	int diffuse_tex_location;
	int texture_matrix_location;
	int sundir_location;
	int exponent_location;
	int fresnel_scale_location;

	Reference<OpenGLProgram> transparent_prog;
	int transparent_colour_location;
	int transparent_have_shading_normals_location;

	Reference<OpenGLProgram> env_prog;
	int env_diffuse_colour_location;
	int env_have_texture_location;
	int env_diffuse_tex_location;
	int env_texture_matrix_location;

	bool support_geom_normal_shader;

	//size_t vert_mem_used; // B
	//size_t index_mem_used; // B

	std::string shader_dir;
public:
	bool anisotropic_filtering_supported;
	float max_anisotropy;
};
