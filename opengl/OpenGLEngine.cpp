/*=====================================================================
OpenGLEngine.cpp
----------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include <GL/glew.h>
#include "OpenGLEngine.h"


#include "../dll/include/IndigoMesh.h"
#include "../indigo/globals.h"
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
#include <algorithm>


static const bool PROFILE = false;


OpenGLEngine::OpenGLEngine()
:	init_succeeded(false),
	anisotropic_filtering_supported(false)
{
	viewport_aspect_ratio = 1;
	max_draw_dist = 1;
	render_aspect_ratio = 1;

	sun_dir = normalise(Vec4f(1,1,1,0));
}


OpenGLEngine::~OpenGLEngine()
{
}


static const Vec4f FORWARDS_OS(0.0f, 1.0f, 0.0f, 0.0f); // Forwards in local camera (object) space.
static const Vec4f UP_OS(0.0f, 0.0f, 1.0f, 0.0f);
static const Vec4f RIGHT_OS(1.0f, 0.0f, 0.0f, 0.0f);


void OpenGLEngine::setCameraTransform(const Matrix4f& world_to_camera_space_matrix_, float sensor_width_, /*float sensor_height, */float lens_sensor_dist_, float render_aspect_ratio_)
{
	this->sensor_width = sensor_width_;
	this->world_to_camera_space_matrix = world_to_camera_space_matrix_;
	this->lens_sensor_dist = lens_sensor_dist_;
	this->render_aspect_ratio = render_aspect_ratio_;


	//const double render_aspect_ratio = 1;//(double)rs->getIntSetting("width") / (double)rs->getIntSetting("height");

	float use_sensor_width;
	float use_sensor_height;
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
	const Vec4f lens_center(0,0,0,1);//TEMP
	const Vec4f sensor_center(0,-lens_sensor_dist,0,1);//TEMP

	const Vec4f sensor_bottom = (sensor_center - UP_OS    * (use_sensor_height * 0.5f));
	const Vec4f sensor_top    = (sensor_center + UP_OS    * (use_sensor_height * 0.5f));
	const Vec4f sensor_left   = (sensor_center - RIGHT_OS * (use_sensor_width  * 0.5f));
	const Vec4f sensor_right  = (sensor_center + RIGHT_OS * (use_sensor_width  * 0.5f));

	Planef planes_cs[5]; // Clipping planes in camera space

	// We won't bother with a culling plane at the near clip plane, since it near draw distance is so close to zero, so it's unlikely to cull any objects.

	planes_cs[0] = Planef(
		toVec3f(FORWARDS_OS * this->max_draw_dist), // pos.
		toVec3f(FORWARDS_OS) // normal
	); // far clip plane of frustum

	const Vec4f left_normal = normalise(crossProduct(UP_OS, lens_center - sensor_right));
	planes_cs[1] = Planef(
		toVec3f(lens_center),
		toVec3f(left_normal)
	); // left

	const Vec4f right_normal = normalise(crossProduct(lens_center - sensor_left, UP_OS));
	planes_cs[2] = Planef(
		toVec3f(lens_center),
		toVec3f(right_normal)
	); // right

	const Vec4f bottom_normal = normalise(crossProduct(lens_center - sensor_top, RIGHT_OS));
	planes_cs[3] = Planef(
		toVec3f(lens_center),
		toVec3f(bottom_normal)
	); // bottom

	const Vec4f top_normal = normalise(crossProduct(RIGHT_OS, lens_center - sensor_bottom));
	planes_cs[4] = Planef(
		toVec3f(lens_center),
		toVec3f(top_normal)
	); // top

	// Transform clipping planes to world space
	Matrix4f cam_to_world;
	world_to_camera_space_matrix.getInverseForRandTMatrix(cam_to_world);

	for(int i=0; i<5; ++i)
	{
		// Get point on plane and plane normal in Camera space.
		Vec4f plane_point_cs;
		planes_cs[i].getPointOnPlane().pointToVec4f(plane_point_cs);

		Vec4f plane_normal_cs;
		planes_cs[i].getNormal().vectorToVec4f(plane_normal_cs);

		// Transform from camera space -> world space, then world space -> object space.
		const Vec4f plane_point_ws = cam_to_world * plane_point_cs;
		const Vec4f normal_ws = normalise(cam_to_world * plane_normal_cs);

		frustum_clip_planes[i] = Plane<float>(
			toVec3f(plane_point_ws),
			toVec3f(normal_ws)
		);
	}


	// Calculate frustum verts
	Vec4f verts_cs[5];
	const float d_w = use_sensor_width  * max_draw_dist / (2 * lens_sensor_dist);
	const float d_h = use_sensor_height * max_draw_dist / (2 * lens_sensor_dist);
	verts_cs[0] = lens_center;
	// NOTE: probably wrong for shift lens
	verts_cs[1] = lens_center + FORWARDS_OS * max_draw_dist - RIGHT_OS * d_w - UP_OS * d_h; // bottom left
	verts_cs[2] = lens_center + FORWARDS_OS * max_draw_dist + RIGHT_OS * d_w - UP_OS * d_h; // bottom right
	verts_cs[3] = lens_center + FORWARDS_OS * max_draw_dist + RIGHT_OS * d_w + UP_OS * d_h; // top right
	verts_cs[4] = lens_center + FORWARDS_OS * max_draw_dist - RIGHT_OS * d_w + UP_OS * d_h; // top left

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


void OpenGLEngine::setSunDir(const Vec4f& d)
{
	this->sun_dir = d;
}


void OpenGLEngine::setEnvMapTransform(const Matrix3f& transform)
{
	this->env_mat_transform = transform;
}


void OpenGLEngine::setEnvMat(const OpenGLMaterial& env_mat_)
{
	this->env_mat = env_mat_;
}


static void 
#ifdef _WIN32
	// NOTE: not sure what this should be on non-windows platforms.  APIENTRY does not seem to be defined with GCC on Linux 64.
	APIENTRY 
#endif
myMessageCallback(
	GLenum _source, 
	GLenum _type, GLuint _id, GLenum _severity, 
	GLsizei _length, const char* _message, 
	void* _userParam) 
{
	// See https://www.opengl.org/sdk/docs/man/html/glDebugMessageControl.xhtml

	std::string type;
	switch(_type)
	{
	case GL_DEBUG_TYPE_ERROR: type = "Error"; break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: type = "Deprecated Behaviour"; break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: type = "Undefined Behaviour"; break;
	case GL_DEBUG_TYPE_PORTABILITY: type = "Portability"; break;
	case GL_DEBUG_TYPE_PERFORMANCE: type = "Performance"; break;
	case GL_DEBUG_TYPE_MARKER: type = "Marker"; break;
	case GL_DEBUG_TYPE_PUSH_GROUP: type = "Push group"; break;
	case GL_DEBUG_TYPE_POP_GROUP: type = "Pop group"; break;
	case GL_DEBUG_TYPE_OTHER: type = "Other"; break;
	default: type = "Unknown"; break;
	}

	std::string severity;
	switch(_severity)
	{
	case GL_DEBUG_SEVERITY_LOW: severity = "low"; break;
	case GL_DEBUG_SEVERITY_MEDIUM: severity = "medium"; break;
	case GL_DEBUG_SEVERITY_HIGH : severity = "high"; break;
	case GL_DEBUG_SEVERITY_NOTIFICATION : severity = "notification"; break;
	case GL_DONT_CARE: severity = "Don't care"; break;
	default: severity = "Unknown"; break;
	}

	if(_severity != GL_DEBUG_SEVERITY_NOTIFICATION) // Don't print out notifications by default.
	{
		conPrint("==============================================================");
		conPrint("OpenGL msg, severity: " + severity + ", type: " + type + ":");
		conPrint(std::string(_message));
		conPrint("==============================================================");
	}
}


static void buildPassData(OpenGLPassData& pass_data, const std::vector<Vec3f>& vertices, const std::vector<Vec3f>& normals, const std::vector<Vec2f>& uvs)
{
	pass_data.vertices_vbo = new VBO(&vertices[0].x, vertices.size() * 3);

	if(!normals.empty())
		pass_data.normals_vbo = new VBO(&normals[0].x, normals.size() * 3);

	if(!uvs.empty())
		pass_data.uvs_vbo = new VBO(&uvs[0].x, uvs.size() * 2);
}


void OpenGLEngine::initialise()
{
	GLenum err = glewInit();
	if(GLEW_OK != err)
	{
		conPrint("glewInit failed: " + std::string((const char*)glewGetErrorString(err)));
		init_succeeded = false;
		initialisation_error_msg = std::string((const char*)glewGetErrorString(err));
		return;
	}

	// conPrint("OpenGL version: " + std::string((const char*)glGetString(GL_VERSION)));

	// Check to see if OpenGL 2.0 is supported, which is required for our VBO usage.  (See https://www.opengl.org/sdk/docs/man/html/glGenBuffers.xhtml etc..)
	if(!GLEW_VERSION_2_0)
	{
		conPrint("OpenGL version is too old (< v2.0))");
		init_succeeded = false;
		initialisation_error_msg = "OpenGL version is too old (< v2.0))";
		return;
	}

#if BUILD_TESTS
	if(GLEW_ARB_debug_output)
	{
		// Enable error message handling,.
		// See "Porting Source to Linux: Valve’s Lessons Learned": https://developer.nvidia.com/sites/default/files/akamai/gamedev/docs/Porting%20Source%20to%20Linux.pdf
		glDebugMessageCallbackARB(myMessageCallback, NULL); 
		glEnable(GL_DEBUG_OUTPUT);
	}
	else
		conPrint("GLEW_ARB_debug_output OpenGL extension not available.");
#endif

	// Check if anisotropic texture filtering is available, and get max anisotropy if so.  
	// See 'Texture Mapping in OpenGL: Beyond the Basics' - http://www.informit.com/articles/article.aspx?p=770639&seqNum=2
	if(GLEW_EXT_texture_filter_anisotropic)
	{
		anisotropic_filtering_supported = true;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropy);
	}


	// Set up the rendering context, define display lists etc.:
	glClearColor(32.f / 255.f, 32.f / 255.f, 32.f / 255.f, 1.f);

	glEnable(GL_NORMALIZE);		// Enable normalisation of normals
	glEnable(GL_DEPTH_TEST);	// Enable z-buffering
	glDisable(GL_CULL_FACE);	// Disable backface culling


	if(PROFILE)
		glGenQueries(1, &timer_query_id);

	// Create VBOs for sphere
	{
		int phi_res = 100;
		int theta_res = 50;

		std::vector<Vec3f> verts;
		verts.resize(phi_res * theta_res * 4);
		std::vector<Vec3f> normals;
		normals.resize(phi_res * theta_res * 4);
		std::vector<Vec2f> uvs;
		uvs.resize(phi_res * theta_res * 4);

		int i = 0;
		for(int phi_i=0; phi_i<phi_res; ++phi_i)
		for(int theta_i=0; theta_i<theta_res; ++theta_i)
		{
			const float phi_1 = Maths::get2Pi<float>() * phi_i / phi_res;
			const float phi_2 = Maths::get2Pi<float>() * (phi_i + 1) / phi_res;
			const float theta_1 = Maths::pi<float>() * theta_i / theta_res;
			const float theta_2 = Maths::pi<float>() * (theta_i + 1) / theta_res;

			const Vec4f p1 = GeometrySampling::dirForSphericalCoords(phi_1, theta_1);
			const Vec4f p2 = GeometrySampling::dirForSphericalCoords(phi_2, theta_1);
			const Vec4f p3 = GeometrySampling::dirForSphericalCoords(phi_2, theta_2);
			const Vec4f p4 = GeometrySampling::dirForSphericalCoords(phi_1, theta_2);

			verts[i    ] = Vec3f(p1); 
			verts[i + 1] = Vec3f(p2); 
			verts[i + 2] = Vec3f(p3); 
			verts[i + 3] = Vec3f(p4);

			normals[i    ] = Vec3f(p1); 
			normals[i + 1] = Vec3f(p2);
			normals[i + 2] = Vec3f(p3); 
			normals[i + 3] = Vec3f(p4);

			uvs[i    ] = Vec2f(phi_1, theta_1); 
			uvs[i + 1] = Vec2f(phi_2, theta_1); 
			uvs[i + 2] = Vec2f(phi_2, theta_2); 
			uvs[i + 3] = Vec2f(phi_1, theta_2);

			i += 4;
		}

		sphere_passdata.num_prims = (uint32)verts.size() / 4;
		sphere_passdata.material_index = 0;
		buildPassData(sphere_passdata, verts, normals, uvs);
	}




	// Create VBO for unit AABB.
	{
		std::vector<Vec3f> corners;
		corners.push_back(Vec3f(0,0,0));
		corners.push_back(Vec3f(1,0,0));
		corners.push_back(Vec3f(0,1,0));
		corners.push_back(Vec3f(1,1,0));
		corners.push_back(Vec3f(0,0,1));
		corners.push_back(Vec3f(1,0,1));
		corners.push_back(Vec3f(0,1,1));
		corners.push_back(Vec3f(1,1,1));

		std::vector<Vec3f> verts;
		verts.push_back(corners[0]); verts.push_back(corners[1]); verts.push_back(corners[5]); verts.push_back(corners[4]); // front face
		verts.push_back(corners[4]); verts.push_back(corners[5]); verts.push_back(corners[7]); verts.push_back(corners[6]); // top face
		verts.push_back(corners[7]); verts.push_back(corners[5]); verts.push_back(corners[1]); verts.push_back(corners[3]); // right face
		verts.push_back(corners[6]); verts.push_back(corners[7]); verts.push_back(corners[3]); verts.push_back(corners[2]); // back face
		verts.push_back(corners[4]); verts.push_back(corners[6]); verts.push_back(corners[2]); verts.push_back(corners[0]); // left face
		verts.push_back(corners[0]); verts.push_back(corners[2]); verts.push_back(corners[3]); verts.push_back(corners[1]); // bottom face

		std::vector<Vec3f> normals;
		std::vector<Vec2f> uvs;

		aabb_passdata.num_prims = 6;
		aabb_passdata.material_index = 0;
		buildPassData(aabb_passdata, verts, normals, uvs);
	}

	init_succeeded = true;
}


