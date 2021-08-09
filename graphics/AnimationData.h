/*=====================================================================
AnimationData.h
---------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "../maths/vec3.h"
#include "../maths/Quat.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include <string>
#include <vector>

//TEMP:
struct AnimationNodeData
{
	int parent_index;
};


struct AnimationDatum : public RefCounted
{
	std::string name;
	std::vector<float> time_vals;

	// Translation for node n at input time index i
	//	i * num_nodes + n						// is n * time_vals.size() + i
	std::vector<Vec3f> translations;
	std::vector<Quatf> rotations;
	std::vector<Vec3f> scales;
};


struct AnimationData
{
	std::vector<AnimationNodeData> nodes;
	std::vector<int> sorted_nodes; // Node indices sorted such that children always come after parents.

	std::vector<Reference<AnimationDatum> > animations;
};
