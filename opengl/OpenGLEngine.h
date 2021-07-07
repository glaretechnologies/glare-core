/*=====================================================================
OpenGLEngine.h
--------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "IncludeOpenGL.h"
#include "../utils/IncludeWindows.h" // This needs to go first for NOMINMAX.
#include "TextureLoading.h"
#include "../graphics/colour3.h"
#include "../graphics/Colour4f.h"
#include "../graphics/BatchedMesh.h"
#include "../physics/jscol_aabbox.h"
#include "../opengl/OpenGLTexture.h"
#include "../opengl/OpenGLProgram.h"
#include "../opengl/ShadowMapping.h"
#include "../opengl/VBO.h"
#include "../opengl/VAO.h"
#include "../opengl/UniformBufOb.h"
#include "../maths/vec2.h"
#include "../maths/vec3.h"
#include "../maths/Matrix2.h"
#include "../maths/matrix3.h"
#include "../maths/plane.h"
#include "../maths/Matrix4f.h"
#include "../maths/PCG32.h"
#include "../utils/Timer.h"
#include "../utils/Vector.h"
#include "../utils/Reference.h"
#include "../utils/RefCounted.h"
#include "../utils/Task.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/StringUtils.h"
#include "../utils/PrintOutput.h"
#include <unordered_set>
namespace Indigo { class Mesh; }
namespace glare { class TaskManager; }
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


struct GLMemUsage
{
	GLMemUsage() : geom_cpu_usage(0), geom_gpu_usage(0), texture_cpu_usage(0), texture_gpu_usage(0) {}

	size_t geom_cpu_usage;
	size_t geom_gpu_usage;

	size_t texture_cpu_usage;
	size_t texture_gpu_usage;

	size_t totalCPUUsage() const;
	size_t totalGPUUsage() const;

	void operator += (const GLMemUsage& other);
};


// OpenGL data for a given mesh.
class OpenGLMeshRenderData : public ThreadSafeRefCounted
{
public:
	OpenGLMeshRenderData() : has_vert_colours(false) {}

	GLARE_ALIGNED_16_NEW_DELETE

	GLMemUsage getTotalMemUsage() const;
	size_t getNumVerts() const; // Just for testing/debugging.  Only valid before data is uploaded and host buffers are cleared.
	size_t getNumTris() const; // Just for testing/debugging.  Only valid before data is uploaded and host buffers are cleared.

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
	bool has_vert_colours;
	GLenum index_type; // One of GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, or GL_UNSIGNED_INT.

	VertexSpec vertex_spec;
	
	VAORef vert_vao;

	// If this is non-null, load vertex and index data directly from batched_mesh instead of from vert_data and vert_index_buffer etc..
	// We take this approach to avoid copying the data from batched_mesh to vert_data etc..
	Reference<BatchedMesh> batched_mesh;
};


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
		metallic_frac(0.f),
		gen_planar_uvs(false),
		draw_planar_uv_grid(false),
		convert_albedo_from_srgb(false)
	{}

	Colour3f albedo_rgb; // First approximation to material colour.  Non-linear sRGB.
	float alpha; // Used for transparent mats.

	bool transparent;
	bool gen_planar_uvs;
	bool draw_planar_uv_grid;
	bool convert_albedo_from_srgb;

	Reference<OpenGLTexture> albedo_texture;
	Reference<OpenGLTexture> lightmap_texture;
	Reference<OpenGLTexture> texture_2;

	Reference<OpenGLProgram> shader_prog;

	Matrix2f tex_matrix;
	Vec2f tex_translation;

	float roughness;
	float fresnel_scale;
	float metallic_frac;
	
	uint64 userdata;
	std::string tex_path;      // Kind-of user-data.  Only used in textureLoaded currently, which should be removed/refactored.
	std::string lightmap_path; // Kind-of user-data.  Only used in textureLoaded currently, which should be removed/refactored.

	js::Vector<OpenGLUniformVal, 16> user_uniform_vals;
};


struct OpenGLTextureKey
{
	OpenGLTextureKey() {}
	explicit OpenGLTextureKey(const std::string& path_) : path(path_) {}
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

	GLObject() : object_type(0), line_width(1.f), random_num(0) {}

	void enableInstancing(const VBORef new_instance_matrix_vbo); // Enables instancing attributes, and builds vert_vao.

	Matrix4f ob_to_world_matrix;
	Matrix4f ob_to_world_inv_transpose_matrix; // inverse transpose of upper-left part of to-world matrix.

	js::AABBox aabb_ws;

	Reference<OpenGLMeshRenderData> mesh_data;

	VAORef vert_vao; // Overrides mesh_data->vert_vao if non-null.  Having a vert_vao here allows us to enable instancing, by binding to the instance_matrix_vbo etc..

	VBORef instance_matrix_vbo;
	VBORef instance_colour_vbo;
	
	std::vector<OpenGLMaterial> materials;

	int object_type; // 0 = tri mesh
	float line_width;
	uint32 random_num;
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

	OverlayObject();

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
	OpenGLEngineSettings() : enable_debug_output(false), shadow_mapping(false), compress_textures(false), use_final_image_buffer(false), depth_fog(false) {}

	bool enable_debug_output;
	bool shadow_mapping;
	bool compress_textures;
	bool use_final_image_buffer; // Render to an off-screen buffer, which can be used for post-processing.  Required for bloom post-processing.
	bool depth_fog;
};


// The OpenGLEngine contains one or more OpenGLScene.
// An OpenGLScene is a set of objects, plus a camera transform, and associated information.
// The current scene being rendered by the OpenGLEngine can be set with OpenGLEngine::setCurrentScene().
class OpenGLScene : public ThreadSafeRefCounted
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	OpenGLScene();

	friend class OpenGLEngine;

	void setPerspectiveCameraTransform(const Matrix4f& world_to_camera_space_matrix_, float sensor_width_, float lens_sensor_dist_, float render_aspect_ratio_, float lens_shift_up_distance_,
		float lens_shift_right_distance_, float viewport_aspect_ratio);
	void setOrthoCameraTransform(const Matrix4f& world_to_camera_space_matrix_, float sensor_width_, float render_aspect_ratio_, float lens_shift_up_distance_,
		float lens_shift_right_distance_, float viewport_aspect_ratio);
	void setIdentityCameraTransform();

	void calcCamFrustumVerts(float near_dist, float far_dist, Vec4f* verts_out);

	void unloadAllData();

	GLMemUsage getTotalMemUsage() const;

	Colour3f background_colour;

	bool use_z_up; // Should the +z axis be up (for cameras, sun lighting etc..).  True by default.  If false, y is up.
	// NOTE: Use only with CameraType_Identity currently, clip planes won't be computed properly otherwise.

	GLenum dest_blending_factor; // Defaults to GL_ONE_MINUS_SRC_ALPHA

	float bloom_strength; // [0-1].  Strength 0 turns off bloom.  0 by default.
private:
	float use_sensor_width;
	float use_sensor_height;
	float sensor_width;
	
	float lens_sensor_dist;
	float render_aspect_ratio;
	float lens_shift_up_distance;
	float lens_shift_right_distance;

	enum CameraType
	{
		CameraType_Identity, // Identity camera transform.
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

	GLObjectRef env_ob;
private:
	float max_draw_dist;
	float near_draw_dist;

	Planef frustum_clip_planes[6];
	int num_frustum_clip_planes;
	Vec4f frustum_verts[8];
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
	Reference<OpenGLTexture> depth_texture;
};


struct BatchDrawInfo
{
	const GLObject* ob;
	const OpenGLBatch* batch;
	const OpenGLMaterial* mat;
	const OpenGLProgram* prog;
	uint32 flags; // 1 = use shading normals

	bool operator < (const BatchDrawInfo& other) const
	{
		if(prog < other.prog)
			return true;
		else if(prog > other.prog)
			return false;
		else
			return ob->mesh_data.ptr() < other.ob->mesh_data.ptr();
	}
};


// Matches that defined in phong_frag_shader.glsl.
struct PhongUniforms
{
	Vec4f sundir_cs;
	Colour4f diffuse_colour;
	float texture_matrix[12];
	int have_shading_normals;
	int have_texture;
	float roughness;
	float fresnel_scale;
	float metallic_frac;
};


struct SharedVertUniforms
{
	Matrix4f proj_matrix; // same for all objects
	Matrix4f view_matrix; // same for all objects
	//#if NUM_DEPTH_TEXTURES > 0
	Matrix4f shadow_texture_matrix[ShadowMapping::NUM_DYNAMIC_DEPTH_TEXTURES + ShadowMapping::NUM_STATIC_DEPTH_TEXTURES]; // same for all objects
	//#endif
	Vec4f campos_ws; // same for all objects
};


struct PerObjectVertUniforms
{
	Matrix4f model_matrix; // per-object
	Matrix4f normal_matrix; // per-object
};


class OpenGLEngine : public ThreadSafeRefCounted
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	OpenGLEngine(const OpenGLEngineSettings& settings);
	~OpenGLEngine();

	friend class TextureLoading;

	//---------------------------- Initialisation/deinitialisation --------------------------
	void initialise(const std::string& data_dir, TextureServer* texture_server, PrintOutput* print_output); // data_dir should have 'shaders' and 'gl_data' in it.
	bool initSucceeded() const { return init_succeeded; }
	std::string getInitialisationErrorMsg() const { return initialisation_error_msg; }

	void unloadAllData();
	//----------------------------------------------------------------------------------------


	//---------------------------- Adding and removing objects -------------------------------
	void addObject(const Reference<GLObject>& object);
	void removeObject(const Reference<GLObject>& object);
	bool isObjectAdded(const Reference<GLObject>& object) const;

	// Overlay objects are considered to be in OpenGL clip-space coordinates.
	// The x axis is to the right, y up, and negative z away from the camera into the scene.
	// x=-1 is the left edge of the viewport, x=+1 is the right edge.
	// y=-1 is the bottom edge of the viewport, y=+1 is the top edge.
	//
	// Overlay objects are drawn back-to-front, without z-testing, using the overlay shaders.
	void addOverlayObject(const Reference<OverlayObject>& object);
	void removeOverlayObject(const Reference<OverlayObject>& object);
	//----------------------------------------------------------------------------------------


	//---------------------------- Updating objects ------------------------------------------
	void updateObjectTransformData(GLObject& object);
	const js::AABBox getAABBWSForObjectWithTransform(GLObject& object, const Matrix4f& to_world);

	void newMaterialUsed(OpenGLMaterial& mat, bool use_vert_colours, bool uses_instancing);
	void objectMaterialsUpdated(const Reference<GLObject>& object);
	//----------------------------------------------------------------------------------------


	//---------------------------- Object selection ------------------------------------------
	void selectObject(const Reference<GLObject>& object);
	void deselectObject(const Reference<GLObject>& object);
	void setSelectionOutlineColour(const Colour4f& col);
	//----------------------------------------------------------------------------------------


	//---------------------------- Texture loading -------------------------------------------
	// Return an OpenGL texture based on tex_path.  Loads it from disk if needed.  Blocking.
	// Throws glare::Exception
	Reference<OpenGLTexture> getTexture(const std::string& tex_path);

	// If the texture identified by tex_path has been loaded and processed, load into OpenGL if needed, then return the OpenGL texture.
	// If the texture is not loaded or not processed yet, return a null reference.
	// Throws glare::Exception
	Reference<OpenGLTexture> getTextureIfLoaded(const OpenGLTextureKey& key);

	// Notify the OpenGL engine that a texture has been loaded.
	void textureLoaded(const std::string& path, const OpenGLTextureKey& key);

	Reference<OpenGLTexture> loadCubeMap(const std::vector<Reference<Map2D> >& face_maps,
		OpenGLTexture::Filtering filtering = OpenGLTexture::Filtering_Fancy, OpenGLTexture::Wrapping wrapping = OpenGLTexture::Wrapping_Repeat);

	Reference<OpenGLTexture> loadOpenGLTextureFromTexData(const OpenGLTextureKey& key, Reference<TextureData> texture_data,
		OpenGLTexture::Filtering filtering, OpenGLTexture::Wrapping wrapping);

	Reference<OpenGLTexture> getOrLoadOpenGLTexture(const OpenGLTextureKey& key, const Map2D& map2d, /*BuildUInt8MapTextureDataScratchState& state,*/
		OpenGLTexture::Filtering filtering = OpenGLTexture::Filtering_Fancy, OpenGLTexture::Wrapping wrapping = OpenGLTexture::Wrapping_Repeat);

	void addOpenGLTexture(const OpenGLTextureKey& key, const Reference<OpenGLTexture>& tex);

	void removeOpenGLTexture(const OpenGLTextureKey& key); // Erases from opengl_textures.

	bool isOpenGLTextureInsertedForKey(const OpenGLTextureKey& key) const;

	TextureServer* getTextureServer() { return texture_server; }
	//------------------------------- End texture loading ------------------------------------


	//------------------------------- Camera management --------------------------------------
	void setPerspectiveCameraTransform(const Matrix4f& world_to_camera_space_matrix, float sensor_width, float lens_sensor_dist, float render_aspect_ratio, float lens_shift_up_distance,
		float lens_shift_right_distance);

	// If world_to_camera_space_matrix is the identity, results in a camera, with forwards = +y, right = +x, up = +z. 
	// Left clip plane will be at camera position x - sensor_width/2, right clip plane at camera position x + sensor_width/2
	// Cam space width = sensor_width.
	void setOrthoCameraTransform(const Matrix4f& world_to_camera_space_matrix, float sensor_width, float render_aspect_ratio, float lens_shift_up_distance,
		float lens_shift_right_distance);

	void setIdentityCameraTransform(); // See also use_z_up to use z-up like opengl.
	//----------------------------------------------------------------------------------------


	//------------------------------- Environment material/map management --------------------
	void setSunDir(const Vec4f& d);
	const Vec4f getSunDir() const;

	void setEnvMapTransform(const Matrix3f& transform);

	void setEnvMat(const OpenGLMaterial& env_mat);
	const OpenGLMaterial& getEnvMat() const { return current_scene->env_ob->materials[0]; }
	//----------------------------------------------------------------------------------------


	//--------------------------------------- Drawing ----------------------------------------
	void setDrawWireFrames(bool draw_wireframes_) { draw_wireframes = draw_wireframes_; }
	void setMaxDrawDistance(float d) { current_scene->max_draw_dist = d; } // Set far draw distance
	void setNearDrawDistance(float d) { current_scene->near_draw_dist = d; } // Set near draw distance
	void setCurrentTime(float time);

	void draw();
	//----------------------------------------------------------------------------------------


	//--------------------------------------- Viewport ----------------------------------------
	// This will be the resolution at which the main render framebuffer is allocated, for which res bloom textures are allocated etc..
	void setMainViewport(int main_viewport_w_, int main_viewport_h_);


	void setViewport(int viewport_w_, int viewport_h_);
	int getViewPortWidth()  const { return viewport_w; }
	int getViewPortHeight() const { return viewport_h; }
	float getViewPortAspectRatio() const { return (float)getViewPortWidth() / (float)(getViewPortHeight()); } // Viewport width / viewport height.
	//----------------------------------------------------------------------------------------


	//----------------------------------- Mesh functions -------------------------------------
	Reference<OpenGLMeshRenderData> getLineMeshData() { return line_meshdata; } // A line from (0, 0, 0) to (1, 0, 0)
	Reference<OpenGLMeshRenderData> getSphereMeshData() { return sphere_meshdata; }
	Reference<OpenGLMeshRenderData> getCubeMeshData() { return cube_meshdata; }
	Reference<OpenGLMeshRenderData> getUnitQuadMeshData() { return unit_quad_meshdata; } // A quad from (0, 0, 0) to (1, 1, 0)
	Reference<OpenGLMeshRenderData> getCylinderMesh(); // A cylinder from (0,0,0), to (0,0,1) with radius 1;

	static Matrix4f arrowObjectTransform(const Vec4f& startpos, const Vec4f& endpos, float radius_scale);
	GLObjectRef makeArrowObject(const Vec4f& startpos, const Vec4f& endpos, const Colour4f& col, float radius_scale);
	GLObjectRef makeAABBObject(const Vec4f& min_, const Vec4f& max_, const Colour4f& col);
	static Reference<OpenGLMeshRenderData> makeQuadMesh(const Vec4f& i, const Vec4f& j);
	static Reference<OpenGLMeshRenderData> makeCapsuleMesh(const Vec3f& bottom_spans, const Vec3f& top_spans);

	// Build OpenGLMeshRenderData from an Indigo::Mesh.
	// Throws glare::Exception on failure.
	static Reference<OpenGLMeshRenderData> buildIndigoMesh(const Reference<Indigo::Mesh>& mesh_, bool skip_opengl_calls);

	// May keep a reference to the mesh in the newly created OpenGLMeshRenderData.
	static Reference<OpenGLMeshRenderData> buildBatchedMesh(const Reference<BatchedMesh>& mesh_, bool skip_opengl_calls, const VBORef& instancing_matrix_data);

	static void buildMeshRenderData(OpenGLMeshRenderData& meshdata, const js::Vector<Vec3f, 16>& vertices, const js::Vector<Vec3f, 16>& normals, const js::Vector<Vec2f, 16>& uvs, const js::Vector<uint32, 16>& indices);

	static void loadOpenGLMeshDataIntoOpenGL(OpenGLMeshRenderData& data);
	//---------------------------- End mesh functions ----------------------------------------


	//----------------------------------- Readback -------------------------------------------
	float getPixelDepth(int pixel_x, int pixel_y);

	Reference<ImageMap<uint8, UInt8ComponentValueTraits> > getRenderedColourBuffer();
	//----------------------------------------------------------------------------------------


	//----------------------------------- Target framebuffer ---------------------------------
	// Set the primary render target frame buffer.
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
	//----------------------------------------------------------------------------------------

