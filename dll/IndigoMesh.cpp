/*=====================================================================
IndigoMesh.cpp
--------------
Copyright Glare Technologies Limited 2022 - 
=====================================================================*/
#include "include/IndigoMesh.h"


#include "include/IndigoGeometryUtils.h"
#include "include/IndigoException.h"
#include "IndigoStringUtils.h"
#include "../utils/StringUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/BufferOutStream.h"
#include "../utils/BufferInStream.h"
#include "../utils/Timer.h"
#include "../utils/Vector.h"
#include "../utils/MemMappedFile.h"
#include "../utils/IncludeXXHash.h"
#include "../utils/BufferViewInStream.h"
#include <fstream>
#include <limits>
#include <algorithm>
#include <zstd.h>


// Tests are in indigo/IndigoMeshTest.cpp


namespace Indigo
{


Mesh::Mesh()
{
	num_uv_mappings = 0;
	uv_layout = MESH_UV_LAYOUT_VERTEX_LAYER;
	num_materials_referenced = 0;
	end_of_model_called = false;
}


Mesh::~Mesh()
{
}


// Not used any more, kept for loading of old meshes.
class UVSetExposition
{
public:
	UVSetExposition() {}
	UVSetExposition(const String& n, uint32 i) : uv_set_name(n), uv_set_index(i) {}
	String uv_set_name;
	uint32 uv_set_index;
};


static const size_t MAX_STRING_LENGTH = 1024;
static const uint32 MAGIC_NUMBER = 5456751;
static const uint32 FORMAT_VERSION = 4;


template <class T>
inline static void writeBasic(std::ostream& stream, const T& t)
{
	stream.write((const char*)&t, sizeof(T));
}


inline static void writeData(std::ostream& stream, size_t num_bytes, const void* buf)
{
	stream.write((const char*)buf, num_bytes);
}


template <class T>
inline static void readBasic(BufferViewInStream& file, T& t_out)
{
	file.readData(&t_out, sizeof(T));
}


inline static void readData(BufferViewInStream& file, size_t num_bytes, void* buf_out)
{
	file.readData(buf_out, num_bytes);
}


static void write(std::ostream& stream, const String& s)
{
	if(s.length() > MAX_STRING_LENGTH)
		throw IndigoException("String too long.");

	const uint32 len = (uint32)s.length();

	writeBasic(stream, len);

	if(len > 0)
		stream.write(s.dataPtr(), len);
}


// Read a string
static void read(BufferViewInStream& file, String& s_out)
{
	uint32 len;
	readBasic(file, len);
	if(len > MAX_STRING_LENGTH)
		throw IndigoException("String too long.");

	char buf[MAX_STRING_LENGTH];
	readData(file, len, buf);
	s_out = String(buf, len);
}


static void read(BufferViewInStream& file, UVSetExposition& x)
{
	read(file, x.uv_set_name);
	readBasic(file, x.uv_set_index);
}


template <class T>
static void writeVector(std::ostream& stream, const Vector<T>& v)
{
	const uint32 len = (uint32)v.size();
	//if(len > MAX_VECTOR_LENGTH)
	//	throw IndigoException("Vector too long.");

	writeBasic(stream, len);

	for(uint32 i=0; i<v.size(); ++i)
		write(stream, v[i]);
}


template <class T>
static void writePackedVector(std::ostream& stream, const Vector<T>& v)
{
	const uint32 len = (uint32)v.size();
	//if(len > MAX_VECTOR_LENGTH)
	//	throw IndigoException("Vector too long.");

	writeBasic(stream, len);

	if(len > 0)
		writeData(stream, len * sizeof(T), &v[0]);
}


template <class T>
static void readVector(BufferViewInStream& file, uint32 max_vector_len, Vector<T>& v_out)
{
	uint32 len;
	readBasic(file, len);
	if(len > max_vector_len)
		throw IndigoException("Vector too long.");

	v_out.resize(len);

	for(uint32 i=0; i<len; ++i)
		read(file, v_out[i]);
}


template <class T>
static void readPackedVector(BufferViewInStream& file, Vector<T>& v_out)
{
	uint32 len;
	readBasic(file, len);

	const size_t read_size = sizeof(T) * len; // since len is a uint32, shouldn't overflow a 64-bit size_t.

	if(!file.canReadNBytes(read_size))
		throw IndigoException("Vector too long.");

	v_out.resize(len);
	readData(file, sizeof(T) * len, v_out.data());
}


template <class T>
static void readPackedVector(BufferInStream& stream, Vector<T>& v_out)
{
	const uint32 len = stream.readUInt32();

	const size_t remaining_file_size = stream.buf.size() - stream.read_index;

	const size_t read_size = sizeof(T) * len; // since len is a uint32, shouldn't overflow a 64-bit size_t.

	if(read_size > remaining_file_size)
		throw IndigoException("Vector too long.");

	v_out.resize(len);
	stream.readData(v_out.data(), sizeof(T) * len);
}


void Mesh::addUVs(const Vector<Vec2f>& new_uvs)
{
	assert(new_uvs.size() <= num_uv_mappings);

	for(uint32 i = 0; i < num_uv_mappings; ++i)
	{
		if(i < (uint32)new_uvs.size())
			uv_pairs.push_back(new_uvs[i]);
		else
			uv_pairs.push_back(Vec2f(0.f, 0.f));
	}
}


void Mesh::addVertex(const Vec3f& pos)
{
	vert_positions.push_back(pos);
}


void Mesh::addVertex(const Vec3f& pos, const Vec3f& normal)
{
	const Vec3f n = normalise(normal);

	if(!isFinite(normal.x) || !isFinite(normal.y) || !isFinite(normal.z))
		throw IndigoException("Invalid normal");
	
	vert_positions.push_back(pos);
	vert_normals.push_back(n);
}


void Mesh::addMaterialUsed(const String& material_name)
{
	if(std::find(used_materials.begin(), used_materials.end(), material_name) == used_materials.end()) // If this name not already added...
	{
		//const unsigned int mat_index = (unsigned int)matname_to_index_map.size();
		this->used_materials.push_back(material_name);
	}
}


void Mesh::addTriangle(const uint32* vertex_indices, const uint32* uv_indices, uint32 material_index)
{
	// Check material index is in bounds
	//if(material_index >= used_materials.size())
	//	throw IndigoException("Triangle material_index is out of bounds. (material index=" + toIndigoString(toString(material_index)) + ")");

	// Check vertex indices are in bounds
	for(uint32 i = 0; i < 3; ++i)
		if(vertex_indices[i] >= vert_positions.size())
			throw IndigoException("Triangle vertex index is out of bounds. (vertex index=" + toIndigoString(toString(vertex_indices[i])) + ")");

	// Check uv indices are in bounds
	if(num_uv_mappings > 0)
		for(uint32 i = 0; i < 3; ++i)
			if(uv_indices[i] * num_uv_mappings >= (uint32)uv_pairs.size()) // uv_indices[i] >= getNumUVsPerSet())
				throw IndigoException("Triangle uv index is out of bounds. (uv index=" + toIndigoString(toString(uv_indices[i])) + ")");

	// Check the area of the triangle
	const float MIN_TRIANGLE_AREA = 1.0e-20f;
	if(getTriArea(vert_positions[vertex_indices[0]], vert_positions[vertex_indices[1]], vert_positions[vertex_indices[2]]) < MIN_TRIANGLE_AREA)
	{
		//TEMP: conPrint("WARNING: Ignoring degenerate triangle. (triangle area: " + doubleToStringScientific(getTriArea(vertPos(vertex_indices[0]), vertPos(vertex_indices[1]), vertPos(vertex_indices[2]))) + ")");
		return;
	}

	// Add the triangle to tri array.
	triangles.push_back(Triangle());
	for(unsigned int i = 0; i < 3; ++i)
	{
		triangles.back().vertex_indices[i] = vertex_indices[i];
		triangles.back().uv_indices[i] = uv_indices[i];
	}

	triangles.back().tri_mat_index = material_index;
	//triangles.back().setUseShadingNormals(this->enable_normal_smoothing);
}


void Mesh::addQuad(const uint32* vertex_indices, const uint32* uv_indices, uint32 material_index)
{
	// Check material index is in bounds
	//if(material_index >= used_materials.size())
	//	throw IndigoException("Quad material_index is out of bounds. (material index=" + toIndigoString(toString(material_index)) + ")");

	// Check vertex indices are in bounds
	for(unsigned int i = 0; i < 4; ++i)
		if(vertex_indices[i] >= vert_positions.size())
			throw IndigoException("Quad vertex index is out of bounds. (vertex index=" + toIndigoString(toString(vertex_indices[i])) + ")");

	// Check uv indices are in bounds
	if(num_uv_mappings > 0)
		for(unsigned int i = 0; i < 4; ++i)
			if(uv_indices[i] * num_uv_mappings >= (uint32)uv_pairs.size()) // uv_indices[i] >= getNumUVsPerSet())
				throw IndigoException("Quad uv index is out of bounds. (uv index=" + toIndigoString(toString(uv_indices[i])) + ")");


	// Check the area of the triangle
/*	const float MIN_TRIANGLE_AREA = 1.0e-20f;
	if(getTriArea(vertPos(vertex_indices[0]), vertPos(vertex_indices[1]), vertPos(vertex_indices[2])) < MIN_TRIANGLE_AREA)
	{
		//TEMP: conPrint("WARNING: Ignoring degenerate triangle. (triangle area: " + doubleToStringScientific(getTriArea(vertPos(vertex_indices[0]), vertPos(vertex_indices[1]), vertPos(vertex_indices[2]))) + ")");
		return;
	}
*/
	// Add the quad to quad array.
	Quad q;
	for(unsigned int i = 0; i < 4; ++i)
	{
		q.vertex_indices[i] = vertex_indices[i];
		q.uv_indices[i] = uv_indices[i];
	}
	q.mat_index = material_index;
	//q.setUseShadingNormals(this->enable_normal_smoothing);

	quads.push_back(q);
}


void Mesh::setMaxNumTexcoordSets(uint32 max_num_texcoord_sets)
{
	num_uv_mappings = indigoMax(num_uv_mappings, max_num_texcoord_sets);
}


uint32 Mesh::getNumUVsPerSet()
{
	return (uint32)uv_pairs.size() / num_uv_mappings;
}


void Mesh::getBoundingBox(AABB<float>& aabb_out) const
{
	aabb_out = aabb_os;
}


void Mesh::endOfModel()
{
	if(num_uv_mappings > 0 && (uv_pairs.size() % num_uv_mappings != 0))
		throw IndigoException("Mesh uv_pairs.size() must be a multiple of num_uv_mappings.  uv_pairs.size(): " + toIndigoString(toString(uv_pairs.size())) + 
			", num_uv_mappings: " + toIndigoString(toString(num_uv_mappings)));

	if(vert_normals.size() != 0 && (vert_normals.size() != vert_positions.size()))
		throw IndigoException("vert_normals must be empty, or have equal size to vert_positions");


	aabb_os.setNull();
	for(size_t i = 0; i < vert_positions.size(); ++i)
	{
		Vec3f v = vert_positions[i];
		aabb_os.contain(v);
	}

	// Compute num_materials_referenced
	uint32 max_mat_index = 0;
	for(size_t i=0; i<triangles.size(); ++i)
		max_mat_index = indigoMax(max_mat_index, triangles[i].tri_mat_index);

	if(max_mat_index > 100000000) // Avoid overflow and wrap back to zero when computing num_materials_referenced.
		throw IndigoException("Invalid max mat index: " + toIndigoString(toString(max_mat_index)) + " (too large)");

	for(size_t i=0; i<quads.size(); ++i)
		max_mat_index = indigoMax(max_mat_index, quads[i].mat_index);

	//for(size_t i=0; i<this->chunks.size(); ++i)
	//	max_mat_index = indigoMax(max_mat_index, this->chunks[i].mat_index);

	num_materials_referenced = max_mat_index + 1;

	end_of_model_called = true;
}


// Check structures are not padded, so we can use writePackedVector() etc...
static_assert(sizeof(Vec2f) == sizeof(float) * 2, "Assume densely packed");
static_assert(sizeof(Vec3f) == sizeof(float) * 3, "Assume densely packed");
static_assert(sizeof(Triangle) == sizeof(uint32) * 7, "Assume densely packed");
static_assert(sizeof(Quad) == sizeof(uint32) * 9, "Assume densely packed");


void Mesh::writeToFile(const String& dest_path, const Mesh& mesh, bool use_compression) // throws IndigoException on failure
{
	if(mesh.num_uv_mappings > 0 && (mesh.uv_pairs.size() % mesh.num_uv_mappings != 0))
		throw IndigoException("Mesh uv_pairs.size() must be a multiple of num_uv_mappings.  uv_pairs.size(): " + toIndigoString(toString(mesh.uv_pairs.size())) + 
			", num_uv_mappings: " + toIndigoString(toString(mesh.num_uv_mappings)));

	if(mesh.vert_normals.size() != 0 && (mesh.vert_normals.size() != mesh.vert_positions.size()))
		throw IndigoException("Must be zero normals, or normals.size() must equal verts.size()");

	std::ofstream file(FileUtils::convertUTF8ToFStreamPath(toStdString(dest_path)).c_str(), std::ios_base::out | std::ios_base::binary);

	if(!file)
		throw IndigoException("Failed to open file '" + dest_path + "' for writing.");

	//Timer write_timer;

	const uint32 compression = use_compression ? 1 : 0; // write data with zstandard compression?
	const uint32 data_filtering = 1; // Will we filter the data before compressing it?

	writeBasic(file, MAGIC_NUMBER);
	writeBasic(file, FORMAT_VERSION);
	writeBasic(file, compression);
	writeBasic(file, data_filtering);
	writeBasic(file, mesh.num_uv_mappings);
	writeVector(file, mesh.used_materials);
	writeBasic(file, (uint32)0); // Write uv_set_expositions 'vector'.  (just write vector length of zero)


	// Write the rest of the data compressed
	if(compression)
	{
		// Copy/format the mesh data into a single buffer
		BufferOutStream data;

		// Reserve space to avoid unnecessary allocations
		const size_t initial_capacity =
			sizeof(uint32) +								// num vert positions
			mesh.vert_positions.size() * sizeof(Vec3f) +	// vert positions
			sizeof(uint32) +								// num vert normals
			mesh.vert_normals.size()   * sizeof(Vec3f) +	// vert normals
			//sizeof(uint32) +								// num vert colours
			//mesh.vert_colours.size()   * sizeof(Vec3f) +	// vert colours
			sizeof(uint32) +								// mesh uv layout
			sizeof(uint32) +								// num uvs
			mesh.uv_pairs.size()       * sizeof(Vec2f) +	// uv parirs
			sizeof(uint32) +								// num triangles
			mesh.triangles.size()      * sizeof(Indigo::Triangle) + // triangles
			sizeof(uint32) +								// num quads
			mesh.quads.size()          * sizeof(Indigo::Quad); // quads
		data.buf.reserve(initial_capacity);

		data.writeUInt32((uint32)mesh.vert_positions.size());
		data.writeData(mesh.vert_positions.data(), mesh.vert_positions.size() * sizeof(Vec3f));

		data.writeUInt32((uint32)mesh.vert_normals.size());
		data.writeData(mesh.vert_normals.data(), mesh.vert_normals.size() * sizeof(Vec3f));
		
		//data.writeUInt32((uint32)mesh.vert_colours.size());
		//data.writeData(mesh.vert_colours.data(), mesh.vert_colours.size() * sizeof(Vec3f));

		data.writeUInt32(mesh.uv_layout);

		data.writeUInt32((uint32)mesh.uv_pairs.size());
		data.writeData(mesh.uv_pairs.data(), mesh.uv_pairs.size() * sizeof(Vec2f));

		// Write filtered triangle data
		data.writeUInt32((uint32)mesh.triangles.size());
		if(!mesh.triangles.empty())
		{
			const size_t cur_offset = data.buf.size();
			data.buf.resize(data.buf.size() + mesh.triangles.size() * sizeof(Indigo::Triangle));
			int32* const filtered_tris = (int32*)&data.buf[cur_offset];
			uint32 last_v0 = 0;
			uint32 last_uv0 = 0;
			for(size_t i=0; i<mesh.triangles.size(); ++i)
			{
				const uint32 v0 = mesh.triangles[i].vertex_indices[0];
				filtered_tris[i*7 + 0] = (int32)v0 - (int32)last_v0;
				filtered_tris[i*7 + 1] = (int32)mesh.triangles[i].vertex_indices[1] - (int32)v0;
				filtered_tris[i*7 + 2] = (int32)mesh.triangles[i].vertex_indices[2] - (int32)v0;
				const uint32 uv0 = mesh.triangles[i].uv_indices[0];
				filtered_tris[i*7 + 3] = (int32)uv0 - last_uv0;
				filtered_tris[i*7 + 4] = (int32)mesh.triangles[i].uv_indices[1] - (int32)uv0;
				filtered_tris[i*7 + 5] = (int32)mesh.triangles[i].uv_indices[2] - (int32)uv0;
				filtered_tris[i*7 + 6] = mesh.triangles[i].tri_mat_index;

				last_v0  = v0;
				last_uv0 = uv0;
			}
		}

	
		// Write filtered quad data
		data.writeUInt32((uint32)mesh.quads.size());
		if(!mesh.quads.empty())
		{
			const size_t cur_offset = data.buf.size();
			data.buf.resize(data.buf.size() + mesh.quads.size() * sizeof(Indigo::Quad));
			int32* const filtered_quads = (int32*)&data.buf[cur_offset];
			uint32 last_v0 = 0;
			uint32 last_uv0 = 0;
			for(size_t i=0; i<mesh.quads.size(); ++i)
			{
				const uint32 v0 = mesh.quads[i].vertex_indices[0];
				filtered_quads[i*9 + 0] = (int32)v0 - (int32)last_v0;
				filtered_quads[i*9 + 1] = (int32)mesh.quads[i].vertex_indices[1] - (int32)v0;
				filtered_quads[i*9 + 2] = (int32)mesh.quads[i].vertex_indices[2] - (int32)v0;
				filtered_quads[i*9 + 3] = (int32)mesh.quads[i].vertex_indices[3] - (int32)v0;
				const uint32 uv0 = mesh.quads[i].uv_indices[0];
				filtered_quads[i*9 + 4] = (int32)uv0 - last_uv0;
				filtered_quads[i*9 + 5] = (int32)mesh.quads[i].uv_indices[1] - (int32)uv0;
				filtered_quads[i*9 + 6] = (int32)mesh.quads[i].uv_indices[2] - (int32)uv0;
				filtered_quads[i*9 + 7] = (int32)mesh.quads[i].uv_indices[3] - (int32)uv0;
				filtered_quads[i*9 + 8] = mesh.quads[i].mat_index;

				last_v0  = v0;
				last_uv0 = uv0;
			}
		}

		assert(data.buf.size() == initial_capacity && data.buf.capacity() == initial_capacity);


		// Compress the buffer with zstandard
		const size_t compressed_bound = ZSTD_compressBound(data.buf.size());
		js::Vector<uint8, 16> compressed_data(compressed_bound);
		//Timer timer;
		const size_t compressed_size = ZSTD_compress(compressed_data.data(), compressed_data.size(), data.buf.data(), data.buf.size(),
			1 // compression level
		);
		if(ZSTD_isError(compressed_size))
			throw IndigoException("Compression failed: " + toIndigoString(toString(compressed_size)));

		//const double compression_speed = data.buf.size() / timer.elapsed();
		//conPrint("\nUncompressed size: " + toString(data.buf.size()) + " B");
		//conPrint("compressed size:   " + toString(compressed_size) + " B");
		//conPrint("Compression took " + timer.elapsedString() + " (" + toString(compression_speed / (1024*1024)) + " MB/s)");

		// Write compressed size as uint64
		const uint64 compressed_size_64 = compressed_size;
		file.write((const char*)&compressed_size_64, sizeof(compressed_size_64));

		// Now write compressed data to disk
		file.write((const char*)compressed_data.data(), compressed_size);
	}
	else
	{
		writePackedVector(file, mesh.vert_positions);
		writePackedVector(file, mesh.vert_normals);
		//writePackedVector(file, mesh.vert_colours);
		writeBasic(file, mesh.uv_layout);
		writePackedVector(file, mesh.uv_pairs);
		writePackedVector(file, mesh.triangles);
		writePackedVector(file, mesh.quads);
	}

	if(!file)
		throw IndigoException("Error while writing to '" + dest_path + "'.");

	//conPrint("Total time to write to disk: " + write_timer.elapsedString());
}


void Mesh::readFromFile(const String& src_path, Mesh& mesh_out) // throws IndigoException on failure
{
	try
	{
		MemMappedFile file(toStdString(src_path));

		readFromBuffer((const uint8*)file.fileData(), file.fileSize(), mesh_out);
	}
	catch(glare::Exception& e)
	{
		throw IndigoException(toIndigoString(e.what()));
	}
}


void Mesh::readFromBuffer(const uint8* data, size_t len, Mesh& mesh_out)
{
	try
	{
		BufferViewInStream file(ArrayRef<uint8>(data, len));

		uint32 magic_number;
		readBasic(file, magic_number);
		if(magic_number != MAGIC_NUMBER)
			throw IndigoException("Invalid magic number.");

		uint32 format_version;
		readBasic(file, format_version);
		if(format_version != 1 && format_version != 2 && format_version != 3 && format_version != 4)
			throw IndigoException("Unsupported format version " + toIndigoString(toString(format_version)) + ".");

		uint32 compression = 0;
		uint32 data_filtering = 0;
		if(format_version >= 4)
		{
			readBasic(file, compression);
			readBasic(file, data_filtering);
		}

		readBasic(file, mesh_out.num_uv_mappings);
		
		readVector(file, /*max vector len=*/10000, mesh_out.used_materials);

		Vector<UVSetExposition> uv_set_expositions;
		readVector(file, /*max vector len=*/10000, uv_set_expositions);

		if(compression)
		{
			//assert(data_filtering != 0);

			// Read compressed size
			uint64 compressed_size;
			readBasic(file, compressed_size);

			// Check that reading the compressed data will be in the file bounds.
			if(!file.canReadNBytes(compressed_size)) // Check compressed_size is valid, while taking care with wraparound
				throw IndigoException("Compressed data extends past end of file (file truncated?)");

			const uint64 decompressed_size = ZSTD_getFrameContentSize(file.currentReadPtr(), compressed_size);
			if(decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN || decompressed_size == ZSTD_CONTENTSIZE_ERROR)
				throw IndigoException("Failed to get decompressed_size");

			const uint64 MAX_DECOMPRESSED_SIZE = 1 << 30;
			if(decompressed_size > MAX_DECOMPRESSED_SIZE)
				throw IndigoException(toIndigoString("Decompressed size is too large: " + toString(decompressed_size)));

			// Decompress data into 'instream' buffer.
			BufferInStream instream;
			instream.buf.resizeNoCopy(decompressed_size);
			//Timer timer;
			const size_t res = ZSTD_decompress(instream.buf.begin(), decompressed_size, file.currentReadPtr(), compressed_size);
			if(ZSTD_isError(res))
				throw IndigoException("Decompression of buffer failed: " + toIndigoString(toString(res)));
			if(res < decompressed_size)
				throw IndigoException("Decompression of buffer failed: not enough bytes in result");

			//const double dempression_speed = decompressed_size / timer.elapsed();
			//conPrint("\ndecompressed size: " + toString(decompressed_size) + " B");
			//conPrint("decompression took " + timer.elapsedString() + " (" + toString(dempression_speed / (1024*1024)) + " MB/s)");

			// Process decompressed data
			readPackedVector(instream, mesh_out.vert_positions);
			readPackedVector(instream, mesh_out.vert_normals);
			
			//if(format_version >= 5)
			//	readPackedVector(instream, mesh_out.vert_colours);

			mesh_out.uv_layout = instream.readUInt32();
			if(!(mesh_out.uv_layout == MESH_UV_LAYOUT_VERTEX_LAYER || mesh_out.uv_layout == MESH_UV_LAYOUT_LAYER_VERTEX))
				throw IndigoException("Invalid uv_layout value " + toIndigoString(toString(mesh_out.uv_layout)));

			readPackedVector(instream, mesh_out.uv_pairs);
			
			// Read and unfilter triangle data
			const uint32 num_tris = instream.readUInt32();
			if(num_tris > 0)
			{
				const size_t after_read_index = instream.read_index + num_tris*sizeof(Indigo::Triangle);
				if(after_read_index > instream.buf.size())
					throw IndigoException("Read past end of file.");

				mesh_out.triangles.resize(num_tris);

				const uint32* const filtered_tris = (uint32*)&instream.buf[instream.read_index];
				Triangle* const dest_triangles = mesh_out.triangles.data();

				uint32 last_v0 = 0;
				uint32 last_uv0 = 0;
				for(size_t i=0; i<num_tris; ++i)
				{
					Triangle& tri = dest_triangles[i];

					const uint32 v0 = filtered_tris[i*7 + 0] + last_v0;
					tri.vertex_indices[0] = v0;
					tri.vertex_indices[1] = filtered_tris[i*7 + 1] + v0;
					tri.vertex_indices[2] = filtered_tris[i*7 + 2] + v0;
					const uint32 uv0 = filtered_tris[i*7 + 3] + last_uv0;
					tri.uv_indices[0]     = uv0;
					tri.uv_indices[1]     = filtered_tris[i*7 + 4] + uv0;
					tri.uv_indices[2]     = filtered_tris[i*7 + 5] + uv0;
					tri.tri_mat_index     = filtered_tris[i*7 + 6];

					last_v0  = v0;
					last_uv0 = uv0;
				}

				instream.read_index = after_read_index;
			}
			else
				mesh_out.triangles.resize(0);


			// Read and unfilter quad data
			const uint32 num_quads = instream.readUInt32();
			if(num_quads > 0)
			{
				const size_t after_read_index = instream.read_index + num_quads*sizeof(Indigo::Quad);
				if(after_read_index > instream.buf.size())
					throw IndigoException("Read past end of file.");

				
				mesh_out.quads.resize(num_quads);

				const uint32* const filtered_quads = (uint32*)&instream.buf[instream.read_index];
				Quad* const dest_quads = mesh_out.quads.data();

				uint32 last_v0 = 0;
				uint32 last_uv0 = 0;
				for(size_t i=0; i<num_quads; ++i)
				{
					Quad& quad = dest_quads[i];
					
					const uint32 v0 = filtered_quads[i*9 + 0] + last_v0;
					quad.vertex_indices[0] = v0;
					quad.vertex_indices[1] = filtered_quads[i*9 + 1] + v0;
					quad.vertex_indices[2] = filtered_quads[i*9 + 2] + v0;
					quad.vertex_indices[3] = filtered_quads[i*9 + 3] + v0;

					const uint32 uv0 = filtered_quads[i*9 + 4] + last_uv0;
					quad.uv_indices[0]     = uv0;
					quad.uv_indices[1]     = filtered_quads[i*9 + 5] + uv0;
					quad.uv_indices[2]     = filtered_quads[i*9 + 6] + uv0;
					quad.uv_indices[3]     = filtered_quads[i*9 + 7] + uv0;
					quad.mat_index         = filtered_quads[i*9 + 8];

					last_v0  = v0;
					last_uv0 = uv0;
				}

				instream.read_index = after_read_index;
			}
			else
				mesh_out.quads.resize(0);
		}
		else
		{
			readPackedVector(file, mesh_out.vert_positions);
			readPackedVector(file, mesh_out.vert_normals);

			//if(format_version >= 5)
			//	readPackedVector(file, mesh_out.vert_colours, file_offset);

			if(format_version >= 3)
			{
				readBasic(file, mesh_out.uv_layout);
				if(!(mesh_out.uv_layout == MESH_UV_LAYOUT_VERTEX_LAYER || mesh_out.uv_layout == MESH_UV_LAYOUT_LAYER_VERTEX))
					throw IndigoException("Invalid uv_layout value " + toIndigoString(toString(mesh_out.uv_layout)));
			}
			else
				mesh_out.uv_layout = MESH_UV_LAYOUT_VERTEX_LAYER;

			readPackedVector(file, mesh_out.uv_pairs);

			readPackedVector(file, mesh_out.triangles);

			if(format_version > 1)
				readPackedVector(file, mesh_out.quads);
		}

		mesh_out.endOfModel(); // Called to do checks and initialise bounds
	}
	catch(glare::Exception& e)
	{
		throw IndigoException(toIndigoString(e.what()));
	}
	catch(std::bad_alloc&)
	{
		throw IndigoException("Bad allocation while reading from file.");
	}
}


bool Mesh::isCompressed(const String& mesh_path)
{
	MemMappedFile file(toStdString(mesh_path));

	BufferViewInStream stream(ArrayRef<uint8>((const uint8*)file.fileData(), file.fileSize()));

	uint32 magic_number;
	readBasic(stream, magic_number);
	if(magic_number != MAGIC_NUMBER)
		throw IndigoException("Invalid magic number.");

	uint32 format_version;
	readBasic(stream, format_version);
	if(format_version != 1 && format_version != 2 && format_version != 3 && format_version != 4)
		throw IndigoException("Unsupported format version " + toIndigoString(toString(format_version)) + ".");

	uint32 compression = 0;
	if(format_version >= 4)
		readBasic(stream, compression);

	return compression != 0;
}


uint64 Mesh::checksum() const
{
	XXH64_state_t hash_state;
	XXH64_reset(&hash_state, 1);

	XXH64_update(&hash_state, (void*)&num_uv_mappings, sizeof(num_uv_mappings));
	
	for(size_t i=0; i<used_materials.size(); ++i)
		if(used_materials[i].length() > 0)
			XXH64_update(&hash_state, (void*)used_materials[i].dataPtr(), used_materials[i].length());

	if(!vert_positions.empty())
		XXH64_update(&hash_state, (void*)&vert_positions[0], (vert_positions.size() * sizeof(Vec3f)));

	if(!vert_normals.empty())
		XXH64_update(&hash_state, (void*)&vert_normals[0], (vert_normals.size() * sizeof(Vec3f)));

	//if(!vert_colours.empty())
	//	XXH64_update(&hash_state, (void*)&vert_colours[0], (vert_colours.size() * sizeof(Vec3f)));

	XXH64_update(&hash_state, (void*)&uv_layout, sizeof(uint32));

	if(!uv_pairs.empty())
		XXH64_update(&hash_state, (void*)&uv_pairs[0], (uv_pairs.size() * sizeof(Vec2f)));

	if(!triangles.empty())
		XXH64_update(&hash_state, (void*)&triangles[0], (triangles.size() * sizeof(Triangle)));

	if(!quads.empty())
		XXH64_update(&hash_state, (void*)&quads[0], (quads.size() * sizeof(Quad)));
	
	return XXH64_digest(&hash_state);
}


bool Triangle::operator == (const Triangle& other) const
{
	return
		vertex_indices[0] == other.vertex_indices[0] &&
		vertex_indices[1] == other.vertex_indices[1] &&
		vertex_indices[2] == other.vertex_indices[2] &&
		uv_indices[0] == other.uv_indices[0] &&
		uv_indices[1] == other.uv_indices[1] &&
		uv_indices[2] == other.uv_indices[2] &&
		tri_mat_index == other.tri_mat_index;
}


bool Triangle::operator != (const Triangle& other) const
{
	return !(*this == other);
}


bool Quad::operator == (const Quad& other) const
{
	return
		vertex_indices[0] == other.vertex_indices[0] &&
		vertex_indices[1] == other.vertex_indices[1] &&
		vertex_indices[2] == other.vertex_indices[2] &&
		vertex_indices[3] == other.vertex_indices[3] &&
		uv_indices[0] == other.uv_indices[0] &&
		uv_indices[1] == other.uv_indices[1] &&
		uv_indices[2] == other.uv_indices[2] &&
		uv_indices[3] == other.uv_indices[3] &&
		mat_index == other.mat_index;
}


bool Quad::operator != (const Quad& other) const
{
	return !(*this == other);
}


bool Mesh::operator == (const Mesh& other) const
{
	if(num_uv_mappings != other.num_uv_mappings)
		return false;
	if(used_materials != other.used_materials)
		return false;
	if(vert_positions != other.vert_positions)
		return false;
	if(vert_normals != other.vert_normals)
		return false;
	//if(vert_colours != other.vert_colours)
	//	return false;
	if(uv_layout != other.uv_layout)
		return false;
	if(uv_pairs != other.uv_pairs)
		return false;
	if(triangles != other.triangles)
		return false;
	if(quads != other.quads)
		return false;

	return true;
}

}
