
#if READ_FROM_MSAA_TEX
uniform sampler2DMS albedo_texture;
#else
uniform sampler2D albedo_texture;
#endif

in vec2 pos; // [0, 1] x [0, 1]

out vec4 colour_out;

void main()
{
#if READ_FROM_MSAA_TEX
	ivec2 tex_res = textureSize(albedo_texture);

	ivec2 px_coords = ivec2(int(tex_res.x * pos.x - 0.5), int(tex_res.y * pos.y - 0.5));

	vec4 texcol_0 =	(texelFetch(albedo_texture, px_coords, 0) +
				texelFetch(albedo_texture, px_coords, 1) +
				texelFetch(albedo_texture, px_coords, 2) +
				texelFetch(albedo_texture, px_coords, 3)) * 0.25;

	px_coords.x += 1;
	vec4 texcol_1 =	(texelFetch(albedo_texture, px_coords, 0) +
				texelFetch(albedo_texture, px_coords, 1) +
				texelFetch(albedo_texture, px_coords, 2) +
				texelFetch(albedo_texture, px_coords, 3)) * 0.25;

	px_coords.x -= 1;
	px_coords.y += 1;
	vec4 texcol_2 =	(texelFetch(albedo_texture, px_coords, 0) +
		texelFetch(albedo_texture, px_coords, 1) +
		texelFetch(albedo_texture, px_coords, 2) +
		texelFetch(albedo_texture, px_coords, 3)) * 0.25;

	px_coords.x += 1;
	vec4 texcol_3 =	(texelFetch(albedo_texture, px_coords, 0) +
		texelFetch(albedo_texture, px_coords, 1) +
		texelFetch(albedo_texture, px_coords, 2) +
		texelFetch(albedo_texture, px_coords, 3)) * 0.25;

	vec4 texcol = (texcol_0 + texcol_1 + texcol_2 + texcol_3) * (1.0 / 4.0);

	float av = (texcol.x + texcol.y + texcol.z) * (1.0 / 3.0);
	if(av < 1.0 || isnan(av))
		texcol = vec4(0.0);
#else
	//vec4 texcol = texture(albedo_texture, pos);

	ivec2 tex_res = textureSize(albedo_texture, 0); // Resolution, in texels, of source texture we are reading from.

	ivec2 px_coords = ivec2(int(tex_res.x * pos.x - 0.5), int(tex_res.y * pos.y - 0.5));

	vec4 texcol_0 =	texelFetch(albedo_texture, px_coords + ivec2(0, 0), 0); // + (0, 0)
	vec4 texcol_1 =	texelFetch(albedo_texture, px_coords + ivec2(1, 0), 0); // + (1, 0)
	vec4 texcol_2 =	texelFetch(albedo_texture, px_coords + ivec2(1, 1), 0); // + (1, 1)
	vec4 texcol_3 =	texelFetch(albedo_texture, px_coords + ivec2(0, 1), 0); // + (0, 1)
	// 
	//float half_px_offset_x = 0.5f / float(tex_res.x); // Half-texel offset in source texture
	//float half_px_offset_y = 0.5f / float(tex_res.y);

	////vec2 texcoords = pos;
	//vec4 texcol_0 =	texture(albedo_texture, pos + vec2(-half_px_offset_x, -half_px_offset_y)); // + (0, 0)
	//vec4 texcol_1 =	texture(albedo_texture, pos + vec2( half_px_offset_x, -half_px_offset_y)); // + (1, 0)
	//vec4 texcol_2 =	texture(albedo_texture, pos + vec2( half_px_offset_x,  half_px_offset_y)); // + (1, 1)
	//vec4 texcol_3 =	texture(albedo_texture, pos + vec2(-half_px_offset_x,  half_px_offset_y)); // + (0, 1)

	/*texcoords.x += half_px_offset_x;
	vec4 texcol_1 =	texture(albedo_texture, texcoords);

	texcoords.x -= half_px_offset_x;
	texcoords.y += half_px_offset_y;
	vec4 texcol_2 =	texture(albedo_texture, texcoords);

	texcoords.x += half_px_offset_x;
	vec4 texcol_3 =	texture(albedo_texture, texcoords);*/

	vec4 texcol = (texcol_0 + texcol_1 + texcol_2 + texcol_3) * (1.0 / 4.0);
#endif

	colour_out = vec4(texcol.x, texcol.y, texcol.z, 1);
}