private:
	void calcCamFrustumVerts(float near_dist, float far_dist, Vec4f* verts_out);
	void assignShaderProgToMaterial(OpenGLMaterial& material, bool use_vert_colours, bool uses_instancing);
	void drawBatch(const GLObject& ob, const Matrix4f& view_mat, const Matrix4f& proj_mat, const OpenGLMaterial& opengl_mat, 
		const OpenGLProgram& shader_prog, const OpenGLMeshRenderData& mesh_data, const OpenGLBatch& batch);
	void drawPrimitives(const GLObject& ob, const Matrix4f& view_mat, const Matrix4f& proj_mat, const OpenGLMaterial& opengl_mat, 
		const OpenGLProgram& shader_prog, const OpenGLMeshRenderData& mesh_data, uint32 prim_start_offset, uint32 num_indices);
	void buildOutlineTextures();
	static Reference<OpenGLMeshRenderData> make3DArrowMesh();
public:
	static Reference<OpenGLMeshRenderData> makeCubeMesh();
private:
	static Reference<OpenGLMeshRenderData> makeUnitQuadMesh(); // Makes a quad from (0, 0, 0) to (1, 1, 0)
	void drawDebugPlane(const Vec3f& point_on_plane, const Vec3f& plane_normal, const Matrix4f& view_matrix, const Matrix4f& proj_matrix,
		float plane_draw_half_width);
