/*=====================================================================
AnimationData.h
---------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "../maths/vec3.h"
#include "../maths/Quat.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Vector.h"
#include <string>
#include <vector>
#include <map>
class InStream;
class RandomAccessInStream;
class OutStream;


// Animation information for a node, common to all animations.
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


// Animation information for a node, for a particular animation.
struct PerAnimationNodeData
{
	void init() // Set all accessors to -1.
	{
		translation_input_accessor	= -1;
		translation_output_accessor	= -1;
		rotation_input_accessor		= -1;
		rotation_output_accessor	= -1;
		scale_input_accessor		= -1;
		scale_output_accessor		= -1;
	}

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


// Data that can be loaded once and shared among multiple different meshes.  Will not be changed by retargetting.
struct AnimationDatum : public ThreadSafeRefCounted
{
	AnimationDatum() : anim_len(-1) {}

	void writeToStream(OutStream& stream) const;
	void readFromStream(uint32 file_version, InStream& stream, js::Vector<KeyFrameTimeInfo>& old_keyframe_times_out, js::Vector<js::Vector<Vec4f, 16> >& old_output_data);

	void checkData(const js::Vector<KeyFrameTimeInfo>& keyframe_times, const js::Vector<js::Vector<Vec4f, 16> >& output_data) const;

	size_t getTotalMemUsage() const;

	std::string name;

	std::vector<PerAnimationNodeData> raw_per_anim_node_data; // non-retargetted.

	std::vector<int> used_input_accessor_indices;

	// These can be empty, in which case the base AnimationData::keyframe_times and outout_data are used.  They should be non-empty for AnimationDatum objects shared by multiple avatars, that need to do retargetting.
	js::Vector<KeyFrameTimeInfo> m_keyframe_times; // For each input accessor index, a vector of input keyframe times, and some precomputed info about the times.
	js::Vector<js::Vector<Vec4f, 16> > m_output_data; // For each output accessor index, a vector of translations, rotations or scales.

	float anim_len;
};


struct VRMBoneInfo
{
	int node_index;
};

struct GLTFVRMExtension : public ThreadSafeRefCounted
{
	GLTFVRMExtension() {}

	std::map<std::string, VRMBoneInfo> human_bones;
};
typedef Reference<GLTFVRMExtension> GLTFVRMExtensionRef;


/*
AnimationData
-------------
A list of animations, as well as some information shared by all the referenced animations, such as default node transformations and
retargetting information.

AnimationDatums can be referenced by multiple different AnimationData objects.


Example object diagram:

  -------------
  |  gl ob 0  |                                   --------------------------------
  |           |------\     -----------------     /|  AnimationDatum 0  ("Idle")  |
  -------------       \--- |   mesh data 0 |    / --------------------------------
                           |               |   / 
                      /----|               |  /  ----------------------------------
  -------------      /     |   anim_data   |-----|  AnimationDatum  1 ("Walking") |
  |  gl ob 1  |------      -----------------     ----------------------------------
  |           |                              \
  -------------                               \  ----------------------------------
                                             --- |  AnimationDatum 2  ("Running") |
                                            /    ----------------------------------
                                           /                 ^
                        ----------------- /            AnimationDatum
  -------------         |   mesh data 1 |/
  |  gl ob 2  |---------|               |
  |           |         |               |
  -------------         |   anim_data   |
      ^                 -----------------
   GLObject                     ^ 
                      OpenGLMeshRenderData
                        

*/
struct AnimationData : public ThreadSafeRefCounted
{
	AnimationData() : retarget_adjustments_set(false) {}

	void operator = (const AnimationData& other);

	void checkDataIsValid() const; // Check indices are in-bounds etc.  Throws glare::Exception if not.

	void build(); // Builds keyframe_times data, computes animation length, builds per_anim_node_data.

	void checkPerAnimNodeDataIsValid() const; // Should only be called after build().

	void writeToStream(OutStream& stream) const;
	void readFromStream(RandomAccessInStream& stream);

	// Copy keyframe_times and output_data from the AnimationData object to the AnimationDatum object, so it can be used by multiple different avatars
	void prepareForMultipleUse();

	void appendAnimationData(const AnimationData& other);

	void removeInverseBindMatrixScaling();

	void removeUnneededOutputData();

	void printStats();

	const AnimationDatum* findAnimation(const std::string& name);
	int getAnimationIndex(const std::string& name);

	AnimationNodeData* findNode(const std::string& name); // Returns NULL if not found
	int getNodeIndex(const std::string& name) const; // Returns -1 if not found
	int getNodeIndexWithNameSuffix(const std::string& name_suffix); // Returns -1 if not found

	void loadAndRetargetAnim(const AnimationData& other);

	Vec4f getNodePositionModelSpace(int node_index, bool use_retarget_adjustment) const;
	Vec4f getNodePositionModelSpace(const std::string& name, bool use_retarget_adjustment) const;

	size_t getTotalMemUsage() const;

	static void test();
private:
	Matrix4f getNodeToObjectSpaceTransform(int node_index, bool use_retarget_adjustment) const;
	Vec4f transformPointToNodeParentSpace(int node_index, bool use_retarget_adjustment, const Vec4f& point_node_space) const;
public:
	
	// NOTE: update operator = if changing fields.
	std::vector<AnimationNodeData> nodes;
	std::vector<int> sorted_nodes; // Indices of nodes, sorted such that children always come after parents.

	std::vector<int> joint_nodes; // Indices into nodes array.  Joint vertex values index into this array, or rather index into matrices with the same ordering.  Skin data.

	js::Vector<KeyFrameTimeInfo> keyframe_times; // For each input accessor index, a vector of input keyframe times, and some precomputed info about the times.

	js::Vector<js::Vector<Vec4f, 16> > output_data; // For each output accessor index, a vector of translations, rotations or scales.

	std::vector<Reference<AnimationDatum> > animations;

	std::vector<std::vector<PerAnimationNodeData>> per_anim_node_data; // One vector of PerAnimationNodeData per animation.  Ephemeral data, built in build()

	GLTFVRMExtensionRef vrm_data;

	bool retarget_adjustments_set; // A single AnimationData can be referenced by multiple avatars with the same glmeshdata.  Use this to make sure we only do retargetting once for the animation data. 
};


struct AnimationKeyFrameLocation
{
	int i_0;
	int i_1;
	float frac;
};
