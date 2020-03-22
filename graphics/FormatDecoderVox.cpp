/*=====================================================================
FormatDecoderVox.cpp
--------------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#include "FormatDecoderVox.h"


#include "../utils/StringUtils.h"
#include "../utils/Timer.h"
#include "../utils/ConPrint.h"
#include "../utils/FileUtils.h"
#include "../utils/MemMappedFile.h"
#include "../utils/Exception.h"
#include <unordered_map>


static const std::string currentChunkID(const uint8* data, size_t datalen, size_t cur_i)
{
	if(cur_i + 4 > datalen)
		throw Indigo::Exception("EOF while parsing chunk ID.");
	std::string res(4, '\0');
	std::memcpy(&res[0], &data[cur_i], 4);
	return res;
}


static const std::string parseChunkID(const uint8* data, size_t datalen, size_t& cur_i)
{
	if(cur_i + 4 > datalen)
		throw Indigo::Exception("EOF while parsing chunk ID.");
	std::string res(4, '\0');
	std::memcpy(&res[0], &data[cur_i], 4);
	cur_i += 4;
	return res;
}


static int32 parseInt32(const uint8* data, size_t datalen, size_t& cur_i)
{
	if(cur_i + 4 > datalen)
		throw Indigo::Exception("EOF while parsing int.");
	int32 res;
	std::memcpy(&res, &data[cur_i], 4);
	cur_i += 4;
	return res;
}


/*static float parseFloat(const uint8* data, size_t datalen, size_t& cur_i)
{
	if(cur_i + 4 > datalen)
		throw Indigo::Exception("EOF while parsing float.");
	float res;
	std::memcpy(&res, &data[cur_i], 4);
	cur_i += 4;
	return res;
}*/


static void parseSizeChunk(const uint8* data, size_t datalen, size_t& cur_i, VoxModel& model)
{
	model.size_x = parseInt32(data, datalen, cur_i);
	model.size_y = parseInt32(data, datalen, cur_i);
	model.size_z = parseInt32(data, datalen, cur_i);
}


static void parseXYZIChunk(const uint8* data, size_t datalen, size_t& cur_i, VoxModel& model)
{
	const int32 num_voxels = parseInt32(data, datalen, cur_i);
	if(num_voxels <= 0)
		throw Indigo::Exception("Invalid num voxels (" + toString(num_voxels) + ").");
	
	// Parse 4 * N bytes, 4 for each voxel.  Check we have enough file left.
	if(cur_i + 4 * (size_t)num_voxels > datalen)
		throw Indigo::Exception("EOF while parsing XYZI chunk.");

	model.voxels.resize(num_voxels);

	size_t byte_i = cur_i;
	for(int i=0; i<num_voxels; ++i)
	{
		model.voxels[i].x             = data[byte_i++];
		model.voxels[i].y             = data[byte_i++];
		model.voxels[i].z             = data[byte_i++];
		model.voxels[i].palette_index = data[byte_i++];
		assert(model.voxels[i].palette_index >= 0 && model.voxels[i].palette_index < 256);
	}

	cur_i += 4 * (size_t)num_voxels;
}


static void parseRGBAChunk(const uint8* data, size_t datalen, size_t& cur_i, VoxFileContents& contents)
{
	contents.palette.resize(256);

	// Parse 4 * 256 bytes.  Check we have enough file left.
	if(cur_i + 4 * 256 > datalen)
		throw Indigo::Exception("EOF while parsing RGBA chunk.");

	// Palette indices are offset by 1 in vox format for some reason.
	size_t byte_i = cur_i;
	for(int i=1; i<256; ++i)
	{
		contents.palette[i][0] = data[byte_i++] * (1.f / 255.f);
		contents.palette[i][1] = data[byte_i++] * (1.f / 255.f);
		contents.palette[i][2] = data[byte_i++] * (1.f / 255.f);
		contents.palette[i][3] = data[byte_i++] * (1.f / 255.f);
	}

	cur_i += 4 * 256;
}


