
// Applies the half-resolution volumetric cloud buffer written by cloud_frag_shader.glsl to the scene colour.
//
// The upsample is depth-aware.  Each half-res pixel traced its ray for one particular full-res pixel - the one
// at twice its coordinates - and stopped at whatever that pixel's depth was.  So a half-res pixel that stopped
// at a nearby rooftop holds a very different amount of cloud from its neighbour that ran on to the horizon, and
// blending the two bilinearly would draw a halo of sky-cloud around every silhouette.  Weighting each of the
// four taps by how well its depth matches this pixel's keeps the cloud on the correct side of the edge.


uniform sampler2D albedo_texture;	// Main colour buffer, full resolution.
uniform sampler2D cloud_texture;	// Half resolution.  rgb = in-scattered radiance, a = transmittance.
uniform sampler2D depth_tex;		// Main depth buffer, full resolution.

in vec2 pos; // [0, 1] x [0, 1]

out vec4 colour_out;


// Distance at which two samples are considered to be on different surfaces, as a fraction of the distance to
// the nearer of them.
const float DEPTH_MISMATCH_TOLERANCE = 0.01;


highp float linearDistAtTexel(ivec2 px)
{
	highp float depth_val = texelFetch(depth_tex, px, /*mip level=*/0).x;

	// Sky pixels are at an effectively infinite distance; clamp so that two of them compare as equal rather
	// than as inf - inf.
	return min(getDepthFromDepthTextureValue(near_clip_dist, depth_val), 1.0e9); // getDepthFrom... is in frag_utils.glsl
}


void main()
{
	ivec2 full_res = textureSize(depth_tex, /*mip level=*/0);
	ivec2 half_res = textureSize(cloud_texture, /*mip level=*/0);
	ivec2 px = ivec2(gl_FragCoord.xy);

	vec4 scene_colour = texelFetch(albedo_texture, px, /*mip level=*/0);

	highp float linear_dist = linearDistAtTexel(px);

	// Position of this pixel in half-res texel-centre units, so that flooring gives the lower-left of the four
	// taps to blend between.
	vec2 half_coords = (vec2(px) + vec2(0.5)) * 0.5 - vec2(0.5);
	ivec2 base = ivec2(floor(half_coords));
	vec2 frac = half_coords - vec2(base);

	vec4 cloud_sum = vec4(0.0);
	float weight_sum = 0.0;
	for(int dy=0; dy<2; ++dy)
	for(int dx=0; dx<2; ++dx)
	{
		ivec2 tap = clamp(base + ivec2(dx, dy), ivec2(0), half_res - ivec2(1));

		float bilinear_weight = (dx != 0 ? frac.x : 1.0 - frac.x) * (dy != 0 ? frac.y : 1.0 - frac.y);

		// The full-res texel this half-res pixel traced its ray for.
		highp float tap_dist = linearDistAtTexel(min(tap * 2, full_res - ivec2(1)));

		highp float relative_mismatch = abs(tap_dist - linear_dist) / max(min(tap_dist, linear_dist), 1.0);
		float weight = bilinear_weight / (relative_mismatch + DEPTH_MISMATCH_TOLERANCE);

		cloud_sum += texelFetch(cloud_texture, tap, /*mip level=*/0) * weight;
		weight_sum += weight;
	}

	vec4 cloud = cloud_sum / weight_sum;

	colour_out = vec4(scene_colour.rgb * cloud.a + cloud.rgb, scene_colour.a);
}
