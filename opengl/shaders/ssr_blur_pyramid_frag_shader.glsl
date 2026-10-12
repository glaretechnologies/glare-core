// Blurs the screen-space reflection trace output (see compute_ssao_frag_shader.glsl), using a pyramid of half-resolution levels, so that each trace
// hit is spread into the image of its reflected GGX lobe: 1 / (pi b^2 (1 + r^2 / b^2)^2), where b is the lobe width the trace computed for that hit.
//
// That function is a mixture of Gaussians of all scales, whose precision 1 / sigma^2 is exponentially distributed with rate b^2 / 2.  So the fraction of
// it made of Gaussians wider than s is Q(s) = 1 - exp(-b^2 / (2 s^2)).  Each level k >= 1 blurs by about a Gaussian, and stands for the scales from
// LEVEL_SCALE * 2^k up to the next level's.  Level 0 is unblurred, and stands for the scales below level 1's.  So each hit's content is split across
// the levels in proportion to Q, by its own lobe width, at level 0.  Each level k >= 1 holds a stream for each level from k to NUM_SSR_BLUR_LEVELS,
// containing the content that belongs at that level.  The down passes (SSR_BLUR_DOWN_PASS 1) filter all the streams of one level down to the next, apart
// from that level's own, which stays there.  The up passes then upsample each level with a cubic B-spline filter and add it to the next finer level's own
// stream.  As the split is made by each hit's own lobe width, and the streams are only ever summed, content is spread by its own lobe width, however it
// is mixed with other content on the way.
//
// So content is spread by its own lobe width, rather than by the lobe width of the texel receiving it: a reflected object is blurred according to its own
// trace distance, not the trace distances of the neighbouring texels, which may have hit something behind it, or nothing:
// 
//                   ----
//             -----/    \---  ^
//        ----/              \/--\--
//   ----/                   /    \ \----
//  |----|----|---|----|----|---|---|----|
//                     a        b
// 
// Consider pixel b.  If it has the minimum blur radius (e.g. is a very smooth surface), It should still accumulate the blur spread from pixel a.
// The blur spread from pixel a should also be weighted according to the support area of pixel a, not the support area of pixel b.
// 
//
// The streams hold rgb = hit radiance, a = hit coverage, where each hit has coverage 1, and misses contribute nothing.  The sum at level 0 is the blurred
// (hit radiance * coverage, coverage) that phong_frag_shader.glsl reads.
//
// The filters don't test whether taps are on the same surface as the texel being written.  A coarse texel can cover several surfaces, so there is no
// single surface to test against, and choosing one per texel makes the result depend on the texel grid, so it flickers as the camera moves.  Instead,
// the coarse levels are split into NUM_SSR_DIR_BINS direction bins, by the direction of each level 0 texel's mirror reflection: leftwards and rightwards
// in camera space.  Each level 0 texel's content goes into the bins in proportion to its membership of them (see dirBinMembership()), which depends only
// on the texel itself, and each bin is filtered through the pyramid separately.  The final up pass (SSR_BLUR_FINAL_UP_PASS 1) then takes from each bin
// in proportion to the receiving texel's own membership.  So reflections only spread between texels reflecting in similar directions, such as across
// a silhouette where the surfaces either side reflect things on opposite sides of the view, and as the split doesn't depend on the texel grid, it
// doesn't flicker.  Content still spreads between surfaces reflecting in similar directions, and narrow lobes are mostly in the finer levels, so bleed
// little anyway.  The main pass also keeps surfaces apart at the scale of a half-res texel: it only uses SSR texels whose prepass depth and normal match
// the fragment's.
//
// Only texels where something was drawn hold content.  Each coarse texel has, for its bin, the membership-weighted fraction of its down filter footprint
// where something was drawn, its area, and the up passes divide by it, giving content per unit drawn area.  So reflections don't fade out next to the
// sky and the screen edges, nor where a bin has few texels nearby.
//
// The down passes filter with weights [1 3 3 1] / 8 per axis rather than a 2x2 box, and the up passes with a cubic B-spline rather than a tent, as with
// narrower filters the result depends on where content lies relative to the coarse levels' texel grid, and wobbles as the camera moves.


