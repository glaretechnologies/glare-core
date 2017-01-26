#version 150

in vec3 normal;
in vec3 pos_cs;
in vec2 texture_coords;
in vec3 shadow_tex_coords;

uniform vec4 sundir;
uniform vec4 diffuse_colour;
uniform int have_shading_normals;
uniform int have_texture;
uniform sampler2D diffuse_tex;
uniform int have_depth_texture;
uniform sampler2D depth_tex;
uniform mat4 texture_matrix;
uniform float roughness;
uniform float fresnel_scale;

out vec4 colour_out;


float square(float x) { return x*x; }
float pow5(float x) { return x*x*x*x*x; }
float pow6(float x) { return x*x*x*x*x*x; }

float alpha2ForRoughness(float r)
{
	return pow6(r);
}

float trowbridgeReitzPDF(float cos_theta, float alpha2)
{
	return cos_theta * alpha2 / (3.1415926535897932384626433832795 * square(square(cos_theta) * (alpha2 - 1.0) + 1.0));
}

// https://en.wikipedia.org/wiki/Schlick%27s_approximation
float fresnellApprox(float cos_theta, float ior)
{
	float r_0 = square((1.0 - ior) / (1.0 + ior));
	return r_0 + (1.0 - r_0)*pow5(1.0 - cos_theta);
}


vec2 samples[16] = vec2[](
	vec2(327, 128),
	vec2(789, 168),
	vec2(507, 219),
	vec2(200, 439),
	vec2(409, 392),
	vec2(599, 401),
	vec2(490, 470),
	vec2(387, 574),
	vec2(546, 535),
	vec2(686, 530),
	vec2(814, 545),
	vec2(496, 648),
	vec2(42, 724),
	vec2(383, 802),
	vec2(599, 716),
	vec2(865, 367)
);


uint ha(uint x)
{
	x  = (x ^ 12345391u) * 2654435769u;
	x ^= (x << 6) ^ (x >> 26);
	x *= 2654435769u;
	x += (x << 5) ^ (x >> 12);

	return x;
}


void main()
{
	vec3 use_normal;
	if(have_shading_normals != 0)
	{
		use_normal = normal;
	}
	else
	{
		vec3 dp_dx = dFdx(pos_cs);    
		vec3 dp_dy = dFdy(pos_cs);  
		vec3 N_g = normalize(cross(dp_dx, dp_dy)); 
		use_normal = N_g;
	}
    vec3 unit_normal = normalize(use_normal);

	float light_cos_theta = max(dot(unit_normal, sundir.xyz), 0.0);

	vec3 frag_to_cam = normalize(pos_cs * -1.0);

	vec3 h = normalize(frag_to_cam + sundir.xyz);

	float h_cos_theta = max(0.0, dot(h, unit_normal));
	float specular = trowbridgeReitzPDF(h_cos_theta, max(1.0e-8f, alpha2ForRoughness(roughness))) * 
		fresnellApprox(h_cos_theta, 1.5) * fresnel_scale;
 
	vec4 col;
	if(have_texture != 0)
		col = texture(diffuse_tex, (texture_matrix * vec4(texture_coords.x, texture_coords.y, 0.0, 1.0)).xy);
	else
		col = diffuse_colour;

	// Shadow mapping
	float sun_vis_factor;
#if SHADOW_MAPPING
	vec2 depth_texcoords = shadow_tex_coords.xy * 0.5 + vec2(0.5, 0.5);
	float actual_depth = shadow_tex_coords.z * 0.5 + 0.5;

	if(depth_texcoords.x < 0.0 || depth_texcoords.x >= 1.0 || depth_texcoords.y <= 0.0 || depth_texcoords.y >= 1.0)
		sun_vis_factor = 1.0;
	else
	{
		float bias = 0.0001;// / abs(light_cos_theta);
		
		sun_vis_factor = 0;
		int pixel_index = int((gl_FragCoord.y * 1920.0 + gl_FragCoord.x));

		float theta = float(ha(uint(pixel_index)) * (6.283185307179586 / 4294967296.0));
		mat2 R = mat2(cos(theta), sin(theta), -sin(theta), cos(theta));

		for(int i=0; i<16; ++i)
		{
			vec2 st = depth_texcoords + R * ((samples[i] * 0.001) - vec2(0.5, 0.5)) * (4.0 / 2048.0);
			float light_depth = texture(depth_tex, st).x;

			sun_vis_factor += (light_depth + bias) > actual_depth ? (1.0 / 16.0) : 0.0; 
		}
	}
#else
	sun_vis_factor = 1.0;
#endif

	colour_out = 0.5 * col + // ambient
		sun_vis_factor * (
			col * light_cos_theta * 0.5 + 
			specular);
}
