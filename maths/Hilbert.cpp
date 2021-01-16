/*=====================================================================
Hilbert.cpp
-----------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "Hilbert.h"


#include "../maths/Matrix2.h"
#include "../utils/StringUtils.h"
#include "../indigo/globals.h"
#include <vector>


// Mirror along x=y
static const Matrix2<int> mirror(const Matrix2<int>& m)
{
	assert(Matrix2<int>(0, 1, 1, 0) * m == Matrix2<int>(m.e[2], m.e[3], m.e[0], m.e[1]));
	return Matrix2<int>(m.e[2], m.e[3], m.e[0], m.e[1]);
}


static const Matrix2<int> negMirror(const Matrix2<int>& m)
{
	assert(Matrix2<int>(0, -1, -1, 0) * m == Matrix2<int>(-m.e[2], -m.e[3], -m.e[0], -m.e[1]));
	return Matrix2<int>(-m.e[2], -m.e[3], -m.e[0], -m.e[1]);
}


static int run(int depth, int maxdepth, Matrix2<int> m, int x, int y, int index, Vec2i* indices_out)
{
	const int step = 1 << (maxdepth - depth);

	const int i_x = m.e[0];
	const int i_y = m.e[2];
	const int j_x = m.e[1];
	const int j_y = m.e[3];

	if(depth == maxdepth)
	{
		indices_out[index + 0] = Vec2i(x, y);
		indices_out[index + 1] = Vec2i(x + j_x, y + j_y);
		indices_out[index + 2] = Vec2i(x + i_x + j_x, y + i_y + j_y);
		indices_out[index + 3] = Vec2i(x + i_x, y + i_y);
		return index + 4;
	}
	else
	{
		index = run(depth + 1, maxdepth, mirror(m),    x,                                   y,                                   index, indices_out);
		index = run(depth + 1, maxdepth, m,            x + j_x*step,                        y + j_y*step,                        index, indices_out);
		index = run(depth + 1, maxdepth, m,            x + (i_x + j_x)*step,                y + (i_y + j_y)*step,                index, indices_out);
		index = run(depth + 1, maxdepth, negMirror(m), x + (2*i_x + j_x)*step - i_x - j_x,  y + (2*i_y + j_y)*step - i_y - j_y,  index, indices_out);
		return index;
	}
}


void Hilbert::generate(int maxdepth, Vec2i* indices_out)
{
	if(maxdepth == 0)
	{
		indices_out[0] = Vec2i(0, 0);
		return;
	}

	// 4^maxdepth = (2^2)^maxdepth = 2^(2*maxdepth)

	run(
		1, // depth
		maxdepth,
		Matrix2<int>::identity(), // direction
		0, // x
		0, // y
		0, // index
		indices_out
	);
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../graphics/PNGDecoder.h"
#include "../graphics/bitmap.h"


static const Colour3f colourForDensity(float density, float max_density)
{
	// From http://local.wasp.uwa.edu.au/~pbourke/texture_colour/colourramp/
	Colour3f c(1);

	const float vmin = 0;
	const float vmax = max_density;
	const float dv = vmax - vmin;
	const float v = density;

	if (v < (vmin + 0.25f * dv)) {
		c.r = 0;
		c.g = 4 * (v - vmin) / dv;
	} else if (v < (vmin + 0.5f * dv)) {
		c.r = 0;
		c.b = 1 + 4 * (vmin + 0.25f * dv - v) / dv;
	} else if (v < (vmin + 0.75f * dv)) {
		c.r = 4 * (v - vmin - 0.5f * dv) / dv;
		c.b = 0;
	} else {
		c.g = 1 + 4 * (vmin + 0.75f * dv - v) / dv;
		c.b = 0;
	}

	return c;
}


static size_t fourToN(size_t N)
{
	size_t x = 1;
	for(size_t i=0; i<N; ++i)
		x *= 4;
	return x;
}


void Hilbert::test()
{
	for(int max_depth=0; max_depth<8; ++max_depth)
	{
		std::vector<Vec2i> indices;
		indices.resize(getNumIndicesForDepth(max_depth));
		generate(max_depth, indices.data());

		testAssert(indices.size() == fourToN(max_depth));

		const int num_buckets_x = 1 << max_depth;
		testAssert(indices.size() == (size_t)(num_buckets_x * num_buckets_x));

		std::vector<bool> generated(indices.size(), false);

		const int block_disp_size = 8;
		Bitmap bitmap(block_disp_size * num_buckets_x, block_disp_size * num_buckets_x, 3, NULL);
		for(size_t i=0; i<indices.size(); ++i)
		{
			//conPrint("index " + ::toString((uint64)i) + ": " + toString(indices[i].x) + ", " + toString(indices[i].y));

			const Colour3f col = colourForDensity((float)i, (float)indices.size());

			const int x = indices[i].x;
			const int y = indices[i].y;
			testAssert(x >= 0 && x < num_buckets_x);
			testAssert(y >= 0 && y < num_buckets_x);

			for(int px=x*block_disp_size; px<(x+1)*block_disp_size; ++px)
				for(int py=y*block_disp_size; py<(y+1)*block_disp_size; ++py)
				{
					bitmap.setPixelComp(px, py, 0, (uint8)(col.r * 255.0f));
					bitmap.setPixelComp(px, py, 1, (uint8)(col.g * 255.0f));
					bitmap.setPixelComp(px, py, 2, (uint8)(col.b * 255.0f));
				}

			const int p_index = y*num_buckets_x + x;
			testAssert(!generated[p_index]);
			generated[p_index] = true;
		}

		// PNGDecoder::write(bitmap, "hilbert_curve_depth_" + toString(max_depth) + ".png");
	}
}


#endif
