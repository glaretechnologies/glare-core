/*=====================================================================
ShelfPack.h
-----------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "../maths/vec2.h"
#include <vector>


struct BinRect
{
	friend class ShelfPack;

	float w, h; // Input: width and height.  Only relative widths and heights are important.
	
	Vec2f pos; // Output: position where rectangle was placed in the unit square.
	Vec2f scale; // Output: scaling of resulting rotated input width and height, so that the rect fits in the atlas on the unit square.
	bool rotated; // Output: was the rect rotated 90 degrees?

	float area() const { return w * h; }
	float rotatedWidth()  const { return rotated ? h : w; }
	float rotatedHeight() const { return rotated ? w : h; }

private:
	int original_index; // private to ShelfPack
};


/*=====================================================================
ShelfPack
---------
=====================================================================*/
class ShelfPack
{
public:

	// Pack rectangles into the unit square, [0, 1]^2.
	// Output member variables in each BinRect will be updated.
	static void shelfPack(std::vector<BinRect>& rects);

	static void test();
};
