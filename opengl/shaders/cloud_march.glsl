/*=====================================================================
cloud_march.glsl
----------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
// Shared volumetric cloud raymarching.
//
// Appended to the fragment shader defines of each program that needs to march clouds - the cloud pass, the
// directional cloud map, and water - rather than living in frag_utils.glsl, because it declares a uniform block
// and two sampler3Ds that most shaders have no business carrying.  See OpenGLEngine::buildPrograms().


// Should match CloudGPUSettings struct in OpenGLEngine.h
layout (std140) uniform CloudSettings
{
	float bottom_z;			// Altitude of the base of the cloud layer (m).
	float top_z;			// Altitude of the top of the cloud layer (m).
	float coverage_bias;	// Added to the weather coverage before clamping.  Negative for fewer clouds.
	float density;			// Extinction coefficient at full density, per metre.

	float shape_period;		// World-space period of the shape noise volume (m).
	float detail_period;	// World-space period of the detail noise volume (m).
	float wind_speed;		// Drift speed of the layer in +x (m/s).  Matches the drift of cumulusCoverage().
	float max_march_dist;	// Cap on the distance marched along the view ray (m).

	float light_march_dist;	// Distance marched towards the sun when computing self-shadowing (m).
	float sun_factor;		// Scales the sunlight contribution.
	float ambient_factor;	// Scales the sky light contribution.
	float detail_fade_dist;	// Fine erosion fades to an average approximation over the last half of this distance (m).

	float powder_dist;		// Depth scale of the dark-edge (powder) term (m).
	float ground_albedo;	// Fraction of light the ground bounces back up onto the cloud bases.
	float shape_vertical_scale;	// Vertical period of the shape volume, as a fraction of its horizontal period.
	float padding2;

} cloud_settings;


uniform highp sampler3D cloud_shape_tex;	// See CloudNoise.h.  GLSL ES gives sampler3D no default precision.
uniform highp sampler3D cloud_detail_tex;


const float PLANET_RADIUS = 6371000.0;

const int MAX_MARCH_STEPS = 96;
const int NUM_LIGHT_STEPS = 6;
const int EMPTY_STEPS_BEFORE_SKIPPING = 8; // How much empty space to cross before going back to coarse steps.

// Distance over which the step size doubles.
const float STEP_GROWTH_DIST = 8000.0;

// Dominant feature sizes in the three detail channels (base frequencies from CloudNoise.cpp).
const vec3 DETAIL_FEATURE_FRAC = vec3(1.0 / 4.0, 1.0 / 8.0, 1.0 / 16.0);


float remap(float x, float old_lo, float old_hi, float new_lo, float new_hi)
{
	return new_lo + (x - old_lo) / (old_hi - old_lo) * (new_hi - new_lo);
}


// Solves t^2 + 2*b*t + c = 0, returning the roots in increasing order, or a pair of negative values that every
// caller's "is there a root in front of me" test rejects if there are none.
// The root of smaller magnitude is computed as c/q rather than as -b + sqrt(disc), which for a ray pointing at a
// sphere nearly as large as the planet would be the difference of two nearly equal numbers.
// Everything here is highp: c is around 1e10 for a cloud layer a kilometre up, which is well outside the range
// mediump is allowed to represent.
highp vec2 solveQuadratic(highp float b, highp float c)
{
	highp float disc = b*b - c;
	if(disc < 0.0)
		return vec2(-2.0, -1.0);

	highp float sqrt_disc = sqrt(disc);
	highp float q = (b >= 0.0) ? -(b + sqrt_disc) : -(b - sqrt_disc);
	if(q == 0.0)
		return vec2(0.0, 0.0); // The ray starts on the sphere and grazes it.

	highp float t_a = q;
	highp float t_b = c / q;
	return vec2(min(t_a, t_b), max(t_a, t_b));
}


// Henyey-Greenstein phase function.
float HG(float cos_theta, float g)
{
	float g2 = g * g;
	return (1.0 - g2) / (4.0 * PI * pow(max(1.0 + g2 - 2.0 * g * cos_theta, 1.0e-4), 1.5));
}


// Draine's phase function: a Henyey-Greenstein lobe with an extra (1 + alpha*u^2) factor, which lets it take on
// the much sharper forward peak that Mie scattering has for particles as large as cloud droplets.
// See Jendersie and d'Eon, "An Approximate Mie Scattering Function for Fog and Cloud Rendering" (2023).
float draine(float cos_theta, float g, float alpha)
{
	float g2 = g * g;
	float denom = pow(max(1.0 + g2 - 2.0 * g * cos_theta, 1.0e-4), 1.5);

	return ((1.0 - g2) / (4.0 * PI)) * ((1.0 + alpha * cos_theta * cos_theta) / (1.0 + alpha * (1.0 + 2.0 * g2) / 3.0)) / denom;
}


// Cloud droplets are tens of times larger than the wavelengths they scatter, so their true Mie phase function
// has a diffraction peak a few degrees wide and orders of magnitude above isotropic.  That peak is what lights
// the rim of a cloud with the sun behind it.  A single Henyey-Greenstein lobe peaks at only ~45x isotropic and
// is tens of degrees wide, which is why a plain HG cloud has no silver lining to speak of.
//
// This is a sharp Draine lobe for that peak, a broad HG lobe for the bulk of the forward scattering, and a weak
// backwards lobe.  It is not Mie: there is no cloudbow at ~138 degrees and no glory.  The constants are chosen
// to look right at this engine's exposure rather than fitted to a droplet size distribution.
float cloudPhase(float cos_theta, float eccentricity_scale)
{
	float sharp = draine(cos_theta, 0.965 * eccentricity_scale, 12.0);
	float broad = HG(cos_theta, 0.72 * eccentricity_scale);
	float back  = HG(cos_theta, -0.35 * eccentricity_scale);

	return sharp * 0.28 + broad * 0.57 + back * 0.15;
}


// Rounded upper profile, applied before erosion so detail can carve the caps.  The basal fade is
// applied separately after erosion, which prevents its low densities being cut off by the erosion threshold.
float cloudTopGradient(float h, float cloud_top)
{
	float local_h = h / cloud_top;
	return 1.0 - smoothstep(0.55, 1.0, local_h);
}


// Extinction multiplier in [0, 1] at a world-space point, 'h' being its fractional height in the layer.
// Coarse search rays omit erosion for a conservative density estimate.  View and sun visibility rays
// retain erosion at every distance, with detail_weight blending resolved noise into an average approximation.
float cloudDensity(highp vec3 p_ws, float h, float coverage, float cloud_top, bool do_detail, vec3 detail_weight)
{
	if(!(coverage > 0.0))
		return 0.0; // Also guards the division by coverage in the remap below.
	float top_gradient = cloudTopGradient(h, cloud_top);
	// A gradual condensation transition through the lower 30% of each cloud, rather than a thin,
	// nearly opaque sheet.  Keep this outside the threshold/remap operations below.
	float base_gradient = smoothstep(0.0, 0.30, h / cloud_top);
	if(!(top_gradient > 0.0) || !(base_gradient > 0.0))
		return 0.0;

	// The layer drifts downwind, and shears as it goes, so the tops trail behind the bases.
	highp vec3 p = p_ws;
	p.x += time * cloud_settings.wind_speed * (1.0 + h * 0.3);

	// Cumulus are wider than they are tall, so the shape volume is sampled anisotropically, with a longer period
	// horizontally than vertically.  Sampled isotropically its lobes come out as tall as they are wide, and the
	// result reads as a field of towers rather than of cumulus.
	highp vec3 shape_coords = vec3(
		p.xy * (1.0 / cloud_settings.shape_period),
		p.z  * (1.0 / (cloud_settings.shape_period * cloud_settings.shape_vertical_scale)));

	vec4 shape = textureLod(cloud_shape_tex, shape_coords, 0.0);

	// Erode the Perlin-Worley base with the Worley channels.  This is what turns a smooth blob into something
	// with lobes and crevices in it.
	float worley_fbm = shape.g * 0.625 + shape.b * 0.25 + shape.a * 0.125;
	float base = remap(shape.r, worley_fbm - 1.0, 1.0, 0.0, 1.0);

	// Cut away everything below the coverage threshold, so that raising coverage grows the clouds outwards
	// from their cores.  Do not multiply by coverage: small clouds should still have dense cores.
	// Apply the upper profile after the footprint threshold, but before erosion so that detail can
	// carve the rounded caps as well as the sides of the cloud.
	float density = remap(base, 1.0 - coverage, 1.0, 0.0, 1.0);
	if(!(density > 0.0))
		return 0.0;
	density = clamp(density, 0.0, 1.0) * top_gradient;

	if(do_detail)
	{
		// Approximate unresolved noise by its midrange, retaining bulk erosion rather than letting
		// distant clouds fill in.  This is an approximation, not the measured mean of the noise volume.
		vec3 detail_noise = vec3(0.5);
		if(any(greaterThan(detail_weight, vec3(0.0))))
		{
			vec3 d = textureLod(cloud_detail_tex, p * (1.0 / cloud_settings.detail_period), 0.0).rgb;
			// Give the finer channels enough contrast to break up the silhouette, then fade each
			// band independently so unresolved fine noise does not erase the larger billows.
			d = clamp((d - 0.5) * 1.7 + 0.5, 0.0, 1.0);
			detail_noise = mix(detail_noise, d, detail_weight);
		}
		float detail_fbm = dot(detail_noise, vec3(0.35, 0.40, 0.25));

		// Wispy, stretched-out detail near the base of the cloud; rounded billows higher up.
		float detail = mix(1.0 - detail_fbm, detail_fbm, clamp(h * 4.0, 0.0, 1.0));

		density = remap(density, detail * 0.55, 1.0, 0.0, 1.0);
	}

	return clamp(density, 0.0, 1.0) * base_gradient;
}


// Marches the cloud layer along a ray, returning rgb = radiance scattered towards the ray origin and
// a = transmittance along it.  Composite over what lies beyond with: light = light * result.a + result.rgb.
//
// The ray origin is a parameter rather than being assumed to be the camera, so that a reflective surface can
// march its own reflected ray and get the clouds that are really above it.  Sampling a precomputed directional
// map instead treats the layer as infinitely far away, which on water is plainly visible.
//
//   scene_dist         - stop here (distance along the ray).  Use a large value if nothing occludes it.
//   pixel_hash         - per-pixel dither in [0, 1], to break up the first step of the march.
//   step_scale         - multiplies the step size.  Above 1 for callers that want a cheaper, coarser march.
//   aerial_dist_offset - distance from the camera to the ray origin, for the aerial perspective term.
//
// fbm_tex is a parameter rather than a global because this file is appended to its callers' defines, ahead of
// the shader body that declares it.
vec4 raymarchClouds(highp vec3 origin_ws, highp vec3 dir_ws, highp float scene_dist, float pixel_hash, float step_scale,
	float aerial_dist_offset, in sampler2D fbm_tex)
{
	highp float origin_z = origin_ws.z;
	highp float origin_r = PLANET_RADIUS + origin_z; // Distance from the planet centre to the ray origin.

	highp float layer_thickness = cloud_settings.top_z - cloud_settings.bottom_z;

	//------------------------------ Find the part of the shell the ray passes through ------------------------------
	// Both boundary spheres are centred on the planet centre, which in camera-relative coordinates is at
	// (0, 0, -origin_r), so the linear coefficient is the same for both and c is a difference of altitudes.
	highp float b = origin_r * dir_ws.z;
	highp float c_bottom = (origin_z - cloud_settings.bottom_z) * (PLANET_RADIUS * 2.0 + origin_z + cloud_settings.bottom_z);
	highp float c_top    = (origin_z - cloud_settings.top_z   ) * (PLANET_RADIUS * 2.0 + origin_z + cloud_settings.top_z   );

	highp vec2 bottom_roots = solveQuadratic(b, c_bottom);
	highp vec2 top_roots    = solveQuadratic(b, c_top);

	highp float t_start, t_end;
	if(origin_z < cloud_settings.bottom_z) // Below the layer: the ray crosses the base, then the top.
	{
		t_start = bottom_roots.y;
		t_end   = top_roots.y;
	}
	else if(origin_z <= cloud_settings.top_z) // Inside the layer: start at the camera, leave by whichever boundary comes first.
	{
		t_start = 0.0;
		t_end   = top_roots.y;
		if(bottom_roots.x > 0.0)
			t_end = min(t_end, bottom_roots.x);
	}
	else // Above the layer: enter through the top, leave through the base, or through the far side of the top if the ray misses the base.
	{
		t_start = max(top_roots.x, 0.0);
		t_end   = (bottom_roots.x > 0.0) ? bottom_roots.x : top_roots.y;
	}

	t_start = max(t_start, 0.0);
	t_end   = min(min(t_end, scene_dist), t_start + cloud_settings.max_march_dist);

	if(!(t_end > t_start)) // Also catches the NaN that a missed shell would otherwise produce.
		return vec4(0.0, 0.0, 0.0, 1.0);

	// The distance fade below has already reached zero everywhere in [t_start, t_end], so the march can only
	// return nothing.  Grazing rays meet the layer tens of kilometres out and hit this constantly - without it
	// they spend the whole step budget crossing faded-out space.
	if(t_start >= cloud_settings.max_march_dist)
		return vec4(0.0, 0.0, 0.0, 1.0);

	//------------------------------ Raymarch ------------------------------
	// The step size is set by how thick the layer is rather than by how far the ray travels through it, so that
	// a cloud is sampled at the same rate whether the ray goes straight up through it or grazes along it.  It
	// then grows with distance: a cloud 20 km away covers a small fraction of the pixels a nearby one does, and
	// growing the step is what lets a fixed step budget reach the horizon at all.
	highp float base_step = layer_thickness * (1.0 / 48.0) * step_scale;
	float inv_thickness = 1.0 / layer_thickness;

	float cos_theta = dot(sundir_ws.xyz, dir_ws);

	vec3 scattered_radiance = vec3(0.0);
	float T = 1.0; // Transmittance from the camera to the current sample.

	highp float t = t_start + pixel_hash * base_step * 3.0;
	bool taking_big_steps = true;
	int empty_steps = 0;

	for(int i=0; i<MAX_MARCH_STEPS; ++i)
	{
		if((t >= t_end) || (T < 0.01))
			break;

		highp float small_step = base_step * (1.0 + t * (1.0 / STEP_GROWTH_DIST));
		highp float big_step = small_step * 3.0;

		highp vec3 p_rel = dir_ws * t; // Camera-relative position of the sample.

		// Altitude above sea level: a second-order expansion of length(p_rel - planet_centre) - PLANET_RADIUS.
		// The horizontal term is the drop of the curved surface away from the camera, ~790 m at 100 km.
		highp float altitude = origin_z + p_rel.z + dot(p_rel.xy, p_rel.xy) / (2.0 * (origin_r + p_rel.z));
		float h = (altitude - cloud_settings.bottom_z) * inv_thickness;

		float density = 0.0;
		float coverage = 0.0;
		float cloud_top = 1.0;
		if((h >= 0.0) && (h <= 1.0))
		{
			highp vec3 p_ws = origin_ws + p_rel;

			coverage = cumulusCoverageLod(p_ws.xy, time, fbm_tex, small_step, cloud_settings.coverage_bias);
			// Sparse regions carry shallow puffs; stronger weather grows taller clouds, with the slower
			// regional field still varying their maximum height from one cloud bank to the next.
			float regional_top = cumulusTopHeightFrac(p_ws.xy, time, fbm_tex, small_step);
			cloud_top = mix(0.25, regional_top, smoothstep(0.0, 0.75, coverage));

			// Fade fine structure smoothly both with distance and as the march stops resolving its features.
			// Past either limit we skip the texture fetch, but keep approximate average erosion.
			float fade_dist = max(cloud_settings.detail_fade_dist, 0.001);
			vec3 feature_size = max(cloud_settings.detail_period * DETAIL_FEATURE_FRAC, vec3(0.001));
			vec3 detail_weight = (1.0 - smoothstep(fade_dist * 0.5, fade_dist, t)) *
				(1.0 - smoothstep(feature_size * 0.5, feature_size, vec3(small_step)));

			density = cloudDensity(p_ws, h, coverage, cloud_top, !taking_big_steps, detail_weight);

			// Fade the layer out before the end of the march, so that the cap on march distance doesn't show
			// up as a wall of cloud cut off in mid-air.
			density *= 1.0 - smoothstep(cloud_settings.max_march_dist * 0.6, cloud_settings.max_march_dist, t);
		}

		if(taking_big_steps)
		{
			if(density > 0.0)
			{
				// Back up to before the cloud we just stepped into and resample it finely, so the lit edge
				// doesn't land wherever the coarse steps happened to fall.
				t = max(t - (big_step - small_step), t_start);
				taking_big_steps = false;
				empty_steps = 0;
				continue;
			}

			t += big_step;
			continue;
		}

		if(density > 0.0)
		{
			empty_steps = 0;

			float sigma_t = cloud_settings.density * density;

			//------------------------------ Light the sample ------------------------------
			// March towards the sun to find how much of the layer is between this sample and the sun.
			// Coverage varies over kilometres, so it is sampled once here rather than at every light step.
			float light_optical_depth = 0.0;
			{
				for(int j=0; j<NUM_LIGHT_STEPS; ++j)
				{
					// Short intervals near the sample resolve shadows from the eroded surface; longer
					// intervals further away still cover the full light path with the same sample count.
					float u0 = float(j) / float(NUM_LIGHT_STEPS);
					float u1 = float(j + 1) / float(NUM_LIGHT_STEPS);
					float light_start = cloud_settings.light_march_dist * u0 * u0;
					float light_end = cloud_settings.light_march_dist * u1 * u1;
					float light_step = light_end - light_start;
					highp vec3 lp_rel = p_rel + sundir_ws.xyz * ((light_start + light_end) * 0.5);

					highp float l_altitude = origin_z + lp_rel.z + dot(lp_rel.xy, lp_rel.xy) / (2.0 * (origin_r + lp_rel.z));
					float l_h = (l_altitude - cloud_settings.bottom_z) * inv_thickness;
					if((l_h < 0.0) || (l_h > 1.0))
						break; // Left the layer, so there is nothing further along the ray to occlude the sun.

					float fade_dist = max(cloud_settings.detail_fade_dist, 0.001);
					vec3 feature_size = max(cloud_settings.detail_period * DETAIL_FEATURE_FRAC, vec3(0.001));
					vec3 light_detail_weight = (1.0 - smoothstep(fade_dist * 0.5, fade_dist, t)) *
						(1.0 - smoothstep(feature_size * 0.5, feature_size, vec3(light_step)));
					float l_density = cloudDensity(origin_ws + lp_rel, l_h, coverage, cloud_top, /*do detail=*/true, light_detail_weight);

					light_optical_depth += l_density * light_step;
				}
			}

			// Approximate multiple scattering with a few octaves of progressively cheaper-to-reach, more
			// isotropic light (Wrenninge et al., "Oz: The Great and Volumetric").  Beer's law alone makes the
			// interior of a cloud far too dark, because in reality light that has bounced a few times still
			// gets there.
			float sun_scatter = 0.0;
			float octave_weight = 1.0, octave_extinction = 1.0, octave_eccentricity = 1.0;
			for(int n=0; n<3; ++n)
			{
				sun_scatter += octave_weight * exp(-light_optical_depth * cloud_settings.density * octave_extinction) *
					cloudPhase(cos_theta, octave_eccentricity);

				octave_weight *= 0.5;
				octave_extinction *= 0.5;
				octave_eccentricity *= 0.5;
			}

			// Powder term: just under a lit surface, light that scattered a short way in comes back out, so the
			// edge of a cloud reads darker than Beer's law on its own would make it.
			// It only applies when looking at the lit side of a cloud.  Applied to a backlit edge it would
			// darken precisely the thin rim that the forward scattering above is there to make bright, which is
			// the opposite of what a cloud in front of the sun does.
			float powder = 1.0 - exp(-sigma_t * cloud_settings.powder_dist);
			float powder_weight = clamp(0.5 - 0.5 * cos_theta, 0.0, 1.0); // 1 looking away from the sun, 0 looking into it.
			powder = mix(1.0, powder, powder_weight);

			// The three octaves above are a cheap stand-in for light that has bounced many times, and they fall
			// well short of it: a real sunlit cumulus is about as bright as fresh snow, whereas single scattering
			// plus these octaves lands several times darker.  sun_factor closes that gap.
			vec3 sun_radiance = sun_spec_rad_times_solid_angle.xyz * (sun_scatter * powder * cloud_settings.sun_factor);

			// Sky light from above, plus light bounced up off the ground.  Without the bounce a cloud base is lit
			// only by whatever sky light gets past the cloud above it, which makes it far darker than a real one:
			// undersides get a lot of their light from the ground.
			vec3 sky_light = sky_av_spec_rad.xyz * mix(0.45, 1.0, h);
			vec3 ground_bounce = sun_and_sky_av_spec_rad.xyz * cloud_settings.ground_albedo * (1.0 - h * 0.7);

			vec3 ambient_radiance = (sky_light + ground_bounce) * cloud_settings.ambient_factor;

			//------------------------------ Integrate over the step ------------------------------
			// Cloud droplets scatter almost everything they extinguish, so sigma_s / sigma_t is taken as 1 and
			// the integral of T over the step is just (1 - step_T).
			float step_T = exp(-sigma_t * small_step);
			vec3 sample_radiance = sun_radiance + ambient_radiance;
#if DEPTH_FOG
			// Match the aerial perspective in phong_frag_shader.glsl, using this sample's distance
			// from the camera.  Weight the haze by cloud opacity below: the background already carries
			// its own atmosphere, so empty space must not add haze again or change cloud transmittance.
			vec3 air_transmission = exp(air_scattering_coeffs.xyz * -(t + aerial_dist_offset));
			sample_radiance = sample_radiance * air_transmission +
				sun_and_sky_av_spec_rad.xyz * (1.0 - air_transmission);
#endif
			scattered_radiance += T * sample_radiance * (1.0 - step_T);
			T *= step_T;
		}
		else
		{
			empty_steps++;
			if(empty_steps >= EMPTY_STEPS_BEFORE_SKIPPING)
				taking_big_steps = true;
		}

		t += small_step;
	}

	return vec4(scattered_radiance, T);
}