void OpenGLEngine::unloadAllData()
{
	this->objects.resize(0);

	this->env_mat = OpenGLMaterial();
}


void OpenGLEngine::addObject(const Reference<GLObject>& object)
{
	// Compute world space AABB of object
	
	const Vec4f min_os = object->mesh_data->aabb_os.min_;
	const Vec4f max_os = object->mesh_data->aabb_os.max_;
	const Matrix4f& to_world = object->ob_to_world_matrix;

	js::AABBox bbox_ws = js::AABBox::emptyAABBox();
	bbox_ws.enlargeToHoldPoint(to_world * Vec4f(min_os.x[0], min_os.x[1], min_os.x[2], 1.0f));
	bbox_ws.enlargeToHoldPoint(to_world * Vec4f(min_os.x[0], min_os.x[1], max_os.x[2], 1.0f));
	bbox_ws.enlargeToHoldPoint(to_world * Vec4f(min_os.x[0], max_os.x[1], min_os.x[2], 1.0f));
	bbox_ws.enlargeToHoldPoint(to_world * Vec4f(min_os.x[0], max_os.x[1], max_os.x[2], 1.0f));
	bbox_ws.enlargeToHoldPoint(to_world * Vec4f(max_os.x[0], min_os.x[1], min_os.x[2], 1.0f));
	bbox_ws.enlargeToHoldPoint(to_world * Vec4f(max_os.x[0], min_os.x[1], max_os.x[2], 1.0f));
	bbox_ws.enlargeToHoldPoint(to_world * Vec4f(max_os.x[0], max_os.x[1], min_os.x[2], 1.0f));
	bbox_ws.enlargeToHoldPoint(to_world * Vec4f(max_os.x[0], max_os.x[1], max_os.x[2], 1.0f));
	object->aabb_ws = bbox_ws;

	this->objects.push_back(object);

	// Build the mesh used by the object if it's not built already.
	//if(mesh_render_data.find(object->mesh) == mesh_render_data.end())
	//	this->buildMesh(object->mesh);

	// Build materials
	//for(size_t i=0; i<object->materials.size(); ++i)
	//	buildMaterial(object->materials[i]);
}


