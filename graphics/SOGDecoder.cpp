/*=====================================================================
SOGDecoder.cpp
--------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "SOGDecoder.h"


#include "ImageMap.h"
#include "WebPDecoder.h"
#include "../maths/mathstypes.h"
#include "../utils/Exception.h"
#include "../utils/JSONParser.h"
#include "../utils/MemMappedFile.h"
#include "../utils/StringUtils.h"
#include "../utils/Vector.h"
#include <cmath>
#include <cstring>
#include <map>
#include <string>


// ZIP entries in a .sog file may be DEFLATE-compressed, so we need an inflate implementation.
//
// NOTE: WUFFS_IMPLEMENTATION is deliberately not defined here.  graphics/PNGDecoder.cpp already includes this same
// amalgamated file with WUFFS_IMPLEMENTATION and WUFFS_CONFIG__MODULE__DEFLATE (it needs DEFLATE for PNG's zlib-wrapped
// streams), and the function bodies and error-message globals that emits have external linkage, so defining
// WUFFS_IMPLEMENTATION a second time here gives duplicate-symbol link errors.  Including the file without it pulls in
// just the declarations, which link against the definitions PNGDecoder.cpp's translation unit already provides - the
// usual declare-in-many-places, define-in-one-place split, done with a macro because Wuffs ships as a single .c file.
#if !WUFFS_SUPPORT
#error WUFFS_SUPPORT should be defined in preprocessor defs.  SOGDecoder needs Wuffs' DEFLATE decoder, whose definitions come from PNGDecoder.cpp.
#endif
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__DEFLATE
#include "wuffs/wuffs-v0.3.c"


namespace
{


//------------------------------------------------------------------------------------------
// Just enough of a ZIP reader to get the named entries out of a SOG bundle.
//------------------------------------------------------------------------------------------

inline uint16 readU16LE(const uint8* p) { return (uint16)(p[0] | (p[1] << 8)); }
inline uint32 readU32LE(const uint8* p) { return (uint32)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32)p[3] << 24)); }


const uint32 END_OF_CENTRAL_DIR_SIG   = 0x06054b50;
const uint32 CENTRAL_DIR_FILE_HDR_SIG = 0x02014b50;
const uint32 LOCAL_FILE_HDR_SIG       = 0x04034b50;

const uint16 COMPRESSION_STORE   = 0;
const uint16 COMPRESSION_DEFLATE = 8;

const size_t EOCD_RECORD_SIZE           = 22;
const size_t CENTRAL_DIR_ENTRY_HDR_SIZE = 46;
const size_t LOCAL_FILE_HDR_SIZE        = 30;


struct ZipEntry
{
	size_t data_offset; // Offset in the .sog buffer of the entry's (possibly compressed) data.
	size_t compressed_size;
	size_t uncompressed_size;
	uint16 compression_method;
};


// Note that this just indexes the archive - it doesn't decompress anything, so entries we don't care about
// (e.g. the shN images, which we don't decode) cost nothing beyond their central-directory entry.
typedef std::map<std::string, ZipEntry> ZipIndex;


ZipIndex readZipIndex(const uint8* data, size_t size)
{
	if(size < EOCD_RECORD_SIZE)
		throw glare::Exception("SOGDecoder: file is too small to be a valid ZIP archive.");

	// Find the End Of Central Directory record by scanning backwards.  It's a fixed-size record plus an optional
	// trailing comment of up to 65535 bytes, so we only need to scan the last ~64 KB.
	const size_t scan_start = (size > EOCD_RECORD_SIZE + 65535) ? (size - EOCD_RECORD_SIZE - 65535) : 0;
	size_t eocd_offset = 0;
	bool found_eocd = false;
	for(size_t i=size - EOCD_RECORD_SIZE; ; --i)
	{
		if(readU32LE(data + i) == END_OF_CENTRAL_DIR_SIG)
		{
			eocd_offset = i;
			found_eocd = true;
			break;
		}
		if(i == scan_start)
			break;
	}
	if(!found_eocd)
		throw glare::Exception("SOGDecoder: could not find the End Of Central Directory record - not a valid ZIP archive.");

	const uint16 num_entries      = readU16LE(data + eocd_offset + 10);
	const uint32 central_dir_size = readU32LE(data + eocd_offset + 12);
	const uint32 central_dir_off  = readU32LE(data + eocd_offset + 16);

	// These sentinel values mean the real values live in a ZIP64 record, which we don't parse.
	if(num_entries == 0xFFFF || central_dir_size == 0xFFFFFFFF || central_dir_off == 0xFFFFFFFF)
		throw glare::Exception("SOGDecoder: ZIP64 archives are not supported.");

	if((size_t)central_dir_off + central_dir_size > size)
		throw glare::Exception("SOGDecoder: ZIP central directory offset/size is out of range.");

	ZipIndex index;

	size_t cursor = central_dir_off;
	for(uint16 entry_i=0; entry_i<num_entries; ++entry_i)
	{
		if(cursor + CENTRAL_DIR_ENTRY_HDR_SIZE > size)
			throw glare::Exception("SOGDecoder: truncated ZIP central directory entry.");
		if(readU32LE(data + cursor) != CENTRAL_DIR_FILE_HDR_SIG)
			throw glare::Exception("SOGDecoder: bad ZIP central directory file header signature.");

		const uint16 compression_method = readU16LE(data + cursor + 10);
		const uint32 compressed_size    = readU32LE(data + cursor + 20);
		const uint32 uncompressed_size  = readU32LE(data + cursor + 24);
		const uint16 filename_len       = readU16LE(data + cursor + 28);
		const uint16 extra_len          = readU16LE(data + cursor + 30);
		const uint16 comment_len        = readU16LE(data + cursor + 32);
		const uint32 local_hdr_off      = readU32LE(data + cursor + 42);

		if(cursor + CENTRAL_DIR_ENTRY_HDR_SIZE + filename_len > size)
			throw glare::Exception("SOGDecoder: truncated filename in ZIP central directory entry.");

		const std::string filename((const char*)(data + cursor + CENTRAL_DIR_ENTRY_HDR_SIZE), filename_len);

		cursor += CENTRAL_DIR_ENTRY_HDR_SIZE + filename_len + extra_len + comment_len;

		if(!filename.empty() && filename[filename.size() - 1] == '/') // Skip directory entries.
			continue;

		// The entry data starts after the local file header, whose 'extra' field can be a different length from the
		// central directory's, so we have to read the local header to find it.
		if((size_t)local_hdr_off + LOCAL_FILE_HDR_SIZE > size)
			throw glare::Exception("SOGDecoder: ZIP local file header offset out of range for '" + filename + "'.");
		if(readU32LE(data + local_hdr_off) != LOCAL_FILE_HDR_SIG)
			throw glare::Exception("SOGDecoder: bad ZIP local file header signature for '" + filename + "'.");

		const uint16 local_filename_len = readU16LE(data + local_hdr_off + 26);
		const uint16 local_extra_len    = readU16LE(data + local_hdr_off + 28);

		ZipEntry entry;
		entry.data_offset        = (size_t)local_hdr_off + LOCAL_FILE_HDR_SIZE + local_filename_len + local_extra_len;
		entry.compressed_size    = compressed_size;
		entry.uncompressed_size  = uncompressed_size;
		entry.compression_method = compression_method;

		if(entry.data_offset + entry.compressed_size > size)
			throw glare::Exception("SOGDecoder: ZIP entry data out of range for '" + filename + "'.");

		index[filename] = entry;
	}

	return index;
}


// Inflates a raw DEFLATE stream (RFC 1951).  ZIP entries store the deflate stream directly, with none of the
// header/checksum framing that zlib (RFC 1950) adds.
void inflateRaw(const uint8* compressed_data, size_t compressed_size, js::Vector<uint8, 16>& decompressed_out, const std::string& filename)
{
	// alloc() heap-allocates using the real struct size, which is only known in the translation unit that defines
	// WUFFS_IMPLEMENTATION (PNGDecoder.cpp) - this TU's view of the struct is deliberately not the real one - so we
	// can't put a decoder on the stack here.  alloc() calls initialize() internally.
	wuffs_deflate__decoder::unique_ptr decoder = wuffs_deflate__decoder::alloc();
	if(!decoder)
		throw glare::Exception("SOGDecoder: failed to allocate DEFLATE decoder for '" + filename + "'.");

	wuffs_base__io_buffer src = wuffs_base__ptr_u8__reader(const_cast<uint8*>(compressed_data), compressed_size, /*closed=*/true);
	wuffs_base__io_buffer dst = wuffs_base__ptr_u8__writer(decompressed_out.data(), decompressed_out.size());

	const wuffs_base__range_ii_u64 workbuf_range = decoder->workbuf_len();
	js::Vector<uint8, 16> workbuf((size_t)workbuf_range.max_incl);
	const wuffs_base__slice_u8 workbuf_slice = wuffs_base__make_slice_u8(workbuf.data(), workbuf.size());

	// A closed source and an exactly-sized destination should complete in a single call, but loop on suspensions
	// anyway rather than rely on that.
	for(;;)
	{
		const wuffs_base__status status = decoder->transform_io(&dst, &src, workbuf_slice);
		if(status.is_ok())
			break;
		if(!status.is_suspension())
			throw glare::Exception("SOGDecoder: DEFLATE decode error in '" + filename + "': " + std::string(status.message()));
	}

	if(dst.meta.wi != decompressed_out.size())
		throw glare::Exception("SOGDecoder: DEFLATE-decoded size mismatch for '" + filename + "': expected " +
			toString(decompressed_out.size()) + " B, got " + toString(dst.meta.wi) + " B.");
}


