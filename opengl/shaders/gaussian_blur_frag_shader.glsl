
uniform sampler2D albedo_texture;
uniform int x_blur;

in vec2 pos; // [0, 1] x [0, 1]

out vec4 colour_out;

void main()
{
	vec4 col = vec4(0, 0, 0, 0);

	// See code in OpenGLEngine::doBloomPostProcess() that generates these weights.
	float offsets[17] = float[]( -8., -7., -6., -5., -4., -3., -2., -1., 0., 1., 2., 3., 4., 5., 6., 7., 8. );
	float weights[17] = float[]( 0.0009542271145619452, 0.00316814542748034, 0.008963371627032757, 0.02160979062318802, 0.044395867735147476, 0.07772262394428253, 0.11594853550195694, 0.14739947021007538, 0.1596759408712387, 0.14739947021007538, 0.11594853550195694, 0.07772262394428253, 0.044395867735147476, 0.02160979062318802, 0.008963371627032757, 0.00316814542748034, 0.0009542271145619452);

	if(x_blur == 1)
	{
		float pixel_size = 1.0 / float(textureSize(albedo_texture, 0).x);
		for(int i=0; i<17; ++i)
			col += texture(albedo_texture, pos + vec2(pixel_size * offsets[i], 0)) * weights[i];
	}
	else
	{
		float pixel_size = 1.0 / float(textureSize(albedo_texture, 0).y);
		for(int i=0; i<17; ++i)
			col += texture(albedo_texture, pos + vec2(0, pixel_size * offsets[i])) * weights[i];
	}

	colour_out = col;
}
