/*=====================================================================
DisplacementUtils.cpp
---------------------
File created by ClassTemplate on Thu May 15 20:31:26 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "DisplacementUtils.h"


#include "../maths/Rect2.h"
#include "../maths/mathstypes.h"
#include "../graphics/TriBoxIntersection.h"
#include "ScalarMatParameter.h"
#include "VoidMedium.h"
#include "TestUtils.h"
#include "globals.h"
#include "../utils/stringutils.h"
#include "PrintOutput.h"
#include "StandardPrintOutput.h"
#include "ThreadContext.h"
#include "Diffuse.h"
#include "SpectrumMatParameter.h"
#include "DisplaceMatParameter.h"


DisplacementUtils::DisplacementUtils()
{	
}


DisplacementUtils::~DisplacementUtils()
{	
}


static const uint32_t mod3[] = { 0, 1, 2, 0, 1, 2 };
static const uint32_t mod4[] = { 0, 1, 2, 3, 0, 1, 2, 3 };



const float doMod(float x)
{
	if(x >= 0)
	{
		return fmod(x, 1.0f);
	}
	else
	{
		const float y = -x;
		return 1.0f - fmod(y, 1.0f);
	}
}

const Vec2f doMod(const Vec2f& v)
{
	return Vec2f(doMod(v.x), doMod(v.y));
}


const Vec2f uvCombination(const std::vector<Vec2f>& uvs, const std::vector<float>& weights)
{
	const Vec2f trans = Vec2f(0.5,0.5) - uvs[0];

	Vec2f sum(0,0);
	for(size_t i=0; i<uvs.size(); ++i)
	{
		const Vec2f T_uv = uvs[i] + trans;
		const Vec2f mod_T_uv = doMod(T_uv);
		const Vec2f w_mod_T_uv = mod_T_uv * weights[i];
		sum += w_mod_T_uv;
	}

	sum -= trans;

	return Vec2f(doMod(sum));
}


static inline const Vec3f triGeometricNormal(const std::vector<DUVertex>& verts, uint32_t v0, uint32_t v1, uint32_t v2)
{
	return normalise(crossProduct(verts[v1].pos - verts[v0].pos, verts[v2].pos - verts[v0].pos));
}


class DUVertIndexPair
{
public:
	DUVertIndexPair(){}
	DUVertIndexPair(unsigned int v_a_, unsigned int v_b_) : v_a(v_a_), v_b(v_b_) {}

	unsigned int v_a, v_b;
};


inline bool operator < (const DUVertIndexPair& a, const DUVertIndexPair& b)
{
	if(a.v_a < b.v_a)
		return true;
	else if(a.v_a > b.v_a)
		return false;
	else	//else a.v_a = b.v_a
	{
		return a.v_b < b.v_b;
	}
}


static inline const Vec2f& getUVs(const std::vector<Vec2f>& uvs, uint32_t num_uv_sets, uint32_t uv_index, uint32_t set_index)
{
	assert(num_uv_sets > 0);
	assert(set_index < num_uv_sets);
	return uvs[uv_index * num_uv_sets + set_index];
}


static inline uint32_t uvIndex(uint32_t num_uv_sets, uint32_t uv_index, uint32_t set_index)
{
	return uv_index * num_uv_sets + set_index;
}


static inline Vec2f& getUVs(std::vector<Vec2f>& uvs, uint32_t num_uv_sets, uint32_t uv_index, uint32_t set_index)
{
	assert(num_uv_sets > 0);
	assert(set_index < num_uv_sets);
	return uvs[uv_index * num_uv_sets + set_index];
}

static void computeVertexNormals(const std::vector<DUTriangle>& triangles,
								 const std::vector<DUQuad>& quads,
								 std::vector<DUVertex>& vertices)
{
	// Initialise vertex normals to null vector
	for(uint32_t i = 0; i < vertices.size(); ++i) vertices[i].normal = Vec3f(0, 0, 0);

	// Add quad face normals to constituent vertices' normals
	for(uint32_t q = 0; q < quads.size(); ++q)
	{
		if(quads[q].dimension == 2)
		{
			const Vec3f quad_normal = triGeometricNormal(vertices, quads[q].vertex_indices[0], quads[q].vertex_indices[1], quads[q].vertex_indices[2]);

			for(uint32_t i = 0; i < 4; ++i)
				vertices[quads[q].vertex_indices[i]].normal += quad_normal;
		}
	}

	// Add triangle face normals to constituent vertices' normals
	for(uint32_t t = 0; t < triangles.size(); ++t)
	{
		if(triangles[t].dimension == 2)
		{
			const Vec3f tri_normal = triGeometricNormal(
					vertices, 
					triangles[t].vertex_indices[0], 
					triangles[t].vertex_indices[1], 
					triangles[t].vertex_indices[2]
				);

			for(uint32_t i = 0; i < 3; ++i)
				vertices[triangles[t].vertex_indices[i]].normal += tri_normal;
		}
	}

	for(uint32_t i = 0; i < vertices.size(); ++i)
		vertices[i].normal.normalise();
}




void DisplacementUtils::subdivideAndDisplace(
	PrintOutput& print_output,
	ThreadContext& context,
	//const Object& object,
	const std::vector<Reference<MaterialBinding> >& materials,
	//const CoordFramed& camera_coordframe_os, 
	//double pixel_height_at_dist_one, 
	//double subdivide_pixel_threshold, 
	//double subdivide_curvature_threshold,
	//unsigned int num_subdivisions,
	//const std::vector<Plane<float> >& camera_clip_planes,
	bool smooth,
	const js::Vector<RayMeshTriangle, 16>& triangles_in, 
	const js::Vector<RayMeshQuad, 16>& quads_in,
	const std::vector<RayMeshVertex>& vertices_in,
	const std::vector<Vec2f>& uvs_in,
	unsigned int num_uv_sets,
	const DUOptions& options,
	js::Vector<RayMeshTriangle, 16>& tris_out, 
	std::vector<RayMeshVertex>& verts_out,
	std::vector<Vec2f>& uvs_out
	)
{
	// Convert RayMeshVertices to DUVertices
	std::vector<DUVertex> temp_verts(vertices_in.size());
	for(uint32_t i = 0; i < vertices_in.size(); ++i)
		temp_verts[i] = DUVertex(vertices_in[i].pos, vertices_in[i].normal);


	// Convert RayMeshTriangles to DUTriangles
	std::vector<DUTriangle> temp_tris(triangles_in.size());
	for(uint32_t i = 0; i < triangles_in.size(); ++i)
		temp_tris[i] = DUTriangle(
			triangles_in[i].vertex_indices[0], triangles_in[i].vertex_indices[1], triangles_in[i].vertex_indices[2],
			triangles_in[i].uv_indices[0], triangles_in[i].uv_indices[1], triangles_in[i].uv_indices[2],
			triangles_in[i].getTriMatIndex(),
			2 // dimension
			);


	// Convert RayMeshQuads to DUQuads
	std::vector<DUQuad> temp_quads(quads_in.size());
	for(uint32_t i = 0; i < quads_in.size(); ++i)
		temp_quads[i] = DUQuad(
		quads_in[i].vertex_indices[0], quads_in[i].vertex_indices[1], quads_in[i].vertex_indices[2], quads_in[i].vertex_indices[3],
		quads_in[i].uv_indices[0], quads_in[i].uv_indices[1], quads_in[i].uv_indices[2], quads_in[i].uv_indices[3],
		quads_in[i].getMatIndex(),
		2 // dimension
		);


	// Add edge and vertex polygons
	{
		std::map<DUVertIndexPair, uint32_t> num_adjacent_polys; // Map from edge -> num adjacent tris

		for(uint32_t t = 0; t < temp_tris.size(); ++t)
		{
			const uint32_t v0 = temp_tris[t].vertex_indices[0];
			const uint32_t v1 = temp_tris[t].vertex_indices[1];
			const uint32_t v2 = temp_tris[t].vertex_indices[2];

			num_adjacent_polys[DUVertIndexPair(myMin(v0, v1), myMax(v0, v1))]++;
			num_adjacent_polys[DUVertIndexPair(myMin(v1, v2), myMax(v1, v2))]++;
			num_adjacent_polys[DUVertIndexPair(myMin(v2, v0), myMax(v2, v0))]++;
		}

		const uint32_t initial_num_temp_tris = temp_tris.size(); // Store number of original triangles

		for(uint32_t q = 0; q < temp_quads.size(); ++q)
		{
			const uint32_t v0 = temp_quads[q].vertex_indices[0]; const uint32_t v1 = temp_quads[q].vertex_indices[1];
			const uint32_t v2 = temp_quads[q].vertex_indices[2]; const uint32_t v3 = temp_quads[q].vertex_indices[3];

			num_adjacent_polys[DUVertIndexPair(myMin(v0, v1), myMax(v0, v1))]++;
			num_adjacent_polys[DUVertIndexPair(myMin(v1, v2), myMax(v1, v2))]++;
			num_adjacent_polys[DUVertIndexPair(myMin(v2, v3), myMax(v2, v3))]++;
			num_adjacent_polys[DUVertIndexPair(myMin(v3, v0), myMax(v3, v0))]++;
		}

		const uint32_t initial_num_temp_quads = temp_quads.size(); // Store number of original quads


		for(uint32_t t = 0; t < initial_num_temp_tris; ++t) // For each original triangle...
		{
			const uint32_t v0 = temp_tris[t].vertex_indices[0];
			const uint32_t v1 = temp_tris[t].vertex_indices[1];
			const uint32_t v2 = temp_tris[t].vertex_indices[2];

			if(num_adjacent_polys[DUVertIndexPair(myMin(v0, v1), myMax(v0, v1))] == 1) // Boundary edge (has only 1 adjacent poly)
				temp_tris.push_back(DUTriangle(v0, v1, 666, temp_tris[t].uv_indices[0], temp_tris[t].uv_indices[1], 666, 666, 1));

			if(num_adjacent_polys[DUVertIndexPair(myMin(v1, v2), myMax(v1, v2))] == 1) // Boundary edge (has only 1 adjacent poly)
				temp_tris.push_back(DUTriangle(v1, v2, 666, temp_tris[t].uv_indices[1], temp_tris[t].uv_indices[2], 666, 666, 1));

			if(num_adjacent_polys[DUVertIndexPair(myMin(v2, v0), myMax(v2, v0))] == 1) // Boundary edge (has only 1 adjacent poly)
				temp_tris.push_back(DUTriangle(v2, v0, 666, temp_tris[t].uv_indices[2], temp_tris[t].uv_indices[0], 666, 666, 1));
		}

		for(uint32_t q = 0; q < initial_num_temp_quads; ++q) // For each original quad...
		{
			const uint32_t v0 = temp_quads[q].vertex_indices[0]; const uint32_t v1 = temp_quads[q].vertex_indices[1];
			const uint32_t v2 = temp_quads[q].vertex_indices[2]; const uint32_t v3 = temp_quads[q].vertex_indices[3];

			if(num_adjacent_polys[DUVertIndexPair(myMin(v0, v1), myMax(v0, v1))] == 1) // Boundary edge (has only 1 adjacent poly)
				temp_quads.push_back(DUQuad(v0, v1, 666, 666, temp_quads[q].uv_indices[0], temp_quads[q].uv_indices[1], 666, 666, 666, 1));

			if(num_adjacent_polys[DUVertIndexPair(myMin(v1, v2), myMax(v1, v2))] == 1) // Boundary edge (has only 1 adjacent poly)
				temp_quads.push_back(DUQuad(v1, v2, 666, 666, temp_quads[q].uv_indices[1], temp_quads[q].uv_indices[2], 666, 666, 666, 1));

			if(num_adjacent_polys[DUVertIndexPair(myMin(v2, v3), myMax(v2, v3))] == 1) // Boundary edge (has only 1 adjacent poly)
				temp_quads.push_back(DUQuad(v2, v3, 666, 666, temp_quads[q].uv_indices[2], temp_quads[q].uv_indices[3], 666, 666, 666, 1));

			if(num_adjacent_polys[DUVertIndexPair(myMin(v3, v0), myMax(v3, v0))] == 1) // Boundary edge (has only 1 adjacent poly)
				temp_quads.push_back(DUQuad(v3, v0, 666, 666, temp_quads[q].uv_indices[3], temp_quads[q].uv_indices[0], 666, 666, 666, 1));
		}
	}

	//TEMP: Merge vertices sharing the same position
	/*{
		conPrint("\tMerging vertices...");
		std::map<DUVertex, unsigned int> new_vert_indices;
		std::vector<DUVertex> newverts;
		for(unsigned int t=0; t<temp_tris.size(); ++t)
		{
			for(unsigned int i=0; i<3; ++i)
			{
				const DUVertex& old_vert = temp_verts[temp_tris[t].vertex_indices[i]];

				unsigned int new_vert_index;
				if(new_vert_indices.find(old_vert) == new_vert_indices.end())
				{
					new_vert_index = newverts.size();
					newverts.push_back(old_vert);
					new_vert_indices.insert(std::make_pair(old_vert, new_vert_index));
				}
				else
					new_vert_index = new_vert_indices[old_vert];

				temp_tris[t].vertex_indices[i] = new_vert_index;
			}
		}
		temp_verts = newverts;
		conPrint("\t\tDone.");
	}*/

	std::vector<Vec2f> temp_uvs = uvs_in;

	std::vector<DUTriangle> temp_tris2;
	std::vector<DUQuad> temp_quads2;
	std::vector<DUVertex> temp_verts2;
	std::vector<Vec2f> temp_uvs2;

	for(uint32_t i = 0; i < options.max_num_subdivisions; ++i)
	{
		print_output.print("\tDoing subdivision level " + toString(i) + "...");

		linearSubdivision(
			print_output,
			context,
			//object,
			materials,
			//materials,
			//camera_coordframe_os, 
			//pixel_height_at_dist_one, 
			//subdivide_pixel_threshold, 
			//subdivide_curvature_threshold,
			//camera_clip_planes,
			temp_tris, // tris in
			temp_quads, // quads in
			temp_verts, // verts in
			temp_uvs, // uvs in
			num_uv_sets,
			options,
			temp_tris2, // tris out
			temp_quads2, // quads out
			temp_verts2, // verts out
			temp_uvs2 // uvs out
			);

		if(smooth)
			averagePass(temp_tris2, temp_quads2, temp_verts2, temp_uvs2, num_uv_sets, options, temp_verts, temp_uvs);
		else
		{
			temp_verts = temp_verts2;
			temp_uvs = temp_uvs2;
		}

		// XXX slow!
		temp_tris = temp_tris2;
		temp_quads = temp_quads2;

		// Recompute vertex normals
		computeVertexNormals(temp_tris, temp_quads, temp_verts);

		print_output.print("\t\tresulting num vertices: " + toString((unsigned int)temp_verts.size()));
		print_output.print("\t\tresulting num triangles: " + toString((unsigned int)temp_tris.size()));
		print_output.print("\t\tresulting num quads: " + toString((unsigned int)temp_quads.size()));
		print_output.print("\t\tDone.");
	}

	std::vector<bool> tris_unclipped; // XXX this data can be made much more compact (bits)
	std::vector<bool> quads_unclipped;

	// Apply the final displacement
	displace(
		context,
		//object,
		materials,
		true, // use anchoring
		//materials, 
		temp_tris, // tris in
		temp_quads, // quads in
		temp_verts, // verts in
		temp_uvs, // uvs in
		num_uv_sets,
		temp_verts2, // verts out
		&tris_unclipped,
		&quads_unclipped
		);

	temp_verts = temp_verts2;

	// Build tris_out
	tris_out.resize(0);
	for(uint32_t i = 0; i < temp_tris.size(); ++i)
		if(tris_unclipped[i] && temp_tris[i].dimension == 2)
		{
			tris_out.push_back(RayMeshTriangle(temp_tris[i].vertex_indices[0], temp_tris[i].vertex_indices[1], temp_tris[i].vertex_indices[2], temp_tris[i].tri_mat_index));

			for(uint32_t c = 0; c < 3; ++c)
				tris_out.back().uv_indices[c] = temp_tris[i].uv_indices[c];
		}

	for(uint32_t i = 0; i < temp_quads.size(); ++i)
	{
		if(quads_unclipped[i] && temp_quads[i].dimension == 2)
		{
			// Split the quad into two triangles
			tris_out.push_back(RayMeshTriangle(temp_quads[i].vertex_indices[0], temp_quads[i].vertex_indices[1], temp_quads[i].vertex_indices[2], temp_quads[i].mat_index));
			tris_out.push_back(RayMeshTriangle(temp_quads[i].vertex_indices[0], temp_quads[i].vertex_indices[2], temp_quads[i].vertex_indices[3], temp_quads[i].mat_index));

			tris_out[tris_out.size() - 2].uv_indices[0] = temp_quads[i].uv_indices[0];
			tris_out[tris_out.size() - 2].uv_indices[1] = temp_quads[i].uv_indices[1];
			tris_out[tris_out.size() - 2].uv_indices[2] = temp_quads[i].uv_indices[2];

			tris_out[tris_out.size() - 1].uv_indices[0] = temp_quads[i].uv_indices[0];
			tris_out[tris_out.size() - 1].uv_indices[1] = temp_quads[i].uv_indices[2];
			tris_out[tris_out.size() - 1].uv_indices[2] = temp_quads[i].uv_indices[3];
			// XXX this could be wrong?
			//for(uint32_t c = 0; c < 4; ++c)
			//	tris_out.back().uv_indices[c] = temp_quads[i].uv_indices[c];
		}
	}


	// Recompute all vertex normals, as they will be completely wrong by now due to any displacement.
	computeVertexNormals(temp_tris, temp_quads, temp_verts);
	/*for(unsigned int i=0; i<temp_verts.size(); ++i)
		temp_verts[i].normal = Vec3f(0.f, 0.f, 0.f);

	for(unsigned int t=0; t<temp_tris.size(); ++t)
	{
		if(temp_tris[t].dimension == 2)
		{
			const Vec3f tri_normal = triGeometricNormal(
					temp_verts, 
					temp_tris[t].vertex_indices[0], 
					temp_tris[t].vertex_indices[1], 
					temp_tris[t].vertex_indices[2]
				);
			for(int i=0; i<3; ++i)
				temp_verts[temp_tris[t].vertex_indices[i]].normal += tri_normal;
		}
	}
	for(unsigned int i=0; i<temp_verts.size(); ++i)
		temp_verts[i].normal.normalise();*/



	// Convert DUVertex's back into RayMeshVertex and store in verts_out.
	verts_out.resize(temp_verts.size());
	for(uint32_t i = 0; i < verts_out.size(); ++i)
		verts_out[i] = RayMeshVertex(temp_verts[i].pos, temp_verts[i].normal);

	uvs_out = temp_uvs;
}



