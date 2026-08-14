/*=====================================================================
IrradianceProbes.h
------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "FrameBuffer.h"
#include "OpenGLTexture.h"
#include "RenderBuffer.h"
#include "../maths/Matrix4f.h"
#include "../maths/Vec4f.h"
#include "../maths/vec3.h"
#include <vector>
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include <string>
class OpenGLEngine;


/*=====================================================================
IrradianceProbes
----------------
Storage for a set of irradiance probes.  Each probe holds the cosine-weighted irradiance arriving from
every direction, stored as an octahedral map in a tile of a shared 2D atlas texture.

Tile layout: an interior of IRRADIANCE_TILE_INTERIOR_RES^2 texels holding the octahedral map, surrounded by a
border ring holding wrapped-around copies of interior texels.  Without the border, a tap taken near a tile edge
would blend in texels belonging to a neighbouring probe, so the ring has to be as wide as the filter's reach:
IRRADIANCE_TILE_BORDER = 2 for the B-spline used on irradiance, DEPTH_TILE_BORDER = 1 for the bilinear tap used
on depth.  The border is written by the same pass that writes the interior (see
probe_bake_from_cubemap_frag_shader.glsl), rather than by a separate copy pass, by folding out-of-range
octahedral coordinates back onto the octahedron.

Probe 0 is reserved as the global sky probe: it holds what cosine_env_tex used to hold, and is used for
shading points that fall outside the probe volume.  It is baked by resampling the cosine env cube map, so
that it agrees with the sun-elevation blend already done by OpenGLEngine::loadMapsForSunDir().
=====================================================================*/
class IrradianceProbes : public RefCounted
{
public:
	// Constructor just computes the atlas layout; it does not touch OpenGL.  The layout has to be known before
	// OpenGLEngine assembles its shader preprocessor defines, which happens before GL resources are created.
	// The atlas is sized to hold the global sky probe plus grid_dims_x * grid_dims_y * grid_dims_z grid probes.
	IrradianceProbes(int grid_dims_x, int grid_dims_y, int grid_dims_z, float grid_spacing);
	~IrradianceProbes();

	void allocateGLResources(OpenGLEngine* opengl_engine);

	// Emitted into OpenGLEngine::preprocessor_defines, so that frag_utils.glsl can do tile addressing.
	std::string getShaderPreprocessorDefines() const;

	// Atlas texel rects of a probe's tiles, for setting the viewport when writing into them.
	void getIrradianceTileRect(int probe_index, int& x_out, int& y_out, int& w_out, int& h_out) const;
	void getDepthTileRect(int probe_index, int& x_out, int& y_out, int& w_out, int& h_out) const;


	//------------------------------------- Capture target -------------------------------------
	// The 6 cube faces are rendered side by side into one 2D texture rather than into a cube map.  Rendering to
	// a region of a 2D texture with a viewport avoids cube-face framebuffer attachment (and the ES 3.2+ calls
	// layered rendering would need), and the convolution has to compute a direction and solid angle per texel
	// explicitly anyway, so a cube map's addressing and filtering conventions would go unused.
	//
	// Depth is captured by attaching a depth texture rather than a renderbuffer, so the convolution can read it
	// back.  That avoids needing the material shaders to write distance into a colour channel, which would mean
	// a dedicated capture program permutation.
	static const int CAPTURE_FACE_RES = 32;

	// Orthonormal basis for cube face 'face' (0..5).  A texel at face coordinates (u, v), both in [-1, 1],
	// looks along normalize(forward + u*right + v*up).  The convolution must use this same mapping.
	static void getCaptureFaceBasis(int face, Vec4f& forward_out, Vec4f& right_out, Vec4f& up_out);

	// World-to-camera matrix for capturing 'face' from 'probe_pos'.
	static Matrix4f getCaptureFaceViewMatrix(int face, const Vec4f& probe_pos);

	// Texel rect of a face within the capture texture.
	static void getCaptureFaceRect(int face, int& x_out, int& y_out, int& w_out, int& h_out);


	// The irradiance tiles are sampled with a cubic B-spline (see sampleProbeIrradiance() in frag_utils.glsl),
	// which reaches two texels either side of the sample point, so they carry a 2-texel border.  The depth tiles
	// are sampled bilinearly and only need one; widening them too would add ~23% to the convolve's output texels
	// for no benefit.  The wider border is harmless when the B-spline is toggled off - bilinear just never
	// reaches the outer ring.
	static const int IRRADIANCE_TILE_BORDER = 2;
	static const int DEPTH_TILE_BORDER = 1;

