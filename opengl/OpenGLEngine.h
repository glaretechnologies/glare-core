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
class OpenGLPassData
{
public:
	uint32 material_index;
	uint32 num_prims; // Number of triangles or quads.

	VBORef vertices_vbo;
	VBORef normals_vbo;
	VBORef uvs_vbo;
};


// OpenGL data for a given mesh.
class OpenGLMeshRenderData : public RefCounted
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	std::vector<OpenGLPassData> tri_pass_data;
	std::vector<OpenGLPassData> quad_pass_data;

	js::AABBox aabb_os;
};


class OpenGLMaterial
{
public:
	OpenGLMaterial()
	:	transparent(false),
		albedo_rgb(0.85f, 0.25f, 0.85f),
		specular_rgb(0.f),
		shininess(100.f),
		tex_matrix(1,0,0,1),
		tex_translation(0,0),
		userdata(0)
	{}

	std::string albedo_tex_path;

	Colour3f albedo_rgb; // First approximation to material colour
	Colour3f specular_rgb; // Used for OpenGL specular colour

	bool transparent;

	Reference<OpenGLTexture> albedo_texture;

	Matrix2f tex_matrix;
	Vec2f tex_translation;

	float shininess;

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

	void initialise();

	void unloadAllData();

	void addObject(const Reference<GLObject>& object);

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
	void drawPrimitives(const OpenGLMaterial& opengl_mat, const OpenGLPassData& pass_data, int num_verts_per_primitive);
	void drawPrimitivesWireframe(const OpenGLPassData& pass_data, int num_verts_per_primitive);


	bool init_succeeded;
	std::string initialisation_error_msg;
	
	OpenGLMaterial env_mat;
	Matrix3f env_mat_transform;

	Vec4f sun_dir;

	OpenGLPassData aabb_passdata;
	OpenGLPassData sphere_passdata;

	float sensor_width;
	float viewport_aspect_ratio;
	float lens_sensor_dist;
	float render_aspect_ratio;

	Matrix4f world_to_camera_space_matrix;
public:
	std::vector<Reference<GLObject> > objects;
private:
	float max_draw_dist;

	GLuint timer_query_id;


	uint64 num_face_groups_submitted;
	uint64 num_faces_submitted;
	uint64 num_aabbs_submitted;

	Plane<float> frustum_clip_planes[5];
	Vec4f frustum_verts[5];
	js::AABBox frustum_aabb;

public:
	bool anisotropic_filtering_supported;
	float max_anisotropy;
};
