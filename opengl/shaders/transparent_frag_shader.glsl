#version 150

in vec3 normal;
in vec3 pos_cs;

uniform vec4 colour;
uniform int have_shading_normals;

out vec4 colour_out;



float square(float x) { return x*x; }
float pow5(float x) { return x*x*x*x*x; }

float fresnellApprox(float cos_theta, float ior)
{
	float r_0 = square((1.0 - ior) / (1.0 + ior));
	return r_0 + (1.0 - r_0)*pow5(1.0 - cos_theta);
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

	vec4 transmission_col = vec4(colour.x, colour.y, colour.z, 0.1);
	vec4 refl_col = vec4(1.0, 1.0, 1.0, 0.5);
	colour_out = transmission_col * (1.0 - r) + refl_col * r;
}