public:
	static void getPhongUniformLocations(Reference<OpenGLProgram>& phong_prog, bool shadow_mapping_enabled, UniformLocations& phong_locations_out);
private:
	void setUniformsForPhongProg(const OpenGLMaterial& opengl_mat, const OpenGLMeshRenderData& mesh_data, const UniformLocations& locations);
	void partiallyClearBuffer(const Vec2f& begin, const Vec2f& end);

	void addDebugHexahedron(const Vec4f* verts_ws, const Colour4f& col);
	static Reference<OpenGLMeshRenderData> makeCylinderMesh(); // Make a cylinder from (0,0,0), to (0,0,1) with radius 1;

	OpenGLProgramRef getProgramWithFallbackOnError(const ProgramKey& key);

	OpenGLProgramRef getPhongProgram(const ProgramKey& key); // Throws glare::Exception on shader compilation failure.
	OpenGLProgramRef getTransparentProgram(const ProgramKey& key); // Throws glare::Exception on shader compilation failure.
	OpenGLProgramRef getDepthDrawProgram(const ProgramKey& key); // Throws glare::Exception on shader compilation failure.
public:
	glare::TaskManager& getTaskManager();
private:
	bool init_succeeded;
	std::string initialisation_error_msg;
	
	Vec4f sun_dir; // Dir to sun.
	Vec4f sun_dir_cam_space;

	Reference<OpenGLMeshRenderData> line_meshdata;
	Reference<OpenGLMeshRenderData> sphere_meshdata;
	Reference<OpenGLMeshRenderData> arrow_meshdata;
	Reference<OpenGLMeshRenderData> cube_meshdata;
	Reference<OpenGLMeshRenderData> unit_quad_meshdata;
	Reference<OpenGLMeshRenderData> cylinder_meshdata;


	int main_viewport_w, main_viewport_h;

	int viewport_w, viewport_h;
	
	Planef shadow_clip_planes[6];
	std::vector<OverlayObject*> temp_obs;