// NUM_SSR_BLUR_LEVELS and NUM_SSR_DIR_BINS are defined by SSRBlurPyramid::build(), as is SSR_BLUR_OUT_LEVEL for the down passes, and
// SSR_BLUR_FINAL_UP_PASS for the up passes.  The down passes and the up passes other than the final one each write one bin.

#if SSR_BLUR_DOWN_PASS

// The level being written has a stream for each level from SSR_BLUR_OUT_LEVEL to NUM_SSR_BLUR_LEVELS.
#define NUM_OUT_STREAMS (NUM_SSR_BLUR_LEVELS - SSR_BLUR_OUT_LEVEL + 1)

uniform int dir_bin; // The bin being written.  Only used for SSR_BLUR_OUT_LEVEL 1, as the finer levels' textures are already for one bin.

// The finer level's area texture, for the bin being written.  For SSR_BLUR_OUT_LEVEL 1, it is instead ssao_depth_normal_texture (see
// compute_ssao_frag_shader.glsl): xyz = cam space normal, w = linear depth, or < 0 where nothing was drawn, which counts as area 1 or 0.
uniform sampler2D area_tex;

// For SSR_BLUR_OUT_LEVEL 1, in_stream_tex_0 is the trace output: rgb = hit radiance, w = lobe width for a hit, or < 0 for a miss.
// Otherwise in_stream_tex_i is the finer level's stream for level SSR_BLUR_OUT_LEVEL + i.
uniform sampler2D in_stream_tex_0;
uniform sampler2D in_stream_tex_1;
uniform sampler2D in_stream_tex_2;
uniform sampler2D in_stream_tex_3;
uniform sampler2D in_stream_tex_4;

layout(location = 0) out vec4 area_out;
layout(location = 1) out vec4 stream_out_0; // Stream for level SSR_BLUR_OUT_LEVEL + i.
#if NUM_OUT_STREAMS > 1
layout(location = 2) out vec4 stream_out_1;
#endif
#if NUM_OUT_STREAMS > 2
layout(location = 3) out vec4 stream_out_2;
#endif
#if NUM_OUT_STREAMS > 3
layout(location = 4) out vec4 stream_out_3;
#endif
#if NUM_OUT_STREAMS > 4
layout(location = 5) out vec4 stream_out_4;
#endif
#if NUM_OUT_STREAMS > 5
layout(location = 6) out vec4 stream_out_5;
#endif

#elif SSR_BLUR_FINAL_UP_PASS

uniform sampler2D own_stream_tex;  // The trace output.
uniform sampler2D area_tex;        // Level 0's area texture, ssao_depth_normal_texture.  See the down pass's area_tex.
uniform sampler2D coarse_tex_0;    // Level 1 of each bin, after its own up pass (or its stream, if it is the coarsest level, as that has no up pass).
uniform sampler2D coarse_tex_1;
uniform sampler2D coarse_area_tex_0;
uniform sampler2D coarse_area_tex_1;

layout(location = 0) out vec4 colour_out;

#else // else if up pass for a level >= 1, for one bin:

uniform sampler2D own_stream_tex;  // The stream for the level being written.
uniform sampler2D area_tex;        // The level being written.
uniform sampler2D coarse_tex;      // Next coarser level, after its own up pass (or the coarsest level's stream, as it has no up pass).
uniform sampler2D coarse_area_tex;

layout(location = 0) out vec4 colour_out;

#endif


// Scale of level 1, in level 0 texels.  Fitted by simulating the pyramid on a flat surface: for lobe widths of 4 to 32 texels, the fraction of the
// energy within 1, 3 and 10 lobe widths then matches GGX to within 0.01, and the radial profile is within about 15% of it out to 8 lobe widths, beyond
// which the coarsest level truncates it.  Below a lobe width of about 2 texels, the texel grid limits how well it can match.
const float LEVEL_SCALE = 0.6;


// Fraction of the lobe made of Gaussians wider than level k's scale, so the fraction to be spread at level k or coarser.  See the top of this file.
float lobeFractionAtOrAboveLevel(float lobe_width, int k)
{
	if(k == 0)
		return 1.0;
	if(k > NUM_SSR_BLUR_LEVELS)
		return 0.0;
	float s = LEVEL_SCALE * exp2(float(k));
	return 1.0 - exp(-(lobe_width * lobe_width) / (2.0 * s * s));
}


