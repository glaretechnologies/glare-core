/*=====================================================================
SSRBlurPyramid.cpp
------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "SSRBlurPyramid.h"


#include "IncludeOpenGL.h"
#include "OpenGLEngine.h"
#include "OpenGLProgram.h"
#include "OpenGLShader.h"
#include "OpenGLTexture.h"
#include "OpenGLMeshRenderData.h"
#include "FrameBuffer.h"
#include "../maths/mathstypes.h"
#include "../utils/ArrayRef.h"
#include "../utils/StringUtils.h"


// Binds texture to texture unit, and texture unit to sampler.  As in OpenGLEngine.cpp.
static void bindTextureUnitToSampler(const OpenGLTexture& texture, int texture_unit_index, GLint sampler_uniform_location)
{
	glActiveTexture(GL_TEXTURE0 + texture_unit_index);
	glBindTexture(/*target=*/texture.getTextureTarget(), /*texture=*/texture.texture_handle);
	glActiveTexture(GL_TEXTURE0); // Change the active texture unit index back to zero, so we don't accidentally change the binding when creating new textures etc.

	glUniform1i(sampler_uniform_location, texture_unit_index);
}


static void drawQuad(const OpenGLMeshRenderData& unit_quad)
{
#if defined(EMSCRIPTEN)
	// glDrawElementsBaseVertex is not in WebGL, but base_vertex is always 0 there.  See drawElementsBaseVertex() in OpenGLEngine.cpp.
	assert(unit_quad.vbo_handle.base_vertex == 0);
	glDrawElements(GL_TRIANGLES, (GLsizei)unit_quad.batches[0].num_indices, unit_quad.getIndexType(), (void*)unit_quad.getBatch0IndicesTotalBufferOffset());
#else
	glDrawElementsBaseVertex(GL_TRIANGLES, (GLsizei)unit_quad.batches[0].num_indices, unit_quad.getIndexType(), (void*)unit_quad.getBatch0IndicesTotalBufferOffset(),
		unit_quad.vbo_handle.base_vertex);
#endif
}


// Binds the uniform block, if the program uses it.
static void bindUniformBlockIfUsed(OpenGLProgram& prog, const char* name, int binding_point_index)
{
	const GLuint block_index = glGetUniformBlockIndex(prog.program, name);
	if(block_index != GL_INVALID_INDEX)
		glUniformBlockBinding(prog.program, block_index, /*binding point=*/binding_point_index);
}


SSRBlurPyramid::SSRBlurPyramid()
{}


SSRBlurPyramid::~SSRBlurPyramid()
{}


