

uniform sampler2D albedo_texture; // source texture

uniform sampler2D depth_normal_tex; // xyz = cam space normal, w = linear depth, or -1 where no object was drawn.  Written by compute_ssao_frag_shader.glsl.

uniform int is_ssao_blur;
uniform int blur_x;

in vec2 pos; // [0, 1] x [0, 1]

out vec4 colour_out;


vec3 camSpaceFromScreenSpacePos(vec2 normed_pos_ss, float depth)
{
	return vec3(
		(normed_pos_ss.x - 0.5) * depth * w_over_l,
		(normed_pos_ss.y - 0.5) * depth * h_over_l,
		-depth
	);
}


// See MitchellNetravali.h
float mitchellNetravaliEval(float x)
{
	float B = 1.0f; // max blur
	float C = 0.0f;

	float region_0_a = (float(12)  - B*9  - C*6) * (1.f/6);
	float region_0_b = (float(-18) + B*12 + C*6) * (1.f/6);
	float region_0_d = (float(6)   - B*2       ) * (1.f/6);

	float region_1_a = (-B - C*6)                * (1.f/6);
	float region_1_b = (B*6 + C*30)              * (1.f/6);
	float region_1_c = (B*-12 - C*48)            * (1.f/6);
	float region_1_d = (B*8 + C*24)              * (1.f/6);

	float region_0_f = region_0_a * (x*x*x) + region_0_b * (x*x) + region_0_d;
	float region_1_f = region_1_a * (x*x*x) + region_1_b * (x*x) + region_1_c * x + region_1_d;
	if(x < 1.0)
		return region_0_f;
	else if(x < 2.0)
		return region_1_f;
	else
		return 0.0;
}


const float MAX_SPEC_BLUR_RADIUS = 25.0;


// Blur radius, in texels, for a specular reflection texel, from its roughness * trace distance.  TODO: make radius calculation not ad hoc
float specBlurRadius(float roughness_times_trace_dist)
{
	return clamp(roughness_times_trace_dist * 200.0, 3.0, MAX_SPEC_BLUR_RADIUS);
}


// Weight of a tap at the given offset (in texels) from the texel being blurred.
// The SSAO blur uses a fixed radius.
// The specular blur spreads each tap over that tap's own blur radius (scatter-as-gather), so how far a reflected object is blurred depends on the trace
// distance to that object, not on the trace distances of the neighbouring texels, which may have hit something behind it, or nothing.
float tapWeight(int offset, vec4 tap_val, float ssao_radius_scale)
{
	if(is_ssao_blur != 0)
		return mitchellNetravaliEval(float(abs(offset)) * ssao_radius_scale);
	else
	{
		float tap_radius = specBlurRadius(tap_val.w);
		return mitchellNetravaliEval(float(abs(offset)) * (2.0 / tap_radius)) * (1.0 / tap_radius); // Divide by the radius so that every tap's kernel has the same total weight.
	}
}


void main()
{
	ivec2 tex_res = textureSize(albedo_texture, /*mip level*/0);
	
	ivec2 px_coords = ivec2(int(float(tex_res.x) * pos.x), int(float(tex_res.y) * pos.y));

	vec4 centre_depth_normal = texelFetch(depth_normal_tex, px_coords, /*mip level=*/0);
	float centre_depth = centre_depth_normal.w;
	if(centre_depth < 0.0) // If nothing was drawn here in the prepass:
	{
		// The main pass only uses SSAO texels whose prepass depth matches the fragment depth, so this texel's value is never used.
		colour_out = vec4(0.0, 0.0, 0.0, 1.0);
		return;
	}

	vec3 centre_n_cs = centre_depth_normal.xyz;
	vec3 centre_p_cs = camSpaceFromScreenSpacePos(pos, centre_depth); // View/camera space 'fragment' position

	float V_dot_n = abs(dot(centre_n_cs, centre_p_cs)) / length(centre_p_cs);
	float depth_thresh = 0.03 * centre_depth / max(0.05, V_dot_n);


	// For the specular blur, any tap within the max radius may spread to this texel, see tapWeight().
	float radius = (is_ssao_blur != 0) ? 5.0 : MAX_SPEC_BLUR_RADIUS;
	int r = int(radius + 0.9999); // round up
	float radius_scale = 2.f / radius;

	vec4 val = vec4(0.0);
	float sum_weight = 0.0;
	if(blur_x != 0) // If should blur in x direction:
	{
		int y = px_coords.y;
		int x_begin = max(px_coords.x - r, 0); // Clamp the tap range to the texture, rather than checking each tap.
		int x_end   = min(px_coords.x + r, tex_res.x - 1);
		for(int x = x_begin; x <= x_end; ++x)
		{
			vec4 depth_normal = texelFetch(depth_normal_tex, ivec2(x, y), /*mip level=*/0); // Taps where nothing was drawn have depth -1, so fail the depth test.
			if((abs(depth_normal.w - centre_depth) < depth_thresh) && (dot(centre_n_cs, depth_normal.xyz) > 0.7))
			{
				vec4 tap_val = texelFetch(albedo_texture, ivec2(x, y), /*mip level=*/0);
				float weight = tapWeight(x - px_coords.x, tap_val, radius_scale);
				val += tap_val * weight;
				sum_weight += weight;
			}
		}
	}
	else // else if should blur in y direction:
	{
		int x = px_coords.x;
		int y_begin = max(px_coords.y - r, 0); // Clamp the tap range to the texture, rather than checking each tap.
		int y_end   = min(px_coords.y + r, tex_res.y - 1);
		for(int y = y_begin; y <= y_end; ++y)
		{
			vec4 depth_normal = texelFetch(depth_normal_tex, ivec2(x, y), /*mip level=*/0); // Taps where nothing was drawn have depth -1, so fail the depth test.
			if((abs(depth_normal.w - centre_depth) < depth_thresh) && (dot(centre_n_cs, depth_normal.xyz) > 0.7))
			{
				vec4 tap_val = texelFetch(albedo_texture, ivec2(x, y), /*mip level=*/0);
				float weight = tapWeight(y - px_coords.y, tap_val, radius_scale);
				val += tap_val * weight;
				sum_weight += weight;
			}
		}
	}

	val /= sum_weight;

	colour_out = val;
}
