
uniform sampler2D albedo_texture; // main colour buffer

uniform sampler2D depth_tex;

uniform float focus_distance;
uniform float dof_blur_strength;

in vec2 pos; // [0, 1] x [0, 1]

out vec4 colour_out;


float getDepthFromDepthTexture(vec2 normed_pos_ss)
{
	return getDepthFromDepthTextureValue(near_clip_dist, texture(depth_tex, normed_pos_ss).x); // defined in frag_utils.glsl
}


// https://blog.voxagon.se/2018/05/04/bokeh-depth-of-field-in-single-pass.html
const float GOLDEN_ANGLE = 2.39996323; 
const float MAX_BLUR_SIZE = 20.0; 
const float RAD_SCALE = 0.5; // Smaller = nicer blur, larger = faster

float getBlurSize(float depth, float focusPoint, float focusScale)
{
	float coc = clamp((1.0 / focusPoint - 1.0 / depth)*focusScale, -1.0, 1.0);
	return abs(coc) * MAX_BLUR_SIZE;
}

vec3 depthOfField(vec2 texCoord, float focusPoint, float focusScale, vec2 uPixelSize)
{
	float centerDepth = getDepthFromDepthTexture(texCoord); // texture(uDepth, texCoord).r * uFar;
	float centerSize = getBlurSize(centerDepth, focusPoint, focusScale);
	vec3 color = texture(albedo_texture, /*vTexCoord*/texCoord).rgb;
	float tot = 1.0;
	float radius = RAD_SCALE;
	for (float ang = 0.0; radius<MAX_BLUR_SIZE; ang += GOLDEN_ANGLE)
	{
		vec2 tc = texCoord + vec2(cos(ang), sin(ang)) * uPixelSize * radius;
		vec3 sampleColor = texture(albedo_texture, tc).rgb;
		float sampleDepth = getDepthFromDepthTexture(tc); // texture(uDepth, tc).r * uFar;
		float sampleSize = getBlurSize(sampleDepth, focusPoint, focusScale);
		if (sampleDepth > centerDepth)
			sampleSize = clamp(sampleSize, 0.0, centerSize*2.0);
		float m = smoothstep(radius-0.5, radius+0.5, sampleSize);
		color += mix(color/tot, sampleColor, m);
		tot += 1.0;   radius += RAD_SCALE/radius;
	}
	return color /= tot;
}


void main()
{
	ivec2 tex_res = textureSize(albedo_texture, /*mip level*/0);
	ivec2 px_coords = ivec2(int(float(tex_res.x) * pos.x), int(float(tex_res.y) * pos.y));

	vec4 col = texelFetch(albedo_texture, px_coords, /*mip level=*/0);

	vec2 uPixelSize = vec2(1.0 / float(tex_res.x), 1.0 / float(tex_res.y)); //The size of a pixel: vec2(1.0/width, 1.0/height) 

	col.xyz = depthOfField(pos, focus_distance, dof_blur_strength, uPixelSize);

	colour_out = col;
}