class DUTexCoordEvaluator : public TexCoordEvaluator
{
public:
	DUTexCoordEvaluator(){}
	~DUTexCoordEvaluator(){}

	virtual const TexCoordsType getTexCoords(const HitInfo& hitinfo, unsigned int texcoords_set) const
	{
		return texcoords[texcoords_set];
	}

	virtual unsigned int getNumTexCoordSets() const { return texcoords.size(); }

	virtual void getUVPartialDerivs(const HitInfo& hitinfo, unsigned int texcoord_set, TexCoordsRealType& ds_du_out, TexCoordsRealType& ds_dv_out, TexCoordsRealType& dt_du_out, TexCoordsRealType& dt_dv_out) const
	{
		// This should never be evaluated, because we don't need to know the partial derives when doing displacement.

		/*assert(texcoords_set < texcoords.size());

		const Vec2f& v0tex = this->uvs[triangles[hitinfo.sub_elem_index].uv_indices[0] * num_uvs_per_group + texcoords_set];
		const Vec2f& v1tex = this->uvs[triangles[hitinfo.sub_elem_index].uv_indices[1] * num_uvs_per_group + texcoords_set];
		const Vec2f& v2tex = this->uvs[triangles[hitinfo.sub_elem_index].uv_indices[2] * num_uvs_per_group + texcoords_set];

		ds_du_out = v1tex.x - v0tex.x;
		dt_du_out = v1tex.y - v0tex.y;

		ds_dv_out = v2tex.x - v0tex.x;
		dt_dv_out = v2tex.y - v0tex.y;*/

		assert(0);
		ds_du_out = ds_dv_out = dt_du_out = dt_dv_out = 0.0;
	}


	std::vector<TexCoordsType> texcoords;

	//Matrix2d d_st_d_uv;
	//double ds_du_out, ds_dv_out, dt_du_out, dt_dv_out;
};


inline const Vec3d triGeomNormal(const std::vector<DUVertex>& verts, const DUTriangle& tri)
{
	return toVec3d(normalise(::crossProduct(
		verts[tri.vertex_indices[1]].pos - verts[tri.vertex_indices[0]].pos,
		verts[tri.vertex_indices[2]].pos - verts[tri.vertex_indices[0]].pos
		)));
}

/*

Returns the displacement at a point on a triangle, 
evaluated at an arbitrary point on the triangle, according to the barycentric coordinates (b1, b2)


*/
static float evalDisplacement(ThreadContext& context, 
								//const Object& object,
								const std::vector<Reference<MaterialBinding> >& materials,
								const DUTriangle& triangle, 
								const std::vector<DUVertex>& verts,
								const std::vector<Vec2f>& uvs,
								unsigned int num_uv_sets,
								float b1, // barycentric coords
								float b2
								)
{
	const Material& material = *materials[triangle.tri_mat_index]->material; //object.getMaterial(triangle.tri_mat_index);
	
	if(material.displacing())
	{
		DUTexCoordEvaluator du_texcoord_evaluator;
		du_texcoord_evaluator.texcoords.resize(num_uv_sets);

		// Set up UVs
		const float b0 = (1 - b1) - b2;
		for(uint32_t z = 0; z < num_uv_sets; ++z)
		{
			du_texcoord_evaluator.texcoords[z] = 
				getUVs(uvs, num_uv_sets, triangle.uv_indices[0], z) * b0 + 
				getUVs(uvs, num_uv_sets, triangle.uv_indices[1], z) * b1 + 
				getUVs(uvs, num_uv_sets, triangle.uv_indices[2], z) * b2;
		}

		HitInfo hitinfo(std::numeric_limits<unsigned int>::max(), HitInfo::SubElemCoordsType(-666, -666));

		const MaterialBinding& material_binding = *materials[triangle.tri_mat_index]; //object.getMaterialBinding(triangle.tri_mat_index);

		return (float)material.evaluateDisplacement(context, hitinfo, material_binding, du_texcoord_evaluator);
	}
	else
	{
		return 0.0f;
	}
}


/*

Returns the displacement at a point on a triangle, interpolated from the stored displacement at the vertices.

*/
static float interpolatedDisplacement(
								const DUTriangle& triangle, 
								const std::vector<DUVertex>& verts,
								const std::vector<Vec2f>& uvs,
								unsigned int num_uv_sets,
								float b1, // barycentric coords
								float b2
								)
{
		const float b0 = (1.0f - b1) - b2;

		return 
			verts[triangle.vertex_indices[0]].displacement * b0 + 
			verts[triangle.vertex_indices[1]].displacement * b1 + 
			verts[triangle.vertex_indices[2]].displacement * b2;
}


