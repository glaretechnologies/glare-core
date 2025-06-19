
uniform sampler2D diffuse_tex;
#if NORMAL_TEXTURE_IS_UINT
uniform usampler2D normal_tex;
#else
uniform sampler2D normal_tex;
#endif
uniform sampler2D depth_tex;

uniform sampler2D blue_noise_tex;
uniform samplerCube cosine_env_tex;
uniform sampler2D specular_env_tex;



in vec2 texture_coords;


layout(location = 0) out vec4 irradiance_out;
layout(location = 1) out vec4 specular_spec_rad_out;


float getDepthFromDepthTexture(vec2 normed_pos_ss)
{
	return getDepthFromDepthTextureValue(near_clip_dist, texture(depth_tex, normed_pos_ss).x);
}


vec3 viewSpaceFromScreenSpacePosAndDepth(vec2 normed_pos_ss, float depth)
{
	return vec3(
		(normed_pos_ss.x - 0.5) * depth / l_over_w,
		(normed_pos_ss.y - 0.5) * depth / l_over_h,
		-depth
	);
}


vec3 viewSpaceFromScreenSpacePos(vec2 normed_pos_ss)
{
	float depth = getDepthFromDepthTexture(normed_pos_ss); // depth := -pos_cs.z
 
	return viewSpaceFromScreenSpacePosAndDepth(normed_pos_ss, depth);
}

// Returns coords in [0, 1] for visible positions
vec2 cameraToScreenSpace(vec3 pos_cs)
{
	return vec2(
		pos_cs.x / -pos_cs.z * l_over_w + 0.5,
		pos_cs.y / -pos_cs.z * l_over_h + 0.5
	);
}


// See https://forwardscattering.org/post/64
vec2 rayDirCameraToScreenSpace(vec3 p, vec3 d)
{
	return vec2(
		(p.x * d.z - p.z * d.x) * l_over_w,
		(p.y * d.z - p.z * d.y) * l_over_h
	);
}



const uint SECTOR_COUNT = 32u;

// Adapted from https://cdrinmatane.github.io/posts/ssaovb-code/
uint occlusionBitMask(float minHorizon, float maxHorizon)
{
	uint startHorizonInt = uint(minHorizon * float(SECTOR_COUNT));
	uint angleHorizonInt = uint(ceil((maxHorizon - minHorizon) * float(SECTOR_COUNT)));
	uint angleHorizonBitfield = angleHorizonInt > 0u ? (0xFFFFFFFFu >> (SECTOR_COUNT - angleHorizonInt)) : 0u;
	return angleHorizonBitfield << startHorizonInt;
}


// https://graphics.stanford.edu/%7Eseander/bithacks.html#CountBitsSetParallel | license: public domain
uint countSetBits(uint v)
{
	v = v - ((v >> 1u) & 0x55555555u);
	v = (v & 0x33333333u) + ((v >> 2u) & 0x33333333u);
	return ((v + (v >> 4u) & 0xF0F0F0Fu) * 0x1010101u) >> 24u;
}


float heaviside(float x)
{
	return (x > 0.0) ? 1.0 : 0.0;
}


vec3 removeComponentInDir(vec3 v, vec3 unit_dir)
{
	return v - unit_dir * dot(v, unit_dir);
}


float sinForCos(float cos_x)
{
	// cos^2 x + sin^2 x = 1
	// sin^2 x = 1 - cos^2 x
	// sin x = sqrt(1 - cos^2 x)
	return sqrt(max(0.0, 1.0 - cos_x*cos_x));
}


// Returns normalised vector in cam space
vec3 readNormalFromNormalTexture(vec2 use_tex_coords)
{
	// NOTE: oct_to_float32x3 returns a normalized vector
#if NORMAL_TEXTURE_IS_UINT
	return oct_to_float32x3(unorm8x3_to_snorm12x2(textureLod(normal_tex, use_tex_coords, 0.0)));
#else
	return oct_to_float32x3(unorm8x3_to_snorm12x2(textureLod(normal_tex, use_tex_coords, 0.0).xyz)); 
#endif
}


