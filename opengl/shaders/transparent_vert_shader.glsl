
in vec3 position_in;
in vec3 normal_in;
in vec2 texture_coords_0_in;
#if INSTANCE_MATRICES
in mat4 instance_matrix_in;
#endif

out vec3 normal_cs; // cam (view) space
out vec3 normal_ws; // world space
out vec3 pos_cs;
#if GENERATE_PLANAR_UVS
out vec3 pos_os;
#endif
out vec3 pos_ws;
out vec2 texture_coords;
out vec3 cam_to_pos_ws;

#if USE_MULTIDRAW_ELEMENTS_INDIRECT
out flat int material_index;
#endif


layout(std140) uniform SharedVertUniforms
{
	mat4 proj_matrix; // same for all objects
	mat4 view_matrix; // same for all objects
//#if NUM_DEPTH_TEXTURES > 0
	mat4 shadow_texture_matrix[5]; // same for all objects
//#endif
	vec3 campos_ws; // same for all objects
	float vert_uniforms_time;
};


#if USE_MULTIDRAW_ELEMENTS_INDIRECT

struct PerObjectVertUniformsStruct
{
	mat4 model_matrix; // per-object
	mat4 normal_matrix; // per-object
};

layout(std140) uniform PerObjectVertUniforms
{
	PerObjectVertUniformsStruct per_object_data[256];
};

#else // else if !USE_MULTIDRAW_ELEMENTS_INDIRECT:

layout (std140) uniform PerObjectVertUniforms
{
	mat4 model_matrix; // per-object
	mat4 normal_matrix; // per-object
};

#endif // !USE_MULTIDRAW_ELEMENTS_INDIRECT


void main()
{
#if USE_MULTIDRAW_ELEMENTS_INDIRECT
	material_index = gl_DrawID;
	mat4 model_matrix  = per_object_data[material_index].model_matrix;
	mat4 normal_matrix = per_object_data[material_index].normal_matrix;
#endif

#if INSTANCE_MATRICES //-------------------------
	gl_Position = proj_matrix * (view_matrix * (instance_matrix_in * vec4(position_in, 1.0)));

#if GENERATE_PLANAR_UVS
	pos_os = position_in;
#endif

	pos_ws = (instance_matrix_in * vec4(position_in, 1.0)).xyz;
	cam_to_pos_ws = pos_ws - campos_ws;
	pos_cs = (view_matrix * (instance_matrix_in * vec4(position_in, 1.0))).xyz;

	normal_ws = (instance_matrix_in * vec4(normal_in, 0.0)).xyz;
	normal_cs = (view_matrix * (instance_matrix_in * vec4(normal_in, 0.0))).xyz;
#else //-------- else if !INSTANCE_MATRICES:
	gl_Position = proj_matrix * (view_matrix * (model_matrix * vec4(position_in, 1.0)));

#if GENERATE_PLANAR_UVS
	pos_os = position_in;
#endif

	pos_ws = (model_matrix  * vec4(position_in, 1.0)).xyz;
	cam_to_pos_ws = pos_ws - campos_ws;
	pos_cs = (view_matrix * (model_matrix  * vec4(position_in, 1.0))).xyz;
 
	normal_ws = (normal_matrix * vec4(normal_in, 0.0)).xyz;
	normal_cs = (view_matrix * (normal_matrix * vec4(normal_in, 0.0))).xyz;
#endif //-------------------------
	//texture_coords = texture_coords_0_in;

	texture_coords = texture_coords_0_in;
}