// Fraction of a hit's content that belongs at level k.  The coarsest level takes all the rest.
float lobeFractionAtLevel(float lobe_width, int k)
{
	return lobeFractionAtOrAboveLevel(lobe_width, k) - lobeFractionAtOrAboveLevel(lobe_width, k + 1);
}


// Reads a texel of the trace output, as content: (hit radiance, coverage) for a hit, or zero for a miss.  Returns the lobe width.
float readTraceTexel(sampler2D trace_tex, ivec2 coords, out vec4 content)
{
	vec4 c = texelFetch(trace_tex, coords, /*mip level=*/0);
	bool hit = c.w >= 0.0;
	content = hit ? vec4(c.xyz, 1.0) : vec4(0.0);
	return c.w;
}


vec3 camSpacePos(ivec2 coords, ivec2 res, float depth)
{
	vec2 normed_pos_ss = (vec2(coords) + 0.5) / vec2(res);
	return vec3(
		(normed_pos_ss.x - 0.5) * depth * w_over_l,
		(normed_pos_ss.y - 0.5) * depth * h_over_l,
		-depth
	);
}


// Half-width of the range of the mirror direction's camera space x component over which a texel's membership moves from the leftwards bin to the
// rightwards one.  Narrower separates directions better, but makes the membership change more abruptly across curved surfaces.
const float DIR_BIN_HALF_WIDTH = 0.25;

// Membership of a level 0 texel in direction bin dir_bin: 0 = leftwards, 1 = rightwards.  The memberships sum to 1.  Uses the mirror reflection of the
// view direction in the texel's normal, so it is defined for texels whose trace missed too.
float dirBinMembership(ivec2 coords, ivec2 res, vec4 depth_normal, int dir_bin)
{
	vec3 view_dir = normalize(camSpacePos(coords, res, depth_normal.w));
	vec3 refl_dir = reflect(view_dir, depth_normal.xyz);
	float rightwards = smoothstep(-DIR_BIN_HALF_WIDTH, DIR_BIN_HALF_WIDTH, refl_dir.x);
	return (dir_bin == 1) ? rightwards : (1.0 - rightwards);
}


// Down filter weight for a child at the given offset from 2 * the output texel coordinates, for offsets -1 to 2: [1 3 3 1] / 8.
float downFilterWeight(int offset)
{
	return ((offset == 0) || (offset == 1)) ? (3.0 / 8.0) : (1.0 / 8.0);
}


float cubicBSpline(float x)
{
	x = abs(x);
	if(x < 1.0)
		return (4.0 - 6.0 * x * x + 3.0 * x * x * x) * (1.0 / 6.0);
	else if(x < 2.0)
		return (2.0 - x) * (2.0 - x) * (2.0 - x) * (1.0 / 6.0);
	else
		return 0.0;
}


#if SSR_BLUR_DOWN_PASS

// Bilinearly filtered read of the finer level's stream for level SSR_BLUR_OUT_LEVEL + i.
vec4 readInStreamBilinear(int i, vec2 uv)
{
	if(i == 0) return textureLod(in_stream_tex_0, uv, 0.0);
	if(i == 1) return textureLod(in_stream_tex_1, uv, 0.0);
	if(i == 2) return textureLod(in_stream_tex_2, uv, 0.0);
	if(i == 3) return textureLod(in_stream_tex_3, uv, 0.0);
	return textureLod(in_stream_tex_4, uv, 0.0);
}


