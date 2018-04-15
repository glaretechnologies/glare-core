#version 150

in vec3 normal;
in vec3 pos_cs;

uniform vec4 sundir_cs;
uniform vec4 colour;
uniform int have_shading_normals;

out vec4 colour_out;



float square(float x) { return x*x; }
float pow5(float x) { return x*x*x*x*x; }
float pow6(float x) { return x*x*x*x*x*x; }

float fresnellApprox(float cos_theta, float ior)
{
	float r_0 = square((1.0 - ior) / (1.0 + ior));
	return r_0 + (1.0 - r_0)*pow5(1.0 - cos_theta);
}

float trowbridgeReitzPDF(float cos_theta, float alpha2)
{
	return cos_theta * alpha2 / (3.1415926535897932384626433832795 * square(square(cos_theta) * (alpha2 - 1.0) + 1.0));
}

float alpha2ForRoughness(float r)
{
	return pow6(r);
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

	vec3 frag_to_cam = normalize(pos_cs * -1.0);

	float cos_theta = abs(dot(frag_to_cam, unit_normal));

	float r = fresnellApprox(cos_theta, 2.5);

	vec3 h = normalize(frag_to_cam + sundir_cs.xyz);
	float h_cos_theta = max(0.0, dot(h, unit_normal));
	float roughness = 0.2;
	float fresnel_scale = 1.0;
	float specular = trowbridgeReitzPDF(h_cos_theta, max(1.0e-8f, alpha2ForRoughness(roughness))) * 
		fresnel_scale;

	vec4 transmission_col = vec4(colour.x, colour.y, colour.z, 0.1);
	vec4 refl_col = vec4(1.0, 1.0, 1.0, 0.5);

	colour_out = transmission_col * (1.0 - r) + refl_col * r + vec4(1.0,1.0,1.0,1.0) * specular;
}
