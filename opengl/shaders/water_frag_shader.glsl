
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


uniform sampler2D caustic_tex_a;
uniform sampler2D caustic_tex_b;


uniform sampler2D main_colour_texture; // source texture
uniform sampler2D main_normal_texture;
uniform sampler2D main_depth_texture;


//----------------------------------------------------------------------------------------------------------------------------
#if USE_MULTIDRAW_ELEMENTS_INDIRECT

flat in int material_index;

layout(std430) buffer PhongUniforms
{
	MaterialData material_data[];
};

#define MAT_UNIFORM					material_data[material_index]

#define DIFFUSE_TEX					MAT_UNIFORM.diffuse_tex
#define EMISSION_TEX				MAT_UNIFORM.emission_tex

//----------------------------------------------------------------------------------------------------------------------------
#else // else if !USE_MULTIDRAW_ELEMENTS_INDIRECT:


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

#endif // end if !USE_MULTIDRAW_ELEMENTS_INDIRECT
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
layout(location = 1) out vec3 normal_out;



float square(float x) { return x*x; }
float pow5(float x) { return x*x*x*x*x; }
float pow6(float x) { return x*x*x*x*x*x; }

//float fresnellApprox(float cos_theta, float ior)
//{
//	float r_0 = square((1.0 - ior) / (1.0 + ior));
//	return r_0 + (1.0 - r_0)*pow5(1.0 - cos_theta);
//}
float fresnelApprox(float cos_theta_i, float n2)
{
	//float r_0 = square((1.0 - n2) / (1.0 + n2));
	//return r_0 + (1.0 - r_0)*pow5(1.0 - cos_theta_i); // https://en.wikipedia.org/wiki/Schlick%27s_approximation

	float sintheta_i = sqrt(1.0 - cos_theta_i*cos_theta_i); // Get sin(theta_i)
	float sintheta_t = sintheta_i / n2; // Use Snell's law to get sin(theta_t)

	float costheta_t = sqrt(1.0 - sintheta_t*sintheta_t); // Get cos(theta_t)

	float a2 = square(cos_theta_i - n2*costheta_t);
	float b2 = square(cos_theta_i + n2*costheta_t);

	float c2 = square(n2*cos_theta_i - costheta_t);
	float d2 = square(costheta_t + n2*cos_theta_i);

	return 0.5 * (a2*d2 + b2*c2) / (b2*d2);
}

float trowbridgeReitzPDF(float cos_theta, float alpha2)
{
	return cos_theta * alpha2 / (3.1415926535897932384626433832795 * square(square(cos_theta) * (alpha2 - 1.0) + 1.0));
}

float alpha2ForRoughness(float r)
{
	return pow6(r);
}

vec3 toNonLinear(vec3 x)
{
	// Approximation to pow(x, 0.4545).  Max error of ~0.004 over [0, 1].
	return 0.124445006f*x*x + -0.35056138f*x + 1.2311935*sqrt(x);
}




float rayPlaneIntersect(vec3 raystart, vec3 ray_unitdir, float plane_h)
{
	float start_to_plane_dist = raystart.z - plane_h;

	return start_to_plane_dist / -ray_unitdir.z;
}

vec2 rot(vec2 p)
{
	float theta = 1.618034 * 3.141592653589 * 2.0;
	return vec2(cos(theta) * p.x - sin(theta) * p.y, sin(theta) * p.x + cos(theta) * p.y);
}

float fbm(vec2 p)
{
	return (texture(fbm_tex, p).x - 0.5) * 2.f;
}

float fbmMix(vec2 p)
{
	return 
		fbm(p) +
		fbm(rot(p * 2.0)) * 0.5;
}



// 'A Survey of Efficient Representations for Independent Unit Vectors', listing 1+2.
// Returns +- 1
vec2 signNotZero(vec2 v) {
	return vec2(((v.x >= 0.0) ? 1.0 : -1.0), ((v.y >= 0.0) ? 1.0 : -1.0));
}

vec3 oct_to_float32x3(vec2 e) {
	vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
	if (v.z < 0.0) v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
	return normalize(v);
}

// 'A Survey of Efficient Representations for Independent Unit Vectors', listing 5.
vec2 unorm8x3_to_snorm12x2(vec3 u) {
	u *= 255.0;
	u.y *= (1.0 / 16.0);
	vec2 s = vec2(u.x * 16.0 + floor(u.y),
		fract(u.y) * (16.0 * 256.0) + u.z);
	return clamp(s * (1.0 / 2047.0) - 1.0, vec2(-1.0), vec2(1.0));
}