/*

Returns the maximum absolute difference between the displacement as interpolated from the vertices,
and the displacement as evaluated directly.

*/
static float displacementError(ThreadContext& context, 
								//const Object& object,
								const std::vector<Reference<MaterialBinding> >& materials,
								const DUTriangle& triangle, 
								const std::vector<DUVertex>& verts,
								const std::vector<Vec2f>& uvs,
								unsigned int num_uv_sets,
								int res
								)
{
	// Early out if material not displacing:
	if(!materials[triangle.tri_mat_index]->material->displacing()) // if(!object.getMaterial(triangle.tri_mat_index).displacing())
		return 0.0f;

	float max_error = -std::numeric_limits<float>::max();
	const float recip_res = 1.0f / (float)res;
	for(int x=0; x<=res; ++x)
		for(int y=0; y<=(res - x); ++y)
		{
			const float nudge = 0.999f; // nudge so that barycentric coords are valid
			const float b1 = x * recip_res * nudge;
			const float b2 = y * recip_res * nudge;

			assert(b1 + b2 <= 1.0f);

			const float error = fabs(
				interpolatedDisplacement(triangle, verts, uvs, num_uv_sets, b1, b2) - 
				evalDisplacement(context, materials, triangle, verts, uvs, num_uv_sets, b1, b2)
				);

			max_error = myMax(max_error, error);
		}

	return max_error;
}


/*
Apply displacement to the given vertices, storing the displaced vertices in verts_out


*/
void DisplacementUtils::displace(ThreadContext& context,
								 //const Object& object,
								 const std::vector<Reference<MaterialBinding> >& materials,
								 bool use_anchoring,
								 const std::vector<DUTriangle>& triangles,
								 const std::vector<DUQuad>& quads,
								 const std::vector<DUVertex>& verts_in,
								 const std::vector<Vec2f>& uvs,
								 unsigned int num_uv_sets,
								 std::vector<DUVertex>& verts_out,
								 std::vector<bool>* unclipped_tris_out,
								 std::vector<bool>* unclipped_quads_out
								 )
{
	verts_out = verts_in;

	if(unclipped_tris_out)
	{
		unclipped_tris_out->resize(triangles.size());
		for(uint32_t i = 0; i < triangles.size(); ++i)
			(*unclipped_tris_out)[i] = true;
	}

	if(unclipped_quads_out)
	{
		unclipped_quads_out->resize(quads.size());
		for(uint32_t i = 0; i < quads.size(); ++i)
			(*unclipped_quads_out)[i] = true;
	}


	DUTexCoordEvaluator du_texcoord_evaluator;
	du_texcoord_evaluator.texcoords.resize(num_uv_sets);


	//std::vector<bool> vert_displaced(verts_in.size(), false); // Only displace each vertex once

	VoidMedium temp_void_medium;

	// For each triangle
	for(uint32_t t = 0; t < triangles.size(); ++t)
	{
		if(triangles[t].dimension == 2)
		{
			const uint32_t material_index = triangles[t].tri_mat_index;
			const Material* material = materials[triangles[t].tri_mat_index]->material.getPointer(); // &object.getMaterial(material_index); //materials[triangles[t].tri_mat_index].getPointer(); // Get the material assigned to this triangle
			const MaterialBinding& material_binding = *materials[triangles[t].tri_mat_index]; // object.getMaterialBinding(material_index);

			if(material->displacing())
			{
				const Vec3d N_g = triGeomNormal(verts_in, triangles[t]);
	

				//hitinfo.hitobject = &object;

				//const int uv_set_index = material->getDisplacementTextureUVSetIndex();
				//assert(uv_set_index >= 0 && uv_set_index < (int)num_uv_sets);

				float min_displacement = std::numeric_limits<float>::infinity();

				// For each vertex
				for(uint32_t i = 0; i < 3; ++i)
				{
					/*const Vec3d& N_s = toVec3d(verts_in[triangles[t].vertex_indices[i]].normal);
					assert(N_s.isUnitLength());

					//Basisd shading_basis;
					//shading_basis.constructFromVector(N_s);
					FullHitInfo hitinfo(
						&object,
						&temp_void_medium,
						toVec3d(verts_in[triangles[t].vertex_indices[i]].pos), // position
						N_g,
						N_g,
						N_s,
						HitInfo(std::numeric_limits<unsigned int>::max(), Vec2d(-666.0, -666.0)) // NOTE: we use dummy values here because they will not be used, because of our custom texcoord evaluator.
						);*/

					HitInfo hitinfo(std::numeric_limits<unsigned int>::max(), HitInfo::SubElemCoordsType(-666, -666));

					//hitinfo.hitpos = toVec3d(verts_in[triangles[t].vertex_indices[i]].pos);

					for(uint32_t z = 0; z < num_uv_sets; ++z)
						du_texcoord_evaluator.texcoords[z] = getUVs(uvs, num_uv_sets, triangles[t].uv_indices[i], z);

					/*const Vec2f& uv = getUVs(uvs, num_uv_sets, triangles[t].uv_indices[i], uv_set_index);
					const float displacement = (float)material->displacement(
						uv.x, //uvs[triangles[t].uv_indices[i] * num_uv_sets + uv_set_index].x, //verts_out[triangles[t].vertex_indices[i]].texcoords[uv_set_index].x,
						uv.y //uvs[triangles[t].uv_indices[i] * num_uv_sets + uv_set_index].y //verts_out[triangles[t].vertex_indices[i]].texcoords[uv_set_index].y
						);*/
					//const float displacement = (float)material->getDisplacementParam()->eval(context, hitinfo, *material, du_texcoord_evaluator);
					const float displacement = (float)material->evaluateDisplacement(context, hitinfo, material_binding, du_texcoord_evaluator);

					min_displacement = myMin(min_displacement, displacement);

					//if(!vert_displaced[triangles[t].vertex_indices[i]])
					//{
						// Translate vertex position along vertex shading normal
						//verts_out[triangles[t].vertex_indices[i]].pos += verts_out[triangles[t].vertex_indices[i]].normal * displacement;
					assert(verts_in[triangles[t].vertex_indices[i]].normal.isUnitLength());

					verts_out[triangles[t].vertex_indices[i]].displacement = displacement;
					verts_out[triangles[t].vertex_indices[i]].pos = verts_in[triangles[t].vertex_indices[i]].pos + verts_in[triangles[t].vertex_indices[i]].normal * displacement;


						

						//vertices[triangles[t].vertex_indices[i]].pos.x += sin(vertices[triangles[t].vertex_indices[i]].pos.z * 50.0f) * 0.03f;

						//vert_displaced[triangles[t].vertex_indices[i]] = true;
					//}
				}

				if(unclipped_tris_out)
				{
					(*unclipped_tris_out)[t] = true;//min_displacement > 0.2f;
				}
			}
		}
	}

	// If any vertex is anchored, then set its position to the average of its 'parent' vertices
	for(uint32_t v = 0; v < verts_out.size(); ++v)
		if(verts_out[v].anchored)
			verts_out[v].pos = (verts_out[verts_out[v].adjacent_vert_0].pos + verts_out[verts_out[v].adjacent_vert_1].pos) * 0.5f;
}


static float triangleMaxCurvature(const Vec3f& v0_normal, const Vec3f& v1_normal, const Vec3f& v2_normal)
{
	const float curvature_u = ::angleBetweenNormalized(v0_normal, v1_normal);// / v0.pos.getDist(v1.pos);
	const float curvature_v = ::angleBetweenNormalized(v0_normal, v2_normal);// / v0.pos.getDist(v2.pos);
	const float curvature_uv = ::angleBetweenNormalized(v1_normal, v2_normal);// / v1.pos.getDist(v2.pos);

	return myMax(curvature_u, curvature_v, curvature_uv);
}


inline static const Vec2f screenSpacePosForCameraSpacePos(const Vec3f& cam_space_pos)
{
	return Vec2f(cam_space_pos.x / cam_space_pos.y, cam_space_pos.z / cam_space_pos.y);
}


class DUEdgeInfo
{
public:
	DUEdgeInfo()
	:	midpoint_vert_index(0), 
		num_adjacent_subdividing_polys(0), 
		border(false) 
	{}
	DUEdgeInfo(unsigned int midpoint_vert_index_, unsigned int num_adjacent_subdividing_polys_) :
		midpoint_vert_index(midpoint_vert_index_), num_adjacent_subdividing_polys(num_adjacent_subdividing_polys_)
		{}
	~DUEdgeInfo(){}
	unsigned int midpoint_vert_index;
	unsigned int num_adjacent_subdividing_polys;
	//unsigned int num_adjacent_non_subdividing_tris;
	//int left_tri_index, right_tri_index;
	bool border;
};


template <class Real>
static Real wrappedLerp(Real x_, Real y_, Real t)
{
	//
	/*Real x = fmod(x_, (Real)1.0);
	Real y = fmod(y_, (Real)1.0);

	if(x < 0.0)
		x += 1.0;
	if(y < 0.0)
		y += 1.0;

	assert(x >= 0.0 && x < 1.0);
	assert(y >= 0.0 && y < 1.0);*/

	Real x = x_;
	Real y = y_;

	//const double x = myMin(x_, y_);
	//const double y = myMax(x_, y_);

	/*if(x > y)
	{
		mySwap(x, y);
		t = 1.0 - t;
	}

	assert(x <= y);

	const double usual_dist = y - x;
	const double wrapping_dist = x + 1.0 - y;

	assert(usual_dist >= 0.0);
	assert(wrapping_dist >= 0.0);

	if(usual_dist < wrapping_dist)
	{
		return y * t + (1.0 - t) * x;
	}
	else
	{
		return fmod(y * t + (1.0 - t) * (x + 1.0), 1.0);
	}*/

	const Real d = fabs(x - y);

	if(d < 0.5)
	{
		// use usual interpolation
		return y * t + (1 - t) * x;
	}
	else
	{
		if(x < y)
		{
			// interpolate between 1 + x and y
			return y * t + (1 - t) * (x + 1);
		}
		else
		{
			// Interpolate between x and 1 + y
			return (1 + y) * t + (1 - t) * x;
		}
	}
}


/*
static inline const Vec2f mod1(const Vec2f& v)
{
	return Vec2f(
		fmod(v.x, 1.0f),
		fmod(v.y, 1.0f)
		);
}
*/

static const Vec2f lerpUVs(const Vec2f& a, const Vec2f& b, float t, bool wrap_u, bool wrap_v)
{
	/*return mod1(Vec2f(
		wrappedLerp(a.x, b.x, t),
		wrappedLerp(a.y, b.y, t)
		));*/

	return Vec2f(
		wrap_u ? fmod(wrappedLerp(a.x, b.x, t), 1) : Maths::uncheckedLerp(a.x, b.x, t),
		wrap_v ? fmod(wrappedLerp(a.y, b.y, t), 1) : Maths::uncheckedLerp(a.y, b.y, t)
		);


	//return a * (1 - t) + b * t;
}


static int getDispErrorRes(unsigned int num_tris)
{
	if(num_tris < 100)
		return 15;
	else if(num_tris < 10000)
		return 10;
	else
		return 4;

	//return 10;
}