void OpenGLEngine::buildMaterial(OpenGLMaterial& opengl_mat)
{
}


// http://iquilezles.org/www/articles/frustumcorrect/frustumcorrect.htm
static bool AABBIntersectsFrustum(const Plane<float>* frustum_clip_planes, const js::AABBox& frustum_aabb, /*const Vec4f* frustum_verts,*/ const js::AABBox& aabb_ws)
{
	const Vec4f min_ws = aabb_ws.min_;
	const Vec4f max_ws = aabb_ws.max_;

	for(int z=0; z<5; ++z) // For each frustum plane:
	{
		const Vec3f normal = frustum_clip_planes[z].getNormal();
		float dist = std::numeric_limits<float>::infinity();
		dist = myMin(dist, dot(normal, Vec3f(min_ws[0], min_ws[1], min_ws[2])));
		dist = myMin(dist, dot(normal, Vec3f(min_ws[0], min_ws[1], max_ws[2])));
		dist = myMin(dist, dot(normal, Vec3f(min_ws[0], max_ws[1], min_ws[2])));
		dist = myMin(dist, dot(normal, Vec3f(min_ws[0], max_ws[1], max_ws[2])));
		dist = myMin(dist, dot(normal, Vec3f(max_ws[0], min_ws[1], min_ws[2])));
		dist = myMin(dist, dot(normal, Vec3f(max_ws[0], min_ws[1], max_ws[2])));
		dist = myMin(dist, dot(normal, Vec3f(max_ws[0], max_ws[1], min_ws[2])));
		dist = myMin(dist, dot(normal, Vec3f(max_ws[0], max_ws[1], max_ws[2])));
		if(dist >= frustum_clip_planes[z].getD())
			return false;
	}

	if(frustum_aabb.disjoint(aabb_ws) != 0) // If the frustum AABB is disjoint from the object AABB:
		return false;

	return true;
}


