/*=====================================================================
AnimationData.cpp
-----------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "AnimationData.h"


#include "../utils/InStream.h"
#include "../utils/OutStream.h"
#include "../utils/Exception.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/ContainerUtils.h"
#include <unordered_map>


// Throws an exception if b is false.
static inline void checkProperty(bool b, const char* on_false_message)
{
	if(!b)
		throw glare::Exception(std::string(on_false_message));
}


AnimationNodeData::AnimationNodeData() : retarget_adjustment(Matrix4f::identity()) {}


void AnimationNodeData::writeToStream(OutStream& stream) const
{
	stream.writeData(inverse_bind_matrix.e, sizeof(float) * 16);
	stream.writeData(trans.x, sizeof(float) * 4);
	stream.writeData(rot.v.x, sizeof(float) * 4);
	stream.writeData(scale.x, sizeof(float) * 4);
	stream.writeStringLengthFirst(name);
	stream.writeInt32(parent_index);
}


void AnimationNodeData::readFromStream(InStream& stream)
{
	stream.readData(inverse_bind_matrix.e, sizeof(float) * 16);
	stream.readData(trans.x, sizeof(float) * 4);
	stream.readData(rot.v.x, sizeof(float) * 4);
	stream.readData(scale.x, sizeof(float) * 4);
	name = stream.readStringLengthFirst(10000);
	parent_index = stream.readInt32();
}


void PerAnimationNodeData::writeToStream(OutStream& stream) const
{
	stream.writeInt32(translation_input_accessor);
	stream.writeInt32(translation_output_accessor);
	stream.writeInt32(rotation_input_accessor);
	stream.writeInt32(rotation_output_accessor);
	stream.writeInt32(scale_input_accessor);
	stream.writeInt32(scale_output_accessor);
}


void PerAnimationNodeData::readFromStream(InStream& stream)
{
	translation_input_accessor	= stream.readInt32();
	translation_output_accessor	= stream.readInt32();
	rotation_input_accessor		= stream.readInt32();
	rotation_output_accessor	= stream.readInt32();
	scale_input_accessor		= stream.readInt32();
	scale_output_accessor		= stream.readInt32();
}


void AnimationDatum::writeToStream(OutStream& stream) const
{
	stream.writeStringLengthFirst(name);

	// Write per_anim_node_data
	stream.writeUInt32((uint32)per_anim_node_data.size());
	for(size_t i=0; i<per_anim_node_data.size(); ++i)
		per_anim_node_data[i].writeToStream(stream);
}


void AnimationDatum::readFromStream(uint32 file_version, InStream& stream, std::vector<std::vector<float> >& old_keyframe_times_out, std::vector<js::Vector<Vec4f, 16> >& old_output_data)
{
	name = stream.readStringLengthFirst(10000);

	// Read per_anim_node_data
	{
		const uint32 num = stream.readUInt32();
		if(num > 100000)
			throw glare::Exception("invalid num");
		per_anim_node_data.resize(num);
		for(size_t i=0; i<per_anim_node_data.size(); ++i)
			per_anim_node_data[i].readFromStream(stream);
	}

	// Read keyframe_times
	if(file_version == 1)
	{
		const uint32 num = stream.readUInt32();
		if(num > 100000)
			throw glare::Exception("invalid num");
		old_keyframe_times_out.resize(num);
		for(size_t i=0; i<old_keyframe_times_out.size(); ++i)
		{
			std::vector<float>& vec = old_keyframe_times_out[i];
			const uint32 vec_size = stream.readUInt32();
			if(vec_size > 100000)
				throw glare::Exception("invalid vec_size");
			vec.resize(vec_size);
			stream.readData(vec.data(), vec_size * sizeof(float));
		}
	}

	// Read output_data
	if(file_version == 1)
	{
		const uint32 num = stream.readUInt32();
		if(num > 100000)
			throw glare::Exception("invalid num");
		old_output_data.resize(num);
		for(size_t i=0; i<old_output_data.size(); ++i)
		{
			js::Vector<Vec4f, 16>& vec = old_output_data[i];
			const uint32 vec_size = stream.readUInt32();
			if(vec_size > 100000)
				throw glare::Exception("invalid vec_size");
			vec.resize(vec_size);
			stream.readData(vec.data(), vec_size * sizeof(Vec4f));
		}
	}
}


void AnimationDatum::checkData(const std::vector<std::vector<float> >& keyframe_times, const std::vector<js::Vector<Vec4f, 16> >& output_data) const
{
	// Bounds-check data
	for(size_t i=0; i<per_anim_node_data.size(); ++i)
	{
		// -1 is a valid value
		const PerAnimationNodeData& data = per_anim_node_data[i];
		checkProperty(data.translation_input_accessor  >= -1 && data.translation_input_accessor   < (int)keyframe_times.size(), "invalid input accessor index");
		checkProperty(data.rotation_input_accessor     >= -1 && data.rotation_input_accessor      < (int)keyframe_times.size(), "invalid input accessor index");
		checkProperty(data.scale_input_accessor        >= -1 && data.scale_input_accessor         < (int)keyframe_times.size(), "invalid input accessor index");

		checkProperty(data.translation_output_accessor >= -1 && data.translation_output_accessor  < (int)output_data.size(), "invalid output accessor index");
		checkProperty(data.rotation_output_accessor    >= -1 && data.rotation_output_accessor     < (int)output_data.size(), "invalid output accessor index");
		checkProperty(data.scale_output_accessor       >= -1 && data.scale_output_accessor        < (int)output_data.size(), "invalid output accessor index");

		if(data.translation_input_accessor >= 0) checkProperty(keyframe_times[data.translation_input_accessor].size() >= 1, "invalid num keyframes");
		if(data.rotation_input_accessor    >= 0) checkProperty(keyframe_times[data.rotation_input_accessor   ].size() >= 1, "invalid num keyframes");
		if(data.scale_input_accessor       >= 0) checkProperty(keyframe_times[data.scale_input_accessor      ].size() >= 1, "invalid num keyframes");

		// Output data vectors should be the same size as the input keyframe vectors, when they are used together.
		if(data.translation_input_accessor >= 0) checkProperty(keyframe_times[data.translation_input_accessor].size() == output_data[data.translation_output_accessor].size(), "num keyframes != output_data size");
		if(data.rotation_input_accessor    >= 0) checkProperty(keyframe_times[data.rotation_input_accessor   ].size() == output_data[data.rotation_output_accessor   ].size(), "num keyframes != output_data size");
		if(data.scale_input_accessor       >= 0) checkProperty(keyframe_times[data.scale_input_accessor      ].size() == output_data[data.scale_output_accessor      ].size(), "num keyframes != output_data size");
	}
}


static const uint32 ANIMATION_DATA_VERSION = 3;
// Version 3: added serialisation of vrm_data if present


void AnimationData::writeToStream(OutStream& stream) const
{
	stream.writeUInt32(ANIMATION_DATA_VERSION);

	stream.writeData(skeleton_root_transform.e, sizeof(float)*16);

	// Write nodes
	stream.writeUInt32((uint32)nodes.size());
	for(size_t i=0; i<nodes.size(); ++i)
		nodes[i].writeToStream(stream);

	// Write sorted_nodes
	stream.writeUInt32((uint32)sorted_nodes.size());
	stream.writeData(sorted_nodes.data(), sizeof(int) * sorted_nodes.size());

	// Write joint_nodes
	stream.writeUInt32((uint32)joint_nodes.size());
	stream.writeData(joint_nodes.data(), sizeof(int) * joint_nodes.size());

	// Write keyframe_times
	stream.writeUInt32((uint32)keyframe_times.size());
	for(size_t i=0; i<keyframe_times.size(); ++i)
	{
		const std::vector<float>& vec = keyframe_times[i];
		stream.writeUInt32((uint32)vec.size());
		stream.writeData(vec.data(), sizeof(float) * vec.size());
	}

	// Write output_data
	stream.writeUInt32((uint32)output_data.size());
	for(size_t i=0; i<output_data.size(); ++i)
	{
		const js::Vector<Vec4f, 16>& vec = output_data[i];
		stream.writeUInt32((uint32)vec.size());
		stream.writeData(vec.data(), sizeof(Vec4f) * vec.size());
	}

	// Write animations
	stream.writeUInt32((uint32)animations.size());
	for(size_t i=0; i<animations.size(); ++i)
		animations[i]->writeToStream(stream);

	const uint32 vrm_data_present = vrm_data.nonNull() ? 1 : 0;
	stream.writeUInt32(vrm_data_present);

	if(vrm_data.nonNull())
	{
		stream.writeUInt32((uint32)vrm_data->human_bones.size());
		for(auto it = vrm_data->human_bones.begin(); it != vrm_data->human_bones.end(); ++it)
		{
			stream.writeStringLengthFirst(it->first); // Write bone name
			stream.writeInt32(it->second.node_index); // Write node_index
		}
	}
}


void AnimationData::readFromStream(InStream& stream)
{
	const uint32 version = stream.readUInt32();
	if(version > ANIMATION_DATA_VERSION)
		throw glare::Exception("Invalid animation data version: " + toString(version));

	stream.readData(skeleton_root_transform.e, sizeof(float)*16);

	// Read nodes
	{
		const uint32 num = stream.readUInt32();
		if(num > 100000)
			throw glare::Exception("invalid num");
		nodes.resize(num);
		for(size_t i=0; i<nodes.size(); ++i)
			nodes[i].readFromStream(stream);
	}

	// Read sorted_nodes
	{
		const uint32 num = stream.readUInt32();
		if(num > 100000)
			throw glare::Exception("invalid num");
		sorted_nodes.resize(num);
		stream.readData(sorted_nodes.data(), sizeof(int) * sorted_nodes.size());
	}

	// Read joint_nodes
	{
		const uint32 num = stream.readUInt32();
		if(num > 100000)
			throw glare::Exception("invalid num");
		joint_nodes.resize(num);
		stream.readData(joint_nodes.data(), sizeof(int) * joint_nodes.size());
	}

	// Read keyframe_times
	if(version >= 2)
	{
		const uint32 num = stream.readUInt32();
		if(num > 100000)
			throw glare::Exception("invalid num");
		keyframe_times.resize(num);
		for(size_t i=0; i<keyframe_times.size(); ++i)
		{
			std::vector<float>& vec = keyframe_times[i];
			const uint32 vec_size = stream.readUInt32();
			if(vec_size > 100000)
				throw glare::Exception("invalid vec_size");
			vec.resize(vec_size);
			stream.readData(vec.data(), vec_size * sizeof(float));
		}
	}

	// Read output_data
	if(version >= 2)
	{
		const uint32 num = stream.readUInt32();
		if(num > 100000)
			throw glare::Exception("invalid num");
		output_data.resize(num);
		for(size_t i=0; i<output_data.size(); ++i)
		{
			js::Vector<Vec4f, 16>& vec = output_data[i];
			const uint32 vec_size = stream.readUInt32();
			if(vec_size > 100000)
				throw glare::Exception("invalid vec_size");
			vec.resize(vec_size);
			stream.readData(vec.data(), vec_size * sizeof(Vec4f));
		}
	}

	// Read animations
	{
		const uint32 num = stream.readUInt32();
		if(num > 100000)
			throw glare::Exception("invalid num");
		animations.resize(num);
		for(size_t i=0; i<animations.size(); ++i)
		{
			animations[i] = new AnimationDatum();

			std::vector<std::vector<float> > old_keyframe_times;
			std::vector<js::Vector<Vec4f, 16> > old_output_data;

			animations[i]->readFromStream(version, stream, old_keyframe_times, old_output_data);

			// Do backwwards-compat handling
			if(version == 1)
			{
				const int keyframe_times_offset = (int)keyframe_times.size();
				const int output_data_offset = (int)output_data.size();
				ContainerUtils::append(keyframe_times, old_keyframe_times);
				ContainerUtils::append(output_data, old_output_data);

				// Offset accessors
				for(size_t z=0; z<animations[i]->per_anim_node_data.size(); ++z)
				{
					PerAnimationNodeData& data = animations[i]->per_anim_node_data[z];
					if(data.translation_input_accessor != -1)	data.translation_input_accessor		+= keyframe_times_offset;
					if(data.rotation_input_accessor != -1)		data.rotation_input_accessor		+= keyframe_times_offset;
					if(data.scale_input_accessor != -1)			data.scale_input_accessor			+= keyframe_times_offset;
					if(data.translation_output_accessor != -1)	data.translation_output_accessor	+= output_data_offset;
					if(data.rotation_output_accessor != -1)		data.rotation_output_accessor		+= output_data_offset;
					if(data.scale_output_accessor != -1)		data.scale_output_accessor			+= output_data_offset;
				}
			}

			animations[i]->checkData(keyframe_times, output_data);
		}
	}

	// Read VRM data, if present
	if(version >= 3)
	{
		const uint32 vrm_data_present = stream.readUInt32();
		if(vrm_data_present == 1)
		{
			vrm_data = new GLTFVRMExtension();

			const uint32 human_bones_size = stream.readUInt32();
			for(size_t i=0; i<human_bones_size; ++i)
			{
				const std::string bone_name = stream.readStringLengthFirst(10000);
				
				VRMBoneInfo bone_info;
				bone_info.node_index = stream.readInt32(); // Not bounds-checked on loading, bounds checked when used.

				vrm_data->human_bones.insert(std::make_pair(bone_name, bone_info));
			}
		}
	}

	

	// Bounds-check data
	for(size_t i=0; i<sorted_nodes.size(); ++i)
		checkProperty(sorted_nodes[i] >= 0 && sorted_nodes[i] < (int)nodes.size(), "invalid sorted_nodes index");

	for(size_t i=0; i<joint_nodes.size(); ++i)
		checkProperty(joint_nodes[i] >= 0 && joint_nodes[i] < (int)nodes.size(), "invalid joint_nodes index");

	for(size_t i=0; i<nodes.size(); ++i)
		checkProperty(nodes[i].parent_index >= -1 && nodes[i].parent_index < (int)nodes.size(), "invalid parent_index index"); // parent_index of -1 is valid.
}


const AnimationDatum* AnimationData::findAnimation(const std::string& name)
{
	for(size_t i=0; i<animations.size(); ++i)
		if(animations[i]->name == name)
			return animations[i].ptr();
	return NULL;
}


int AnimationData::getAnimationIndex(const std::string& name)
{
	for(size_t i=0; i<animations.size(); ++i)
		if(animations[i]->name == name)
			return (int)i;
	return -1;
}


float AnimationData::getAnimationLength(const AnimationDatum& anim) const
{
	float max_len = 0;
	for(size_t i=0; i<anim.per_anim_node_data.size(); ++i)
	{
		if(anim.per_anim_node_data[i].rotation_input_accessor >= 0)
		{
			max_len = myMax(max_len, this->keyframe_times[anim.per_anim_node_data[i].rotation_input_accessor].back());
		}
	}

	return max_len;
}


AnimationNodeData* AnimationData::findNode(const std::string& name) // Returns NULL if not found
{
	for(size_t i=0; i<nodes.size(); ++i)
		if(nodes[i].name == name)
			return &nodes[i];
	return NULL;
}


int AnimationData::getNodeIndex(const std::string& name) // Returns -1 if not found
{
	for(size_t i=0; i<nodes.size(); ++i)
		if(nodes[i].name == name)
			return (int)i;
	return -1;
}


void AnimationData::loadAndRetargetAnim(InStream& stream)
{
	const bool VERBOSE = true;

	const char* VRM_to_RPM_name_data[] = {
		// From vroidhub_avatarsample_A.vrm or testguy.vrm
		"J_Bip_L_Index3_end", "LeftHandIndex4",
		"J_Bip_L_Little3_end", "LeftHandPinky4",
		"J_Bip_L_Middle3_end", "LeftHandMiddle4",
		"J_Bip_L_Ring3_end", "LeftHandRing4",

		"J_Bip_R_Index3_end",  "RightHandIndex4",
		"J_Bip_R_Little3_end", "RightHandPinky4",
		"J_Bip_R_Middle3_end", "RightHandMiddle4",
		"J_Bip_R_Ring3_end",   "RightHandRing4",

		"J_Bip_L_Little2", "LeftHandPinky2",
		"J_Bip_L_Thumb3_end", "LeftHandThumb4",
		"J_Bip_L_Thumb1", "LeftHandThumb1",

		"J_Bip_R_Little2", "RightHandPinky2",
		"J_Bip_R_Thumb3_end", "RightHandThumb4",
		"J_Bip_R_Thumb1", "RightHandThumb1",

		// From PolygonApe_97.vrm
		"LittleFinger1_L",		"LeftHandPinky1",
		"LittleFinger2_L",		"LeftHandPinky2",
		"LittleFinger3_L",		"LeftHandPinky3",
		"LittleFinger3_L_end",	"LeftHandPinky4",

		"RingFinger1_L",		"LeftHandRing1",
		"RingFinger2_L",		"LeftHandRing2",
		"RingFinger3_L",		"LeftHandRing3",
		"RingFinger3_L_end",	"LeftHandRing4",

		"MiddleFinger1_L",		"LeftHandMiddle1",
		"MiddleFinger2_L",		"LeftHandMiddle2",
		"MiddleFinger3_L",		"LeftHandMiddle3",
		"MiddleFinger3_L_end",	"LeftHandMiddle4",
		
		"IndexFinger1_L",		"LeftHandIndex1",
		"IndexFinger2_L",		"LeftHandIndex2",
		"IndexFinger3_L",		"LeftHandIndex3",
		"IndexFinger3_L_end",	"LeftHandIndex4",

		"Thumb0_R",		"LeftHandThumb1", // NOTE: these mappings correct?
		"Thumb1_R",		"LeftHandThumb2",
		"Thumb2_R",		"LeftHandThumb3",
		"Thumb2_R_end",	"LeftHandThumb4",

		"LittleFinger1_R",		"RightHandPinky1",
		"LittleFinger2_R",		"RightHandPinky2",
		"LittleFinger3_R",		"RightHandPinky3",
		"LittleFinger3_R_end",	"RightHandPinky4",

		"RingFinger1_R",		"RightHandRing1",
		"RingFinger2_R",		"RightHandRing2",
		"RingFinger3_R",		"RightHandRing3",
		"RingFinger3_R_end",	"RightHandRing4",

		"MiddleFinger1_R",		"RightHandMiddle1",
		"MiddleFinger2_R",		"RightHandMiddle2",
		"MiddleFinger3_R",		"RightHandMiddle3",
		"MiddleFinger3_R_end",	"RightHandMiddle4",

		"IndexFinger1_R",		"RightHandIndex1",
		"IndexFinger2_R",		"RightHandIndex2",
		"IndexFinger3_R",		"RightHandIndex3",
		"IndexFinger3_R_end",	"RightHandIndex4",

		"Thumb0_R",		"RightHandThumb1", // NOTE: these mappings correct?
		"Thumb1_R",		"RightHandThumb2",
		"Thumb2_R",		"RightHandThumb3",
		"Thumb2_R_end",	"RightHandThumb4",

		"J_Bip_L_ToeBase_end",	"LeftToe_End",
		"J_Bip_R_ToeBase_end",	"RightToe_End",
	};
	static_assert(staticArrayNumElems(VRM_to_RPM_name_data) % 2 == 0, "staticArrayNumElems(VRM_to_RPM_name_data) % 2 == 0");

	std::map<std::string, std::string> VRM_to_RPM_names;

	for(int z=0; z<staticArrayNumElems(VRM_to_RPM_name_data); z += 2)
		VRM_to_RPM_names.insert(std::make_pair(VRM_to_RPM_name_data[z], VRM_to_RPM_name_data[z + 1]));


	//----------- Make mappings like Mixamorig_RightHandPinky2 -> RightHandPinky2
	const char* finger_VRM_names[] = {
		"HandPinky1",
		"HandPinky2",
		"HandPinky3",
		"HandPinky4",

		"HandRing1",
		"HandRing2",
		"HandRing3",
		"HandRing4",

		"HandMiddle1",
		"HandMiddle2",
		"HandMiddle3",
		"HandMiddle4",

		"HandIndex1",
		"HandIndex2",
		"HandIndex3",
		"HandIndex4",

		"HandThumb1",
		"HandThumb2",
		"HandThumb3",
		"HandThumb4"
	};

	for(int z=0; z<staticArrayNumElems(finger_VRM_names); ++z)
	{
		const std::string left_hand_name  = std::string("Left")  + finger_VRM_names[z];
		const std::string right_hand_name = std::string("Right") + finger_VRM_names[z];

		VRM_to_RPM_names.insert(std::make_pair("Mixamorig_" + left_hand_name, left_hand_name));
		VRM_to_RPM_names.insert(std::make_pair("Mixamorig_" + right_hand_name, right_hand_name));
	}


	const char* RPM_bone_names[] = {
		"Hips",
		"Spine",
		"Spine1",
		"Spine2",
		"Neck",
		"Head",
		"HeadTop_End",
		"LeftEye",
		"RightEye",
		"LeftShoulder",
		"LeftArm",
		"LeftForeArm",
		"LeftHand",
		"LeftHandThumb1",
		"LeftHandThumb2",
		"LeftHandThumb3",
		"LeftHandThumb4",
		"LeftHandIndex1",
		"LeftHandIndex2",
		"LeftHandIndex3",
		"LeftHandIndex4",
		"LeftHandMiddle1",
		"LeftHandMiddle2",
		"LeftHandMiddle3",
		"LeftHandMiddle4",
		"LeftHandRing1",
		"LeftHandRing2",
		"LeftHandRing3",
		"LeftHandRing4",
		"LeftHandPinky1",
		"LeftHandPinky2",
		"LeftHandPinky3",
		"LeftHandPinky4",
		"RightShoulder",
		"RightArm",
		"RightForeArm",
		"RightHand",
		"RightHandThumb1",
		"RightHandThumb2",
		"RightHandThumb3",
		"RightHandThumb4",
		"RightHandRing1",
		"RightHandRing2",
		"RightHandRing3",
		"RightHandRing4",
		"RightHandMiddle1",
		"RightHandMiddle2",
		"RightHandMiddle3",
		"RightHandMiddle4",
		"RightHandIndex1",
		"RightHandIndex2",
		"RightHandIndex3",
		"RightHandIndex4",
		"RightHandPinky1",
		"RightHandPinky2",
		"RightHandPinky3",
		"RightHandPinky4",
		"LeftUpLeg",
		"LeftLeg",
		"LeftFoot",
		"LeftToeBase",
		"LeftToe_End",
		"RightUpLeg",
		"RightLeg",
		"RightFoot",
		"RightToeBase",
		"RightToe_End"
	};

	const char* VRM_bone_names[] = {
		"hips",
		"spine",
		"chest",
		"upperChest",
		"neck",
		"head",
		"",
		"leftEye",
		"rightEye",
		"leftShoulder",
		"leftUpperArm",
		"leftLowerArm",
		"leftHand",
		"leftThumbProximal",
		"leftThumbIntermediate",
		"leftThumbDistal",
		"",
		"leftIndexProximal",
		"leftIndexIntermediate",
		"leftIndexDistal",
		"",
		"leftMiddleProximal",
		"leftMiddleIntermediate",
		"leftMiddleDistal",
		"",
		"leftRingProximal",
		"leftRingIntermediate",
		"leftRingDistal",
		"",
		"leftLittleProximal",
		"leftLittleIntermediate",
		"leftLittleDistal",
		"",
		"rightShoulder",
		"rightUpperArm",
		"rightLowerArm",
		"rightHand",
		"rightThumbProximal",
		"rightThumbIntermediate",
		"rightThumbDistal",
		"",
		"rightRingProximal",
		"rightRingIntermediate",
		"rightRingDistal",
		"",
		"rightMiddleProximal",
		"rightMiddleIntermediate",
		"rightMiddleDistal",
		"",
		"rightIndexProximal",
		"rightIndexIntermediate",
		"rightIndexDistal",
		"",
		"rightLittleProximal",
		"rightLittleProximal",
		"rightLittleDistal",
		"",
		"leftUpperLeg",
		"leftLowerLeg",
		"leftFoot",
		"leftToes",
		"",
		"rightUpperLeg",
		"rightLowerLeg",
		"rightFoot",
		"rightToes",
		""
	};

	static_assert(staticArrayNumElems(RPM_bone_names) == staticArrayNumElems(VRM_bone_names), "staticArrayNumElems(RPM_bone_names) == staticArrayNumElems(VRM_bone_names)");


	// Make map from RPM_bone_name to index
	std::map<std::string, int> RPM_bone_name_to_index;
	for(int z=0; z<staticArrayNumElems(RPM_bone_names); ++z)
		RPM_bone_name_to_index.insert(std::make_pair(RPM_bone_names[z], z));
	

	const std::vector<AnimationNodeData> old_nodes = nodes; // Copy old node data
	const std::vector<int> old_joint_nodes = joint_nodes;
	const std::vector<int> old_sorted_nodes = sorted_nodes;

	GLTFVRMExtensionRef old_vrm_data = vrm_data;

	this->readFromStream(stream);

	printVar(old_joint_nodes == joint_nodes);
	printVar(old_sorted_nodes == sorted_nodes);


	std::unordered_map<std::string, int> new_bone_names_to_index;
	for(size_t z=0; z<nodes.size(); ++z)
		new_bone_names_to_index.insert(std::make_pair(nodes[z].name, (int)z));

	std::unordered_map<std::string, int> old_node_names_to_index;
	for(size_t z=0; z<old_nodes.size(); ++z)
		old_node_names_to_index.insert(std::make_pair(old_nodes[z].name, (int)z));


	// Build mapping from old node index to new node index

	// Add to mappings based on VRM metadata
	std::map<int, int> old_to_new_node_index;
	std::map<int, int> new_to_old_node_index;
	for(size_t i=0; i<nodes.size(); ++i) // For each new node (node from GLB animation data):
	{
		AnimationNodeData& new_node = nodes[i];

		int old_node_index = -1;
		if(old_vrm_data.nonNull())
		{
			// We have the new node name (RPM name).
			// Find corresponding VRM bone name.
			// Then look up in map from VRM bone name to bone index.
			auto res = RPM_bone_name_to_index.find(new_node.name); // Get index of this bone name in list of RPM bone names
			if(res != RPM_bone_name_to_index.end())
			{
				const std::string VRM_name = VRM_bone_names[res->second]; // Get corresponding VRM bone name

				// Lookup in our VRM metadata to get the node index for that bone name.
				auto res2 = old_vrm_data->human_bones.find(VRM_name);
				if(res2 != old_vrm_data->human_bones.end())
				{
					const int bone_index = res2->second.node_index;
					// Check bone_index is in bounds
					if(bone_index < 0 || bone_index >= (int)old_nodes.size())
						throw glare::Exception("VRM node_index is out of bounds.");
					if(VERBOSE) conPrint("Mapping new node " + new_node.name + " to old node " + old_nodes[bone_index].name + " via VRM metadata");
					old_node_index = bone_index;
				}
			}
		}
		else
		{
			auto res = old_node_names_to_index.find(new_node.name);
			if(res != old_node_names_to_index.end())
			{
				old_node_index = res->second;
			}
		}

		if(old_node_index != -1)
		{
			new_to_old_node_index[(int)i] = old_node_index;
			old_to_new_node_index[old_node_index] = (int)i;
		}
	}



	// Map old nodes to new nodes based on our VRM_to_RPM_names map.
	for(size_t i=0; i<old_nodes.size(); ++i) // For each old node (node from VRM data):
	{
		if(old_to_new_node_index.count((int)i) == 0) // If no mapping to new node yet:
		{
			const AnimationNodeData& old_node = old_nodes[i];
			auto res = VRM_to_RPM_names.find(old_node.name);
			if(res != VRM_to_RPM_names.end())
			{
				const std::string RPM_name = res->second;

				// Find new node with such name
				auto RPM_res = new_bone_names_to_index.find(RPM_name);
				if(RPM_res != new_bone_names_to_index.end())
				{
					const int new_i = RPM_res->second;

					if(VERBOSE) conPrint("----------------Found node match via VRM_to_RPM_names map.----------------");
					if(VERBOSE) conPrint("old node: " + old_nodes[i].name);
					if(VERBOSE) conPrint("new node: " + nodes[new_i].name);

					old_to_new_node_index[(int)i] = new_i;
					new_to_old_node_index[new_i] = (int)i;
				}
			}
		}
	}


	// For old nodes for which we couldn't find a mapping to new nodes, just create a new node for the old node.
	// This will cover nodes such as the hair nodes in VRM models, which don't have corresponding nodes in RPM files.
	// TODO: make sure the sorted_nodes order is maintained/correct.

	const size_t initial_num_new_nodes = nodes.size();
	for(size_t i=0; i<old_nodes.size(); ++i) // For each old node (node from VRM data):
	{
		if(old_to_new_node_index.count((int)i) == 0) // If no mapping to new node yet:
		{
			const AnimationNodeData& old_node = old_nodes[i];

			// If the old node has a parent, and the parent has a corresponding new node, use that new node as the parent
			if(old_to_new_node_index.count(old_node.parent_index) > 0)
			{
				const int new_parent_i = old_to_new_node_index[old_node.parent_index];

				const bool new_parent_is_new = new_parent_i >= initial_num_new_nodes;

				const int new_i = (int)nodes.size();
				nodes.push_back(AnimationNodeData());
				nodes[new_i] = old_node;
				nodes[new_i].parent_index = new_parent_i;

				assert(new_parent_i < new_i);
				this->sorted_nodes.push_back(new_i);

				for(int z=0; z<this->animations.size(); ++z)
				{
					this->animations[z]->per_anim_node_data.push_back(PerAnimationNodeData());
					this->animations[z]->per_anim_node_data.back().translation_input_accessor = -1;
					this->animations[z]->per_anim_node_data.back().translation_output_accessor = -1;
					this->animations[z]->per_anim_node_data.back().rotation_input_accessor = -1;
					this->animations[z]->per_anim_node_data.back().rotation_output_accessor = -1;
					this->animations[z]->per_anim_node_data.back().scale_input_accessor = -1;
					this->animations[z]->per_anim_node_data.back().scale_output_accessor = -1;
				}
			
				/*
				old parent                     new parent
					
				^       trans                        trans
				|      ^                           ^
				|    /                           /
				|  /                           /
				|/                           /
				-------------->              -------------->
				                            |
				                            |
				                            |
				                            |
				                            v
					
				We want trans applied in new parent space to have the same effect (in model space) as trans in old parent space.
				So do this, first apply trans in new old parent space, then transform to model space with old_parent_to_model, then 
				to new parent space with new_parent_to_child (e.g. new_parent.inverse_bind_matrix)
				*/

				const AnimationNodeData& old_parent = old_nodes[old_node.parent_index];
				const AnimationNodeData& new_parent = nodes[new_parent_i];

				Matrix4f old_parent_to_model;
				old_parent.inverse_bind_matrix.getInverseForAffine3Matrix(old_parent_to_model);
				old_parent_to_model.setColumn(3, Vec4f(0,0,0,1));


				Matrix4f new_parent_to_child = new_parent.inverse_bind_matrix;
				new_parent_to_child.setColumn(3, Vec4f(0,0,0,1));

				nodes[new_i].retarget_adjustment = new_parent_to_child * old_parent_to_model;

				if(new_parent_is_new)
					nodes[new_i].retarget_adjustment = Matrix4f::identity(); // If this is a newly created node, and the parent is also a newly created node, it shouldn't need any adjustment.


				if(VERBOSE) conPrint("----------------Created new node.----------------");
				if(VERBOSE) conPrint("new node: " + nodes[new_i].name + " (parent node: " + nodes[new_parent_i].name + ")");

				old_to_new_node_index[(int)i] = new_i;
				new_to_old_node_index[new_i] = (int)i;
			}
		}
	}


	for(size_t i=0; i<nodes.size(); ++i)
	{
		AnimationNodeData& new_node = nodes[i];

		auto res = new_to_old_node_index.find((int)i);
		if(res != new_to_old_node_index.end())
		{
			const int old_node_index = res->second;
			const AnimationNodeData& old_node = old_nodes[old_node_index];

			Matrix4f retarget_adjustment = Matrix4f::identity();
			Vec4f retarget_trans(0.f);
			if(old_node.parent_index != -1 && new_node.parent_index != -1)
			{
				const bool is_new_node_new = i >= initial_num_new_nodes;
				if(!is_new_node_new)
				{
					// We want to compute a 'retargetting' translation, so that the bone lengths of the old model, with the new animation data applied (which gives relative node translations),
					// are similar in length to the bones of the old model.

					// This is a complicated by the fact that the new translations and old translations may be in different bases.

					const Vec4f old_translation_bs = old_node.trans; // Translations in parent bone space
					const Vec4f new_translation_bs = new_node.trans;


					const AnimationNodeData& old_parent = old_nodes[old_node.parent_index];
					      AnimationNodeData& new_parent = nodes[new_node.parent_index];

					// Get bone->world transform for the parent of the new node.
					Matrix4f new_parent_to_world;
					new_parent.inverse_bind_matrix.getInverseForAffine3Matrix(new_parent_to_world); // bind matrix = bone space to model/world space.  So inverse bind matrix = world to bone space.
					
					// Compute the translation for the new node in model space.
					Vec4f new_translation_model_space = new_parent_to_world * new_translation_bs;


					// Handle the case where the parents are not the same, but the parent of the old node is the grandparent of the new node.
					// For now we will only try and handle this for specific nodes, such as the Spine2 case below.
					if(old_to_new_node_index[old_node.parent_index] != new_node.parent_index)
					{
						/*
						spine       spine1      spine2      neck         GLB (new anim data)
						*------------*-----------*-----------*
						^            ^                       ^
						|            |                       |
						v            v                       v
						spine      chest                    neck         VRM (old data)
						*------------*-----------------------*
						
						*/
						if(VERBOSE) conPrint("!!!!!!!!parents don't match!  old_node: " + old_node.name + ", old_parent: " + old_parent.name + ", new_node: " + new_node.name + ", new_parent: " + new_parent.name);
						// e.g. !!!!!!!!parents don't match!  old_node: Right shoulder, old_parent: Chest, new_node: RightShoulder, new_parent: Spine2
						if(new_parent.name == "Spine2")
						{
							if(new_parent.parent_index >= 0)
							{
								const AnimationNodeData& grandparent = nodes[new_parent.parent_index]; // E.g. spine1

								Matrix4f grandparent_to_world;
								grandparent.inverse_bind_matrix.getInverseForAffine3Matrix(grandparent_to_world);

								const Vec4f parent_translation_model_space = grandparent_to_world * new_parent.trans; // e.g. translation of Spine2 node in Spine1 space.

								new_translation_model_space = new_translation_model_space + parent_translation_model_space; // Translation from spine1 to spine2, plus translation from spine2 to neck, in spine1 space.
							}
						}
					}

					Matrix4f old_parent_to_world;
					old_parent.inverse_bind_matrix.getInverseForAffine3Matrix(old_parent_to_world);

					// Compute the translation of the old node in model space.  Note that we rotate the VRM data around the y-axis to match the RPM data.
					const Vec4f old_translation_model_space = /*Matrix4f::rotationAroundYAxis(Maths::pi<float>()) **/ old_parent_to_world * old_translation_bs;

					// The retargetting vector (in model space) is now the extra translation to to make the new translation the same as the old translation
					Vec4f retarget_trans_model_space = old_translation_model_space - new_translation_model_space;

					// Transform translation back into the space of the new parent node.
					Matrix4f new_parent_to_local = new_parent.inverse_bind_matrix;

					Vec4f retarget_trans_new_parent_space = new_parent_to_local * retarget_trans_model_space;

					retarget_trans = retarget_trans_new_parent_space;
					retarget_adjustment = Matrix4f::translationMatrix(retarget_trans_new_parent_space);
				}
				else
					retarget_adjustment = new_node.retarget_adjustment; // Keep existing value
			}

			new_node.retarget_adjustment = retarget_adjustment;

			if(VERBOSE) conPrint("new node: " + new_node.name + " (" + toString(i) + "), old node: " + old_node.name + " (index " + toString(old_node_index) + "), retarget_trans: " + retarget_trans.toStringNSigFigs(4));
		}
		else
		{
			if(VERBOSE) conPrint("could not find old node for new node " + new_node.name);
		}
	}



	// Update joint_nodes, since the nodes may have been completely updated
	if(VERBOSE) conPrint("---------------------updating joint nodes.----------------------------");
	joint_nodes = old_joint_nodes;
	for(size_t i=0; i<joint_nodes.size(); ++i)
	{
		if(VERBOSE) conPrint("");
		const int old_joint_node_i = joint_nodes[i];
		const AnimationNodeData& old_node = old_nodes[old_joint_node_i];
		const std::string old_joint_node_name = old_node.name;
		if(VERBOSE) conPrint("old_joint_node_name: " + old_joint_node_name);
		auto res = old_to_new_node_index.find(old_joint_node_i);

		if(res != old_to_new_node_index.end())
		{
			joint_nodes[i] = res->second;

			AnimationNodeData& new_node = nodes[joint_nodes[i]];

			const std::string new_joint_node_name = new_node.name;
			if(VERBOSE) conPrint("new_joint_node_name: " + new_joint_node_name);

			// Our inverse bind matrix will take a point on the old mesh (VRM mesh)
			// It will transform into a basis with origin at the old bone position, but with orientation given by the new bone.
			// To find this transform, we will find the desired bind matrix (bone to model transform), then invert it.
			// This will result in an inverse bind matrix, that when concatenated with the relevant joint matrices (animated joint to model space transforms), 
			// Will transform a vertex from a point on the old mesh (VRM mesh), to bone space, and then finally to model space.
			// Note that the joint matrices expect a certain orientation of the mesh data in bone space, which is why we use the new bone orientations.

			// Take position of old node (VRM node).
			// Use orientation of new matrix
			
			Matrix4f old_bind_matrix;
			old_nodes[old_joint_node_i].inverse_bind_matrix.getInverseForAffine3Matrix(old_bind_matrix);
			const Vec4f old_node_pos = old_bind_matrix.getColumn(3);
			
			Matrix4f bind_matrix; // = bone to model-space transform
			new_node.inverse_bind_matrix.getInverseForAffine3Matrix(bind_matrix);

			// Make our desired bind matrix, then invert it to get our desired inverse bind matrix.
			bind_matrix.setColumn(3, old_node_pos);
			
			Matrix4f desired_inverse_bind_matrix;
			bind_matrix.getInverseForAffine3Matrix(desired_inverse_bind_matrix);

			new_node.inverse_bind_matrix = desired_inverse_bind_matrix;
		}
		else
		{
			conPrint("!!!!! Corresponding new node not found.");

			joint_nodes[i] = 0;
		}
	}

	this->retarget_adjustments_set = true;
}


Vec4f AnimationData::getNodePositionModelSpace(const std::string& name, bool use_retarget_adjustment)
{
	const size_t num_nodes = sorted_nodes.size();
	js::Vector<Matrix4f, 16> node_matrices(num_nodes);

	for(int n=0; n<sorted_nodes.size(); ++n)
	{
		const int node_i = sorted_nodes[n];
		const AnimationNodeData& node_data = nodes[node_i];
		
		const Matrix4f rot_mat = node_data.rot.toMatrix();

		const Matrix4f TRS(
			rot_mat.getColumn(0) * copyToAll<0>(node_data.scale),
			rot_mat.getColumn(1) * copyToAll<1>(node_data.scale),
			rot_mat.getColumn(2) * copyToAll<2>(node_data.scale),
			setWToOne(node_data.trans));

		const Matrix4f node_transform = (node_data.parent_index == -1) ? TRS : (node_matrices[node_data.parent_index] * (use_retarget_adjustment ? node_data.retarget_adjustment : Matrix4f::identity()) * TRS);

		node_matrices[node_i] = node_transform;

		if(node_data.name == name)
			return node_transform.getColumn(3);
	}

	return Vec4f(0,0,0,1);
}
