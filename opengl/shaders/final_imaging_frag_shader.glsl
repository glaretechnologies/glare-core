
#if MAIN_BUFFER_MSAA_SAMPLES > 1
uniform sampler2DMS albedo_texture; // main colour buffer
//uniform sampler2DMS transparent_accum_texture;
//uniform sampler2DMS total_transmittance_texture;
#else
uniform sampler2D albedo_texture; // main colour buffer
//uniform sampler2D transparent_accum_texture;
//uniform sampler2D total_transmittance_texture;
#endif

uniform sampler2D blur_tex_0;
uniform sampler2D blur_tex_1;
uniform sampler2D blur_tex_2;
uniform sampler2D blur_tex_3;
uniform sampler2D blur_tex_4;
uniform sampler2D blur_tex_5;
uniform sampler2D blur_tex_6;
uniform sampler2D blur_tex_7;


uniform float bloom_strength;
uniform float exposure_factor;
uniform float saturation_multiplier;

in vec2 pos; // [0, 1] x [0, 1]

out vec4 colour_out;


// Based on https://mattlockyer.github.io/iat455/documents/rgb-hsv.pdf
// Could also use https://gist.github.com/983/e170a24ae8eba2cd174f which seems slightly faster.
vec3 rgb2hsv(vec3 c)
{
	float m_max = max(c.r, max(c.g, c.b));
	float m_min = min(c.r, min(c.g, c.b));
	float delta = m_max - m_min;

	float H;
	if(delta == 0.0)
		H = 0.0;
	else if(m_max == c.r)
		H = (c.g - c.b) / delta;
	else if(m_max == c.g)
		H = (c.b - c.r) / delta + 2.0;
	else
		H = (c.r - c.g) / delta + 4.0;

	float V = m_max;
	float S = 0.0;
	if(V != 0.0)
		S = delta / V;
	
	return vec3(H, S, V);
}

vec3 hsv2rgb(vec3 c)
{
	float H = fract(c.r * (1.0/6.0)) * 6.0; // H = c.r mod 6
	float S = c.g;
	float V = c.b;
	float alpha = V * (1.0 - S);
	float beta = V * (1.0 - fract(H) * S);
	float gamma = V * (1.0 - (1.0 - fract(H)) * S);
	if(H < 1.0)
		return vec3(V, gamma, alpha);
	else if(H < 2.0)
		return vec3(beta, V, alpha);
	else if(H < 3.0)
		return vec3(alpha, V, gamma);
	else if(H < 4.0)
		return vec3(alpha, beta, V);
	else if(H < 5.0)
		return vec3(gamma, alpha, V);
	else
		return vec3(V, alpha, beta);
}


// See MitchellNetravali.h
float mitchellNetravaliEval(float x)
{
	const float B = 0.5f;
	const float C = 0.25f;

	const float region_0_a = (float(12)  - B*9  - C*6) * (1.f/6);
	const float region_0_b = (float(-18) + B*12 + C*6) * (1.f/6);
	const float region_0_d = (float(6)   - B*2       ) * (1.f/6);

	const float region_1_a = (-B - C*6)                * (1.f/6);
	const float region_1_b = (B*6 + C*30)              * (1.f/6);
	const float region_1_c = (B*-12 - C*48)            * (1.f/6);
	const float region_1_d = (B*8 + C*24)              * (1.f/6);

	float x2 = x*x;
	float x3 = x2*x;

	if(x < 1.0)
		return region_0_a * x3 + region_0_b * x2 + region_0_d;
	else if(x < 2.0)
		return region_1_a * x3 + region_1_b * x2 + region_1_c * x + region_1_d;
	else
		return 0.0;
}

