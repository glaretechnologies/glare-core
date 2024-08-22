/*=====================================================================
ShelfPack.cpp
-------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "ShelfPack.h"


#include <algorithm>


struct AreaGreaterThan
{
	bool operator() (const BinRect& a, const BinRect& b)
	{
		return a.area() > b.area();
	}
};

struct Shelf
{
	float top_y;
	float height;
	float right_x;
};


void ShelfPack::shelfPack(std::vector<BinRect>& rects_in_out)
{
	std::vector<BinRect> rects = rects_in_out;

	for(size_t i=0; i<rects.size(); ++i)
		rects[i].original_index = (int)i;

	// Do initial sort of rectangles in descending area order.
	std::sort(rects.begin(), rects.end(), AreaGreaterThan());

	// Get sum area of rectangles
	float sum_A = 0;
	for(size_t i=0; i<rects.size(); ++i)
		sum_A += rects[i].area();

	// Choose a maximum width (x value) based on the sum of rectangle areas.
	const float max_x = std::sqrt(sum_A) * 1.2f;

	std::vector<Shelf> shelves;

	for(size_t i=0; i<rects.size(); ++i)
	{
		BinRect& rect = rects[i];

		float min_leftover_height = std::numeric_limits<float>::infinity();
		size_t best_shelf = 0;
		bool best_rotated = false;

		// Find best shelf to put rectangle on
		for(size_t s=0; s<shelves.size(); ++s)
		{
			// Try in initial orientation
			if(shelves[s].right_x + rect.w <= max_x)
			{
				const float leftover_height = shelves[s].height - rect.h; 
				if(leftover_height >= 0 &&  // If fits in height in this orientation
					leftover_height < min_leftover_height) // And this is the smallest leftover height so far:
				{
					min_leftover_height = leftover_height;
					best_shelf = s;
					best_rotated = false;
				}
			}

			// Try in rotated orientation 
			if(shelves[s].right_x + rect.h <= max_x)
			{
				const float leftover_height = shelves[s].height - rect.w; 
				if(leftover_height >= 0 && // If fits in height in this orientation
					leftover_height < min_leftover_height) // And this is the smallest leftover height so far:
				{
					min_leftover_height = leftover_height;
					best_shelf = s;
					best_rotated = true;
				}
			}
		}

		if(min_leftover_height != std::numeric_limits<float>::infinity()) // If we found a shelf to store the rect on:
		{
			// Store the rectangle
			const float rot_rect_w = best_rotated ? rect.h : rect.w; // width of possibly-rotated rectangle.

			rect.pos = Vec2f(shelves[best_shelf].right_x, shelves[best_shelf].top_y);
			rect.rotated = best_rotated;

			shelves[best_shelf].right_x += rot_rect_w;

			assert(/*rot_rect_h=*/(best_rotated ? rect.w : rect.h) <= shelves[best_shelf].height);
		}
		else
		{
			// If we couldn't find a shelf to store the rect on, create a new shelf, and store the rectangle on the shelf

			// Place rect with the largest side horizontal.
			const float rect_w = myMax(rect.w, rect.h); // sideways w
			const float rect_h = myMin(rect.w, rect.h); // sideways h

			shelves.push_back(Shelf());
			if(shelves.size() == 1)
				shelves.back().top_y = 0;
			else
				shelves.back().top_y = shelves[shelves.size() - 2].top_y + shelves[shelves.size() - 2].height; // top y = bottom y of preview shelf
			shelves.back().height = rect_h;
			shelves.back().right_x = rect_w;

			rect.pos = Vec2f(0, shelves.back().top_y);
			rect.rotated = rect_w != rect.w;
		}
	}

	// Now that we have packed all rectangles, scale so coordinates fill [0, 1]^2

	// Get the max right_x from all shelves
	float max_right_x = 0;
	for(size_t s=0; s<shelves.size(); ++s)
		max_right_x = myMax(max_right_x, shelves[s].right_x);

	const float cur_y = shelves.back().top_y + shelves.back().height;

	for(size_t i=0; i<rects.size(); ++i)
	{
		rects[i].pos.x /= max_right_x;
		rects[i].pos.y /= cur_y;
		rects[i].scale = Vec2f(1 / max_right_x, 1 / cur_y);

		// Update the corresponding unsorted rects_in_out rectangle
		rects_in_out[rects[i].original_index].pos     = rects[i].pos;
		rects_in_out[rects[i].original_index].scale   = rects[i].scale;
		rects_in_out[rects[i].original_index].rotated = rects[i].rotated;
	}
}


#if BUILD_TESTS


#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../graphics/PNGDecoder.h"
#include "../graphics/bitmap.h"
#include "../maths/PCG32.h"


void ShelfPack::test()
{
	conPrint("ShelfPack::test()");

	//========================== Test bin packing =====================================
	if(true)
	{
		PCG32 rng(1);

		std::vector<BinRect> rects(100);
		for(size_t i=0; i<rects.size(); ++i)
		{
			rects[i].w = rng.unitRandom() * 0.2f;
			rects[i].h = rng.unitRandom() * 0.2f;
		}

		// Do bin packing
		shelfPack(rects);

		conPrint("Result x-scale: " + ::doubleToStringNSigFigs(rects[0].scale.x, 3) + ", y-scale: " + doubleToStringNSigFigs(rects[0].scale.y, 3));
		conPrint("Result overall scale: " + doubleToStringNSigFigs(rects[0].scale.x * rects[0].scale.y, 4));

		// Draw results
		const int W = 1000;
		Bitmap bitmap(W, W, 3, NULL);
		bitmap.zero();
		for(size_t i=0; i<rects.size(); ++i)
		{
			int r = (int)(rng.unitRandom() * 255);
			int g = (int)(rng.unitRandom() * 255);
			int b = (int)(100 + rng.unitRandom() * 155);

			const int sx = (int)(rects[i].pos.x * W);
			const int sy = (int)(rects[i].pos.y * W);
			const int endx = sx + (int)((rects[i].rotatedWidth())  * rects[i].scale.x * W);
			const int endy = sy + (int)((rects[i].rotatedHeight()) * rects[i].scale.y * W);
			for(int y=sy; y<endy; ++y)
			for(int x=sx; x<endx; ++x)
			{
				if(x >= 0 && x < W && y >= 0 && y < W)
				{
					bitmap.getPixelNonConst(x, y)[0] = (uint8)r;
					bitmap.getPixelNonConst(x, y)[1] = (uint8)g;
					bitmap.getPixelNonConst(x, y)[2] = (uint8)b;
				}
			}
		}

		PNGDecoder::write(bitmap, "d:/tempfiles/binpacking.png");
	}

	conPrint("ShelfPack::test() done.");
}


#endif // BUILD_TESTS
