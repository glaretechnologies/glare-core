/*=====================================================================
OpenGLEngine.h
--------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "../utils/IncludeWindows.h" // This needs to go first for NOMINMAX.
#include "TextureLoading.h"
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
#include "../utils/Vector.h"
#include "../utils/Reference.h"
#include "../utils/RefCounted.h"
#include "../utils/Task.h"
#include "../utils/ThreadSafeRefCounted.h"
#include <unordered_set>
namespace Indigo { class Mesh; }
namespace Indigo { class TaskManager; }
class Map2D;
class TextureServer;
class UInt8ComponentValueTraits;
template <class V, class VTraits> class ImageMap;


// Data for a bunch of triangles from a given mesh, that all share the same material.
class OpenGLBatch
{
public:
	uint32 material_index;
	uint32 prim_start_offset; // Offset in bytes from the start of the index buffer.
	uint32 num_indices; // Number of vertex indices (= num triangles/3).
};


// OpenGL data for a given mesh.
class OpenGLMeshRenderData : public ThreadSafeRefCounted
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	js::AABBox aabb_os; // Should go first as is aligned.
	
	js::Vector<uint8, 16> vert_data;
	VBORef vert_vbo;

	js::Vector<uint32, 16> vert_index_buffer;
	js::Vector<uint16, 16> vert_index_buffer_uint16;
	js::Vector<uint8, 16> vert_index_buffer_uint8;
	VBORef vert_indices_buf;

	std::vector<OpenGLBatch> batches;
	bool has_uvs;
	bool has_shading_normals;
	GLenum index_type; // One of GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, or GL_UNSIGNED_INT.

	VertexSpec vertex_spec;
	
	VAORef vert_vao;
};


class OpenGLMaterial
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	OpenGLMaterial()
	:	transparent(false),
		albedo_rgb(0.85f, 0.85f, 0.85f),
		alpha(1.f),
		roughness(0.5f),
		tex_matrix(1,0,0,1),
		tex_translation(0,0),
		userdata(0),
		fresnel_scale(0.5f),
		metallic_frac(0.f)
	{}

	std::string albedo_tex_path;

	Colour3f albedo_rgb; // First approximation to material colour.  Non-linear sRGB.
	float alpha; // Used for transparent mats.

	bool transparent;

	Reference<OpenGLTexture> albedo_texture;

	Reference<OpenGLProgram> shader_prog;

	Matrix2f tex_matrix;
	Vec2f tex_translation;

	float roughness;
	float fresnel_scale;
	float metallic_frac;
	
	uint64 userdata;
};


struct OpenGLTextureKey
{
	OpenGLTextureKey() {}
	OpenGLTextureKey(const std::string& path_) : path(path_) {}
	std::string path;

	bool operator < (const OpenGLTextureKey& other) const { return path < other.path; }
};


#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable:4324) // Disable 'structure was padded due to __declspec(align())' warning.
#endif

struct GLObject : public ThreadSafeRefCounted
{
	GLARE_ALIGNED_16_NEW_DELETE

	Matrix4f ob_to_world_matrix;
	Matrix4f ob_to_world_inv_transpose_matrix; // inverse transpose of upper-left part of to-world matrix.

	js::AABBox aabb_ws;

	Reference<OpenGLMeshRenderData> mesh_data;
	
	std::vector<OpenGLMaterial> materials;
};
typedef Reference<GLObject> GLObjectRef;


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

	Matrix4f ob_to_world_matrix;

	Reference<OpenGLMeshRenderData> mesh_data;
	
	OpenGLMaterial material;
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
	OpenGLEngineSettings() : shadow_mapping(false), compress_textures(false) {}

	bool shadow_mapping;
	bool compress_textures;
};


class OpenGLEngine : public ThreadSafeRefCounted
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	OpenGLEngine(const OpenGLEngineSettings& settings);
	~OpenGLEngine();

	friend class TextureLoading;

	void initialise(const std::string& data_dir, TextureServer* texture_server); // data_dir should have 'shaders' and 'gl_data' in it.

	void unloadAllData();

	void addObject(const Reference<GLObject>& object);
	void removeObject(const Reference<GLObject>& object);
	bool isObjectAdded(const Reference<GLObject>& object) const;

	void addOverlayObject(const Reference<OverlayObject>& object);
	void removeOverlayObject(const Reference<OverlayObject>& object);

	void textureLoaded(const std::string& path);

	void selectObject(const Reference<GLObject>& object);
	void deselectObject(const Reference<GLObject>& object);
	void setSelectionOutlineColour(const Colour4f& col);


	void newMaterialUsed(OpenGLMaterial& mat);

	void objectMaterialsUpdated(const Reference<GLObject>& object);

	void draw();

	void setPerspectiveCameraTransform(const Matrix4f& world_to_camera_space_matrix_, float sensor_width, float lens_sensor_dist, float render_aspect_ratio, float lens_shift_up_distance,
		float lens_shift_right_distance);

	void setOrthoCameraTransform(const Matrix4f& world_to_camera_space_matrix_, float sensor_width, float render_aspect_ratio, float lens_shift_up_distance,
		float lens_shift_right_distance);

	void setCurrentTime(float time);

	void setSunDir(const Vec4f& d);

	void setEnvMapTransform(const Matrix3f& transform);

	void setEnvMat(const OpenGLMaterial& env_mat);
	const OpenGLMaterial& getEnvMat() const { return env_ob->materials[0]; }

	void setDrawWireFrames(bool draw_wireframes_) { draw_wireframes = draw_wireframes_; }


	Reference<OpenGLMeshRenderData> getSphereMeshData() { return sphere_meshdata; }
	Reference<OpenGLMeshRenderData> getCubeMeshData() { return cube_meshdata; }

	const js::AABBox getAABBWSForObjectWithTransform(GLObject& object, const Matrix4f& to_world);
	void updateObjectTransformData(GLObject& object);

	void viewportChanged(int viewport_w_, int viewport_h_);
	void setViewportAspectRatio(float r, int viewport_w_, int viewport_h_) { viewport_aspect_ratio = r; viewport_w = viewport_w_; viewport_h = viewport_h_; }

	void setMaxDrawDistance(float d) { max_draw_dist = d; }

	bool initSucceeded() const { return init_succeeded; }
	std::string getInitialisationErrorMsg() const { return initialisation_error_msg; }


	GLObjectRef makeArrowObject(const Vec4f& startpos, const Vec4f& endpos, const Colour4f& col, float radius_scale);
	GLObjectRef makeAABBObject(const Vec4f& min_, const Vec4f& max_, const Colour4f& col);

	static Reference<OpenGLMeshRenderData> makeCapsuleMesh(const Vec3f& bottom_spans, const Vec3f& top_spans);

	// Built OpenGLMeshRenderData from an Indigo::Mesh.
	// Throws Indigo::Exception on failure
	static Reference<OpenGLMeshRenderData> buildIndigoMesh(const Reference<Indigo::Mesh>& mesh_, bool skip_opengl_calls);

	static void loadOpenGLMeshDataIntoOpenGL(OpenGLMeshRenderData& data);

	Reference<OpenGLTexture> loadCubeMap(const std::vector<Reference<Map2D> >& face_maps,
		OpenGLTexture::Filtering filtering = OpenGLTexture::Filtering_Fancy, OpenGLTexture::Wrapping wrapping = OpenGLTexture::Wrapping_Repeat);


	Reference<OpenGLTexture> getOrLoadOpenGLTexture(const OpenGLTextureKey& key, const Map2D& map2d, /*BuildUInt8MapTextureDataScratchState& state,*/
		OpenGLTexture::Filtering filtering = OpenGLTexture::Filtering_Fancy, OpenGLTexture::Wrapping wrapping = OpenGLTexture::Wrapping_Repeat);

	void removeOpenGLTexture(const OpenGLTextureKey& key); // Erases from opengl_textures.

	float getPixelDepth(int pixel_x, int pixel_y);

	Reference<ImageMap<uint8, UInt8ComponentValueTraits> > getRenderedColourBuffer();

	// Set the primary render target frame buffer.
	void setTargetFrameBuffer(unsigned int target_frame_buffer_) { target_frame_buffer = target_frame_buffer_; use_target_frame_buffer = true; }
	void dontUseTargetFrameBuffer() { use_target_frame_buffer = false; }

	static Reference<OpenGLMeshRenderData> makeUnitQuadMesh(); // Makes a quad from (0, 0, 0) to (1, 1, 0)
	static Reference<OpenGLMeshRenderData> makeQuadMesh(const Vec4f& i, const Vec4f& j);
	static Reference<OpenGLMeshRenderData> makeOverlayQuadMesh() { return makeUnitQuadMesh(); }
	
	Reference<OpenGLMeshRenderData> getCylinderMesh(); // A cylinder from (0,0,0), to (0,0,1) with radius 1;

	static void buildMeshRenderData(OpenGLMeshRenderData& meshdata, const js::Vector<Vec3f, 16>& vertices, const js::Vector<Vec3f, 16>& normals, const js::Vector<Vec2f, 16>& uvs, const js::Vector<uint32, 16>& indices);