// Converts a unit vector to a point in octahedral representation ('oct').
// 'A Survey of Efficient Representations for Independent Unit Vectors', listing 1.

// Assume normalized input. Output is on [-1, 1] for each component.
vec2 float32x3_to_oct(in vec3 v) {
	// Project the sphere onto the octahedron, and then onto the xy plane
	vec2 p = v.xy * (1.0 / (abs(v.x) + abs(v.y) + abs(v.z)));
	// Reflect the folds of the lower hemisphere over the diagonals
	return (v.z <= 0.0) ? ((1.0 - abs(p.yx)) * signNotZero(p)) : p;
}

// 'A Survey of Efficient Representations for Independent Unit Vectors', listing 5.
vec3 snorm12x2_to_unorm8x3(vec2 f) {
	vec2 u = vec2(round(clamp(f, -1.0, 1.0) * 2047.0 + 2047.0));
	float t = floor(u.y / 256.0);
	// If storing to GL_RGB8UI, omit the final division
	return floor(vec3(u.x / 16.0,
		fract(u.x / 16.0) * 256.0 + t,
		u.y - t * 256.0)) / 255.0;
}




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
#if USE_REVERSE_Z
	return near_clip_dist / texture(main_depth_texture, vec2(px, py)).x;
#else
	return -near_clip_dist / (texture(main_depth_texture, vec2(px, py)).x - 1.0);
#endif
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
	vec3 src_normal_encoded = texture(main_normal_texture, vec2(refracted_px, refracted_py)).xyz; // Encoded as a RGB8 texture (converted to floating point)
	vec3 src_normal_ws = oct_to_float32x3(unorm8x3_to_snorm12x2(src_normal_encoded)); // Read normal from normal texture

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
	vec3 exp_optical_depth = exp(extinction * -final_refracted_water_ground_d);
	vec3 inscattering = inscatter_radiance_sigma_s_over_sigma_t * (vec3(1.0) - exp_optical_depth);

	vec3 attentuated_col = src_col * exp_optical_depth;

	return attentuated_col + inscattering;
}