#if !defined(OSX)
	GLuint timer_query_id;
#endif

	uint64 num_face_groups_submitted;
	uint64 num_indices_submitted;
	uint64 num_aabbs_submitted;

	std::string preprocessor_defines;

	// Map from preprocessor defs to phong program.
	std::map<ProgramKey, OpenGLProgramRef> progs;
	OpenGLProgramRef fallback_phong_prog;
	OpenGLProgramRef fallback_transparent_prog;

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

	Reference<OpenGLProgram> downsize_prog;
	Reference<OpenGLProgram> gaussian_blur_prog;

	Reference<OpenGLProgram> final_imaging_prog; // Adds bloom, tonemaps

	//size_t vert_mem_used; // B
	//size_t index_mem_used; // B

	std::string data_dir;

	Reference<ShadowMapping> shadow_mapping;
	Reference<OpenGLProgram> depth_draw_prog;
	Reference<OpenGLProgram> depth_draw_alpha_prog;
	Reference<OpenGLProgram> depth_draw_instancing_prog;
	Reference<OpenGLProgram> depth_draw_alpha_instancing_prog;

	OverlayObjectRef clear_buf_overlay_ob;

	double draw_time;
	Timer draw_timer;

	std::map<OpenGLTextureKey, Reference<OpenGLTexture> > opengl_textures;
