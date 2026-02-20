
uniform sampler2D albedo_texture; // source texture

uniform int dest_xres;
uniform int dest_yres;

in vec2 pos; // [0, 1] x [0, 1]

out vec4 colour_out;


vec4 resolveTextureSample(ivec2 px_coords)
{
	return texelFetch(albedo_texture, px_coords, /*mip level=*/0);
}


/*

Note that dest texture xres = ceil(src texture xres / 2)

x = texture sample coordinate
 src texture (xres = 5)

0     0.5    1       1.5      2     2.5      3       3.5     4     4.5      5
-----------------------------------------------------------------------------
|            |                |              |               |              |
|     x      |       x        |      x       |        x      |      x       |


dest texture (xres = ceil(src_xres / 2) = 3)


0           0.5               1             1.5             2.0             2.5            3
--------------------------------------------------------------------------------------------
|                             |                              |                             |
|            x                |              x               |               x             |
 
                                             ^
                                          example

So to get src integer texel coord for dest coord:
Start with dest coord in [0, 1].  Multiply by dest tex width to get in [0, dest_xres] = [0, 3]
multiply by 2, which will result in an integer value, e.g. 1.5 * 2 = 3.  Subtract 0.5 then truncate to integer, giving floor(3 - 0.5)
= floor(2.5) = 2
Texel integer coords 2 and 3 are the ones we want.
*/


void main()
{
	// Get res of source texture
	ivec2 src_res = textureSize(albedo_texture, /*mip level*/0);
	ivec2 max_src_coords = src_res - ivec2(1,1);

	ivec2 px_coords = ivec2(int(pos.x * float(dest_xres) * 2.0 - 0.5), int(pos.y * float(dest_yres) * 2.0 - 0.5));

	vec4 texcol_0 =	resolveTextureSample(min(px_coords + ivec2(0, 0), max_src_coords));
	vec4 texcol_1 =	resolveTextureSample(min(px_coords + ivec2(1, 0), max_src_coords));
	vec4 texcol_2 =	resolveTextureSample(min(px_coords + ivec2(1, 1), max_src_coords));
	vec4 texcol_3 =	resolveTextureSample(min(px_coords + ivec2(0, 1), max_src_coords));

	vec4 col = (texcol_0 + texcol_1 + texcol_2 + texcol_3) * (1.0 / 4.0);

	float av = (col.x + col.y + col.z) * (1.0 / 3.0);

#if DOWNSIZE_FROM_MAIN_BUF
	// Check for NaNs, also only use pixels that are sufficiently bright, to avoid blooming the whole image.
	if(av < 1.0 || isnan(av))
		col = vec4(0.0);
#endif

	colour_out = vec4(col.xyz, 1.0);
}
