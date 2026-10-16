/*=====================================================================
MP4FastStart.cpp
----------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "MP4FastStart.h"


#include "../utils/Exception.h"
#include "../utils/FileUtils.h"
#include "../utils/StringUtils.h"
#include <cstring>
#include <limits>


namespace MP4FastStart
{


static inline uint32 makeFourCC(const char* s)
{
	return ((uint32)(uint8)s[0] << 24) | ((uint32)(uint8)s[1] << 16) | ((uint32)(uint8)s[2] << 8) | (uint32)(uint8)s[3];
}


static inline uint32 readBE32(const uint8* p)
{
	return ((uint32)p[0] << 24) | ((uint32)p[1] << 16) | ((uint32)p[2] << 8) | (uint32)p[3];
}


static inline uint64 readBE64(const uint8* p)
{
	return ((uint64)readBE32(p) << 32) | (uint64)readBE32(p + 4);
}


static inline void writeBE32(uint8* p, uint32 x)
{
	p[0] = (uint8)(x >> 24);
	p[1] = (uint8)(x >> 16);
	p[2] = (uint8)(x >> 8);
	p[3] = (uint8)x;
}


static inline void writeBE64(uint8* p, uint64 x)
{
	writeBE32(p, (uint32)(x >> 32));
	writeBE32(p + 4, (uint32)x);
}


struct Box
{
	uint64 offset; // Offset of the start of the box header
	uint64 size; // Total size, including the header
	uint64 header_size; // 8, or 16 if a 64-bit size is used
	uint32 type;
};


// Parses the sequence of boxes in data[start, end).
static void parseBoxes(const uint8* data, uint64 start, uint64 end, std::vector<Box>& boxes_out)
{
	boxes_out.clear();

	uint64 pos = start;
	while(pos < end)
	{
		if(end - pos < 8)
			throw glare::Exception("MP4: truncated box header");

		Box box;
		box.offset = pos;
		box.type = readBE32(data + pos + 4);

		const uint32 size32 = readBE32(data + pos);
		if(size32 == 1) // 64-bit size follows the type
		{
			if(end - pos < 16)
				throw glare::Exception("MP4: truncated box header");
			box.size = readBE64(data + pos + 8);
			box.header_size = 16;
		}
		else if(size32 == 0) // Box extends to the end of the enclosing range
		{
			box.size = end - pos;
			box.header_size = 8;
		}
		else
		{
			box.size = size32;
			box.header_size = 8;
		}

		if(box.size < box.header_size || box.size > end - pos)
			throw glare::Exception("MP4: invalid box size");

		boxes_out.push_back(box);
		pos += box.size;
	}
}


// Adjusts chunk offsets in the stco and co64 boxes in moov_data (a copy of the moov box).
// Offsets in [shifted_begin, shifted_end) are increased by shift.
static void adjustChunkOffsets(uint8* moov_data, uint64 begin, uint64 end, uint64 shifted_begin, uint64 shifted_end, uint64 shift, int depth)
{
	if(depth > 8)
		throw glare::Exception("MP4: boxes nested too deeply");

	std::vector<Box> boxes;
	parseBoxes(moov_data, begin, end, boxes);

	for(size_t i=0; i<boxes.size(); ++i)
	{
		const Box& box = boxes[i];
		const uint64 payload_begin = box.offset + box.header_size;
		const uint64 payload_end   = box.offset + box.size;

		// The boxes on the path from moov to stco/co64: moov/trak/mdia/minf/stbl
		if(box.type == makeFourCC("trak") || box.type == makeFourCC("mdia") || box.type == makeFourCC("minf") || box.type == makeFourCC("stbl"))
		{
			adjustChunkOffsets(moov_data, payload_begin, payload_end, shifted_begin, shifted_end, shift, depth + 1);
		}
		else if(box.type == makeFourCC("stco") || box.type == makeFourCC("co64"))
		{
			// Full box: version (1 byte), flags (3 bytes), entry count (4 bytes), then entries.
			if(payload_end - payload_begin < 8)
				throw glare::Exception("MP4: truncated chunk offset box");

			const bool is_64_bit = box.type == makeFourCC("co64");
			const uint64 entry_size = is_64_bit ? 8 : 4;
			const uint64 num_entries = readBE32(moov_data + payload_begin + 4);
			if(num_entries > (payload_end - payload_begin - 8) / entry_size)
				throw glare::Exception("MP4: chunk offset box entry count too large");

			uint8* entries = moov_data + payload_begin + 8;
			for(uint64 z=0; z<num_entries; ++z)
			{
				uint8* entry = entries + z * entry_size;
				const uint64 offset = is_64_bit ? readBE64(entry) : readBE32(entry);
				if(offset >= shifted_begin && offset < shifted_end)
				{
					const uint64 new_offset = offset + shift;
					if(is_64_bit)
						writeBE64(entry, new_offset);
					else
					{
						if(new_offset > std::numeric_limits<uint32>::max())
							throw glare::Exception("MP4: chunk offset too large for stco box after moving moov");
						writeBE32(entry, (uint32)new_offset);
					}
				}
			}
		}
	}
}


bool makeFastStart(const uint8* data, size_t data_size, std::vector<uint8>& data_out)
{
	std::vector<Box> top_level_boxes;
	parseBoxes(data, 0, data_size, top_level_boxes);

	const Box* moov = nullptr;
	const Box* first_mdat = nullptr;
	for(size_t i=0; i<top_level_boxes.size(); ++i)
	{
		const Box& box = top_level_boxes[i];
		if(box.type == makeFourCC("moov"))
		{
			if(moov)
				throw glare::Exception("MP4: more than one moov box");
			moov = &box;
		}
		else if(box.type == makeFourCC("mdat"))
		{
			if(!first_mdat)
				first_mdat = &box;
		}
		else if(box.type == makeFourCC("moof"))
			throw glare::Exception("MP4: fragmented MP4 files are not supported");
	}

	if(!moov)
		throw glare::Exception("MP4: no moov box");
	if(!first_mdat)
		throw glare::Exception("MP4: no mdat box");

	if(moov->offset < first_mdat->offset)
		return false; // Already faststart.

	// moov will be inserted where the first mdat currently starts.  Everything from there up to the old moov position moves
	// later by the size of moov.  Anything after the old moov position stays where it is.
	const uint64 insert_pos = first_mdat->offset;
	const uint64 old_moov_pos = moov->offset;
	const uint64 moov_size = moov->size;

	std::vector<uint8> moov_data(data + old_moov_pos, data + old_moov_pos + moov_size);

	// A size field of 0 means the box extends to the end of the file, which is only valid for the last box.  Write the explicit size, since moov won't be last after moving.
	if(readBE32(moov_data.data()) == 0)
	{
		if(moov_size > std::numeric_limits<uint32>::max())
			throw glare::Exception("MP4: moov box with size 0 is too large for a 32-bit size field");
		writeBE32(moov_data.data(), (uint32)moov_size);
	}

	adjustChunkOffsets(moov_data.data(), /*begin=*/moov->header_size, /*end=*/moov_size, /*shifted_begin=*/insert_pos, /*shifted_end=*/old_moov_pos, /*shift=*/moov_size, /*depth=*/0);

	data_out.resize(data_size);
	uint8* out = data_out.data();
	std::memcpy(out, data, insert_pos); // Boxes before the first mdat, e.g. ftyp
	out += insert_pos;
	std::memcpy(out, moov_data.data(), moov_size);
	out += moov_size;
	std::memcpy(out, data + insert_pos, old_moov_pos - insert_pos); // mdat and anything else up to the old moov position
	out += old_moov_pos - insert_pos;
	std::memcpy(out, data + old_moov_pos + moov_size, data_size - (old_moov_pos + moov_size)); // Anything after the old moov position

	return true;
}