void OpenGLEngine::draw()
{
	if(PROFILE) glBeginQuery(GL_TIME_ELAPSED, timer_query_id);
	this->num_faces_submitted = 0;
	this->num_face_groups_submitted = 0;
	this->num_aabbs_submitted = 0;

	Timer profile_timer;

	if(!init_succeeded)
		return;

	// NOTE: We want to clear here first, even if the scene node is null.
	// Clearing here fixes the bug with the OpenGL widget buffer not being initialised properly and displaying garbled mem on OS X.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	//const double render_aspect_ratio = 1;//(double)rs->getIntSetting("width") / (double)rs->getIntSetting("height");

	glLineWidth(1);

	// Initialise projection matrix from Indigo camera settings
	{
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		const double z_far  = max_draw_dist;
		const double z_near = max_draw_dist * 2e-5;

		
		double w, h;
		if(viewport_aspect_ratio > render_aspect_ratio)
		{
			h = 0.5 * (this->sensor_width / this->lens_sensor_dist) / render_aspect_ratio;
			w = h * viewport_aspect_ratio;
		}
		else
		{
			w = 0.5 * this->sensor_width / this->lens_sensor_dist;
			h = w / viewport_aspect_ratio;
		}

		const double x_min = z_near * (-w);
		const double x_max = z_near * ( w);

		const double y_min = z_near * (-h);
		const double y_max = z_near * ( h);

		glFrustum(x_min, x_max, y_min, y_max, z_near, z_far);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		// Translate from Indigo to OpenGL camera view basis
		const float e[16] = { 1, 0, 0, 0,	0, 0, -1, 0,	0, 1, 0, 0,		0, 0, 0, 1 };
		glMultMatrixf(e);
	}

	glPushMatrix(); // Push camera transformation matrix

	// Do camera transformation
	glMultMatrixf(world_to_camera_space_matrix.e);
	//conPrint("world_to_camera_space_matrix: \n" + world_to_camera_space_matrix.toString());

	// Draw background env map if there is one.
	{
		if(env_mat.albedo_texture.nonNull())
		{
			glPushMatrix();
			glLoadIdentity();
			// Translate from Indigo to OpenGL camera view basis
			const float e2[16] = { 1, 0, 0, 0,	0, 0, -1, 0,	0, 1, 0, 0,		0, 0, 0, 1 };
			glMultMatrixf(e2);

			Matrix4f world_to_camera_space_no_translation = world_to_camera_space_matrix;
			world_to_camera_space_no_translation.e[12] = 0;
			world_to_camera_space_no_translation.e[13] = 0;
			world_to_camera_space_no_translation.e[14] = 0;
			glMultMatrixf(world_to_camera_space_no_translation.e);

			// Apply any rotation, mirroring etc. of the environment map.
			const float e[16]	= { (float)env_mat_transform.e[0], (float)env_mat_transform.e[3], (float)env_mat_transform.e[6], 0,
									(float)env_mat_transform.e[1], (float)env_mat_transform.e[4], (float)env_mat_transform.e[7], 0,
									(float)env_mat_transform.e[2], (float)env_mat_transform.e[5], (float)env_mat_transform.e[8], 0,	
									0, 0, 0, 1.f };
			glMultMatrixf(e);


			glDepthMask(GL_FALSE); // Disable writing to depth buffer.

			glDisable(GL_LIGHTING);
			glColor3f(1, 1, 1);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

			drawPrimitives(env_mat, sphere_passdata, 4);
			
			glDepthMask(GL_TRUE); // Re-enable writing to depth buffer.

			glPopMatrix();
		}
	}


	// Set up lights
	{
		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0); 

		const float ambient_amount = 0.5f;
		const float directional_amount = 0.5f;
		GLfloat light_ambient[]  = { ambient_amount, ambient_amount, ambient_amount, 1.0f };
		GLfloat light_diffuse[]  = { directional_amount, directional_amount, directional_amount, 1.0f };
		GLfloat light_specular[] = { directional_amount, directional_amount, directional_amount, 1.0f };

		glLightfv(GL_LIGHT0, GL_AMBIENT,  light_ambient);
		glLightfv(GL_LIGHT0, GL_DIFFUSE,  light_diffuse);
		glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
		glLightfv(GL_LIGHT0, GL_POSITION, sun_dir.x);
		glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, (-sun_dir).x);
	}


	// Draw objects.
	uint64 num_frustum_culled = 0;
	for(size_t i=0; i<objects.size(); ++i)
	{
		const GLObject* const ob = objects[i].getPointer();

		// Do Frustum culling
		if(!AABBIntersectsFrustum(frustum_clip_planes, frustum_aabb, ob->aabb_ws))
		{
			num_frustum_culled++;
			continue;
		}

		const Reference<OpenGLMeshRenderData>& render_data = ob->mesh_data;

		glPushMatrix();
		glMultMatrixf(ob->ob_to_world_matrix.e);

		// Draw solid polygons
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		for(uint32 tri_pass = 0; tri_pass < render_data->tri_pass_data.size(); ++tri_pass)
		{
			const uint32 mat_index = render_data->tri_pass_data[tri_pass].material_index;

			drawPrimitives(ob->materials[mat_index], render_data->tri_pass_data[tri_pass], 3);
		}

		for(uint32 quad_pass = 0; quad_pass < render_data->quad_pass_data.size(); ++quad_pass)
		{
			const uint32 mat_index = render_data->quad_pass_data[quad_pass].material_index;

			drawPrimitives(ob->materials[mat_index], render_data->quad_pass_data[quad_pass], 4);
		}

		glPopMatrix();
	}


	// Draw camera frustum for debugging purposes.
	if(false)
	{
		glDisable(GL_LIGHTING);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glColor3f(1.f, 0.f, 0.f);
		glLineWidth(2.0f);
		glBegin(GL_LINES);
		
		glVertex3fv(frustum_verts[0].x);
		glVertex3fv(frustum_verts[1].x);

		glVertex3fv(frustum_verts[0].x);
		glVertex3fv(frustum_verts[2].x);

		glVertex3fv(frustum_verts[0].x);
		glVertex3fv(frustum_verts[3].x);

		glVertex3fv(frustum_verts[0].x);
		glVertex3fv(frustum_verts[4].x);


		glVertex3fv(frustum_verts[1].x);
		glVertex3fv(frustum_verts[2].x);

		glVertex3fv(frustum_verts[2].x);
		glVertex3fv(frustum_verts[3].x);

		glVertex3fv(frustum_verts[3].x);
		glVertex3fv(frustum_verts[4].x);

		glVertex3fv(frustum_verts[4].x);
		glVertex3fv(frustum_verts[1].x);
		glEnd();
	}


	{
		// Draw alpha-blended grey quads on the outside of 'render viewport'.
		// Also draw some blue lines on the edge of the quads.

		// Reset transformations
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glDisable(GL_DEPTH_TEST); // Disable depth test, so the viewport lines are always drawn over existing fragments.
		glDisable(GL_LIGHTING);

		// Set quad colour
		const float grey = 0.5f;
		glColor4d(grey, grey, grey, 0.5f);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		const Vec3d line_colour(0.094, 0.41960784313725490196078431372549, 0.54901960784313725490196078431373);

		const float z = 0.02f;

		if(viewport_aspect_ratio > render_aspect_ratio)
		{
			const float x = render_aspect_ratio / viewport_aspect_ratio;

			// Draw quads
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glBegin(GL_QUADS);

			// Left quad
			glVertex3f(-1, -1, z);
			glVertex3f(-x, -1, z);
			glVertex3f(-x, 1, z);
			glVertex3f(-1, 1, z);

			// Right quad
			glVertex3f(x, -1, z);
			glVertex3f(1, -1, z);
			glVertex3f(1, 1, z);
			glVertex3f(x, 1, z);

			glEnd(); // GL_QUADS
			glDisable(GL_BLEND);

			// Draw vertical lines
			glColor3dv(&line_colour.x);
			glBegin(GL_LINES);

			glVertex3f(x, -1, z); // origin of the line
			glVertex3f(x, 1, z); // ending point of the line

			glVertex3f(-x, -1, z); // origin of the line
			glVertex3f(-x, 1, z); // ending point of the line

			glEnd(); // GL_LINES
		}
		else
		{
			const float y = viewport_aspect_ratio / render_aspect_ratio;

			// Draw quads
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glBegin(GL_QUADS);

			// Bottom quad
			glVertex3f(-1, -1, z);
			glVertex3f(1, -1, z);
			glVertex3f(1, -y, z);
			glVertex3f(-1,-y, z);

			// Top quad
			glVertex3f(-1, y, z);
			glVertex3f(1, y, z);
			glVertex3f(1, 1, z);
			glVertex3f(-1,1, z);

			glEnd(); // GL_QUADS
			glDisable(GL_BLEND);

			// Draw vertical lines
			glColor3dv(&line_colour.x);
			glBegin(GL_LINES);

			glVertex3f(-1, y, z); // origin of the line
			glVertex3f( 1, y, z); // ending point of the line

			glVertex3f(-1, -y, z); // origin of the line
			glVertex3f( 1, -y, z); // ending point of the line

			glEnd(); // GL_LINES
		}
	
		glEnable(GL_DEPTH_TEST);
	}


	glPopMatrix(); // Pop camera transformation matrix

	if(PROFILE)
	{
		const double cpu_time = profile_timer.elapsed();

		glEndQuery(GL_TIME_ELAPSED);

		uint64 elapsed_ns;
		glGetQueryObjectui64v(timer_query_id, GL_QUERY_RESULT, &elapsed_ns); // Blocks

		if(PROFILE) conPrint("Frame times: CPU: " + doubleToStringNDecimalPlaces(cpu_time * 1.0e3, 4) + " ms, GPU: " + doubleToStringNDecimalPlaces(elapsed_ns * 1.0e-6, 4) + " ms.");
		conPrint("Submitted: face groups: " + toString(num_face_groups_submitted) + ", faces: " + toString(num_faces_submitted) + ", aabbs: " + toString(num_aabbs_submitted) + ", " + 
			toString(objects.size() - num_frustum_culled) + "/" + toString(objects.size()) + " obs");
	}
}