// Disabled for now, as this chunk type is deprecated, and is not exported by MagicaVoxel currently, so is hard to test.
// TODO: check parsing of this chunk.
// Not sure am handling properties the right way.
// static void parseMATTChunk(const uint8* data, size_t datalen, size_t& cur_i, VoxMaterial& material)
// {
// 	material.id = parseInt32(data, datalen, cur_i); // TODO: do something with this id
// 	const int32 type = parseInt32(data, datalen, cur_i);
// 	if(type < 0 || type >= 4)
// 		throw Indigo::Exception("Unhandled material type " + toString(type) + ".");
// 	material.type = (VoxMaterial::Type)type;
// 	material.weight = parseFloat(data, datalen, cur_i);
// 
// 	const int32 properties = parseInt32(data, datalen, cur_i);
// 	if((properties >> 0) & 0x1) 
// 		material.plastic = parseFloat(data, datalen, cur_i);
// 	if((properties >> 1) & 0x1)
// 		material.roughness = parseFloat(data, datalen, cur_i);
// 	if((properties >> 2) & 0x1)
// 		material.specular = parseFloat(data, datalen, cur_i);
// 	if((properties >> 3) & 0x1)
// 		material.IOR = parseFloat(data, datalen, cur_i);
// 	if((properties >> 4) & 0x1)
// 		material.attenuation = parseFloat(data, datalen, cur_i);
// 	if((properties >> 5) & 0x1)
// 		material.power = parseFloat(data, datalen, cur_i);
// 	if((properties >> 6) & 0x1)
// 		material.glow = parseFloat(data, datalen, cur_i);
// 	if((properties >> 7) & 0x1)
// 		/*totalPower=*/ parseFloat(data, datalen, cur_i);
// }


