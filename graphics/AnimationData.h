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
#include "../utils/Vector.h"
#include <string>
#include <vector>


struct AnimationNodeData
{
	GLARE_ALIGNED_16_NEW_DELETE

	Matrix4f node_hierarchical_to_world; // The overall transformation from walking up the node hierarchy
	Matrix4f inverse_bind_matrix;

	// The non-animated transformation of the node.
	Vec4f trans;
	Quatf rot;
	Vec4f scale;

	int parent_index;
};


struct PerAnimationNodeData
{
	int translation_input_accessor; // Indexes into keyframe_times, or -1 if translation is not animated.
	int translation_output_accessor; // Indexes into output data, or -1 if translation is not animated.
	int rotation_input_accessor;
	int rotation_output_accessor;
	int scale_input_accessor;
	int scale_output_accessor;
};


struct AnimationDatum : public RefCounted
{
	std::string name;

	std::vector<PerAnimationNodeData> per_anim_node_data;

	std::vector<std::vector<float> > keyframe_times; // For each input accessor index, a vector of input keyframe times.

	std::vector<js::Vector<Vec4f, 16> > output_data; // For each output accessor index, a vector of translations, rotations or scales.
};


struct AnimationData
{
	Matrix4f skeleton_root_transform;

	std::vector<AnimationNodeData> nodes;
	std::vector<int> sorted_nodes; // Node indices sorted such that children always come after parents.

	std::vector<int> joint_nodes; // Indices into nodes array.

	std::vector<Reference<AnimationDatum> > animations;
};


struct AnimationKeyFrameLocation
{
	int i_0;
	int i_1;
	float frac;
};