	// Odd on purpose.  The octahedral parameterisation has a ridge along p = 0 (the x=0 and y=0 octahedron edges),
	// and with cell-centred sampling an even resolution puts p = 0 on a texel *boundary*, so the ridge is never
	// stored and reconstruction can only chord across it.  An odd resolution puts a texel centre exactly on it -
	// here texel 6, since oct_uv = 0.5 gives tile texel 2 + 4.5 = 6.5.  The trade is that nothing then lands on
	// the diamond |p.x| + |p.y| = 1, so the z=0 edge loses the sample it used to have; that one was less visible.
	// Cosine convolution removes everything higher-frequency than this anyway.
	static const int IRRADIANCE_TILE_INTERIOR_RES = 9;
	static const int IRRADIANCE_TILE_RES = IRRADIANCE_TILE_INTERIOR_RES + IRRADIANCE_TILE_BORDER * 2;

	// Visibility needs sharper angular detail than irradiance, so the depth tiles are larger.  They hold mean
	// distance and mean squared distance, for the Chebyshev test.
	static const int DEPTH_TILE_INTERIOR_RES = 16;
	static const int DEPTH_TILE_RES = DEPTH_TILE_INTERIOR_RES + DEPTH_TILE_BORDER * 2;

	// Both tile types live in one texture so they cost one texture unit rather than two.  The irradiance tiles
	// occupy a band across the top, the depth tiles a band below it.  Columns are pitched at DEPTH_TILE_RES in
	// both bands, so irradiance tiles leave some texels unused - simpler addressing, and the waste is a few
	// hundred KB.
	static const int ATLAS_COLUMN_PITCH = DEPTH_TILE_RES;

	static const int GLOBAL_SKY_PROBE_INDEX = 0;
	static const int FIRST_GRID_PROBE_INDEX = 1; // Grid probes follow the global sky probe in the atlas.

	// Position of the grid probe at integer grid coordinates (x, y, z).
	Vec4f gridProbePos(int x, int y, int z) const;

	int numGridProbes() const { return grid_dims[0] * grid_dims[1] * grid_dims[2]; }

	// Centre the grid on 'centre', snapped to whole probe spacings so that probes keep their world positions as
	// the grid moves and captures stay reusable.  Probe slots whose world cell was not inside the previous
	// window are marked as needing capture; the rest keep the data they already have.
	void setGridCentre(const Vec4f& centre);

	// Atlas slot for a probe at window coordinates (x, y, z).
	//
	// Slots are assigned toroidally - by world cell modulo the grid dimensions - so that scrolling the window by
	// one cell only invalidates the newly exposed slab rather than shifting every probe to a different slot.
	// probeIndexForGridCoords() in frag_utils.glsl has to agree with this.
	int gridProbeIndex(int x, int y, int z) const;

	Vec3i baseCell() const; // World cell coordinates of window cell (0, 0, 0).

	void markAllProbesForCapture();

	bool anyProbesNeedCapture() const { return num_probes_needing_capture > 0; }

	// The stalest probe nearest to 'pos' that still needs capturing, or -1 if there are none.
	// Returns the window coordinates in x_out/y_out/z_out.
	int nextProbeToCapture(const Vec4f& pos, int& x_out, int& y_out, int& z_out) const;

	void markProbeCaptured(int x, int y, int z);

private:
	GLARE_DISABLE_COPY(IrradianceProbes);
public:
	int max_num_probes;
	int probes_per_row;
	int atlas_w, atlas_h;
	int depth_region_y; // Atlas row at which the depth tile band starts.

	// The probe grid.  grid_origin is the world position of grid probe (0, 0, 0).
	Vec4f grid_origin;
	float grid_spacing;
	int grid_dims[3];

	// One entry per grid probe, indexed by window coordinates.  Set when the probe's contents no longer
	// correspond to its world position, cleared once it has been recaptured.
	std::vector<uint8> probe_needs_capture;
	int num_probes_needing_capture;
	bool grid_origin_valid; // False until setGridCentre() has been called for the first time.

	Reference<OpenGLTexture> irradiance_tex;
	Reference<FrameBuffer> irradiance_framebuffer;

	Reference<OpenGLTexture> capture_tex;       // (CAPTURE_FACE_RES * 6) x CAPTURE_FACE_RES, RGBA16F radiance.
	Reference<OpenGLTexture> capture_depth_tex; // Same dimensions, depth.  Read back by the convolution.
	Reference<FrameBuffer> capture_framebuffer;
};


typedef Reference<IrradianceProbes> IrradianceProbesRef;
