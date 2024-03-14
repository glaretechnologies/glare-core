
#if MAIN_BUFFER_MSAA_SAMPLES > 1
uniform sampler2DMS albedo_texture; // main colour buffer
uniform sampler2DMS transparent_accum_texture;
uniform sampler2DMS av_transmittance_texture;
#else
uniform sampler2D albedo_texture; // main colour buffer
uniform sampler2D transparent_accum_texture;
uniform sampler2D av_transmittance_texture;
#endif

uniform sampler2D blur_tex_0;
uniform sampler2D blur_tex_1;
uniform sampler2D blur_tex_2;
uniform sampler2D blur_tex_3;
uniform sampler2D blur_tex_4;
uniform sampler2D blur_tex_5;
uniform sampler2D blur_tex_6;
uniform sampler2D blur_tex_7;


uniform float bloom_strength;

in vec2 pos; // [0, 1] x [0, 1]

out vec4 colour_out;


vec3 toNonLinear(vec3 x)
{
	// Approximation to pow(x, 0.4545).  Max error of ~0.004 over [0, 1].
	return 0.124445006f*x*x + -0.35056138f*x + 1.2311935*sqrt(x);
}



void main()
{
#if MAIN_BUFFER_MSAA_SAMPLES > 1
	ivec2 tex_res = textureSize(albedo_texture);
#else
	ivec2 tex_res = textureSize(albedo_texture, /*mip level*/0);
#endif

	ivec2 px_coords = ivec2(int(float(tex_res.x) * pos.x), int(float(tex_res.y) * pos.y));

#if MAIN_BUFFER_MSAA_SAMPLES > 1
	vec4 col = vec4(0.f);
	for(int i=0; i<MAIN_BUFFER_MSAA_SAMPLES; ++i)
		col += texelFetch(albedo_texture, px_coords, i);
	col *= (1.f / MAIN_BUFFER_MSAA_SAMPLES);
#else
	vec4 col = texelFetch(albedo_texture, px_coords, /*mip level=*/0);
#endif

#if ORDER_INDEPENDENT_TRANSPARENCY
		// Add order-independent transparency terms:
		// Get transparent_accum_texture colour
	#if MAIN_BUFFER_MSAA_SAMPLES > 1
		vec4 accum_col = vec4(0.f);
		for(int i=0; i<MAIN_BUFFER_MSAA_SAMPLES; ++i)
			accum_col += texelFetch(transparent_accum_texture, px_coords, i);
		accum_col *= (1.f / MAIN_BUFFER_MSAA_SAMPLES);
	#else
		vec4 accum_col = texelFetch(transparent_accum_texture, px_coords, /*mip level=*/0);
	#endif

		// Get av transmittance colour
	#if MAIN_BUFFER_MSAA_SAMPLES > 1
		float av_transmittance = 0.f;
		for(int i=0; i<MAIN_BUFFER_MSAA_SAMPLES; ++i)
			av_transmittance += texelFetch(av_transmittance_texture, px_coords, i).x;
		av_transmittance *= (1.f / MAIN_BUFFER_MSAA_SAMPLES);
	#else
		float av_transmittance = texelFetch(av_transmittance_texture, px_coords, /*mip level=*/0).x;
	#endif
		col += accum_col * av_transmittance;
#endif

	if(bloom_strength > 0.0)
	{
		vec4 blur_col_0 = texture(blur_tex_0, pos);
		vec4 blur_col_1 = texture(blur_tex_1, pos);
		vec4 blur_col_2 = texture(blur_tex_2, pos);
		vec4 blur_col_3 = texture(blur_tex_3, pos);
		vec4 blur_col_4 = texture(blur_tex_4, pos);
		vec4 blur_col_5 = texture(blur_tex_5, pos);
		vec4 blur_col_6 = texture(blur_tex_6, pos);
		vec4 blur_col_7 = texture(blur_tex_7, pos);

		vec4 blur_col =
			blur_col_0 * (0.5 / 8.0) +
			blur_col_1 * (0.5 / 8.0) +
			blur_col_2 * (0.5 / 8.0) +
			blur_col_3 * (0.5 / 8.0) +
			blur_col_4 * (1.0 / 8.0) +
			blur_col_5 * (1.0 / 8.0) +
			blur_col_6 * (1.0 / 8.0) +
			blur_col_7 * (3.0 / 8.0);

		col += blur_col * bloom_strength;
	}
	
	float alpha = 1.f;
	colour_out = vec4(toNonLinear(col.xyz), alpha);
}
