/*=====================================================================
DXTImageMap.cpp
---------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#include "DXTImageMap.h"


#include "ImageMap.h"
#include "DXTCompression.h"
#include "../utils/Exception.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"


DXTImageMap::DXTImageMap()
:	width(0), height(0), N(0), gamma(2.2f), num_blocks_x(0), num_blocks_y(0), log2_uint64s_per_block(0)
{
}


DXTImageMap::DXTImageMap(size_t width_, size_t height_, size_t N_)
:	width(width_), height(height_), N(N_), gamma(2.2f), ds_over_2(0.5f / width_), dt_over_2(0.5f / height_)
{
	assert(N == 3 || N == 4);

	num_blocks_x = Maths::roundedUpDivide(width,  (size_t)4);
	num_blocks_y = Maths::roundedUpDivide(height, (size_t)4);
	log2_uint64s_per_block = (N == 3) ? 0 : 1;

	try
	{
		data.resizeNoCopy(num_blocks_x * num_blocks_y * ((N == 3) ? 1 : 2));
	}
	catch(std::bad_alloc&)
	{
		throw Indigo::Exception("Failed to create image (memory allocation failure)");
	}
}


DXTImageMap::~DXTImageMap() {}


DXTImageMap& DXTImageMap::operator = (const DXTImageMap& other)
{
	if(this == &other)
		return *this;

	width = other.width;
	height = other.height;
	num_blocks_x = Maths::roundedUpDivide(width,  (size_t)4);
	num_blocks_y = Maths::roundedUpDivide(height, (size_t)4);
	log2_uint64s_per_block = other.log2_uint64s_per_block;
	N = other.N;
	data = other.data;
	gamma = other.gamma;
	ds_over_2 = other.ds_over_2;
	dt_over_2 = other.dt_over_2;
	return *this;
}


bool DXTImageMap::operator == (const DXTImageMap& other) const
{
	if(width != other.width || height != other.height || N != other.N)
		return false;

	return data == other.data;
}


void DXTImageMap::resize(size_t width_, size_t height_, size_t N_)
{
	width = width_;
	height = height_;
	num_blocks_x = Maths::roundedUpDivide(width,  (size_t)4);
	num_blocks_y = Maths::roundedUpDivide(height, (size_t)4);
	log2_uint64s_per_block = (N == 3) ? 0 : 1;
	N = N_;
	ds_over_2 = 0.5f / width_;
	dt_over_2 = 0.5f / height_;

	try
	{
		data.resize(num_blocks_x * num_blocks_y * ((N == 3) ? 1 : 2));
	}
	catch(std::bad_alloc&)
	{
		throw Indigo::Exception("Failed to create image (memory allocation failure)");
	}
}


void DXTImageMap::resizeNoCopy(size_t width_, size_t height_, size_t N_)
{
	width = width_;
	height = height_;
	num_blocks_x = Maths::roundedUpDivide(width,  (size_t)4);
	num_blocks_y = Maths::roundedUpDivide(height, (size_t)4);
	log2_uint64s_per_block = (N == 3) ? 0 : 1;
	N = N_;
	ds_over_2 = 0.5f / width_;
	dt_over_2 = 0.5f / height_;

	try
	{
		data.resizeNoCopy(num_blocks_x * num_blocks_y * ((N == 3) ? 1 : 2));
	}
	catch(std::bad_alloc&)
	{
		throw Indigo::Exception("Failed to create image (memory allocation failure)");
	}
}


/*
See https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_compression_dxt1.txt
byte index:
   7   6   5   4   3   2   1   0
|              |       |       |
    mask bits      c1      c0

msb                       lsb


      c1r    |     c1g      |     c1b    |      c0r    |     c0g      |     c0b    |
32           27             21          16             11             5            0
*/


