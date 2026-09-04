
in vec3 normal_ws;
in vec3 pos_cs;
#if GENERATE_PLANAR_UVS
in vec3 pos_os;
#endif
in vec3 pos_ws;
//in vec2 texture_coords;
in vec3 cam_to_pos_ws;


uniform sampler2D specular_env_tex;
uniform sampler2D fbm_tex;
uniform sampler2D blue_noise_tex;
uniform sampler2D cirrus_tex;
uniform sampler2D aurora_tex;


uniform sampler2D main_colour_texture; // source texture
uniform sampler2D main_normal_texture;
uniform sampler2D main_depth_texture;


//----------------------------------------------------------------------------------------------------------------------------
#if OB_AND_MAT_DATA_GPU_RESIDENT

flat in int material_index;

layout(std430) buffer PhongUniforms
{
	MaterialData material_data[];
};

#define MAT_UNIFORM					material_data[material_index]

#define DIFFUSE_TEX					MAT_UNIFORM.diffuse_tex
#define EMISSION_TEX				MAT_UNIFORM.emission_tex

//----------------------------------------------------------------------------------------------------------------------------
#else // else if !OB_AND_MAT_DATA_GPU_RESIDENT:


layout (std140) uniform PhongUniforms
{
	MaterialData matdata;

} mat_data;

#define MAT_UNIFORM mat_data.matdata


#if !USE_BINDLESS_TEXTURES
uniform sampler2D diffuse_tex;
uniform sampler2D emission_tex;
#endif


#if USE_BINDLESS_TEXTURES
#define DIFFUSE_TEX  MAT_UNIFORM.diffuse_tex
#define EMISSION_TEX MAT_UNIFORM.emission_tex
#else
#define DIFFUSE_TEX  diffuse_tex
#define EMISSION_TEX emission_tex
#endif

#endif // end if !OB_AND_MAT_DATA_GPU_RESIDENT
//----------------------------------------------------------------------------------------------------------------------------


//#if USE_SSBOS
//layout (std430) buffer LightDataStorage
//{
//	LightData light_data[];
//};
//#else
//layout (std140) uniform LightDataStorage
//{
//	LightData light_data[256];
//};
//#endif



layout(location = 0) out vec4 colour_out;
#if NORMAL_TEXTURE_IS_UINT
layout(location = 1) out uvec4 normal_out;
#else
layout(location = 1) out vec3 normal_out;
#endif


// https://www.shadertoy.com/view/MdcfDj
#define M1 1597334677U     //1719413*929
#define M2 3812015801U     //140473*2467*11
float hash( uvec2 q )
{
	q *= uvec2(M1, M2); 

	uint n = (q.x ^ q.y) * M1;

	return float(n) * (1.0/float(0xffffffffU));
}

// 'A Survey of Efficient Representations for Independent Unit Vectors', listing 1+2.
// Returns +- 1
//vec2 signNotZero(vec2 v) {
//	return vec2(((v.x >= 0.0) ? 1.0 : -1.0), ((v.y >= 0.0) ? 1.0 : -1.0));
//}

//vec3 oct_to_float32x3(vec2 e) {
//	vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
//	if (v.z < 0.0) v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
//	return normalize(v);
//}
//
//// 'A Survey of Efficient Representations for Independent Unit Vectors', listing 5.
//vec2 unorm8x3_to_snorm12x2(vec3 u) {
//	u *= 255.0;
//	u.y *= (1.0 / 16.0);
//	vec2 s = vec2(u.x * 16.0 + floor(u.y),
//		fract(u.y) * (16.0 * 256.0) + u.z);
//	return clamp(s * (1.0 / 2047.0) - 1.0, vec2(-1.0), vec2(1.0));
//}



// Converts a unit vector to a point in octahedral representation ('oct').
// 'A Survey of Efficient Representations for Independent Unit Vectors', listing 1.

// Assume normalized input. Output is on [-1, 1] for each component.
//vec2 float32x3_to_oct(in vec3 v) {
//	// Project the sphere onto the octahedron, and then onto the xy plane
//	vec2 p = v.xy * (1.0 / (abs(v.x) + abs(v.y) + abs(v.z)));
//	// Reflect the folds of the lower hemisphere over the diagonals
//	return (v.z <= 0.0) ? ((1.0 - abs(p.yx)) * signNotZero(p)) : p;
//}

// 'A Survey of Efficient Representations for Independent Unit Vectors', listing 5.
//vec3 snorm12x2_to_unorm8x3(vec2 f) {
//	vec2 u = vec2(round(clamp(f, -1.0, 1.0) * 2047.0 + 2047.0));
//	float t = floor(u.y / 256.0);
//	// If storing to GL_RGB8UI, omit the final division
//	return floor(vec3(u.x / 16.0,
//		fract(u.x / 16.0) * 256.0 + t,
//		u.y - t * 256.0)) / 255.0;
//}




// See 'Calculations for recovering depth values from depth buffer' in OpenGLEngine.cpp
float getDepthFromDepthTextureOrthographic(float px, float py)
{
	float z_01 = texture(main_depth_texture, vec2(px, py)).x;

	float n = near_clip_dist;
	float f = far_clip_dist;
	return (0.5 - z_01)*(f-n) + 0.5*(f+n);

	// TODO: Handle non-USE_REVERSE_Z case
}


float getDepthFromDepthTexture(float px, float py)
{
	return getDepthFromDepthTextureValue(near_clip_dist, texture(main_depth_texture, vec2(px, py)).x);
}


// Returns coords in [0, 1] for visible positions
vec2 cameraToScreenSpace(vec3 pos_cs)
{
	return vec2(
		pos_cs.x / -pos_cs.z * l_over_w + 0.5,
		pos_cs.y / -pos_cs.z * l_over_h + 0.5
	);
}


