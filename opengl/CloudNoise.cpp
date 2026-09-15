/*=====================================================================
CloudNoise.cpp
--------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "CloudNoise.h"


#include "OpenGLTexture.h"
#include "../graphics/ImageMap.h"
#include "../graphics/PNGDecoder.h"
#include "../maths/mathstypes.h"
#include "../utils/Task.h"
#include "../utils/TaskManager.h"
#include "../utils/Vector.h"
#include "../utils/Timer.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include <cmath>
#include <limits>
#include <assert.h>
#if BUILD_TESTS
#include "../utils/TestUtils.h"
#endif


namespace CloudNoise
{


// Finalisation step of MurmurHash3.  Any hash with decent avalanche behaviour will do here; the lattices just
// need values that look independent from cell to cell.
static inline uint32 mixBits(uint32 x)
{
	x ^= x >> 16;
	x *= 0x85ebca6bu;
	x ^= x >> 13;
	x *= 0xc2b2ae35u;
	x ^= x >> 16;
	return x;
}


// Non-negative remainder, so that lattice cells at negative coordinates wrap onto the same cells as their
// positive counterparts.
static inline int wrapIndex(int x, int period)
{
	const int r = x % period;
	return (r < 0) ? (r + period) : r;
}


// Hash of a lattice cell, wrapped to 'period' cells in each axis.  Wrapping here is what makes every noise
// function below periodic, and hence what makes the volumes tile.
static inline uint32 cellHash(int x, int y, int z, int period, uint32 seed)
{
	const uint32 wx = (uint32)wrapIndex(x, period);
	const uint32 wy = (uint32)wrapIndex(y, period);
	const uint32 wz = (uint32)wrapIndex(z, period);

	return mixBits(wx * 73856093u ^ wy * 19349663u ^ wz * 83492791u ^ seed * 2654435761u);
}


//========================================= Periodic Perlin noise =========================================


// The 12 edge-midpoint gradients from Perlin's improved noise.  A dot product with one of these is just an
// add and a subtract, and they are spread evenly enough over the sphere to avoid axis-aligned artifacts.
static const float grad_table[12][3] =
{
	{ 1, 1, 0}, {-1, 1, 0}, { 1,-1, 0}, {-1,-1, 0},
	{ 1, 0, 1}, {-1, 0, 1}, { 1, 0,-1}, {-1, 0,-1},
	{ 0, 1, 1}, { 0,-1, 1}, { 0, 1,-1}, { 0,-1,-1}
};


static inline float fade(float t)
{
	return t * t * t * (t * (t * 6.f - 15.f) + 10.f);
}


// Gradient noise with a lattice that repeats every 'period' cells.  x, y, z are in cell units.
static float periodicPerlin(float x, float y, float z, int period, uint32 seed)
{
	const int X = (int)std::floor(x);
	const int Y = (int)std::floor(y);
	const int Z = (int)std::floor(z);

	const float fx = x - (float)X;
	const float fy = y - (float)Y;
	const float fz = z - (float)Z;

	const float u = fade(fx);
	const float v = fade(fy);
	const float w = fade(fz);

	float sum = 0;
	for(int dz=0; dz<2; ++dz)
	for(int dy=0; dy<2; ++dy)
	for(int dx=0; dx<2; ++dx)
	{
		const float* const g = grad_table[cellHash(X + dx, Y + dy, Z + dz, period, seed) % 12];

		// Vector from this corner of the cell to the sample point:
		const float ox = fx - (float)dx;
		const float oy = fy - (float)dy;
		const float oz = fz - (float)dz;

		const float corner_val = g[0]*ox + g[1]*oy + g[2]*oz;

		const float weight = (dx != 0 ? u : (1.f - u)) * (dy != 0 ? v : (1.f - v)) * (dz != 0 ? w : (1.f - w));

		sum += corner_val * weight;
	}

	return sum;
}


// Perlin FBM mapped to [0, 1].  Each octave doubles the frequency, and so doubles the lattice period with it.
static float periodicPerlinFBM(float x, float y, float z, int base_freq, int num_octaves, uint32 seed)
{
	float sum = 0;
	float weight_sum = 0;
	float amplitude = 1.f;
	int freq = base_freq;

	for(int i=0; i<num_octaves; ++i)
	{
		sum += amplitude * periodicPerlin(x * (float)freq, y * (float)freq, z * (float)freq, freq, seed + (uint32)i * 977u);
		weight_sum += amplitude;

		amplitude *= 0.5f;
		freq *= 2;
	}

	// A single octave of gradient noise reaches about +/-0.7 rather than +/-1, so scale up before centring.
	return myClamp(0.5f + 0.5f * (sum / weight_sum) * 1.4f, 0.f, 1.f);
}


//========================================= Periodic Worley noise =========================================


// Inverted Worley noise: 1 at a feature point, falling to 0 a cell away.  Inverting is what gives the
// billowy, cauliflower-like lobes; plain Worley would give the complementary web of creases.
// x, y, z are in cell units.
static float periodicInvWorley(float x, float y, float z, int period, uint32 seed)
{
	const int X = (int)std::floor(x);
	const int Y = (int)std::floor(y);
	const int Z = (int)std::floor(z);

	float closest_d2 = 3.f; // Larger than the largest distance we care about (1 cell), squared.

	for(int dz=-1; dz<=1; ++dz)
	for(int dy=-1; dy<=1; ++dy)
	for(int dx=-1; dx<=1; ++dx)
	{
		const int cx = X + dx;
		const int cy = Y + dy;
		const int cz = Z + dz;

		// One feature point per cell, placed at a hash-determined position inside it.
		const uint32 h = cellHash(cx, cy, cz, period, seed);
		const float px = (float)cx + (float)( h        & 1023u) * (1.f / 1024.f);
		const float py = (float)cy + (float)((h >> 10) & 1023u) * (1.f / 1024.f);
		const float pz = (float)cz + (float)((h >> 20) & 1023u) * (1.f / 1024.f);

		const float ox = px - x;
		const float oy = py - y;
		const float oz = pz - z;

		closest_d2 = myMin(closest_d2, ox*ox + oy*oy + oz*oz);
	}

	return 1.f - myMin(std::sqrt(closest_d2), 1.f);
}


// A few octaves of inverted Worley noise.  Result is in [0, 1].
// The weights fall off fast: summing octaves of equal weight averages the lobes away into featureless mush,
// and lobes are the whole reason for using Worley here.
static float periodicInvWorleyFBM(float x, float y, float z, int base_freq, int num_octaves, uint32 seed)
{
	const float octave_weights[3] = { 0.75f, 0.1875f, 0.0625f };
	assert(num_octaves <= 3);

	float sum = 0;
	float weight_sum = 0;
	int freq = base_freq;

	for(int i=0; i<num_octaves; ++i)
	{
		sum += octave_weights[i] * periodicInvWorley(x * (float)freq, y * (float)freq, z * (float)freq, freq, seed + (uint32)i * 6151u);
		weight_sum += octave_weights[i];

		freq *= 2;
	}

	return sum / weight_sum;
}


static inline float remap(float x, float old_lo, float old_hi, float new_lo, float new_hi)
{
	return new_lo + (x - old_lo) / (old_hi - old_lo) * (new_hi - new_lo);
}


static inline uint8 toUint8(float x)
{
	return (uint8)(myClamp(x, 0.f, 1.f) * 255.f + 0.5f);
}


//========================================= Volume building =========================================


// Rescales each channel so that it spans [0, 1].
//
// The shaping maths in cloud_frag_shader.glsl is a chain of remaps that each assume their input covers the
// full range; feed them a channel that only reaches [0.3, 0.98] and the output is a near-constant field with
// no power to carve a cloud.  How much of [0, 1] a given combination of noise functions happens to cover is
// not something worth predicting analytically, so measure it and correct for it.
static void normaliseChannels(float* data, size_t num_texels, int num_channels)
{
	for(int c=0; c<num_channels; ++c)
	{
		float min_v = std::numeric_limits<float>::infinity();
		float max_v = -std::numeric_limits<float>::infinity();
		for(size_t i=0; i<num_texels; ++i)
		{
			min_v = myMin(min_v, data[i * num_channels + c]);
			max_v = myMax(max_v, data[i * num_channels + c]);
		}

		if(max_v > min_v)
		{
			const float scale = 1.f / (max_v - min_v);
			for(size_t i=0; i<num_texels; ++i)
				data[i * num_channels + c] = (data[i * num_channels + c] - min_v) * scale;
		}
	}
}


struct BuildShapeVolumeTask : public glare::Task
{
	virtual void run(size_t /*thread_index*/)
	{
		const int res = CloudNoise::SHAPE_RES;
		const float inv_res = 1.f / (float)res;

		for(int z=begin_z; z<end_z; ++z)
		for(int y=0; y<res; ++y)
		for(int x=0; x<res; ++x)
		{
			// Texel i maps to i/res, so that texel 0 sits exactly on the lattice origin and texel res wraps
			// back onto it.
			const float px = (float)x * inv_res;
			const float py = (float)y * inv_res;
			const float pz = (float)z * inv_res;

			// The Worley lobes are what give cumulus their cauliflower look, so they carry the structure here,
			// and the Perlin only modulates how pronounced they are from place to place.  The other way round -
			// blending the two, or pulling Perlin up towards Worley as remap(perlin, 0, 1, worley, 1) does -
			// saturates: every point ends up at least as bright as one of the two inputs, the contrast collapses,
			// and the shader is left with a near-constant field that can't carve a silhouette.
			const float perlin = periodicPerlinFBM   (px, py, pz, /*base freq=*/3, /*num octaves=*/4, /*seed=*/1);
			const float billow = periodicInvWorleyFBM(px, py, pz, /*base freq=*/4, /*num octaves=*/3, /*seed=*/2);

			float* const texel = &data[((size_t)z * res * res + (size_t)y * res + x) * 4];
			texel[0] = billow * Maths::lerp(0.35f, 1.f, perlin);
			texel[1] = periodicInvWorleyFBM(px, py, pz, /*base freq=*/ 6, /*num octaves=*/3, /*seed=*/3);
			texel[2] = periodicInvWorleyFBM(px, py, pz, /*base freq=*/12, /*num octaves=*/2, /*seed=*/4);
			texel[3] = periodicInvWorleyFBM(px, py, pz, /*base freq=*/24, /*num octaves=*/2, /*seed=*/5);
		}
	}

	float* data;
	int begin_z, end_z;
};


