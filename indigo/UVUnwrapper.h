/*=====================================================================
UVUnwrapper.h
-------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "object.h"
#include "../dll/include/IndigoMesh.h"
#include "SampleTypes.h"
#include "../graphics/ImageMap.h"
#include "../maths/vec2.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Vector.h"
#include "../utils/Mutex.h"
#include "../utils/HashMapInsertOnly2.h"


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

	static void test();
};