/*
sampleTexHighQual
-----------------
We will use a Mitchell-Netravali cubic, radially symmetric filter.

Only handling magnification (without aliasing) currently, because we don't increase the filter size past a radius of 2.

We will also normalise the weight sum to avoid slight variations from a weight sum of 1 between integer sample locations.

Mitchell-Netravali has a support radius of 2 pixels, so we will read from these samples:

               ^ y
               |
               |        *            *            *              *
               |       
  floor(py)+2  -       
               |       
               |       
               |        *            *            *              *
               |       
               |                        +
  floor(py)+1  -                      (px, py)
               |       
               |        *            *            *              *
               |       
               |       
  floor(py)    -       
               |       
               |        *            *            *              *    
               |
               |                      range of x values that get mapped to texel floor(px-0.5)
  floor(py)-1  -                     |============|
               |--|------------|------------|--------------|---->x 
                  |            |            |              |
        floor(px-0.5)-1  floor(px-0.5)  floor(px-0.5)+1    floor(px-0.5)+2
*/
vec4 sampleTexHighQual(in sampler2D tex, float u, float v)
{
	vec2 normed_frac_part = vec2(u, v);

	ivec2 src_res = textureSize(tex, /*mip level=*/0);
	int width  = src_res.x;
	int height = src_res.y;

	float f_pixels_x = normed_frac_part.x * width;
	float f_pixels_y = normed_frac_part.y * height;

	int i_pixels_clamped_x = int(floor(f_pixels_x - 0.5));
	int i_pixels_clamped_y = int(floor(f_pixels_y - 0.5));

	int i_pixels_x = min(i_pixels_clamped_x, width  - 1); // clamp to <= (width-1, height-1).
	int i_pixels_y = min(i_pixels_clamped_y, height - 1);

	int ut = max(i_pixels_x, 0);
	int vt = max(i_pixels_y, 0);
	int ut_1 = min(i_pixels_clamped_x + 1, width  - 1);
	int vt_1 = min(i_pixels_clamped_y + 1, height - 1);
	int ut_2 = min(i_pixels_clamped_x + 2, width  - 1);
	int vt_2 = min(i_pixels_clamped_y + 2, height - 1);
	int ut_minus_1 = max(i_pixels_clamped_x - 1, 0);
	int vt_minus_1 = max(i_pixels_clamped_y - 1, 0);


	const vec4  v0 = texelFetch(tex, ivec2(ut_minus_1, vt_minus_1), /*lod=*/0);
	const vec4  v1 = texelFetch(tex, ivec2(ut,         vt_minus_1), /*lod=*/0);
	const vec4  v2 = texelFetch(tex, ivec2(ut_1,       vt_minus_1), /*lod=*/0);
	const vec4  v3 = texelFetch(tex, ivec2(ut_2,       vt_minus_1), /*lod=*/0);
	const vec4  v4 = texelFetch(tex, ivec2(ut_minus_1, vt        ), /*lod=*/0);
	const vec4  v5 = texelFetch(tex, ivec2(ut,         vt        ), /*lod=*/0);
	const vec4  v6 = texelFetch(tex, ivec2(ut_1,       vt        ), /*lod=*/0);
	const vec4  v7 = texelFetch(tex, ivec2(ut_2,       vt        ), /*lod=*/0);
	const vec4  v8 = texelFetch(tex, ivec2(ut_minus_1, vt_1      ), /*lod=*/0);
	const vec4  v9 = texelFetch(tex, ivec2(ut,         vt_1      ), /*lod=*/0);
	const vec4 v10 = texelFetch(tex, ivec2(ut_1,       vt_1      ), /*lod=*/0);
	const vec4 v11 = texelFetch(tex, ivec2(ut_2,       vt_1      ), /*lod=*/0);
	const vec4 v12 = texelFetch(tex, ivec2(ut_minus_1, vt_2      ), /*lod=*/0);
	const vec4 v13 = texelFetch(tex, ivec2(ut,         vt_2      ), /*lod=*/0);
	const vec4 v14 = texelFetch(tex, ivec2(ut_1,       vt_2      ), /*lod=*/0);
	const vec4 v15 = texelFetch(tex, ivec2(ut_2,       vt_2      ), /*lod=*/0);

	float left_pixel_x = floor(f_pixels_x - 0.5) + 0.5;
	float bot_pixel_y  = floor(f_pixels_y - 0.5) + 0.5;

	const float w0  = mitchellNetravaliEval(sqrt(square(f_pixels_x - (left_pixel_x - 1)) + square(f_pixels_y - (bot_pixel_y - 1))));
	const float w1  = mitchellNetravaliEval(sqrt(square(f_pixels_x - (left_pixel_x - 0)) + square(f_pixels_y - (bot_pixel_y - 1))));
	const float w2  = mitchellNetravaliEval(sqrt(square(f_pixels_x - (left_pixel_x + 1)) + square(f_pixels_y - (bot_pixel_y - 1))));
	const float w3  = mitchellNetravaliEval(sqrt(square(f_pixels_x - (left_pixel_x + 2)) + square(f_pixels_y - (bot_pixel_y - 1))));
	const float w4  = mitchellNetravaliEval(sqrt(square(f_pixels_x - (left_pixel_x - 1)) + square(f_pixels_y - (bot_pixel_y + 0))));
	const float w5  = mitchellNetravaliEval(sqrt(square(f_pixels_x - (left_pixel_x - 0)) + square(f_pixels_y - (bot_pixel_y + 0))));
	const float w6  = mitchellNetravaliEval(sqrt(square(f_pixels_x - (left_pixel_x + 1)) + square(f_pixels_y - (bot_pixel_y + 0))));
	const float w7  = mitchellNetravaliEval(sqrt(square(f_pixels_x - (left_pixel_x + 2)) + square(f_pixels_y - (bot_pixel_y + 0))));
	const float w8  = mitchellNetravaliEval(sqrt(square(f_pixels_x - (left_pixel_x - 1)) + square(f_pixels_y - (bot_pixel_y + 1))));
	const float w9  = mitchellNetravaliEval(sqrt(square(f_pixels_x - (left_pixel_x - 0)) + square(f_pixels_y - (bot_pixel_y + 1))));
	const float w10 = mitchellNetravaliEval(sqrt(square(f_pixels_x - (left_pixel_x + 1)) + square(f_pixels_y - (bot_pixel_y + 1))));
	const float w11 = mitchellNetravaliEval(sqrt(square(f_pixels_x - (left_pixel_x + 2)) + square(f_pixels_y - (bot_pixel_y + 1))));
	const float w12 = mitchellNetravaliEval(sqrt(square(f_pixels_x - (left_pixel_x - 1)) + square(f_pixels_y - (bot_pixel_y + 2))));
	const float w13 = mitchellNetravaliEval(sqrt(square(f_pixels_x - (left_pixel_x - 0)) + square(f_pixels_y - (bot_pixel_y + 2))));
	const float w14 = mitchellNetravaliEval(sqrt(square(f_pixels_x - (left_pixel_x + 1)) + square(f_pixels_y - (bot_pixel_y + 2))));
	const float w15 = mitchellNetravaliEval(sqrt(square(f_pixels_x - (left_pixel_x + 2)) + square(f_pixels_y - (bot_pixel_y + 2))));

	float filter_sum = 
		w0 	+
		w1 	+
		w2 	+
		w3 	+
			
		w4 	+
		w5 	+
		w6 	+
		w7 	+
			
		w8 	+
		w9 	+
		w10	+
		w11	+
			
		w12	+
		w13	+
		w14	+
		w15;

	const vec4 sum = 
		(((v0  * w0 +
		   v1  * w1) +
		  (v2  * w2 +
		   v3  * w3)) +

		 ((v4  * w4 +
		   v5  * w5) +
		  (v6  * w6 +
		   v7  * w7))) +

		(((v8  * w8  +
		   v9  * w9 ) +
		  (v10 * w10 +
		   v11 * w11)) +

		 ((v12 * w12 +
		   v13 * w13) +
		  (v14 * w14 +
		   v15 * w15)));

	return sum / filter_sum;
}