void extractEntry(const uint8* data, size_t size, const ZipIndex& index, const std::string& filename, js::Vector<uint8, 16>& data_out)
{
	const ZipIndex::const_iterator it = index.find(filename);
	if(it == index.end())
		throw glare::Exception("SOGDecoder: SOG bundle is missing '" + filename + "'.");

	const ZipEntry& entry = it->second;

	// readZipIndex() already checked this, but re-check it here since we're parsing untrusted data.
	if(entry.data_offset + entry.compressed_size > size)
		throw glare::Exception("SOGDecoder: ZIP entry data out of range for '" + filename + "'.");

	if(entry.compression_method == COMPRESSION_STORE)
	{
		if(entry.compressed_size != entry.uncompressed_size)
			throw glare::Exception("SOGDecoder: stored ZIP entry '" + filename + "' has mismatched compressed and uncompressed sizes.");

		data_out.resizeNoCopy(entry.uncompressed_size);
		if(entry.uncompressed_size > 0)
			std::memcpy(data_out.data(), data + entry.data_offset, entry.uncompressed_size);
	}
	else if(entry.compression_method == COMPRESSION_DEFLATE)
	{
		data_out.resizeNoCopy(entry.uncompressed_size);
		inflateRaw(data + entry.data_offset, entry.compressed_size, data_out, filename);
	}
	else
		throw glare::Exception("SOGDecoder: ZIP entry '" + filename + "' uses unsupported compression method " + toString(entry.compression_method) + ".");
}