private:
	struct PhongUniformLocations
	{
		int diffuse_colour_location;
		int have_shading_normals_location;
		int have_texture_location;
		int diffuse_tex_location;
		int cosine_env_tex_location;
		int specular_env_tex_location;
		int texture_matrix_location;
		int sundir_cs_location;
		int roughness_location;
		int fresnel_scale_location;
		int metallic_frac_location;
		int campos_ws_location;

		int dynamic_depth_tex_location;
		int static_depth_tex_location;
		int shadow_texture_matrix_location;
	};

	void calcCamFrustumVerts(float near_dist, float far_dist, Vec4f* verts_out);
	void assignShaderProgToMaterial(OpenGLMaterial& material);
	void buildMaterial(OpenGLMaterial& mat, bool load_textures_immediately);
	void drawBatch(const GLObject& ob, const Matrix4f& view_mat, const Matrix4f& proj_mat, const OpenGLMaterial& opengl_mat, 
		const Reference<OpenGLProgram>& shader_prog, const OpenGLMeshRenderData& mesh_data, const OpenGLBatch& batch);
	void drawBatchWireframe(const OpenGLBatch& pass_data, int num_verts_per_primitive);
	void buildOutlineTexturesForViewport();
	static Reference<OpenGLMeshRenderData> make3DArrowMesh();
	static Reference<OpenGLMeshRenderData> makeCubeMesh();
	void drawDebugPlane(const Vec3f& point_on_plane, const Vec3f& plane_normal, const Matrix4f& view_matrix, const Matrix4f& proj_matrix,
		float plane_draw_half_width);
	static void getPhongUniformLocations(Reference<OpenGLProgram>& phong_prog, bool shadow_mapping_enabled, PhongUniformLocations& phong_locations_out);
	void setUniformsForPhongProg(const OpenGLMaterial& opengl_mat, const OpenGLMeshRenderData& mesh_data,
		const PhongUniformLocations& phong_locations);
	void partiallyClearBuffer(const Vec2f& begin, const Vec2f& end);

	void addDebugHexahedron(const Vec4f* verts_ws, const Colour4f& col);
	static Reference<OpenGLMeshRenderData> makeCylinderMesh(); // Make a cylinder from (0,0,0), to (0,0,1) with radius 1;

	Indigo::TaskManager& getTaskManager();

	bool init_succeeded;
	std::string initialisation_error_msg;
	
	Vec4f sun_dir; // Dir to sun.
	Vec4f sun_dir_cam_space;

	//OpenGLMeshRenderData aabb_meshdata;
	Reference<OpenGLMeshRenderData> sphere_meshdata;
	Reference<OpenGLMeshRenderData> arrow_meshdata;
	Reference<OpenGLMeshRenderData> cube_meshdata;
	Reference<OpenGLMeshRenderData> cylinder_meshdata;
	GLObjectRef env_ob;

	float use_sensor_width;
	float use_sensor_height;
	float sensor_width;
	float viewport_aspect_ratio;
	float lens_sensor_dist;
	float render_aspect_ratio;
	float lens_shift_up_distance;
	float lens_shift_right_distance;
	int viewport_w, viewport_h;
	enum CameraType
	{
		CameraType_Perspective,
		CameraType_Orthographic
	};
	CameraType camera_type;

	Matrix4f world_to_camera_space_matrix;
	Matrix4f cam_to_world;
