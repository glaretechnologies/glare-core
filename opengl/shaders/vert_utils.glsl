// Common code used in various vertex shaders


// Returns +- 1
vec2 signNotZero(vec2 v) {
	return vec2(((v.x >= 0.0) ? 1.0 : -1.0), ((v.y >= 0.0) ? 1.0 : -1.0));
}

vec3 oct_to_float32x3(vec2 e) {
	vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
	if (v.z < 0.0) v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
	//return normalize(v);
	return v; // Don't normalise here as returned normal will be normalised later.
}


vec3 decodeNormalFromPositionW(float position_in_w)
{
	uint uw = uint(position_in_w); // position_in is not normalised, so values should be in [0, 65535]
	int ux = int(uw >> 8); // Get upper 8 bits
	int uy = int(uw & 0xFF); // Get lower 8 bits
	ux = (ux << 24) >> 24; // Sign-extend
	uy = (uy << 24) >> 24; // Sign-extend
	float u = max(float(ux) * (1.0 / 127.0), -1.0); // Convert to [-1, 1]
	float v = max(float(uy) * (1.0 / 127.0), -1.0);
	return oct_to_float32x3(vec2(u, v));
}