//------------------------------------------------------------------------------------------
// meta.json parsing
//------------------------------------------------------------------------------------------

std::vector<std::string> getChildStringArray(const JSONParser& parser, const JSONNode& node, const string_view name)
{
	const JSONNode& array_node = node.getChildArray(parser, name);
	std::vector<std::string> res;
	res.reserve(array_node.child_indices.size());
	for(uint32 index : array_node.child_indices)
		res.push_back(parser.nodes[index].getStringValue());
	return res;
}


std::vector<double> getChildDoubleArray(const JSONParser& parser, const JSONNode& node, const string_view name)
{
	const JSONNode& array_node = node.getChildArray(parser, name);
	std::vector<double> res(array_node.child_indices.size());
	if(!res.empty())
		array_node.parseDoubleArrayValues(parser, res.size(), res.data());
	return res;
}


// Positions are stored log-encoded, to give more precision near the origin.  Note that this is monotonically
// increasing over all reals, which readMetaSummaryFromBuffer() relies on.
inline float unlog(float x)
{
	const float sign = (x > 0.f) ? 1.f : ((x < 0.f) ? -1.f : 0.f);
	return sign * (std::exp(std::fabs(x)) - 1.f);
}


inline float lerp(double a, double b, float t) { return (float)(a + (b - a) * t); }


