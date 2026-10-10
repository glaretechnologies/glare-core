/*=====================================================================
water_wave_utils.glsl
---------------------
Copyright Glare Technologies Limited 2025 -

The water wave spectrum.  Appended to both water_vert_shader.glsl and water_frag_shader.glsl, so that the two
shaders are summing one definition of the surface and cannot drift apart.

The surface is z = sum over i of a_i sin(k_i . (x,y) - omega_i t), 200 components.  The water mesh is tessellated
(TerrainSystem.cpp in substrata builds it from the terrain quadtree), so the split of that sum is:

  * the vertex shader displaces the mesh by the components the mesh is fine enough to carry, and computes the
    matching normal analytically,
  * the fragment shader adds the rest - the components finer than the mesh - to the shading normal.

waterWaveSum() takes the band of the spectrum to sum, so neither shader drops a component and neither counts one
twice.  Both get the split point from waterGeomCutoffK().

NOTE: PI and square() live in frag_utils.glsl, which the vertex shader does not get, so this file uses WATER_PI
and writes products out.
=====================================================================*/


#define WATER_PI 3.1415926535897932384626433832795


// Component amplitude in metres is WATER_WAVE_AMPLITUDE_SCALE * k_len^-1.5.
//
// Note that there is no lower clamp on k_len.  There used to be (the amplitude was computed from
// max(1.0, k_len)), which held every component with a wavelength longer than about 6 m down to a 2 cm ripple.
// That made sense while the water was a flat mesh: a long wave could only ever be faked in the shading normal,
// and a 55 m wave faked that way reads as a smear rather than as a wave.  The mesh carries them for real now.
#define WATER_WAVE_AMPLITUDE_SCALE 0.03

// The sum of all 200 component amplitudes, e.g. the furthest the surface can be displaced from z = 0.  Only
// reached if every component happens to peak at the same place, so it is a loose bound, but it is the bound the
// chunk bounding boxes in TerrainSystem.cpp are padded by - keep the two in sync.
#define WATER_MAX_WAVE_DISPLACEMENT 0.5

// Wavenumber above which components are left to the fragment shader however finely the mesh is tessellated.
// A component here has a wavelength of about 2 m and an amplitude of about 4 mm, so displacing it, or anything
// finer, would cost a lot of vertex work for a bump well under a pixel.  It also bounds the loop below to about
// 15 of the 200 components in the vertex shader.
#define WATER_MAX_GEOM_K 3.0

// Vertex spacing of the water mesh, as a fraction of the distance from the camera.  The chunk quadtree picks a
// depth of log2(world_w / (chunk_res * d * quad_w_screenspace_target)) for a chunk d away, which works out to a
// quad width of about d * quad_w_screenspace_target.  Keep in sync with quad_w_screenspace_target in
// substrata's TerrainSystem.cpp.  Erring high is safe: it hands work back to the fragment shader, whereas
// erring low would double-count components.
#define WATER_QUAD_W_SCREENSPACE_TARGET 0.032

// The short wind waves - the band whose amplitude follows the local wind, so is scaled up and down by gusts
// - is the part of the spectrum above WATER_SHORT_WAVE_K0, fully so above WATER_SHORT_WAVE_K1 (wavelengths
// of about 3 m and 1 m).  It carries about 56% of the spectrum's slope variance.  The longer waves are left alone; they were raised by wind elsewhere and long ago.
#define WATER_SHORT_WAVE_K0 2.0
#define WATER_SHORT_WAVE_K1 6.0

// The total per-axis slope variance of all 200 wave components, e.g. the value that resolved_slope_var takes
// when k_lowpass is large enough that every component is resolved, with short_wave_scale = 1.  It depends only on
// waterWaveHash(), the amplitude formula and the short wave band, so it is just a constant.  Computed by evaluating
//     sum over i of ((a_i*k_i.x)^2 + (a_i*k_i.y)^2) / 4
// offline with the same hash, per unit WATER_WAVE_AMPLITUDE_SCALE^2; recompute it if the hash, the amplitude formula
// or the short wave band changes.  At WATER_WAVE_AMPLITUDE_SCALE = 0.09 the rms slope is 0.060, e.g. about 3.4 degrees.
// SHORT_WAVE_SLOPE_VAR_1 and _2 are the same sum weighted by s_i and s_i^2, where s_i is the short wave weight
// smoothstep(WATER_SHORT_WAVE_K0, WATER_SHORT_WAVE_K1, |k_i|).  See waterTotalSlopeVar().
const float TOTAL_SLOPE_VAR        = 0.439469 * WATER_WAVE_AMPLITUDE_SCALE * WATER_WAVE_AMPLITUDE_SCALE;
const float SHORT_WAVE_SLOPE_VAR_1 = 0.268130 * WATER_WAVE_AMPLITUDE_SCALE * WATER_WAVE_AMPLITUDE_SCALE;
const float SHORT_WAVE_SLOPE_VAR_2 = 0.248111 * WATER_WAVE_AMPLITUDE_SCALE * WATER_WAVE_AMPLITUDE_SCALE;


// The total per-axis slope variance of the spectrum with its short waves scaled by short_wave_scale.
// Component i has slope variance (1 + (r-1) s_i)^2 v_i, which sums to V + 2(r-1) V_1 + (r-1)^2 V_2.
float waterTotalSlopeVar(float short_wave_scale)
{
	float d = short_wave_scale - 1.0;
	return TOTAL_SLOPE_VAR + 2.0 * d * SHORT_WAVE_SLOPE_VAR_1 + d * d * SHORT_WAVE_SLOPE_VAR_2;
}


