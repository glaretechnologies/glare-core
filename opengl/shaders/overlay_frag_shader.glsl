#version 150

uniform vec4 diffuse_colour;

out vec4 colour_out;

void main()
{
	colour_out = diffuse_colour;
}