//------------------------------------------------------------------------------------------
// Attribute images
//------------------------------------------------------------------------------------------

// One decoded WebP image out of the bundle - one pixel per splat, in row-major order.
struct AttributeImage
{
	Reference<Map2D> map; // Owns the pixel data.
	const uint8* data;
	size_t N; // Components per pixel.
	size_t width, height;

	inline const uint8* pixel(size_t i) const { return data + i * N; }
};


void decodeAttributeImage(const uint8* sog_data, size_t sog_size, const ZipIndex& index, const std::string& filename,
	glare::Allocator* mem_allocator, AttributeImage& image_out)
{
	js::Vector<uint8, 16> webp_data;
	extractEntry(sog_data, sog_size, index, filename, webp_data);

	// Check the header before decoding, so a malformed or unexpected image gives a clear error cheaply.
	const WebPDecoder::ImageInfo info = WebPDecoder::getInfoFromBuffer(webp_data.data(), webp_data.size());
	if(info.has_animation)
		throw glare::Exception("SOGDecoder: attribute image '" + filename + "' is an animated WebP file.");

	Reference<Map2D> map = WebPDecoder::decodeFromBuffer(webp_data.data(), webp_data.size(), /*return_animated_webp_as_sequence=*/false, mem_allocator);

	// decodeFromBuffer() returns an ImageMapUInt8 for any non-animated file, and we rejected animations above.
	const ImageMapUInt8* image_map = map.downcastToPtr<ImageMapUInt8>();

	image_out.map    = map;
	image_out.data   = image_map->getData();
	image_out.N      = image_map->getN();
	image_out.width  = image_map->getWidth();
	image_out.height = image_map->getHeight();
}


// The attribute images all have one pixel per splat, so they must agree on dimensions.  (shN, which we don't decode,
// is packed differently and is not checked against these.)
void checkMatchingDims(const AttributeImage& image, const AttributeImage& reference, const std::string& filename)
{
	if(image.width != reference.width || image.height != reference.height)
		throw glare::Exception("SOGDecoder: attribute image '" + filename + "' is " + toString(image.width) + "x" + toString(image.height) +
			", but expected " + toString(reference.width) + "x" + toString(reference.height) + " to match the other attribute images.");
}


// Some attributes carry data in the alpha channel, so a 3-component decode of them means the file is malformed.
// (WebPDecoder returns a 3-component image for files with no alpha channel, and a 4-component one otherwise.)
void checkHasAlpha(const AttributeImage& image, const std::string& filename)
{
	if(image.N < 4)
		throw glare::Exception("SOGDecoder: attribute image '" + filename + "' has no alpha channel, but the SOG format stores data in it.");
}


} // end anonymous namespace


