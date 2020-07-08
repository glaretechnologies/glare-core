/*=====================================================================
UVUnwrapper.h
-------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "object.h"
#include "../simpleraytracer/raymesh.h"
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

	static void build(const RendererSettings& settings, Indigo::TaskManager& task_manager,
		RayMesh* mesh, PrintOutput& print_output);

	static void test();
};
