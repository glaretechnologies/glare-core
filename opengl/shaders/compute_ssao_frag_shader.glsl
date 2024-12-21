
uniform sampler2D diffuse_tex;
uniform sampler2D normal_tex;
uniform sampler2D depth_tex;

uniform sampler2D blue_noise_tex;
uniform samplerCube cosine_env_tex;
uniform sampler2D specular_env_tex;



in vec2 texture_coords;


//uniform vec2 iResolution;
//uniform mat4 pmat; // proj matrix
//uniform mat4 ipmat; // inverse proj matrix

layout(location = 0) out vec4 irradiance_out;
layout(location = 1) out vec3 specular_spec_rad_out;


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


// See 'Calculations for recovering depth values from depth buffer', OpenGLEngine.cpp.
float getDepthFromDepthTexture(vec2 normed_pos_ss)
{
#if USE_REVERSE_Z
	return near_clip_dist / texture(depth_tex, normed_pos_ss).x;
#else
	return -near_clip_dist / (texture(depth_tex, normed_pos_ss).x - 1.0);
#endif
}


vec3 viewSpaceFromScreenSpacePos(vec2 normed_pos_ss)
{
	float depth = getDepthFromDepthTexture(normed_pos_ss); // depth := -pos_cs.z
 
	return vec3(
		(normed_pos_ss.x - 0.5) * depth / l_over_w,
		(normed_pos_ss.y - 0.5) * depth / l_over_h,
		-depth
	);
}

// Returns coords in [0, 1] for visible positions
vec2 cameraToScreenSpace(vec3 pos_cs)
{
	return vec2(
		pos_cs.x / -pos_cs.z * l_over_w + 0.5,
		pos_cs.y / -pos_cs.z * l_over_h + 0.5
	);
}

const float PI = 3.1415926535897932384626433832795;


//#define SECTOR_COUNT 32
const uint SECTOR_COUNT = 32u;

// From https://cdrinmatane.github.io/posts/ssaovb-code/
//uint updateSectorBitMask(float minHorizon, float maxHorizon, uint globalOccludedBitfield)
//{
//    uint startHorizonInt = uint(minHorizon * SECTOR_COUNT);
//    uint angleHorizonInt = uint(ceil((maxHorizon - minHorizon) * SECTOR_COUNT));
//    uint angleHorizonBitfield = angleHorizonInt > 0u ? (0xFFFFFFFFu >> (SECTOR_COUNT - angleHorizonInt)) : 0u;
//    uint currentOccludedBitfield = angleHorizonBitfield << startHorizonInt;
//    return globalOccludedBitfield | currentOccludedBitfield;
//}
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


float square(float x) { return x*x; }
float pow4(float x) { return (x*x)*(x*x); }

float heaviside(float x)
{
	return (x > 0.0) ? 1.0 : 0.0;
}

float trowbridgeReitzPDF(float cos_theta, float alpha2)
{
	return cos_theta * alpha2 / (3.1415926535897932384626433832795 * square(square(cos_theta) * (alpha2 - 1.0) + 1.0));
}

vec3 removeComponentInDir(vec3 v, vec3 unit_dir)
{
	return v - unit_dir * dot(v, unit_dir);
}


// From https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.pdf, B.3.2. Specular BRDF
// Note that we know dot(H, L) >= 0 and dot(H, V) >= from how we constructed H.
float smithMaskingShadowingV(vec3 N, /*vec3 H, */vec3 L, vec3 V, float alpha2)
{
	/*float N_dot_L = dot(N, L);
	float N_dot_V = dot(N, V);
	return 
		heaviside(dot(H, L)) / (N_dot_L + sqrt(alpha2 + (1.0 - alpha2)*square(N_dot_L))) * 
		heaviside(dot(H, V)) / (N_dot_V + sqrt(alpha2 + (1.0 - alpha2)*square(N_dot_V)));*/
	float N_dot_L = dot(N, L);
	float N_dot_V = dot(N, V);
	return 1.0 / (
		(N_dot_L + sqrt(alpha2 + (1.0 - alpha2)*square(N_dot_L))) * 
		(N_dot_V + sqrt(alpha2 + (1.0 - alpha2)*square(N_dot_V)))
	);
}

float alpha2ForRoughness(float r) { return pow4(r); }


// See "Screen Space Indirect Lighting with Visibility Bitmask", PDF: https://arxiv.org/pdf/2301.11376
// See "SSAO using Visibility Bitmasks" blog post: https://cdrinmatane.github.io/posts/ssaovb-code/

