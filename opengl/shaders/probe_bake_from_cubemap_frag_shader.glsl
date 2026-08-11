
// Bakes one irradiance probe tile by resampling a cube map.  Used to fill the global sky probe (probe 0) from
// cosine_env_tex, which already holds cosine-convolved sky irradiance, so this is a pure change of storage
// layout from cube map to octahedral tile.
//
// The whole tile is rendered, border ring included: border texels have octahedral coordinates outside
// [-1, 1]^2, which wrapOctCoord() folds back onto the octahedron.  That gives them the direction of the
// interior texel they wrap around to, so no separate border copy pass is needed.
//
// Sampling the cube map on the GPU rather than resampling on the CPU keeps the cube face conventions
// identical to what the shaders were doing before, so the result is directly comparable.

uniform samplerCube source_cube_tex;
uniform vec2 probe_tile_origin; // Atlas texel coordinates of the lower left corner of the tile being written.

out vec4 colour_out;


void main()
{
	vec2 tile_texel = gl_FragCoord.xy - probe_tile_origin; // In [0, PROBE_TILE_RES]

	// Map to octahedral coordinates.  Interior texels land in [-1, 1]^2, border texels just outside it.
	vec2 oct_uv = (tile_texel - vec2(float(PROBE_TILE_BORDER))) * (1.0 / float(PROBE_TILE_INTERIOR_RES));
	vec2 p = wrapOctCoord(oct_uv * 2.0 - vec2(1.0));

	vec3 dir = oct_to_float32x3(p);

	colour_out = vec4(texture(source_cube_tex, dir).xyz, 1.0);
}
