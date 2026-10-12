/*=====================================================================
SSRBlurPyramid.h
----------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "../utils/Reference.h"


class OpenGLEngine;
class OpenGLProgram;
class OpenGLTexture;
class OpenGLMeshRenderData;
class FrameBuffer;
class SSRBlurPyramidTextures;


/*=====================================================================
SSRBlurPyramid
--------------
Blurs the screen-space reflection trace output, spreading each hit into the
image of its reflected GGX lobe, through a pyramid of half-resolution levels.
See ssr_blur_pyramid_frag_shader.glsl.

Owned by OpenGLEngine.  Holds the programs, which are shared by all scenes.
The textures the passes write are per scene, in SSRBlurPyramidTextures.
=====================================================================*/
class SSRBlurPyramid
{
public:
	SSRBlurPyramid();
	~SSRBlurPyramid();

	// Number of coarse levels.  At most 6, as the down pass shader has outputs for at most 6 streams.  Level 1's down pass then writes 7 render
	// targets, with the area.
	static const int NUM_LEVELS = 6;

	// Number of direction bins the coarse levels are split into: leftwards and rightwards reflections.  The final up pass shader reads 2.
	static const int NUM_DIR_BINS = 2;

	// Builds (or rebuilds, e.g. after a shader change) the programs.
	// material_common_uniforms_binding_point: the binding point of the MaterialCommonUniforms uniform buffer, for the camera's field of view.
	void build(OpenGLEngine& engine, int material_common_uniforms_binding_point);

	// Blurs trace_tex into out_framebuffer.
	// trace_tex: the trace output, rgb = hit radiance, w = lobe width for a hit, or < 0 for a miss.
	// depth_normal_tex: the prepass resolution depth and normal, w = linear depth, or < 0 where nothing was drawn.
	// unit_quad: must already be bound.
	// texture_units: 6 texture units the passes can bind their inputs to.
	void blur(SSRBlurPyramidTextures& textures, const OpenGLTexture& trace_tex, const OpenGLTexture& depth_normal_tex, FrameBuffer& out_framebuffer,
		const OpenGLMeshRenderData& unit_quad, const int texture_units[6]);

private:
	struct DownProg
	{
		Reference<OpenGLProgram> prog;
		int in_stream_tex_location[5];
		int area_tex_location;
		int dir_bin_location;
	};
	DownProg down_progs[NUM_LEVELS]; // Index i writes level i + 1.

	// Up pass for the levels >= 1, for one bin.
	Reference<OpenGLProgram> up_prog;
	int up_own_stream_tex_location;
	int up_area_tex_location;
	int up_coarse_tex_location;
	int up_coarse_area_tex_location;

	// Up pass for level 0, combining the bins.
	Reference<OpenGLProgram> final_up_prog;
	int final_up_own_stream_tex_location;
	int final_up_area_tex_location;
	int final_up_coarse_tex_location[NUM_DIR_BINS];
	int final_up_coarse_area_tex_location[NUM_DIR_BINS];
};


/*=====================================================================
SSRBlurPyramidTextures
----------------------
The textures and framebuffers of the SSR blur pyramid for one scene, sized
from the prepass resolution.
=====================================================================*/
class SSRBlurPyramidTextures
{
public:
	SSRBlurPyramidTextures(OpenGLEngine* engine, int prepass_xres, int prepass_yres);
	~SSRBlurPyramidTextures();

	// The first index is the direction bin.  Index i holds level i + 1, at 1 / 2^(i + 1) of the prepass resolution, rounded up.
	// Level k (index i) has a stream for each level from k to NUM_LEVELS: stream_tex[b][i][j] is bin b's stream for level k + j.
	Reference<OpenGLTexture> stream_tex[SSRBlurPyramid::NUM_DIR_BINS][SSRBlurPyramid::NUM_LEVELS][SSRBlurPyramid::NUM_LEVELS];
	Reference<OpenGLTexture> area_tex[SSRBlurPyramid::NUM_DIR_BINS][SSRBlurPyramid::NUM_LEVELS];
	Reference<FrameBuffer> down_framebuffer[SSRBlurPyramid::NUM_DIR_BINS][SSRBlurPyramid::NUM_LEVELS]; // Writes area_tex and stream_tex.
	Reference<OpenGLTexture> up_tex[SSRBlurPyramid::NUM_DIR_BINS][SSRBlurPyramid::NUM_LEVELS]; // The coarsest level has no up pass, so the last elements are null.
	Reference<FrameBuffer> up_framebuffer[SSRBlurPyramid::NUM_DIR_BINS][SSRBlurPyramid::NUM_LEVELS];
};
