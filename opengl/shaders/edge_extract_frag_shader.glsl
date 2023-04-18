
uniform sampler2D tex;
uniform vec4 col;
uniform float line_width;

in vec2 texture_coords;

out vec4 colour_out;

void main()
{
	// Adapted from http://www.geoffprewett.com/blog/software/opengl-outline/index.html

	float use_line_width = line_width;

	ivec2 tex_size = textureSize(tex, 0);
	float delta_x = 1.0 / float(tex_size.x);
	float delta_y = 1.0 / float(tex_size.y);
	int W = int(ceil(use_line_width));
	float coverage = 0;
	float min_dist = 1.0e10;
	bool inside = false;
	for (int y = -W;  y <= W;  ++y)
	for (int x = -W;  x <= W;  ++x)
	{
		float val = texture(tex, vec2(texture_coords.x + x*delta_x, texture_coords.y + y*delta_y)).x;
		if(x == 0 && y == 0)
			inside = val > 0.5;
		coverage += val;
		if(val > 0.01) // if this texel is inside the solid object:
		{
			float dist = sqrt(float(x*x + y*y)) + 0.5 - val;
			min_dist = min(min_dist, dist);
		}
	}
	coverage /= float((W*2 + 1)*(W*2 + 1));
	float use_val = 0;
	if(inside)
	{
		use_val = 0.0; // (coverage < 0.5) ? 1.0 : 0.0;
	}
	else
	{
		use_val = 1.0 - smoothstep(use_line_width - 1.0, use_line_width, min_dist);
	}
	
	colour_out = vec4(col.x, col.y, col.z, use_val * col.w);
}
