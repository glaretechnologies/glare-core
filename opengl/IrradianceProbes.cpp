/*=====================================================================
IrradianceProbes.cpp
--------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "IrradianceProbes.h"


#include "OpenGLEngine.h"
#include "IncludeOpenGL.h"
#include "../maths/mathstypes.h"
#include "../utils/StringUtils.h"
#include "../utils/RuntimeCheck.h"
#include <limits>


IrradianceProbes::IrradianceProbes(int grid_dims_x, int grid_dims_y, int grid_dims_z, float grid_spacing_)
:	probes_per_row(0),
	atlas_w(0),
	atlas_h(0),
	grid_origin(0.f, 0.f, 0.f, 1.f),
	grid_spacing(grid_spacing_),
	num_probes_needing_capture(0),
	grid_origin_valid(false)
{
	runtimeCheck(grid_dims_x >= 1 && grid_dims_y >= 1 && grid_dims_z >= 1);
	runtimeCheck(grid_spacing > 0.f);

	grid_dims[0] = grid_dims_x;
	grid_dims[1] = grid_dims_y;
	grid_dims[2] = grid_dims_z;

	max_num_probes = FIRST_GRID_PROBE_INDEX + numGridProbes();

	probe_needs_capture.assign(numGridProbes(), 1);
	num_probes_needing_capture = numGridProbes();

	// Lay the tiles out in a roughly square atlas.  probes_per_row is used for integer division in the shader,
	// so it stays fixed for the lifetime of the atlas.
	probes_per_row = 1;
	while(probes_per_row * probes_per_row < max_num_probes)
		probes_per_row++;

	const int num_rows = Maths::roundedUpDivide<int>(max_num_probes, probes_per_row);

	atlas_w         = probes_per_row * ATLAS_COLUMN_PITCH;
	depth_region_y  = num_rows * IRRADIANCE_TILE_RES;
	atlas_h         = depth_region_y + num_rows * DEPTH_TILE_RES;
}


IrradianceProbes::~IrradianceProbes()
{}


void IrradianceProbes::allocateGLResources(OpenGLEngine* opengl_engine)
{
	// RGBA16F rather than RGB16F: RGB16F is not a valid render buffer format in WebGL 2.
	irradiance_tex = new OpenGLTexture(atlas_w, atlas_h, opengl_engine, /*data=*/ArrayRef<uint8>(),
		OpenGLTextureFormat::Format_RGBA_Linear_Half, OpenGLTexture::Filtering_Bilinear, OpenGLTexture::Wrapping_Clamp,
		/*has_mipmaps=*/false, /*MSAA_samples=*/1);
	irradiance_tex->setDebugName("probe_irradiance_tex");

	irradiance_framebuffer = new FrameBuffer();
	irradiance_framebuffer->attachTexture(*irradiance_tex, GL_COLOR_ATTACHMENT0);
	irradiance_framebuffer->setSingleDrawBuffer(GL_COLOR_ATTACHMENT0);

	//------------------------------------- Capture target -------------------------------------
	capture_tex = new OpenGLTexture(CAPTURE_FACE_RES * 6, CAPTURE_FACE_RES, opengl_engine, /*data=*/ArrayRef<uint8>(),
		OpenGLTextureFormat::Format_RGBA_Linear_Half, OpenGLTexture::Filtering_Nearest, OpenGLTexture::Wrapping_Clamp,
		/*has_mipmaps=*/false, /*MSAA_samples=*/1);
	capture_tex->setDebugName("probe_capture_tex");

	// One depth texture spanning all 6 faces.  Each face clears and draws within its own viewport, so they don't
	// interfere.  A texture rather than a renderbuffer so the convolution can sample it for the visibility term.
	capture_depth_tex = new OpenGLTexture(CAPTURE_FACE_RES * 6, CAPTURE_FACE_RES, opengl_engine, /*data=*/ArrayRef<uint8>(),
		OpenGLTextureFormat::Format_Depth_Float, OpenGLTexture::Filtering_Nearest, OpenGLTexture::Wrapping_Clamp,
		/*has_mipmaps=*/false, /*MSAA_samples=*/1);
	capture_depth_tex->setDebugName("probe_capture_depth_tex");

	capture_framebuffer = new FrameBuffer();
	capture_framebuffer->attachTextures(*capture_tex,       GL_COLOR_ATTACHMENT0,
	                                    *capture_depth_tex, GL_DEPTH_ATTACHMENT);
	assert(capture_framebuffer->isComplete());
}


Vec4f IrradianceProbes::gridProbePos(int x, int y, int z) const
{
	return grid_origin + Vec4f((float)x, (float)y, (float)z, 0.f) * grid_spacing;
}


Vec3i IrradianceProbes::baseCell() const
{
	return Vec3i(
		Maths::floorToInt(grid_origin[0] / grid_spacing + 0.5f),
		Maths::floorToInt(grid_origin[1] / grid_spacing + 0.5f),
		Maths::floorToInt(grid_origin[2] / grid_spacing + 0.5f));
}