// Writes the area and streams of level SSR_BLUR_OUT_LEVEL, filtering the 4x4 finer texels around each output texel.  The streams are per unit area of
// the footprint, not of the drawn area: the up pass divides by the area.
void main()
{
	ivec2 coords = ivec2(gl_FragCoord.xy);
	ivec2 in_res = textureSize(area_tex, /*mip level=*/0);

	float area_sum = 0.0;
	vec4 stream_sums[NUM_OUT_STREAMS];
	for(int i=0; i<NUM_OUT_STREAMS; ++i)
		stream_sums[i] = vec4(0.0);

#if SSR_BLUR_OUT_LEVEL == 1
	// Each trace texel's content is split across the levels by its own lobe width, and into the bins by its own membership, before it is filtered, so the
	// texels are read individually.
	for(int y=-1; y<=2; ++y)
	for(int x=-1; x<=2; ++x)
	{
		ivec2 child = coords * 2 + ivec2(x, y);
		if((child.x >= 0) && (child.y >= 0) && (child.x < in_res.x) && (child.y < in_res.y))
		{
			vec4 depth_normal = texelFetch(area_tex, child, /*mip level=*/0);
			if(depth_normal.w >= 0.0) // If something was drawn here:
			{
				float w = downFilterWeight(x) * downFilterWeight(y) * dirBinMembership(child, in_res, depth_normal, dir_bin);
				area_sum += w;

				vec4 content;
				float lobe_width = readTraceTexel(in_stream_tex_0, child, content);
				if(content.w > 0.0)
					for(int i=0; i<NUM_OUT_STREAMS; ++i)
						stream_sums[i] += content * (w * lobeFractionAtLevel(lobe_width, SSR_BLUR_OUT_LEVEL + i));
			}
		}
	}
#else
	// The finer level's streams and area are zero where nothing was drawn, so they can be filtered as they are, with bilinear fetches.  Per axis, the
	// [1 3 3 1] / 8 weights for the children at offsets -1 to 2 are 2 bilinear fetches of weight 1/2 each: at offset -0.25, blending offsets -1 and 0 in
	// the ratio 1:3, and at offset 1.25, blending offsets 1 and 2 in the ratio 3:1.
	// Off the edge of the finer level, the fetches clamp to its edge texels.  That spreads the edge texels' content and area alike, so the content per
	// unit area the up pass computes stays a weighted average of what's there.
	vec2 in_res_f = vec2(in_res);
	vec2 uv_lo = (2.0 * vec2(coords) + 0.25) / in_res_f; // Texel centre of offset -0.25, so (2 * coords - 0.25 + 0.5) / in_res.
	vec2 uv_hi = (2.0 * vec2(coords) + 1.75) / in_res_f; // Offset 1.25.
	for(int j=0; j<4; ++j)
	{
		vec2 uv = vec2(((j & 1) == 0) ? uv_lo.x : uv_hi.x, ((j & 2) == 0) ? uv_lo.y : uv_hi.y);
		area_sum += textureLod(area_tex, uv, 0.0).x * 0.25;
		for(int i=0; i<NUM_OUT_STREAMS; ++i)
			stream_sums[i] += readInStreamBilinear(i, uv) * 0.25;
	}
#endif

	area_out = vec4(area_sum, 0.0, 0.0, 0.0);
	stream_out_0 = stream_sums[0];
#if NUM_OUT_STREAMS > 1
	stream_out_1 = stream_sums[1];
#endif
#if NUM_OUT_STREAMS > 2
	stream_out_2 = stream_sums[2];
#endif
#if NUM_OUT_STREAMS > 3
	stream_out_3 = stream_sums[3];
#endif
#if NUM_OUT_STREAMS > 4
	stream_out_4 = stream_sums[4];
#endif
#if NUM_OUT_STREAMS > 5
	stream_out_5 = stream_sums[5];
#endif
}

#else // else if up pass:

// Upsamples the coarser level with a cubic B-spline filter, returning the filtered content and area.  Dividing one by the other gives content per unit
// drawn area.
// The filter covers the 4x4 coarse texels around this texel.  Per axis, each pair of adjacent taps is done as one bilinear fetch, at the position
// between them that blends them in the ratio of their weights, so it takes 2x2 fetches.  Off the edge of the coarse level, the fetches clamp to its
// edge texels, which spreads their content and area alike.
void upsampleCoarse(sampler2D coarse_tex_, sampler2D coarse_area_tex_, ivec2 coords, out vec4 up_sum, out float area_sum)
{
	ivec2 coarse_res = textureSize(coarse_area_tex_, /*mip level=*/0);
	vec2 coarse_pos = (vec2(coords) + 0.5) * 0.5 - 0.5; // This texel's centre in coarse texel coordinates, where coarse texel centres are at integers.
	vec2 coarse_base = floor(coarse_pos);
	vec2 f = coarse_pos - coarse_base;
	vec2 w0 = vec2(cubicBSpline(1.0 + f.x), cubicBSpline(1.0 + f.y)); // Weights of the taps at coarse_base - 1, coarse_base, coarse_base + 1 and coarse_base + 2.
	vec2 w1 = vec2(cubicBSpline(f.x),       cubicBSpline(f.y));
	vec2 w2 = vec2(cubicBSpline(1.0 - f.x), cubicBSpline(1.0 - f.y));
	vec2 w3 = vec2(cubicBSpline(2.0 - f.x), cubicBSpline(2.0 - f.y));
	vec2 g_lo = w0 + w1;
	vec2 g_hi = w2 + w3;
	vec2 inv_coarse_res = 1.0 / vec2(coarse_res);
	vec2 uv_lo = (coarse_base - 1.0 + w1 / g_lo + 0.5) * inv_coarse_res;
	vec2 uv_hi = (coarse_base + 1.0 + w3 / g_hi + 0.5) * inv_coarse_res;

	up_sum = vec4(0.0);
	area_sum = 0.0;
	for(int j=0; j<4; ++j)
	{
		bool x_lo = (j & 1) == 0;
		bool y_lo = (j & 2) == 0;
		vec2 uv = vec2(x_lo ? uv_lo.x : uv_hi.x, y_lo ? uv_lo.y : uv_hi.y);
		float filter_weight = (x_lo ? g_lo.x : g_hi.x) * (y_lo ? g_lo.y : g_hi.y);
		up_sum   += textureLod(coarse_tex_,      uv, 0.0) * filter_weight;
		area_sum += textureLod(coarse_area_tex_, uv, 0.0).x * filter_weight;
	}
}