public:
	std::unordered_set<Reference<GLObject>, GLObjectHash> objects;
	std::unordered_set<Reference<GLObject>, GLObjectHash> transparent_objects;
	std::unordered_set<Reference<OverlayObject>, OverlayObjectHash> overlay_objects; // UI overlays
private:
	float max_draw_dist;

#if !defined(OSX)
	GLuint timer_query_id;
#endif


	uint64 num_face_groups_submitted;
	uint64 num_indices_submitted;
	uint64 num_aabbs_submitted;

	Planef frustum_clip_planes[6];
	int num_frustum_clip_planes;
	Vec4f frustum_verts[8];
	js::AABBox frustum_aabb;

	Planef shadow_clip_planes[6];

	Reference<OpenGLProgram> phong_prog;
	Reference<OpenGLProgram> phong_with_alpha_test_prog;

	PhongUniformLocations phong_locations;
	PhongUniformLocations phong_with_alpha_test_locations;

	Reference<OpenGLProgram> transparent_prog;
	int transparent_colour_location;
	int transparent_have_shading_normals_location;
	int transparent_sundir_cs_location;
	int transparent_specular_env_tex_location;
	int transparent_campos_ws_location;

	Reference<OpenGLProgram> env_prog;
	int env_diffuse_colour_location;
	int env_have_texture_location;
	int env_diffuse_tex_location;
	int env_texture_matrix_location;
	int env_sundir_cs_location;

	Reference<OpenGLProgram> overlay_prog;
	int overlay_diffuse_colour_location;
	int overlay_have_texture_location;
	int overlay_diffuse_tex_location;
	int overlay_texture_matrix_location;

	Reference<OpenGLProgram> outline_prog; // Used for drawing constant flat shaded pixels currently.

	Reference<OpenGLProgram> edge_extract_prog;
	int edge_extract_tex_location;
	int edge_extract_col_location;
	Colour4f outline_colour;

	//size_t vert_mem_used; // B
	//size_t index_mem_used; // B

	std::string data_dir;

	Reference<ShadowMapping> shadow_mapping;
	OpenGLMaterial depth_draw_mat;
	OpenGLMaterial depth_draw_with_alpha_test_mat;
	int depth_diffuse_tex_location;
	int depth_texture_matrix_location;
	int depth_proj_view_model_matrix_location;
	int depth_with_alpha_proj_view_model_matrix_location;

	OverlayObjectRef tex_preview_overlay_ob;

	OverlayObjectRef clear_buf_overlay_ob;

	double draw_time;
	Timer draw_timer;

	std::map<OpenGLTextureKey, Reference<OpenGLTexture> > opengl_textures; // Used for cyberspace