int IrradianceProbes::gridProbeIndex(int x, int y, int z) const
{
	const Vec3i base = baseCell();

	const int sx = Maths::intMod(base.x + x, grid_dims[0]);
	const int sy = Maths::intMod(base.y + y, grid_dims[1]);
	const int sz = Maths::intMod(base.z + z, grid_dims[2]);

	return FIRST_GRID_PROBE_INDEX + (sz * grid_dims[1] + sy) * grid_dims[0] + sx;
}


void IrradianceProbes::markAllProbesForCapture()
{
	probe_needs_capture.assign(numGridProbes(), 1);
	num_probes_needing_capture = numGridProbes();
}


void IrradianceProbes::setGridCentre(const Vec4f& centre)
{
	// Snap to whole probe spacings.  Without this the probes would shift slightly every time the grid moved, so
	// no capture would ever stay valid.
	const Vec4f half_extent = Vec4f((float)(grid_dims[0] - 1), (float)(grid_dims[1] - 1), (float)(grid_dims[2] - 1), 0.f) * (grid_spacing * 0.5f);
	const Vec4f unsnapped = centre - half_extent;

	const Vec4f new_origin(
		Maths::roundDownToMultipleFloating(unsnapped[0], grid_spacing),
		Maths::roundDownToMultipleFloating(unsnapped[1], grid_spacing),
		Maths::roundDownToMultipleFloating(unsnapped[2], grid_spacing),
		1.f);

	if(grid_origin_valid && (new_origin == grid_origin))
		return;

	const Vec3i old_base = baseCell();
	const bool had_valid_window = grid_origin_valid;

	grid_origin = new_origin;
	grid_origin_valid = true;

	const Vec3i new_base = baseCell();

	if(!had_valid_window)
	{
		markAllProbesForCapture();
		return;
	}

	// Slots are toroidal, so a scroll only invalidates the cells that were not in the old window.  Walk the new
	// window and mark any cell whose world coordinates fall outside the old one.
	for(int z=0; z<grid_dims[2]; ++z)
	for(int y=0; y<grid_dims[1]; ++y)
	for(int x=0; x<grid_dims[0]; ++x)
	{
		const Vec3i world_cell(new_base.x + x, new_base.y + y, new_base.z + z);

		const bool was_in_old_window =
			(world_cell.x >= old_base.x) && (world_cell.x < old_base.x + grid_dims[0]) &&
			(world_cell.y >= old_base.y) && (world_cell.y < old_base.y + grid_dims[1]) &&
			(world_cell.z >= old_base.z) && (world_cell.z < old_base.z + grid_dims[2]);

		if(!was_in_old_window)
		{
			const int slot = gridProbeIndex(x, y, z) - FIRST_GRID_PROBE_INDEX;
			if(!probe_needs_capture[slot])
			{
				probe_needs_capture[slot] = 1;
				num_probes_needing_capture++;
			}
		}
	}
}


int IrradianceProbes::nextProbeToCapture(const Vec4f& pos, int& x_out, int& y_out, int& z_out) const
{
	int best_slot = -1;
	float best_dist2 = std::numeric_limits<float>::infinity();

	// Nearest first: probes close to the viewer are the ones whose lighting is most visible.  Linear scan, which
	// is fine at these grid sizes; a heap would be worth it only for much larger volumes.
	for(int z=0; z<grid_dims[2]; ++z)
	for(int y=0; y<grid_dims[1]; ++y)
	for(int x=0; x<grid_dims[0]; ++x)
	{
		const int slot = gridProbeIndex(x, y, z) - FIRST_GRID_PROBE_INDEX;
		if(!probe_needs_capture[slot])
			continue;

		const float dist2 = pos.getDist2(gridProbePos(x, y, z));
		if(dist2 < best_dist2)
		{
			best_dist2 = dist2;
			best_slot = slot;
			x_out = x;
			y_out = y;
			z_out = z;
		}
	}

	return best_slot;
}


void IrradianceProbes::markProbeCaptured(int x, int y, int z)
{
	const int slot = gridProbeIndex(x, y, z) - FIRST_GRID_PROBE_INDEX;
	if(probe_needs_capture[slot])
	{
		probe_needs_capture[slot] = 0;
		num_probes_needing_capture--;
	}
}


