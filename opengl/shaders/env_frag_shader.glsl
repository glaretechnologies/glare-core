#version 150

in vec3 pos_cs;
in vec2 texture_coords;
in vec3 dir_ws;

uniform vec4 sundir_cs;
uniform vec4 diffuse_colour;
uniform int have_texture;
uniform sampler2D diffuse_tex;
//uniform sampler2D noise_tex;
uniform sampler2D fbm_tex;
uniform sampler2D cirrus_tex;
uniform mat3 texture_matrix;
uniform vec3 campos_ws;
uniform float time;

out vec4 colour_out;


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
	float theta = 1.618034 * 3.141592653589 * 2;
	return vec2(cos(theta) * p.x - sin(theta) * p.y, sin(theta) * p.x + cos(theta) * p.y);
}

float fbm(vec2 p)
{
	return (texture(fbm_tex, p).x - 0.5) * 2.f;
}

//float noise(vec2 p)
//{
//	return (texture(noise_tex, p).x - 0.5) * 2.f;
//}

float fbmMix(vec2 p)
{
	return 
		fbm(p) +
		fbm(rot(p * 2)) * 0.5 +
		0;
}

float length2(vec2 v)
{
	return dot(v, v);
}


void main()
{
	vec4 col;
	if(have_texture != 0)
		col = texture(diffuse_tex, (texture_matrix * vec3(texture_coords.x, texture_coords.y, 1.0)).xy) * 1.0e9f;
	else
		col = diffuse_colour;

	// Render sun
	vec4 suncol = vec4(9124154304.569067, 8038831044.193394, 7154376815.37873, 1);
	float d = dot(sundir_cs.xyz, normalize(pos_cs));
	//col = mix(col, suncol, smoothstep(0.9999, 0.9999892083461507, d));
	col = mix(col, suncol, smoothstep(0.99997, 0.9999892083461507, d));

	
	// Get position ray hits cloud plane
	float cirrus_cloudfrac = 0;
	float cumulus_cloudfrac = 0;
	float ray_t = rayPlaneIntersect(campos_ws, dir_ws, 6000);
	//vec4 cumulus_col = vec4(0,0,0,0);
	//float cumulus_alpha = 0;
	float cumulus_edge = 0;
	if(ray_t > 0)
	{
		vec3 hitpos = campos_ws + dir_ws * ray_t;
		vec2 p = hitpos.xy * 0.0001;
		p.x += time * 0.002;
	
		vec2 coarse_noise_coords = vec2(p.x * 0.16, p.y * 0.20);
		float course_detail = fbmMix(vec2(coarse_noise_coords));

		cirrus_cloudfrac = max(course_detail * 0.9, 0.f) * texture(cirrus_tex, p).x * 1.5;
	}
		
	{
		float cumulus_ray_t = rayPlaneIntersect(campos_ws, dir_ws, 1000);
		if(cumulus_ray_t > 0)
		{
			vec3 hitpos = campos_ws + dir_ws * cumulus_ray_t;
			vec2 p = hitpos.xy * 0.0001;
			p.x += time * 0.002;

			vec2 cumulus_coords = vec2(p.x * 2 + 2.3453, p.y * 2 + 1.4354);
			
			float cumulus_val = max(0.f, fbmMix(cumulus_coords) - 0.3f);
			//cumulus_alpha = max(0.f, cumulus_val - 0.7f);

			cumulus_edge = smoothstep(0.0001, 0.1, cumulus_val) - smoothstep(0.2, 0.6, cumulus_val) * 0.5;

			float dist_factor = 1.f - smoothstep(80000, 160000, ray_t);

			//cumulus_col = vec4(cumulus_val, cumulus_val, cumulus_val, 1);
			cumulus_cloudfrac = dist_factor * cumulus_val;
		}
	}

	float cloudfrac = max(cirrus_cloudfrac, cumulus_cloudfrac);
	float w = 2.0e8;
	vec4 cloudcol = vec4(w, w, w, 1);
	col = mix(col, cloudcol, max(0.f, cloudfrac));
	float sunw = w * 2.5;
	vec4 suncloudcol = vec4(sunw, sunw, sunw, 1);
	float blend = max(0.f, cumulus_edge) * max(0, pow(d, 32));// smoothstep(0.9, 0.9999892083461507, d);
	col = mix(col, suncloudcol, blend);

	//col = mix(col, cumulus_col, cumulus_alpha);


#if DEPTH_FOG
	// Blend lower hemisphere into a colour that matches fogged ground quad in Substrata
	// Chosen by hand to match the fogged phong colour at ~2km (edge of ground quad)
	vec4 lower_hemis_col = vec4(pow(8.63, 2.2), pow(8.94, 2.2), pow(9.37, 2.2), 1.0) * 1.6e6;

	// Cloud shadows on lower half of hemisphere, to match shadows on ground plane
	if(texture_coords.y > 1.58)
	{
		float ground_ray_t = campos_ws.z / -dir_ws.z;
		vec3 ground_pos = campos_ws + dir_ws * ground_ray_t;

		vec3 sundir_ws = vec3(3.6716393E-01, 6.3513672E-01, 6.7955279E-01); // TEMP HACK
		vec3 cum_layer_pos = ground_pos + sundir_ws * (1000.f) * (1.f / sundir_ws.z);

		vec2 cum_tex_coords = vec2(cum_layer_pos.x, cum_layer_pos.y) * 1.0e-4f;
		cum_tex_coords.x += time * 0.002;

		vec2 cumulus_coords = vec2(cum_tex_coords.x * 2 + 2.3453, cum_tex_coords.y * 2 + 1.4354);
		float cumulus_val = max(0.f, fbmMix(cumulus_coords) - 0.3f);

		float dist_factor = 1.f - smoothstep(10000, 20000, ground_ray_t);
		cumulus_val *= dist_factor;

		float cumulus_trans = max(0.f, 1.f - cumulus_val * 1.4);
		float sun_vis_factor = cumulus_trans;

		vec4 shadowed_col = vec4(pow(4.5, 2.2), pow(5.4, 2.2), pow(6.4, 2.2), 1.0) * 2.0e6;
		lower_hemis_col = mix(shadowed_col, lower_hemis_col, sun_vis_factor);
	}

	float lower_hemis_factor = smoothstep(1.52, 1.6, texture_coords.y);
	col = mix(col, lower_hemis_col, lower_hemis_factor);
#endif

	col *= 0.000000003; // tone-map
	colour_out = vec4(toNonLinear(col.xyz), 1);
}