INDIGO_STRONG_INLINE static void getBlockColours(uint32 block, uint32* cols_out)
{
	uint32 t0 = block & 0xF800;
	uint32 c0 = (t0 >> 8) | (t0 >> (8 + 5));  // shift 16 - 8 bits to right, isolate 5 bits
	uint32 t1 = block & 0x7E0;
	uint32 c1 = (t1 >> 3) | (t1 >> (3 + 6)); // shift 11 - 8 bits to right, isolate 6 bits
	uint32 t2 = block & 0x1F;
	uint32 c2 = (t2 << 3) | (t2 >> 2); // shift 5 - 8 bits to right = 3 to left, isolate 5 bits

	uint32 t3 = block & 0xF8000000;
	uint32 c3 = (t3 >> 24) | (t3 >> (24 + 5)); // shift 32 - 8 bits to right, isolate 5 bits
	uint32 t4 = block & 0x7E00000;
	uint32 c4 = (t4 >> 19) | (t4 >> (19 + 6)); // shift 27 - 8 bits to right, isolate 6 bits
	uint32 t5 = block & 0x1F0000;
	uint32 c5 = (t5 >> 13) | (t5 >> (13 + 5)); // shift 21 - 8 bits to right, isolate 5 bits

	Vec4i c0v(c0, c1, c2, 0);
	Vec4i c1v(c3, c4, c5, 0);
	_mm_store_si128((__m128i*)(cols_out + 0), c0v.v);
	_mm_store_si128((__m128i*)(cols_out + 4), c1v.v);

	// c_2 = 2/3 c_0 + 1/3 c_1
	Vec4i c2v = mulLo((c0v << 1) + c1v, Vec4i(2796203)) >> 23; // mul and shift is to divide by 3.  See derivation in DXTImageMapTests.cpp.
	_mm_store_si128((__m128i*)(cols_out + 8), c2v.v);

	// c_3 = 1/3 c_0 + 2/3 c_1
	Vec4i c3v = mulLo(c0v + (c1v << 1), Vec4i(2796203)) >> 23;
	_mm_store_si128((__m128i*)(cols_out + 12), c3v.v);
}


Vec4i DXTImageMap::pixelRGBColourBytes(size_t x, size_t y) const
{
	const size_t block_x = x / 4;
	const size_t block_y = y / 4;
	const size_t block_i = block_y*num_blocks_x + block_x;
	const size_t in_block_x = x & 0x3; // x mod 4
	const size_t in_block_y = y & 0x3; // y mod 4
	const size_t in_block_i = in_block_y * 4 + in_block_x; // [0, 16)

	// If we have alpha data as well, uint64s_per_block=2, so log2_uint64s_per_block=1.  Alpha block is stored before RGB block.
	const uint64 rgb_block = data[(block_i << log2_uint64s_per_block) + log2_uint64s_per_block];

	// Decode block RGB colours
	SSE_ALIGN uint32 cols[16];
	getBlockColours((uint32)rgb_block, cols);

	const size_t bit_index = 2*in_block_i;
	const size_t table_index = (rgb_block >> (32 + bit_index)) & 0x3;

	return loadVec4i(&cols[table_index*4]);
}


