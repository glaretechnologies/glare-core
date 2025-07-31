
#if MAIN_BUFFER_MSAA_SAMPLES > 1
uniform sampler2DMS albedo_texture; // main colour buffer
//uniform sampler2DMS transparent_accum_texture;
//uniform sampler2DMS total_transmittance_texture;
#else
uniform sampler2D albedo_texture; // main colour buffer
//uniform sampler2D transparent_accum_texture;
//uniform sampler2D total_transmittance_texture;
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
uniform float exposure_factor;
uniform float saturation_multiplier;

in vec2 pos; // [0, 1] x [0, 1]

out vec4 colour_out;


// Based on https://mattlockyer.github.io/iat455/documents/rgb-hsv.pdf
// Could also use https://gist.github.com/983/e170a24ae8eba2cd174f which seems slightly faster.
vec3 rgb2hsv(vec3 c)
{
	float m_max = max(c.r, max(c.g, c.b));
	float m_min = min(c.r, min(c.g, c.b));
	float delta = m_max - m_min;

	float H;
	if(delta == 0)
		H = 0.0;
	else if(m_max == c.r)
		H = (c.g - c.b) / delta;
	else if(m_max == c.g)
		H = (c.b - c.r) / delta + 2.0;
	else
		H = (c.r - c.g) / delta + 4.0;

	float V = m_max;
	float S = 0.0;
	if(V != 0.0)
		S = delta / V;
	
	return vec3(H, S, V);
}

vec3 hsv2rgb(vec3 c)
{
	float H = fract(c.r * (1.0/6.0)) * 6.0; // H = c.r mod 6
	float S = c.g;
	float V = c.b;
	float alpha = V * (1 - S);
	float beta = V * (1 - fract(H) * S);
	float gamma = V * (1 - (1 - fract(H)) * S);
	if(H < 1.0)
		return vec3(V, gamma, alpha);
	else if(H < 2.0)
		return vec3(beta, V, alpha);
	else if(H < 3.0)
		return vec3(alpha, V, gamma);
	else if(H < 4.0)
		return vec3(alpha, beta, V);
	else if(H < 5.0)
		return vec3(gamma, alpha, V);
	else
		return vec3(V, alpha, beta);
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

#if 0 // ORDER_INDEPENDENT_TRANSPARENCY
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

		// Get total transmittance colour
	#if MAIN_BUFFER_MSAA_SAMPLES > 1
		vec4 total_transmittance = 0.f;
		for(int i=0; i<MAIN_BUFFER_MSAA_SAMPLES; ++i)
			total_transmittance += texelFetch(total_transmittance_texture, px_coords, i);
		total_transmittance *= (1.f / MAIN_BUFFER_MSAA_SAMPLES);
	#else
		vec4 total_transmittance = texelFetch(total_transmittance_texture, px_coords, /*mip level=*/0);
	#endif

		vec4 T_tot = clamp(total_transmittance, 0.00001, 0.9999);

		col *= total_transmittance;

		vec4 L = accum_col * ((T_tot - vec4(1.0)) / log(T_tot));
		col += L;
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

		col.xyz += (blur_col * bloom_strength).xyz; // Leave alpha as-is.
	}
	
	float alpha = col.w;

	vec3 col_rgb = col.xyz * exposure_factor;
	vec3 col_hsv = rgb2hsv(col_rgb);
	col_hsv.y = min(1.0, col_hsv.y * saturation_multiplier);
	col_rgb = hsv2rgb(col_hsv);

	colour_out = vec4(toneMapToNonLinear(col_rgb), alpha);
	//colour_out = vec4(toNonLinear(3.0 * col.xyz), alpha); // linear tonemapping
}
