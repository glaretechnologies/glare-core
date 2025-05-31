

uniform sampler2D albedo_texture; // source texture

uniform sampler2D main_depth_texture;

in vec2 pos; // [0, 1] x [0, 1]

out vec4 colour_out;


// See 'Calculations for recovering depth values from depth buffer', OpenGLEngine.cpp.
float getDepthFromDepthTexture(ivec2 px_coords)
{
	const float near_clip_dist = 0.1; // TEMP
#if USE_REVERSE_Z
	return near_clip_dist / texelFetch(main_depth_texture, px_coords, /*mip level=*/0).x;
#else
	return -near_clip_dist / (texelFetch(main_depth_texture, px_coords, /*mip level=*/0).x - 1.0);
#endif
}


void main()
{
	const ivec2 tex_res = textureSize(albedo_texture, /*mip level*/0);
	
	const ivec2 px_coords = ivec2(int(float(tex_res.x) * pos.x), int(float(tex_res.y) * pos.y));

	const float centre_depth = getDepthFromDepthTexture(px_coords);

#if 0
	vec4 val = texelFetch(albedo_texture, px_coords, /*mip level=*/0);
#else
	vec4 val = texelFetch(albedo_texture, px_coords, /*mip level=*/0);
	float sum_weight = 1.0;



	const int r = 4;
	for(int y = px_coords.y-r; y <= px_coords.y + r; ++y)
	for(int x = px_coords.x-r; x <= px_coords.x + r; ++x)
	{
		if(x >= 0 && x < tex_res.x && y >= 0 && y < tex_res.y &&
			(x != px_coords.x || y != px_coords.y)) // we have already sampled the centre pixel.
		{
			const float depth = getDepthFromDepthTexture(ivec2(x, y));
			if(abs(depth - centre_depth) < 0.05) // TODO: make use normal or something
			{
				val += texelFetch(albedo_texture, ivec2(x, y), /*mip level=*/0);
				sum_weight += 1.0;
			}
		}
	}

	val /= sum_weight;
#endif
	
	//// -1
	//if((px_coords.x - 1) >= 0)
	//{
	//	val += texelFetch(albedo_texture, ivec2(px_coords.x - 1, px_coords.y), /*mip level=*/0);
	//	sum_weight += 1.0;
	//}
	//if((px_coords.x - 1) >= 0)
	//{
	//	val += texelFetch(albedo_texture, ivec2(px_coords.x - 1, px_coords.y), /*mip level=*/0);
	//	sum_weight += 1.0;
	//}
	
	colour_out = val;
}
