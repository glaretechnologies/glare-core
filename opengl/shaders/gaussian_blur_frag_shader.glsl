#version 150

uniform sampler2D albedo_texture;
uniform int x_blur;

in vec2 pos; // [0, 1] x [0, 1]

out vec4 colour_out;

void main()
{
	vec4 col = vec4(0, 0, 0, 0);


	float offsets[5] = float[]( -2, -1, 0, 1, 2 );
	float weights[5] = float[]( 0.054488685, 0.24420133, 0.40261996, 0.24420133, 0.054488685 );

	if(x_blur == 1)
	{
		float pixel_size = 1.0 / textureSize(albedo_texture, 0).x;
		for(int i=0; i<5; ++i)
			col += texture(albedo_texture, pos + vec2(pixel_size * offsets[i], 0)) * weights[i];
	}
	else
	{
		float pixel_size = 1.0 / textureSize(albedo_texture, 0).y;
		for(int i=0; i<5; ++i)
			col += texture(albedo_texture, pos + vec2(0, pixel_size * offsets[i])) * weights[i];
	}

	colour_out = col;
}