// KVP == (key, value) pair
class uint32KVP
{
public:
	inline bool operator()(const std::pair<uint32, uint32>& lhs, const std::pair<uint32, uint32>& rhs) const { return lhs.first < rhs.first; }
};


Reference<OpenGLMeshRenderData> OpenGLEngine::buildIndigoMesh(const Reference<Indigo::Mesh>& mesh_)
{
	const Indigo::Mesh* const mesh = mesh_.getPointer();
	const bool mesh_has_shading_normals = !mesh->vert_normals.empty();
	const bool mesh_has_uvs = mesh->num_uv_mappings > 0;

	Reference<OpenGLMeshRenderData> opengl_render_data = new OpenGLMeshRenderData();

	js::AABBox aabb_os = js::AABBox::emptyAABBox();

	// We will build up vertex data etc.. in these buffers, before the data is uploaded to a VBO.
	std::vector<Vec3f> vertices;
	std::vector<Vec3f> normals;
	std::vector<Vec2f> uvs;

	// Create per-material OpenGL triangle indices
	if(mesh->triangles.size() > 0)
	{
		// Create list of triangle references sorted by material index
		std::vector<std::pair<uint32, uint32> > tri_indices(mesh->triangles.size());
				
		assert(mesh->num_materials_referenced > 0);
		std::vector<uint32> material_tri_counts(mesh->num_materials_referenced, 0); // A count of how many triangles are using the given material i.
				
		for(uint32 t = 0; t < mesh->triangles.size(); ++t)
		{
			tri_indices[t] = std::make_pair(mesh->triangles[t].tri_mat_index, t);
			material_tri_counts[mesh->triangles[t].tri_mat_index]++;
		}
		std::sort(tri_indices.begin(), tri_indices.end(), uint32KVP());

		uint32 current_mat_index = std::numeric_limits<uint32>::max();
		OpenGLPassData* pass_data = NULL;
		uint32 offset = 0; // Offset into pass_data->vertices etc..

		for(uint32 t = 0; t < tri_indices.size(); ++t)
		{
			// If we've switched to a new material then start a new triangle range
			if(tri_indices[t].first != current_mat_index)
			{
				if(pass_data != NULL)
					buildPassData(*pass_data, vertices, normals, uvs); // Build last pass data

				current_mat_index = tri_indices[t].first;

				opengl_render_data->tri_pass_data.push_back(OpenGLPassData());
				pass_data = &opengl_render_data->tri_pass_data.back();

				pass_data->material_index = current_mat_index;
				pass_data->num_prims = material_tri_counts[current_mat_index];

				if(current_mat_index >= material_tri_counts.size())
				{
					//assert(0);
					//return;
					throw Indigo::Exception("error");
				}

				// Resize arrays.
				vertices.resize(material_tri_counts[current_mat_index] * 3);
				normals.resize(material_tri_counts[current_mat_index] * 3);
				if(mesh_has_uvs)
					uvs.resize(material_tri_counts[current_mat_index] * 3);
				offset = 0;
			}


			const Indigo::Triangle& tri = mesh->triangles[tri_indices[t].second];


			// Return if vertex indices are bogus
			const size_t vert_positions_size = mesh->vert_positions.size();
			if(tri.vertex_indices[0] >= vert_positions_size) throw Indigo::Exception("vert index out of bounds");
			if(tri.vertex_indices[1] >= vert_positions_size) throw Indigo::Exception("vert index out of bounds");
			if(tri.vertex_indices[2] >= vert_positions_size) throw Indigo::Exception("vert index out of bounds");

			// Compute geometric normal if there are no shading normals in the mesh.
			Indigo::Vec3f N_g;
			if(!mesh_has_shading_normals)
			{
				Indigo::Vec3f v0 = mesh->vert_positions[tri.vertex_indices[0]];
				Indigo::Vec3f v1 = mesh->vert_positions[tri.vertex_indices[1]];
				Indigo::Vec3f v2 = mesh->vert_positions[tri.vertex_indices[2]];

				N_g = normalise(crossProduct(v1 - v0, v2 - v0));
			}


			for(uint32 i = 0; i < 3; ++i) // For each vert in tri:
			{
				const uint32 vert_i = tri.vertex_indices[i];

				const Indigo::Vec3f& v = mesh->vert_positions[vert_i];
				vertices[offset + i].set(v.x, v.y, v.z);

				if(!mesh_has_shading_normals)
				{
					normals[offset + i].set(N_g.x, N_g.y, N_g.z); // Use geometric normal
				}
				else
				{
					const Indigo::Vec3f& n = mesh->vert_normals[vert_i];
					normals[offset + i].set(n.x, n.y, n.z);
				}

				if(mesh_has_uvs)
				{
					const Indigo::Vec2f& uv = mesh->uv_pairs[tri.uv_indices[i]];
					uvs[offset + i].set(uv.x, uv.y);
				}

				aabb_os.enlargeToHoldPoint(Vec4f(v.x, v.y, v.z, 1.f));
			}
			offset += 3;
		}

		// Build last pass data that won't have been built yet.
		if(pass_data != NULL)
			buildPassData(*pass_data, vertices, normals, uvs);
	}

	// Create per-material OpenGL quad indices
	if(mesh->quads.size() > 0)
	{
		// Create list of quad references sorted by material index
		std::vector<std::pair<uint32, uint32> > quad_indices(mesh->quads.size());

		assert(mesh->num_materials_referenced > 0);
		std::vector<uint32> material_quad_counts(mesh->num_materials_referenced, 0); // A count of how many quads are using the given material i.

		for(uint32 q = 0; q < mesh->quads.size(); ++q)
		{
			quad_indices[q] = std::make_pair(mesh->quads[q].mat_index, q);
			material_quad_counts[mesh->quads[q].mat_index]++;
		}
		std::sort(quad_indices.begin(), quad_indices.end(), uint32KVP());

		uint32 current_mat_index = std::numeric_limits<uint32>::max();
		OpenGLPassData* pass_data = NULL;
		uint32 offset = 0;

		for(uint32 q = 0; q < quad_indices.size(); ++q)
		{
			// If we've switched to a new material then start a new quad range
			if(quad_indices[q].first != current_mat_index)
			{
				if(pass_data != NULL)
					buildPassData(*pass_data, vertices, normals, uvs); // Build last pass data

				current_mat_index = quad_indices[q].first;

				opengl_render_data->quad_pass_data.push_back(OpenGLPassData());
				pass_data = &opengl_render_data->quad_pass_data.back();

				pass_data->material_index = current_mat_index;
				pass_data->num_prims = material_quad_counts[current_mat_index];

				if(current_mat_index >= material_quad_counts.size())
				{
					assert(0);
					throw Indigo::Exception("Error");
				}

				// Resize arrays.
				vertices.resize(material_quad_counts[current_mat_index] * 4);
				normals.resize(material_quad_counts[current_mat_index] * 4);
				if(mesh_has_uvs)
					uvs.resize(material_quad_counts[current_mat_index] * 4);
				offset = 0;
			}

					

			const Indigo::Quad& quad = mesh->quads[quad_indices[q].second];

			// Return if vertex indices are bogus
			const size_t vert_positions_size = mesh->vert_positions.size();
			if(quad.vertex_indices[0] >= vert_positions_size) throw Indigo::Exception("vert index out of bounds");
			if(quad.vertex_indices[1] >= vert_positions_size) throw Indigo::Exception("vert index out of bounds");
			if(quad.vertex_indices[2] >= vert_positions_size) throw Indigo::Exception("vert index out of bounds");
			if(quad.vertex_indices[3] >= vert_positions_size) throw Indigo::Exception("vert index out of bounds");

			// Compute geometric normal if there are no shading normals in the mesh.
			Indigo::Vec3f N_g;
			if(!mesh_has_shading_normals)
			{
				Indigo::Vec3f v0 = mesh->vert_positions[quad.vertex_indices[0]];
				Indigo::Vec3f v1 = mesh->vert_positions[quad.vertex_indices[1]];
				Indigo::Vec3f v2 = mesh->vert_positions[quad.vertex_indices[2]];

				N_g = normalise(crossProduct(v1 - v0, v2 - v0));
			}

			for(uint32 i = 0; i < 4; ++i)
			{
				const uint32 vert_i = quad.vertex_indices[i];

				const Indigo::Vec3f& v = mesh->vert_positions[vert_i];
				vertices[offset + i].set(v.x, v.y, v.z);

				if(mesh_has_shading_normals)
				{
					const Indigo::Vec3f& n = mesh->vert_normals[vert_i];
					normals[offset + i].set(n.x, n.y, n.z);
				}
				else
				{
					normals[offset + i].set(N_g.x, N_g.y, N_g.z); // Use geometric normal
				}

				if(mesh_has_uvs)
				{
					const Indigo::Vec2f& uv = mesh->uv_pairs[quad.uv_indices[i]];
					uvs[offset + i].set(uv.x, uv.y);
				}

				aabb_os.enlargeToHoldPoint(Vec4f(v.x, v.y, v.z, 1.f));
			}
			offset += 4;
		}

		// Build last pass data that won't have been built yet.
		if(pass_data != NULL)
			buildPassData(*pass_data, vertices, normals, uvs);
	}

	opengl_render_data->aabb_os = aabb_os;
	return opengl_render_data;
}


