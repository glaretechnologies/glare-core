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
class InStream;
class OutStream;


struct AnimationNodeData
{
	AnimationNodeData();

	GLARE_ALIGNED_16_NEW_DELETE

	void writeToStream(OutStream& stream) const;
	void readFromStream(InStream& stream);

	Matrix4f node_hierarchical_to_world; // The overall transformation from walking up the node hierarchy.  Ephemeral data computed every frame.
	Matrix4f inverse_bind_matrix; // As read from GLTF file skin data.
	Matrix4f retarget_adjustment;
	//Matrix4f default_node_hierarchical_to_world; // This could be stored and used in the no-animations case, but we will just recompute it for now.

	// The non-animated transformation of the node.
	Vec4f trans;
	Quatf rot;
	Vec4f scale;

	std::string name; // Name of the node.  Might come in handy for retargetting animations.
	int parent_index; // Index of parent node, or -1 if this is a root.
};


struct PerAnimationNodeData
{
	void writeToStream(OutStream& stream) const;
	void readFromStream(InStream& stream);

	int translation_input_accessor; // Indexes into keyframe_times, or -1 if translation is not animated.
	int translation_output_accessor; // Indexes into output data, or -1 if translation is not animated.
	int rotation_input_accessor;
	int rotation_output_accessor;
	int scale_input_accessor;
	int scale_output_accessor;
};


struct AnimationDatum : public RefCounted
{
	void writeToStream(OutStream& stream) const;
	void readFromStream(uint32 file_version, InStream& stream, std::vector<std::vector<float> >& old_keyframe_times_out, std::vector<js::Vector<Vec4f, 16> >& old_output_data);

	void checkData(const std::vector<std::vector<float> >& keyframe_times, const std::vector<js::Vector<Vec4f, 16> >& output_data) const;

	std::string name;

	std::vector<PerAnimationNodeData> per_anim_node_data;
};


struct AnimationData
{
	AnimationData() {}

	void writeToStream(OutStream& stream) const;
	void readFromStream(InStream& stream);

	int findAnimation(const std::string& name);

	AnimationNodeData* findNode(const std::string& name); // Returns NULL if not found

	void loadAndRetargetAnim(InStream& stream);

	Vec4f getNodePositionModelSpace(const std::string& name, bool use_retarget_adjustment);
	
	Matrix4f skeleton_root_transform; // to-world transform of the skeleton root node.

	std::vector<AnimationNodeData> nodes;
	std::vector<int> sorted_nodes; // Indices of nodes, sorted such that children always come after parents.

	std::vector<int> joint_nodes; // Indices into nodes array.  Joint vertex values index into this array, or rather index into matrices with the same ordering.  Skin data.

	std::vector<std::vector<float> > keyframe_times; // For each input accessor index, a vector of input keyframe times.

	std::vector<js::Vector<Vec4f, 16> > output_data; // For each output accessor index, a vector of translations, rotations or scales.

	std::vector<Reference<AnimationDatum> > animations;
};


struct AnimationKeyFrameLocation
{
	int i_0;
	int i_1;
	float frac;
};