#if SSR_BLUR_FINAL_UP_PASS

// Writes level 0, from its own content, plus each bin's upsampled level 1, taken in proportion to this texel's membership of the bin.
void main()
{
	ivec2 coords = ivec2(gl_FragCoord.xy);
	ivec2 res = textureSize(area_tex, /*mip level=*/0);

	vec4 depth_normal = texelFetch(area_tex, coords, /*mip level=*/0);
	if(depth_normal.w < 0.0) // If nothing was drawn here:
	{
		// The main pass only uses SSR texels whose prepass depth and normal match the fragment's, so this texel's value is never used.
		colour_out = vec4(0.0);
		return;
	}

	vec4 own;
	float lobe_width = readTraceTexel(own_stream_tex, coords, own);
	own *= lobeFractionAtLevel(lobe_width, 0);

	// Each bin's content and area are weighted by the membership before dividing, rather than each bin being divided by its own area, so that where a bin
	// has little area nearby, the content per unit area comes from the other bin rather than being lost.
	vec4 up_sum_0, up_sum_1;
	float area_sum_0, area_sum_1;
	upsampleCoarse(coarse_tex_0, coarse_area_tex_0, coords, up_sum_0, area_sum_0);
	upsampleCoarse(coarse_tex_1, coarse_area_tex_1, coords, up_sum_1, area_sum_1);
	float m_0 = dirBinMembership(coords, res, depth_normal, 0);
	float m_1 = 1.0 - m_0;
	float area_sum = m_0 * area_sum_0 + m_1 * area_sum_1;
	vec4 up = (area_sum > 1.0e-4) ? ((m_0 * up_sum_0 + m_1 * up_sum_1) / area_sum) : vec4(0.0);

	vec4 total = own + up; // Per unit drawn area, as level 0 texels where something was drawn have area 1.

	// Where differently sized lobes overlap, the coverage can sum to a little over 1.  Clamp it, keeping the radiance per unit coverage.
	colour_out = total / max(total.w, 1.0);
}

#else // else if up pass for a level >= 1:

// Writes the level, from its own stream, plus the upsampled next coarser level.  The output is per unit area of the footprint, like the streams, so that
// the next finer level's up pass can upsample it the same way.
void main()
{
	ivec2 coords = ivec2(gl_FragCoord.xy);

	float area = texelFetch(area_tex, coords, /*mip level=*/0).x;
	if(area <= 0.0) // If nothing of this bin was drawn under this texel:
	{
		colour_out = vec4(0.0);
		return;
	}

	vec4 own = texelFetch(own_stream_tex, coords, /*mip level=*/0);

	vec4 up_sum;
	float area_sum;
	upsampleCoarse(coarse_tex, coarse_area_tex, coords, up_sum, area_sum);
	vec4 up = (area_sum > 1.0e-4) ? (up_sum / area_sum) : vec4(0.0);

	colour_out = own + up * area; // own / area + up is per unit drawn area, so multiplying by the area gives per unit area of the footprint.
}

#endif // end if up pass for a level >= 1

#endif