// https://www.shadertoy.com/view/MdcfDj
float waterWaveHash(uvec2 q)
{
	q *= uvec2(1597334677u /* 1719413*929 */, 3812015801u /* 140473*2467*11 */);

	uint n = (q.x ^ q.y) * 1597334677u;

	return float(n) * (1.0 / float(0xffffffffu));
}


// The largest wavenumber the water geometry carries at a world space position.  The tessellated mesh gets finer
// towards the camera, so this does too, up to the WATER_MAX_GEOM_K cap.
//
// This is a function of world space position alone, which is what makes the tessellation work: two neighbouring
// chunks at different LOD levels agree on it along their shared edge, so they agree on the displacement there
// and meet without a crack, and no skirt geometry is needed.  It also means the vertex and fragment shaders
// agree on where the spectrum is split without anything being passed between them.
float waterGeomCutoffK(vec3 pos_ws, vec3 cam_pos_ws)
{
	float d = distance(pos_ws, cam_pos_ws);
	float quad_w = max(d * WATER_QUAD_W_SCREENSPACE_TARGET, 1.0e-3); // Vertex spacing of the chunk covering pos_ws.
	return min(WATER_PI / quad_w, WATER_MAX_GEOM_K);
}


// The fraction of a component of wavenumber magnitude k_mag that a surface sampled at the Nyquist limit
// k_cutoff can carry.  Faded out as the component approaches the limit rather than dropped abruptly, so that
// the handover between geometry, sum and stochastic sampling is smooth as the camera moves.
float waterWaveWindow(float k_mag, float k_cutoff)
{
	return 1.0 - smoothstep(0.5, 1.0, k_mag / k_cutoff);
}


// Sums a band of the wave spectrum:
//   k_lowpass:  components above this are left out, as the caller cannot resolve them.
//   k_highpass: components below this are left out, as they are already displaced into the geometry.  Pass 0.0
//               to sum the whole band below k_lowpass.
//
// Returns the vertical displacement of the components summed; their combined slope (df/dx, df/dy) is returned
// in slope_out, so the normal is normalize(vec3(0,0,1) - vec3(slope_out, 0)).
//
// short_wave_scale scales the amplitude of the short wind waves (see WATER_SHORT_WAVE_K0).  Pass 1.0 for the
// unmodified spectrum.
//
// slope_var_out gets the per-axis slope variance of every component below k_lowpass, the ones the high-pass
// left out included: it says how much of waterTotalSlopeVar(short_wave_scale) is accounted for, and a component
// sitting in the geometry is just as accounted for as one summed here.
//
// NOTE: contains no derivative operations, so is safe to call from non-uniform control flow.
float waterWaveSum(vec2 pos_xy, float wave_time, float k_lowpass, float k_highpass, float short_wave_scale, out vec2 slope_out, out float slope_var_out)
{
	float displacement = 0.0;
	vec2 slope = vec2(0.0);
	float slope_var = 0.0;

	float k_len = 1.0;
	for(int i=0; i<200; ++i)
	{
		// f(x) = a sin(k.(x,y) - omega*t)
		// df/dx = a k_x cos(k.(x,y) - omega*t)
		// df/dy = a k_y cos(k.(x,y) - omega*t)

		// |k| <= k_len * sqrt(2)/2 (see the construction of k below), and k_len only increases, so once this
		// upper bound passes the low-pass limit, every remaining component is left out and we can stop.
		if(k_len * 0.70710678 > k_lowpass)
			break;

		float a = WATER_WAVE_AMPLITUDE_SCALE * pow(k_len, -1.5);
		if(k_len < 4.0)
			a *= 0.5;
		//if(k_len > 50.0)
		//	a *= 0.2;

		vec2 k = vec2(
			-0.5 + waterWaveHash(uvec2(uint(i), 0)),
			-0.5 + waterWaveHash(uvec2(uint(i), 1))
		) * k_len;
		float k_mag = length(k);

		a *= 1.0 + (short_wave_scale - 1.0) * smoothstep(WATER_SHORT_WAVE_K0, WATER_SHORT_WAVE_K1, k_mag);

		float low_window = waterWaveWindow(k_mag, k_lowpass);

		// Slope variance this component accounts for.  The x slope is a*low_window*k_x*cos(phase), whose
		// variance over the phase is (a*low_window*k_x)^2 / 2, and likewise for y; take the mean of the two axes
		// for an isotropic estimate.
		slope_var += (low_window * low_window) * ((a * k.x) * (a * k.x) + (a * k.y) * (a * k.y)) * 0.25;

		// Take out the part of the component the geometry is already carrying.
		float window = low_window * ((k_highpass > 0.0) ? (1.0 - waterWaveWindow(k_mag, k_highpass)) : 1.0);

		float omega = sqrt(9.8 * k_mag); // Deep water dispersion relation.
		float phase = dot(k, pos_xy) - omega * wave_time;

		displacement += (a * window) * sin(phase);
		slope        += (a * window) * k * cos(phase);

		k_len += 0.3;
	}

	slope_out = slope;
	slope_var_out = slope_var;
	return displacement;
}