struct BuildDetailVolumeTask : public glare::Task
{
	virtual void run(size_t /*thread_index*/)
	{
		const int res = CloudNoise::DETAIL_RES;
		const float inv_res = 1.f / (float)res;

		for(int z=begin_z; z<end_z; ++z)
		for(int y=0; y<res; ++y)
		for(int x=0; x<res; ++x)
		{
			const float px = (float)x * inv_res;
			const float py = (float)y * inv_res;
			const float pz = (float)z * inv_res;

			float* const texel = &data[((size_t)z * res * res + (size_t)y * res + x) * 3];
			texel[0] = periodicInvWorleyFBM(px, py, pz, /*base freq=*/ 4, /*num octaves=*/3, /*seed=*/11);
			texel[1] = periodicInvWorleyFBM(px, py, pz, /*base freq=*/ 8, /*num octaves=*/2, /*seed=*/12);
			texel[2] = periodicInvWorleyFBM(px, py, pz, /*base freq=*/16, /*num octaves=*/2, /*seed=*/13);
		}
	}

	float* data;
	int begin_z, end_z;
};


// Generates a volume: runs the slice tasks in parallel into a float buffer, normalises each channel, and
// quantises to the uint8 data the GPU texture holds.
template <class TaskType>
static void buildVolume(glare::TaskManager& task_manager, int res, int num_channels, js::Vector<uint8, 16>& data_out)
{
	const size_t num_texels = (size_t)res * res * res;

	js::Vector<float, 16> float_data(num_texels * num_channels);

	const size_t num_tasks = myMax<size_t>(1, task_manager.getNumThreads());
	const size_t num_slices_per_task = Maths::roundedUpDivide((size_t)res, num_tasks);

	glare::TaskGroupRef group = new glare::TaskGroup();
	group->tasks.resize(num_tasks);

	for(size_t t=0; t<num_tasks; ++t)
	{
		TaskType* task = new TaskType();
		task->data = float_data.data();
		task->begin_z = (int)myMin( t      * num_slices_per_task, (size_t)res);
		task->end_z   = (int)myMin((t + 1) * num_slices_per_task, (size_t)res);
		group->tasks[t] = task;
	}

	task_manager.runTaskGroup(group);

	normaliseChannels(float_data.data(), num_texels, num_channels);

	data_out.resize(num_texels * num_channels);
	for(size_t i=0; i<num_texels * (size_t)num_channels; ++i)
		data_out[i] = toUint8(float_data[i]);
}


