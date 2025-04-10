// Common code used in various shaders


// From SRGBUtils::fastApproxLinearSRGBToNonLinearSRGB().
vec3 fastApproxLinearSRGBToNonLinearSRGB(vec3 c)
{
	vec3 sqrt_c = sqrt(c);
	vec3 nonlinear = c*(c*0.0404024488f + vec3(-0.19754737849999998f)) + 
		sqrt_c*1.0486722787999998f + sqrt(sqrt_c)*0.1634726509f - vec3(0.055f);

	vec3 linear = c * 12.92f;

	return mix(nonlinear, linear, lessThanEqual(c, vec3(0.0031308f)));
}


vec3 toNonLinear(vec3 v)
{
	return fastApproxLinearSRGBToNonLinearSRGB(v);
}