static unsigned int default_palette[256] = {
	0x00000000, 0xffffffff, 0xffccffff, 0xff99ffff, 0xff66ffff, 0xff33ffff, 0xff00ffff, 0xffffccff, 0xffccccff, 0xff99ccff, 0xff66ccff, 0xff33ccff, 0xff00ccff, 0xffff99ff, 0xffcc99ff, 0xff9999ff,
	0xff6699ff, 0xff3399ff, 0xff0099ff, 0xffff66ff, 0xffcc66ff, 0xff9966ff, 0xff6666ff, 0xff3366ff, 0xff0066ff, 0xffff33ff, 0xffcc33ff, 0xff9933ff, 0xff6633ff, 0xff3333ff, 0xff0033ff, 0xffff00ff,
	0xffcc00ff, 0xff9900ff, 0xff6600ff, 0xff3300ff, 0xff0000ff, 0xffffffcc, 0xffccffcc, 0xff99ffcc, 0xff66ffcc, 0xff33ffcc, 0xff00ffcc, 0xffffcccc, 0xffcccccc, 0xff99cccc, 0xff66cccc, 0xff33cccc,
	0xff00cccc, 0xffff99cc, 0xffcc99cc, 0xff9999cc, 0xff6699cc, 0xff3399cc, 0xff0099cc, 0xffff66cc, 0xffcc66cc, 0xff9966cc, 0xff6666cc, 0xff3366cc, 0xff0066cc, 0xffff33cc, 0xffcc33cc, 0xff9933cc,
	0xff6633cc, 0xff3333cc, 0xff0033cc, 0xffff00cc, 0xffcc00cc, 0xff9900cc, 0xff6600cc, 0xff3300cc, 0xff0000cc, 0xffffff99, 0xffccff99, 0xff99ff99, 0xff66ff99, 0xff33ff99, 0xff00ff99, 0xffffcc99,
	0xffcccc99, 0xff99cc99, 0xff66cc99, 0xff33cc99, 0xff00cc99, 0xffff9999, 0xffcc9999, 0xff999999, 0xff669999, 0xff339999, 0xff009999, 0xffff6699, 0xffcc6699, 0xff996699, 0xff666699, 0xff336699,
	0xff006699, 0xffff3399, 0xffcc3399, 0xff993399, 0xff663399, 0xff333399, 0xff003399, 0xffff0099, 0xffcc0099, 0xff990099, 0xff660099, 0xff330099, 0xff000099, 0xffffff66, 0xffccff66, 0xff99ff66,
	0xff66ff66, 0xff33ff66, 0xff00ff66, 0xffffcc66, 0xffcccc66, 0xff99cc66, 0xff66cc66, 0xff33cc66, 0xff00cc66, 0xffff9966, 0xffcc9966, 0xff999966, 0xff669966, 0xff339966, 0xff009966, 0xffff6666,
	0xffcc6666, 0xff996666, 0xff666666, 0xff336666, 0xff006666, 0xffff3366, 0xffcc3366, 0xff993366, 0xff663366, 0xff333366, 0xff003366, 0xffff0066, 0xffcc0066, 0xff990066, 0xff660066, 0xff330066,
	0xff000066, 0xffffff33, 0xffccff33, 0xff99ff33, 0xff66ff33, 0xff33ff33, 0xff00ff33, 0xffffcc33, 0xffcccc33, 0xff99cc33, 0xff66cc33, 0xff33cc33, 0xff00cc33, 0xffff9933, 0xffcc9933, 0xff999933,
	0xff669933, 0xff339933, 0xff009933, 0xffff6633, 0xffcc6633, 0xff996633, 0xff666633, 0xff336633, 0xff006633, 0xffff3333, 0xffcc3333, 0xff993333, 0xff663333, 0xff333333, 0xff003333, 0xffff0033,
	0xffcc0033, 0xff990033, 0xff660033, 0xff330033, 0xff000033, 0xffffff00, 0xffccff00, 0xff99ff00, 0xff66ff00, 0xff33ff00, 0xff00ff00, 0xffffcc00, 0xffcccc00, 0xff99cc00, 0xff66cc00, 0xff33cc00,
	0xff00cc00, 0xffff9900, 0xffcc9900, 0xff999900, 0xff669900, 0xff339900, 0xff009900, 0xffff6600, 0xffcc6600, 0xff996600, 0xff666600, 0xff336600, 0xff006600, 0xffff3300, 0xffcc3300, 0xff993300,
	0xff663300, 0xff333300, 0xff003300, 0xffff0000, 0xffcc0000, 0xff990000, 0xff660000, 0xff330000, 0xff0000ee, 0xff0000dd, 0xff0000bb, 0xff0000aa, 0xff000088, 0xff000077, 0xff000055, 0xff000044,
	0xff000022, 0xff000011, 0xff00ee00, 0xff00dd00, 0xff00bb00, 0xff00aa00, 0xff008800, 0xff007700, 0xff005500, 0xff004400, 0xff002200, 0xff001100, 0xffee0000, 0xffdd0000, 0xffbb0000, 0xffaa0000,
	0xff880000, 0xff770000, 0xff550000, 0xff440000, 0xff220000, 0xff110000, 0xffeeeeee, 0xffdddddd, 0xffbbbbbb, 0xffaaaaaa, 0xff888888, 0xff777777, 0xff555555, 0xff444444, 0xff222222, 0xff111111
};


// Returns total remaining chunk size, including children chunks
static size_t parseRestOfChunkHeader(const uint8* data, size_t datalen, size_t& cur_i)
{
	const int chunk_content_B   = parseInt32(data, datalen, cur_i);
	const int children_chunks_B = parseInt32(data, datalen, cur_i);
	if(chunk_content_B < 0 || children_chunks_B < 0)
		throw Indigo::Exception("Invalid chunk header, size(s) were < 0.");

	return (size_t)chunk_content_B + (size_t)children_chunks_B;
}