void SSRBlurPyramid::build(OpenGLEngine& engine, int material_common_uniforms_binding_point)
{
	const std::string key_defs = preprocessorDefsForKey(ProgramKey(ProgramKey::ProgramName_blur_ssao, ProgramKeyArgs()));
	const std::string vert_defs = key_defs + engine.getPreprocessorDefinesWithCommonVertStructs();
	const std::string common_frag_defs = key_defs + engine.getPreprocessorDefinesWithCommonFragStructs() + "#define NUM_SSR_BLUR_LEVELS " + toString(NUM_LEVELS) + "\n" +
		"#define NUM_SSR_DIR_BINS " + toString(NUM_DIR_BINS) + "\n";
	const std::string vert_shader_path = engine.getShadersDir() + "/blur_ssao_vert_shader.glsl"; // Plain unit-quad-to-clip-space vertex shader.
	const std::string frag_shader_path = engine.getShadersDir() + "/ssr_blur_pyramid_frag_shader.glsl";

	// A down pass program for each level written, as the number of streams written depends on the level.
	for(int i=0; i<NUM_LEVELS; ++i)
	{
		OpenGLProgramRef prog = new OpenGLProgram(
			"ssr_blur_down_" + toString(i + 1),
			new OpenGLShader(vert_shader_path, engine.getVersionDirective(), vert_defs, GL_VERTEX_SHADER),
			new OpenGLShader(frag_shader_path, engine.getVersionDirective(), common_frag_defs + "#define SSR_BLUR_DOWN_PASS 1\n#define SSR_BLUR_FINAL_UP_PASS 0\n#define SSR_BLUR_OUT_LEVEL " + toString(i + 1) + "\n", GL_FRAGMENT_SHADER),
			engine.getAndIncrNextProgramIndex(),
			/*wait for build to complete=*/true
		);
		OpenGLEngine::getUniformLocations(prog);
		engine.addProgram(prog);
		bindUniformBlockIfUsed(*prog, "MaterialCommonUniforms", material_common_uniforms_binding_point);

		DownProg& down = down_progs[i];
		down.prog = prog;
		for(int z=0; z<5; ++z)
			down.in_stream_tex_location[z] = prog->getUniformLocation("in_stream_tex_" + toString(z));
		down.area_tex_location = prog->getUniformLocation("area_tex");
		down.dir_bin_location  = prog->getUniformLocation("dir_bin");
	}

	{
		OpenGLProgramRef prog = new OpenGLProgram(
			"ssr_blur_up",
			new OpenGLShader(vert_shader_path, engine.getVersionDirective(), vert_defs, GL_VERTEX_SHADER),
			new OpenGLShader(frag_shader_path, engine.getVersionDirective(), common_frag_defs + "#define SSR_BLUR_DOWN_PASS 0\n#define SSR_BLUR_FINAL_UP_PASS 0\n", GL_FRAGMENT_SHADER),
			engine.getAndIncrNextProgramIndex(),
			/*wait for build to complete=*/true
		);
		OpenGLEngine::getUniformLocations(prog);
		engine.addProgram(prog);
		bindUniformBlockIfUsed(*prog, "MaterialCommonUniforms", material_common_uniforms_binding_point);

		up_prog = prog;
		up_own_stream_tex_location   = prog->getUniformLocation("own_stream_tex");
		up_area_tex_location         = prog->getUniformLocation("area_tex");
		up_coarse_tex_location       = prog->getUniformLocation("coarse_tex");
		up_coarse_area_tex_location  = prog->getUniformLocation("coarse_area_tex");
	}

	{
		OpenGLProgramRef prog = new OpenGLProgram(
			"ssr_blur_final_up",
			new OpenGLShader(vert_shader_path, engine.getVersionDirective(), vert_defs, GL_VERTEX_SHADER),
			new OpenGLShader(frag_shader_path, engine.getVersionDirective(), common_frag_defs + "#define SSR_BLUR_DOWN_PASS 0\n#define SSR_BLUR_FINAL_UP_PASS 1\n", GL_FRAGMENT_SHADER),
			engine.getAndIncrNextProgramIndex(),
			/*wait for build to complete=*/true
		);
		OpenGLEngine::getUniformLocations(prog);
		engine.addProgram(prog);
		bindUniformBlockIfUsed(*prog, "MaterialCommonUniforms", material_common_uniforms_binding_point);

		final_up_prog = prog;
		final_up_own_stream_tex_location = prog->getUniformLocation("own_stream_tex");
		final_up_area_tex_location       = prog->getUniformLocation("area_tex");
		for(int b=0; b<NUM_DIR_BINS; ++b)
		{
			final_up_coarse_tex_location[b]      = prog->getUniformLocation("coarse_tex_" + toString(b));
			final_up_coarse_area_tex_location[b] = prog->getUniformLocation("coarse_area_tex_" + toString(b));
		}
	}
}


void SSRBlurPyramid::blur(SSRBlurPyramidTextures& textures, const OpenGLTexture& trace_tex, const OpenGLTexture& depth_normal_tex, FrameBuffer& out_framebuffer,
	const OpenGLMeshRenderData& unit_quad, const int texture_units[6])
{
	const int N = NUM_LEVELS;
	static_assert(NUM_LEVELS >= 2, "The final up pass reads the output of level 1's up pass");

	// Down passes: write level i + 1 from level i, for each bin.  Level 0's area comes from depth_normal_tex.
	for(int i=0; i<N; ++i)
	{
		glViewport(0, 0, (int)textures.area_tex[0][i]->xRes(), (int)textures.area_tex[0][i]->yRes());

		const DownProg& down = down_progs[i];
		down.prog->useProgram();

		for(int b=0; b<NUM_DIR_BINS; ++b)
		{
			textures.down_framebuffer[b][i]->bindForDrawing();

			if(i == 0)
			{
				// Level 1 splits the trace output into the bins.
				bindTextureUnitToSampler(trace_tex,        texture_units[0], down.in_stream_tex_location[0]);
				bindTextureUnitToSampler(depth_normal_tex, texture_units[5], down.area_tex_location);
				glUniform1i(down.dir_bin_location, b);
			}
			else
			{
				// Level i's streams for levels i + 1 onwards.  Its stream for level i is its own, and stays there.
				const int num_in_streams = N - i;
				for(int z=0; z<num_in_streams; ++z)
					bindTextureUnitToSampler(*textures.stream_tex[b][i - 1][z + 1], texture_units[z], down.in_stream_tex_location[z]);
				bindTextureUnitToSampler(*textures.area_tex[b][i - 1], texture_units[5], down.area_tex_location);
			}

			drawQuad(unit_quad);
		}
	}

	// Up passes for levels N - 1 to 1, for each bin: write the level from its own stream and the upsampled level + 1.  The coarsest level has no up pass,
	// so its stream is upsampled directly.
	up_prog->useProgram();
	for(int level=N-1; level>=1; --level)
	{
		glViewport(0, 0, (int)textures.up_tex[0][level - 1]->xRes(), (int)textures.up_tex[0][level - 1]->yRes());

		for(int b=0; b<NUM_DIR_BINS; ++b)
		{
			textures.up_framebuffer[b][level - 1]->bindForDrawing();

			const OpenGLTexture& coarse_tex = (level + 1 == N) ? *textures.stream_tex[b][level][0] : *textures.up_tex[b][level];
			bindTextureUnitToSampler(*textures.stream_tex[b][level - 1][0], texture_units[0], up_own_stream_tex_location);
			bindTextureUnitToSampler(*textures.area_tex[b][level - 1],      texture_units[1], up_area_tex_location);
			bindTextureUnitToSampler(coarse_tex,                            texture_units[2], up_coarse_tex_location);
			bindTextureUnitToSampler(*textures.area_tex[b][level],          texture_units[3], up_coarse_area_tex_location);

			drawQuad(unit_quad);
		}
	}

	// Final up pass: write level 0 from the trace output and the bins' upsampled level 1.
	final_up_prog->useProgram();
	out_framebuffer.bindForDrawing();
	glViewport(0, 0, (int)out_framebuffer.xRes(), (int)out_framebuffer.yRes());

	bindTextureUnitToSampler(trace_tex,        texture_units[0], final_up_own_stream_tex_location);
	bindTextureUnitToSampler(depth_normal_tex, texture_units[1], final_up_area_tex_location);
	for(int b=0; b<NUM_DIR_BINS; ++b)
	{
		bindTextureUnitToSampler(*textures.up_tex[b][0],   texture_units[2 + b],                final_up_coarse_tex_location[b]);
		bindTextureUnitToSampler(*textures.area_tex[b][0], texture_units[2 + NUM_DIR_BINS + b], final_up_coarse_area_tex_location[b]);
	}

	drawQuad(unit_quad);
}