uint32 DXTImageMap::pixelAlphaByte(size_t x, size_t y) const
{
	assert(N == 4);
	const size_t block_x = x / 4;
	const size_t block_y = y / 4;
	const size_t block_i = block_y*num_blocks_x + block_x;
	const size_t in_block_x = x & 0x3; // x mod 4
	const size_t in_block_y = y & 0x3; // y mod 4
	const size_t in_block_i = in_block_y * 4 + in_block_x; // [0, 16)

	const uint64 alpha_block = data[block_i*2 + 0];

	/*
	From https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_compression_s3tc.txt:
	in ascending address order:
	alpha0, alpha1, bits_0, bits_1, bits_2, bits_3, bits_4, bits_5

	or MSB on left, LSB on right of uint64:
	bits_5, bits_4, bits_3, bits_2, bits_1, bits_0, alpha1, alpha0
	*/
	// Decode alpha
	const uint32 a0 = (alpha_block >> 0) & 0xFF;
	const uint32 a1 = (alpha_block >> 8) & 0xFF;

	SSE_ALIGN uint32 alpha_vals[8];
	Vec4i a0v(a0);
	Vec4i a1v(a1);
	Vec4i a03 = mulLo(mulLo(Vec4i(a0), Vec4i(7, 0, 6, 5)) + mulLo(Vec4i(a1), Vec4i(0, 7, 1, 2)), Vec4i(149797)) >> 20;
	Vec4i a47 = mulLo(mulLo(Vec4i(a0), Vec4i(4, 3, 2, 1)) + mulLo(Vec4i(a1), Vec4i(3, 4, 5, 6)), Vec4i(149797)) >> 20;
	_mm_store_si128((__m128i*)(alpha_vals + 0), a03.v);
	_mm_store_si128((__m128i*)(alpha_vals + 4), a47.v);

	assert(alpha_vals[0] == a0);// = (7*a0 + 0*a1)/7
	assert(alpha_vals[1] == a1);// = (0*a0 + 7*a1)/7
	assert(alpha_vals[2] == (6*a0 + 1*a1)/7);
	assert(alpha_vals[3] == (5*a0 + 2*a1)/7);
	assert(alpha_vals[4] == (4*a0 + 3*a1)/7);
	assert(alpha_vals[5] == (3*a0 + 4*a1)/7);
	assert(alpha_vals[6] == (2*a0 + 5*a1)/7);
	assert(alpha_vals[7] == (1*a0 + 6*a1)/7);

	const size_t bit_index = 3*in_block_i;
	const size_t table_index = (alpha_block >> (16 + bit_index)) & 0x7; // Shift so that bit_index=0 isolates lsb of bits_0 etc..  Need to shift off alpha1 and alpha0.
	assert(table_index < 8);
	return alpha_vals[table_index];
}


const Colour3<Map2D::Value> DXTImageMap::pixelColour(size_t x, size_t y) const
{
	const Vec4i col = pixelRGBColourBytes(x, y);
	const Vec4f c = toVec4f(col) * (1 / (Map2D::Value)255);
	return Colour3<Map2D::Value>(c[0], c[1], c[2]);
}


const Map2D::Value DXTImageMap::pixelComponent(size_t x, size_t y, size_t c) const
{
	// To be removed

	assert(0);
	return 0;
	//const uint32 col = pixelRGBColourBytes(x, y);
	//const uint32 v = (col >> (24 - 8*c)) & 0xFF;
	//return v * (1 / (Map2D::Value)255);
}