Reference<OpenGLTexture> buildShapeTexture(OpenGLEngine* opengl_engine, glare::TaskManager& task_manager)
{
	Timer timer;

	js::Vector<uint8, 16> data;
	buildVolume<BuildShapeVolumeTask>(task_manager, SHAPE_RES, /*num channels=*/4, data);

	Reference<OpenGLTexture> tex = new OpenGLTexture();
	tex->create3DTexture(SHAPE_RES, SHAPE_RES, SHAPE_RES, opengl_engine, ArrayRef<uint8>(data.data(), data.size()),
		OpenGLTextureFormat::Format_RGBA_Linear_Uint8, OpenGLTexture::Filtering_Bilinear, OpenGLTexture::Wrapping_Repeat);
	tex->setDebugName("cloud shape noise");

	conPrint("Cloud shape noise volume (" + toString(SHAPE_RES) + "^3) creation took " + timer.elapsedString());
	return tex;
}


Reference<OpenGLTexture> buildDetailTexture(OpenGLEngine* opengl_engine, glare::TaskManager& task_manager)
{
	Timer timer;

	js::Vector<uint8, 16> data;
	buildVolume<BuildDetailVolumeTask>(task_manager, DETAIL_RES, /*num channels=*/3, data);

	Reference<OpenGLTexture> tex = new OpenGLTexture();
	tex->create3DTexture(DETAIL_RES, DETAIL_RES, DETAIL_RES, opengl_engine, ArrayRef<uint8>(data.data(), data.size()),
		OpenGLTextureFormat::Format_RGB_Linear_Uint8, OpenGLTexture::Filtering_Bilinear, OpenGLTexture::Wrapping_Repeat);
	tex->setDebugName("cloud detail noise");

	conPrint("Cloud detail noise volume (" + toString(DETAIL_RES) + "^3) creation took " + timer.elapsedString());
	return tex;
}


