
in vec3 position_in;
in vec3 normal_in;

#if SKINNING
in uvec4 joint;
in vec4 weight;
#endif

#if SKINNING
layout (std140) uniform JointMatrixUniforms
{
	mat4 joint_matrix[256];
};
#endif

layout (std140) uniform PerObjectVertUniforms
{
	PerObjectVertUniformsStruct per_object_data;
};



void main()
{
	vec4 pos_os = per_object_data.dequantise_scale * vec4(position_in.xyz, 1.0) + per_object_data.dequantise_translation;

#if SKINNING
	// See https://www.khronos.org/files/gltf20-reference-guide.pdf
	mat4 skin_matrix =
		weight.x * joint_matrix[int(joint.x & 0xFF)] +
		weight.y * joint_matrix[int(joint.y & 0xFF)] +
		weight.z * joint_matrix[int(joint.z & 0xFF)] +
		weight.w * joint_matrix[int(joint.w & 0xFF)];

	gl_Position = proj_matrix * (view_matrix * (per_object_data.model_matrix * (skin_matrix * pos_os)));
#else
	gl_Position = proj_matrix * (view_matrix * (per_object_data.model_matrix * pos_os));
#endif
}