// X and Y are normalised image coordinates.
// (X, Y) = (0, 0) is at the bottom left of the image.
// Although this returns an SSE 4-vector, only the first three RGB components will be set.
const Colour4f DXTImageMap::vec3SampleTiled(Coord u, Coord v) const
{
	Colour4f colour_out;

	// Get fractional normalised image coordinates
	const Coord u_frac_part = Maths::fract(u);
	const Coord v_frac_part = Maths::fract(-v);

	// Convert from normalised image coords to pixel coordinates
	const Coord u_pixels = u_frac_part * (Coord)width;
	const Coord v_pixels = v_frac_part * (Coord)height;

	// Get pixel indices
	const size_t ut = myMin((size_t)u_pixels, width - 1);
	const size_t vt = myMin((size_t)v_pixels, height - 1);

	const Coord ufrac = u_pixels - (Coord)ut;
	const Coord vfrac = v_pixels - (Coord)vt;
	const Coord oneufrac = 1 - ufrac;
	const Coord onevfrac = 1 - vfrac;

	const Value a = oneufrac * onevfrac; // Top left pixel weight
	const Value b = ufrac * onevfrac; // Top right pixel weight
	const Value c = oneufrac * vfrac; // Bottom left pixel weight
	const Value d = ufrac * vfrac; // Bottom right pixel weight

	assert(ut < width && vt < height);

	const size_t x = ut;
	const size_t y = vt;

	const size_t x_1 = (x + 1) >= width  ? 0 : x + 1;
	const size_t y_1 = (y + 1) >= height ? 0 : y + 1;

	const size_t in_block_x = x & 0x3; // x mod 4
	const size_t in_block_y = y & 0x3; // y mod 4

	Vec4i col_a, col_b, col_c, col_d;

	if(in_block_x <= 2)
	{
		if(in_block_y <= 2)
		{
			// Special case: all 4 pixels for bilinear sample lie in the same block:
			const size_t block_x = x / 4;
			const size_t block_y = y / 4;
			const size_t block_i = (block_y*num_blocks_x + block_x);
			const uint64 rgb_block = data[(block_i << log2_uint64s_per_block) + log2_uint64s_per_block];

			uint32 cols[4 * 4];
			getBlockColours((uint32)rgb_block, cols);

			const size_t a_offset = 32 + 2*(in_block_y * 4 + in_block_x);
			const size_t a_table_index = (rgb_block >> (a_offset +  0)) & 0x3; // top left:  shift amount = 32+2*(in_block_y * 4 + in_block_x + 0)
			const size_t b_table_index = (rgb_block >> (a_offset +  2)) & 0x3; // top right: shift amount = 32+2*(in_block_y * 4 + in_block_x + 1)
			const size_t c_table_index = (rgb_block >> (a_offset +  8)) & 0x3; // bot left:  shift amount = 32+2*(in_block_y * 4 + in_block_x + 4)
			const size_t d_table_index = (rgb_block >> (a_offset + 10)) & 0x3; // bot right: shift amount = 32+2*(in_block_y * 4 + in_block_x + 5)

			col_a = loadVec4i(&cols[a_table_index*4]);
			col_b = loadVec4i(&cols[b_table_index*4]);
			col_c = loadVec4i(&cols[c_table_index*4]);
			col_d = loadVec4i(&cols[d_table_index*4]);
		}
		else // else in_block_y == 3:
		{
			const size_t block_x = x / 4;
			// bottom two samples lie in next block down
			// Compute top pixel colours:
			{
				const size_t block_y = y / 4;
				const size_t block_i = (block_y*num_blocks_x + block_x);
				const uint64 rgb_block = data[(block_i << log2_uint64s_per_block) + log2_uint64s_per_block];

				uint32 cols[4 * 4];
				getBlockColours((uint32)rgb_block, cols);

				const size_t a_offset = 32 + 2*(in_block_y * 4 + in_block_x);
				const size_t a_table_index = (rgb_block >> (a_offset +  0)) & 0x3; // top left:  shift amount = 32+2*(in_block_y * 4 + in_block_x + 0)
				const size_t b_table_index = (rgb_block >> (a_offset +  2)) & 0x3; // top right: shift amount = 32+2*(in_block_y * 4 + in_block_x + 1)

				col_a = loadVec4i(&cols[a_table_index*4]);
				col_b = loadVec4i(&cols[b_table_index*4]);
			}
			// Compute bottom pixel colours:
			{
				const size_t block_y = y_1 / 4; // NOTE: may have wrapped back to block_y = 0.
				const size_t block_i = (block_y*num_blocks_x + block_x);
				const uint64 rgb_block = data[(block_i << log2_uint64s_per_block) + log2_uint64s_per_block];

				const size_t use_in_block_y = y_1 & 0x3; // y_1 mod 4.

				uint32 cols[4 * 4];
				getBlockColours((uint32)rgb_block, cols);

				const size_t c_offset = 32 + 2*(use_in_block_y * 4 + in_block_x);
				const size_t c_table_index = (rgb_block >> (c_offset + 0)) & 0x3; // bot left:  shift amount = 32+2*(in_block_y * 4 + in_block_x + 4)
				const size_t d_table_index = (rgb_block >> (c_offset + 2)) & 0x3; // bot right: shift amount = 32+2*(in_block_y * 4 + in_block_x + 5)

				col_c = loadVec4i(&cols[c_table_index*4]);
				col_d = loadVec4i(&cols[d_table_index*4]);
			}
		}
	}
	else // else in_block_x == 3:
	{
		if(in_block_y <= 2)
		{
			// right two samples lie in next block to right.
			// Compute left pixel colours:
			const size_t block_y = y / 4;
			{
				const size_t block_x = x / 4;
				const size_t block_i = (block_y*num_blocks_x + block_x);
				const uint64 rgb_block = data[(block_i << log2_uint64s_per_block) + log2_uint64s_per_block];

				uint32 cols[4 * 4];
				getBlockColours((uint32)rgb_block, cols);

				const size_t a_offset = 32 + 2*(in_block_y * 4 + in_block_x);
				const size_t a_table_index = (rgb_block >> (a_offset + 0)) & 0x3; // top left
				const size_t c_table_index = (rgb_block >> (a_offset + 8)) & 0x3; // bot left

				col_a = loadVec4i(&cols[a_table_index*4]);
				col_c = loadVec4i(&cols[c_table_index*4]);
			}
			// Compute right pixel colours:
			{
				const size_t block_x = x_1 / 4; // NOTE: may have wrapped back to block_x = 0.
				const size_t block_i = (block_y*num_blocks_x + block_x);
				const uint64 rgb_block = data[(block_i << log2_uint64s_per_block) + log2_uint64s_per_block];

				const size_t use_in_block_x = x_1 & 0x3; // x_1 mod 4.

				uint32 cols[4 * 4];
				getBlockColours((uint32)rgb_block, cols);

				const size_t b_offset = 32 + 2*(in_block_y * 4 + use_in_block_x);
				const size_t b_table_index = (rgb_block >> (b_offset + 0)) & 0x3; // top right
				const size_t d_table_index = (rgb_block >> (b_offset + 8)) & 0x3; // bot right

				col_b = loadVec4i(&cols[b_table_index*4]);
				col_d = loadVec4i(&cols[d_table_index*4]);
			}
		}
		else
		{
			// Slow case: decode each pixel separately.
			col_a = pixelRGBColourBytes(x, y);
			col_b = pixelRGBColourBytes(x_1, y);
			col_c = pixelRGBColourBytes(x, y_1);
			col_d = pixelRGBColourBytes(x_1, y_1);
		}
	}

	colour_out = Colour4f(
		((	toVec4f(col_a)*a +
			toVec4f(col_b)*b +
			toVec4f(col_c)*c +
			toVec4f(col_d)*d)
			* (1 / (Map2D::Value)255)).v);

	return colour_out;
}