void OpenGLEngine::drawPrimitives(const OpenGLMaterial& opengl_mat, const OpenGLPassData& pass_data, int num_verts_per_primitive)
{
	assert(num_verts_per_primitive == 3 || num_verts_per_primitive == 4);

	if(opengl_mat.transparent)
		return; // Don't render transparent materials in the solid pass

	// Set OpenGL material state
	if(opengl_mat.albedo_texture.nonNull() && pass_data.uvs_vbo.nonNull())
	{
		GLfloat mat_amb_diff[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, mat_amb_diff);

		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, opengl_mat.albedo_texture->texture_handle);

		// Set texture matrix
		glMatrixMode(GL_TEXTURE); // Enter texture matrix mode
		const GLfloat tex_elems[16] = {
			opengl_mat.tex_matrix.e[0], opengl_mat.tex_matrix.e[2], 0, 0,
			opengl_mat.tex_matrix.e[1], opengl_mat.tex_matrix.e[3], 0, 0,
			0, 0, 1, 0,
			opengl_mat.tex_translation.x, opengl_mat.tex_translation.y, 0, 1
		};
		glLoadMatrixf(tex_elems);
		glMatrixMode(GL_MODELVIEW); // Back to modelview mode

		glEnableClientState(GL_TEXTURE_COORD_ARRAY);							
		pass_data.uvs_vbo->bind();
		glTexCoordPointer(2, GL_FLOAT, 0, NULL);
		pass_data.uvs_vbo->unbind();
	}
	else
	{
		GLfloat mat_amb_diff[] = { opengl_mat.albedo_rgb[0], opengl_mat.albedo_rgb[1], opengl_mat.albedo_rgb[2], 1.0f };
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, mat_amb_diff);
	}


	GLfloat mat_spec[] = { opengl_mat.specular_rgb[0], opengl_mat.specular_rgb[1], opengl_mat.specular_rgb[2], 1.0f };
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_spec);
	GLfloat shininess[] = { opengl_mat.shininess };
	glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, shininess);

	// Bind normals
	if(pass_data.normals_vbo.nonNull())
	{
		glEnableClientState(GL_NORMAL_ARRAY);
		pass_data.normals_vbo->bind();
		glNormalPointer(GL_FLOAT, 0, NULL);
		pass_data.normals_vbo->unbind();
	}

	glEnableClientState(GL_VERTEX_ARRAY);

	// Bind vertices
	pass_data.vertices_vbo->bind();
	glVertexPointer(3, GL_FLOAT, 0, NULL);
	pass_data.vertices_vbo->unbind();

	// Draw the triangles or quads
	if(num_verts_per_primitive == 3)
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)pass_data.num_prims * 3);
	else
		glDrawArrays(GL_QUADS, 0, (GLsizei)pass_data.num_prims * 4);

	glDisableClientState(GL_VERTEX_ARRAY);

	if(pass_data.normals_vbo.nonNull())
		glDisableClientState(GL_NORMAL_ARRAY);

	if(opengl_mat.albedo_texture.nonNull())
	{
		glDisable(GL_TEXTURE_2D);

		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	this->num_faces_submitted += pass_data.num_prims;
	this->num_face_groups_submitted++;
}


// glEnableClientState(GL_VERTEX_ARRAY); should be called before this function is called.
void OpenGLEngine::drawPrimitivesWireframe(const OpenGLPassData& pass_data, int num_verts_per_primitive)
{
	assert(glIsEnabled(GL_VERTEX_ARRAY));

	// Bind vertices
	pass_data.vertices_vbo->bind();
	glVertexPointer(3, GL_FLOAT, 0, NULL);
	pass_data.vertices_vbo->unbind();

	// Draw the triangles or quads
	if(num_verts_per_primitive == 3)
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)pass_data.num_prims * 3);
	else
		glDrawArrays(GL_QUADS, 0, (GLsizei)pass_data.num_prims * 4);
}
