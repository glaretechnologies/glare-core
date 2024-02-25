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
#include <map>
class InStream;
class OutStream;


struct AnimationNodeData
{
	AnimationNodeData();

	GLARE_ALIGNED_16_NEW_DELETE

	void writeToStream(OutStream& stream) const;
	void readFromStream(InStream& stream);

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


struct KeyFrameTimeInfo
{
	KeyFrameTimeInfo() : equally_spaced(false) {}

	std::vector<float> times;

	bool equally_spaced;
	int times_size;
	float t_0;
	float t_back;
	float spacing;
	float recip_spacing;
};


struct AnimationDatum : public RefCounted
{
	AnimationDatum() : anim_len(-1) {}

	void writeToStream(OutStream& stream) const;
	void readFromStream(uint32 file_version, InStream& stream, std::vector<KeyFrameTimeInfo>& old_keyframe_times_out, std::vector<js::Vector<Vec4f, 16> >& old_output_data);

	void checkData(const std::vector<KeyFrameTimeInfo>& keyframe_times, const std::vector<js::Vector<Vec4f, 16> >& output_data) const;

	size_t getTotalMemUsage() const;

	std::string name;

	std::vector<PerAnimationNodeData> per_anim_node_data;

	std::vector<int> used_input_accessor_indices;

	float anim_len;
};


struct VRMBoneInfo
{
	int node_index;
};

struct GLTFVRMExtension : public RefCounted
{
	GLTFVRMExtension() {}

	std::map<std::string, VRMBoneInfo> human_bones;
};
typedef Reference<GLTFVRMExtension> GLTFVRMExtensionRef;



struct AnimationData
{
	AnimationData() : retarget_adjustments_set(false) {}

	void build(); // Builds keyframe_times data, computes animation length.

	void writeToStream(OutStream& stream) const;
	void readFromStream(InStream& stream);

	const AnimationDatum* findAnimation(const std::string& name);
	int getAnimationIndex(const std::string& name);

	AnimationNodeData* findNode(const std::string& name); // Returns NULL if not found
	int getNodeIndex(const std::string& name); // Returns -1 if not found

	void loadAndRetargetAnim(InStream& stream);

	Vec4f getNodePositionModelSpace(const std::string& name, bool use_retarget_adjustment);

	size_t getTotalMemUsage() const;
	
	std::vector<AnimationNodeData> nodes;
	std::vector<int> sorted_nodes; // Indices of nodes, sorted such that children always come after parents.

	std::vector<int> joint_nodes; // Indices into nodes array.  Joint vertex values index into this array, or rather index into matrices with the same ordering.  Skin data.

	std::vector<KeyFrameTimeInfo> keyframe_times; // For each input accessor index, a vector of input keyframe times, and some precomputed info about the times.

	std::vector<js::Vector<Vec4f, 16> > output_data; // For each output accessor index, a vector of translations, rotations or scales.

	std::vector<Reference<AnimationDatum> > animations;

	GLTFVRMExtensionRef vrm_data;

	bool retarget_adjustments_set;
};


struct AnimationKeyFrameLocation
{
	int i_0;
	int i_1;
	float frac;
};
