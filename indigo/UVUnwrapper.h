/*=====================================================================
UVUnwrapper.h
-------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "../dll/include/IndigoMesh.h"
#include "SampleTypes.h"
#include "../graphics/ImageMap.h"
#include "../maths/vec2.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Vector.h"
#include "../utils/HashMapInsertOnly2.h"
class Matrix4f;
class PrintOutput;


struct BinRect
{
	float w, h; // Input width and height
	Vec2f pos; // Output: position where rectangle was placed.
	Vec2f scale; // Output: scaling of resulting rotated width and height.
	int index; // Input, output: Index of patch
	bool rotated; // Output: was the rect rotated 90 degrees?

	float area() const { return w * h; }
	float rotatedWidth()  const { return rotated ? h : w; }
	float rotatedHeight() const { return rotated ? w : h; }
};


/*=====================================================================
UVUnwrapper
------------
Generates a UV mapping for a mesh, for lightmapping purposes.
=====================================================================*/
class UVUnwrapper
{
public:
	UVUnwrapper();
	~UVUnwrapper();

	struct Results
	{
		size_t num_patches;

	};

	// A margin of 2 pixels on a 1024 pixel wide image would have normed_margin = 2 / 1024
	static Results build(Indigo::Mesh& mesh, const Matrix4f& ob_to_world, PrintOutput& print_output, float normed_margin);


	// Pack rectangles into the unit square, [0, 1]^2.
	// Output member variables in each BinRect will be updated.
	static void shelfPack(std::vector<BinRect>& rects);

	static void test();
};