// Lays 16 evenly spaced z slices of one channel out in a 4x4 grid and writes them as a greyscale PNG.
static void writeSliceMontage(const uint8* data, int res, int num_channels, int channel, const std::string& path)
{
	const int grid = 4;
	const int montage_w = res * grid;

	ImageMapUInt8 montage((size_t)montage_w, (size_t)montage_w, 1);

	for(int tile=0; tile<grid*grid; ++tile)
	{
		const int z = (tile * res) / (grid * grid);
		const int tile_x = (tile % grid) * res;
		const int tile_y = (tile / grid) * res;

		for(int y=0; y<res; ++y)
		for(int x=0; x<res; ++x)
			montage.getPixel((size_t)(tile_x + x), (size_t)(tile_y + y))[0] = data[(((size_t)z * res * res) + (size_t)y * res + x) * num_channels + channel];
	}

	PNGDecoder::write(montage, path);
	conPrint("Wrote " + path);
}


static void printChannelRange(const uint8* data, size_t num_texels, int num_channels, int channel, const std::string& name)
{
	int min_v = 255, max_v = 0;
	double sum = 0;
	for(size_t i=0; i<num_texels; ++i)
	{
		const int v = data[i * num_channels + channel];
		min_v = myMin(min_v, v);
		max_v = myMax(max_v, v);
		sum += v;
	}

	conPrint(name + ": min " + doubleToStringNDecimalPlaces(min_v / 255.0, 3) + ", mean " + doubleToStringNDecimalPlaces(sum / (num_texels * 255.0), 3) +
		", max " + doubleToStringNDecimalPlaces(max_v / 255.0, 3));
}


