
// gaussian_splat_frag_shader.glsl
// Copyright Glare Technologies Limited 2026 -
//
// Evaluates the 2D Gaussian for one splat and blends it into the transparent pass.
// See opengl/GaussianSplatRenderer.h for the overall design.

precision highp float; // Override the engine's default "precision mediump float;" for Emscripten - the exponent below is precision-sensitive for large splats.

in vec2 frag_screen_offset_px;
in vec3 frag_conic;
in vec4 frag_colour;

// Note that there's deliberately no order-independent-transparency variant here.  Splat clouds are drawn by
// drawAlphaBlendedObjects(), which always renders to a single colour buffer with ordinary back-to-front alpha blending,
// never through the OIT path - see the material setup in GaussianSplatRenderer::addObject() for why.
layout(location = 0) out vec4 colour_out;


// Non-linear sRGB to linear sRGB.  Copied from frag_utils.glsl, which this shader doesn't include.
// See http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec3 nonLinearSRGBToLinearSRGB(vec3 c)
{
	vec3 c2 = c * c;
	return c * c2 * 0.305306011 + c2 * 0.682171111 + c * 0.012522878;
}


// Inverse of ACESFilm() in frag_utils.glsl, which is f(x) = (x*(a*x + b)) / (x*(c*x + d) + e).
// Solving f(x) = y for x rearranges to the quadratic x^2*(y*c - a) + x*(y*d - b) + y*e = 0.
vec3 inverseACESFilm(vec3 y)
{
	const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;

	vec3 quad_a = y * c - a; // For y in [0, 1] this lies in [-2.51, -0.08], so it's never zero and the quadratic stays well conditioned.
	vec3 quad_b = y * d - b;
	vec3 quad_c = y * e;

	// Of the two roots, this is the one that is >= 0 over the range of interest.
	return (-quad_b - sqrt(max(quad_b*quad_b - 4.0*quad_a*quad_c, vec3(0.0)))) / (2.0 * quad_a);
}


void main()
{
	// Evaluate the 2D Gaussian at this pixel: exponent = -0.5 * offset^T * conic * offset, where conic is the inverse 2D
	// covariance (see gaussian_splat_vert_shader.glsl).
	float power = -0.5 * (frag_conic.x * frag_screen_offset_px.x * frag_screen_offset_px.x
	                     + 2.0 * frag_conic.y * frag_screen_offset_px.x * frag_screen_offset_px.y
	                     + frag_conic.z * frag_screen_offset_px.y * frag_screen_offset_px.y);
	if(power > 0.0)
		discard;

	float alpha = frag_colour.a * exp(power);
	if(alpha < (1.0 / 255.0))
		discard;

	// Clamp the colour to a displayable range.  Evaluating the spherical harmonics can land outside [0, 1] - the DC term
	// alone reaches slightly negative values in real files - and a negative value here is not just wrong but dangerous,
	// since pow() below is undefined for a negative base and returns NaN on most drivers.  This is also the right place
	// for the clamp if view-dependent (shN) terms are ever added, since those are summed per-fragment.
	vec3 base_col = clamp(frag_colour.rgb, 0.0, 1.0);

	// SOG files store display-referred (sRGB) colours: the value in the file is what the splat should look like on
	// screen.  With post-processing enabled, though, the engine blends in linear space and then applies
	// toneMapToNonLinear(), i.e. toNonLinear(ACESFilm(col * 2.0)), so writing the file's colour directly would put it
	// through a transform it has already effectively had applied.  Instead write the value that that transform maps
	// *to* the authored colour, by inverting the whole chain.
	//
	// Note that this is why a plain gamma conversion can't work: the display transform isn't a gamma curve, and raising
	// each channel to a power widens the ratios between channels, which shows up as oversaturation.
#if DO_POST_PROCESSING
	vec3 col = inverseACESFilm(nonLinearSRGBToLinearSRGB(base_col)) * 0.5; // The 0.5 undoes toneMapToNonLinear()'s * 2.0.
#else
	vec3 col = base_col;
#endif

	// drawAlphaBlendedObjects() blends with (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA), so the colour written here is
	// straight, not premultiplied by alpha.
	colour_out = vec4(col, alpha);
}