void main()
{
	//------------------------------------------
	//Resolution = iResolution;
	//isPerspectiveCam = true;
	//nearZ = near_clip_dist; // looks like expected to be positive.
	//cpos = mat_common_campos_ws.xyz;
	//world_to_cam = mat3(frag_view_matrix[0].xyz, frag_view_matrix[1].xyz, frag_view_matrix[2].xyz);
	//cam_to_world = transpose(world_to_cam);
	//------------------------------------------

	//float depth_tex_val = textureLod(depth_tex, texture_coords, 0.0).x;
	float pixel_hash = texture(blue_noise_tex, gl_FragCoord.xy * (1.f / 128.f)).x;

	vec3 src_normal_encoded = textureLod(normal_tex, texture_coords, 0.0).xyz; // Encoded as a RGB8 texture (converted to floating point)
	vec3 src_normal_ws = oct_to_float32x3(unorm8x3_to_snorm12x2(src_normal_encoded)); // Read normal from normal texture

	vec3 n_p = normalize((frag_view_matrix * vec4(src_normal_ws, 0)).xyz); // View space 'fragment' normal
	vec3 p = viewSpaceFromScreenSpacePos(texture_coords); // View space 'fragment' position
	vec3 unit_p = normalize(p);
	vec3 V = -unit_p; // View vector: vector from 'fragment' position to camera in view/camera space
	
	vec2 origin_ss = texture_coords; // Origin of stepping in screen space


	// Form basis in camera/view space aligned with fragment surface
	vec3 tangent = normalize(removeComponentInDir(V, n_p));
	vec3 bitangent = cross(tangent, n_p);

	//uint pixel_id = uint(gl_FragCoord.x + gl_FragCoord.y * iResolution.x);

	float final_roughness = 0.3; // TEMP

	float thickness = 0.1;
	float r = 0.4; // Total stepping radius in screen space
	int N_s = 150; // Number of steps per direction
	float step_size = r / (float(N_s) + 1.0);

	int N_d = 116; // num directions

	//float ao = 0.0; // Accumulated ambient occlusion. 1 = not occluded at all, 0 = totally occluded.
	//float unocclusion = 0.0;
	float uniform_irradiance = 0.0;
	vec3 irradiance = vec3(0.0);
	vec3 specular = vec3(0.0);
	float uniform_specular_spec_rad = 0.0;

	float debug_val = 0.0;
	for(int i=0; i<N_d; ++i) // For each direction:
	{
#if 1
		float theta_cs = PI * (float(i) + pixel_hash * 0.5) / float(N_d);
		vec3 d_i_vs = tangent * cos(theta_cs) + bitangent * sin(theta_cs);
		vec3 offset_p_vs = p + d_i_vs * 1.0e-2;
		vec2 offset_p_ss = cameraToScreenSpace(offset_p_vs);
		vec2 d_i_ss = normalize(offset_p_ss - origin_ss);
#else
		// Direction d_i is given by the view space direction (1,0,0) rotated around (0, 0, 1) by theta = 2pi * i / N_d
		float theta = PI * float(i) / float(N_d);
		vec3 d_i_vs = vec3(cos(theta), sin(theta), 0); // direction i (d_i) in view/camera space
		vec2 d_i_ss = d_i_vs.xy; // direction i (d_i) in normalised screen space
#endif

		d_i_vs = d_i_vs - V * dot(V, d_i_vs); // Make d_i_vs orthogonal to view vector. NOTE: needed?

		vec3 d_i_vs_cross_v = normalize(cross(d_i_vs, V)); // NOTE: need normalize?  Get vector normal to sampling plane and orthogonal to view vector.
		vec3 projected_n_p = normalize(n_p - d_i_vs_cross_v * dot(n_p, d_i_vs_cross_v)); // fragment surface normal projected into sampling plane

		// Get angle between projected normal and view vector
		float N = acos(dot(projected_n_p, V)) * sign(dot(d_i_vs_cross_v, cross(projected_n_p, V))) + ((pixel_hash - 0.5f) * PI/ float(SECTOR_COUNT));
		

		uint b_i = 0u; // bitmask (= globalOccludedBitfield)
		for(int j=1; j<=N_s; ++j) // For each step
		{
			float dir_sign = (j % 2 == 0) ? 1.f : -1.f;

			vec2 pos_j_ss = origin_ss + d_i_ss * dir_sign * step_size * float(j); // step_j position in screen space
			if(!(pos_j_ss.x >= 0.0 && pos_j_ss.y <= 1.0 && pos_j_ss.y >= 0.0 && pos_j_ss.y <= 1.0))
				continue;
			
			vec3 pos_j_vs = viewSpaceFromScreenSpacePos(pos_j_ss); // step_j position in camera/view space

			vec3 back_pos_j_vs = pos_j_vs - V * thickness; // position of guessed 'backside' of step position in camera/view space

			vec3 p_to_pos_j_vs = pos_j_vs - p; // Vector from fragment position to step position
			vec3 p_to_back_pos_j_vs = back_pos_j_vs - p;
			float front_view_angle = acos(dot(normalize(     p_to_pos_j_vs), V)); // Angle between view vector (frag to cam) and frag-to-step_j position.
			float back_view_angle  = acos(dot(normalize(p_to_back_pos_j_vs), V)); // Angle between view vector (frag to cam) and frag-to-back_step_j position.

			

			float theta_min = min(front_view_angle, back_view_angle);
			float theta_max = max(front_view_angle, back_view_angle);

			//float front_angle = front_view_angle - N;
			


			//debug_val = clamp(theta_min, 0.0, 1.0);

			// Adjust to get min and max angles around sampling-plane normal
			float min_angle_from_proj_N = theta_min - N * dir_sign;
			float min_angle_from_proj_N_01 = clamp((min_angle_from_proj_N + (0.5 * PI)) / PI, 0.0, 1.0); // Map from [-pi/2, pi/2] to [0, 1]

			float max_angle_from_proj_N = theta_max - N * dir_sign;
			float max_angle_from_proj_N_01 = clamp((max_angle_from_proj_N + (0.5 * PI)) / PI, 0.0, 1.0); // Map from [-pi/2, pi/2] to [0, 1]

			uint occlusion_mask = occlusionBitMask(min_angle_from_proj_N_01, max_angle_from_proj_N_01);
			uint new_b_i = b_i | occlusion_mask;
			uint bits_changed = new_b_i & ~b_i;
			b_i  = new_b_i;

			float p_to_pos_j_vs_len = length(p_to_pos_j_vs);
			vec3 unit_p_to_pos_j_vs = normalize(p_to_pos_j_vs);//TEMP
			float cos_norm_angle = dot(unit_p_to_pos_j_vs, n_p);
			if(cos_norm_angle > 0.0)
			{
				vec3 n_j_encoded = textureLod(normal_tex, pos_j_ss, 0.0).xyz; // Encoded as a RGB8 texture (converted to floating point)
				vec3 n_j_ws = oct_to_float32x3(unorm8x3_to_snorm12x2(n_j_encoded)); // Read normal from normal texture
				vec3 n_j_vs = normalize((frag_view_matrix * vec4(n_j_ws, 0)).xyz); // unnormalised TEMP: normalizing
				float n_j_cos_theta = dot(n_j_vs, -unit_p_to_pos_j_vs); // cosine of angle between surface normal at step position and vector from step position to p.
				//if(n_j_cos_theta > 0.0) // dot(n_j_ss, p_to_pos_j_vs) < 0)
				{
					float norm_angle = acos(cos_norm_angle);
					float sin_factor = sin(norm_angle);

					
					vec3 h = normalize(unit_p_to_pos_j_vs + V); // half-vector in view/camera space
					float h_cos_theta = max(0.0, dot(h, n_p));
					float alpha2 = alpha2ForRoughness(final_roughness);
					float D = trowbridgeReitzPDF(h_cos_theta, alpha2);// * specular_fresnel * shadow_factor;

					//float smithMaskingShadowing(vec3 N, vec3 H, vec3 L, vec3 V)
					//if(dot(h, unit_p_to_pos_j_vs) > 0.0 && dot(h, V) > 0.0) // heaviside(dot(H, L)) * heaviside(dot(H, V))
					{
						float V = smithMaskingShadowingV(n_p, /*h, */unit_p_to_pos_j_vs, V, alpha2);

						float specular_brdf = D * V;

						//float sin_factor = 1.f;
						//if(texture_coords.x >= 0.49 && texture_coords.x <= 0.51 &&
						//	texture_coords.y >= -0.1 && texture_coords.y <= 0.1)
						{
							//debug_val = n_j_vs.z;
							float scalar_factors = cos_norm_angle * sin_factor * float(countSetBits(bits_changed));
							uniform_irradiance += scalar_factors;
							vec3 common_factors = scalar_factors * textureLod(diffuse_tex, pos_j_ss, 0.0).xyz;
							irradiance += /*n_j_cos_theta * */common_factors;
							specular += specular_brdf * common_factors;
							uniform_specular_spec_rad += specular_brdf * scalar_factors;
						}
					}
				}
			}
		}

		//ao += 1.0 - float(countSetBits(b_i)) * (1.0 / float(SECTOR_COUNT));
	}

	//ao       *= 1.0 / float(N_d);
	uniform_irradiance        *= PI * PI / float(N_d * SECTOR_COUNT);
	uniform_specular_spec_rad *= PI * PI / float(N_d * SECTOR_COUNT);
	irradiance                *= PI * PI / float(N_d * SECTOR_COUNT);
	specular                  *= PI * PI / float(N_d * SECTOR_COUNT);

	// Mix in unoccluded environment lighting from env map
	vec3 sky_irradiance = texture(cosine_env_tex, src_normal_ws).xyz; // integral over hemisphere of cosine * incoming radiance from sky * 1.0e-9

	float irradiance_scale = 1.0 - clamp(uniform_irradiance / PI, 0.0, 1.0); // NOTE: hack 2.0 factor
	irradiance += irradiance_scale * sky_irradiance;


	//========================= Mix in unoccluded specular env light ============================
	vec3 frag_dir_ws = (transpose(frag_view_matrix) * vec4(unit_p, 0.0)).xyz;
	vec3 reflected_dir_ws = reflect(frag_dir_ws, src_normal_ws);

	// Look up env map for reflected dir
	int map_lower = int(final_roughness * 6.9999);
	int map_higher = map_lower + 1;
	float map_t = final_roughness * 6.9999 - float(map_lower);

	float refl_theta = acos(reflected_dir_ws.z);
	float refl_phi = atan(reflected_dir_ws.y, reflected_dir_ws.x) - env_phi; // env_phi term is to rotate reflection so it aligns with env rotation.
	vec2 refl_map_coords = vec2(refl_phi * (1.0 / 6.283185307179586), clamp(refl_theta * (1.0 / 3.141592653589793), 1.0 / 64.0, 1.0 - 1.0 / 64.0)); // Clamp to avoid texture coord wrapping artifacts.

	vec4 spec_refl_light_lower  = texture(specular_env_tex, vec2(refl_map_coords.x, float(map_lower)  * (1.0/8.0) + refl_map_coords.y * (1.0/8.0))); //  -refl_map_coords / 8.0 + map_lower  * (1.0 / 8)));
	vec4 spec_refl_light_higher = texture(specular_env_tex, vec2(refl_map_coords.x, float(map_higher) * (1.0/8.0) + refl_map_coords.y * (1.0/8.0)));
	vec4 spec_refl_light = spec_refl_light_lower * (1.0 - map_t) + spec_refl_light_higher * map_t; // spectral radiance * 1.0e-9

	float env_scale = 1.0 - clamp(uniform_specular_spec_rad * 2.0, 0.0, 1.0);
	//specular += env_scale * spec_refl_light.xyz;

//
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

	//float thickness = 0.01;
	//uint N_b = 32; // bitmask size
	//vec3 p = (frag_view_matrix * vec4(pos_ws, 1)).xyz; // View space fragment position


	//colour_out = vec4(ao, ao, ao, 1.f);
	//n_p.z = -n_p.z;
	//colour_out = vec4(clamp(n_p.x, 0.0, 1.0), 1.f);
	//colour_out = vec4(vec3(clamp(-n_p.z, 0.0, 1.0)), 1.f);
	
	
	irradiance_out = vec4(irradiance, 1.0);
	specular_spec_rad_out = specular;
	//colour_out = vec4(vec3(ao), 1.0);

	//TEMP HACK:
	//vec2 pos_j_ss = origin_ss; // step_j position in screen space
	//vec3 pos_j_vs = viewSpaceFromScreenSpacePos(pos_j_ss); // step_j position in camera/view space
	//vec3 pos_j_ss_col = textureLod(diffuse_tex, pos_j_ss, 0.0).xyz;

	//colour_out = vec4(vec3(debug_val * 0.1), 1.0);
}
