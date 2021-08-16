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
}


void AnimationDatum::readFromStream(InStream& stream)
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
}


static const int ANIMATION_DATA_VERSION = 1;


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

	// Write animations
	stream.writeUInt32((uint32)animations.size());
	for(size_t i=0; i<animations.size(); ++i)
		animations[i]->writeToStream(stream);
}


void AnimationData::readFromStream(InStream& stream)
{
	const uint32 version = stream.readUInt32();
	if(version != ANIMATION_DATA_VERSION)
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

	// Read animations
	{
		const uint32 num = stream.readUInt32();
		if(num > 100000)
			throw glare::Exception("invalid num");
		animations.resize(num);
		for(size_t i=0; i<animations.size(); ++i)
		{
			animations[i] = new AnimationDatum();
			animations[i]->readFromStream(stream);
		}
	}
}