public:
	Reference<TextureDataManager> texture_data_manager;
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
	//Reference<OpenGLTexture> main_render_texture;
	//Reference<OpenGLTexture> main_depth_texture;
	std::map<Vec2i, FrameBufTextures> main_render_textures; // Map from (viewport w, viewport_h) to framebuffer textures of that size.

	std::unordered_set<GLObject*> selected_objects;

	bool draw_wireframes;

	GLObjectRef debug_arrow_ob;

	Reference<OpenGLTexture> cosine_env_tex;
	Reference<OpenGLTexture> specular_env_tex;

	float current_time;

	TextureServer* texture_server;

	Reference<FrameBuffer> target_frame_buffer;

	mutable Mutex task_manager_mutex;
	glare::TaskManager* task_manager; // Used for building 8-bit texture data (DXT compression, mip-map data building).  Lazily created when needed.
public:
	std::string opengl_vendor;
	std::string opengl_renderer;
	std::string opengl_version;
	std::string glsl_version;

	bool GL_EXT_texture_sRGB_support;
	bool GL_EXT_texture_compression_s3tc_support;
	float max_anisotropy;

	OpenGLEngineSettings settings;

	uint64 frame_num;

	bool are_8bit_textures_sRGB; // If true, load textures as sRGB, otherwise treat them as in an 8-bit colour space.  Defaults to true.
	// Currently compressed textures are always treated as sRGB.

private:
	std::unordered_set<Reference<OpenGLScene>, OpenGLSceneHash> scenes;
	Reference<OpenGLScene> current_scene;

	PCG32 rng;

	uint64 last_num_obs_in_frustum;

	Reference<const OpenGLProgram> current_bound_prog;

	js::Vector<BatchDrawInfo, 16> batch_draw_info;
	uint32 num_prog_changes;
	uint32 last_num_prog_changes;


	UniformBufObRef phong_uniform_buf_ob;
	UniformBufObRef shared_vert_uniform_buf_ob;
	UniformBufObRef per_object_vert_uniform_buf_ob;

public:
	PrintOutput* print_output; // May be NULL
};


#ifdef _WIN32
#pragma warning(pop)
#endif
