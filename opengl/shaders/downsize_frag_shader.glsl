

// The source texture has MSAA iff we are downsizing directly from the main buffer, and the main buffer has MSAA.

#if DOWNSIZE_FROM_MAIN_BUF && (MAIN_BUFFER_MSAA_SAMPLES > 1)
uniform sampler2DMS albedo_texture; // source texture
#else
uniform sampler2D albedo_texture; // source texture
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

	vec4 texcol = (texcol_0 + texcol_1 + texcol_2 + texcol_3) * (1.0 / 4.0);

	float av = (texcol.x + texcol.y + texcol.z) * (1.0 / 3.0);

#if DOWNSIZE_FROM_MAIN_BUF
	// Check for NaNs, also only use pixels that are suffuciently bright, to avoid blooming the whole image.
	if(av < 1.0 || isnan(av))
		texcol = vec4(0.0);
#endif

	colour_out = vec4(texcol.x, texcol.y, texcol.z, 1);
}
