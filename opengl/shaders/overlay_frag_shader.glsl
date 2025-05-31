
uniform vec4 diffuse_colour;
uniform mat3 texture_matrix;
uniform sampler2D diffuse_tex;
uniform int overlay_flags;
uniform vec2 clip_min_coords;
uniform vec2 clip_max_coords;

in vec3 pos;
in vec2 texture_coords;

out vec4 colour_out;

#define OVERLAY_HAVE_TEXTURE_FLAG			1
#define OVERLAY_TARGET_IS_NONLINEAR_FLAG	2
#define OVERLAY_SHOW_JUST_TEX_RGB_FLAG		4
#define OVERLAY_SHOW_JUST_TEX_W_FLAG		8


void main()
{
	if(pos.x < clip_min_coords.x || pos.y < clip_min_coords.y || pos.x > clip_max_coords.x || pos.y > clip_max_coords.y)
		discard;

	if((overlay_flags & OVERLAY_HAVE_TEXTURE_FLAG) != 0)
	{
		vec4 texcol = texture(diffuse_tex, (texture_matrix * vec3(texture_coords.x, texture_coords.y, 1.0)).xy);

		if((overlay_flags & OVERLAY_SHOW_JUST_TEX_RGB_FLAG) != 0)
			texcol.w = 1;
		if((overlay_flags & OVERLAY_SHOW_JUST_TEX_W_FLAG) != 0)
			texcol = vec4(texcol.w, texcol.w, texcol.w, 1.f);

		colour_out = vec4(texcol.x, texcol.y, texcol.z, texcol.w) * diffuse_colour;
	}
	else
		colour_out = diffuse_colour;

	// For the substrata minimap, webgl doesn't allow a non-linear sRGB framebuffer, so we use a linear framebuffer.  So we have support for not doing toNonLinear().
	if((overlay_flags & OVERLAY_TARGET_IS_NONLINEAR_FLAG) != 0) // If overlay target is non-linear:
		colour_out.xyz = toNonLinear(colour_out.xyz);

	colour_out = vec4(colour_out.xyz, colour_out.w);
}
