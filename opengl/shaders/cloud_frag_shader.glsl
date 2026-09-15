
// Volumetric cumulus clouds.
//
// Raymarches a shell of cloud between CloudSettings::bottom_z and top_z, at half the main buffer's resolution.
// Writes rgb = radiance scattered towards the camera, a = transmittance through the cloud along the ray;
// cloud_composite_frag_shader.glsl upsamples that and applies it to the scene colour.
//
// The shell is spherical rather than a flat slab, so that the clouds curve down to a horizon at a believable
// distance instead of stretching out forever.  Everything is computed in camera-relative coordinates with the
// planet centre directly below the camera, which keeps the intersection maths away from the catastrophic
// cancellation you get from working with radii around 6.4e6 in float32.





uniform sampler2D depth_tex;		// Main depth buffer, full resolution.
uniform sampler2D blue_noise_tex;
uniform sampler2D fbm_tex;


in vec2 pos; // [0, 1] x [0, 1]

out vec4 colour_out;





vec3 camSpaceFromScreenSpaceDir(vec2 normed_pos_ss)
{
	return vec3(
		(normed_pos_ss.x - 0.5) / l_over_w,
		(normed_pos_ss.y - 0.5) / l_over_h,
		-1
	);
}


#if CLOUD_ENV_MAP
uniform int cloud_env_debug_pattern; // 0 = off (march clouds normally).  See OpenGLEngine::cloud_env_debug_pattern.


// Test patterns written into the cloud map in place of the clouds, for checking what a reflective material's
// lookup actually does with it, independently of whether the raymarch is producing anything sensible.
//
// A reflection of pattern 1 should read as a clean checkerboard wherever the water is resolving the map at
// better than one texel per pixel, and break up into flickering noise wherever it isn't - which makes the
// sampling rate visible directly.  Scaled to the sky's own brightness so the result tone maps like a cloud.
vec4 cloudEnvDebugPattern(vec2 map_pos)
{
	float ref = sun_and_sky_av_spec_rad.y; // A rough "as bright as a cloud" reference level.

	if(cloud_env_debug_pattern == 1) // One square per texel of the map.
	{
		ivec2 texel = ivec2(gl_FragCoord.xy);
		float v = float((texel.x + texel.y) & 1);
		return vec4(vec3(v * ref), 0.0);
	}
	else if(cloud_env_debug_pattern == 2) // Checkerboard in direction space: 16 cells around, 8 top to bottom.
	{
		float v = mod(floor(map_pos.x * 64.0) + floor(map_pos.y * 32.0), 2.0);
		return vec4(vec3(v * ref), 0.0);
	}
	else if(cloud_env_debug_pattern == 3) // Diagonal stripes 4 texels wide.
	{
		// Diagonal, so neither axis of the map is being tested on its own, and a regular stripe period so that
		// undersampling shows up as moire - which is far easier to see, and to judge the severity of, than the
		// speckle a checkerboard breaks into.
		/*ivec2 texel = ivec2(gl_FragCoord.xy);
		float v = float(((texel.x + texel.y) / 4) & 1);
		return vec4(vec3(v * ref), 0.0);*/
		float v = mod(floor((map_pos.x + map_pos.y) * 32.0), 2.0);
		return vec4(vec3(v * ref), 0.0);
	}
	else // Map coordinates: red = phi, green = theta.  Shows which part of the map a reflection is reading.
	{
		return vec4(vec3(map_pos.x, map_pos.y, 0.0) * ref, 0.0);
	}
}
#endif


void main()
{
#if CLOUD_ENV_MAP
	// Building the directional cloud map: one texel per world-space direction, laid out lat-long to match
	// cloudEnvMapCoordsForDir() in frag_utils.glsl.  Nothing occludes these rays - the map is what reflective
	// surfaces see of the sky, not what the camera sees of the scene.
	float map_phi   = (pos.x - 0.5) * (2.0 * PI);
	float map_theta = cloudEnvMapThetaForV(pos.y);
	float sin_map_theta = sin(map_theta);

	highp vec3 dir_ws = vec3(sin_map_theta * cos(map_phi), sin_map_theta * sin(map_phi), cos(map_theta));

	if(cloud_env_debug_pattern != 0)
	{
		colour_out = cloudEnvDebugPattern(pos);
		return;
	}

	highp float scene_dist = 1.0e9;

	// The map is sampled by direction rather than per screen pixel, so there is no screen-space dither to hide
	// step artifacts behind.  Halving the step keeps the coarse march from banding across a texel.
	float step_scale = 0.5;
#else
	// This half-res pixel stands for one particular full-res pixel, and traces the ray through it.  The
	// composite pass reads the depth at the same texel to work out which half-res pixels it can blend.
	ivec2 full_res = textureSize(depth_tex, /*mip level=*/0);
	ivec2 full_px = min(ivec2(gl_FragCoord.xy) * 2, full_res - ivec2(1));
	vec2 full_res_pos = (vec2(full_px) + vec2(0.5)) / vec2(full_res);

	vec3 dir_cs = normalize(camSpaceFromScreenSpaceDir(full_res_pos));

	// Note that right multiplying by a matrix is the same as left multiplying by its transpose in GLSL.
	highp vec3 dir_ws = normalize((vec4(dir_cs, 0.0) * frag_view_matrix).xyz);

	// Stop at the first thing the ray hits.  Sky pixels have a depth value that maps to an effectively infinite
	// distance, so clamp before it can turn into an inf or a NaN.
	highp float depth_val = texelFetch(depth_tex, full_px, /*mip level=*/0).x;
	highp float linear_depth = getDepthFromDepthTextureValue(near_clip_dist, depth_val); // defined in frag_utils.glsl
	highp float scene_dist = min(max(0.01, linear_depth / -dir_cs.z), 1.0e9);

	float step_scale = 1.0;
#endif

	// A per-pixel (but not per-frame) offset into the first step.  Without it the first sample of every ray
	// lands on the same surface and the cloud base shows a set of concentric shells.
	float pixel_hash = texture(blue_noise_tex, gl_FragCoord.xy * (1.0 / 64.0)).x;

	colour_out = raymarchClouds(mat_common_campos_ws.xyz, dir_ws, scene_dist, pixel_hash, step_scale, /*aerial dist offset=*/0.0, fbm_tex);
}
