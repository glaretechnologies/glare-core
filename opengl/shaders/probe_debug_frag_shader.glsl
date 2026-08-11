
// See probe_debug_vert_shader.glsl.
//
// Drawn into the main render buffer along with the scene rather than as an overlay, so that the normal exposure
// and tone mapping apply - the raw irradiance values are far too small to read otherwise.

in vec3 normal_ws;

uniform sampler2D probe_irradiance_tex;
uniform int probe_index;

layout(location = 0) out vec4 colour_out;
#if NORMAL_TEXTURE_IS_UINT
layout(location = 1) out uvec4 normal_out;
#else
layout(location = 1) out vec3 normal_out;
#endif


void main()
{
	vec3 unit_normal = normalize(normal_ws);

	// Show the probe as a white lambertian sphere: irradiance / pi is the radiance such a surface would emit.
	vec3 radiance = sampleProbeIrradiance(probe_index, unit_normal, probe_irradiance_tex) * (1.0 / PI);

	colour_out = vec4(radiance, 1.0);

	normal_out = snorm12x2_to_unorm8x3(float32x3_to_oct(unit_normal));
}
