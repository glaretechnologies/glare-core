
uniform sampler2D albedo_texture; // main colour buffer
uniform sampler2D transparent_accum_texture;
uniform sampler2D total_transmittance_texture;


in vec2 pos; // [0, 1] x [0, 1]

out vec4 colour_out;


void main()
{
	ivec2 tex_res = textureSize(albedo_texture, /*mip level*/0);

	ivec2 px_coords = ivec2(int(float(tex_res.x) * pos.x), int(float(tex_res.y) * pos.y));

	vec4 col = texelFetch(albedo_texture, px_coords, /*mip level=*/0);

#if ORDER_INDEPENDENT_TRANSPARENCY
	// Add order-independent transparency terms:
	// Get transparent_accum_texture colour
	vec4 accum_col = texelFetch(transparent_accum_texture, px_coords, /*mip level=*/0);

	// Get total transmittance colour
	vec4 total_transmittance = texelFetch(total_transmittance_texture, px_coords, /*mip level=*/0);

	vec4 T_tot = clamp(total_transmittance, 0.00001, 0.9999);

	col *= total_transmittance;

	vec4 L = accum_col * ((T_tot - vec4(1.0)) / log(T_tot));
	col += L;
#endif

	colour_out = col;
}