// Returns spectral radiance from refracted_hitpos_ws towards camera.
vec3 colourForUnderwaterPoint(vec3 refracted_hitpos_ws, float refracted_px, float refracted_py, float final_refracted_water_ground_d, float water_to_ground_sun_d)
{
	//-----------------
	vec3 extinction = vec3(1.0, 0.10, 0.1) * 2.0;
	vec3 scattering = vec3(0.4, 0.4, 0.1);

	vec3 src_col = texture(main_colour_texture, vec2(refracted_px, refracted_py)).xyz; // Get colour value at refracted ground position.
//return src_col;
	//vec3 src_normal_encoded = texture(main_normal_texture, vec2(refracted_px, refracted_py)).xyz; // Encoded as a RGB8 texture (converted to floating point)
	//vec3 src_normal_ws = oct_to_float32x3(unorm8x3_to_snorm12x2(src_normal_encoded)); // Read normal from normal texture

	//--------------- Apply caustic texture ---------------
	// Caustics are projected onto a plane normal to the direction to the sun.
//	vec3 sun_right = normalize(cross(sundir_ws.xyz, vec3(0,0,1)));
//	vec3 sun_up = cross(sundir_ws.xyz, sun_right);
//	vec2 hitpos_sunbasis = vec2(dot(refracted_hitpos_ws, sun_right), dot(refracted_hitpos_ws, sun_up));
//
//	float sun_lambert_factor = max(0.0, dot(src_normal_ws, sundir_ws.xyz));
//
//	float caustic_depth_factor = 0.03 + 0.9 * (smoothstep(0.1, 2.0, water_to_ground_sun_d) - 0.8 *smoothstep(2.0, 8.0, water_to_ground_sun_d)); // Caustics should not be visible just under the surface.
//	float caustic_frac = fract(time * 24.0); // Get fraction through frame, assuming 24 fps.
//	float scale_factor = 1.0; // Controls width of caustic pattern in world space.
//	// Interpolate between caustic animation frames
//	vec3 caustic_val = mix(texture(caustic_tex_a, hitpos_sunbasis * scale_factor),  texture(caustic_tex_b, hitpos_sunbasis * scale_factor), caustic_frac).xyz;

	// Since the caustic is focused light, we should dim the src texture slightly between the focused caustic light areas.
//	src_col *= mix(vec3(1.0), vec3(0.3, 0.5, 0.7) + vec3(3.0, 1.0, 0.8) * caustic_val * 7.0, caustic_depth_factor * sun_lambert_factor);

	// TODO: compute inscatter_radiance better.
	// It should depend on the sun+sky colour, but also take into account attenuation through water giving a blue tint.
	vec3 inscatter_radiance_sigma_s_over_sigma_t = sun_and_sky_av_spec_rad.xyz * vec3(0.004, 0.015, 0.03) * 3.0;
	vec3 exp_optical_depth = exp(extinction * -final_refracted_water_ground_d/*100.f*/); // TEMP HACK IMPORTANT
	vec3 inscattering = inscatter_radiance_sigma_s_over_sigma_t * (vec3(1.0) - exp_optical_depth);

	vec3 attentuated_col = src_col * exp_optical_depth;

	return attentuated_col + inscattering;
}




// The total per-axis slope variance of all 200 wave components, e.g. the value that resolved_slope_var below takes
// when k_nyquist is large enough that every component is resolved.  It depends only on hash() and the amplitude
// formula in waterNormalWS(), so it is just a constant.  Computed by evaluating
//     sum over i of ((a_i*k_i.x)^2 + (a_i*k_i.y)^2) / 4
// offline with the same hash; recompute it if either the hash or the amplitude formula changes.
// The rms slope it corresponds to is 0.0157, e.g. just under a degree.
const float TOTAL_SLOPE_VAR = 0.000244974378;


// Sums the wave components that this pixel is able to resolve, e.g. those whose wavelength is more than about twice
// the width of the pixel's footprint on the water.  Returns the resulting (unnormalised) normal.
//
// The components above that limit are deliberately not summed.  They are not dropped either: their combined per-axis
// slope variance is returned in resolved_slope_var_out (as the part of TOTAL_SLOPE_VAR that *was* resolved), so that
// main() can put them back stochastically, one Gaussian draw per sub-sample rather than more sin() calls.
// That works because those components are, by definition, at effectively uncorrelated phase from one sub-pixel
// position to the next, so their sum over the footprint is Gaussian by the central limit theorem.
//
// NOTE: contains no derivative operations, so is safe to call from non-uniform control flow.
vec3 waterNormalWS(vec2 pos_xy, vec3 base_normal_ws, float k_nyquist, out float resolved_slope_var_out)
{
	vec3 normal_out_ws = base_normal_ws;
	float resolved_slope_var = 0.0;

	float k_len = 0.2;
	for(int i=0; i<200; ++i)
	{
		// f(x) = a sin(k.(x,y) - omega*t)
		// f(x) = a sin(k_x*x + k_y*y)
		// df/dx = a k_x cos(k_x*x + k_y*y)
		// df/dy = a k_y cos(k_x*x + k_y*y)

		// |k| <= k_len * sqrt(2)/2 (see the construction of k below), and k_len only increases, so once this upper
		// bound passes the Nyquist limit, every remaining component is unresolvable and we can stop.
		if(k_len * 0.70710678 > k_nyquist)
			break;

		float a = 0.02  * pow(max(1.0, k_len), -1.5);
		if(k_len > 50.0)
			a *= 0.2;

		vec2 k = vec2(
			-0.5 + hash(uvec2(uint(i), 0)),
			-0.5 + hash(uvec2(uint(i), 1))
		) * k_len;
		float k_mag = length(k);

		// Fade the component out as it approaches the limit, rather than dropping it abruptly, so that the handover
		// to the stochastic part is smooth as the camera moves.
		float window = 1.0 - smoothstep(0.5, 1.0, k_mag / k_nyquist);

		float omega = sqrt(9.8 * k_mag); // Deep water dispersion relation.
		vec2 df_dxy = (a * window) * k * cos(dot(k, pos_xy) - omega * time);

		normal_out_ws.x -= df_dxy.x;
		normal_out_ws.y -= df_dxy.y;

		// Slope variance this component accounts for.  The x slope is a*window*k_x*cos(phase), whose variance over the
		// phase is (a*window*k_x)^2 / 2, and likewise for y; take the mean of the two axes for an isotropic estimate.
		resolved_slope_var += square(window) * (square(a * k.x) + square(a * k.y)) * 0.25;

		k_len += 0.3;
	}

	resolved_slope_var_out = resolved_slope_var;
	return normal_out_ws;
}


// Perturbs a water normal by a random slope drawn from the unresolved part of the wave spectrum.  slope_var is the
// per-axis variance of that part; the slope distribution of a sum of many uncorrelated components is Gaussian, and
// this is the Box-Muller transform of two uniform randoms into an isotropic 2D Gaussian slope.
// Note that this is also exactly Beckmann NDF importance sampling, since the Beckmann slope distribution is Gaussian.
vec3 perturbNormalBySlope(vec3 normal_ws, float slope_var, float u1, float u2)
{
	float r = sqrt(max(0.0, -2.0 * slope_var * log(max(u1, 1.0e-7))));
	float theta = u2 * (2.0 * PI);

	normal_ws.x -= r * cos(theta);
	normal_ws.y -= r * sin(theta);
	return normal_ws;
}