SSRBlurPyramidTextures::SSRBlurPyramidTextures(OpenGLEngine* engine, int prepass_xres, int prepass_yres)
{
	const int N = SSRBlurPyramid::NUM_LEVELS;

	// Bilinear filtering, as the passes do their filtering partly with bilinear fetches.  See ssr_blur_pyramid_frag_shader.glsl.
	int level_xres = prepass_xres;
	int level_yres = prepass_yres;
	for(int i=0; i<N; ++i)
	{
		const int level = i + 1;
		level_xres = Maths::roundedUpDivide(level_xres, 2); // Round up, so that every texel of the finer level has a parent.
		level_yres = Maths::roundedUpDivide(level_yres, 2);

		for(int b=0; b<SSRBlurPyramid::NUM_DIR_BINS; ++b)
		{
			const std::string name_suffix = " " + toString(level) + " bin " + toString(b);

			area_tex[b][i] = new OpenGLTexture(level_xres, level_yres, engine, /*data=*/ArrayRef<uint8>(),
				OpenGLTextureFormat::Format_Greyscale_Half, OpenGLTexture::Filtering_Bilinear, OpenGLTexture::Wrapping_Clamp, /*has_mipmaps=*/false, /*MSAA_samples=*/1);
			area_tex[b][i]->setDebugName("ssr_blur_area_tex" + name_suffix);

			// The down pass writes the area to attachment 0, and the streams to the following attachments.
			down_framebuffer[b][i] = new FrameBuffer();
			down_framebuffer[b][i]->attachTexture(*area_tex[b][i], GL_COLOR_ATTACHMENT0);
			const int num_streams = N - level + 1;
			for(int z=0; z<num_streams; ++z)
			{
				stream_tex[b][i][z] = new OpenGLTexture(level_xres, level_yres, engine, /*data=*/ArrayRef<uint8>(),
					OpenGLTextureFormat::Format_RGBA_Linear_Half, OpenGLTexture::Filtering_Bilinear, OpenGLTexture::Wrapping_Clamp, /*has_mipmaps=*/false, /*MSAA_samples=*/1);
				stream_tex[b][i][z]->setDebugName("ssr_blur_stream_tex" + name_suffix + " for level " + toString(level + z));
				down_framebuffer[b][i]->attachTexture(*stream_tex[b][i][z], GL_COLOR_ATTACHMENT0 + 1 + z);
			}

			down_framebuffer[b][i]->bindForDrawing();
			down_framebuffer[b][i]->setDrawBuffers(1 + num_streams);
			down_framebuffer[b][i]->unbindFromDrawing();

			if(level < N) // The coarsest level has no up pass.
			{
				up_tex[b][i] = new OpenGLTexture(level_xres, level_yres, engine, /*data=*/ArrayRef<uint8>(),
					OpenGLTextureFormat::Format_RGBA_Linear_Half, OpenGLTexture::Filtering_Bilinear, OpenGLTexture::Wrapping_Clamp, /*has_mipmaps=*/false, /*MSAA_samples=*/1);
				up_tex[b][i]->setDebugName("ssr_blur_up_tex" + name_suffix);

				up_framebuffer[b][i] = new FrameBuffer();
				up_framebuffer[b][i]->attachTexture(*up_tex[b][i], GL_COLOR_ATTACHMENT0);
			}
		}
	}
}


SSRBlurPyramidTextures::~SSRBlurPyramidTextures()
{}
