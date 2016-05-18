/*=====================================================================
OpenGLEngine.h
--------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "../utils/IncludeWindows.h" // This needs to go first for NOMINMAX.
#include "../graphics/colour3.h"
#include "../graphics/Colour4f.h"
#include "../physics/jscol_aabbox.h"
#include "../opengl/OpenGLTexture.h"
#include "../opengl/OpenGLProgram.h"
#include "../opengl/ShadowMapping.h"
#include "../opengl/VBO.h"
#include "../opengl/VAO.h"
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


// Data for a bunch of triangles from a given mesh, that all share the same material.
class OpenGLBatch
{
public:
	uint32 material_index;
	uint32 prim_start_offset; // Offset in bytes from the start of the index buffer.
	uint32 num_indices; // Number of vertex indices (= num triangles/3).
};


// OpenGL data for a given mesh.
class OpenGLMeshRenderData : public RefCounted
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	js::AABBox aabb_os; // Should go first as is aligned.
	VBORef vert_vbo;
	VBORef vert_indices_buf;
	std::vector<OpenGLBatch> batches;
	bool has_uvs;
	bool has_shading_normals;
	GLenum index_type; // One of GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, or GL_UNSIGNED_INT.
	VAORef vert_vao;
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
	Matrix4f ob_to_world_inv_tranpose_matrix;

	js::AABBox aabb_ws;

	Reference<OpenGLMeshRenderData> mesh_data;
	
	std::vector<OpenGLMaterial> materials;
};
typedef Reference<GLObject> GLObjectRef;


struct OverlayObject : public RefCounted
{
	INDIGO_ALIGNED_NEW_DELETE

	Matrix4f ob_to_world_matrix;

	Reference<OpenGLMeshRenderData> mesh_data;
	
	OpenGLMaterial material;
};
typedef Reference<OverlayObject> OverlayObjectRef;



class OpenGLEngineSettings
{
public:
	OpenGLEngineSettings() : shadow_mapping(false) {}

	bool shadow_mapping;
};



#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable:4324) // Disable 'structure was padded due to __declspec(align())' warning.
#endif
class OpenGLEngine : public ThreadSafeRefCounted
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	OpenGLEngine(const OpenGLEngineSettings& settings);
	~OpenGLEngine();

	void initialise(const std::string& shader_dir);

	void unloadAllData();

	void addObject(const Reference<GLObject>& object);
	void removeObject(const Reference<GLObject>& object);

	void addOverlayObject(const Reference<OverlayObject>& object);


	void newMaterialUsed(OpenGLMaterial& mat);

	void objectMaterialsUpdated(const Reference<GLObject>& object);

	void draw();

	void setCameraTransform(const Matrix4f& world_to_camera_space_matrix_, float sensor_width, float lens_sensor_dist, float render_aspect_ratio);

	void setSunDir(const Vec4f& d);

	void setEnvMapTransform(const Matrix3f& transform);

	void setEnvMat(const OpenGLMaterial& env_mat);
	const OpenGLMaterial& getEnvMat() const { return env_ob->materials[0]; }


	Reference<OpenGLMeshRenderData> getSphereMeshData() { return sphere_meshdata; }

	void updateObjectTransformData(GLObject& object);


	void setViewportAspectRatio(float r, int viewport_w_, int viewport_h_) { viewport_aspect_ratio = r; viewport_w = viewport_w_; viewport_h = viewport_h_; }

	void setMaxDrawDistance(float d) { max_draw_dist = d; }

	bool initSucceeded() const { return init_succeeded; }
	std::string getInitialisationErrorMsg() const { return initialisation_error_msg; }


	GLObjectRef makeArrowObject(const Vec4f& startpos, const Vec4f& endpos, const Colour4f& col, float radius_scale);
	GLObjectRef makeAABBObject(const Vec4f& min_, const Vec4f& max_, const Colour4f& col);

	static Reference<OpenGLMeshRenderData> makeCapsuleMesh(const Vec3f& bottom_spans, const Vec3f& top_spans);

	// Built OpenGLMeshRenderData from an Indigo::Mesh.
	// Throws Indigo::Exception on failure
	static Reference<OpenGLMeshRenderData> buildIndigoMesh(const Reference<Indigo::Mesh>& mesh_);


	static Reference<OpenGLMeshRenderData> makeOverlayQuadMesh();
private:
	void buildMaterial(OpenGLMaterial& mat);
	void drawBatch(const GLObject& ob, const Matrix4f& view_mat, const Matrix4f& proj_mat, const OpenGLMaterial& opengl_mat, const OpenGLMeshRenderData& mesh_data, const OpenGLBatch& batch/*, int num_verts_per_primitive*/);
	void drawBatchWireframe(const OpenGLBatch& pass_data, int num_verts_per_primitive);
	static Reference<OpenGLMeshRenderData> make3DArrowMesh();
	static Reference<OpenGLMeshRenderData> makeCubeMesh();
	

	bool init_succeeded;
	std::string initialisation_error_msg;
	
	Vec4f sun_dir; // Dir to sun.
	Vec4f sun_dir_cam_space;

	//OpenGLMeshRenderData aabb_meshdata;
	Reference<OpenGLMeshRenderData> sphere_meshdata;
	Reference<OpenGLMeshRenderData> arrow_meshdata;
	Reference<OpenGLMeshRenderData> cube_meshdata;
	GLObjectRef env_ob;

	float sensor_width;
	float viewport_aspect_ratio;
	float lens_sensor_dist;
	float render_aspect_ratio;
	int viewport_w, viewport_h;

	Matrix4f world_to_camera_space_matrix;
public:
	std::vector<Reference<GLObject> > objects;
	std::vector<Reference<GLObject> > transparent_objects;
	std::vector<Reference<OverlayObject> > overlay_objects; // UI overlays
private:
	float max_draw_dist;

	GLuint timer_query_id;


	uint64 num_face_groups_submitted;
	uint64 num_indices_submitted;
	uint64 num_aabbs_submitted;

	Plane<float> frustum_clip_planes[5];
	Vec4f frustum_verts[5];
	js::AABBox frustum_aabb;

	Reference<OpenGLProgram> phong_untextured_prog;
	int diffuse_colour_location;
	int have_shading_normals_location;
	int have_texture_location;
	int diffuse_tex_location;
	int phong_have_depth_texture_location;
	int phong_depth_tex_location;
	int texture_matrix_location;
	int sundir_location;
	int exponent_location;
	int fresnel_scale_location;
	int phong_shadow_texture_matrix_location;

	Reference<OpenGLProgram> transparent_prog;
	int transparent_colour_location;
	int transparent_have_shading_normals_location;

	Reference<OpenGLProgram> env_prog;
	int env_diffuse_colour_location;
	int env_have_texture_location;
	int env_diffuse_tex_location;
	int env_texture_matrix_location;

	Reference<OpenGLProgram> overlay_prog;
	int overlay_diffuse_colour_location;
	int overlay_have_texture_location;
	int overlay_diffuse_tex_location;
	int overlay_texture_matrix_location;

	//size_t vert_mem_used; // B
	//size_t index_mem_used; // B

	std::string shader_dir;

	Reference<ShadowMapping> shadow_mapping;
	OpenGLMaterial depth_draw_mat;

	OverlayObjectRef tex_preview_overlay_ob;
public:
	bool anisotropic_filtering_supported;
	float max_anisotropy;

	OpenGLEngineSettings settings;
};