void IrradianceProbes::getCaptureFaceBasis(int face, Vec4f& forward_out, Vec4f& right_out, Vec4f& up_out)
{
	// World is z-up.  The four side faces keep world +z as their up vector; the two vertical faces pick an
	// arbitrary but fixed up.
	//
	// The set must be right handed in the sense OpenGL camera space uses - x = right, y = up, z = -forward - so
	// right x up == -forward.  Getting this backwards makes the view matrix a reflection, which reverses triangle
	// winding and leaves backface culling keeping the wrong faces.
	switch(face)
	{
	case 0: forward_out = Vec4f( 1.f, 0.f, 0.f, 0.f); right_out = Vec4f(0.f, -1.f, 0.f, 0.f); up_out = Vec4f( 0.f, 0.f, 1.f, 0.f); break;
	case 1: forward_out = Vec4f(-1.f, 0.f, 0.f, 0.f); right_out = Vec4f(0.f,  1.f, 0.f, 0.f); up_out = Vec4f( 0.f, 0.f, 1.f, 0.f); break;
	case 2: forward_out = Vec4f(0.f,  1.f, 0.f, 0.f); right_out = Vec4f( 1.f, 0.f, 0.f, 0.f); up_out = Vec4f( 0.f, 0.f, 1.f, 0.f); break;
	case 3: forward_out = Vec4f(0.f, -1.f, 0.f, 0.f); right_out = Vec4f(-1.f, 0.f, 0.f, 0.f); up_out = Vec4f( 0.f, 0.f, 1.f, 0.f); break;
	case 4: forward_out = Vec4f(0.f, 0.f,  1.f, 0.f); right_out = Vec4f(0.f, -1.f, 0.f, 0.f); up_out = Vec4f(-1.f, 0.f, 0.f, 0.f); break;
	default:
		assert(face == 5);
		forward_out = Vec4f(0.f, 0.f, -1.f, 0.f); right_out = Vec4f(0.f, -1.f, 0.f, 0.f); up_out = Vec4f(1.f, 0.f, 0.f, 0.f); break;
	}

	assert(epsEqual(crossProduct(right_out, up_out), -forward_out));
}


Matrix4f IrradianceProbes::getCaptureFaceViewMatrix(int face, const Vec4f& probe_pos)
{
	Vec4f forward, right, up;
	getCaptureFaceBasis(face, forward, right, up);

	// Camera space is x = right, y = up, z = -forward, so the rotation rows are those axes.  The translation
	// column is the probe position expressed in camera space.
	return Matrix4f::fromRows(
		Vec4f(right[0],     right[1],     right[2],     -dot(right,   probe_pos)),
		Vec4f(up[0],        up[1],        up[2],        -dot(up,      probe_pos)),
		Vec4f(-forward[0], -forward[1], -forward[2],     dot(forward, probe_pos)),
		Vec4f(0.f, 0.f, 0.f, 1.f));
}


void IrradianceProbes::getCaptureFaceRect(int face, int& x_out, int& y_out, int& w_out, int& h_out)
{
	assert(face >= 0 && face < 6);
	x_out = face * CAPTURE_FACE_RES;
	y_out = 0;
	w_out = CAPTURE_FACE_RES;
	h_out = CAPTURE_FACE_RES;
}


std::string IrradianceProbes::getShaderPreprocessorDefines() const
{
	std::string s;
	s += "#define PROBE_TILE_INTERIOR_RES "    + toString(IRRADIANCE_TILE_INTERIOR_RES) + "\n";
	s += "#define PROBE_TILE_BORDER "          + toString(TILE_BORDER) + "\n";
	s += "#define PROBE_TILE_RES "             + toString(IRRADIANCE_TILE_RES) + "\n";
	s += "#define PROBE_DEPTH_TILE_INTERIOR_RES " + toString(DEPTH_TILE_INTERIOR_RES) + "\n";
	s += "#define PROBE_DEPTH_TILE_RES "       + toString(DEPTH_TILE_RES) + "\n";
	s += "#define PROBE_DEPTH_REGION_Y "       + toString(depth_region_y) + "\n";
	s += "#define PROBE_ATLAS_COLUMN_PITCH "   + toString(ATLAS_COLUMN_PITCH) + "\n";
	s += "#define PROBE_ATLAS_PROBES_PER_ROW " + toString(probes_per_row) + "\n";
	s += "#define PROBE_ATLAS_W "              + toString(atlas_w) + "\n";
	s += "#define PROBE_ATLAS_H "              + toString(atlas_h) + "\n";
	s += "#define GLOBAL_SKY_PROBE_INDEX "     + toString(GLOBAL_SKY_PROBE_INDEX) + "\n";
	s += "#define PROBE_CAPTURE_FACE_RES "     + toString(CAPTURE_FACE_RES) + "\n";
	return s;
}


void IrradianceProbes::getIrradianceTileRect(int probe_index, int& x_out, int& y_out, int& w_out, int& h_out) const
{
	runtimeCheck(probe_index >= 0 && probe_index < max_num_probes);

	x_out = (probe_index % probes_per_row) * ATLAS_COLUMN_PITCH;
	y_out = (probe_index / probes_per_row) * IRRADIANCE_TILE_RES;
	w_out = IRRADIANCE_TILE_RES;
	h_out = IRRADIANCE_TILE_RES;
}


void IrradianceProbes::getDepthTileRect(int probe_index, int& x_out, int& y_out, int& w_out, int& h_out) const
{
	runtimeCheck(probe_index >= 0 && probe_index < max_num_probes);

	x_out = (probe_index % probes_per_row) * ATLAS_COLUMN_PITCH;
	y_out = depth_region_y + (probe_index / probes_per_row) * DEPTH_TILE_RES;
	w_out = DEPTH_TILE_RES;
	h_out = DEPTH_TILE_RES;
}
