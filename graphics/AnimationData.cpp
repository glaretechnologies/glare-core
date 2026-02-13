/*=====================================================================
AnimationData.cpp
-----------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "AnimationData.h"


#include "../meshoptimizer/src/meshoptimizer.h"
#include "../utils/InStream.h"
#include "../utils/RandomAccessInStream.h"
#include "../utils/OutStream.h"
#include "../utils/Exception.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/ContainerUtils.h"
#include "../utils/RuntimeCheck.h"
#include "../utils/Timer.h"
#include <unordered_map>
#include <algorithm>
#include <set>
#include <tracy/Tracy.hpp>
#include <zstd.h>


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

	// Write raw_per_anim_node_data
	stream.writeUInt32((uint32)raw_per_anim_node_data.size());
	for(size_t i=0; i<raw_per_anim_node_data.size(); ++i)
		raw_per_anim_node_data[i].writeToStream(stream);
}


void AnimationDatum::readFromStream(uint32 file_version, InStream& stream, js::Vector<KeyFrameTimeInfo>& old_keyframe_times_out, js::Vector<js::Vector<Vec4f, 16> >& old_output_data)
{
	name = stream.readStringLengthFirst(10000);

	// Read raw_per_anim_node_data
	{
		const uint32 num = stream.readUInt32();
		if(num > 100000)
			throw glare::Exception("invalid num");
		raw_per_anim_node_data.resize(num);
		for(size_t i=0; i<raw_per_anim_node_data.size(); ++i)
			raw_per_anim_node_data[i].readFromStream(stream);
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
			std::vector<float>& vec = old_keyframe_times_out[i].times;
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


void AnimationDatum::checkData(const js::Vector<KeyFrameTimeInfo>& shared_keyframe_times, const js::Vector<js::Vector<Vec4f, 16> >& shared_output_data) const
{
	const js::Vector<KeyFrameTimeInfo>&       keyframe_times = (!m_keyframe_times.empty()) ? m_keyframe_times : shared_keyframe_times;
	const js::Vector<js::Vector<Vec4f, 16> >& output_data    = (!m_output_data.empty())    ? m_output_data    : shared_output_data;

	// Bounds-check data
	for(size_t i=0; i<raw_per_anim_node_data.size(); ++i)
	{
		// -1 is a valid value
		const PerAnimationNodeData& data = raw_per_anim_node_data[i];
		checkProperty(data.translation_input_accessor  >= -1 && data.translation_input_accessor   < (int)keyframe_times.size(), "invalid input accessor index");
		checkProperty(data.rotation_input_accessor     >= -1 && data.rotation_input_accessor      < (int)keyframe_times.size(), "invalid input accessor index");
		checkProperty(data.scale_input_accessor        >= -1 && data.scale_input_accessor         < (int)keyframe_times.size(), "invalid input accessor index");

		checkProperty(data.translation_output_accessor >= -1 && data.translation_output_accessor  < (int)output_data.size(), "invalid output accessor index");
		checkProperty(data.rotation_output_accessor    >= -1 && data.rotation_output_accessor     < (int)output_data.size(), "invalid output accessor index");
		checkProperty(data.scale_output_accessor       >= -1 && data.scale_output_accessor        < (int)output_data.size(), "invalid output accessor index");

		if(data.translation_input_accessor >= 0) checkProperty(keyframe_times[data.translation_input_accessor].times.size() >= 1, "invalid num keyframes");
		if(data.rotation_input_accessor    >= 0) checkProperty(keyframe_times[data.rotation_input_accessor   ].times.size() >= 1, "invalid num keyframes");
		if(data.scale_input_accessor       >= 0) checkProperty(keyframe_times[data.scale_input_accessor      ].times.size() >= 1, "invalid num keyframes");

		// Output data vectors should be the same size as the input keyframe vectors, when they are used together.
		if(data.translation_input_accessor >= 0)
		{
			checkProperty(data.translation_output_accessor >= 0, "invalid output_accessor, must be >= 0 for >= 0 input accessor.");
			checkProperty(keyframe_times[data.translation_input_accessor].times.size() == output_data[data.translation_output_accessor].size(), "num keyframes != output_data size");
		}
		if(data.rotation_input_accessor    >= 0)
		{
			checkProperty(data.rotation_output_accessor >= 0, "invalid output_accessor, must be >= 0 for >= 0 input accessor.");
			checkProperty(keyframe_times[data.rotation_input_accessor   ].times.size() == output_data[data.rotation_output_accessor   ].size(), "num keyframes != output_data size");
		}
		if(data.scale_input_accessor       >= 0)
		{
			checkProperty(data.scale_output_accessor >= 0, "invalid output_accessor, must be >= 0 for >= 0 input accessor.");
			checkProperty(keyframe_times[data.scale_input_accessor      ].times.size() == output_data[data.scale_output_accessor      ].size(), "num keyframes != output_data size");
		}
	}
}


size_t AnimationDatum::getTotalMemUsage() const
{
	size_t sum =
		name.capacity() + 
		raw_per_anim_node_data.capacity() * sizeof(PerAnimationNodeData) + 
		used_input_accessor_indices.capacity() * sizeof(int);

	for(size_t i=0; i<m_keyframe_times.size(); ++i)
		sum += m_keyframe_times[i].times.capacity() * sizeof(float);

	for(size_t i=0; i<m_output_data.size(); ++i)
		sum += m_output_data[i].capacitySizeBytes();

	return sum;
}


static const uint32 ANIMATION_DATA_VERSION = 4;
// Version 3: Added serialisation of vrm_data if present
// Version 4: Added compression of quaternions.  Removed old_skeleton_root_transform serialisation.

static const uint32 OUTPUT_COMPRESSION_TYPE_NONE = 0;
static const uint32 OUTPUT_COMPRESSION_TYPE_QUAT_MESHOPT_ZSTD = 1;



void AnimationData::writeToStream(OutStream& stream) const
{
	ZoneScoped; // Tracy profiler

	stream.writeUInt32(ANIMATION_DATA_VERSION);

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
		const std::vector<float>& vec = keyframe_times[i].times;
		stream.writeUInt32((uint32)vec.size());
		stream.writeData(vec.data(), sizeof(float) * vec.size());
	}

	// Write output_data

	// Work out which output data vectors are rotation data
	std::vector<bool> is_rot_output_data(output_data.size(), false);
	for(size_t i=0; i<animations.size(); ++i)
		for(size_t z=0; z<animations[i]->raw_per_anim_node_data.size(); ++z)
			if(animations[i]->raw_per_anim_node_data[z].rotation_output_accessor != -1)
			{
				runtimeCheck(animations[i]->raw_per_anim_node_data[z].rotation_output_accessor < is_rot_output_data.size());
				is_rot_output_data[animations[i]->raw_per_anim_node_data[z].rotation_output_accessor] = true;
			}


	stream.writeUInt32((uint32)output_data.size());
	for(size_t i=0; i<output_data.size(); ++i)
	{
		const js::Vector<Vec4f, 16>& vec = output_data[i];

		if(is_rot_output_data[i])
		{
			// Write compression type
			stream.writeUInt32(OUTPUT_COMPRESSION_TYPE_QUAT_MESHOPT_ZSTD);

			// Write vec size (num quats)
			stream.writeUInt32((uint32)vec.size()); 

			// Filter data
			std::vector<uint8> filtered(vec.size() * 4 * sizeof(uint16));
			meshopt_encodeFilterQuat(/*destination=*/filtered.data(), /*count=*/vec.size(), /*(destination) stride=*/4 * sizeof(uint16), /*bits=*/16, /*data=*/(const float*)vec.data());

			// Encode
			const size_t encoded_size_bound = meshopt_encodeVertexBufferBound(/*vertex count=*/vec.size(), /*attr size=*/4 * sizeof(uint16));
			js::Vector<uint8> encoded_buf(encoded_size_bound);

			// Note 2 is the default meshopt compression level.
			const size_t res_encoded_vert_buf_size = meshopt_encodeVertexBufferLevel(encoded_buf.data(), encoded_buf.size(), filtered.data(), vec.size(), /*attr size=*/4 * sizeof(uint16), /*compression level=*/2, /*vertex version=*/1);
			encoded_buf.resize(res_encoded_vert_buf_size);

			// Compress the buffer with zstandard
			const size_t compressed_bound = ZSTD_compressBound(encoded_buf.size());
			js::Vector<uint8, 16> compressed_data(compressed_bound);
			const size_t compressed_size = ZSTD_compress(/*dest=*/compressed_data.data(), /*destsize=*/compressed_data.size(), /*src=*/encoded_buf.data(), /*srcsize=*/encoded_buf.size(),
				9 // compression level
			);

			if(ZSTD_isError(compressed_size))
				throw glare::Exception("Compression failed: " + toString(compressed_size));

			// Write compressed size
			stream.writeUInt32((uint32)compressed_size);

			// Write compressed data
			stream.writeData(compressed_data.data(), compressed_size);
		}
		else
		{
			// Write compression type
			stream.writeUInt32(OUTPUT_COMPRESSION_TYPE_NONE);

			stream.writeUInt32((uint32)vec.size());
			stream.writeData(vec.data(), sizeof(Vec4f) * vec.size());
		}
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