Map2D::Value DXTImageMap::sampleSingleChannelTiled(Coord u, Coord v, size_t channel) const
{
	assert(channel == 3); // This is only used for evaluating alpha.
	assert(N == 4);

	// Get fractional normalised image coordinates
	const Coord u_frac_part = Maths::fract(u);
	const Coord v_frac_part = Maths::fract(-v);

	// Convert from normalised image coords to pixel coordinates
	const Coord u_pixels = u_frac_part * (Coord)width;
	const Coord v_pixels = v_frac_part * (Coord)height;

	// Get pixel indices
	const size_t ut = myMin((size_t)u_pixels, width - 1);
	const size_t vt = myMin((size_t)v_pixels, height - 1);

	const Coord ufrac = u_pixels - (Coord)ut;
	const Coord vfrac = v_pixels - (Coord)vt;
	const Coord oneufrac = 1 - ufrac;
	const Coord onevfrac = 1 - vfrac;

	const Value a = oneufrac * onevfrac; // Top left pixel weight
	const Value b = ufrac * onevfrac; // Top right pixel weight
	const Value c = oneufrac * vfrac; // Bottom left pixel weight
	const Value d = ufrac * vfrac; // Bottom right pixel weight

	assert(ut < width && vt < height);

	const size_t x = ut;
	const size_t y = vt;

	const size_t x_1 = (x + 1) >= width  ? 0 : x + 1;
	const size_t y_1 = (y + 1) >= height ? 0 : y + 1;

	const size_t in_block_x = x & 0x3; // x mod 4
	const size_t in_block_y = y & 0x3; // y mod 4

	if(in_block_x <= 2 && in_block_y <= 2) // Special case: all 4 pixels for bilinear sample lie in block:
	{
		const size_t block_x = x / 4;
		const size_t block_y = y / 4;
		const size_t block_i = (block_y*num_blocks_x + block_x);

		const uint64 alpha_block = data[block_i*2 + 0];

		/*
		From https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_compression_s3tc.txt:
		in ascending address order:
		alpha0, alpha1, bits_0, bits_1, bits_2, bits_3, bits_4, bits_5

		or MSB on left, LSB on right of uint64:
		bits_5, bits_4, bits_3, bits_2, bits_1, bits_0, alpha1, alpha0
		*/
		// Decode alpha
		const uint32 a0 = (alpha_block >> 0) & 0xFF;
		const uint32 a1 = (alpha_block >> 8) & 0xFF;

		// Compute table of alpha values.
		SSE_ALIGN uint32 alpha_vals[8];
		const Vec4i a0v(a0);
		const Vec4i a1v(a1);
		const Vec4i a03 = mulLo(mulLo(Vec4i(a0), Vec4i(7, 0, 6, 5)) + mulLo(Vec4i(a1), Vec4i(0, 7, 1, 2)), Vec4i(149797)) >> 20; // mul and shift is to divide by 7.  See derivation in DXTImageMapTests.cpp.
		const Vec4i a47 = mulLo(mulLo(Vec4i(a0), Vec4i(4, 3, 2, 1)) + mulLo(Vec4i(a1), Vec4i(3, 4, 5, 6)), Vec4i(149797)) >> 20;
		_mm_store_si128((__m128i*)(alpha_vals + 0), a03.v);
		_mm_store_si128((__m128i*)(alpha_vals + 4), a47.v);

		assert(alpha_vals[0] == a0); // = (7*a0 + 0*a1)/7
		assert(alpha_vals[1] == a1); // = (0*a0 + 7*a1)/7
		assert(alpha_vals[2] == (6*a0 + 1*a1)/7);
		assert(alpha_vals[3] == (5*a0 + 2*a1)/7);
		assert(alpha_vals[4] == (4*a0 + 3*a1)/7);
		assert(alpha_vals[5] == (3*a0 + 4*a1)/7);
		assert(alpha_vals[6] == (2*a0 + 5*a1)/7);
		assert(alpha_vals[7] == (1*a0 + 6*a1)/7);

		const size_t a_offset = 16 + 3*(in_block_y * 4 + in_block_x); // Number of bits alpha_block needs to be shifted right to isolate table index for top-left pixel.
		// top-right pixel is 3 more bits, bot left is the next row (of 4 pixels) down, so 4*3 bits etc..
		// Need to shift off alpha1 and alpha0, hence the 16.

		const size_t a_table_index = (alpha_block >> (a_offset + 3*0)) & 0x7;
		const size_t b_table_index = (alpha_block >> (a_offset + 3*1)) & 0x7;
		const size_t c_table_index = (alpha_block >> (a_offset + 3*4)) & 0x7;
		const size_t d_table_index = (alpha_block >> (a_offset + 3*5)) & 0x7;
		assert(a_table_index < 8);

		const uint32 a_A = alpha_vals[a_table_index];
		const uint32 b_A = alpha_vals[b_table_index];
		const uint32 c_A = alpha_vals[c_table_index];
		const uint32 d_A = alpha_vals[d_table_index];

		return (a * a_A + b * b_A + c * c_A + d * d_A) * (1 / (Map2D::Value)255);
	}
	else
	{
		// Slow case: decode each pixel separately.
		const uint32 acol = pixelAlphaByte(x, y);
		const uint32 bcol = pixelAlphaByte(x_1, y);
		const uint32 ccol = pixelAlphaByte(x, y_1);
		const uint32 dcol = pixelAlphaByte(x_1, y_1);

		return (a * acol + b * bcol + c * ccol + d * dcol) * (1 / (Map2D::Value)255);
	}
}