// Advance cur_i until it indexes just past the chunk.
static void skipPastChunk(size_t datalen, size_t chunk_end, size_t& cur_i)
{
	if(cur_i > chunk_end)
		throw Indigo::Exception("Parsed past end of chunk as reported in chunk header.");
	cur_i = chunk_end;
	if(cur_i > datalen)
		throw Indigo::Exception("Reported end of chunk was past end of file.");
}


void FormatDecoderVox::loadModel(const std::string& filename, VoxFileContents& contents_out) // Throws Indigo::Exception on failure.
{
	MemMappedFile file(filename);

	const uint8* data = (const uint8*)file.fileData();
	const size_t datalen = file.fileSize();

	loadModelFromData(data, datalen, contents_out);
}


void FormatDecoderVox::loadModelFromData(const uint8* data, const size_t datalen, VoxFileContents& contents_out) // Throws Indigo::Exception on failure.
{
	size_t cur_i = 0;

	const std::string vox_string = parseChunkID(data, datalen, cur_i);
	if(vox_string != "VOX ")
		throw Indigo::Exception("Header was != \"VOX \".");

	contents_out.version = parseInt32(data, datalen, cur_i);

	// Parse MAIN chunk
	const std::string main_chunk_id = parseChunkID(data, datalen, cur_i);
	if(main_chunk_id == "MAIN")
	{
		// Parse chunk header
		const size_t main_chunk_size = parseRestOfChunkHeader(data, datalen, cur_i);
		const size_t main_chunk_end = cur_i + main_chunk_size;

		int numModels = 1;
		bool parsed_RGBA_chunk = false;

		std::string chunk_id;

		// Handle optional PACK chunk (if it is absent, only one model in the file)
		if(currentChunkID(data, datalen, cur_i) == "PACK")
		{
			chunk_id = parseChunkID(data, datalen, cur_i);
			assert(chunk_id == "PACK");

			const size_t chunk_size = parseRestOfChunkHeader(data, datalen, cur_i);
			const size_t chunk_end = cur_i + chunk_size;
			
			numModels = parseInt32(data, datalen, cur_i);
			if(numModels < 1)
				throw Indigo::Exception("numModels was < 1.");
			
			skipPastChunk(datalen, chunk_end, cur_i); // Skip rest of chunk (if any unparsed)
		}

		// Parse next chunks
		while(cur_i < main_chunk_end)
		{
			chunk_id = parseChunkID(data, datalen, cur_i);
			const size_t chunk_size = parseRestOfChunkHeader(data, datalen, cur_i);
			const size_t chunk_end = cur_i + chunk_size;
			if(chunk_id == "SIZE")
			{
				contents_out.models.push_back(VoxModel());

				parseSizeChunk(data, datalen, cur_i, contents_out.models.back());
			}
			else if(chunk_id == "XYZI")
			{
				if(contents_out.models.empty())
					throw Indigo::Exception("XYZI chunk before any SIZE chunk.");

				parseXYZIChunk(data, datalen, cur_i, contents_out.models.back());
			}
			else if(chunk_id == "RGBA")
			{
				parseRGBAChunk(data, datalen, cur_i, contents_out);
				parsed_RGBA_chunk = true;
			}
			/*else if(chunk_id == "MATT")
			{
				contents_out.materials.push_back(VoxMaterial());

				parseMATTChunk(data, datalen, cur_i, contents_out.materials.back());
			}*/

			skipPastChunk(datalen, chunk_end, cur_i); // Skip rest of chunk (if any unparsed)
		}

		if(!parsed_RGBA_chunk)
		{
			// Load default palette
			contents_out.palette.resize(256);

			for(int i=0; i<256; ++i)
			{
				// Red is least significant byte.
				contents_out.palette[i][0] = ((default_palette[i] >>  0) & 0xFF) * (1.f / 255.f);
				contents_out.palette[i][1] = ((default_palette[i] >>  8) & 0xFF) * (1.f / 255.f);
				contents_out.palette[i][2] = ((default_palette[i] >> 16) & 0xFF) * (1.f / 255.f);
				contents_out.palette[i][3] = 1; // All alpha values are 1 in default palette.
			}
		}
	}
	else
		throw Indigo::Exception("Expected 'MAIN' chunk.");

	// Work out which materials are used:
	std::vector<bool> mat_used(256, false);
	for(size_t z=0; z<contents_out.models.size(); ++z)
	{
		const VoxModel& model = contents_out.models[z];
		for(size_t i=0; i<model.voxels.size(); ++i)
			mat_used[model.voxels[i].palette_index] = true;
	}

	int mat_final_index[256];
	int mat_i = 0;
	for(size_t i=0; i<256; ++i)
	{
		if(mat_used[i])
		{
			mat_final_index[i] = mat_i;
			mat_i++;

			//if(i < contents_out.materials.size())
			//	contents_out.used_materials.push_back(contents_out.materials[i]);
			//else
			{ 
				// Use default diffuse material.
				contents_out.used_materials.push_back(VoxMaterial());
				contents_out.used_materials.back().type = VoxMaterial::Type_Diffuse;
			}

			contents_out.used_materials.back().col_from_palette = contents_out.palette[i];
		}
		else
			mat_final_index[i] = -1;
	}

	// Go back over voxels and compute mat_index.
	for(size_t z=0; z<contents_out.models.size(); ++z)
	{
		VoxModel& model = contents_out.models[z];
		for(size_t i=0; i<model.voxels.size(); ++i)
		{
			assert(mat_final_index[model.voxels[i].palette_index] >= 0);
			model.voxels[i].mat_index = mat_final_index[model.voxels[i].palette_index];
		}
	}

	// Compute AABBs of models
	{
		for(size_t z=0; z<contents_out.models.size(); ++z)
		{
			VoxModel& model = contents_out.models[z];
			js::AABBox box = js::AABBox::emptyAABBox();
			for(size_t i=0; i<model.voxels.size(); ++i)
				box.enlargeToHoldPoint(Vec4f((float)model.voxels[i].x, (float)model.voxels[i].y, (float)model.voxels[i].z, 1.f));

			model.aabb = js::AABBox(box.min_, box.max_ + Vec4f(1, 1, 1, 0));
		}
	}
}


