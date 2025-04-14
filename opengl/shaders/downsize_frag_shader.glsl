

// The source texture has MSAA iff we are downsizing directly from the main buffer, and the main buffer has MSAA.

#if DOWNSIZE_FROM_MAIN_BUF && (MAIN_BUFFER_MSAA_SAMPLES > 1)
uniform sampler2DMS albedo_texture; // source texture
uniform sampler2DMS transparent_accum_texture;
uniform sampler2DMS total_transmittance_texture;
#else
uniform sampler2D albedo_texture; // source texture
uniform sampler2D transparent_accum_texture;
uniform sampler2D total_transmittance_texture;
#endif

in vec2 pos; // [0, 1] x [0, 1]

out vec4 colour_out;


vec4 resolveTextureSample(ivec2 px_coords)
{
#if DOWNSIZE_FROM_MAIN_BUF && (MAIN_BUFFER_MSAA_SAMPLES > 1)
	vec4 col = vec4(0.f);
	for(int i=0; i<MAIN_BUFFER_MSAA_SAMPLES; ++i)
		col += texelFetch(albedo_texture, px_coords, i);
	col *= (1.f / MAIN_BUFFER_MSAA_SAMPLES);
	return col;
#else
	return texelFetch(albedo_texture, px_coords, /*mip level=*/0);
#endif
}


vec4 resolveTransAccumTextureSample(ivec2 px_coords)
{
#if DOWNSIZE_FROM_MAIN_BUF && (MAIN_BUFFER_MSAA_SAMPLES > 1)
	vec4 col = vec4(0.f);
	for(int i=0; i<MAIN_BUFFER_MSAA_SAMPLES; ++i)
		col += texelFetch(transparent_accum_texture, px_coords, i);
	col *= (1.f / MAIN_BUFFER_MSAA_SAMPLES);
	return col;
#else
	return texelFetch(transparent_accum_texture, px_coords, /*mip level=*/0);
#endif
}


vec4 resolveTotalTransTextureSample(ivec2 px_coords)
{
#if DOWNSIZE_FROM_MAIN_BUF && (MAIN_BUFFER_MSAA_SAMPLES > 1)
	vec4 col = vec4(0.f);
	for(int i=0; i<MAIN_BUFFER_MSAA_SAMPLES; ++i)
		col += texelFetch(total_transmittance_texture, px_coords, i);
	col *= (1.f / MAIN_BUFFER_MSAA_SAMPLES);
	return col;
#else
	return texelFetch(total_transmittance_texture, px_coords, /*mip level=*/0);
#endif
}




void main()
{
#if DOWNSIZE_FROM_MAIN_BUF && (MAIN_BUFFER_MSAA_SAMPLES > 1)
	ivec2 tex_res = textureSize(albedo_texture);
#else
	ivec2 tex_res = textureSize(albedo_texture, /*mip level*/0);
#endif

	ivec2 px_coords = ivec2(int(float(tex_res.x) * pos.x - 0.5), int(float(tex_res.y) * pos.y - 0.5));

	vec4 texcol_0 =	resolveTextureSample(px_coords + ivec2(0, 0));
	vec4 texcol_1 =	resolveTextureSample(px_coords + ivec2(1, 0));
	vec4 texcol_2 =	resolveTextureSample(px_coords + ivec2(1, 1));
	vec4 texcol_3 =	resolveTextureSample(px_coords + ivec2(0, 1));

	vec4 col;

#if DOWNSIZE_FROM_MAIN_BUF && ORDER_INDEPENDENT_TRANSPARENCY
	// Add order-independent transparency terms.
	vec4 accum_texcol_0 =	resolveTransAccumTextureSample(px_coords + ivec2(0, 0));
	vec4 accum_texcol_1 =	resolveTransAccumTextureSample(px_coords + ivec2(1, 0));
	vec4 accum_texcol_2 =	resolveTransAccumTextureSample(px_coords + ivec2(1, 1));
	vec4 accum_texcol_3 =	resolveTransAccumTextureSample(px_coords + ivec2(0, 1));

	vec4 total_texcol_0 =	resolveTotalTransTextureSample(px_coords + ivec2(0, 0));
	vec4 total_texcol_1 =	resolveTotalTransTextureSample(px_coords + ivec2(1, 0));
	vec4 total_texcol_2 =	resolveTotalTransTextureSample(px_coords + ivec2(1, 1));
	vec4 total_texcol_3 =	resolveTotalTransTextureSample(px_coords + ivec2(0, 1));

	col = (texcol_0 + texcol_1 + texcol_2 + texcol_3) * (1.0 / 4.0);
	vec4 total_transmittance = (total_texcol_0 + total_texcol_1 + total_texcol_2 + total_texcol_3) * (1.0 / 4.0);
	vec4 accum_col = (accum_texcol_0 + accum_texcol_1 + accum_texcol_2 + accum_texcol_3) * (1.0 / 4.0);

	vec4 T_tot = clamp(total_transmittance, 0.00001, 0.9999);

	col *= total_transmittance;

	vec4 L = accum_col * ((T_tot - vec4(1.0)) / log(T_tot));
	col += L;

#else
	col = (texcol_0 + texcol_1 + texcol_2 + texcol_3) * (1.0 / 4.0);
#endif

	float av = (col.x + col.y + col.z) * (1.0 / 3.0);

#if DOWNSIZE_FROM_MAIN_BUF
	// Check for NaNs, also only use pixels that are sufficiently bright, to avoid blooming the whole image.
	if(av < 1.0 || isnan(av))
		col = vec4(0.0);
#endif

	colour_out = vec4(col.xyz, 1.0);
}
