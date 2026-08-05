
// gaussian_splat_resolve_vert_shader.glsl
// Copyright Glare Technologies Limited 2026 -
//
// Full-viewport quad for the splat resolve pass.  See OpenGLEngine::drawSplatClouds().

in vec3 position_in; // position_in is [0, 1] x [0, 1]

void main()
{
	vec2 usepos = vec2(position_in.x * 2.0 - 1.0, position_in.y * 2.0 - 1.0); // Map to [-1, 1] x [-1, 1]
	gl_Position = vec4(usepos.x, usepos.y, 0.0, 1.0);
}
