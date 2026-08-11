
// Debug visualisation of an irradiance probe: a small sphere at the probe position, shaded by looking the probe
// up in the direction of the sphere normal.

in vec3 position_in;

out vec3 normal_ws;

uniform mat4 proj_matrix;
uniform mat4 view_matrix;
uniform vec4 probe_sphere_pos_radius; // xyz = probe world position, w = sphere radius.


void main()
{
	// MeshPrimitiveBuilding::makeSphereMesh() gives a unit sphere centred on the origin, so the vertex position
	// is also the normal.
	normal_ws = position_in;

	vec3 pos_ws = probe_sphere_pos_radius.xyz + position_in * probe_sphere_pos_radius.w;

	gl_Position = proj_matrix * (view_matrix * vec4(pos_ws, 1.0));
}