bool makeFileFastStart(const std::string& path)
{
	try
	{
		std::vector<uint8> data;
		FileUtils::readEntireFile(path, data);

		std::vector<uint8> new_data;
		if(!makeFastStart(data.data(), data.size(), new_data))
			return false;

		// Write to a temp file then move over the original, so the original isn't left half-written if writing fails.
		const std::string temp_path = path + ".faststart_temp";
		FileUtils::writeEntireFile(temp_path, new_data);
		FileUtils::moveFile(temp_path, path);
		return true;
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw glare::Exception(e.what());
	}
}


} // end namespace MP4FastStart


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/ConPrint.h"


static void appendBE32(std::vector<uint8>& v, uint32 x)
{
	v.push_back((uint8)(x >> 24));
	v.push_back((uint8)(x >> 16));
	v.push_back((uint8)(x >> 8));
	v.push_back((uint8)x);
}


static void appendBox(std::vector<uint8>& v, const char* type, const std::vector<uint8>& payload)
{
	appendBE32(v, (uint32)(8 + payload.size()));
	v.insert(v.end(), type, type + 4);
	v.insert(v.end(), payload.begin(), payload.end());
}


static std::vector<uint8> makeBox(const char* type, const std::vector<uint8>& payload)
{
	std::vector<uint8> v;
	appendBox(v, type, payload);
	return v;
}