void AnimationData::readFromStream(RandomAccessInStream& stream)
{
	ZoneScoped; // Tracy profiler

	const uint32 version = stream.readUInt32();
	if(version > ANIMATION_DATA_VERSION)
		throw glare::Exception("Invalid animation data version: " + toString(version));

	if(version <= 3)
	{
		Matrix4f skeleton_root_transform; // unused
		stream.readData(skeleton_root_transform.e, sizeof(float)*16);
	}

	// Read nodes
	{
		const uint32 num = stream.readUInt32();
		if(num > 100000)
			throw glare::Exception("invalid num nodes: " + toString(num));
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
			std::vector<float>& vec = keyframe_times[i].times;
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


		js::Vector<uint8> decompressed;
		js::Vector<uint8> decoded;

		for(size_t i=0; i<output_data.size(); ++i)
		{
			if(version <= 3)
			{
				js::Vector<Vec4f, 16>& vec = output_data[i];
				const uint32 vec_size = stream.readUInt32();
				if(vec_size > 100000)
					throw glare::Exception("invalid vec_size");
				vec.resize(vec_size);
				stream.readData(vec.data(), vec_size * sizeof(Vec4f));
			}
			else
			{
				const uint32 compression_type = stream.readUInt32();

				js::Vector<Vec4f, 16>& vec = output_data[i];
				const uint32 vec_size = stream.readUInt32();
				if(vec_size > 100000)
					throw glare::Exception("invalid vec_size");
				vec.resize(vec_size);

				if(compression_type == OUTPUT_COMPRESSION_TYPE_NONE)
				{
					stream.readData(vec.data(), vec_size * sizeof(Vec4f));
				}
				else if(compression_type == OUTPUT_COMPRESSION_TYPE_QUAT_MESHOPT_ZSTD)
				{
					const uint32 compressed_size = stream.readUInt32(); // Read compressed size

					// Check that reading the compressed data will be in the file bounds.
					if(!stream.canReadNBytes(compressed_size))
						throw glare::Exception("Compressed data extends past end of stream (file truncated?)");

					// Read decompressed size
					const uint64 decompressed_size = ZSTD_getFrameContentSize(stream.currentReadPtr(), /*src size=*/compressed_size);
					if(decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN || decompressed_size == ZSTD_CONTENTSIZE_ERROR)
						throw glare::Exception("Failed to get decompressed_size");

					const size_t max_decompressed_size = 1'000'000;
					if(decompressed_size > max_decompressed_size) // Sanity check decompressed_size
						throw glare::Exception("decompressed_size too large.");

					// Decompress data
					decompressed.resizeNoCopy(decompressed_size);
					const size_t res = ZSTD_decompress(/*des=*/decompressed.data(), decompressed.size(), stream.currentReadPtr(), compressed_size);
					if(ZSTD_isError(res))
						throw glare::Exception("Decompression of buffer failed: " + toString(res));
					if(res < decompressed_size)
						throw glare::Exception("Decompression of buffer failed: not enough bytes in result");

					stream.advanceReadIndex(compressed_size);

					// meshopt decode
					decoded.resizeNoCopy(vec.size() * 4 * sizeof(uint16));
					if(meshopt_decodeVertexBuffer(/*destination=*/decoded.data(), /*vertex count=*/vec.size(), /*vertex size=*/4 * sizeof(uint16), decompressed.data(), decompressed.size()) != 0)
						throw glare::Exception("meshopt_decodeVertexBuffer failed.");

					// unfilter (in-place)
					meshopt_decodeFilterQuat(/*buffer=*/decoded.data(), vec.size(), /*stride=*/4 * sizeof(uint16));

					// Dequantize into the final quat vector
					// Note that we normalise in Quatf::nlerp when doing animation with this data, so we shouldn't need a normalise here.
					const int16* const decoded_data = (int16*)decoded.data();
					Vec4f* const vec_data = vec.data();
					for(size_t z=0; z<vec.size(); ++z)
					{
						vec_data[z] = Vec4f(
							decoded_data[4 * z + 0],
							decoded_data[4 * z + 1],
							decoded_data[4 * z + 2],
							decoded_data[4 * z + 3]
						) * (1 / 32767.f);
					}
				}
				else
					throw glare::Exception("Unknown compression type " + toString(compression_type));
			}
		}
	}

	// Read animations
	{
		const uint32 num = stream.readUInt32();
		if(num > 10000)
			throw glare::Exception("invalid num anims: " + toString(num));
		animations.resize(num);
		for(size_t i=0; i<animations.size(); ++i)
		{
			animations[i] = new AnimationDatum();

			js::Vector<KeyFrameTimeInfo> old_keyframe_times;
			js::Vector<js::Vector<Vec4f, 16> > old_output_data;

			animations[i]->readFromStream(version, stream, old_keyframe_times, old_output_data);

			// Do backwards-compat handling
			if(version == 1)
			{
				const int keyframe_times_offset = (int)keyframe_times.size();
				const int output_data_offset = (int)output_data.size();
				ContainerUtils::append(keyframe_times, old_keyframe_times);
				ContainerUtils::append(output_data, old_output_data);

				// Offset accessors
				for(size_t z=0; z<animations[i]->raw_per_anim_node_data.size(); ++z)
				{
					PerAnimationNodeData& data = animations[i]->raw_per_anim_node_data[z];
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
	vrm_data = nullptr;
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

	checkDataIsValid();
	
	build();

	checkPerAnimNodeDataIsValid();
}


static float getMatrixMaxScaling(const Matrix4f& m)
{
	return myMax(
		m.getColumn(0).length(),
		m.getColumn(1).length(),
		m.getColumn(2).length()
	);
}


// Mixamo animations exported from Blender have some scales of 100 and 0.01 due to use of cm units.  Remove this.
void AnimationData::removeInverseBindMatrixScaling()
{
	for(size_t i=0; i<this->animations.size(); ++i)
	{
		AnimationDatum& anim = *this->animations[i];

		// Look through the bind matrices and see if they have the scaling
		float max_scaling = 0.f;
		for(size_t node_i=0; node_i < anim.raw_per_anim_node_data.size(); ++node_i) // For each new node index:
		{
			const float node_max_scaling = getMatrixMaxScaling(this->nodes[node_i].inverse_bind_matrix);
			max_scaling = myMax(max_scaling, node_max_scaling);
		}

		if(epsEqual(max_scaling, 100.f, 0.1f))
		{
			// Remove the scaling
			for(size_t node_i=0; node_i < anim.raw_per_anim_node_data.size(); ++node_i) // For each new node index:
			{
				if(this->nodes[node_i].name == "Armature")
				{
					assert(node_i == 67);
					// conPrint("scale: " + this->nodes[node_i].scale.toStringMaxNDecimalPlaces(4));
					this->nodes[node_i].scale = Vec4f(1,1,1,0); // Set root armature node scale = 1
				}

				this->nodes[node_i].trans *= 0.01f;

				// Adjust inverse_bind_matrices that have a 100 scaling.
				if(epsEqual(getMatrixMaxScaling(this->nodes[node_i].inverse_bind_matrix), 100.f, 0.1f))
					this->nodes[node_i].inverse_bind_matrix = Matrix4f::uniformScaleMatrix(0.01f) * this->nodes[node_i].inverse_bind_matrix;


				js::Vector<js::Vector<Vec4f, 16> >& use_output_data = (!anim.m_output_data.empty()) ? anim.m_output_data : output_data;

				PerAnimationNodeData& anim_node_data = anim.raw_per_anim_node_data[node_i];
				//if(anim_node_data.scale_output_accessor != -1)
				//{
				//	js::Vector<Vec4f, 16>& scale_output_data = use_output_data[anim_node_data.scale_output_accessor];
				//	for(size_t z=0; z<scale_output_data.size(); ++z)
				//	{
				//		Vec4f& v = scale_output_data[z];
				//		//conPrint("scale_output_data: " + scale_output_data[z].toStringMaxNDecimalPlaces(4));
				//		//conPrint(doubleToStringNSigFigs(v[0], 4));
				//	}
				//}

				if(anim_node_data.translation_output_accessor != -1)
				{
					js::Vector<Vec4f, 16>& trans_output_data = use_output_data[anim_node_data.translation_output_accessor];
					for(size_t z=0; z<trans_output_data.size(); ++z)
					{
						//conPrint("trans_output_data: " + trans_output_data[z].toStringMaxNDecimalPlaces(4));

						Vec4f& v = trans_output_data[z];

						const float factor = 0.01f;
						v = mul(v, Vec4f(factor, factor, factor, 1.0f));
					}
				}
			}
		}
	}
}


void AnimationData::removeUnneededOutputData()
{
	// Which output data is actually needed? (e.g. which is accessed via accessors where the animated output data is != the node data?)
	std::vector<bool> output_data_needed(output_data.size(), false);

	for(size_t anim_i=0; anim_i<animations.size(); ++anim_i)
	{
		AnimationDatum& anim = *animations[anim_i];

		for(size_t i=0; i<anim.raw_per_anim_node_data.size(); ++i)
		{
			const PerAnimationNodeData& node_data = anim.raw_per_anim_node_data[i];
			if(node_data.scale_output_accessor != -1)
			{
				js::Vector<Vec4f, 16>& data = output_data[node_data.scale_output_accessor];
				//conPrint("Found scale data with " + toString(data.size()) + " output values.");
			
				bool all_equal_to_node_scale = true;
				for(size_t z=0; z<data.size(); ++z)
				{
					//conPrint("value " + toString(z) + ": " + data[z].toStringMaxNDecimalPlaces(10));

					if(!epsEqual(data[z], nodes[i].scale, /*eps=*/1.0e-4f))
						all_equal_to_node_scale = false;
				}

				//conPrint("all_equal_to_node_scale: " + boolToString(all_equal_to_node_scale));
				if(!all_equal_to_node_scale)
					output_data_needed[node_data.scale_output_accessor] = true;
			}

			//const PerAnimationNodeData& node_data = anim.raw_per_anim_node_data[i];
			if(node_data.translation_output_accessor != -1)
			{
				js::Vector<Vec4f, 16>& data = output_data[node_data.translation_output_accessor];
				//conPrint("Found translation data with " + toString(data.size()) + " output values.");

				bool all_equal_to_node_trans = true;
				for(size_t z=0; z<data.size(); ++z)
				{
					//conPrint("value " + toString(z) + ": " + data[z].toStringMaxNDecimalPlaces(10));

					if(!epsEqual(data[z], nodes[i].trans, /*eps=*/1.0e-4f))
						all_equal_to_node_trans = false;
				}

				//conPrint("node translation: " + nodes[i].trans.toStringMaxNDecimalPlaces(10));
				//conPrint("all_equal_to_node_trans: " + boolToString(all_equal_to_node_trans));

				if(!all_equal_to_node_trans)
					output_data_needed[node_data.translation_output_accessor] = true;
			}

			if(node_data.rotation_output_accessor != -1)
			{
				js::Vector<Vec4f, 16>& data = output_data[node_data.rotation_output_accessor];
				//conPrint("Found rotation data with " + toString(data.size()) + " output values.");

				bool all_equal_to_node_rot = true;
				for(size_t z=0; z<data.size(); ++z)
				{
					//conPrint("value " + toString(z) + ": " + data[z].toStringMaxNDecimalPlaces(10));

					if(!epsEqual(data[z], nodes[i].rot.v, /*eps=*/1.0e-4f))
						all_equal_to_node_rot = false;
				}

				//conPrint("node rotation: " + nodes[i].rot.toString());
				//conPrint("all_equal_to_node_rot: " + boolToString(all_equal_to_node_rot));

				if(!all_equal_to_node_rot)
					output_data_needed[node_data.rotation_output_accessor] = true;
			}
		}
	}


	js::Vector<js::Vector<Vec4f, 16> > new_output_data;
	std::vector<int> new_output_indices(output_data.size());
	int new_i = 0;
	for(size_t i=0; i<output_data.size(); ++i)
	{
		if(output_data_needed[i])
		{
			new_output_indices[i] = new_i;
			new_output_data.push_back(output_data[i]);
			new_i++;
		}
		else
			new_output_indices[i] = -1;
	}

	assert(new_output_data.size() == new_i);

	conPrint("\tremoveUnneededOutputData(): keeping " + toString(new_i) + " / " + toString(output_data.size()) + " output_data vectors");

	for(size_t anim_i=0; anim_i<animations.size(); ++anim_i)
	{
		AnimationDatum& anim = *animations[anim_i];

		// Update accessor indices
		for(size_t i=0; i<anim.raw_per_anim_node_data.size(); ++i)
		{
			PerAnimationNodeData& node_data = anim.raw_per_anim_node_data[i];
			if(node_data.rotation_output_accessor != -1)
			{
				node_data.rotation_output_accessor = new_output_indices[node_data.rotation_output_accessor];
				if(node_data.rotation_output_accessor == -1)
					node_data.rotation_input_accessor = -1;
			}

			if(node_data.translation_output_accessor != -1)
			{
				node_data.translation_output_accessor = new_output_indices[node_data.translation_output_accessor];
				if(node_data.translation_output_accessor == -1)
					node_data.translation_input_accessor = -1;
			}

			if(node_data.scale_output_accessor != -1)
			{
				node_data.scale_output_accessor = new_output_indices[node_data.scale_output_accessor];
				if(node_data.scale_output_accessor == -1)
					node_data.scale_input_accessor = -1;
			}
		}
	}


	output_data = new_output_data;


	checkDataIsValid();
}


void AnimationData::printStats()
{
	size_t total_trans_output_size = 0;
	size_t total_rot_output_size = 0;
	size_t total_scale_output_size = 0;
	size_t total_compressed_output_size_B = 0;

	for(size_t anim_i=0; anim_i<animations.size(); ++anim_i)
	{
		AnimationDatum& anim = *animations[anim_i];

		conPrint("----- Animation " + toString(anim_i) + " ('" + anim.name + "') -----");

		for(size_t i=0; i<anim.raw_per_anim_node_data.size(); ++i)
		{
			const PerAnimationNodeData& node_data = anim.raw_per_anim_node_data[i];

			if(node_data.translation_output_accessor != -1)
				total_trans_output_size += output_data[node_data.translation_output_accessor].size();

			if(node_data.scale_output_accessor != -1)
				total_scale_output_size += output_data[node_data.scale_output_accessor].size();

			if(node_data.rotation_output_accessor != -1)
			{
				total_rot_output_size += output_data[node_data.rotation_output_accessor].size();

				// Try compressing
				std::vector<uint8> filtered(output_data[node_data.rotation_output_accessor].size() * 4 * sizeof(uint16));
				meshopt_encodeFilterQuat(filtered.data(), /*count=*/output_data[node_data.rotation_output_accessor].size(), /*(destination) stride=*/4 * sizeof(uint16), /*bits=*/16, /*data=*/(const float*)output_data[node_data.rotation_output_accessor].data());

#if 1
				const size_t encoded_size_bound = meshopt_encodeVertexBufferBound(/*vertex count=*/output_data[node_data.rotation_output_accessor].size(), /*attr size=*/4 * sizeof(uint16));
				js::Vector<uint8> encoded_buf(encoded_size_bound);

				// Note 2 is the default meshopt compression level.
				const size_t res_encoded_vert_buf_size = meshopt_encodeVertexBufferLevel(encoded_buf.data(), encoded_buf.size(), filtered.data(), output_data[node_data.rotation_output_accessor].size(), /*attr size=*/4 * sizeof(uint16), /*compression level=*/2, /*vertex version=*/1);
				encoded_buf.resize(res_encoded_vert_buf_size);

				// Compress the buffer with zstandard
				const size_t compressed_bound = ZSTD_compressBound(encoded_buf.size());
				js::Vector<uint8, 16> compressed_data(compressed_bound);
				const size_t compressed_size = ZSTD_compress(/*dest=*/compressed_data.data(), /*destsize=*/compressed_data.size(), /*src=*/encoded_buf.data(), /*srcsize=*/encoded_buf.size(),
					9 // compression level
				);
#else
				// Compress the buffer with zstandard
				const size_t compressed_bound = ZSTD_compressBound(filtered.size());
				js::Vector<uint8, 16> compressed_data(compressed_bound);
				const size_t compressed_size = ZSTD_compress(/*dest=*/compressed_data.data(), /*destsize=*/compressed_data.size(), /*src=*/filtered.data(), /*srcsize=*/filtered.size(),
					9 // compression level
				);

#endif
				if(ZSTD_isError(compressed_size))
					throw glare::Exception("Compression failed: " + toString(compressed_size));

				const size_t old_size_B = output_data[node_data.rotation_output_accessor].size() * sizeof(Vec4f);
				const float compression_ratio = (float)old_size_B / compressed_size;
				conPrint("old size: " + uInt64ToStringCommaSeparated(old_size_B) + " B, new size: " + uInt64ToStringCommaSeparated(compressed_size) + " B (compression_ratio: " + doubleToStringMaxNDecimalPlaces(compression_ratio, 3) + ")");

				total_compressed_output_size_B += compressed_size;
			}
		}

		printVar(total_trans_output_size);
		printVar(total_scale_output_size);
		printVar(total_rot_output_size);
		printVar(total_compressed_output_size_B);
	}
}



void dumpNodeData(const AnimationData& data)
{
	for(size_t i=0; i<data.nodes.size(); ++i)
	{
		conPrint("-----node " + toString(i) + ":--------");
		conPrint("inverse_bind_matrix: " + data.nodes[i].inverse_bind_matrix.toString());
		conPrint("name: " + data.nodes[i].name);
		conPrint("");
	}
}


// Copy keyframe_times and output_data from the AnimationData object to the AnimationDatum object, so it can be used by multiple different avatars
void AnimationData::prepareForMultipleUse()
{
	runtimeCheck(animations.size() > 0);
	animations[0]->m_keyframe_times.takeFrom(keyframe_times);
	animations[0]->m_output_data   .takeFrom(output_data);
}


void AnimationData::appendAnimationData(const AnimationData& other)
{
	ZoneScoped; // Tracy profiler

	//conPrint("appendAnimationData(): this data:");
	//dumpNodeData(*this);

	//conPrint("appendAnimationData(): other data:");
	//dumpNodeData(other);

	const size_t new_anim_start = this->animations.size();
	this->animations.insert(this->animations.end(), other.animations.begin(), other.animations.end());

	this->per_anim_node_data.resize(this->animations.size());
	
	for(size_t i=new_anim_start; i<this->animations.size(); ++i)
	{
		const AnimationDatum& anim = *this->animations[i];

		std::vector<PerAnimationNodeData>& anim_i_node_data = this->per_anim_node_data[i];
	
		const std::vector<PerAnimationNodeData>& anim_0_per_node_data = this->per_anim_node_data[0];
		anim_i_node_data.resize(anim_0_per_node_data.size()); // Make the same size as for animation 0 (may have added some nodes in retargetting)
	
		// When we retargetted animation data, nodes where shuffled, new nodes added etc.
		// So we need to assign the node data from other to the correct node data for this.
	
		//Timer timer;

		for(size_t node_i=0; node_i < anim_i_node_data.size(); ++node_i) // For each new node index:
		{
			// Find corresponding old index.
			int other_node_i = -1;

			for(size_t q=0; q<other.nodes.size(); ++q)
				if(other.nodes[q].name == this->nodes[node_i].name)
				{
					other_node_i = (int)q;
					break;
				}
	
			if((other_node_i == -1) || (other_node_i >= anim.raw_per_anim_node_data.size()))
			{
				anim_i_node_data[node_i].init(); // Set all to accessors to -1
			}
			else
			{
				anim_i_node_data[node_i] = anim.raw_per_anim_node_data[other_node_i];
			}
		}

		//conPrint("node reordering took " + timer.elapsedStringNSWIthNSigFigs(4));
	}

	checkDataIsValid();
	checkPerAnimNodeDataIsValid();
}


void AnimationData::operator =(const AnimationData& other)
{
	ZoneScoped; // Tracy profiler

	nodes = other.nodes;
	sorted_nodes = other.sorted_nodes;
	joint_nodes = other.joint_nodes;
	keyframe_times = other.keyframe_times;
	output_data = other.output_data;

	animations = other.animations; // Note: shallow copy.

	per_anim_node_data = other.per_anim_node_data;

	vrm_data = other.vrm_data; // Note: shallow copy.
}


// Check sorted nodes are indeed sorted.
// Throws glare::Exception if not.
static void checkSortedNodesAreSorted(const std::vector<int>& sorted_nodes, const std::vector<AnimationNodeData>& nodes)
{
	// Make map from node index to index in sorted_nodes
	std::vector<int> in_sorted_nodes_index(nodes.size(), -1);
	for(size_t i=0; i<sorted_nodes.size(); ++i)
	{
		const int node_i = sorted_nodes[i];
		if(in_sorted_nodes_index[node_i] != -1)
			throw glare::Exception("node appears twice in sorted_nodes");
		in_sorted_nodes_index[node_i] = (int)i;
	}

	for(size_t i=0; i<sorted_nodes.size(); ++i)
	{
		const int node_i = sorted_nodes[i];
		const int parent_node_i = nodes[node_i].parent_index; // Will be -1 if no parent
		if(parent_node_i != -1)
		{
			const int parent_i_in_sorted_nodes = in_sorted_nodes_index[parent_node_i];
			if(parent_i_in_sorted_nodes == -1)
				throw glare::Exception("Parent is not in sorted nodes");
			if(parent_i_in_sorted_nodes > i)
				throw glare::Exception("Parent is after in sorted nodes");
		}
	}
}


void AnimationData::checkDataIsValid() const
{
	ZoneScoped; // Tracy profiler

	// Bounds-check data
	for(size_t i=0; i<sorted_nodes.size(); ++i)
		checkProperty(sorted_nodes[i] >= 0 && sorted_nodes[i] < (int)nodes.size(), "invalid sorted_nodes index");

	for(size_t i=0; i<joint_nodes.size(); ++i)
		checkProperty(joint_nodes[i] >= 0 && joint_nodes[i] < (int)nodes.size(), "invalid joint_nodes index");

	for(size_t i=0; i<nodes.size(); ++i)
		checkProperty((nodes[i].parent_index >= -1) && (nodes[i].parent_index < (int)nodes.size()) && (nodes[i].parent_index != (int)i), "invalid parent_index index"); // parent_index of -1 is valid.

	if(sorted_nodes.size() != nodes.size())
		throw glare::Exception("sorted_nodes.size() != nodes.size()");

	// Check sorted nodes are indeed sorted
	checkSortedNodesAreSorted(sorted_nodes, nodes);

	for(size_t a=0; a<animations.size(); ++a)
	{
		const AnimationDatum* datum = animations[a].ptr();
		datum->checkData(this->keyframe_times, this->output_data);
	}
}


void AnimationData::build() // Builds keyframe_times data, computes animation length.
{
	// For each animation, compute length of animation - largest input timestamp, as well as used input accessor indices.

	ZoneScoped; // Tracy profiler

	this->per_anim_node_data.resize(animations.size());
	
	for(size_t anim_i=0; anim_i<animations.size(); ++anim_i)
	{
		AnimationDatum* anim = animations[anim_i].ptr();

		std::set<int> used_input_accessor_indices;
		float max_len = 0.f;
		for(size_t z=0; z<anim->raw_per_anim_node_data.size(); ++z)
		{
			PerAnimationNodeData& data = anim->raw_per_anim_node_data[z];
			if(data.translation_input_accessor != -1)	max_len = myMax(max_len, keyframe_times[data.translation_input_accessor].times.back());
			if(data.rotation_input_accessor != -1)		max_len = myMax(max_len, keyframe_times[data.rotation_input_accessor].times.back());
			if(data.scale_input_accessor != -1)			max_len = myMax(max_len, keyframe_times[data.scale_input_accessor].times.back());

			if(data.translation_input_accessor != -1)	used_input_accessor_indices.insert(data.translation_input_accessor);
			if(data.rotation_input_accessor != -1)		used_input_accessor_indices.insert(data.rotation_input_accessor);
			if(data.scale_input_accessor != -1)			used_input_accessor_indices.insert(data.scale_input_accessor);
		}
		anim->anim_len = max_len;

		// Build used_input_accessor_indices vector for this animation.
		// This is used so we only need to process input keyframes actually used for a particular animation.
		anim->used_input_accessor_indices.reserve(used_input_accessor_indices.size());
		for(auto it = used_input_accessor_indices.begin(); it != used_input_accessor_indices.end(); ++it)
			anim->used_input_accessor_indices.push_back(*it);
		
		assert(std::is_sorted(anim->used_input_accessor_indices.begin(), anim->used_input_accessor_indices.end()));


		this->per_anim_node_data[anim_i] = anim->raw_per_anim_node_data;
	}

	// Check if keyframe_times are equally spaced.  If they are, store some info that will allow fast finding of the current frame given the time in the animation.
	for(size_t i=0; i<keyframe_times.size(); ++i)
	{
		const std::vector<float>& vec = keyframe_times[i].times;

		keyframe_times[i].times_size = (int)vec.size();
		if(vec.size() >= 1)
		{
			keyframe_times[i].t_0 = vec[0];
			keyframe_times[i].t_back = vec.back();
		}
		if(vec.size() >= 2)
		{
			bool is_equally_spaced = true;
			const float t_0 = vec[0];
			const float t_back = vec.back();
			const float equal_gap = (t_back - t_0) / (float)(vec.size() - 1);
			for(size_t z=0; z<vec.size(); ++z)
			{
				const float t = vec[z];
				const float expected_t = t_0 + z * equal_gap;
				if(!::epsEqual(t, expected_t, /*eps=*/1.0e-3f))
				{
					// conPrint("key frame not equally spaced: t: " + toString(t) + ", expected_t: " + toString(expected_t));
					is_equally_spaced = false;
					break;
				}
			}

			// conPrint("Anim data with " + toString(vec.size()) + " keyframe times: is_equally_spaced: " + boolToString(is_equally_spaced));

			keyframe_times[i].equally_spaced = is_equally_spaced;
			if(is_equally_spaced)
			{
				keyframe_times[i].spacing = equal_gap;
				keyframe_times[i].recip_spacing = (float)(vec.size() - 1) / (t_back - t_0);
			}
		}
		else
			keyframe_times[i].equally_spaced = false;
	}
}


void AnimationData::checkPerAnimNodeDataIsValid() const
{
	// Check per_anim_node_data
	checkProperty(per_anim_node_data.size() == animations.size(), "per_anim_node_data.size() != animations.size()");
	for(size_t a=0; a<animations.size(); ++a)
		checkProperty(per_anim_node_data[a].size() >= nodes.size(), "per_anim_node_data[a].size() was not >= nodes.size()");
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


AnimationNodeData* AnimationData::findNode(const std::string& name) // Returns NULL if not found
{
	for(size_t i=0; i<nodes.size(); ++i)
		if(nodes[i].name == name)
			return &nodes[i];
	return NULL;
}


int AnimationData::getNodeIndex(const std::string& name) const // Returns -1 if not found
{
	for(size_t i=0; i<nodes.size(); ++i)
		if(nodes[i].name == name)
			return (int)i;
	return -1;
}


int AnimationData::getNodeIndexWithNameSuffix(const std::string& name_suffix)
{
	for(size_t i=0; i<nodes.size(); ++i)
		if(::hasSuffix(nodes[i].name, name_suffix))
			return (int)i;
	return -1;
}


// Load animation data from a GLB file from the stream.
// Take the animation data, retarget it (adjust bone lengths etc.) and store in the current object.
void AnimationData::loadAndRetargetAnim(const AnimationData& other)
{
	ZoneScoped; // Tracy profiler

	const bool VERBOSE = false;

	const char* VRM_to_RPM_name_data[] = {
		// From vroidhub_avatarsample_A.vrm or testguy.vrm
		"J_Bip_L_Index3_end",	"LeftHandIndex4",
		"J_Bip_L_Little3_end",	"LeftHandPinky4",
		"J_Bip_L_Middle3_end",	"LeftHandMiddle4",
		"J_Bip_L_Ring3_end",	"LeftHandRing4",

		"J_Bip_R_Index3_end",	"RightHandIndex4",
		"J_Bip_R_Little3_end",	"RightHandPinky4",
		"J_Bip_R_Middle3_end",	"RightHandMiddle4",
		"J_Bip_R_Ring3_end",	"RightHandRing4",

		"J_Bip_L_Little2",		"LeftHandPinky2",
		"J_Bip_L_Thumb3_end",	"LeftHandThumb4",
		"J_Bip_L_Thumb1",		"LeftHandThumb1",

		"J_Bip_R_Little2",		"RightHandPinky2",
		"J_Bip_R_Thumb3_end",	"RightHandThumb4",
		"J_Bip_R_Thumb1",		"RightHandThumb1",

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

		"Thumb0_R",				"LeftHandThumb1", // NOTE: these mappings correct?
		"Thumb1_R",				"LeftHandThumb2",
		"Thumb2_R",				"LeftHandThumb3",
		"Thumb2_R_end",			"LeftHandThumb4",

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

		"Thumb0_R",				"RightHandThumb1", // NOTE: these mappings correct?
		"Thumb1_R",				"RightHandThumb2",
		"Thumb2_R",				"RightHandThumb3",
		"Thumb2_R_end",			"RightHandThumb4",

		"J_Bip_L_ToeBase_end",	"LeftToe_End",
		"J_Bip_R_ToeBase_end",	"RightToe_End",
	};
	static_assert(staticArrayNumElems(VRM_to_RPM_name_data) % 2 == 0, "staticArrayNumElems(VRM_to_RPM_name_data) % 2 == 0");

	std::map<std::string, std::string> VRM_to_RPM_names;

	for(size_t z=0; z<staticArrayNumElems(VRM_to_RPM_name_data); z += 2)
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

	for(size_t z=0; z<staticArrayNumElems(finger_VRM_names); ++z)
	{
		const std::string left_hand_name  = std::string("Left")  + finger_VRM_names[z];
		const std::string right_hand_name = std::string("Right") + finger_VRM_names[z];

		VRM_to_RPM_names.insert(std::make_pair("Mixamorig_" + left_hand_name, left_hand_name));
		VRM_to_RPM_names.insert(std::make_pair("Mixamorig_" + right_hand_name, right_hand_name));
	}

	// Ready Player Me (RPM) bone names.
	// These seem to be from the Mixamo rig, but with the "mixamorig:" prefix removed.
	// These are the bone names our animation data uses.
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

	// VRM bone names, corresponding to the RPM bone names above.
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
		"rightLittleIntermediate",
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

	std::map<std::string, std::string> RPM_to_VRM_bone_name;
	for(size_t z=0; z<staticArrayNumElems(RPM_bone_names); ++z)
		RPM_to_VRM_bone_name[RPM_bone_names[z]] = VRM_bone_names[z];
	

	// The old nodes have the correct sizing, the new nodes are associated with the correct animation data.
	const std::vector<AnimationNodeData> old_nodes = nodes; // Copy old node data
	const std::vector<int> old_joint_nodes = joint_nodes;
	const std::vector<int> old_sorted_nodes = sorted_nodes;

	GLTFVRMExtensionRef old_vrm_data = vrm_data;

	this->nodes = other.nodes;
	this->sorted_nodes = other.sorted_nodes;
	this->joint_nodes = other.joint_nodes;
	this->keyframe_times = other.keyframe_times;
	this->output_data = other.output_data;
	this->animations = other.animations;
	this->per_anim_node_data = other.per_anim_node_data;
	this->vrm_data = other.vrm_data;


	std::unordered_map<std::string, int> new_bone_names_to_index;
	for(size_t z=0; z<nodes.size(); ++z)
		new_bone_names_to_index.insert(std::make_pair(nodes[z].name, (int)z));

	std::unordered_map<std::string, int> old_node_names_to_index;
	for(size_t z=0; z<old_nodes.size(); ++z)
		old_node_names_to_index.insert(std::make_pair(old_nodes[z].name, (int)z));


	// Build mapping from old node index to new node index and vice-versa.
	std::map<int, int> old_to_new_node_index;
	std::map<int, int> new_to_old_node_index;
	for(size_t i=0; i<nodes.size(); ++i) // For each new node (node from GLB animation data):
	{
		const AnimationNodeData& new_node = nodes[i];
		const std::string new_node_name = eatPrefix(new_node.name, "mixamorig:");

		int old_node_index = -1;

		// Add to mappings based on VRM metadata
		if(old_vrm_data.nonNull()) // If we had VRM metadata:
		{
			// We have the new node name (RPM name).
			// Find corresponding VRM bone name.
			// Then look up in VRM metadata to get the node index from the bone name.
			auto res = RPM_to_VRM_bone_name.find(new_node_name);
			if(res != RPM_to_VRM_bone_name.end())
			{
				const std::string VRM_name = res->second;

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

		if(old_node_index == -1) // If no mapping found yet:
		{
			// See if there is an old node with the same name as the new node
			auto res = old_node_names_to_index.find(new_node_name);
			if(res != old_node_names_to_index.end())
				old_node_index = res->second;
		}

		if(old_node_index == -1) // If no mapping found yet:
		{
			// Try finding an old node with the same name, but with a "mixamorig:" prefix.  This should match files rigged with Mixamo bone names.
			auto res = old_node_names_to_index.find("mixamorig:" + new_node.name); 
			if(res != old_node_names_to_index.end())
				old_node_index = res->second;
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

	// We will do multiple passes over this, assigning new nodes.  This is to handle cases where the nodes are in reverse order, like
	// Tail_2, Tail_1, Tail, where Tail_1 is a child of Tail.  Tail needs to be created first before Tail_1 can use it as a parent.
	for(int q=0; ; ++q)
	{
		bool made_change = false; // Once we have done a pass that makes no change, we are done doing passes.
		for(size_t i=0; i<old_nodes.size(); ++i) // For each old node (node from VRM data):
		{
			if(old_to_new_node_index.count((int)i) == 0) // If no mapping to new node yet:
			{
				const AnimationNodeData& old_node = old_nodes[i];

				// If the old node has a parent, and the parent has a corresponding new node, use that new node as the parent
				if(old_to_new_node_index.count(old_node.parent_index) > 0)
				{
					const int new_parent_i = old_to_new_node_index[old_node.parent_index];

					const bool new_parent_is_new = new_parent_i >= (int)initial_num_new_nodes;

					const int new_i = (int)nodes.size();
					nodes.push_back(AnimationNodeData());
					nodes[new_i] = old_node;
					nodes[new_i].parent_index = new_parent_i;

					assert(new_parent_i < new_i);
					this->sorted_nodes.push_back(new_i);

					for(size_t z=0; z<this->animations.size(); ++z)
					{
						this->per_anim_node_data[z].push_back(PerAnimationNodeData());
						this->per_anim_node_data[z].back().init(); // Init all accessors to -1.
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
					So do this, first apply trans in old parent space, then transform to model space with old_parent_to_model, then 
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

					made_change = true;
				}
			}
		}

		if(!made_change)
			break;
		if(q > 100)
			throw glare::Exception("Apparent infinite loop while creating node mappings");
	}


	js::Vector<Matrix4f, 16> old_node_hier_to_world_matrices(old_sorted_nodes.size());

	for(size_t n=0; n<old_sorted_nodes.size(); ++n)
	{
		const int node_i = old_sorted_nodes[n];
		const AnimationNodeData& node_data = old_nodes[node_i];

		const Matrix4f rot_mat = node_data.rot.toMatrix();

		const Matrix4f TRS(
			rot_mat.getColumn(0) * copyToAll<0>(node_data.scale),
			rot_mat.getColumn(1) * copyToAll<1>(node_data.scale),
			rot_mat.getColumn(2) * copyToAll<2>(node_data.scale),
			setWToOne(node_data.trans));

		const Matrix4f node_transform = (node_data.parent_index == -1) ? TRS : (old_node_hier_to_world_matrices[node_data.parent_index] * TRS);

		old_node_hier_to_world_matrices[node_i] = node_transform;
	}


	js::Vector<Matrix4f, 16> new_node_hier_to_world_matrices(sorted_nodes.size());

	for(size_t n=0; n<sorted_nodes.size(); ++n)
	{
		const int node_i = sorted_nodes[n];
		const AnimationNodeData& node_data = nodes[node_i];

		const Matrix4f rot_mat = node_data.rot.toMatrix();

		const Matrix4f TRS(
			rot_mat.getColumn(0) * copyToAll<0>(node_data.scale),
			rot_mat.getColumn(1) * copyToAll<1>(node_data.scale),
			rot_mat.getColumn(2) * copyToAll<2>(node_data.scale),
			setWToOne(node_data.trans));

		const Matrix4f node_transform = (node_data.parent_index == -1) ? TRS : (new_node_hier_to_world_matrices[node_data.parent_index] * TRS);

		new_node_hier_to_world_matrices[node_i] = node_transform;
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
					Matrix4f new_parent_to_world = new_node_hier_to_world_matrices[new_node.parent_index]; // Use new_node_hier_to_world_matrices for bone -> posed mesh space.  (was using bind matrix)
					
					// Compute the translation for the new node in model space.
					Vec4f new_translation_model_space = new_parent_to_world * new_translation_bs;


					// Handle the case where the parents are not the same, but the parent of the old node is the grandparent of the new node.
					// For now we will only try and handle this for specific nodes, such as the Spine2 case below.
					if(old_to_new_node_index[old_node.parent_index] != new_node.parent_index)
					//if((old_to_new_node_index.count(old_node.parent_index) > 0) && (old_to_new_node_index[old_node.parent_index] != new_node.parent_index))
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

					//Matrix4f old_parent_to_world;
					//old_parent.inverse_bind_matrix.getInverseForAffine3Matrix(old_parent_to_world);

					// NOTE: using old_node_hier_to_world_matrices instead of the bind matrix works better for Ready player me models that have a pose node transform that takes the mesh data in A-pose to T_pose.
					Matrix4f old_parent_to_world = old_node_hier_to_world_matrices[old_node.parent_index];

					// Compute the translation of the old node in model space.
					const Vec4f old_translation_model_space = old_parent_to_world * old_translation_bs;

					// The retargetting vector (in model space) is now the extra translation to make the new translation the same as the old translation
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
		const int old_joint_node_i = joint_nodes[i];
		const AnimationNodeData& old_node = old_nodes[old_joint_node_i];
		auto res = old_to_new_node_index.find(old_joint_node_i);

		if(res != old_to_new_node_index.end())
		{
			const int new_joint_node_i = res->second;
			joint_nodes[i] = new_joint_node_i;

			AnimationNodeData& new_node = nodes[new_joint_node_i];

			if(VERBOSE)
				conPrint("old joint node index: " + toString(old_joint_node_i) + " (" + old_node.name + "), new index: " + toString(new_joint_node_i) + " (" + new_node.name + ")");

			// Our inverse bind matrix will take a point on the old mesh (VRM mesh)
			// It will transform into a basis with origin at the old bone position, but with orientation given by the new bone.
			// To find this transform, we will find the desired bind matrix (bone to model transform), then invert it.
			// This will result in an inverse bind matrix, that when concatenated with the relevant joint matrices (animated joint to model space transforms), 
			// will transform a vertex from a point on the old mesh (VRM mesh), to bone space, and then finally to model space.
			// Note that the joint matrices expect a certain orientation of the mesh data in bone space, which is why we use the new bone orientations.
			//
			// See inverse_bind_matrices.svg

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


			// Translate so that bone root is at origin,
			// do to-T-pose rotation, (note: works in object space), then translate back to mesh space
			// See using_pose_transform.svg
			Matrix4f mesh_to_pose_rot_matrix = old_node_hier_to_world_matrices[old_joint_node_i] * old_node.inverse_bind_matrix;
			mesh_to_pose_rot_matrix.setColumn(3, Vec4f(0,0,0,1)); // Remove translation part of matrix.

			const Matrix4f mesh_to_bone_origin = Matrix4f::translationMatrix(-old_node_pos);
			const Matrix4f bone_origin_to_mesh = Matrix4f::translationMatrix(old_node_pos);

			new_node.inverse_bind_matrix = desired_inverse_bind_matrix * (bone_origin_to_mesh * mesh_to_pose_rot_matrix * mesh_to_bone_origin);
		}
		else
		{
			if(VERBOSE) conPrint("!!!!! Corresponding new node not found (old_joint_node_name: " + old_node.name + ")");

			joint_nodes[i] = 0;
		}
	}


	checkDataIsValid();
	checkPerAnimNodeDataIsValid();

	this->retarget_adjustments_set = true;
}


Matrix4f AnimationData::getNodeToObjectSpaceTransform(int node_index, bool use_retarget_adjustment) const
{
	const AnimationNodeData& node_data = nodes[node_index];
		
	const Matrix4f rot_mat = node_data.rot.toMatrix();

	const Matrix4f TRS(
		rot_mat.getColumn(0) * copyToAll<0>(node_data.scale),
		rot_mat.getColumn(1) * copyToAll<1>(node_data.scale),
		rot_mat.getColumn(2) * copyToAll<2>(node_data.scale),
		setWToOne(node_data.trans));

	//const Matrix4f node_transform = (node_data.parent_index == -1) ? TRS : (node_matrices[node_data.parent_index] * (use_retarget_adjustment ? node_data.retarget_adjustment : Matrix4f::identity()) * TRS);

	if(node_data.parent_index == -1)
		return TRS;
	else
		return getNodeToObjectSpaceTransform(node_data.parent_index, use_retarget_adjustment) * 
			(use_retarget_adjustment ? node_data.retarget_adjustment : Matrix4f::identity()) * TRS;
}


Vec4f AnimationData::transformPointToNodeParentSpace(int node_index, bool use_retarget_adjustment, const Vec4f& point_node_space) const
{
	const AnimationNodeData& node_data = nodes[node_index];
		
	const Matrix4f rot_mat = node_data.rot.toMatrix();

	const Matrix4f TRS(
		rot_mat.getColumn(0) * copyToAll<0>(node_data.scale),
		rot_mat.getColumn(1) * copyToAll<1>(node_data.scale),
		rot_mat.getColumn(2) * copyToAll<2>(node_data.scale),
		setWToOne(node_data.trans));

	if(node_data.parent_index == -1)
		return TRS * point_node_space;
	else
		return transformPointToNodeParentSpace(node_data.parent_index, use_retarget_adjustment,
			(use_retarget_adjustment ? node_data.retarget_adjustment : Matrix4f::identity()) * (TRS * point_node_space));
}


Vec4f AnimationData::getNodePositionModelSpace(int node_index, bool use_retarget_adjustment) const
{
	if(node_index >= 0 && node_index < (int)nodes.size())
		return transformPointToNodeParentSpace(node_index, use_retarget_adjustment, Vec4f(0,0,0,1));
	else
	{
		assert(0);
		return Vec4f(0,0,0,1);
	}
}


Vec4f AnimationData::getNodePositionModelSpace(const std::string& name, bool use_retarget_adjustment) const
{
	for(size_t n=0; n<sorted_nodes.size(); ++n)
	{
		const int node_i = sorted_nodes[n];
		const AnimationNodeData& node_data = nodes[node_i];
		if(node_data.name == name)
			return getNodePositionModelSpace(node_i, use_retarget_adjustment);
	}

//	assert(0);
	return Vec4f(0,0,0,1);
}


size_t AnimationData::getTotalMemUsage() const
{
	size_t sum =
		nodes.capacity() * sizeof(AnimationNodeData) + 
		sorted_nodes.capacity() * sizeof(int) +
		joint_nodes.capacity() * sizeof(int);

	for(size_t i=0; i<keyframe_times.size(); ++i)
		sum += keyframe_times[i].times.capacity() * sizeof(float);

	for(size_t i=0; i<output_data.size(); ++i)
		sum += output_data[i].capacitySizeBytes();

	for(size_t i=0; i<animations.size(); ++i)
		if(animations[i]->getRefCount() == 1) // Only count animation mem usage if this is the only user of the animation data.  Don't count shared animation data. (walk anim shared by all avatars etc.)
			sum += animations[i]->getTotalMemUsage();

	for(size_t i=0; i<per_anim_node_data.size(); ++i)
		sum += per_anim_node_data[i].capacity() * sizeof(PerAnimationNodeData);

	// Skip vrm_data

	return sum;
}


#if BUILD_TESTS


#include <utils/TestUtils.h>
#include <utils/FileInStream.h>
#include <utils/PlatformUtils.h>
#include <utils/BufferViewInStream.h>
#include <utils/BufferOutStream.h>
#include "../graphics/BatchedMesh.h"


#if 0
// Command line:
// C:\fuzz_corpus\subanim c:/code/glare-core/testfiles\animations

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	Clock::init();
	return 0;
}


//static int iter = 0;
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	try
	{
		BufferViewInStream stream(ArrayRef<uint8>(data, size));

		// Skip magic number
		stream.advanceReadIndex(4);

		Reference<AnimationData> anim = new AnimationData();
		anim->readFromStream(stream);

		BufferOutStream outbuf;
		anim->writeToStream(outbuf);
	}
	catch(glare::Exception& )
	{
	}
	return 0;  // Non-zero return values are reserved for future use.
}
#endif


static inline float dequantiseSNorm(int x)
{
	 return x * (1 / 32767.f);
}


void AnimationData::test()
{
	int a = meshopt_quantizeSnorm(-1.0f, /*N=*/16);
	int b = meshopt_quantizeSnorm( 1.0f, /*N=*/16);
	int c = meshopt_quantizeSnorm( 0.0f, /*N=*/16);
	float fa = dequantiseSNorm(a);
	float fb = dequantiseSNorm(b);
	float fc = dequantiseSNorm(c);
	testAssert(fa == -1.f);
	testAssert(fb ==  1.f);
	testAssert(fc ==  0.f);


	for(int i=0; i<10; ++i)
	{
		AnimationData data;
		FileInStream file(TestUtils::getTestReposDir() + "/testfiles/animations/Idle.subanim");
		file.advanceReadIndex(4); // Skip magic number


		Timer timer;
		data.readFromStream(file);
		conPrint("Reading anim from disk took " + timer.elapsedStringMSWIthNSigFigs(4));
	}


	// Test animation retargetting
	{
		BatchedMeshRef mesh = BatchedMesh::readFromFile(TestUtils::getTestReposDir() + "/testfiles/bmesh/meebit_09842_t_solid_vrm.bmesh", NULL);

		FileInStream file(TestUtils::getTestReposDir() + "/testfiles/animations/Idle.subanim");
		file.advanceReadIndex(4); // Skip magic number
		AnimationData data;
		data.readFromStream(file);

		mesh->animation_data.loadAndRetargetAnim(data);

		testAssert(mesh->animation_data.nodes.size() == 68);
	}
}


#endif