void DisplacementUtils::linearSubdivision(
	PrintOutput& print_output,
	ThreadContext& context,
	const std::vector<Reference<MaterialBinding> >& materials,
	//const Object& object,
	//const std::vector<Reference<Material> >& materials,
	//const CoordFramed& camera_coordframe_os,
	//const std::vector<Plane<float> >& camera_clip_planes,
	const std::vector<DUTriangle>& tris_in,
	const std::vector<DUQuad>& quads_in,
	const std::vector<DUVertex>& verts_in,
	const std::vector<Vec2f>& uvs_in,
	unsigned int num_uv_sets,
	const DUOptions& options,
	std::vector<DUTriangle>& tris_out,
	std::vector<DUQuad>& quads_out,
	std::vector<DUVertex>& verts_out,
	std::vector<Vec2f>& uvs_out
	)
{
	verts_out = verts_in; // Copy over original vertices
	uvs_out = uvs_in; // Copy over uvs

	tris_out.resize(0);
	quads_out.resize(0);


	// Get displaced vertices, which are needed for testing if subdivision is needed
	std::vector<DUVertex> displaced_in_verts;
	displace(
		context,
		materials,
		true, // use anchoring
		//materials,
		tris_in,
		quads_in,
		verts_in,
		uvs_in,
		num_uv_sets,
		displaced_in_verts, // verts out
		NULL, // unclipped tris out
		NULL // unclipped quads out
		);

	// Calculate normals of the displaced vertices
	computeVertexNormals(tris_in, quads_in, displaced_in_verts);

	std::map<DUVertIndexPair, DUEdgeInfo> edge_info_map;
	std::map<DUVertIndexPair, unsigned int> uv_edge_map;

	std::vector<std::pair<uint32_t, uint32_t> > quad_centre_data(quads_in.size()); // (vertex index, uv set index)

	// Create some temporary buffers
	std::vector<Vec3f> tri_verts_pos_os(3);

	std::vector<Vec3f> clipped_tri_verts_os;
	clipped_tri_verts_os.reserve(32);

	std::vector<Vec3f> clipped_tri_verts_cs;
	clipped_tri_verts_cs.reserve(32);

	// Do a pass to decide whether or not to subdivide each triangle, and create new vertices if subdividing.

	/*

	view dependent subdivision:
		* triangle in view frustrum
		* screen space pixel size > subdivide_pixel_threshold

	subdivide = 
		num_subdivs < max_num_subdivs && 
		(curvature >= curvature_threshold && 
		triangle in view frustrum &&
		screen space pixel size > subdivide_pixel_threshold ) ||
		displacement_error > displacement_error_threshold


		if view_dependent:
			subdivide = 
				num_subdivs < max_num_subdivs AND
				triangle in view frustrum AND
				screen space pixel size > subdivide_pixel_threshold AND
				(curvature >= curvature_threshold OR 
				displacement_error >= displacement_error_threshold)

		else if not view dependent:
			subdivide = 
				num_subdivs < max_num_subdivs AND
				(curvature >= curvature_threshold OR 
				displacement_error >= displacement_error_threshold)
	*/

	std::vector<bool> subdividing_tri(tris_in.size(), false);

	// For each triangle
	for(uint32_t t = 0; t < tris_in.size(); ++t)
	{
		if(tris_in[t].dimension == 2) // if 2-d (triangle)
		{
			// Decide if we are going to subdivide the triangle
			bool subdivide_triangle = false;

			if(options.view_dependent_subdivision)
			{
				// Build vector of displaced triangle vertex positions. (in object space)
				for(uint32_t i = 0; i < 3; ++i)
					tri_verts_pos_os[i] = displaced_in_verts[tris_in[t].vertex_indices[i]].pos;

				// Clip triangle against frustrum planes
				TriBoxIntersection::clipPolyToPlaneHalfSpaces(options.camera_clip_planes_os, tri_verts_pos_os, clipped_tri_verts_os);

				if(clipped_tri_verts_os.size() > 0) // If the triangle has not been completely clipped away
				{
					// Convert clipped verts to camera space
					clipped_tri_verts_cs.resize(clipped_tri_verts_os.size());
					for(uint32_t i = 0; i < clipped_tri_verts_cs.size(); ++i)
					{
						Vec4f v_os;
						clipped_tri_verts_os[i].pointToVec4f(v_os);

						const Vec4f v_cs = options.object_to_camera * v_os;

						clipped_tri_verts_cs[i] = Vec3f(v_cs);
					}

					// Compute 2D bounding box of clipped triangle in screen space
					const Vec2f v0_ss = screenSpacePosForCameraSpacePos(clipped_tri_verts_cs[0]);
					Rect2f rect_ss(v0_ss, v0_ss);

					for(uint32_t i = 1; i < clipped_tri_verts_cs.size(); ++i)
						rect_ss.enlargeToHoldPoint(
							screenSpacePosForCameraSpacePos(clipped_tri_verts_cs[i])
							);

					// Subdivide only if the width of height of the screen space triangle bounding rectangle is bigger than the pixel height threshold
					const bool exceeds_pixel_threshold = myMax(rect_ss.getWidths().x, rect_ss.getWidths().y) > options.pixel_height_at_dist_one * options.subdivide_pixel_threshold;

					if(exceeds_pixel_threshold)
					{
						const float tri_curvature = triangleMaxCurvature(
							displaced_in_verts[tris_in[t].vertex_indices[0]].normal, 
							displaced_in_verts[tris_in[t].vertex_indices[1]].normal, 
							displaced_in_verts[tris_in[t].vertex_indices[2]].normal);

						if(tri_curvature >= (float)options.subdivide_curvature_threshold)
							subdivide_triangle = true;
						else
						{
							// Test displacement error
							const int res = getDispErrorRes(tris_in.size());
							const float displacement_error = displacementError(context, materials, tris_in[t], displaced_in_verts, uvs_in, num_uv_sets, res);

							subdivide_triangle = displacement_error >= options.displacement_error_threshold;
						}
					}
				}
			}
			else
			{
				const float tri_curvature = triangleMaxCurvature(
							displaced_in_verts[tris_in[t].vertex_indices[0]].normal, 
							displaced_in_verts[tris_in[t].vertex_indices[1]].normal, 
							displaced_in_verts[tris_in[t].vertex_indices[2]].normal);

				if(tri_curvature >= (float)options.subdivide_curvature_threshold)
					subdivide_triangle = true;
				else
				{
					// Test displacement error
					const int RES = 10;
					const float displacment_error = displacementError(context, materials, tris_in[t], displaced_in_verts, uvs_in, num_uv_sets, RES);

					subdivide_triangle = displacment_error >= options.displacement_error_threshold;
				}
			}


			subdividing_tri[t] = subdivide_triangle;

			if(subdivide_triangle)
			{
				for(uint32_t v = 0; v < 3; ++v)
				{
					{
					const unsigned int v0 = tris_in[t].vertex_indices[v];
					const unsigned int v1 = tris_in[t].vertex_indices[mod3[v + 1]];

					// Get vertex at the midpoint of this edge, or create it if it doesn't exist yet.

					const DUVertIndexPair edge(myMin(v0, v1), myMax(v0, v1)); // Key for the edge

					DUEdgeInfo& edge_info = edge_info_map[edge];

					if(edge_info.num_adjacent_subdividing_polys == 0)
					{
						// Create the edge midpoint vertex
						const unsigned int new_vert_index = verts_out.size();

						verts_out.push_back(DUVertex(
							(verts_in[v0].pos + verts_in[v1].pos) * 0.5f,
							(verts_in[v0].normal + verts_in[v1].normal)// * 0.5f
							));

						verts_out.back().adjacent_vert_0 = edge.v_a;
						verts_out.back().adjacent_vert_1 = edge.v_b;

						edge_info.midpoint_vert_index = new_vert_index;
					}

					edge_info.num_adjacent_subdividing_polys++;
					}

					// Create new uvs at edge midpoint, if not already created.

					if(num_uv_sets > 0)
					{
						const unsigned int uv0 = tris_in[t].uv_indices[v];
						const unsigned int uv1 = tris_in[t].uv_indices[mod3[v + 1]];

						const DUVertIndexPair edge_key(myMin(uv0, uv1), myMax(uv0, uv1)); // Key for the edge

						const std::map<DUVertIndexPair, unsigned int>::const_iterator result = uv_edge_map.find(edge_key);
						if(result == uv_edge_map.end())
						{
							uv_edge_map.insert(std::make_pair(edge_key, uvs_out.size() / num_uv_sets));

							for(uint32_t z = 0; z < num_uv_sets; ++z)
								uvs_out.push_back(
									lerpUVs(
										getUVs(uvs_in, num_uv_sets, uv0, z), 
										getUVs(uvs_in, num_uv_sets, uv1, z), 
										0.5f,
										options.wrap_u, options.wrap_v
										)
									);
						}
						// else midpoint uvs already created
					}
				}
			}
		}
	}


	// For each quad
	for(uint32_t q = 0; q < quads_in.size(); ++q)
	{
		if(quads_in[q].dimension == 2) // if 2-d (quad)
		{
			const uint32_t v[4] = { quads_in[q].vertex_indices[0], quads_in[q].vertex_indices[1],
									quads_in[q].vertex_indices[2], quads_in[q].vertex_indices[3] };

			// Create the quad's centroid vertex
			const uint32_t centroid_vert_index = verts_out.size();
			const uint32_t centroid_uv_index = uvs_out.size() / num_uv_sets;

			//quads_in[q].centre_vertex_index = centroid_vert_index;
			//quads_in[q].centre_uv_index = centroid_uv_index;
			quad_centre_data[q].first  = centroid_vert_index;
			quad_centre_data[q].second = centroid_uv_index;

			verts_out.push_back(DUVertex(
				(verts_in[v[0]].pos + verts_in[v[1]].pos + verts_in[v[2]].pos + verts_in[v[3]].pos) * 0.25f,
				(verts_in[v[0]].normal + verts_in[v[1]].normal + verts_in[v[2]].normal + verts_in[v[3]].normal)// * 0.25f
				));

			for(uint32_t z = 0; z < num_uv_sets; ++z)
			{
				//TODO: remove these lerpUVs
				// this is probably super b0rked and wrong...
				//const Vec2f uv_top = lerpUVs(getUVs(uvs_in, num_uv_sets, quads_in[q].uv_indices[0], z),
				//					 		 getUVs(uvs_in, num_uv_sets, quads_in[q].uv_indices[1], z), 0.5f, options.wrap_u, options.wrap_v);
				//const Vec2f uv_bot = lerpUVs(getUVs(uvs_in, num_uv_sets, quads_in[q].uv_indices[2], z),
				//							 getUVs(uvs_in, num_uv_sets, quads_in[q].uv_indices[3], z), 0.5f, options.wrap_u, options.wrap_v);

				//uvs_out.push_back(lerpUVs(uv_top, uv_bot, 0.5f, options.wrap_u, options.wrap_v));

				uvs_out.push_back((
					getUVs(uvs_in, num_uv_sets, quads_in[q].uv_indices[0], z) +
					getUVs(uvs_in, num_uv_sets, quads_in[q].uv_indices[1], z) +
					getUVs(uvs_in, num_uv_sets, quads_in[q].uv_indices[2], z) + 
					getUVs(uvs_in, num_uv_sets, quads_in[q].uv_indices[3], z)) * 0.25f);
			}


			// Create the quad's edge vertices
			for(uint32_t v = 0; v < 4; ++v)
			{
				const unsigned int v0 = quads_in[q].vertex_indices[v];
				const unsigned int v1 = quads_in[q].vertex_indices[mod4[v + 1]];

				// Get vertex at the midpoint of this edge, or create it if it doesn't exist yet.

				const DUVertIndexPair edge(myMin(v0, v1), myMax(v0, v1)); // Key for the edge

				DUEdgeInfo& edge_info = edge_info_map[edge];

				if(edge_info.num_adjacent_subdividing_polys == 0)
				{
					// Create the edge midpoint vertex
					const uint32_t new_vert_index = verts_out.size();

					verts_out.push_back(DUVertex(
						(verts_in[v0].pos + verts_in[v1].pos) * 0.5f,
						(verts_in[v0].normal + verts_in[v1].normal)// * 0.5f
						));

					verts_out.back().adjacent_vert_0 = edge.v_a;
					verts_out.back().adjacent_vert_1 = edge.v_b;

					edge_info.midpoint_vert_index = new_vert_index;
				}

				edge_info.num_adjacent_subdividing_polys++;

				// Create new uvs at edge midpoint, if not already created.

				if(num_uv_sets > 0)
				{
					const unsigned int uv0 = quads_in[q].uv_indices[v];
					const unsigned int uv1 = quads_in[q].uv_indices[mod4[v + 1]];

					const DUVertIndexPair edge_key(myMin(uv0, uv1), myMax(uv0, uv1)); // Key for the edge

					const std::map<DUVertIndexPair, uint32_t>::const_iterator result = uv_edge_map.find(edge_key);
					if(result == uv_edge_map.end())
					{
						uv_edge_map.insert(std::make_pair(edge_key, uvs_out.size() / num_uv_sets));

						for(uint32_t z = 0; z < num_uv_sets; ++z)
						{
							uvs_out.push_back((getUVs(uvs_in, num_uv_sets, uv0, z) + getUVs(uvs_in, num_uv_sets, uv1, z)) * 0.5f);
							/*uvs_out.push_back(
								lerpUVs(
									getUVs(uvs_in, num_uv_sets, uv0, z),
									getUVs(uvs_in, num_uv_sets, uv1, z),
									0.5f,
									options.wrap_u, options.wrap_v
								)
							);*/
						}
					}
					// else midpoint uvs already created
				}
			}
		}

	}





	// Mark border edges
	for(uint32_t t = 0; t < tris_in.size(); ++t)
		if(tris_in[t].dimension == 1) // border edge
		{
			const uint32_t& v0 = tris_in[t].vertex_indices[0];
			const uint32_t& v1 = tris_in[t].vertex_indices[1];
			edge_info_map[DUVertIndexPair(myMin(v0, v1), myMax(v0, v1))].border = true;
		}

	for(uint32_t q = 0; q < quads_in.size(); ++q)
		if(quads_in[q].dimension == 1) // border edge
		{
			const uint32_t& v0 = quads_in[q].vertex_indices[0];
			const uint32_t& v1 = quads_in[q].vertex_indices[1];
			edge_info_map[DUVertIndexPair(myMin(v0, v1), myMax(v0, v1))].border = true;
		}


	// Mark any new edge midpoint vertices as anchored that need to be
	for(std::map<DUVertIndexPair, DUEdgeInfo>::const_iterator i = edge_info_map.begin(); i != edge_info_map.end(); ++i)
		if(!(*i).second.border && (*i).second.num_adjacent_subdividing_polys == 1)
		{
			// This edge has a subdivided triangle on one side, and a non-subdivided triangle on the other side.
			// So we will 'anchor' the midpoint vertex to the average position of it's neighbours to avoid splitting.
			verts_out[(*i).second.midpoint_vert_index].anchored = true;
		}


	// Update polygons

	// Counters
	uint32_t num_tris_subdivided = 0;
	uint32_t num_tris_unchanged = 0;
	uint32_t num_quads_subdivided = 0;
	uint32_t num_quads_unchanged = 0;

	// For each triangle
	for(uint32_t t = 0; t < tris_in.size(); ++t)
	{
		if(tris_in[t].dimension == 0) // vertex polygon
		{
			// Can't subdivide it, so just copy it over
			tris_out.push_back(tris_in[t]);
		}
		else if(tris_in[t].dimension == 1) // edge polygon
		{
			const uint32_t v0 = tris_in[t].vertex_indices[0];
			const uint32_t v1 = tris_in[t].vertex_indices[1];
			const DUVertIndexPair edge(myMin(v0, v1), myMax(v0, v1)); // Key for the edge

			assert(edge_info_map.find(edge) != edge_info_map.end());

			const std::map<DUVertIndexPair, DUEdgeInfo>::iterator result = edge_info_map.find(edge);
			const DUEdgeInfo& edge_info = (*result).second;
			if(edge_info.num_adjacent_subdividing_polys > 0)
			{
				// A triangle adjacent to this edge is being subdivided.  So subdivide this edge 1-D polygon as well.
				assert(edge_info.midpoint_vert_index > 0);

				// Get the midpoint uv index
				unsigned int midpoint_uv_index = 0;
				if(num_uv_sets > 0)
				{
					const uint32_t uv0 = tris_in[t].uv_indices[0];
					const uint32_t uv1 = tris_in[t].uv_indices[1];
					const DUVertIndexPair uv_edge_key(myMin(uv0, uv1), myMax(uv0, uv1)); // Key for the edge
					assert(uv_edge_map.find(uv_edge_key) != uv_edge_map.end());
					midpoint_uv_index = uv_edge_map[uv_edge_key];
				}

				// Create new edges
				tris_out.push_back(DUTriangle(
						tris_in[t].vertex_indices[0],
						edge_info.midpoint_vert_index,
						666,
						tris_in[t].uv_indices[0],
						midpoint_uv_index, 
						666,
						tris_in[t].tri_mat_index,
						1 // dimension
					));

				tris_out.push_back(DUTriangle(
						edge_info.midpoint_vert_index,
						tris_in[t].vertex_indices[1],
						666,
						midpoint_uv_index,
						tris_in[t].uv_indices[1],
						666,
						tris_in[t].tri_mat_index,
						1 // dimension
					));
			}
			else
			{
				// not being subdivided, so just copy over polygon
				tris_out.push_back(tris_in[t]);
			}
		}
		else
		{
			if(subdividing_tri[t]) // If we are subdividing this triangle...
			{
				uint32_t midpoint_vert_indices[3]; // Indices of edge midpoint vertices in verts_out
				uint32_t midpoint_uv_indices[3] = { 0, 0, 0 };

				for(uint32_t v = 0; v < 3; ++v) // For each edge (v_i, v_i+1)
				{
					{
					const uint32_t v0 = tris_in[t].vertex_indices[v];
					const uint32_t v1 = tris_in[t].vertex_indices[mod3[v + 1]];
					const DUVertIndexPair edge(myMin(v0, v1), myMax(v0, v1)); // Key for the edge

					const std::map<DUVertIndexPair, DUEdgeInfo>::iterator result = edge_info_map.find(edge);
					assert(result != edge_info_map.end());

					midpoint_vert_indices[v] = (*result).second.midpoint_vert_index;
					}
					if(num_uv_sets > 0)
					{
						const uint32_t uv0 = tris_in[t].uv_indices[v];
						const uint32_t uv1 = tris_in[t].uv_indices[mod3[v + 1]];
						const DUVertIndexPair edge(myMin(uv0, uv1), myMax(uv0, uv1)); // Key for the edge

						const std::map<DUVertIndexPair, uint32_t>::iterator result = uv_edge_map.find(edge);
						assert(result != uv_edge_map.end());

						midpoint_uv_indices[v] = (*result).second;
					}
				}

				tris_out.push_back(DUTriangle(
						tris_in[t].vertex_indices[0],
						midpoint_vert_indices[0],
						midpoint_vert_indices[2],
						tris_in[t].uv_indices[0],
						midpoint_uv_indices[0],
						midpoint_uv_indices[2],
						tris_in[t].tri_mat_index,
						2 // dimension
				));

				tris_out.push_back(DUTriangle(
						midpoint_vert_indices[1],
						midpoint_vert_indices[2],
						midpoint_vert_indices[0],
						midpoint_uv_indices[1],
						midpoint_uv_indices[2],
						midpoint_uv_indices[0],
						tris_in[t].tri_mat_index,
						2 // dimension
				));

				tris_out.push_back(DUTriangle(
						midpoint_vert_indices[0],
						tris_in[t].vertex_indices[1],
						midpoint_vert_indices[1],
						midpoint_uv_indices[0],
						tris_in[t].uv_indices[1],
						midpoint_uv_indices[1],
						tris_in[t].tri_mat_index,
						2 // dimension
				));

				tris_out.push_back(DUTriangle(
						midpoint_vert_indices[2],
						midpoint_vert_indices[1],
						tris_in[t].vertex_indices[2],
						midpoint_uv_indices[2],
						midpoint_uv_indices[1],
						tris_in[t].uv_indices[2],
						tris_in[t].tri_mat_index,
						2 // dimension
				));

				num_tris_subdivided++;
			}
			else
			{
				// Else don't subdivide triangle.
				// Original vertices are already copied over, so just copy the triangle
				tris_out.push_back(tris_in[t]);
				num_tris_unchanged++;
			}
		}
	}








	// For each quad
	for(uint32_t q = 0; q < quads_in.size(); ++q)
	{
		if(quads_in[q].dimension == 0) // vertex polygon
		{
			// Can't subdivide it, so just copy it over
			quads_out.push_back(quads_in[q]);
		}
		else if(quads_in[q].dimension == 1) // edge polygon
		{
			const uint32_t v0 = quads_in[q].vertex_indices[0];
			const uint32_t v1 = quads_in[q].vertex_indices[1];
			const DUVertIndexPair edge(myMin(v0, v1), myMax(v0, v1)); // Key for the edge

			assert(edge_info_map.find(edge) != edge_info_map.end());

			const std::map<DUVertIndexPair, DUEdgeInfo>::iterator result = edge_info_map.find(edge);
			const DUEdgeInfo& edge_info = (*result).second;
			if(edge_info.num_adjacent_subdividing_polys > 0)
			{
				// A triangle adjacent to this edge is being subdivided.  So subdivide this edge 1-D polygon as well.
				assert(edge_info.midpoint_vert_index > 0);

				// Get the midpoint uv index
				uint32_t midpoint_uv_index = 0;
				if(num_uv_sets > 0)
				{
					const uint32_t uv0 = quads_in[q].uv_indices[0];
					const uint32_t uv1 = quads_in[q].uv_indices[1];
					const DUVertIndexPair uv_edge_key(myMin(uv0, uv1), myMax(uv0, uv1)); // Key for the edge
					assert(uv_edge_map.find(uv_edge_key) != uv_edge_map.end());
					midpoint_uv_index = uv_edge_map[uv_edge_key];
				}

				// Create new edges
				quads_out.push_back(DUQuad(
					quads_in[q].vertex_indices[0],
					edge_info.midpoint_vert_index,
					666,
					666,
					quads_in[q].uv_indices[0],
					midpoint_uv_index, 
					666,
					666,
					quads_in[q].mat_index,
					1 // dimension
					));

				quads_out.push_back(DUQuad(
					edge_info.midpoint_vert_index,
					quads_in[q].vertex_indices[1],
					666,
					666,
					midpoint_uv_index,
					quads_in[q].uv_indices[1],
					666,
					666,
					quads_in[q].mat_index,
					1 // dimension
					));
			}
			else
			{
				// not being subdivided, so just copy over polygon
				quads_out.push_back(quads_in[q]);
			}
		}
		else
		{
			//if(subdividing_quad[t]) // If we are subdividing this quad...
			{
				uint32_t midpoint_vert_indices[4]; // Indices of edge midpoint vertices in verts_out
				uint32_t midpoint_uv_indices[4] = { 0, 0, 0, 0 };
				const uint32_t centre_vert_idx = quad_centre_data[q].first;
				const uint32_t centre_uv_idx   = quad_centre_data[q].second;

				for(uint32_t v = 0; v < 4; ++v) // For each edge
				{
					{
						const uint32_t v0 = quads_in[q].vertex_indices[v];
						const uint32_t v1 = quads_in[q].vertex_indices[mod4[v + 1]];
						const DUVertIndexPair edge(myMin(v0, v1), myMax(v0, v1)); // Key for the edge

						const std::map<DUVertIndexPair, DUEdgeInfo>::iterator result = edge_info_map.find(edge);
						assert(result != edge_info_map.end());

						midpoint_vert_indices[v] = (*result).second.midpoint_vert_index;
					}
					if(num_uv_sets > 0)
					{
						const uint32_t uv0 = quads_in[q].uv_indices[v];
						const uint32_t uv1 = quads_in[q].uv_indices[mod4[v + 1]];
						const DUVertIndexPair edge(myMin(uv0, uv1), myMax(uv0, uv1)); // Key for the edge

						const std::map<DUVertIndexPair, uint32_t>::iterator result = uv_edge_map.find(edge);
						assert(result != uv_edge_map.end());

						midpoint_uv_indices[v] = (*result).second;
					}
				}

				// Centroid vertex and UV set indices are stored with the original quads
				{
					//midpoint_vert_indices[4] = quads_in[q].centre_vertex_index;
					//midpoint_uv_indices[4] = quads_in[q].centre_uv_index;
				}

				quads_out.push_back(DUQuad(
					quads_in[q].vertex_indices[0], midpoint_vert_indices[0], centre_vert_idx, midpoint_vert_indices[3],
					quads_in[q].uv_indices[0], midpoint_uv_indices[0], centre_uv_idx, midpoint_uv_indices[3],
					quads_in[q].mat_index, 2 // dimension
					));

				quads_out.push_back(DUQuad(
					midpoint_vert_indices[0], quads_in[q].vertex_indices[1], midpoint_vert_indices[1], centre_vert_idx,
					midpoint_uv_indices[0], quads_in[q].uv_indices[1], midpoint_uv_indices[1], centre_uv_idx,
					quads_in[q].mat_index, 2 // dimension
					));

				quads_out.push_back(DUQuad(
					centre_vert_idx, midpoint_vert_indices[1], quads_in[q].vertex_indices[2], midpoint_vert_indices[2],
					centre_uv_idx, midpoint_uv_indices[1], quads_in[q].uv_indices[2], midpoint_uv_indices[2],
					quads_in[q].mat_index, 2 // dimension
					));

				quads_out.push_back(DUQuad(
					midpoint_vert_indices[3], centre_vert_idx, midpoint_vert_indices[2], quads_in[q].vertex_indices[3],
					midpoint_uv_indices[3], centre_uv_idx, midpoint_uv_indices[2], quads_in[q].uv_indices[3],
					quads_in[q].mat_index, 2 // dimension
					));

				num_quads_subdivided++;
			}
			//else
			//{
			//	// Else don't subdivide quad.
			//	// Original vertices are already copied over, so just copy the triangle
			//	tris_out.push_back(tris_in[t]);
			//	num_tris_unchanged++;
			//}
		}
	}





	print_output.print("\t\tnum triangles subdivided: " + toString(num_tris_subdivided));
	print_output.print("\t\tnum triangles unchanged: " + toString(num_tris_unchanged));
	print_output.print("\t\tnum quads subdivided: " + toString(num_quads_subdivided));
	print_output.print("\t\tnum quads unchanged: " + toString(num_quads_unchanged));
}