GaussianSplatDataRef SOGDecoder::decodeFromBuffer(const void* data_, size_t size, glare::Allocator* mem_allocator)
{
	const uint8* const data = (const uint8*)data_;

	const ZipIndex index = readZipIndex(data, size);

	js::Vector<uint8, 16> meta_json_data;
	extractEntry(data, size, index, "meta.json", meta_json_data);

	JSONParser json;
	json.parseBuffer((const char*)meta_json_data.data(), meta_json_data.size());
	const JSONNode& root = json.nodes[0];

	//------------------------------ means (positions) ------------------------------
	const JSONNode& means_node = root.getChildObject(json, "means");
	const std::vector<double> means_mins = getChildDoubleArray(json, means_node, "mins");
	const std::vector<double> means_maxs = getChildDoubleArray(json, means_node, "maxs");
	const std::vector<std::string> means_files = getChildStringArray(json, means_node, "files");
	if(means_mins.size() != 3 || means_maxs.size() != 3 || means_files.size() != 2)
		throw glare::Exception("SOGDecoder: malformed 'means' entry in meta.json.");

	AttributeImage means_l, means_u;
	decodeAttributeImage(data, size, index, means_files[0], mem_allocator, means_l);
	decodeAttributeImage(data, size, index, means_files[1], mem_allocator, means_u);

	//------------------------------ scales ------------------------------
	const JSONNode& scales_node = root.getChildObject(json, "scales");
	const std::vector<double> scales_codebook = getChildDoubleArray(json, scales_node, "codebook");
	const std::vector<std::string> scales_files = getChildStringArray(json, scales_node, "files");
	if(scales_codebook.size() != 256 || scales_files.size() != 1)
		throw glare::Exception("SOGDecoder: malformed 'scales' entry in meta.json.");

	AttributeImage scales_image;
	decodeAttributeImage(data, size, index, scales_files[0], mem_allocator, scales_image);

	//------------------------------ quats (rotations) ------------------------------
	const JSONNode& quats_node = root.getChildObject(json, "quats");
	const std::vector<std::string> quats_files = getChildStringArray(json, quats_node, "files");
	if(quats_files.size() != 1)
		throw glare::Exception("SOGDecoder: malformed 'quats' entry in meta.json.");

	AttributeImage quats_image;
	decodeAttributeImage(data, size, index, quats_files[0], mem_allocator, quats_image);

	//------------------------------ sh0 (base colour and opacity) ------------------------------
	const JSONNode& sh0_node = root.getChildObject(json, "sh0");
	const std::vector<double> sh0_codebook = getChildDoubleArray(json, sh0_node, "codebook");
	const std::vector<std::string> sh0_files = getChildStringArray(json, sh0_node, "files");
	if(sh0_codebook.size() != 256 || sh0_files.size() != 1)
		throw glare::Exception("SOGDecoder: malformed 'sh0' entry in meta.json.");

	AttributeImage sh0_image;
	decodeAttributeImage(data, size, index, sh0_files[0], mem_allocator, sh0_image);

	// Note: any shN (higher order spherical harmonic) data is deliberately ignored - see the class comment.

	//------------------------------ consistency checks ------------------------------
	checkMatchingDims(means_u,      means_l, means_files[1]);
	checkMatchingDims(scales_image, means_l, scales_files[0]);
	checkMatchingDims(quats_image,  means_l, quats_files[0]);
	checkMatchingDims(sh0_image,    means_l, sh0_files[0]);

	// The quaternion encoding stores which component was omitted in the alpha channel, and sh0 stores opacity there.
	checkHasAlpha(quats_image, quats_files[0]);
	checkHasAlpha(sh0_image,   sh0_files[0]);

	const size_t num_pixels = means_l.width * means_l.height;

	// 'count' is optional - without it, every pixel of the attribute images is a splat.
	const size_t count = root.hasChild("count") ? (size_t)root.getChildUIntValue(json, "count") : num_pixels;
	if(count > num_pixels)
		throw glare::Exception("SOGDecoder: meta.json 'count' (" + toString(count) + ") exceeds the number of pixels in the attribute images (" + toString(num_pixels) + ").");

	//------------------------------ reconstruct the splats ------------------------------
	const float SH_C0 = 0.28209479177387814f; // The constant DC spherical harmonic basis function.

	GaussianSplatDataRef splats = new GaussianSplatData();
	splats->positions.resizeNoCopy(count);
	splats->scales   .resizeNoCopy(count);
	splats->rotations.resizeNoCopy(count);
	splats->colours  .resizeNoCopy(count);

	js::AABBox aabb = js::AABBox::emptyAABBox();

	for(size_t i=0; i<count; ++i)
	{
		const uint8* const means_l_px = means_l     .pixel(i);
		const uint8* const means_u_px = means_u     .pixel(i);
		const uint8* const scales_px  = scales_image.pixel(i);
		const uint8* const quats_px   = quats_image .pixel(i);
		const uint8* const sh0_px     = sh0_image   .pixel(i);

		// Position: a 16-bit value per axis, split into a low and a high byte across two images, giving a normalised
		// coordinate that is then mapped onto the [mins, maxs] range and decoded out of the log domain.
		const uint32 quantised_x = ((uint32)means_u_px[0] << 8) | means_l_px[0];
		const uint32 quantised_y = ((uint32)means_u_px[1] << 8) | means_l_px[1];
		const uint32 quantised_z = ((uint32)means_u_px[2] << 8) | means_l_px[2];

		const Vec3f pos(
			unlog(lerp(means_mins[0], means_maxs[0], quantised_x * (1.f / 65535.f))),
			unlog(lerp(means_mins[1], means_maxs[1], quantised_y * (1.f / 65535.f))),
			unlog(lerp(means_mins[2], means_maxs[2], quantised_z * (1.f / 65535.f))));

		splats->positions[i] = pos;
		aabb.enlargeToHoldPoint(pos.toVec4fPoint());

		// Scale: one codebook index per axis, where the codebook holds log-domain values.
		splats->scales[i] = Vec3f(
			std::exp((float)scales_codebook[scales_px[0]]),
			std::exp((float)scales_codebook[scales_px[1]]),
			std::exp((float)scales_codebook[scales_px[2]]));

		// Rotation: the "smallest three" quaternion encoding.  The three stored components are in RGB, and alpha holds
		// 252 + the index of the omitted (largest magnitude) component, which is recovered from the other three given
		// that the quaternion is a unit one.  The stored components are scaled so that they span [-1/sqrt(2), 1/sqrt(2)],
		// which is the range the three smallest components of a unit quaternion can take.
		const float inv_sqrt_2 = 0.70710678118654752f;
		const float qa = ((quats_px[0] * (1.f / 255.f)) - 0.5f) * 2.f * inv_sqrt_2;
		const float qb = ((quats_px[1] * (1.f / 255.f)) - 0.5f) * 2.f * inv_sqrt_2;
		const float qc = ((quats_px[2] * (1.f / 255.f)) - 0.5f) * 2.f * inv_sqrt_2;
		const float qd = std::sqrt(myMax(0.f, 1.f - (qa*qa + qb*qb + qc*qc)));

		// Rebuild the quaternion by writing the three stored components back into the components that weren't omitted,
		// in order, and the recovered one into the omitted slot.  Note that the index in the alpha channel is into
		// (w, x, y, z) order, which is not the (x, y, z, w) order we store.
		const int omitted_index = myClamp((int)quats_px[3] - 252, 0, 3);
		const float stored[3] = { qa, qb, qc };
		float wxyz[4];
		int next_stored = 0;
		for(int c=0; c<4; ++c)
			wxyz[c] = (c == omitted_index) ? qd : stored[next_stored++];

		splats->rotations[i] = Vec4f(wxyz[1], wxyz[2], wxyz[3], wxyz[0]);

		// Base colour, from the DC spherical harmonic term, plus opacity.  Note that this is deliberately not clamped:
		// evaluating the DC term can land slightly outside [0, 1] (real files have codebooks reaching low enough to
		// give around -0.035), but it's the base that any higher-order, view-dependent terms would be added to, so
		// clamping it here would be clipping an intermediate value.  Clamping is the renderer's job - see
		// gaussian_splat_frag_shader.glsl.
		splats->colours[i] = Vec4f(
			0.5f + (float)sh0_codebook[sh0_px[0]] * SH_C0,
			0.5f + (float)sh0_codebook[sh0_px[1]] * SH_C0,
			0.5f + (float)sh0_codebook[sh0_px[2]] * SH_C0,
			sh0_px[3] * (1.f / 255.f));
	}

	splats->aabb_os = (count > 0) ? aabb : js::AABBox(Vec4f(0, 0, 0, 1), Vec4f(0, 0, 0, 1));

	return splats;
}


