
in vec3 position_in; // position_in is [0, 1] x [0, 1]

out vec2 pos; // [0, 1] x [0, 1]

void main()
{
	pos = position_in.xy;

	vec2 usepos = vec2(position_in.x * 2.0 - 1.0, position_in.y * 2.0 - 1.0); // Map to [-1, 1] x [-1, 1]
	gl_Position = vec4(usepos.x, usepos.y, 0.0, 1.0);
}