// s and t are normalised image coordinates.
// Returns texture value (v) at (s, t)
// Also returns dv/ds and dv/dt.
Map2D::Value DXTImageMap::getDerivs(Coord s, Coord t, Value& dv_ds_out, Value& dv_dt_out) const
{
	assert(0);
	return 0.f;//TEMP
}


Reference<Map2D> DXTImageMap::extractAlphaChannel() const
{
	assert(0);
	return Reference<Map2D>();
}


bool DXTImageMap::isAlphaChannelAllWhite() const
{
	if(!hasAlphaChannel())
		return true;

	for(size_t y=0; y<height; ++y)
		for(size_t x=0; x<width; ++x)
			if(pixelAlphaByte(x, y) != 255)
				return false;

	return true;
}


Reference<Map2D> DXTImageMap::extractChannelZero() const
{
	assert(0);
	//ImageMap<V, VTraits>* new_map = new ImageMap<V, VTraits>(width, height, 1);
	//for(size_t y=0; y<height; ++y)
	//	for(size_t x=0; x<width; ++x)
	//	{
	//		new_map->getPixel(x, y)[0] = this->getPixel(x, y)[0];
	//	}

	return Reference<Map2D>();
}


Reference<ImageMapFloat> DXTImageMap::extractChannelZeroLinear() const
{
	assert(0);
	//ImageMapFloat* new_map = new ImageMap<float, FloatComponentValueTraits>(width, height, 1);
	//for(size_t y=0; y<height; ++y)
	//	for(size_t x=0; x<width; ++x)
	//	{
	//		new_map->getPixel(x, y)[0] = VTraits::toLinear(this->getPixel(x, y)[0], this->gamma);
	//	}

	return Reference<ImageMapFloat>();
}


