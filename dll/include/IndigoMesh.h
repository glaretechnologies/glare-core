/*=====================================================================
IndigoMesh.h
------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "IndigoString.h"
#include "IndigoGeometryUtils.h"
#include "IndigoThreadSafeRefCounted.h"
#include <Platform.h>
#include <Reference.h>


namespace Indigo
{


///
/// Triangle class.
///
class Triangle
{
public:
	uint32 vertex_indices[3];	///< Stores indices into the mesh vertex position array.
	uint32 uv_indices[3];		///< Stores indices into the mesh uv_pairs array.
	uint32 tri_mat_index;		///< An index into the materials used by this mesh.

	bool operator == (const Triangle& other) const;
	bool operator != (const Triangle& other) const;
};


///
/// Quad class.
///
class Quad
{
public:
	uint32 vertex_indices[4];	///< Stores indices into the mesh vertex position array.
	uint32 uv_indices[4];		///< Stores indices into the mesh uv_pairs array.
	uint32 mat_index;			///< An index into the materials used by this mesh.

	bool operator == (const Quad& other) const;
	bool operator != (const Quad& other) const;
};


const uint32 MESH_UV_LAYOUT_VERTEX_LAYER = 0;
const uint32 MESH_UV_LAYOUT_LAYER_VERTEX = 1;


///
/// See http://indigorenderer.com/indigo-technical-reference/indigo-mesh-file-format-specification for the Indigo Mesh (.igmesh) specification.
///
class Mesh : public ThreadSafeRefCounted
{
	// Tests are in indigo/IndigoMeshTest.cpp
public:
	Mesh();
	~Mesh();

	/// Write a Mesh object to disk.
	/// @param dest_path		Path on disk.
	/// @param mesh				Mesh to write.
	/// @param use_compression	True if compression should be used.
	/// @throws IndigoException on failure.
	static void writeToFile(const String& dest_path, const Mesh& mesh, bool use_compression);

	/// Read a mesh object from disk.
	/// @param src_path			Path on disk to read from.
	/// @param mesh_out			Mesh object to read to.
	/// @throws IndigoException on failure.
	static void readFromFile(const String& src_path, Mesh& mesh_out);

	static void readFromBuffer(const uint8* data, size_t len, Mesh& mesh_out);

	///
	/// Reads file header and returns if compression was used on the mesh data.
	///
	static bool isCompressed(const String& mesh_path);


	void addUVs(const Vector<Vec2f>& new_uvs);
	void addVertex(const Vec3f& pos);
	void addVertex(const Vec3f& pos, const Vec3f& normal);
	void addMaterialUsed(const String& material_name);
	void addTriangle(const uint32* vertex_indices, const uint32* uv_indices, uint32 material_index);
	void addQuad(const uint32* vertex_indices, const uint32* uv_indices, uint32 material_index);
	void setMaxNumTexcoordSets(uint32 max_num_texcoord_sets);

	///
	/// Checks some properties of the mesh, and computes and caches some information about the mesh - num_materials_referenced and aabb_os.
	/// Will be called automatically by SceneNodeRoot::finalise() if not called manually.
	/// @throws IndigoException if mesh is not valid.
	///
	void endOfModel();

	bool hasEndOfModelBeenCalled() const { return end_of_model_called; }

	uint32 getNumUVsPerSet();
	void getBoundingBox(AABB<float>& aabb_out) const;
	uint64 checksum() const;
	bool operator == (const Mesh& other) const;

	/// Number of UV mappings for this mesh.
	/// Defaults to zero but must be set to a value >= 1 if you want to supply UV coordinates with this mesh.
	uint32 num_uv_mappings;

	/// A vector of the names of all the materials used by this mesh.
	/// Only used for old <model> scene format, not used for new <model2> scene format where materials are specified in the model2 element.  May be left empty.
	Vector<String> used_materials;

	/// Vertex positions
	Vector<Vec3f> vert_positions;

	/// Vertex normals.  This vector can be empty if vertex shading normals are not available, otherwise it must be the same size as vert_positions.
	Vector<Vec3f> vert_normals;

	Vector<Vec3f> vert_colours; // Not serialised currently.

	/*!	
	A vector of UV coordinates.  The number of pairs must be a multiple of num_uv_mappings.
	This vector can be laid out in two possible ways.

		MESH_UV_LAYOUT_VERTEX_LAYER UV layout is as follows (in this example num uvs = n, num layers = 2):

		uv_0  layer 0
		uv_0  layer 1

		uv_1  layer 0
		uv_1  layer 1

		uv_2  layer 0
		uv_2  layer 1

		...

		uv_n  layer 0
		uv_n  layer 1


		MESH_UV_LAYOUT_LAYER_VERTEX UV layout is as follows (in this example num uvs = n, num layers = 2):

		layer 0 uv_0
		layer 0 uv_1
		layer 0 uv_2
		layer 0 uv_3
		...
		layer 0 uv_n

		layer 1 uv_0
		layer 1 uv_1
		layer 1 uv_2
		layer 1 uv_3
		...
		layer 1 uv_n
	*/
	uint32 uv_layout;
	Vector<Vec2f> uv_pairs; 

	/// Triangles
	Vector<Triangle> triangles;

	/// Quads.  These will be converted to triangles for ray tracing purposes.
	Vector<Quad> quads;

	/// Object space bounding box.
	/// This will be computed when endOfModel() is called.
	AABB<float> aabb_os;

	/// This will be computed when endOfModel() is called.
	uint32 num_materials_referenced;

private:
	bool end_of_model_called;
};


typedef Reference<Mesh> MeshRef;


}