void writeDebugSlices(const std::string& out_dir, glare::TaskManager& task_manager)
{
	{
		const int res = SHAPE_RES;
		js::Vector<uint8, 16> data;
		buildVolume<BuildShapeVolumeTask>(task_manager, res, /*num channels=*/4, data);

		const char* channel_names[4] = { "shape.r (billow x perlin)", "shape.g (Worley base 6)", "shape.b (Worley base 12)", "shape.a (Worley base 24)" };
		for(int c=0; c<4; ++c)
		{
			printChannelRange(data.data(), (size_t)res*res*res, 4, c, channel_names[c]);
			writeSliceMontage(data.data(), res, 4, c, out_dir + "/cloud_shape_" + std::string(1, "rgba"[c]) + ".png");
		}

		// The value cloud_frag_shader.glsl actually shapes clouds from, before the height gradient and coverage
		// are applied.  If this doesn't cover a good part of [0, 1], the noise can't carve a silhouette.
		float min_base = 1.f, max_base = 0.f;
		double base_sum = 0;
		for(size_t i=0; i<(size_t)res*res*res; ++i)
		{
			const float r = data[i*4 + 0] * (1.f / 255.f);
			const float worley_fbm = data[i*4 + 1] * (0.625f / 255.f) + data[i*4 + 2] * (0.25f / 255.f) + data[i*4 + 3] * (0.125f / 255.f);
			const float base = myClamp(remap(r, worley_fbm - 1.f, 1.f, 0.f, 1.f), 0.f, 1.f);

			min_base = myMin(min_base, base);
			max_base = myMax(max_base, base);
			base_sum += base;
		}
		conPrint("derived 'base': min " + doubleToStringNDecimalPlaces(min_base, 3) + ", mean " + doubleToStringNDecimalPlaces(base_sum / ((size_t)res*res*res), 3) +
			", max " + doubleToStringNDecimalPlaces(max_base, 3));
	}

	{
		const int res = DETAIL_RES;
		js::Vector<uint8, 16> data;
		buildVolume<BuildDetailVolumeTask>(task_manager, res, /*num channels=*/3, data);

		for(int c=0; c<3; ++c)
		{
			printChannelRange(data.data(), (size_t)res*res*res, 3, c, "detail." + std::string(1, "rgb"[c]));
			writeSliceMontage(data.data(), res, 3, c, out_dir + "/cloud_detail_" + std::string(1, "rgb"[c]) + ".png");
		}
	}
}


#if BUILD_TESTS


void test()
{
	conPrint("CloudNoise::test()");

	// The volumes are sampled with GL_REPEAT, so every noise function has to agree with itself one period on.
	for(int i=0; i<100; ++i)
	{
		const float x = (float)i * 0.0137f;
		const float y = (float)i * 0.0219f - 0.4f;
		const float z = (float)i * 0.0331f + 1.7f;

		testEpsEqual(periodicPerlin(x, y, z, /*period=*/4, /*seed=*/1), periodicPerlin(x + 4.f, y, z - 8.f, /*period=*/4, /*seed=*/1));

		testEpsEqual(periodicInvWorley(x, y, z, /*period=*/8, /*seed=*/2), periodicInvWorley(x - 8.f, y + 16.f, z, /*period=*/8, /*seed=*/2));

		// The FBMs take coordinates in [0, 1) across the volume, so their period is 1.
		testEpsEqual(periodicPerlinFBM(x, y, z, /*base freq=*/4, /*num octaves=*/4, /*seed=*/1),
			periodicPerlinFBM(x + 1.f, y, z - 2.f, /*base freq=*/4, /*num octaves=*/4, /*seed=*/1));

		testEpsEqual(periodicInvWorleyFBM(x, y, z, /*base freq=*/4, /*num octaves=*/3, /*seed=*/2),
			periodicInvWorleyFBM(x, y - 1.f, z + 3.f, /*base freq=*/4, /*num octaves=*/3, /*seed=*/2));
	}

	// Check the noise actually covers a decent part of [0, 1], so that the shader's remapping has something to
	// work with.  (A bug that collapsed the range would otherwise just show up as a sky with no clouds in it.)
	{
		float min_perlin = 1.f, max_perlin = 0.f, min_worley = 1.f, max_worley = 0.f;
		for(int z=0; z<16; ++z)
		for(int y=0; y<16; ++y)
		for(int x=0; x<16; ++x)
		{
			const float px = (float)x * (1.f / 16.f), py = (float)y * (1.f / 16.f), pz = (float)z * (1.f / 16.f);

			const float perlin = periodicPerlinFBM(px, py, pz, 4, 4, 1);
			min_perlin = myMin(min_perlin, perlin);
			max_perlin = myMax(max_perlin, perlin);

			const float worley = periodicInvWorleyFBM(px, py, pz, 4, 3, 2);
			min_worley = myMin(min_worley, worley);
			max_worley = myMax(max_worley, worley);
		}

		testAssert(max_perlin - min_perlin > 0.3f);
		testAssert(max_worley - min_worley > 0.3f);
	}

	conPrint("CloudNoise::test() done.");
}


#else // else if !BUILD_TESTS:


void test()
{}


#endif // BUILD_TESTS


} // end namespace CloudNoise