Reference<Image> DXTImageMap::convertToImage() const
{
	assert(0);
	return Reference<Image>();
}


Reference<Map2D> DXTImageMap::getBlurredLinearGreyScaleImage(Indigo::TaskManager& task_manager) const
{
	assert(0);
	return Reference<Map2D>();
}


Reference<ImageMap<float, FloatComponentValueTraits> > DXTImageMap::resizeToImageMapFloat(const int target_width, bool& is_linear_out) const
{
	assert(0);
	return Reference<ImageMapFloat>();
}


size_t DXTImageMap::getBytesPerPixel() const
{
	//assert(0);
	return 1; // TEMP WRONG
}


size_t DXTImageMap::getByteSize() const
{
	return data.dataSizeBytes();
}


DXTImageMapRef DXTImageMap::compressImageMap(Indigo::TaskManager& task_manager, const ImageMapUInt8& map)
{
	DXTImageMapRef dxt_image = new DXTImageMap(map.getWidth(), map.getHeight(), map.getN());

	DXTCompression::TempData temp_data;
	DXTCompression::compress(task_manager, temp_data, &map, (uint8*)dxt_image->data.data(), dxt_image->data.dataSizeBytes());

	return dxt_image;
}


// There are more tests in DXTImageMapTests.cpp
// checkBlockDecoding() is left in this file because it uses the static/local function getBlockColours().
#if BUILD_TESTS


#include "../indigo/globals.h"
#include "../indigo/TestUtils.h"
#define STB_DXT_STATIC 1
#define STB_DXT_IMPLEMENTATION 1
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable:4244) // Disable warning C4244: onversion from 'int' to 'unsigned char', possible loss of data
#endif
//#include "../libs/stb/stb_dxt.h"
#ifdef _WIN32
#pragma warning(pop)
#endif


// Exhaustively check the decoding of each possible 32 bit value to 2 RGB colours, against stb__EvalColors
/*static void checkBlockDecoding()
{
	conPrint("checkBlockDecoding()");
	for(uint64 i=0; i<=std::numeric_limits<uint32>::max(); i++)
	{
		size_t block = i;
		uint32 cols[4 * 4];
		getBlockColours((uint32)block, cols);

		// Check against stb__EvalColors
		const size_t c0 = block & 0xFFFF;
		const size_t c1 = (block >> 16) & 0xFFFF;
		unsigned char stb_cols[16];
		stb__EvalColors(stb_cols, (uint16)c0, (uint16)c1);
		for(int c=0; c<4; ++c)
		{
			testAssert(stb_cols[c*4 + 0] == cols[c*4 + 0]);
			testAssert(stb_cols[c*4 + 1] == cols[c*4 + 1]);
			testAssert(stb_cols[c*4 + 2] == cols[c*4 + 2]);
		}

		if((i % 100000000) == 0)
			printVar(i);
	}
	conPrint("done.");
}*/


void DXTImageMap::test()
{
	conPrint("DXTImageMap::test()");

	// stb__InitDXT();

	// This is quite slow:
	// checkBlockDecoding();
}


#endif