public:
	Reference<TextureDataManager> texture_data_manager;
private:
	size_t outline_tex_w, outline_tex_h;
	Reference<FrameBuffer> outline_solid_fb;
	Reference<FrameBuffer> outline_edge_fb;
	Reference<OpenGLTexture> outline_solid_tex;
	Reference<OpenGLTexture> outline_edge_tex;
	OpenGLMaterial outline_solid_mat; // Material for drawing selected objects with a flat, unshaded colour.
	Reference<OpenGLMeshRenderData> outline_quad_meshdata;
	OpenGLMaterial outline_edge_mat; // Material for drawing the actual edge overlay.

	std::unordered_set<GLObject*> selected_objects;

	bool draw_wireframes;

	GLObjectRef debug_arrow_ob;

	Reference<OpenGLTexture> cosine_env_tex;
	Reference<OpenGLTexture> specular_env_tex;

	float current_time;

	TextureServer* texture_server;

	unsigned int target_frame_buffer;
	bool use_target_frame_buffer;

	mutable Mutex task_manager_mutex;
	Indigo::TaskManager* task_manager; // Used for building 8-bit texture data (DXT compression, mip-map data building).  Lazily created when needed.
public:
	bool GL_EXT_texture_sRGB_support;
	bool GL_EXT_texture_compression_s3tc_support;
	bool anisotropic_filtering_supported;
	float max_anisotropy;

	OpenGLEngineSettings settings;

	uint64 frame_num;
};

#ifdef _WIN32
#pragma warning(pop)
#endif