// Returns the spectral radiance (* 1.0e-9) arriving from the environment - sky and sun disc - along a direction.
vec3 envReflectedRadiance(vec3 reflected_dir_ws, float roughness)
{
	int map_lower = int(roughness * 6.9999);
	int map_higher = map_lower + 1;
	float map_t = roughness * 6.9999 - float(map_lower);

	float refl_theta = fastApproxACos(reflected_dir_ws.z);
	float refl_phi = fastApproxAtan(reflected_dir_ws.y, reflected_dir_ws.x) - env_phi; // -1.f is to rotate reflection so it aligns with env rotation.
	vec2 refl_map_coords = vec2(refl_phi * (1.0 / PI), clamp(refl_theta * (1.0 / PI), 1.0 / 64.0, 1.0 - 1.0 / 64.0)); // Clamp to avoid texture coord wrapping artifacts.

	vec3 spec_refl_light_lower  = texture(specular_env_tex, vec2(refl_map_coords.x, float(map_lower)  * (1.0/8.0) + refl_map_coords.y * (1.0/8.0))).xyz;
	vec3 spec_refl_light_higher = texture(specular_env_tex, vec2(refl_map_coords.x, float(map_higher) * (1.0/8.0) + refl_map_coords.y * (1.0/8.0))).xyz;
	vec3 spec_refl_light = spec_refl_light_lower * (1.0 - map_t) + spec_refl_light_higher * map_t; // spectral radiance * 1.0e-9

	//-------------- sun ---------------------
	float d = dot(sundir_ws.xyz, reflected_dir_ws);

	float sunscale = 0.15;
	const float sun_solid_angle = 0.00006780608; // See SkyModel2Generator::makeSkyEnvMap();
	vec3 suncol = sun_spec_rad_times_solid_angle.xyz * (1.0 / sun_solid_angle) * sunscale;

	spec_refl_light = mix(spec_refl_light, suncol, smoothstep(0.99997, 0.9999892083461507, d));

	//-------------- clouds ---------------------
	// NOTE: the clouds are evaluated per sample, not once for the pixel centre direction.  getCloudFrac() intersects
	// cloud planes at 1000m and 6000m, and near the horizon that intersection distance becomes enormous, so a tiny
	// change in the reflected direction slides the sample point across the cloud plane by a long way.  Cloud
	// reflections therefore alias at grazing angles just like everything else here, and being higher contrast than
	// the sky gradient they alias more visibly.
	vec2 cloudfrac_cumulus_edge = getCloudFrac(pos_ws, reflected_dir_ws, time, fbm_tex, cirrus_tex);
	float cloudfrac    = cloudfrac_cumulus_edge.x;
	float cumulus_edge = cloudfrac_cumulus_edge.y;

	vec3 cloudcol = sun_and_sky_av_spec_rad.xyz;
	spec_refl_light = mix(spec_refl_light, cloudcol, max(0.f, cloudfrac));
	vec3 suncloudcol = cloudcol * 2.5;
	float blend = max(0.f, cumulus_edge) * pow(max(0.0, d), 32.0);// smoothstep(0.9, 0.9999892083461507, d);
	spec_refl_light = mix(spec_refl_light, suncloudcol, blend);

	return spec_refl_light;
}


#if WATER_DO_SCREENSPACE_REFL_AND_REFR
// Walks the reflected ray through the depth buffer looking for an intersection with the scene.  Returns true if one
// was found, in which case hit_col_out is set to the (already fogged) colour at the intersection.
//
// Split out of main() so that it can be run once per sub-sample of the wave normal - see the multisampling loop in
// main().  Reflections of scene geometry go through here rather than through the environment map, so if this is not
// inside that loop then everything the trace hits gets shaded from a single unperturbed normal, and reflections of
// terrain and buildings come out mirror sharp at any distance.
//
// Reads the fragment's camera space position from the pos_cs varying.
bool traceScreenSpaceRefl(vec3 reflected_dir_ws, out vec3 hit_col_out)
{
	hit_col_out = vec3(0.0);
	bool hit_something = false;

	// First get dir in screen space.
	/*
	suppose we have a point in camera space along some parameterised line:
	p_cs = o_cs + t_1 d_cs

	and a function f that projects a point in camera space onto screen space:
	f(p) = (p_x/-p_z l/w + 1/2, p_y/-p_z l/w w/h + 1/2)

	then the projected point in scren space is
	p_ss = f(p_cs) = f(o_cs + t_1 d_cs)

	and its x coordinate is
	p_ss_x = (o_cs_x + t_1 d_cs_x)/-(o_cs_z + t_1 d_cs_z) l/w

	Solving for t_1:
	t_1 = (o_cs_z (p_ss_x - 1/2)  +  o_cs_x l/w) / (d_cs_z (-p_ss_x + 1/2) - d_cs_x l/w)

	singularity when
	d_cs_z (-p_ss_x + 1/2) - d_cs_x l/w = 0
	*/

	vec2 o_ss = cameraToScreenSpace(pos_cs); // Get current fragment screen space position

	// Get a point some distance along the reflected dir, in world space, and transform to camera space.
	vec3 dir_cs = (frag_view_matrix * vec4(reflected_dir_ws, 0.0)).xyz; // view matrix shouldn't change lengths so don't need to normalise
	vec3 advanced_pos_cs = pos_cs + dir_cs;
	vec2 advanced_p_ss = cameraToScreenSpace(advanced_pos_cs);

	vec2 dir_ss = normalize(advanced_p_ss - o_ss); // Compute normalized dir in screen space

	// Have a minimum intersection depth, to avoid rays intersecting with objects in the foreground (closer to the camera than the fragment), for example avatars.
	float intersection_depth_threshold = -pos_cs.z;

	// Solve for t_1 using x or y coordinates, which ever one changes faster.
	float o_ss_xy, d_ss_xy, l_over_w_factor, o_cs_xy, d_cs_xy;
	if(abs(dir_ss.x) > abs(dir_ss.y))
	{
		o_ss_xy = o_ss.x;
		d_ss_xy = dir_ss.x;
		l_over_w_factor = l_over_w;
		o_cs_xy = pos_cs.x;
		d_cs_xy = dir_cs.x;
	}
	else
	{
		o_ss_xy = o_ss.y;
		d_ss_xy = dir_ss.y;
		l_over_w_factor = l_over_h;
		o_cs_xy = pos_cs.y;
		d_cs_xy = dir_cs.y;
	}

	// Now walk along it
	int MAX_STEPS = 64;
	float step_t = 0.004;
	float prev_t = 0.0;
	float t = -1.0;
	for(int i=1; i<MAX_STEPS; ++i)
	{
		step_t += 0.00008;
		t = float(i) * step_t; // TODO: use += instead of *

		vec2  cur_ss  = o_ss    + dir_ss  * t; // Compute current screen space position
		float p_ss_xy = o_ss_xy + d_ss_xy * t;

		if(p_ss_xy < 0.0 || p_ss_xy > 1.0)
			break; // We walked off the screen
		float t_1 =  (pos_cs.z*(p_ss_xy - 0.5) + o_cs_xy * l_over_w_factor) / (dir_cs.z*(-p_ss_xy + 0.5) - d_cs_xy * l_over_w_factor); // Solve for distance t_1 along camera space ray
		if(t_1 < 0.0)
			t_1 = 100000.0;

		float p_cs_z = pos_cs.z + dir_cs.z * t_1; // Z coordinate of point on camera-space ray that projects onto the current screen space point
		float cur_step_depth = -p_cs_z;

		// Get depth at screen space point
		float cur_depth_buf_depth = getDepthFromDepthTexture(cur_ss.x, cur_ss.y); // Get depth from depth buffer for current step position

		if((cur_step_depth > cur_depth_buf_depth)  && (cur_depth_buf_depth > intersection_depth_threshold)) // if we hit something:
		{
			hit_something = true;
			break;
		}

		prev_t = t;
		intersection_depth_threshold = cur_step_depth * 0.990;//cur_step_depth - 0.1;
	}

	if(hit_something)
	{
		// Binary search to refine hit
		float lower_t = prev_t; // Lower bound of screen-space line interval to search
		float upper_t = t;      // Upper bound

		for(int i=0; i<4; ++i)
		{
			t = (lower_t + upper_t) * 0.5f;
			float p_ss_xy = o_ss_xy + d_ss_xy * t;
			float t_1 =  (pos_cs.z*(p_ss_xy - 0.5) + o_cs_xy * l_over_w_factor) / (dir_cs.z*(-p_ss_xy + 0.5) - d_cs_xy * l_over_w_factor);
			float p_cs_z = pos_cs.z + dir_cs.z * t_1; // Z coordinate of point on camera-space ray that projects onto the current screen space point
			float midpoint_depth = -p_cs_z;
			vec2 cur_ss = o_ss + dir_ss * t;
			float cur_depth_buf_depth = getDepthFromDepthTexture(cur_ss.x, cur_ss.y); // Get depth from depth buffer for current step position
			if(midpoint_depth < cur_depth_buf_depth)
				lower_t = t; // Intersection lies in upper half of interval, update interval to be the upper half of previous interval.
			else
				upper_t = t; // Intersection lies in lower half of interval
		}

		// Take the final point as the midpoint (in screen space) of the interval in which the intersection lies
		t = (lower_t + upper_t) * 0.5f;
		vec2 cur_ss = o_ss + dir_ss * t;
		hit_col_out = texture(main_colour_texture, cur_ss).xyz;
	}

	return hit_something;
}
#endif // end if WATER_DO_SCREENSPACE_REFL_AND_REFR


