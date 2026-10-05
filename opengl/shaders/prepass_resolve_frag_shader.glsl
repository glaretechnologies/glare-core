
// Resolves the multisampled colour + depth pre-pass to the single-sample pre-pass textures used by SSAO etc.
// For each pixel, takes the depth, colour and normal of the sample closest to the camera, so that thin geometry, which may only cover a few of the samples,
// isn't lost.  (With a single sample per pixel, geometry thinner than a pixel can fall between pixel centres and vanish, e.g. from reflections.)


uniform sampler2DMS colour_tex;
#if NORMAL_TEXTURE_IS_UINT
uniform usampler2DMS normal_tex;
#else
uniform sampler2DMS normal_tex;
#endif
uniform sampler2DMS depth_tex;

uniform int num_samples;


layout(location = 0) out vec4 colour_out;
#if NORMAL_TEXTURE_IS_UINT
layout(location = 1) out uvec4 normal_out;
#else
layout(location = 1) out vec4 normal_out;
#endif


void main()
{
	ivec2 px = ivec2(gl_FragCoord.xy);

	int closest_i = 0;
	float closest_depth = texelFetch(depth_tex, px, 0).x;
	for(int i=1; i<num_samples; ++i)
	{
		float depth = texelFetch(depth_tex, px, i).x;
#if USE_REVERSE_Z
		if(depth > closest_depth) // With reverse z, larger depth values are closer.
#else
		if(depth < closest_depth)
#endif
		{
			closest_depth = depth;
			closest_i = i;
		}
	}

	colour_out = texelFetch(colour_tex, px, closest_i);
	normal_out = texelFetch(normal_tex, px, closest_i);
	gl_FragDepth = closest_depth;
}
