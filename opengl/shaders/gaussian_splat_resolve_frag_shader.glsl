
// gaussian_splat_resolve_frag_shader.glsl
// Copyright Glare Technologies Limited 2026 -
//
// Resolves the splat accumulation buffer that gaussian_splat_frag_shader.glsl blends into, and composites the result
// onto the main colour buffer.  See OpenGLEngine::drawSplatClouds() for how the pass is set up.
//
// The splats were blended in display-referred (non-linear sRGB) space, because that is the space 3DGS fits them in -
// see the comment in gaussian_splat_frag_shader.glsl.  That blend is finished by the time this runs, so this is the one
// place where the engine's display transform can be inverted without disturbing it.

precision highp float; // Override the engine's default "precision mediump float;" for Emscripten - dividing out a small alpha and inverting the tone map both want the range.
precision highp sampler2D; // Likewise: sampler2D defaults to lowp in GLSL ES, which would quantise the accumulated colour and coverage read below.

uniform sampler2D albedo_texture; // The splat accumulation buffer: (sum of c_i * a_i * T_i, coverage), non-linear sRGB, premultiplied.

out vec4 colour_out;


#if DO_POST_PROCESSING


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

#endif // DO_POST_PROCESSING


void main()
{
	// The accumulation buffer has the same dimensions as the buffer being drawn to, and this quad covers the whole
	// viewport, so the fragment's own coordinates index it directly.  Any MSAA samples were already resolved into it by
	// the blit, which averages the premultiplied colour and the coverage together - the right order, since dividing
	// per-sample and then averaging would weight sparsely covered samples equally with fully covered ones.
	vec4 accum = texelFetch(albedo_texture, ivec2(gl_FragCoord.xy), /*mip level=*/0);

	float coverage = accum.a; // = 1 - product of (1 - a_i), i.e. how much of this pixel the splat stack covers.
	if(coverage <= 0.0)
		discard; // No splat reached this pixel, so leave the background alone.  Also avoids the 0/0 below.

	// Divide out the alpha to recover the straight (non-premultiplied) colour of the splat stack: the colour the
	// capture says this pixel should be where the splats cover it.  The clamp only guards against half-float rounding
	// pushing a convex combination of values in [0, 1] just outside it, which would take inverseACESFilm() outside the
	// range it is conditioned for.
	vec3 col = clamp(accum.rgb / coverage, 0.0, 1.0);

#if DO_POST_PROCESSING
	// col is what the splats should look like on screen, but the engine will apply toneMapToNonLinear() - that is,
	// toNonLinear(ACESFilm(col * 2.0)) - to this buffer later on.  So hand it the linear value that that transform maps
	// *to* col, by inverting the whole chain.  Note that a plain gamma conversion can't do this job: the display
	// transform isn't a gamma curve, and raising each channel to a power widens the ratios between channels, which
	// shows up as oversaturation.
	col = inverseACESFilm(fastApproxNonLinearSRGBToLinearSRGB(col)) * (1.0 / PRE_TONE_MAP_SCALE_FACTOR);
#endif

	// Premultiplied again for the composite: drawSplatClouds() blends this with (GL_ONE, GL_ONE_MINUS_SRC_ALPHA), so
	// the splat stack goes over the background with its accumulated coverage.
	colour_out = vec4(col * coverage, coverage);
}