void main()
{
	vec2 use_texture_coords = vec2(0, 0);

	//vec3 frag_to_cam = normalize(-pos_cs);

	//vec3 sunrefl_h = normalize(frag_to_cam + sundir_cs.xyz);
	//float sunrefl_h_cos_theta = abs(dot(sunrefl_h, unit_normal_cs));
	float roughness = 0.01;
	//float fresnel_scale = 1.0;
	//float sun_specular = trowbridgeReitzPDF(sunrefl_h_cos_theta, max(1.0e-8f, alpha2ForRoughness(roughness))) * 
	//	fresnel_scale * fresnellApprox(sunrefl_h_cos_theta, ior);


	// waves
	vec3 unit_normal_ws = normalize(normal_ws);

	float deriv = length(dFdx(pos_ws));
	float sin_window = 1.0 - smoothstep(0.0, 0.04, deriv);

	float fbm_window = 1.0 - smoothstep(0.0, 0.2, deriv);

	// Work out the world space width of this pixel's footprint on the water surface.
	// NOTE: both derivatives are needed.  Near the horizon the dFdy footprint is orders of magnitude larger than the
	// dFdx one, so using dFdx alone (as `deriv` above does) drastically underestimates the footprint exactly where the
	// worst aliasing is.  Taking the max over-blurs across the view direction, since the true footprint is very
	// anisotropic at grazing angles, but is conservative.
	vec2 dpos_dx = dFdx(pos_ws.xy);
	vec2 dpos_dy = dFdy(pos_ws.xy);
	float footprint_w = max(length(dpos_dx), length(dpos_dy));

	// A wave component of wavenumber k has wavelength 2pi/k, and this pixel can only resolve it if that wavelength is
	// more than about twice the footprint width, e.g. if k < pi / footprint_w.
	float k_nyquist = PI / max(footprint_w, 1.0e-5);

	// Sum the components this pixel can resolve.  This happens once, no matter how many samples are taken below.
	float resolved_slope_var;
	unit_normal_ws = waterNormalWS(pos_ws.xy, unit_normal_ws, k_nyquist, resolved_slope_var);

	// Per-axis slope variance of everything the sum above left out.  The sub-samples below put this back stochastically.
	float unresolved_slope_var = max(0.0, TOTAL_SLOPE_VAR - resolved_slope_var);


	//unit_normal_ws.y += (fbmMix(pos_ws.xy * 0.1 + vec2(0, -time * 0.1), fbm_tex) * 0.04 + sin(dot(pos_ws.xy, vec2(0.6, 0.3)) * 10.0 + time * 2.0) * 0.003 + sin(pos_ws.y * 20.0 + -time * 2.0) * 0.04) * sin_window;
	//unit_normal_ws.x += sin(dot(pos_ws.xy, vec2(0.2, 0.3)) * 10.0 + time * 2.0) * 0.003 + sin(pos_ws.y * 10.0 + -time * 2.0) * 0.002;

	//unit_normal_ws.x += (fbmMix(pos_ws.xy * 0.1, fbm_tex) * 0.01 + fbmMix(pos_ws.xy * 0.01, fbm_tex) * 0.01) * fbm_window;

//	unit_normal_ws = normalize(unit_normal_ws);
	
	if(dot(unit_normal_ws, cam_to_pos_ws) > 0.0)
		unit_normal_ws = -unit_normal_ws;

	// Decide how many sub-samples to take of the unresolved part of the spectrum.  Where the pixel resolves the whole
	// spectrum there is nothing left to sample and this stays at 1, so water close to the camera behaves and costs
	// exactly as it did before.  The count is capped because at the horizon the footprint is effectively unbounded and
	// no practical number of samples fully resolves it: past that point extra samples only cut the residual noise by
	// sqrt(N).
	// NOTE: each sample now runs its own screen space ray march.  Raise it for less noise, lower it for speed. 
	const int MAX_WATER_SAMPLES = 4;
	float unresolved_frac = unresolved_slope_var * (1.0 / TOTAL_SLOPE_VAR); // In [0, 1]
	int num_samples = 1 + int(float(MAX_WATER_SAMPLES - 1) * smoothstep(0.0, 0.6, unresolved_frac) + 0.5);

	// Decorrelate the sample sequence between neighbouring pixels, so that what noise remains looks like noise rather
	// than a moire pattern banding across the water.
	float water_sample_seed = texture(blue_noise_tex, gl_FragCoord.xy * (1.0 / 64.0)).x;

	vec3 unit_cam_to_pos_ws = normalize(cam_to_pos_ws);

	//ivec2 tex_res = textureSize(main_colour_texture, 0);
	//float width_over_height = float(tex_res.x) / float(tex_res.y);

	vec3 col = vec3(0.0); // spectral radiance * 1.0e-9
	vec3 spec_refl_light_already_fogged = vec3(0.0); // spectral radiance * 1.0e-9
	vec3 spec_refl_light = vec3(0.0); // spectral radiance * 1.0e-9
	float spec_refl_fresnel = 0.0; // Fresnel reflactance
	bool hit_point_under_water = false;
	if(unit_cam_to_pos_ws.z > 0.0) // If the camera is under the water (TEMP: assuming water is flat horizontal plane)
	{
		vec3 I = unit_cam_to_pos_ws;
		vec3 N = unit_normal_ws;
		float eta = 1.3;
		float k = 1.0 - eta * eta * (1.0 - square(dot(N, I)));
		if (k < 0.0)
		{
			// Total internal reflection

			// image coordinates of this fragment
			float px = pos_cs.x / -pos_cs.z * l_over_w + 0.5;
			float py = pos_cs.y / -pos_cs.z * l_over_h + 0.5;

			float water_dist = -pos_cs.z;
			float ground_dist = getDepthFromDepthTexture(px, py); // Get depth from depth buffer.

			float depth = max(0.0, ground_dist - water_dist);

			vec3 reflected_dir_ws = I - N * (2.0 * dot(N, I));

			// Step through water, projecting back onto depth buffer, and looking for an intersection with the world surface, as defined by the depth buffer.
			int MAX_STEPS = 64;
			float step_d = 0.01; // Start with a small step distance, increase it slightly each step.
			float cur_d = step_d;

			float refracted_px = px; // Tex coords of point where refracted ray hits ground
			float refracted_py = py;
			float prev_penetration_depth = 0.0;
			vec3 refracted_hitpos_ws = pos_ws; // World space position where refracted ray hits ground
			bool hit_ground = false;
			for(int i=0; i<MAX_STEPS; ++i)
			{
				vec3 cur_pos_ws = pos_ws + reflected_dir_ws * cur_d; // Current step position = fragment position + refraction vector * dist along refraction vector

				// Transform current step position into cam space.
				vec3 projected_cur_pos_cs = (frag_view_matrix * vec4(cur_pos_ws, 1.0)).xyz;

				// get depth texture coords for the current step position
				float cur_px = projected_cur_pos_cs.x / -projected_cur_pos_cs.z * l_over_w + 0.5;
				float cur_py = projected_cur_pos_cs.y / -projected_cur_pos_cs.z * l_over_h + 0.5;

				float cur_depth = -projected_cur_pos_cs.z;

				float cur_depth_buf_depth = getDepthFromDepthTexture(cur_px, cur_py); // Get depth from depth buffer for current step position

				float penetration_depth = cur_depth - cur_depth_buf_depth;

				if(penetration_depth > 0.0) // We have hit something
				{
					// If the ray penetrated the surface too far, then it indicates we are 'wrapping around' an object in the foreground.  So stop tracing and use the previous position.
					if(penetration_depth > step_d * 5.0)
					{}
					else
					{
						// Solve for approximate distance along ray where we intersect surface.
						float frac = -prev_penetration_depth / (penetration_depth - prev_penetration_depth); // frac = -prev_penetration_depth / (-prev_penetration_depth + penetration_depth);
						float prev_d = cur_d - step_d;
						float intersect_d = mix(prev_d, cur_d, frac);

						cur_pos_ws = pos_ws + reflected_dir_ws * intersect_d; // Current step position = fragment position + refraction vector * dist along refraction vecgor

						// Transform current step position into cam space.
						projected_cur_pos_cs = (frag_view_matrix * vec4(cur_pos_ws, 1.0)).xyz;

						// get depth texture coords for the current step position
						refracted_px = projected_cur_pos_cs.x / -projected_cur_pos_cs.z * l_over_w + 0.5;
						refracted_py = projected_cur_pos_cs.y / -projected_cur_pos_cs.z * l_over_h + 0.5;
						refracted_hitpos_ws = cur_pos_ws;
					}

					hit_ground = true;
					break;
				}

				refracted_px = cur_px;
				refracted_py = cur_py;
				refracted_hitpos_ws = cur_pos_ws;

				step_d += 0.015; // NOTE: increment step_d first, before adding to cur_d, as need to know the cur_d that was last added to step_d when solving for this position above.
				cur_d += step_d;
				prev_penetration_depth = penetration_depth;
			}

			float final_ground_dist = ground_dist; // getDepthFromDepthTexture(refracted_px, refracted_py); // Get depth from depth buffer.

			float use_ground_cam_depth = getDepthFromDepthTexture(refracted_px, refracted_py);
			
			// Distance from water surface to ground, along the refracted ray path.  Used for optical depth computation for water colour etc.
			float final_refracted_water_ground_d = hit_ground ? max(0.0, use_ground_cam_depth - water_dist) : 1.0e10;

			// Distance from water surface to ground, along the sun direction.  Used for computing the caustic effect envelope.
			float water_to_ground_sun_d = hit_ground ? ((pos_ws.z - refracted_hitpos_ws.z) / sundir_ws.z) : 1.0e10;

			//vec3 src_col = texture(main_colour_texture, vec2(refracted_px, refracted_py)).xyz * (1.0 / 0.000000003); // Get colour value at refracted ground position, undo tonemapping.
			//col = src_col;

			// vec3 colourForUnderwaterPoint(vec3 refracted_hitpos_ws, float refracted_px, float refracted_py, float final_refracted_water_ground_d, float water_to_ground_sun_d)
			col = colourForUnderwaterPoint(refracted_hitpos_ws, refracted_px, refracted_py, final_refracted_water_ground_d, water_to_ground_sun_d);
			
		}
		else
		{
			vec3 refracted_dir_ws = eta * I - (eta * dot(N, I) + sqrt(k)) * N;

			vec3 refracted_dir_cs =  (frag_view_matrix * vec4(refracted_dir_ws, 0.0)).xyz;

			float px = refracted_dir_cs.x / -refracted_dir_cs.z * l_over_w + 0.5;
			float py = refracted_dir_cs.y / -refracted_dir_cs.z * l_over_h + 0.5;

			vec3 src_col = texture(main_colour_texture, vec2(px, py)).xyz; // Get colour value at refracted ground position.

			col = src_col;
		}

	}
	else // Else if cam is above water surface:
	{
		// Reflect cam-to-fragment vector in ws normal
		float unit_cam_to_pos_ws_dot_normal_ws = dot(unit_normal_ws, unit_cam_to_pos_ws);
		vec3 reflected_dir_ws = unit_cam_to_pos_ws - unit_normal_ws * (2.0 * unit_cam_to_pos_ws_dot_normal_ws);

		if(reflected_dir_ws.z < 0.0)
			reflected_dir_ws.z = 0.05;


		vec3 refracted_dir_ws = refract(unit_cam_to_pos_ws, unit_normal_ws, 1.0 / 1.33);

		// Vectors on plane orthogonal to incident direction
		vec3 right_ws = normalize(cross(unit_cam_to_pos_ws, vec3(0,0,1)));
		vec3 up_ws = cross(right_ws, unit_cam_to_pos_ws);

		refracted_dir_ws = normalize(refracted_dir_ws - up_ws * dot(refracted_dir_ws, up_ws)); // Remove up/down component of refraction in plane orthogonal to incident direction


		//========================= Multisampled reflection =============================
		// Take several sub-samples of the reflection, each with its own perturbed normal, and average the radiance.
		vec3 env_refl_sum = vec3(0.0);   // Sum of fresnel * env map radiance over the samples that missed.
		vec3 ssr_refl_sum = vec3(0.0);   // Sum of fresnel * scene colour over the samples that hit.  Already fogged.
		float env_fresnel_sum = 0.0;     // Sum of fresnel over the samples that missed.  Needed by the cloud and aurora terms below.
		float num_env_samples = 0.0;     // Number of samples that missed.
		float fresnel_sum = 0.0;         // Sum of fresnel over all samples.  Only used for the transmitted (underwater) part.

		for(int s=0; s<num_samples; ++s)
		{
			// Stratified in u1 so the samples cover the distribution evenly, offset per pixel to decorrelate
			// neighbours.  The golden ratio increment for u2 keeps the angles well spread for any sample count.
			float u1 = (float(s) + water_sample_seed) * (1.0 / float(num_samples));
			float u2 = fract(water_sample_seed + float(s) * 0.61803399);

			// Put back the wave components this pixel could not resolve, as a random slope drawn from their
			// distribution.  Where nothing was filtered out - water close to the camera - unresolved_slope_var is zero
			// and this leaves the normal untouched, so the near field behaves exactly as it did before.
			vec3 sample_normal_ws = perturbNormalBySlope(unit_normal_ws, unresolved_slope_var, u1, u2);

			float sample_n_dot_v = dot(sample_normal_ws, unit_cam_to_pos_ws);
			vec3 sample_refl_dir_ws = unit_cam_to_pos_ws - sample_normal_ws * (2.0 * sample_n_dot_v);
			if(sample_refl_dir_ws.z < 0.0)
				sample_refl_dir_ws.z = 0.05;

			float sample_fresnel = dielectricFresnelReflForIOR1_333(max(0.0, -sample_n_dot_v));
			fresnel_sum += sample_fresnel;

			bool sample_hit = false;
			vec3 sample_hit_col = vec3(0.0);
#if WATER_DO_SCREENSPACE_REFL_AND_REFR
			sample_hit = traceScreenSpaceRefl(sample_refl_dir_ws, sample_hit_col);
#endif
			if(sample_hit)
				ssr_refl_sum += sample_hit_col * sample_fresnel;
			else
			{
				env_refl_sum += envReflectedRadiance(sample_refl_dir_ws, roughness) * sample_fresnel;
				env_fresnel_sum += sample_fresnel;
				num_env_samples += 1.0;
			}
		}

		float inv_num_samples = 1.0 / float(num_samples);

		spec_refl_light_already_fogged = ssr_refl_sum * inv_num_samples; // Already Fresnel weighted, see above.

		spec_refl_fresnel = fresnel_sum * inv_num_samples;

		spec_refl_light = env_refl_sum * inv_num_samples;               // (1/N) * sum of fresnel * env radiance.

		//========================= Add aurora ============================
		if(num_env_samples > 0.0)
		{
			float env_fresnel_frac = env_fresnel_sum * inv_num_samples;     // (1/N) * sum of fresnel.

			// NOTE: spec_refl_light is Fresnel weighted, but the aurora radiance added below is not, so it has to be
			// scaled by env_fresnel_frac to be combined with it.

			// NOTE: the aurora below is single sampled, using the pixel centre reflected direction.
			// It is expensive enough to skip multisampling it.

#if DRAW_AURORA
			// NOTE: code duplicated in env_frag_shader
			const float MAX_AURORA_SUNDIR_Z = 0.1;
			if(sundir_ws.z < MAX_AURORA_SUNDIR_Z)
			{
				float aurora_factor = 1.0 - smoothstep(0.0, MAX_AURORA_SUNDIR_Z, sundir_ws.z);

				vec3 dir_ws = reflected_dir_ws;
				vec3 env_campos_ws = pos_ws;

				float min_aurora_z = 1000.0;
				float max_aurora_z = 8000.0;
				float aurora_start_ray_t = rayPlaneIntersect(env_campos_ws, dir_ws, min_aurora_z);
				float aurora_end_ray_t = rayPlaneIntersect(env_campos_ws, dir_ws, max_aurora_z);

				int num_steps = 32;
				float t_step = min(600.0, (aurora_end_ray_t - aurora_start_ray_t) / float(num_steps));
				float pixel_hash = texture(blue_noise_tex, gl_FragCoord.xy * (1.0 / 64.f)).x;
				float t_offset = pixel_hash * t_step;

				vec3 aurora_up = normalize(vec3(0.3, 0.0, 1.0));
				vec3 aurora_forw = normalize(cross(aurora_up, vec3(0,0,1))); // vector along aurora surface
				vec3 aurora_right = cross(aurora_up, aurora_forw);

				vec4 green_col = vec4(0, pow(0.79, 2.2), pow(0.47, 2.2), 0);
				vec4 blue_col  = vec4(0, pow(0.1, 2.2),  pow(0.6, 2.2), 0);

				for(int i=0; i<num_steps; ++i)
				{
					float ray_t = aurora_start_ray_t + t_offset + t_step * float(i);
					vec3 p = env_campos_ws + dir_ws * ray_t;

					vec3 p_as = vec3(500.0 + dot(p, aurora_right), dot(p, aurora_forw), dot(p, aurora_up));

					vec2 st = p_as.xy * 0.0001;
					if(st.x > -1.0 && st.x <= 1.0 && st.y >= -1.0 && st.y <= 1.0)
					{
						vec4 aurora_val = texture(aurora_tex, st);

						float aurora_start_z = 1000.0 + aurora_val.y * 1000.0;
						if(p_as.z >= aurora_start_z)
						{
							// Smoothly start aurora above aurora_start_z
							float z_factor = smoothstep(aurora_start_z, aurora_start_z + 600.0, p_as.z);
				
							// Smoothly decrease intensity as z increases
							float z_ramp_intensity_factor = exp(-(p_as.z - 1200.0) * 0.001);
							float high_freq_intensity_factor = 1.0 + 3.0 * z_ramp_intensity_factor * (aurora_val.y - 0.5);//(1.0 + aurora_val.y * 2.0 * ramp_intensity_factor*ramp_intensity_factor);
							//float ramp_intensity_factor = max(0.0, 1000 / (p_as.z - 1100) - p_as.z * 0.001);
				
							vec4 col_for_height = mix(green_col, blue_col, min(1.0, (p_as.z - aurora_start_z) * (1.0 / 2000.0)));
				
							spec_refl_light += (0.001 * t_step * col_for_height * aurora_val.r * z_ramp_intensity_factor * high_freq_intensity_factor * z_factor).xyz * aurora_factor * env_fresnel_frac;
						}
					}
				}
			}
#endif
		}


		//vec4 transmission_col = vec4(0.05, 0.2, 0.7, 1.0); //  MAT_UNIFORM.diffuse_colour;

		//float sun_vis_factor = 1.0f;//TODO: use shadow mapping to compute this.
		//vec3 sun_light = vec3(1662102582.6479533,1499657101.1924045,1314152016.0871031) * sun_vis_factor; // Sun spectral radiance multiplied by solid angle, see SkyModel2Generator::makeSkyEnvMap().

		//vec4 col = transmission_col*80000000 + spec_refl_light * spec_refl_fresnel + sun_light * sun_specular;
		//spec_refl_light += sun_light * sun_specular;

#if WATER_DO_SCREENSPACE_REFL_AND_REFR

		float px, py; // image coordinates of this fragment
		float ground_dist;
		if(camera_type == CameraType_Perspective)
		{
			px = pos_cs.x / -pos_cs.z * l_over_w + 0.5;
			py = pos_cs.y / -pos_cs.z * l_over_h + 0.5;
			ground_dist = getDepthFromDepthTexture(px, py); // Get depth from depth buffer.
		}
		else // else if camera_type == CameraType_Orthographic || camera_type == CameraType_DiagonalOrthographic:
		{
			px = pos_cs.x * l_over_w + 0.5; // l is just set to 1 for ortho camera, so l_over_w = 1/w
			py = pos_cs.y * l_over_h + 0.5;
			ground_dist = getDepthFromDepthTextureOrthographic(px, py); // Get depth from depth buffer.
		}
		

		float water_dist = -pos_cs.z;
		float depth = max(0.0, ground_dist - water_dist);


	

		// Step through water, projecting back onto depth buffer, and looking for an intersection with the world surface, as defined by the depth buffer.
		int MAX_STEPS = 64;
		float step_d = 0.01; // Start with a small step distance, increase it slightly each step.
		float cur_d = step_d;

		float refracted_px = px; // Tex coords of point where refracted ray hits ground
		float refracted_py = py;
		float prev_penetration_depth = 0.0;
		vec3 refracted_hitpos_ws = pos_ws; // World space position where refracted ray hits ground
		bool hit_ground = false;
		for(int i=0; i<MAX_STEPS; ++i)
		{
			vec3 cur_pos_ws = pos_ws + refracted_dir_ws * cur_d; // Current step position = fragment position + refraction vector * dist along refraction vecgor
		
			// Transform current step position into cam space.
			vec3 projected_cur_pos_cs = (frag_view_matrix * vec4(cur_pos_ws, 1.0)).xyz;

			// get depth texture coords for the current step position
			float cur_px = projected_cur_pos_cs.x / -projected_cur_pos_cs.z * l_over_w + 0.5;
			float cur_py = projected_cur_pos_cs.y / -projected_cur_pos_cs.z * l_over_h + 0.5;

			float cur_depth = -projected_cur_pos_cs.z;

			float cur_depth_buf_depth = getDepthFromDepthTexture(cur_px, cur_py); // Get depth from depth buffer for current step position

			float penetration_depth = cur_depth - cur_depth_buf_depth;

			if(penetration_depth > 0.0) // We have hit something
			{
				// If the ray penetrated the surface too far, then it indicates we are 'wrapping around' an object in the foreground.  So stop tracing and use the previous position.
				if(penetration_depth > step_d * 5.0)
				{}
				else
				{
					// Solve for approximate distance along ray where we intersect surface.
					float frac = -prev_penetration_depth / (penetration_depth - prev_penetration_depth); // frac = -prev_penetration_depth / (-prev_penetration_depth + penetration_depth);
					float prev_d = cur_d - step_d;
					float intersect_d = mix(prev_d, cur_d, frac);
			
					cur_pos_ws = pos_ws + refracted_dir_ws * intersect_d; // Current step position = fragment position + refraction vector * dist along refraction vecgor
			
					// Transform current step position into cam space.
					projected_cur_pos_cs = (frag_view_matrix * vec4(cur_pos_ws, 1.0)).xyz;
			
					// get depth texture coords for the current step position
					refracted_px = projected_cur_pos_cs.x / -projected_cur_pos_cs.z * l_over_w + 0.5;
					refracted_py = projected_cur_pos_cs.y / -projected_cur_pos_cs.z * l_over_h + 0.5;
					refracted_hitpos_ws = cur_pos_ws;
				}

				hit_ground = true;
				break;
			}

			refracted_px = cur_px;
			refracted_py = cur_py;
			refracted_hitpos_ws = cur_pos_ws;

			step_d += 0.015; // NOTE: increment step_d first, before adding to cur_d, as need to know the cur_d that was last added to step_d when solving for this position above.
			//step_d *= 1.08;
			cur_d += step_d;
			prev_penetration_depth = penetration_depth;
		}

		float final_ground_dist = ground_dist; // getDepthFromDepthTe xture(refracted_px, refracted_py); // Get depth from depth buffer.
	
		//float depth = max(0.0, final_ground_dist - water_dist); // Depth from water surface to ground, used for optical depth computation for water colour etc.
		//float final_water_ground_d = max(0.0, (pos_ws.z - refracted_hitpos_ws.z) * 1.3); // max(0.0, final_ground_dist - water_dist);
		// The correct value would be cur_d, but that suffers from artifacts when we stop tracing due to wrapping around objects.  So use this value instead.

		//vec3 unrefracted_ground_pos_ws = /*pos_ws + */unit_cam_to_pos_ws * ground_dist;
	

		float use_ground_cam_depth = getDepthFromDepthTexture(refracted_px, refracted_py);

		//float final_refracted_water_ground_d = max(0.0, pos_ws.z - unrefracted_ground_pos_ws.z);
	
		// Distance from water surface to ground, along the refracted ray path.  Used for optical depth computation for water colour etc.
		float final_refracted_water_ground_d = hit_ground ? max(0.0, use_ground_cam_depth - water_dist) : 1.0e10;

		//float final_water_ground_d = use_depth * abs(unit_cam_to_pos_ws.z); 
	
		// Distance from water surface to ground, along the sun direction.  Used for computing the caustic effect envelope.
		float water_to_ground_sun_d = hit_ground ? ((pos_ws.z - refracted_hitpos_ws.z) / sundir_ws.z) : 1.0e10;




	//	vec3 extinction = vec3(1.0, 0.10, 0.1) * 2;
	//	vec3 scattering = vec3(0.4, 0.4, 0.1);
	//
	//	vec3 src_col = texture2D(main_colour_texture, vec2(refracted_px, refracted_py)).xyz * (1.0 / 0.000000003); // Get colour value at refracted ground position, undo tonemapping.
	//
	//	vec3 src_normal_encoded = texture2D(main_normal_texture, vec2(refracted_px, refracted_py)).xyz; // Encoded as a RGB8 texture (converted to floating point)
	//	vec3 src_normal_ws = oct_to_float32x3(unorm8x3_to_snorm12x2(src_normal_encoded)); // Read normal from normal texture
	//
	//	//--------------- Apply caustic texture ---------------
	//	// Caustics are projected onto a plane normal to the direction to the sun.
	//	vec3 sun_right = normalize(cross(sundir_ws.xyz, vec3(0,0,1)));
	//	vec3 sun_up = cross(sundir_ws.xyz, sun_right);
	//	vec2 hitpos_sunbasis = vec2(dot(refracted_hitpos_ws, sun_right), dot(refracted_hitpos_ws, sun_up));
	//
	//	float sun_lambert_factor = max(0.0, dot(src_normal_ws, sundir_ws.xyz));
	//
	//	float caustic_depth_factor = 0.03 + 0.9 * (smoothstep(0.1, 2.0, water_to_ground_sun_d) - 0.8 *smoothstep(2.0, 8.0, water_to_ground_sun_d)); // Caustics should not be visible just under the surface.
	//	float caustic_frac = fract(time * 24.0); // Get fraction through frame, assuming 24 fps.
	//	float scale_factor = 1.0; // Controls width of caustic pattern in world space.
	//	// Interpolate between caustic animation frames
	//	vec3 caustic_val = mix(texture2D(caustic_tex_a, hitpos_sunbasis * scale_factor),  texture2D(caustic_tex_b, hitpos_sunbasis * scale_factor), caustic_frac).xyz;
	//
	//	// Since the caustic is focused light, we should dim the src texture slightly between the focused caustic light areas.
	//	src_col *= mix(vec3(1.0), vec3(0.3, 0.5, 0.7) + vec3(3.0, 1.0, 0.8) * caustic_val * 7.0, caustic_depth_factor * sun_lambert_factor);
	//
	//	vec3 inscatter_radiance_sigma_s_over_sigma_t = vec3(1000000.0, 10000000.0, 30000000.0);
	//	vec3 exp_optical_depth = exp(extinction * -final_refracted_water_ground_d);
	//	vec3 inscattering = inscatter_radiance_sigma_s_over_sigma_t * (vec3(1.0) - exp_optical_depth);
	//
	//	vec3 attentuated_col = src_col * exp_optical_depth;
	//
	//	col = //attentuated_col + inscattering;
	//		(attentuated_col + inscattering) * (1.0 - spec_refl_fresnel) +
	//		spec_refl_light                  * spec_refl_fresnel;

		// Handle water depth calculations (in hacky way) for ortho camera types.
		if(camera_type == CameraType_Orthographic || camera_type == CameraType_DiagonalOrthographic)
		{
			final_refracted_water_ground_d = depth;
			water_to_ground_sun_d = depth;
			refracted_px = px;
			refracted_py = py;
		}


		vec3 underwater_col = colourForUnderwaterPoint(refracted_hitpos_ws, refracted_px, refracted_py, final_refracted_water_ground_d, water_to_ground_sun_d);
#else // else if !WATER_DO_SCREENSPACE_REFL_AND_REFR:
		vec3 underwater_col = vec3(0.004, 0.015, 0.03);
#endif

		col = underwater_col * (1.0 - spec_refl_fresnel) +
			spec_refl_light;

	} // End if cam is above water surface


#if DEPTH_FOG
	// Blend with background/fog colour
	float dist_ = max(0.0, -pos_cs.z); // Max with 0 avoids bright artifacts on horizon.
	vec3 transmission = exp(air_scattering_coeffs.xyz * -dist_);

	col.xyz *= transmission;
	col.xyz += sun_and_sky_av_spec_rad.xyz * (1.0 - transmission);
#endif

	col += spec_refl_light_already_fogged; // Already Fresnel weighted per sample, see the multisampling loop above.



	//TEMP
	//vec2 o_ss = cameraToScreenSpace(pos_cs); // Get current fragment screen space position
	//col = vec3(getDepthFromDepthTexture(o_ss.x, o_ss.y)) * 0.001;

#if DO_POST_PROCESSING
	colour_out = vec4(col, 1.f);
#else
	colour_out = vec4(toneMapToNonLinear(col.xyz), 1.0);
#endif

	normal_out = snorm12x2_to_unorm8x3(float32x3_to_oct(unit_normal_ws));
}