void main()
{
#if MAIN_BUFFER_MSAA_SAMPLES > 1
	ivec2 tex_res = textureSize(albedo_texture);
#else
	ivec2 tex_res = textureSize(albedo_texture, /*mip level*/0);
#endif

	ivec2 px_coords = ivec2(int(float(tex_res.x) * pos.x), int(float(tex_res.y) * pos.y));

#if MAIN_BUFFER_MSAA_SAMPLES > 1
	vec4 col = vec4(0.f);
	for(int i=0; i<MAIN_BUFFER_MSAA_SAMPLES; ++i)
		col += texelFetch(albedo_texture, px_coords, i);
	col *= (1.f / MAIN_BUFFER_MSAA_SAMPLES);
#else
	vec4 col = texelFetch(albedo_texture, px_coords, /*mip level=*/0);
#endif

#if 0 // ORDER_INDEPENDENT_TRANSPARENCY
		// Add order-independent transparency terms:
		// Get transparent_accum_texture colour
	#if MAIN_BUFFER_MSAA_SAMPLES > 1
		vec4 accum_col = vec4(0.f);
		for(int i=0; i<MAIN_BUFFER_MSAA_SAMPLES; ++i)
			accum_col += texelFetch(transparent_accum_texture, px_coords, i);
		accum_col *= (1.f / MAIN_BUFFER_MSAA_SAMPLES);
	#else
		vec4 accum_col = texelFetch(transparent_accum_texture, px_coords, /*mip level=*/0);
	#endif

		// Get total transmittance colour
	#if MAIN_BUFFER_MSAA_SAMPLES > 1
		vec4 total_transmittance = 0.f;
		for(int i=0; i<MAIN_BUFFER_MSAA_SAMPLES; ++i)
			total_transmittance += texelFetch(total_transmittance_texture, px_coords, i);
		total_transmittance *= (1.f / MAIN_BUFFER_MSAA_SAMPLES);
	#else
		vec4 total_transmittance = texelFetch(total_transmittance_texture, px_coords, /*mip level=*/0);
	#endif

		vec4 T_tot = clamp(total_transmittance, 0.00001, 0.9999);

		col *= total_transmittance;

		vec4 L = accum_col * ((T_tot - vec4(1.0)) / log(T_tot));
		col += L;
#endif

	if(bloom_strength > 0.0)
	{
		// Approximate aperture diffraction through a circular aperture with a sum of Gaussians.  See https://www.desmos.com/calculator/kahlw2rqvt

		vec4 blur_col_0 = texture(blur_tex_0, pos);
		vec4 blur_col_1 = texture(blur_tex_1, pos);
		vec4 blur_col_2 = texture(blur_tex_2, pos);
		vec4 blur_col_3 = texture(blur_tex_3, pos);
		vec4 blur_col_4 = texture(blur_tex_4, pos);
		vec4 blur_col_5 = texture(blur_tex_5, pos);

		vec4 blur_col =
			blur_col_0 * (1.0/16.0) +
			blur_col_1 * (1.0/16.0) +
			blur_col_2 * (1.0/16.0) +
			blur_col_3 * (1.0/16.0) +
			blur_col_4 * (1.0/16.0) +
			blur_col_5 * (1.0/16.0);
			//sampleTexHighQual(blur_tex_5, pos.x, pos.y)  * (1.0/64.0);

		col.xyz += (blur_col * bloom_strength).xyz; // Leave alpha as-is.
	}
	
	float alpha = col.w;

	vec3 col_rgb = col.xyz * exposure_factor;
	vec3 col_hsv = rgb2hsv(col_rgb);
	col_hsv.y = min(1.0, col_hsv.y * saturation_multiplier);
	col_rgb = hsv2rgb(col_hsv);

	colour_out = vec4(toneMapToNonLinear(col_rgb), alpha);
	//colour_out = vec4(toNonLinear(3.0 * col.xyz), alpha); // linear tonemapping
}
