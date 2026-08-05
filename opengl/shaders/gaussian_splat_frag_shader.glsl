
// gaussian_splat_frag_shader.glsl
// Copyright Glare Technologies Limited 2026 -
//
// Evaluates the 2D Gaussian for one splat and blends it into the splat accumulation buffer, which
// gaussian_splat_resolve_frag_shader.glsl then composites onto the main colour buffer.
// See opengl/GaussianSplatRenderer.h for the overall design.

precision highp float; // Override the engine's default "precision mediump float;" for Emscripten - the exponent below is precision-sensitive for large splats.

in vec2 frag_screen_offset_px;
in vec3 frag_conic;
in vec4 frag_colour;

// Note that there's deliberately no order-independent-transparency variant here.  Splat clouds are drawn by
// drawSplatClouds(), which always renders to a single colour buffer with ordinary back-to-front alpha blending, never
// through the OIT path - see the material setup in GaussianSplatRenderer::addObject() for why.
layout(location = 0) out vec4 colour_out;


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
	// alone reaches slightly negative values in real files - and out-of-range values here are not just wrong but
	// dangerous, since the blend below would carry them into the accumulation buffer, where the resolve pass's inverse
	// tone map is only conditioned for [0, 1].  This is also the right place for the clamp if view-dependent (shN) terms
	// are ever added, since those are summed per-fragment.
	vec3 base_col = clamp(frag_colour.rgb, 0.0, 1.0);

	// Write the splat's colour in the display-referred (non-linear sRGB) space the file stores it in, and let the blend
	// happen in that space.  That is not a rendering choice we're free to make: standard 3DGS training loads the
	// photographs as 8-bit sRGB, divides by 255 and optimises against them directly, with no sRGB->linear conversion
	// anywhere, so the fitted colours and opacities are exactly the values whose alpha composite *in gamma-encoded
	// sRGB* reproduces the captured images.  The compositing space is part of the model.  Blending the same splats in
	// linear space gives a different image wherever a pixel mixes splats of differing brightness - depth edges and
	// semi-transparent regions, which is precisely where a capture is most fragile.  It is physically wrong, and it is
	// a known wart of 3DGS, but reproducing the reference means reproducing the wart.
	//
	// So the engine's display transform can't be inverted here, per splat: the blend has to see the authored sRGB
	// values, and inverting first would blend in a space that is neither sRGB nor anything the model saw.  Since that
	// mapping is convex, the resulting average is biased even where it doesn't blow up.  drawSplatClouds() therefore
	// blends this pass into a buffer of its own and inverts the transform once, after the blend - see
	// gaussian_splat_resolve_frag_shader.glsl.
	//
	// Premultiplied by alpha: drawSplatClouds() blends with (GL_ONE, GL_ONE_MINUS_SRC_ALPHA), so the accumulation
	// buffer ends up holding (sum of c_i * a_i * T_i, coverage), which is what the resolve pass wants.
	colour_out = vec4(base_col * alpha, alpha);
}