bool FormatDecoderVox::isValidVoxFile(const std::string& filename)
{
	MemMappedFile file(filename);

	return currentChunkID((const uint8*)file.fileData(), file.fileSize(), 0) == "VOX ";
}


#if BUILD_TESTS


extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	VoxFileContents contents;
	FormatDecoderVox::loadModelFromData(data, size, contents);

	return 0;  // Non-zero return values are reserved for future use.
}

#include "../indigo/TestUtils.h"
#include "../utils/FileUtils.h"


void FormatDecoderVox::test()
{
	conPrint("FormatDecoderVox::test()");

	try
	{
		{
			const std::string path = TestUtils::getIndigoTestReposDir() + "/testfiles/vox/2-2-0.vox"; // Has a PACK chunk

			VoxFileContents contents;

			loadModel(path, contents);

			testAssert(contents.version == 150);
			testAssert(contents.palette.size() == 256);
			testAssert(contents.models.size() == 1);
			testAssert(contents.models[0].voxels.size() == 1885);
		}
		{
			const std::string path = TestUtils::getIndigoTestReposDir() + "/testfiles/vox/teapot.vox";

			VoxFileContents contents;

			loadModel(path, contents);

			testAssert(contents.version == 150);
			testAssert(contents.palette.size() == 256);
			testAssert(contents.models.size() == 1);
			testAssert(contents.models[0].voxels.size() == 28411);
		}
		{
			const std::string path = TestUtils::getIndigoTestReposDir() + "/testfiles/vox/vox_with_metal_mat.vox";

			VoxFileContents contents;

			loadModel(path, contents);

			testAssert(contents.version == 150);
			testAssert(contents.palette.size() == 256);
			testAssert(contents.models.size() == 1);
		}
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
}


#endif // BUILD_TESTS