static inline float w(unsigned int n_t, unsigned int n_q)
{
	if(n_q == 0 && n_t == 3)
		return 1.5f;
	else
		return 12.0f / (3.0f * (float)n_q + 2.0f * (float)n_t);
}


class VertexUVSetIndices
{
public:
	VertexUVSetIndices() : num_uv_set_indices(0) {}

	static const unsigned int MAX_NUM_UV_SET_INDICES = 8;
	unsigned int uv_set_indices[MAX_NUM_UV_SET_INDICES];
	unsigned int num_uv_set_indices;
};


static bool UVSetIndexPresent(const VertexUVSetIndices& indices, unsigned int index)
{
	for(unsigned int i=0; i<indices.num_uv_set_indices; ++i)
		if(indices.uv_set_indices[i] == index)
			return true;
	return false;
}




void DisplacementUtils::averagePass(
	const std::vector<DUTriangle>& tris, 
	const std::vector<DUQuad>& quads, 
	const std::vector<DUVertex>& verts,
	const std::vector<Vec2f>& uvs_in,
	unsigned int num_uv_sets,
	const DUOptions& options,
	std::vector<DUVertex>& new_verts_out,
	std::vector<Vec2f>& uvs_out
	)
{
	// Init vertex positions to (0,0,0)
	new_verts_out = verts;
	for(uint32_t v = 0; v < new_verts_out.size(); ++v)
		new_verts_out[v].pos = new_verts_out[v].normal = Vec3f(0.f, 0.f, 0.f);

	// Init vertex UVs to (0,0)
	uvs_out = uvs_in;
	for(uint32_t v = 0; v < uvs_out.size(); ++v)
		uvs_out[v] = Vec2f(0.f, 0.f);

	std::vector<uint32_t> dim(verts.size(), 2); // array containing dimension of each vertex
	std::vector<float> total_weight(verts.size(), 0.0f); // total weights per vertex
	//std::vector<float> total_uvs_weight(uvs_in.size() / num_uv_sets, 0.0f); // total weights per vertex
	std::vector<uint32_t> n_t(verts.size(), 0); // array containing number of triangles touching each vertex
	std::vector<uint32_t> n_q(verts.size(), 0); // array containing number of quads touching each vertex
	//std::vector<unsigned int> uv_n_t(uvs_in.size() / num_uv_sets, 0); // array containing number of polygons touching each UV group

	// For each vertex, we will store a list of the indices of UV coord pairs associated with the vertex
	std::vector<VertexUVSetIndices> vert_uv_set_indices(verts.size());

	/*std::vector<Vec2f> uv_offsets(uvs_in.size()); //verts.size() * num_uv_sets);

	// Init uv_offsets
	for(uint32_t i = 0; i < uv_offsets.size(); ++i)
	{
		uv_offsets[i].x = options.wrap_u ? ((uvs_in[i].x < 0.25f || uvs_in[i].x > 0.75f) ? 0.5f : 0.0f) : 0.0f;
		uv_offsets[i].y = options.wrap_v ? ((uvs_in[i].y < 0.25f || uvs_in[i].y > 0.75f) ? 0.5f : 0.0f) : 0.0f;

		//uv_offsets[i] = Vec2f(0.f, 0.f);
	}*/
	std::vector<Vec2f> uv_offsets(uvs_in.size());
	for(uint32_t i = 0; i < uv_offsets.size(); ++i)
	{
		uv_offsets[i] = Vec2f(0.5f, 0.5f) - uvs_in[i];
	}

	// Initialise dim
	for(uint32_t t = 0; t < tris.size(); ++t)
		for(uint32_t v = 0; v < tris[t].dimension + 1; ++v)
			dim[tris[t].vertex_indices[v]] = myMin(dim[tris[t].vertex_indices[v]], tris[t].dimension);
	for(uint32_t q = 0; q < quads.size(); ++q)
	{
		const int num_v = (quads[q].dimension == 0 || quads[q].dimension == 1) ? quads[q].dimension + 1 : 4;
		for(uint32_t v = 0; v < num_v; ++v)
			dim[quads[q].vertex_indices[v]] = myMin(dim[quads[q].vertex_indices[v]], quads[q].dimension);
	}

	// Initialise n_t
	for(uint32_t t = 0; t < tris.size(); ++t)
		for(uint32_t v = 0; v < tris[t].dimension + 1; ++v)
			n_t[tris[t].vertex_indices[v]]++;

	// Initialise n_t
	for(uint32_t q = 0; q < quads.size(); ++q)
	{
		const int num_v = (quads[q].dimension == 0 || quads[q].dimension == 1) ? quads[q].dimension + 1 : 4;
		for(uint32_t v = 0; v < num_v; ++v)
			n_q[quads[q].vertex_indices[v]]++;
	}

	// Initialise uv_n_t
	/*if(num_uv_sets > 0)
		for(unsigned int t=0; t<tris.size(); ++t)
			for(unsigned int v=0; v<tris[t].dimension+1; ++v)
				uv_n_t[tris[t].uv_indices[v]]++;*/


	// Initialise vert_uv_set_indices
	for(uint32_t t = 0; t < tris.size(); ++t) // For each triangle
		if(tris[t].dimension == 2) // If it's a triangle
			for(uint32_t i = 0; i < 3; ++i) // For each vertex
			{
				const uint32_t v_i = tris[t].vertex_indices[i]; // vertex index

				// If it's not in the list, and the list isn't full yet...
				if(!UVSetIndexPresent(vert_uv_set_indices[v_i], tris[t].uv_indices[i]) && vert_uv_set_indices[v_i].num_uv_set_indices < VertexUVSetIndices::MAX_NUM_UV_SET_INDICES)
					vert_uv_set_indices[v_i].uv_set_indices[vert_uv_set_indices[v_i].num_uv_set_indices++] = tris[t].uv_indices[i]; // Store the index of the UV pair.
			}

	for(uint32_t q = 0; q < quads.size(); ++q) // For each quad
		if(quads[q].dimension == 2) // If it's a quad
			for(uint32_t i = 0; i < 4; ++i) // For each vertex
			{
				const uint32_t v_i = quads[q].vertex_indices[i]; // vertex index

				// If it's not in the list, and the list isn't full yet...
				if(!UVSetIndexPresent(vert_uv_set_indices[v_i], quads[q].uv_indices[i]) && vert_uv_set_indices[v_i].num_uv_set_indices < VertexUVSetIndices::MAX_NUM_UV_SET_INDICES)
					vert_uv_set_indices[v_i].uv_set_indices[vert_uv_set_indices[v_i].num_uv_set_indices++] = quads[q].uv_indices[i]; // Store the index of the UV pair.
			}


	std::vector<Vec2f> uv_cent(num_uv_sets);

	for(uint32_t t = 0; t < tris.size(); ++t) // For each triangle
	{
		for(uint32_t v = 0; v < tris[t].dimension + 1; ++v) // For each vertex
		{
			const uint32_t v_i = tris[t].vertex_indices[v];

			if(dim[v_i] == tris[t].dimension) // Only add centroid if vertex has same dimension as polygon
			{
				Vec3f cent;
				float weight;

				if(tris[t].dimension == 0) // t is a vertex
				{
					cent = verts[v_i].pos;
					for(uint32_t z = 0; z < num_uv_sets; ++z)
						uv_cent[z] = getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[v], z);
					weight = 1.0f;
				}
				else if(tris[t].dimension == 1) // t is an edge
				{
					cent = (verts[v_i].pos + verts[tris[t].vertex_indices[(v + 1) % 2]].pos) * 0.5f;
					for(uint32_t z = 0; z < num_uv_sets; ++z)
						uv_cent[z] = lerpUVs(
							getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[v], z),
							getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[(v + 1) % 2], z),
							0.5f,
							options.wrap_u, options.wrap_v
							);
					weight = 1.0f;
				}
				else
				{
					// t is a triangle
					assert(tris[t].dimension == 2);
					const uint32_t v_i_plus_1  = tris[t].vertex_indices[mod3[v + 1]];
					const uint32_t v_i_minus_1 = tris[t].vertex_indices[mod3[v + 2]];

					cent = verts[v_i].pos * (1.0f / 4.0f) + (verts[v_i_plus_1].pos + verts[v_i_minus_1].pos) * (3.0f / 8.0f);


					//uv_cent = uvs_in[tris[t].uv_indices[v]] * (1.0f / 4.0f) + (uvs_in[tris[t].uv_indices[(v + 1) % 3]] + uvs_in[tris[t].uv_indices[(v + 2) % 3]]) * (3.0f / 8.0f);
					
					/*for(unsigned int z=0; z<num_uv_sets; ++z)
						uv_cent[z] = 
							getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[v], z) * (1.0f / 4.0f) + 
							(getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[(v + 1) % 3], z) + getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[(v + 2) % 3], z)) * (3.0f / 8.0f);*/

					for(uint32_t z = 0; z < num_uv_sets; ++z)
						uv_cent[z] = lerpUVs(
							getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[v], z),
							lerpUVs(
								getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[mod3[v + 1]], z),
								getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[mod3[v + 2]], z),
								0.5f,
								options.wrap_u, options.wrap_v
								),
							0.75f,
							options.wrap_u, options.wrap_v
							);
					/*for(unsigned int z=0; z<num_uv_sets; ++z)
						uv_cent[z] = 
							getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[v], z) * (1.0f / 4.0f) + 
							(getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[(v + 1) % 3], z) + getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[(v + 2) % 3], z)) * (3.0f / 8.0f);*/


					weight = (float)(NICKMATHS_PI / 3.0);
				}
			
				total_weight[v_i] += weight;
				
				//if(num_uv_sets > 0)
				//	total_uvs_weight[tris[t].uv_indices[v]] += weight;

				// Add cent to new vertex position
				new_verts_out[v_i].pos += cent * weight;

				//for(unsigned int z=0; z<num_uv_sets; ++z)
				//	getUVs(uvs_out, num_uv_sets, tris[t].uv_indices[v], z) += uv_cent[z] * weight;

				// Add uv_cent to new uv positions
				
				// For each UV pair associated with this vertex
				for(uint32_t i = 0; i < vert_uv_set_indices[v_i].num_uv_set_indices; ++i)
				for(uint32_t z = 0; z < num_uv_sets; ++z)
				{
					const uint32_t uv_index = uvIndex(num_uv_sets, vert_uv_set_indices[v_i].uv_set_indices[i], z);

					//const Vec2f raw_uvs = uv_cent[z];

					const Vec2f shifted_uvs(
						options.wrap_u ? fmod(uv_cent[z].x + uv_offsets[uv_index].x, 1.0f) : uv_cent[z].x,
						options.wrap_v ? fmod(uv_cent[z].y + uv_offsets[uv_index].y, 1.0f) : uv_cent[z].y
						);

					uvs_out[uv_index] += shifted_uvs * weight;

					//getUVs(uvs_out, num_uv_sets, vert_uv_set_indices[v_i].uv_set_indices[i], z) += uv_cent[z] * weight;

					/*if(getUVs(uvs_out, num_uv_sets, vert_uv_set_indices[v_i].uv_set_indices[i], z) == Vec2f(0.f, 0.f))
						getUVs(uvs_out, num_uv_sets, vert_uv_set_indices[v_i].uv_set_indices[i], z) = uv_cent[z] * weight;
					else
					{
						getUVs(uvs_out, num_uv_sets, vert_uv_set_indices[v_i].uv_set_indices[i], z) = lerpUVs(
							getUVs(uvs_out, num_uv_sets, vert_uv_set_indices[v_i].uv_set_indices[i], z) * (1.0 - weight),
							uv_cent[z],
							weight
							);
					}*/
				}


				//new_verts_out[v_i].normal += (verts[v_i].normal * (1.0f / 4.0f) + (verts[v_i_plus_1].normal + verts[v_i_minus_1].normal) * (3.0f / 8.0f)) * weight;

				//for(unsigned int z=0; z<RayMeshVertex::MAX_NUM_RAYMESH_TEXCOORD_SETS; ++z)
				//	new_verts_out[v_i].texcoords[z] += (verts[v_i].texcoords[z] * (1.0f / 4.0f) + (verts[v_i_plus_1].texcoords[z] + verts[v_i_minus_1].texcoords[z]) * (3.0f / 8.0f)) * weight;
			}
		}
	}

	for(uint32_t q = 0; q < quads.size(); ++q) // For each quad
	{
		const int num_v = (quads[q].dimension == 0 || quads[q].dimension == 1) ? quads[q].dimension + 1 : 4;

		for(uint32_t v = 0; v < num_v; ++v) // For each vertex
		{
			const uint32_t v0 = quads[q].vertex_indices[v];

			if(dim[v0] == quads[q].dimension) // Only add centroid if vertex has same dimension as polygon
			{
				Vec3f cent;
				float weight;

				if(quads[q].dimension == 0) // q is a vertex
				{
					cent = verts[v0].pos;
					for(uint32_t z = 0; z < num_uv_sets; ++z)
						uv_cent[z] = getUVs(uvs_in, num_uv_sets, quads[q].uv_indices[v], z);
					weight = 1.0f;
				}
				else if(quads[q].dimension == 1) // q is an edge
				{
					cent = (verts[v0].pos + verts[quads[q].vertex_indices[(v + 1) % 2]].pos) * 0.5f;
					for(uint32_t z = 0; z < num_uv_sets; ++z)
					{
						if(options.wrap_u)
						{
							uv_cent[z] = 
								doMod(uv_offsets[quads[q].uv_indices[v]] + getUVs(uvs_in, num_uv_sets, quads[q].uv_indices[v], z)) * 0.5f + 
								doMod(uv_offsets[quads[q].uv_indices[v]] + getUVs(uvs_in, num_uv_sets, quads[q].uv_indices[v], z)) * 0.5f;
						}
						else
						{
							uv_cent[z] = 
								getUVs(uvs_in, num_uv_sets, quads[q].uv_indices[v], z) * 0.5f + 
								getUVs(uvs_in, num_uv_sets, quads[q].uv_indices[v], z) * 0.5f;
						}
					/*uv_cent[z] = lerpUVs(
						getUVs(uvs_in, num_uv_sets, quads[q].uv_indices[v], z),
						getUVs(uvs_in, num_uv_sets, quads[q].uv_indices[(v + 1) % 2], z),
						0.5f,
						options.wrap_u, options.wrap_v
						);*/
					}
					weight = 1.0f;
				}
				else
				{
					// q is a quad
					assert(quads[q].dimension == 2);
					const uint32_t v1 = quads[q].vertex_indices[mod4[v + 1]];
					const uint32_t v2 = quads[q].vertex_indices[mod4[v + 2]];
					const uint32_t v3 = quads[q].vertex_indices[mod4[v + 3]];

					cent = (verts[v0].pos + verts[v1].pos + verts[v2].pos + verts[v3].pos) * 0.25f;

					//uv_cent = uvs_in[tris[t].uv_indices[v]] * (1.0f / 4.0f) + (uvs_in[tris[t].uv_indices[(v + 1) % 3]] + uvs_in[tris[t].uv_indices[(v + 2) % 3]]) * (3.0f / 8.0f);

					/*for(unsigned int z=0; z<num_uv_sets; ++z)
					uv_cent[z] = 
					getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[v], z) * (1.0f / 4.0f) + 
					(getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[(v + 1) % 3], z) + getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[(v + 2) % 3], z)) * (3.0f / 8.0f);*/

					const uint32 uv0 = quads[q].uv_indices[v];
					const uint32 uv1 = quads[q].uv_indices[mod4[v + 1]];
					const uint32 uv2 = quads[q].uv_indices[mod4[v + 2]];
					const uint32 uv3 = quads[q].uv_indices[mod4[v + 3]];

					for(uint32_t z = 0; z < num_uv_sets; ++z)
					{
						//TEMP
						std::vector<Vec2f> uvs(4);
						uvs[0] = getUVs(uvs_in, num_uv_sets, uv0, z);
						uvs[1] = getUVs(uvs_in, num_uv_sets, uv1, z);
						uvs[2] = getUVs(uvs_in, num_uv_sets, uv2, z);
						uvs[3] = getUVs(uvs_in, num_uv_sets, uv3, z);

						if(options.wrap_u)
						{

							//const std::vector<float> weights(4, 0.25f);
							//uv_cent[z] = uvCombination(uvs, weights);
							uv_cent[z] = doMod(uv_offsets[uv0] + uvs[0]) * 0.25f;
							uv_cent[z] += doMod(uv_offsets[uv0] + uvs[1]) * 0.25f;
							uv_cent[z] += doMod(uv_offsets[uv0] + uvs[2]) * 0.25f;
							uv_cent[z] += doMod(uv_offsets[uv0] + uvs[3]) * 0.25f;
						}
						else
						{
							uv_cent[z] = (uvs[0] + uvs[1] + uvs[2] + uvs[3]) * 0.25f;
						}

						/*uv_cent[z] = lerpUVs(
							lerpUVs(
								getUVs(uvs_in, num_uv_sets, uv0, z),
								getUVs(uvs_in, num_uv_sets, uv1, z),
								0.5f,
								options.wrap_u, options.wrap_v
							),
							lerpUVs(
								getUVs(uvs_in, num_uv_sets, uv2, z),
								getUVs(uvs_in, num_uv_sets, uv3, z),
								0.5f,
								options.wrap_u, options.wrap_v
							),
							0.5f,
							options.wrap_u, options.wrap_v
						);*/
					}

					/*for(unsigned int z=0; z<num_uv_sets; ++z)
					uv_cent[z] = 
					getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[v], z) * (1.0f / 4.0f) + 
					(getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[(v + 1) % 3], z) + getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[(v + 2) % 3], z)) * (3.0f / 8.0f);*/


					weight = (float)(NICKMATHS_PI / 2.0);
				}

				total_weight[v0] += weight;

				//if(num_uv_sets > 0)
				//	total_uvs_weight[tris[t].uv_indices[v]] += weight;

				// Add cent to new vertex position
				new_verts_out[v0].pos += cent * weight;

				//for(unsigned int z=0; z<num_uv_sets; ++z)
				//	getUVs(uvs_out, num_uv_sets, tris[t].uv_indices[v], z) += uv_cent[z] * weight;

				// Add uv_cent to new uv positions

				// For each UV pair associated with this vertex
				for(uint32_t i = 0; i < vert_uv_set_indices[v0].num_uv_set_indices; ++i)
				for(uint32_t z = 0; z < num_uv_sets; ++z)
				{
					const uint32_t uv_index = uvIndex(num_uv_sets, vert_uv_set_indices[v0].uv_set_indices[i], z);

					//const Vec2f raw_uvs = uv_cent[z];

					/*const Vec2f shifted_uvs(
						options.wrap_u ? fmod(uv_cent[z].x + uv_offsets[uv_index].x, 1.0f) : uv_cent[z].x,
						options.wrap_v ? fmod(uv_cent[z].y + uv_offsets[uv_index].y, 1.0f) : uv_cent[z].y
						);

					uvs_out[uv_index] += shifted_uvs * weight;*/
					uvs_out[uv_index] += uv_cent[z] * weight;

					//getUVs(uvs_out, num_uv_sets, vert_uv_set_indices[v_i].uv_set_indices[i], z) += uv_cent[z] * weight;

					/*if(getUVs(uvs_out, num_uv_sets, vert_uv_set_indices[v_i].uv_set_indices[i], z) == Vec2f(0.f, 0.f))
					getUVs(uvs_out, num_uv_sets, vert_uv_set_indices[v_i].uv_set_indices[i], z) = uv_cent[z] * weight;
					else
					{
					getUVs(uvs_out, num_uv_sets, vert_uv_set_indices[v_i].uv_set_indices[i], z) = lerpUVs(
					getUVs(uvs_out, num_uv_sets, vert_uv_set_indices[v_i].uv_set_indices[i], z) * (1.0 - weight),
					uv_cent[z],
					weight
					);
					}*/
				}


				//new_verts_out[v_i].normal += (verts[v_i].normal * (1.0f / 4.0f) + (verts[v_i_plus_1].normal + verts[v_i_minus_1].normal) * (3.0f / 8.0f)) * weight;

				//for(unsigned int z=0; z<RayMeshVertex::MAX_NUM_RAYMESH_TEXCOORD_SETS; ++z)
				//	new_verts_out[v_i].texcoords[z] += (verts[v_i].texcoords[z] * (1.0f / 4.0f) + (verts[v_i_plus_1].texcoords[z] + verts[v_i_minus_1].texcoords[z]) * (3.0f / 8.0f)) * weight;
			}
		}
	}



	// Do 'normalize vertices by the weights' and 'apply correction only for quads and triangles' step.
	for(uint32_t v = 0; v < new_verts_out.size(); ++v)
	{
		const float w_val = w(n_t[v], n_q[v]);

		new_verts_out[v].pos /= total_weight[v];

		//NOTE: Missing if dim == 2 check here?
		if(dim[v] == 2) // NEW
		{
			new_verts_out[v].pos = Maths::uncheckedLerp(
				verts[v].pos, 
				new_verts_out[v].pos, 
				w_val
				); //verts[v].pos + (new_verts_out[v].pos - verts[v].pos) * w_val;


			//new_verts_out[v].normal /= total_weight[v];
			//new_verts_out[v].normal = verts[v].normal + (new_verts_out[v].normal - verts[v].normal) * w_val;
			//new_verts_out[v].normal.normalise();


			for(uint32_t z = 0; z < num_uv_sets; ++z)
				for(uint32_t i = 0; i < vert_uv_set_indices[v].num_uv_set_indices; ++i) // for each UV set at this vertex
				{
					const uint32_t uv_index = uvIndex(num_uv_sets, vert_uv_set_indices[v].uv_set_indices[i], z);


					uvs_out[uv_index] /= total_weight[v];

					uvs_out[uv_index] = uvs_in[uv_index] + (uvs_out[uv_index] - uvs_in[uv_index]) * w_val;

					// Apply inverse transformation
					if(options.wrap_u)
					{
						uvs_out[uv_index] -= uv_offsets[uv_index];
						uvs_out[uv_index] = uvs_out[uv_index];
					}



					/*uvs_out[uv_index] /= total_weight[v];

					uvs_out[uv_index] -= uv_offsets[uv_index];

					uvs_out[uv_index] = lerpUVs(//Maths::uncheckedLerp(
						uvs_in[uv_index], 
						uvs_out[uv_index], 
						w_val,
						options.wrap_u, options.wrap_v
						);*/
	/*
					getUVs(uvs_out, num_uv_sets, vert_uv_set_indices[v].uv_set_indices[i], z) /= total_weight[v];

					getUVs(uvs_out, num_uv_sets, vert_uv_set_indices[v].uv_set_indices[i], z) = Maths::uncheckedLerp(
						getUVs(uvs_in, num_uv_sets, vert_uv_set_indices[v].uv_set_indices[i], z), 
						getUVs(uvs_out, num_uv_sets, vert_uv_set_indices[v].uv_set_indices[i], z), 
						w_val
						);*/
				}
		} // end if dim == 2


	}


	/*for(unsigned int t=0; t<tris.size(); ++t) // For each poly
		if(tris[t].dimension == 2) // If it's a triangle
		{
			for(unsigned int z=0; z<num_uv_sets; ++z)
			{

				bool u_wraps = false;
				for(unsigned int i=0; i<3; ++i) // For each vertex
				{
					const unsigned int v_i = tris[t].vertex_indices[i]; // vertex index

					for(unsigned int s=0; s<vert_uv_set_indices[v_i].num_uv_set_indices; ++s)
					{*/


	for(uint32_t t = 0; t < tris.size(); ++t) // For each triangle
		if(tris[t].dimension == 2) // If it's a triangle
		{
			for(uint32_t z = 0; z < num_uv_sets; ++z)
			{
				bool u_wraps = false;
				bool v_wraps = false;

				for(uint32_t i = 0; i < 3; ++i) // For each vertex
				{
					for(uint32_t q = 0; q < 3; ++q) // For each other vertex
					{
						const Vec2f dist = getUVs(uvs_out, num_uv_sets, tris[t].uv_indices[i], z) - getUVs(uvs_out, num_uv_sets, tris[t].uv_indices[q], z);
						if(fabs(dist.x) > 0.5f)
							u_wraps = true;
						if(fabs(dist.y) > 0.5f)
							v_wraps = true;
					}
				}

				if(options.wrap_u && u_wraps)
					for(uint32_t i = 0; i < 3; ++i) // For each vertex
						if(getUVs(uvs_out, num_uv_sets, tris[t].uv_indices[i], z).x < 0.25f)
							getUVs(uvs_out, num_uv_sets, tris[t].uv_indices[i], z).x += 1.0f;
				if(options.wrap_v && v_wraps)
					for(uint32_t i = 0; i < 3; ++i) // For each vertex
						if(getUVs(uvs_out, num_uv_sets, tris[t].uv_indices[i], z).y < 0.25f)
							getUVs(uvs_out, num_uv_sets, tris[t].uv_indices[i], z).y += 1.0f;
			}
		}


	


	/*if(num_uv_sets > 0)
		for(unsigned int t=0; t<tris.size(); ++t)
			if(tris[t].dimension == 2)
			{
				for(unsigned int i=0; i<3; ++i)
				{
					const unsigned int v = tris[t].vertex_indices[i];

					const float w_val = w(n_t[v], 0);

					const unsigned int uv_index = tris[t].uv_indices[i];

					for(unsigned int z=0; z<num_uv_sets; ++z)
					{
						getUVs(uvs_out, num_uv_sets, uv_index, z) /= total_weight[v];
						getUVs(uvs_out, num_uv_sets, uv_index, z) = Maths::uncheckedLerp(getUVs(uvs_in, num_uv_sets, uv_index, z), getUVs(uvs_out, num_uv_sets, uv_index, z), w_val);
					}
				}
			}*/




	/*if(num_uv_sets > 0)
		for(unsigned int v=0; v<uvs_out.size() / num_uv_sets; ++v)
		{
			const float w_val = w(uv_n_t[v], 0);

			for(unsigned int z=0; z<num_uv_sets; ++z)
			{
				getUVs(uvs_out, num_uv_sets, v, z) /= total_uvs_weight[v];
				//getUVs(uvs_out, num_uv_sets, v, z) = getUVs(uvs_in, num_uv_sets, v, z) + (getUVs(uvs_out, num_uv_sets, v, z) - getUVs(uvs_in, num_uv_sets, v, z)) * w_val;
				getUVs(uvs_out, num_uv_sets, v, z) = Maths::uncheckedLerp(getUVs(uvs_in, num_uv_sets, v, z), getUVs(uvs_out, num_uv_sets, v, z), w_val);
			}
		}*/

	// Set all anchored vertex positions back to the midpoint between the vertex's 'parent positions'
	for(uint32_t v = 0; v < new_verts_out.size(); ++v)
		if(verts[v].anchored)
			new_verts_out[v].pos = (new_verts_out[verts[v].adjacent_vert_0].pos + new_verts_out[verts[v].adjacent_vert_1].pos) * 0.5f;

	// TEMP HACK:
	//uvs_out = uvs_in;
}