GaussianSplatDataRef SOGDecoder::decode(const std::string& path, glare::Allocator* mem_allocator)
{
	MemMappedFile file(path);
	return decodeFromBuffer(file.fileData(), file.fileSize(), mem_allocator);
}


SOGDecoder::MetaSummary SOGDecoder::readMetaSummaryFromBuffer(const void* data_, size_t size)
{
	const uint8* const data = (const uint8*)data_;

	const ZipIndex index = readZipIndex(data, size);

	js::Vector<uint8, 16> meta_json_data;
	extractEntry(data, size, index, "meta.json", meta_json_data);

	JSONParser json;
	json.parseBuffer((const char*)meta_json_data.data(), meta_json_data.size());
	const JSONNode& root = json.nodes[0];

	const JSONNode& means_node = root.getChildObject(json, "means");
	const std::vector<double> means_mins = getChildDoubleArray(json, means_node, "mins");
	const std::vector<double> means_maxs = getChildDoubleArray(json, means_node, "maxs");
	if(means_mins.size() != 3 || means_maxs.size() != 3)
		throw glare::Exception("SOGDecoder: malformed 'means' entry in meta.json.");

	// unlog() is monotonically increasing, so mapping the quantisation range through it gives the exact bounds that a
	// full decode would produce if some splat's quantised coordinate hit both extremes on every axis.  In practice
	// that makes this very slightly looser than the real bounds, but never tighter.
	MetaSummary summary;
	summary.aabb_os = js::AABBox(
		Vec4f(unlog((float)means_mins[0]), unlog((float)means_mins[1]), unlog((float)means_mins[2]), 1.f),
		Vec4f(unlog((float)means_maxs[0]), unlog((float)means_maxs[1]), unlog((float)means_maxs[2]), 1.f));

	// 'count' is optional, and working out the real splat count without it would mean decoding an attribute image.
	summary.num_splats = root.hasChild("count") ? (size_t)root.getChildUIntValue(json, "count") : 0;

	return summary;
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/Timer.h"


void SOGDecoder::test()
{
	conPrint("SOGDecoder::test()");

	const std::string sog_path = TestUtils::getTestReposDir() + "/testfiles/sog/Smile Gril.sog";

	// Test reading the summary without decoding the attribute images.
	try
	{
		MemMappedFile file(sog_path);
		const MetaSummary summary = SOGDecoder::readMetaSummaryFromBuffer(file.fileData(), file.fileSize());

		testAssert(summary.num_splats == 139708);
		testAssert(summary.aabb_os.min_[0] < summary.aabb_os.max_[0]);
		testAssert(summary.aabb_os.min_[1] < summary.aabb_os.max_[1]);
		testAssert(summary.aabb_os.min_[2] < summary.aabb_os.max_[2]);
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	// Test a full decode.
	try
	{
		Timer timer;
		GaussianSplatDataRef splats = SOGDecoder::decode(sog_path);
		conPrint("Decoded " + toString(splats->numSplats()) + " splats in " + timer.elapsedStringNSigFigs(3));

		testAssert(splats->numSplats() == 139708);
		testAssert(splats->scales   .size() == splats->numSplats());
		testAssert(splats->rotations.size() == splats->numSplats());
		testAssert(splats->colours  .size() == splats->numSplats());

		// The decoded bounds should lie inside the (slightly looser) bounds the summary derives from meta.json.
		MemMappedFile file(sog_path);
		const MetaSummary summary = SOGDecoder::readMetaSummaryFromBuffer(file.fileData(), file.fileSize());
		testAssert(summary.aabb_os.containsAABBox(splats->aabb_os));

		for(size_t i=0; i<splats->numSplats(); ++i)
		{
			// Rotations should be unit quaternions.
			const Vec4f q = splats->rotations[i];
			const float len = std::sqrt(dot(q, q));
			testAssert(epsEqual(len, 1.f, 1.0e-3f));

			// Scales are exponentiated, so must be positive.
			testAssert(splats->scales[i].x > 0 && splats->scales[i].y > 0 && splats->scales[i].z > 0);

			// Opacity should be a valid alpha value.
			testAssert(splats->colours[i][3] >= 0.f && splats->colours[i][3] <= 1.f);
		}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	// Truncated and garbage files should throw, not crash.
	try
	{
		MemMappedFile file(sog_path);
		SOGDecoder::decodeFromBuffer(file.fileData(), file.fileSize() / 2);
		failTest("Expected exception.");
	}
	catch(glare::Exception&)
	{}

	try
	{
		const uint8 garbage[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
		SOGDecoder::decodeFromBuffer(garbage, sizeof(garbage));
		failTest("Expected exception.");
	}
	catch(glare::Exception&)
	{}

	conPrint("SOGDecoder::test() done.");
}


#endif // BUILD_TESTS