void main()
{
	vec2 use_texture_coords = vec2(0, 0);

	vec3 frag_to_cam = normalize(-pos_cs);

	const float ior = 1.33f;

	//vec3 sunrefl_h = normalize(frag_to_cam + sundir_cs.xyz);
	//float sunrefl_h_cos_theta = abs(dot(sunrefl_h, unit_normal_cs));
	float roughness = 0.01;
	//float fresnel_scale = 1.0;
	//float sun_specular = trowbridgeReitzPDF(sunrefl_h_cos_theta, max(1.0e-8f, alpha2ForRoughness(roughness))) * 
	//	fresnel_scale * fresnellApprox(sunrefl_h_cos_theta, ior);


	// waves
	vec3 unit_normal_ws = normalize(normal_ws);

	float deriv = length(dFdx(pos_ws));
	float sin_window = 1.0 - smoothstep(0.0, 0.01, deriv);

	float fbm_window = 1.0 - smoothstep(0.0, 0.2, deriv);
	
	unit_normal_ws.y += (fbmMix(pos_ws.xy * 0.1 + vec2(0, -time * 0.1)) * 0.04 + sin(dot(pos_ws.xy, vec2(0.6, 0.3)) * 10.0 + time * 2.0) * 0.003 + sin(pos_ws.y * 20.0 + -time * 2.0) * 0.04) * sin_window;
	//unit_normal_ws.x += sin(dot(pos_ws.xy, vec2(0.2, 0.3)) * 10.0 + time * 2.0) * 0.003 + sin(pos_ws.y * 10.0 + -time * 2.0) * 0.002;

	unit_normal_ws.x += (fbmMix(pos_ws.xy * 0.1) * 0.01 + fbmMix(pos_ws.xy * 0.01) * 0.01) * fbm_window;

//	unit_normal_ws = normalize(unit_normal_ws);
	
	if(dot(unit_normal_ws, cam_to_pos_ws) > 0.0)
		unit_normal_ws = -unit_normal_ws;


	vec3 unit_cam_to_pos_ws = normalize(cam_to_pos_ws);

	//ivec2 tex_res = textureSize(main_colour_texture, 0);
	//float width_over_height = float(tex_res.x) / float(tex_res.y);

	vec3 col = vec3(0.0); // spectral radiance * 1.0e-9
	vec3 spec_refl_light_already_fogged = vec3(0.0); // spectral radiance * 1.0e-9
	vec3 spec_refl_light = vec3(0.0); // spectral radiance * 1.0e-9
	float spec_refl_fresnel = 0.0;
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


		vec3 refracted_dir_ws = refract(unit_cam_to_pos_ws, unit_normal_ws, 1.0 / 1.33);

		// Vectors on plane orthogonal to incident direction
		vec3 right_ws = normalize(cross(unit_cam_to_pos_ws, vec3(0,0,1)));
		vec3 up_ws = cross(right_ws, unit_cam_to_pos_ws);

		refracted_dir_ws = normalize(refracted_dir_ws - up_ws * dot(refracted_dir_ws, up_ws)); // Remove up/down component of refraction in plane orthogonal to incident direction


		//========================= Do screen space reflection trace =============================
		
		bool hit_something = false;
#if WATER_DO_SCREENSPACE_REFL_AND_REFR
		{
			// First get dir in screen space.
			/*
			suppose we have a point in camera space along some parameterised line:
			p_cs = o_cs + t_1 d_cs

			and a function f that projects a point in camera space onto screen space:
			f(p) = (p_x/-p_z l/w + 1/2, p_y/-p_z l/w w/h + 1/2)
			
			then the projected point in scren space is
			p_ss = f(p_cs) = f(o_cs + t_1 d_cs)
			
			and its x coordinate is
			p_ss_x = (o_cs_x + t_1 d_cs_x)/-(o_cs_z + t_1 d_cs_z) l/w + 1/2

			Solving for t_1:
			p_ss_x - 1/2 = (o_cs_x + t_1 d_cs_x)/-(o_cs_z + t_1 d_cs_z) l/w
			-(o_cs_z + t_1 d_cs_z) (p_ss_x - 1/2) = (o_cs_x + t_1 d_cs_x) l/w
			(o_cs_z + t_1 d_cs_z) (-p_ss_x + 1/2) = (o_cs_x + t_1 d_cs_x) l/w
			o_cs_z (-p_ss_x)  +  o_cs_z/2  -  t_1 d_cs_z p_ss_x  +  t_1 d_cs_z/2  =  o_cs_x l/w  +  t_1 d_cs_x l/w
			- t_1 d_cs_z p_ss_x  +  t_1 d_cs_z/2  -  t_1 d_cs_x l/w  =  o_cs_z p_ss_x  -  o_cs_z/2  +  o_cs_x l/w
			t_1 (-d_cs_z p_ss_x  +  d_cs_z/2  -  d_cs_x l/w)  =  o_cs_z p_ss_x  -  o_cs_z/2  +  o_cs_x l/w
			t_1 = (o_cs_z p_ss_x  -  o_cs_z/2  +  o_cs_x l/w) / (-d_cs_z p_ss_x  +  d_cs_z/2  -  d_cs_x l/w)
			t_1 = (o_cs_z (p_ss_x - 1/2)  +  o_cs_x l/w) / (d_cs_z (-p_ss_x + 1/2) - d_cs_x l/w)

			singularity when 
			d_cs_z (-p_ss_x + 1/2) - d_cs_x l/w = 0

			
			Example:
			o_cs = (0,0,-10)           [10m in front of camera]
			d_cs = (1,0,0)

			suppose p_ss_x = 0.6       [Just to right of centre of screen]
			t_1 = (o_cs_z (p_ss_x - 1/2)  +  o_cs_x l/w) / (d_cs_z (-p_ss_x + 1/2) - d_cs_x l/w)
			    = ((-10) (0.6 - 1/2) + (0) l/w) / ((0) (-0.6 + 1/2) - (1)l/w)
				= (-10)(0.1) / (-l/w)
				= 1 / (l/w)
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
			//float prev_penetration_depth = 0;
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
				{
					//hit_something = true;
					//break;
					t_1 = 100000.0;
				}
					//t_1 = 100.0;//1.0e10f;

				float p_cs_z = pos_cs.z + dir_cs.z * t_1; // Z coordinate of point on camera-space ray that projects onto the current screen space point
				float cur_step_depth = -p_cs_z;

				// Get depth at screen space point
				
				float cur_depth_buf_depth = getDepthFromDepthTexture(cur_ss.x, cur_ss.y); // Get depth from depth buffer for current step position

				//float penetration_depth = cur_step_depth - cur_depth_buf_depth;
				//float ob_max_depth = cur_depth_buf_depth * ;

				if((cur_step_depth > cur_depth_buf_depth)  && (cur_depth_buf_depth > intersection_depth_threshold)) // if we hit something:
				{
					// Solve for approximate distance along ray where we intersect surface.
				//	float frac = -prev_penetration_depth / (penetration_depth - prev_penetration_depth); // frac = -prev_penetration_depth / (-prev_penetration_depth + penetration_depth);
				//	float prev_t = t - step_t;
				//	float intersect_t = mix(prev_t, t, frac);
				//	cur_ss = o_ss + dir_ss * intersect_t;
				//
				//	spec_refl_light_already_fogged = texture(main_colour_texture, cur_ss).xyz * (1.0 / 0.000000003);
					hit_something = true;
					break;
				}

				//prev_penetration_depth = penetration_depth;
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
				spec_refl_light_already_fogged = texture(main_colour_texture, cur_ss).xyz;

				//spec_refl_light_already_fogged = vec3(100000000.0);//TEMP HACK
			}
		}
#endif // end if WATER_DO_SCREENSPACE_REFL_AND_REFR

		//========================= Look up env map for reflected dir ============================
		if(!hit_something) // If we didn't hit anything with the screen-space trace:
		{
			int map_lower = int(roughness * 6.9999);
			int map_higher = map_lower + 1;
			float map_t = roughness * 6.9999 - float(map_lower);

			float refl_theta = acos(reflected_dir_ws.z);
			float refl_phi = atan(reflected_dir_ws.y, reflected_dir_ws.x) - env_phi; // -1.f is to rotate reflection so it aligns with env rotation.
			vec2 refl_map_coords = vec2(refl_phi * (1.0 / 6.283185307179586), clamp(refl_theta * (1.0 / 3.141592653589793), 1.0 / 64.0, 1.0 - 1.0 / 64.0)); // Clamp to avoid texture coord wrapping artifacts.

			vec3 spec_refl_light_lower  = texture(specular_env_tex, vec2(refl_map_coords.x, float(map_lower)  * (1.0/8.0) + refl_map_coords.y * (1.0/8.0))).xyz; //  -refl_map_coords / 8.0 + map_lower  * (1.0 / 8)));
			vec3 spec_refl_light_higher = texture(specular_env_tex, vec2(refl_map_coords.x, float(map_higher) * (1.0/8.0) + refl_map_coords.y * (1.0/8.0))).xyz;
			spec_refl_light = spec_refl_light_lower * (1.0 - map_t) + spec_refl_light_higher * map_t; // spectral radiance * 1.0e-9

			//-------------- sun ---------------------
			float d = dot(sundir_ws.xyz, reflected_dir_ws);

			float sunscale = mix(2.0e-2f, 2.0e-3f, sundir_ws.z); // A hack to avoid having too extreme bloom from the sun, also to compensate for larger sun size due to the smoothstep below.
			const float sun_solid_angle = 0.00006780608; // See SkyModel2Generator::makeSkyEnvMap();
			vec3 suncol = sun_spec_rad_times_solid_angle.xyz * (1.0 / sun_solid_angle) * sunscale;
			//vec4 suncol = vec4(9.38083E+12 * sunscale, 4.99901E+12 * sunscale, 1.25408E+12 * sunscale, 1); // From Indigo SkyModel2Generator.cpp::makeSkyEnvMap().
			//float d = dot(sundir_cs.xyz, normalize(pos_cs));
			//col = mix(col, suncol, smoothstep(0.9999, 0.9999892083461507, d));
			spec_refl_light = mix(spec_refl_light, suncol, smoothstep(0.99997, 0.9999892083461507, d));


#if DRAW_AURORA
			// NOTE: code duplicated in env_frag_shader
			vec3 dir_ws = reflected_dir_ws;
			vec3 env_campos_ws = pos_ws;

			float min_aurora_z = 1000.0;
			float max_aurora_z = 8000.0;
			float aurora_start_ray_t = rayPlaneIntersect(env_campos_ws, dir_ws, min_aurora_z);
			float aurora_end_ray_t = rayPlaneIntersect(env_campos_ws, dir_ws, max_aurora_z);

			int num_steps = 32;
			float t_step = min(600.0, (aurora_end_ray_t - aurora_start_ray_t) / float(num_steps));
			float pixel_hash = texture(blue_noise_tex, gl_FragCoord.xy * (1.0 / 128.f)).x;
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
				
						spec_refl_light += (0.001 * t_step * col_for_height * aurora_val.r * z_ramp_intensity_factor * high_freq_intensity_factor * z_factor).xyz;
					}
				}
			}
#endif


			//-------------- clouds ---------------------

			//float d = dot(sundir_cs.xyz, reflected_dir_cs);
			

			// Get position ray hits cloud plane
			float cirrus_cloudfrac = 0.0;
			float cumulus_cloudfrac = 0.0;
			float ray_t = rayPlaneIntersect(pos_ws, reflected_dir_ws, 6000.0);
			//vec4 cumulus_col = vec4(0,0,0,0);
			//float cumulus_alpha = 0;
			float cumulus_edge = 0.0;
			if(ray_t > 0.0)
			{
				vec3 hitpos = pos_ws + reflected_dir_ws * ray_t;
				vec2 p = hitpos.xy * 0.0001;
				p.x += time * 0.002;

				vec2 coarse_noise_coords = vec2(p.x * 0.16, p.y * 0.20);
				float course_detail = fbmMix(vec2(coarse_noise_coords));

				cirrus_cloudfrac = max(course_detail * 0.9, 0.f) * texture(cirrus_tex, p).x * 1.5;
			}

			{
				float cumulus_ray_t = rayPlaneIntersect(pos_ws, reflected_dir_ws, 1000.0);
				if(cumulus_ray_t > 0.0)
				{
					vec3 hitpos = pos_ws + reflected_dir_ws * cumulus_ray_t;
					vec2 p = hitpos.xy * 0.0001;
					p.x += time * 0.002;

					vec2 cumulus_coords = vec2(p.x * 2.0 + 2.3453, p.y * 2.0 + 1.4354);

					float cumulus_val = max(0.f, fbmMix(cumulus_coords) - 0.3f);
					//cumulus_alpha = max(0.f, cumulus_val - 0.7f);

					cumulus_edge = smoothstep(0.0001, 0.1, cumulus_val) - smoothstep(0.2, 0.6, cumulus_val) * 0.5;

					float dist_factor = 1.f - smoothstep(80000.0, 160000.0, ray_t);

					//cumulus_col = vec4(cumulus_val, cumulus_val, cumulus_val, 1);
					cumulus_cloudfrac = dist_factor * cumulus_val;
				}
			}

			float cloudfrac = max(cirrus_cloudfrac, cumulus_cloudfrac);
			vec3 cloudcol = sun_and_sky_av_spec_rad.xyz;
			spec_refl_light = mix(spec_refl_light, cloudcol, max(0.f, cloudfrac));
			vec3 suncloudcol = cloudcol * 2.5;
			float blend = max(0.f, cumulus_edge) * pow(max(0.0, d), 32.0);// smoothstep(0.9, 0.9999892083461507, d);
			spec_refl_light = mix(spec_refl_light, suncloudcol, blend);
		}
		//----------------------------------------------------------------


		//vec4 transmission_col = vec4(0.05, 0.2, 0.7, 1.0); //  MAT_UNIFORM.diffuse_colour;

		float spec_refl_cos_theta = max(0.0, -unit_cam_to_pos_ws_dot_normal_ws);
		spec_refl_fresnel = fresnelApprox(spec_refl_cos_theta, ior);

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
			spec_refl_light * spec_refl_fresnel;
		
	} // End if cam is above water surface


#if DEPTH_FOG
	// Blend with background/fog colour
	float dist_ = max(0.0, -pos_cs.z); // Max with 0 avoids bright artifacts on horizon.
	vec3 transmission = exp(air_scattering_coeffs.xyz * -dist_);

	col.xyz *= transmission;
	col.xyz += sun_and_sky_av_spec_rad.xyz * (1.0 - transmission);
#endif

	col += spec_refl_light_already_fogged                  * spec_refl_fresnel;



	//TEMP
	//vec2 o_ss = cameraToScreenSpace(pos_cs); // Get current fragment screen space position
	//col = vec3(getDepthFromDepthTexture(o_ss.x, o_ss.y)) * 0.001;

#if DO_POST_PROCESSING
	colour_out = vec4(col, 1.f);
#else
	colour_out = vec4(toNonLinear(col), 1.f);
#endif

	normal_out = snorm12x2_to_unorm8x3(float32x3_to_oct(unit_normal_ws));
}
