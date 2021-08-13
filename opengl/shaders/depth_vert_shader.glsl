#version 150

// For capturing depth data for shadow mapping

in vec3 position_in;
in vec2 texture_coords_0_in;
#if INSTANCE_MATRICES
in mat4 instance_matrix_in;
#endif

#if SKINNING
in vec4 joint;
in vec4 weight;
#endif

out vec2 texture_coords;

uniform mat4 proj_view_model_matrix; 

#if INSTANCE_MATRICES || SKINNING
layout(std140) uniform SharedVertUniforms
{
	mat4 proj_matrix; // same for all objects
	mat4 view_matrix; // same for all objects
//#if NUM_DEPTH_TEXTURES > 0
	mat4 shadow_texture_matrix[5]; // same for all objects
//#endif
	vec3 campos_ws; // same for all objects
};


layout (std140) uniform PerObjectVertUniforms
{
	mat4 model_matrix; // per-object
	mat4 normal_matrix; // per-object
};
#endif


#if SKINNING
uniform mat4 joint_matrix[64];
#endif

void main()
{
#if INSTANCE_MATRICES
	gl_Position = proj_matrix * (view_matrix * (instance_matrix_in * vec4(position_in, 1.0)));
#else

#if SKINNING
	mat4 skin_matrix =
		weight.x * joint_matrix[int(joint.x)] +
		weight.y * joint_matrix[int(joint.y)] +
		weight.z * joint_matrix[int(joint.z)] +
		weight.w * joint_matrix[int(joint.w)];

	mat4 model_skin_matrix = model_matrix * skin_matrix;
	gl_Position = proj_matrix * (view_matrix * (model_skin_matrix * vec4(position_in, 1.0)));
#else // else if !SKINNING:
	gl_Position = proj_view_model_matrix * vec4(position_in, 1.0);
#endif

#endif

	texture_coords = texture_coords_0_in;
}
