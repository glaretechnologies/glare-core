/*=====================================================================
CloudNoise.h
------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "../utils/Reference.h"
#include <string>
namespace glare { class TaskManager; }
class OpenGLEngine;
class OpenGLTexture;


/*=====================================================================
CloudNoise
----------
Builds the tiling 3D noise volumes that cloud_frag_shader.glsl raymarches.

Shape volume (SHAPE_RES^3, RGBA8), tiled over CloudGPUSettings::shape_period metres:
	r: Perlin-Worley.  Perlin FBM with its low end pulled up towards inverted Worley noise, which turns the
	   smooth Perlin blobs into the billowy cauliflower shape cumulus have.
	g, b, a: inverted-Worley FBM at three increasing base frequencies, used to erode r.

Detail volume (DETAIL_RES^3, RGB8), tiled over CloudGPUSettings::detail_period metres:
	r, g, b: inverted-Worley FBM at three increasing base frequencies, used to erode the cloud edges into wisps.

Both volumes tile seamlessly: every noise lattice is periodic with a whole number of cells across the texture,
so a texel on one face has the same neighbourhood as the texel it wraps onto.

Generation is parallelised across z slices and takes a few hundred ms, so it is done once at engine startup
(only when OpenGLEngineSettings::volumetric_clouds_support is set).
=====================================================================*/
namespace CloudNoise
{
	const int SHAPE_RES  = 128;
	const int DETAIL_RES = 64;

	Reference<OpenGLTexture> buildShapeTexture(OpenGLEngine* opengl_engine, glare::TaskManager& task_manager);

	Reference<OpenGLTexture> buildDetailTexture(OpenGLEngine* opengl_engine, glare::TaskManager& task_manager);

	// Writes a montage of z slices of each volume channel to out_dir as PNGs, and prints the range each channel
	// covers along with the range of the 'base' value cloud_frag_shader.glsl derives from them.  For working out
	// whether a cloud that looks wrong is the noise's fault or the shaping maths'.
	void writeDebugSlices(const std::string& out_dir, glare::TaskManager& task_manager);

	void test();
}
