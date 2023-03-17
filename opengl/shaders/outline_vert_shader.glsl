
in vec3 position_in;
in vec3 normal_in;

#if SKINNING
in vec4 joint;
in vec4 weight;
#endif

uniform mat4 proj_matrix;
uniform mat4 model_matrix;
uniform mat4 view_matrix;

#if SKINNING
uniform mat4 joint_matrix[128]; // Nvidia drivers are giving internal errors with 256 elements (march 2023)
#endif


void main()
{
#if SKINNING
	// See https://www.khronos.org/files/gltf20-reference-guide.pdf
	mat4 skin_matrix =
		weight.x * joint_matrix[int(joint.x)] +
		weight.y * joint_matrix[int(joint.y)] +
		weight.z * joint_matrix[int(joint.z)] +
		weight.w * joint_matrix[int(joint.w)];

	gl_Position = proj_matrix * (view_matrix * (model_matrix * (skin_matrix * vec4(position_in, 1.0))));
#else
	gl_Position = proj_matrix * (view_matrix * (model_matrix * vec4(position_in, 1.0)));
#endif
}