#if (BUILD_TESTS)
void DisplacementUtils::test()
{
	double z = fmod(-1.25, 1.0);

	testAssert(epsEqual(wrappedLerp(0.1, 0.3, 0.5), 0.2));
	testAssert(epsEqual(wrappedLerp(0.3, 0.1, 0.5), 0.2));

	testAssert(epsEqual(wrappedLerp(0.9, 0.1, 0.25), 0.95));
	testAssert(epsEqual(wrappedLerp(0.1, 0.9, 0.75), 0.95));

	testAssert(epsEqual(wrappedLerp(0.9, 0.1, 0.75), 1.05));
	testAssert(epsEqual(wrappedLerp(0.1, 0.9, 0.25), 1.05));

	testAssert(epsEqual(wrappedLerp(-0.1, 0.1, 0.5), 0.0));
	testAssert(epsEqual(wrappedLerp(0.1, -0.1, 0.5), 0.0));

	{
		std::vector<Vec2f> uvs;
		uvs.push_back(Vec2f(0.1, 0.1));
		uvs.push_back(Vec2f(0.3, 0.3));

		const std::vector<float> weights(2, 0.5);

		const Vec2f v = uvCombination(uvs, weights);

		testAssert(epsEqual(v.x, 0.2f));
		testAssert(epsEqual(v.y, 0.2f));
	}

	{
		std::vector<Vec2f> uvs;
		uvs.push_back(Vec2f(0.3, 0.3));
		uvs.push_back(Vec2f(0.1, 0.1));

		const std::vector<float> weights(2, 0.5);

		const Vec2f v = uvCombination(uvs, weights);

		testAssert(epsEqual(v.x, 0.2f));
		testAssert(epsEqual(v.y, 0.2f));
	}

	{
		std::vector<Vec2f> uvs;
		uvs.push_back(Vec2f(0.9, 0.9));
		uvs.push_back(Vec2f(0.1, 0.1));

		std::vector<float> weights;
		weights.push_back(0.75f);
		weights.push_back(0.25f);

		const Vec2f v = uvCombination(uvs, weights);

		testAssert(epsEqual(v.x, 0.95f));
		testAssert(epsEqual(v.y, 0.95f));
	}

	/////////////////////	
	std::vector<RayMeshVertex> vertices(4);
	std::vector<Vec2f> uvs(4);
	js::Vector<RayMeshTriangle, 16> triangles;
	js::Vector<RayMeshQuad, 16> quads(1);

	js::Vector<RayMeshTriangle, 16> triangles_out;
	std::vector<RayMeshVertex> verts_out;
	std::vector<Vec2f> uvs_out;


	// Quad vertices in CCW order from top right, facing up
	vertices[0] = RayMeshVertex(Vec3f( 1,  1, 0), Vec3f(0, 0, 1));
	vertices[1] = RayMeshVertex(Vec3f(-1,  1, 0), Vec3f(0, 0, 1));
	vertices[2] = RayMeshVertex(Vec3f(-1, -1, 0), Vec3f(0, 0, 1));
	vertices[3] = RayMeshVertex(Vec3f( 1, -1, 0), Vec3f(0, 0, 1));

	uvs[0] = Vec2f(1, 1);
	uvs[1] = Vec2f(0, 1);
	uvs[2] = Vec2f(0, 0);
	uvs[3] = Vec2f(1, 0);


	quads[0] = RayMeshQuad(0, 1, 2, 3, 0);
	quads[0].uv_indices[0] = 0;
	quads[0].uv_indices[1] = 1;
	quads[0].uv_indices[2] = 2;
	quads[0].uv_indices[3] = 3;

	StandardPrintOutput print_output;
	ThreadContext context;

	std::vector<Reference<MaterialBinding> > materials;
	materials.push_back(Reference<MaterialBinding>(new MaterialBinding(Reference<Material>(new Diffuse(
		std::vector<TextureUnit*>(),
		Reference<SpectrumMatParameter>(NULL),
		Reference<DisplaceMatParameter>(NULL),
		Reference<DisplaceMatParameter>(NULL),
		Reference<SpectrumMatParameter>(NULL),
		Reference<SpectrumMatParameter>(NULL),
		0, // layer_index
		false // random_triangle_colours
	)))));

	materials[0]->uv_set_indices.push_back(0);

	DUOptions options;
	options.object_to_camera = Matrix4f::identity();
	options.wrap_u = false;
	options.wrap_v = false;
	options.view_dependent_subdivision = false;
	options.pixel_height_at_dist_one = 1;
	options.subdivide_pixel_threshold = 0;
	options.subdivide_curvature_threshold = 0;
	options.displacement_error_threshold = 0;
	options.max_num_subdivisions = 1;

	DisplacementUtils::subdivideAndDisplace(
		print_output,
		context,
		materials,
		true, // smooth
		triangles,
		quads,
		vertices,
		uvs,
		1, // num uv sets
		options,
		triangles_out,
		verts_out,
		uvs_out
	);

	for(size_t i=0; i<verts_out.size(); ++i)
	{
		conPrint(toString(verts_out[i].pos.x) + ", " + toString(verts_out[i].pos.y) + ", " + toString(verts_out[i].pos.z));
	}

	for(size_t i=0; i<triangles_out.size(); ++i)
	{
		conPrint(toString(triangles_out[i].vertex_indices[0]) + ", " + toString(triangles_out[i].vertex_indices[1]) + ", " + toString(triangles_out[i].vertex_indices[2]));

		for(int v=0; v<3; ++v)
		{
			conPrint("uv " + toString(uvs_out[triangles_out[i].uv_indices[v]].x) + ", " + toString(uvs_out[triangles_out[i].uv_indices[v]].y));
		}
	}
}


#endif