// See "Screen Space Indirect Lighting with Visibility Bitmask", PDF: https://arxiv.org/pdf/2301.11376
// See "SSAO using Visibility Bitmasks" blog post: https://cdrinmatane.github.io/posts/ssaovb-code/

void main()
{
	vec2 pixel_hash = texture(blue_noise_tex, gl_FragCoord.xy * (1.f / 64.f)).xy;

	// Read normal from normal texture
	vec3 src_normal_cs = readNormalFromNormalTexture(texture_coords);

	vec3 n = src_normal_cs; // View/camera space surface normal
	vec3 p = viewSpaceFromScreenSpacePos(texture_coords); // View/camera space surface position
	if(p.z < -100000.0) // Don't do SSAO for the environment sphere
	{
		irradiance_out = vec4(0.0, 0.0, 0.0, 1.0);
		specular_spec_rad_out = vec4(0.0);
		return;
	}
	vec3 V = -normalize(p); // View vector: vector from 'fragment' position to camera in view/camera space
	
	vec2 origin_ss = texture_coords; // Origin of stepping in screen space

	// Form basis in camera/view space aligned with fragment surface
	//vec3 tangent = normalize(removeComponentInDir(V, n));
	//vec3 bitangent = cross(tangent, n);

	const float thickness = 0.2;
	const float r = 0.2; // Total stepping radius in screen space
	const int N_s = 22; // Number of steps per direction
	const float initial_step_size = r / float(N_s); // (float(N_s) + 1.0);

	const int N_d = 4; // num directions to sample

	float uniform_irradiance = 0.0;
	vec3 irradiance = vec3(0.0);

	float aspect_ratio = l_over_h / l_over_w; // viewport width / height

	for(int i=0; i<N_d; ++i) // For each direction:
	{
#if 0
		float theta = PI * (float(i) + pixel_hash.x) / float(N_d);
		vec3 d_i_vs = tangent * cos(theta) + bitangent * sin(theta);
		vec3 offset_p_vs = p + d_i_vs * 1.0e-2;
		vec2 offset_p_ss = cameraToScreenSpace(offset_p_vs);
		vec2 d_i_ss = normalize(offset_p_ss - origin_ss);
#else
		// Direction d_i is given by the view space direction (1,0,0) rotated around (0, 0, 1) by theta = 2pi * i / N_d
		float theta = PI * (float(i) + pixel_hash.x) / float(N_d);
		vec3 d_i_vs = vec3(cos(theta), sin(theta) * aspect_ratio, 0); // direction i (d_i) in view/camera space
		vec2 d_i_ss = d_i_vs.xy; // direction i (d_i) in normalised screen space
#endif

		// d_i_vs = removeComponentInDir(d_i_vs, V);// d_i_vs - V * dot(V, d_i_vs); // Make d_i_vs orthogonal to view vector. NOTE: needed?

		vec3 sampling_plane_n = normalize(cross(d_i_vs, V)); // Get vector normal to sampling plane and orthogonal to view vector.  Normalise needed for removeComponentInDir() below. (could use unnormalised version of function tho)
		vec3 projected_n = normalize(removeComponentInDir(n, sampling_plane_n)); // fragment surface normal projected into sampling plane

		// Get angle between projected normal and view vector
		float view_proj_n_angle = fastApproxACos(dot(projected_n, V));
		float view_alpha = PI_2 + sign(dot(cross(V, projected_n), sampling_plane_n)) * view_proj_n_angle;


		uint b_i = 0u; // bitmask: each bit set to 1 if trace hit something in that sector.
		float step_incr = initial_step_size; // Distance in screen space to step, increases slightly each step.
		float last_step_incr = step_incr;
		float dist_ss = step_incr;// * pixel_hash; // Total distance stepped in screen space, before randomisation

		// We will trace in one direction in screen space for N_s steps, then go back and trace in the reverse direction for another N_s steps.
		// Say N_s = 4.
		// q = 0, 1, 2, 3  are samples in one direction,
		// q = 4, 5, 6, 7 are samples in the backwards direction
		for(int q=0; q<N_s * 2; ++q)
		{
			if(q == N_s)
			{
				// Reset, start walking in other direction
				step_incr = -initial_step_size;
				last_step_incr = step_incr;
				dist_ss = step_incr;
			}
			float cur_dist_ss = dist_ss - pixel_hash.y * last_step_incr;

			vec2 pos_j_ss = origin_ss + d_i_ss * cur_dist_ss; // step_j position in screen space
			if(!(pos_j_ss.x >= 0.0 && pos_j_ss.x <= 1.0 && pos_j_ss.y >= 0.0 && pos_j_ss.y <= 1.0)) // TODO: optimise
				continue;
			
			vec3 pos_j = viewSpaceFromScreenSpacePos(pos_j_ss); // get step_j position in camera/view space

			vec3 back_pos_j = pos_j - V * thickness; // position of guessed 'backside' of step position in camera/view space

			vec3 unit_p_to_pos_j      = normalize(pos_j      - p); // normalised vector from fragment position to step position, in view/camera space
			vec3 unit_p_to_back_pos_j = normalize(back_pos_j - p); // normalised vector from fragment position to step back position, in view/camera space
			
			// Convert to angles in [0, pi], the angle between the surface and frag-to-step_j position
			float V_p_p_j_angle =      fastApproxACos(dot(V, unit_p_to_pos_j)); // Angle between view vector and p to p_j.
			float V_p_p_j_back_angle = fastApproxACos(dot(V, unit_p_to_back_pos_j)); // Angle between view vector and p to p_back_j.
			float angle_add_sign = sign(dot(cross(unit_p_to_pos_j, V), sampling_plane_n));
			float front_alpha = view_alpha + angle_add_sign * V_p_p_j_angle;
			float back_alpha  = view_alpha + angle_add_sign * V_p_p_j_back_angle;

			// Map from [0, pi] to [0, 1]
			front_alpha = clamp(front_alpha / PI, 0.0, 1.0);
			back_alpha  = clamp(back_alpha  / PI, 0.0, 1.0);

			float min_alpha = min(front_alpha, back_alpha);
			float max_alpha = max(front_alpha, back_alpha);

			uint occlusion_mask = occlusionBitMask(min_alpha, max_alpha);
			uint new_b_i = b_i | occlusion_mask;
			uint bits_changed = new_b_i & ~b_i;
			b_i  = new_b_i;

			float cos_norm_angle = dot(unit_p_to_pos_j, n);
			if((cos_norm_angle > 0.01) && (bits_changed != 0u))
			{
				vec3 n_j_vs = readNormalFromNormalTexture(pos_j_ss);

				float n_j_cos_theta = dot(n_j_vs, -unit_p_to_pos_j); // cosine of angle between surface normal at step position and vector from step position to p.

				float sin_factor = sinForCos(cos_norm_angle);

				float scalar_factors = cos_norm_angle * sin_factor * float(countSetBits(bits_changed));
				uniform_irradiance += scalar_factors;

				if(n_j_cos_theta > -0.3)
				{
					vec3 common_factors = scalar_factors * textureLod(diffuse_tex, pos_j_ss, 0.0).xyz;
					irradiance += common_factors;
				}
			}

			dist_ss += step_incr;
			last_step_incr = step_incr;
			// NOTE: we want step_incr_factor^N_s ~= 2      , e.g. step length is approximately doubled at end of stepping.
			// ln(step_incr_factor^N_s) = ln(2)
			// N_s ln(step_incr_factor) = ln(2)
			// ln(step_incr_factor) = ln(2) / N_s
			// step_incr_factor = exp(ln(2) / N_s)
			const float step_incr_factor = exp(log(2.0) / float(N_s));
			step_incr *= step_incr_factor;
		}
	}

	uniform_irradiance        *= PI * PI / float(N_d * int(SECTOR_COUNT));
	irradiance                *= PI * PI / float(N_d * int(SECTOR_COUNT));

	float irradiance_scale = 1.0 - clamp(uniform_irradiance / PI, 0.0, 1.0);

	//========================= Do a traditional trace for specular reflection ===============================
	// Reflect -V in n
	vec3 reflected_dir = reflect(-V, n);
	vec3 dir_cs = reflected_dir;

	vec2 dir_ss = normalize(rayDirCameraToScreenSpace(p, reflected_dir));

	const float max_len = 0.7f;
	float len = max_len; // distance to walk in screen space
	
	// Clip screen-space ray on screen edges.
	// Clip against left or right edge of screen
	{	
		float x_clip_val = (dir_ss.x > 0.0) ? 1.f : 0.f;
		float hit_len = (x_clip_val - origin_ss.x) / dir_ss.x;
		len = min(len, hit_len);
	}
	// Clip against top or bottom of screen
	{	
		float y_clip_val = (dir_ss.y > 0.0) ? 1.f : 0.f;
		float hit_len = (y_clip_val - origin_ss.y) / dir_ss.y;
		len = min(len, hit_len);
	}

	
	// Solve for t_1 using x or y coordinates, which ever one changes faster.
	float o_ss_xy, d_ss_xy, l_over_w_factor, o_cs_xy, d_cs_xy;
	if(abs(dir_ss.x) > abs(dir_ss.y))
	{
		o_ss_xy = origin_ss.x;
		d_ss_xy = dir_ss.x;
		l_over_w_factor = l_over_w;
		o_cs_xy = p.x;
		d_cs_xy = dir_cs.x;
	}
	else
	{
		o_ss_xy = origin_ss.y;
		d_ss_xy = dir_ss.y;
		l_over_w_factor = l_over_h;
		o_cs_xy = p.y;
		d_cs_xy = dir_cs.y;
	}

	const float step_len = max_len / 128.0;
	const int num_steps = int(len / step_len);
	vec3 spec_refl_col = vec3(0.0);
	//float last_step_incr = step_len;
	float last_dist_ss = 0.0;
	float cur_dist_ss = 0.005;// + /*last_step_incr*/step_len * (0.0);
	bool hit_something = false;
	//float prev_t = 0.0;
	float prev_step_depth = 0.0;
	float final_trace_dist_ss = 0.0;

	for(int i=0; i<num_steps; ++i)
	{
		vec2  cur_ss  = origin_ss    + dir_ss  * cur_dist_ss; // Compute current screen space position
		float p_ss_xy = o_ss_xy      + d_ss_xy * cur_dist_ss; // Screen-space pos in x or y.

		float t_1 =  (p.z*(p_ss_xy - 0.5) + o_cs_xy * l_over_w_factor) / (dir_cs.z*(-p_ss_xy + 0.5) - d_cs_xy * l_over_w_factor); // Solve for distance t_1 along camera space ray
		if(t_1 < 0.0) // TODO: solve for the distance to this singularity to avoid this branch.
			break;

		float p_cs_z = p.z + dir_cs.z * t_1; // Z coordinate of point on camera-space ray that projects onto the current screen space point
		float cur_step_depth = -p_cs_z;
		float cur_depth_buf_depth = getDepthFromDepthTexture(cur_ss); // Get depth from depth buffer for current step position
		float pen_depth = cur_step_depth - cur_depth_buf_depth; // penetration depth, > 0 if we have intersected a surface.

		//vec3 frag_pos_cs = viewSpaceFromScreenSpacePosAndDepth(cur_ss, cur_depth_buf_depth);
		//float frag_pos_cs_len = length(frag_pos_cs);
		//vec3 n_cs = readNormalFromNormalTexture(cur_ss);
		//float frag_dot = abs(dot(n_cs, frag_pos_cs)) / frag_pos_cs_len;
		float last_step_depth_delta = cur_step_depth - prev_step_depth;
		float coarse_thickness = clamp(last_step_depth_delta * 2.0, /*minval=*/1.0, /*maxval=*/30.0); // max(0.1, max(frag_pos_cs_len * 0.02 / max(0.2, frag_dot), (cur_step_depth - prev_step_depth) * 1.0));// / max(0.5, frag_dot);// max(0.1, max(t_1 - prev_t, cur_step_depth - prev_step_depth));// * frag_pos_cs_len;// * frag_pos_cs_len / max(0.1, frag_dot);
		if((pen_depth > 0.0) && (pen_depth < coarse_thickness) && (i > 0))
		{
			// Refine intersection point with binary search
			float dist_ss_a = last_dist_ss;
			float dist_ss_b = cur_dist_ss;
			for(int z=0; z<4; ++z)
			{
				float mid_dist_ss = (dist_ss_a + dist_ss_b) * 0.5;
				p_ss_xy = o_ss_xy + d_ss_xy * mid_dist_ss;
				t_1 =  (p.z*(p_ss_xy - 0.5) + o_cs_xy * l_over_w_factor) / (dir_cs.z*(-p_ss_xy + 0.5) - d_cs_xy * l_over_w_factor); // Solve for distance t_1 along camera space ray
				p_cs_z = p.z + dir_cs.z * t_1;
				cur_step_depth = -p_cs_z;

				cur_ss = origin_ss + dir_ss * mid_dist_ss;
				cur_depth_buf_depth = getDepthFromDepthTexture(cur_ss); // Get depth from depth buffer for interval midpoint position

				if(cur_step_depth > cur_depth_buf_depth) // if intersected surface:
					dist_ss_b = mid_dist_ss; // Use first half of interval
				else // Else if didn't intersect surface:
					dist_ss_a = mid_dist_ss; // Use second half of interval
			}

			//cur_ss = origin_ss    + dir_ss  * ((dist_ss_a + dist_ss_b) * 0.5);
			cur_ss = origin_ss    + dir_ss  * dist_ss_b;
			p_ss_xy = o_ss_xy     + d_ss_xy * dist_ss_b;
			t_1 =  (p.z*(p_ss_xy - 0.5) + o_cs_xy * l_over_w_factor) / (dir_cs.z*(-p_ss_xy + 0.5) - d_cs_xy * l_over_w_factor); // Solve for distance t_1 along camera space ray
			p_cs_z = p.z + dir_cs.z * t_1;
			cur_step_depth = -p_cs_z;
			cur_depth_buf_depth = getDepthFromDepthTexture(cur_ss);
			pen_depth = cur_step_depth - cur_depth_buf_depth; // recompute penetration depth
			//frag_pos_cs = viewSpaceFromScreenSpacePosAndDepth(cur_ss, cur_depth_buf_depth);
			//frag_pos_cs_len = length(frag_pos_cs);
			//n_cs = readNormalFromNormalTexture(cur_ss);
			//frag_dot = abs(dot(n_cs, frag_pos_cs)) / frag_pos_cs_lens;
			//float thickness = 10.0;//0.05;//frag_pos_cs_len * 0.01 / max(0.05, frag_dot); // 1.5 * clamp(cur_step_depth - prev_step_depth, 0.1, 10.0); // 0.1 / abs(dot(n_j_vs, view_dir));//*(t_1 - prev_t) * 2*/0.5 / max(abs(dot(n_j_vs, view_dir)), 0.01);
			float refined_thickness = clamp(last_step_depth_delta * 2.0, /*minval=*/0.1, /*maxval=*/2.0); // max(0.05, max(frag_pos_cs_len * 0.01 / max(0.2, frag_dot), (cur_step_depth - prev_step_depth) * 0.5));// / max(0.5, frag_dot);// max(0.1, max(t_1 - prev_t, cur_step_depth - prev_step_depth));// * frag_pos_cs_len;// * frag_pos_cs_len / max(0.1, frag_dot);
			if(pen_depth < refined_thickness) // if we hit something:
			{
				spec_refl_col = textureLod(diffuse_tex, cur_ss, 0.0).xyz;
				final_trace_dist_ss = dist_ss_b;
				hit_something = true;
				break;
			}
		}

	//	vec3 n_j_vs = readNormalFromNormalTexture(cur_ss);
	//
	//	vec3 p_cs = vec3(
	//		(cur_ss.x - 0.5) * cur_depth_buf_depth / l_over_w,
	//		(cur_ss.y - 0.5) * cur_depth_buf_depth / l_over_h,
	//		-cur_depth_buf_depth
	//	);
	//
	//	//vec3 p_cs = viewSpaceFromScreenSpacePos(cur_ss);
	//	vec3 view_dir = normalize(p_cs);

		// Form plane at object surface
		// intersect ray with plane
		// project intersection point back

		//debug_val = abs(dot(n_j_vs, view_dir));
	//	debug_val = vec3(abs(dot(n_j_vs, view_dir)));

		//float thickness = 0.1;// / max(0.6, abs(dot(n_j_vs, view_dir))); // 1.5 * clamp(cur_step_depth - prev_step_depth, 0.1, 10.0); // 0.1 / abs(dot(n_j_vs, view_dir));//*(t_1 - prev_t) * 2*/0.5 / max(abs(dot(n_j_vs, view_dir)), 0.01);
		//if((pen_depth > 0.0) && (pen_depth < thickness)) //   && (cur_depth_buf_depth > intersection_depth_threshold)) // if we hit something:
		//{
		//	spec_refl_col = textureLod(diffuse_tex, cur_ss, 0.0).xyz;
		//	hit_something = true;
		//	break;
		//}

		//prev_t = t_1;
		prev_step_depth = cur_step_depth;
		last_dist_ss = cur_dist_ss;
		cur_dist_ss += step_len;//last_step_incr;
	}


	const float final_roughness = textureLod(diffuse_tex, texture_coords, 0.0).w;

	//========================= Mix in unoccluded specular env light ============================
	if(!hit_something)
	{
		final_trace_dist_ss = max_len;

		vec3 reflected_dir_ws = (transpose(frag_view_matrix) * vec4(reflected_dir, 0.0)).xyz; // NOTE: transpose could be very slow

		// Look up env map for reflected dir
		int map_lower = int(final_roughness * 6.9999);
		int map_higher = map_lower + 1;
		float map_t = final_roughness * 6.9999 - float(map_lower);

		float refl_theta = fastApproxACos(reflected_dir_ws.z);
		float refl_phi = fastApproxAtan(reflected_dir_ws.y, reflected_dir_ws.x) - env_phi; // env_phi term is to rotate reflection so it aligns with env rotation.
		// Note: specular_env_tex is just one side of the sphere of directions, so phi varies from 0 to pi.
		vec2 refl_map_coords = vec2(refl_phi * (1.0 / PI), clamp(refl_theta * (1.0 / PI), 1.0 / 64.0, 1.0 - 1.0 / 64.0)); // Clamp to avoid texture coord wrapping artifacts.

		vec4 spec_refl_light_lower  = texture(specular_env_tex, vec2(refl_map_coords.x, float(map_lower)  * (1.0/8.0) + refl_map_coords.y * (1.0/8.0))); //  -refl_map_coords / 8.0 + map_lower  * (1.0 / 8)));
		vec4 spec_refl_light_higher = texture(specular_env_tex, vec2(refl_map_coords.x, float(map_higher) * (1.0/8.0) + refl_map_coords.y * (1.0/8.0)));
		vec4 spec_refl_light = spec_refl_light_lower * (1.0 - map_t) + spec_refl_light_higher * map_t; // spectral radiance * 1.0e-9

		spec_refl_col = spec_refl_light.xyz;
	}

	// d_omega = sin(theta) dtheta dphi
	// where dtheta = pi / SECTOR_COUNT = pi / SECTOR_COUNT
	// and dphi = pi / N_d
	// so
	// d_omega = sin(theta) (pi / SECTOR_COUNT) (pi / N_d)
	// = sin(theta) pi^2 / (SECTOR_COUNT * N_d)
	

	// Irradiance
	// E = Integral(0, pi/2, Integral(0, pi, L_i(omega_i(phi, theta)) |omega_i(phi, theta), n|  d phi) d theta)
	// if L_i(omega_i(phi, theta)) is constant, e.g. L_i(omega_i(phi, theta)) = L_i, then
	// E = Integral(0, pi/2, Integral(0, pi, L_i |omega_i(phi, theta), n|  d phi) d theta)
	// E = L_i pi					[ See https://pbr-book.org/3ed-2018/Color_and_Radiometry/Working_with_Radiometric_Integrals]

	irradiance_out = vec4(irradiance, irradiance_scale);
	specular_spec_rad_out = vec4(spec_refl_col, final_roughness * final_trace_dist_ss);
}