// Makes a minimal MP4 file: ftyp, mdat, then moov with one trak whose stco box points at the two halves of the mdat payload.
static std::vector<uint8> makeTestFile(const std::vector<uint8>& mdat_payload)
{
	std::vector<uint8> file;

	std::vector<uint8> ftyp_payload = { 'i', 's', 'o', 'm', 0, 0, 2, 0 };
	appendBox(file, "ftyp", ftyp_payload);

	const uint32 mdat_payload_offset = (uint32)file.size() + 8;
	appendBox(file, "mdat", mdat_payload);

	std::vector<uint8> stco_payload;
	appendBE32(stco_payload, 0); // version and flags
	appendBE32(stco_payload, 2); // entry count
	appendBE32(stco_payload, mdat_payload_offset);
	appendBE32(stco_payload, mdat_payload_offset + (uint32)mdat_payload.size() / 2);

	const std::vector<uint8> moov = makeBox("moov", makeBox("trak", makeBox("mdia", makeBox("minf", makeBox("stbl", makeBox("stco", stco_payload))))));
	file.insert(file.end(), moov.begin(), moov.end());
	return file;
}


// Returns the offsets in the first stco box found in the file.  Assumes the layout made by makeTestFile.
static std::vector<uint32> getStcoOffsets(const std::vector<uint8>& file)
{
	for(size_t i=0; i + 4 <= file.size(); ++i)
		if(std::memcmp(file.data() + i, "stco", 4) == 0)
		{
			const uint32 num = MP4FastStart::readBE32(file.data() + i + 8);
			std::vector<uint32> offsets;
			for(uint32 z=0; z<num; ++z)
				offsets.push_back(MP4FastStart::readBE32(file.data() + i + 12 + z * 4));
			return offsets;
		}
	failTest("stco not found");
	return std::vector<uint32>();
}


void MP4FastStart::test()
{
	conPrint("MP4FastStart::test()");

	const std::vector<uint8> mdat_payload = { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25 };

	//-------------------- Test moving moov before mdat --------------------
	{
		const std::vector<uint8> file = makeTestFile(mdat_payload);

		std::vector<uint8> out;
		testAssert(makeFastStart(file.data(), file.size(), out));
		testAssert(out.size() == file.size());

		// moov should now directly follow ftyp (16 bytes)
		testAssert(std::memcmp(out.data() + 16 + 4, "moov", 4) == 0);

		// The chunk offsets should point at the same mdat payload bytes as before.
		const std::vector<uint32> old_offsets = getStcoOffsets(file);
		const std::vector<uint32> new_offsets = getStcoOffsets(out);
		testAssert(old_offsets.size() == 2 && new_offsets.size() == 2);
		for(size_t i=0; i<2; ++i)
		{
			testAssert(new_offsets[i] > old_offsets[i]);
			testAssert(std::memcmp(out.data() + new_offsets[i], file.data() + old_offsets[i], mdat_payload.size() / 2) == 0);
		}

		// Running again should do nothing, as moov is now before mdat.
		std::vector<uint8> out2;
		testAssert(!makeFastStart(out.data(), out.size(), out2));
	}

	//-------------------- Test a moov box with size 0 (extends to the end of the file) --------------------
	{
		const std::vector<uint8> file = makeTestFile(mdat_payload);

		std::vector<uint8> expected_out;
		testAssert(makeFastStart(file.data(), file.size(), expected_out));

		// moov is the last box in the test file, after ftyp (16 bytes) and mdat (8 bytes + payload).
		const size_t moov_offset = 16 + 8 + mdat_payload.size();
		testAssert(std::memcmp(file.data() + moov_offset + 4, "moov", 4) == 0);

		std::vector<uint8> zero_size_file = file;
		writeBE32(zero_size_file.data() + moov_offset, 0);

		// The moved moov should get its explicit size, giving the same output as for the original file.
		std::vector<uint8> out;
		testAssert(makeFastStart(zero_size_file.data(), zero_size_file.size(), out));
		testAssert(out == expected_out);
	}

	//-------------------- Test malformed input --------------------
	{
		const std::vector<uint8> file = makeTestFile(mdat_payload);

		// Every truncation should either throw or be handled, but not crash.
		for(size_t len=0; len<file.size(); ++len)
		{
			try
			{
				std::vector<uint8> out;
				makeFastStart(file.data(), len, out);
			}
			catch(glare::Exception&)
			{}
		}

		// Corrupt the stco entry count
		std::vector<uint8> bad = file;
		for(size_t i=0; i + 4 <= bad.size(); ++i)
			if(std::memcmp(bad.data() + i, "stco", 4) == 0)
				writeBE32(bad.data() + i + 8, 1000000);
		try
		{
			std::vector<uint8> out;
			makeFastStart(bad.data(), bad.size(), out);
			failTest("Expected exception");
		}
		catch(glare::Exception&)
		{}
	}

	conPrint("MP4FastStart::test() done.");
}


#endif // BUILD_TESTS
