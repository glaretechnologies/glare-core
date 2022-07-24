

// The source texture has MSAA iff we are downsizing directly from the main buffer, and the main buffer has MSAA.

#if DOWNSIZE_FROM_MAIN_BUF && (MAIN_BUFFER_MSAA_SAMPLES > 1)
uniform sampler2DMS albedo_texture; // source texture
uniform sampler2DMS transparent_accum_texture;
uniform sampler2DMS av_transmittance_texture;
#else
uniform sampler2D albedo_texture; // source texture
uniform sampler2D transparent_accum_texture;
uniform sampler2D av_transmittance_texture;
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


float resolveAvTransTextureSample(ivec2 px_coords)
{
#if DOWNSIZE_FROM_MAIN_BUF && (MAIN_BUFFER_MSAA_SAMPLES > 1)
	float col = 0.f;
	for(int i=0; i<MAIN_BUFFER_MSAA_SAMPLES; ++i)
		col += texelFetch(av_transmittance_texture, px_coords, i).x;
	col *= (1.f / MAIN_BUFFER_MSAA_SAMPLES);
	return col;
#else
	return texelFetch(av_transmittance_texture, px_coords, /*mip level=*/0).x;
#endif
}


void main()
{
#if DOWNSIZE_FROM_MAIN_BUF && (MAIN_BUFFER_MSAA_SAMPLES > 1)
	ivec2 tex_res = textureSize(albedo_texture);
#else
	ivec2 tex_res = textureSize(albedo_texture, /*mip level*/0);
#endif

	ivec2 px_coords = ivec2(int(tex_res.x * pos.x - 0.5), int(tex_res.y * pos.y - 0.5));

	vec4 texcol_0 =	resolveTextureSample(px_coords + ivec2(0, 0));
	vec4 texcol_1 =	resolveTextureSample(px_coords + ivec2(1, 0));
	vec4 texcol_2 =	resolveTextureSample(px_coords + ivec2(1, 1));
	vec4 texcol_3 =	resolveTextureSample(px_coords + ivec2(0, 1));


#if DOWNSIZE_FROM_MAIN_BUF
	// Add order-independent transparency terms.
	vec4 accum_texcol_0 =	resolveTransAccumTextureSample(px_coords + ivec2(0, 0));
	vec4 accum_texcol_1 =	resolveTransAccumTextureSample(px_coords + ivec2(1, 0));
	vec4 accum_texcol_2 =	resolveTransAccumTextureSample(px_coords + ivec2(1, 1));
	vec4 accum_texcol_3 =	resolveTransAccumTextureSample(px_coords + ivec2(0, 1));

	texcol_0 += accum_texcol_0 * resolveAvTransTextureSample(px_coords + ivec2(0, 0));
	texcol_1 += accum_texcol_1 * resolveAvTransTextureSample(px_coords + ivec2(1, 0));
	texcol_2 += accum_texcol_2 * resolveAvTransTextureSample(px_coords + ivec2(1, 1));
	texcol_3 += accum_texcol_3 * resolveAvTransTextureSample(px_coords + ivec2(0, 1));
#endif


	vec4 texcol = (texcol_0 + texcol_1 + texcol_2 + texcol_3) * (1.0 / 4.0);

	float av = (texcol.x + texcol.y + texcol.z) * (1.0 / 3.0);

#if DOWNSIZE_FROM_MAIN_BUF
	// Check for NaNs, also only use pixels that are suffuciently bright, to avoid blooming the whole image.
	if(av < 1.0 || isnan(av))
		texcol = vec4(0.0);
#endif

	colour_out = vec4(texcol.x, texcol.y, texcol.z, 1);
}
