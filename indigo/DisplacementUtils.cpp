/*=====================================================================
DisplacementUtils.cpp
---------------------
File created by ClassTemplate on Thu May 15 20:31:26 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "DisplacementUtils.h"


#include "../maths/rect2.h"
#include "../maths/mathstypes.h"
#include "../graphics/TriBoxIntersection.h"
#include "ScalarMatParameter.h"
#include "VoidMedium.h"
#include "TestUtils.h"


DisplacementUtils::DisplacementUtils()
{	
}


DisplacementUtils::~DisplacementUtils()
{	
}


static inline const Vec3f triGeometricNormal(const std::vector<DUVertex>& verts, unsigned int v0, unsigned int v1, unsigned int v2)
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


static inline const Vec2f& getUVs(const std::vector<Vec2f>& uvs, unsigned int num_uv_sets, unsigned int uv_index, unsigned int set_index)
{
	assert(num_uv_sets > 0);
	assert(set_index < num_uv_sets);
	return uvs[uv_index * num_uv_sets + set_index];
}


static inline unsigned int uvIndex(unsigned int num_uv_sets, unsigned int uv_index, unsigned int set_index)
{
	return uv_index * num_uv_sets + set_index;
}


static inline Vec2f& getUVs(std::vector<Vec2f>& uvs, unsigned int num_uv_sets, unsigned int uv_index, unsigned int set_index)
{
	assert(num_uv_sets > 0);
	assert(set_index < num_uv_sets);
	return uvs[uv_index * num_uv_sets + set_index];
}

static void computeVertexNormals(const std::vector<DUTriangle>& triangles, std::vector<DUVertex>& vertices)
{
	for(unsigned int i=0; i<vertices.size(); ++i)
		vertices[i].normal = Vec3f(0.f, 0.f, 0.f);

	for(unsigned int t=0; t<triangles.size(); ++t)
	{
		if(triangles[t].dimension == 2)
		{
			const Vec3f tri_normal = triGeometricNormal(
					vertices, 
					triangles[t].vertex_indices[0], 
					triangles[t].vertex_indices[1], 
					triangles[t].vertex_indices[2]
				);
			for(int i=0; i<3; ++i)
				vertices[triangles[t].vertex_indices[i]].normal += tri_normal;
		}
	}
	
	for(unsigned int i=0; i<vertices.size(); ++i)
		vertices[i].normal.normalise();
}




void DisplacementUtils::subdivideAndDisplace(
	ThreadContext& context,
	const Object& object,
	//const CoordFramed& camera_coordframe_os, 
	//double pixel_height_at_dist_one, 
	//double subdivide_pixel_threshold, 
	//double subdivide_curvature_threshold,
	//unsigned int num_subdivisions,
	//const std::vector<Plane<float> >& camera_clip_planes,
	bool smooth,
	const std::vector<RayMeshTriangle>& triangles_in, 
	const std::vector<RayMeshVertex>& vertices_in,
	const std::vector<Vec2f>& uvs_in,
	unsigned int num_uv_sets,
	const DUOptions& options,
	std::vector<RayMeshTriangle>& tris_out, 
	std::vector<RayMeshVertex>& verts_out,
	std::vector<Vec2f>& uvs_out
	)
{
	// Convert RayMeshTriangle's to DUTriangle's
	std::vector<DUTriangle> temp_tris(triangles_in.size());
	
	for(unsigned int i=0; i<temp_tris.size(); ++i)
		temp_tris[i] = DUTriangle(
			triangles_in[i].vertex_indices[0], triangles_in[i].vertex_indices[1], triangles_in[i].vertex_indices[2],
			triangles_in[i].uv_indices[0], triangles_in[i].uv_indices[1], triangles_in[i].uv_indices[2],
			triangles_in[i].tri_mat_index,
			2 // dimension
			);

	// Convert RayMeshVertex's to DUVertex's
	std::vector<DUVertex> temp_verts(vertices_in.size());
	for(unsigned int i=0; i<temp_verts.size(); ++i)
		temp_verts[i] = DUVertex(vertices_in[i].pos, vertices_in[i].normal);


	// Add edge and vertex polygons
	{
		std::map<DUVertIndexPair, unsigned int> num_adjacent_tris; // Map from edge -> num adjacent tris

		for(unsigned int t=0; t<temp_tris.size(); ++t)
		{
			for(unsigned int v=0; v<3; ++v)
			{
				const unsigned int v_i = temp_tris[t].vertex_indices[v];
				const unsigned int v_i1 = temp_tris[t].vertex_indices[(v + 1) % 3];
				const DUVertIndexPair edge(myMin(v_i, v_i1), myMax(v_i, v_i1)); // Key for the edge

				num_adjacent_tris[edge]++;
			}
		}

		const unsigned int initial_num_temp_tris = temp_tris.size(); // precompute this, as temp_tris is being appended to.
		
		for(unsigned int t=0; t<initial_num_temp_tris; ++t) // For each original triangle...
		{
			for(unsigned int v=0; v<3; ++v)
			{
				const unsigned int v_i = temp_tris[t].vertex_indices[v];
				const unsigned int v_i1 = temp_tris[t].vertex_indices[(v + 1) % 3];
				
				const DUVertIndexPair edge(myMin(v_i, v_i1), myMax(v_i, v_i1)); // Key for the edge

				if(num_adjacent_tris[edge] == 1)
				{
					// this is an edge edge aka. boundary !!! :)
					temp_tris.push_back(DUTriangle(
						v_i, v_i1, 666,
						temp_tris[t].uv_indices[v], temp_tris[t].uv_indices[(v + 1) % 3], 666,
						666, 1));
				}
			}
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
	std::vector<DUVertex> temp_verts2;
	std::vector<Vec2f> temp_uvs2;

	for(unsigned int i=0; i<options.max_num_subdivisions; ++i)
	{
		conPrint("\tDoing subdivision level " + toString(i) + "...");

		linearSubdivision(
			context,
			object,
			//materials,
			//camera_coordframe_os, 
			//pixel_height_at_dist_one, 
			//subdivide_pixel_threshold, 
			//subdivide_curvature_threshold,
			//camera_clip_planes,
			temp_tris, // tris in
			temp_verts, // verts in
			temp_uvs, // uvs in
			num_uv_sets,
			options,
			temp_tris2, // tris out
			temp_verts2, // verts out
			temp_uvs2 // uvs out
			);
		
		if(smooth)
			averagePass(temp_tris2, temp_verts2, temp_uvs2, num_uv_sets, options, temp_verts, temp_uvs);
		else
		{
			temp_verts = temp_verts2;
			temp_uvs = temp_uvs2;
		}
		temp_tris = temp_tris2;

		// Recompute vertex normals
		computeVertexNormals(temp_tris, temp_verts);

		conPrint("\t\tresulting num vertices: " + toString(temp_verts.size()));
		conPrint("\t\tresulting num triangles: " + toString(temp_tris.size()));
		conPrint("\t\tDone.");
	}

	// Apply the final displacement

	std::vector<bool> tris_unclipped;
	displace(
		context,
		object,
		true, // use anchoring
		//materials, 
		temp_tris, // tris in
		temp_verts, // verts in
		temp_uvs, // uvs in
		num_uv_sets,
		temp_verts2, // verts out
		&tris_unclipped
		);
	temp_verts = temp_verts2;

	// Build tris_out
	tris_out.resize(0);
	for(unsigned int i=0; i<temp_tris.size(); ++i)
		if(tris_unclipped[i] && temp_tris[i].dimension == 2)
		{
			tris_out.push_back(RayMeshTriangle(temp_tris[i].vertex_indices[0], temp_tris[i].vertex_indices[1], temp_tris[i].vertex_indices[2], temp_tris[i].tri_mat_index));
			
			for(unsigned int c=0; c<3; ++c)
				tris_out.back().uv_indices[c] = temp_tris[i].uv_indices[c];
		}


	// Recompute all vertex normals, as they will be completely wrong by now due to any displacement.
	computeVertexNormals(temp_tris, temp_verts);
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
	for(unsigned int i=0; i<verts_out.size(); ++i)
		verts_out[i] = RayMeshVertex(temp_verts[i].pos, temp_verts[i].normal);

	uvs_out = temp_uvs;
}



class DUTexCoordEvaluator : public TexCoordEvaluator
{
public:
	DUTexCoordEvaluator(){}
	~DUTexCoordEvaluator(){}

	virtual const Vec2d getTexCoords(const HitInfo& hitinfo, unsigned int texcoords_set) const
	{
		return texcoords[texcoords_set];
	}


	virtual void getTexCoordPartialDerivs(const HitInfo& hitinfo, unsigned int texcoord_set, double& ds_du_out, double& ds_dv_out, double& dt_du_out, double& dt_dv_out) const
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


	std::vector<Vec2d> texcoords;

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
								const Object& object,
								const DUTriangle& triangle, 
								const std::vector<DUVertex>& verts,
								const std::vector<Vec2f>& uvs,
								unsigned int num_uv_sets,
								float b1, // barycentric coords
								float b2
								)
{
	const Material& material = object.getMaterial(triangle.tri_mat_index);
	
	if(material.displacing())
	{
		DUTexCoordEvaluator du_texcoord_evaluator;
		du_texcoord_evaluator.texcoords.resize(num_uv_sets);

		// Set up UVs
		const float b0 = (1.0f - b1) - b2;
		for(unsigned int z=0; z<num_uv_sets; ++z)
		{
			du_texcoord_evaluator.texcoords[z] = toVec2d(
				getUVs(uvs, num_uv_sets, triangle.uv_indices[0], z) * b0 + 
				getUVs(uvs, num_uv_sets, triangle.uv_indices[1], z) * b1 + 
				getUVs(uvs, num_uv_sets, triangle.uv_indices[2], z) * b2
				);
		}

		HitInfo hitinfo(std::numeric_limits<unsigned int>::max(), Vec2d(-666.0, -666.0));

		const MaterialBinding& material_binding = object.getMaterialBinding(triangle.tri_mat_index);

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
								const Object& object,
								const DUTriangle& triangle, 
								const std::vector<DUVertex>& verts,
								const std::vector<Vec2f>& uvs,
								unsigned int num_uv_sets,
								int res
								)
{
	// Early out if material not displacing:
	if(!object.getMaterial(triangle.tri_mat_index).displacing())
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
				evalDisplacement(context, object, triangle, verts, uvs, num_uv_sets, b1, b2)
				);

			max_error = myMax(max_error, error);
		}

	return max_error;
}


/*
Apply displacement to the given vertices, storing the displaced vertices in verts_out


*/
void DisplacementUtils::displace(ThreadContext& context, 
								 const Object& object,
								 bool use_anchoring, 
								 const std::vector<DUTriangle>& triangles, 
								 const std::vector<DUVertex>& verts_in, 
								 const std::vector<Vec2f>& uvs,
								 unsigned int num_uv_sets,
								 std::vector<DUVertex>& verts_out, 
								 std::vector<bool>* unclipped_out
								 )
{
	verts_out = verts_in;

	if(unclipped_out)
	{
		unclipped_out->resize(triangles.size());
		for(unsigned int i=0; i<unclipped_out->size(); ++i)
			(*unclipped_out)[i] = true;
	}


	DUTexCoordEvaluator du_texcoord_evaluator;
	du_texcoord_evaluator.texcoords.resize(num_uv_sets);


	//std::vector<bool> vert_displaced(verts_in.size(), false); // Only displace each vertex once

	VoidMedium temp_void_medium;

	// For each triangle
	for(unsigned int t=0; t<triangles.size(); ++t)
	{
		if(triangles[t].dimension == 2)
		{
			const unsigned int material_index = triangles[t].tri_mat_index;
			const Material* material = &object.getMaterial(material_index); //materials[triangles[t].tri_mat_index].getPointer(); // Get the material assigned to this triangle
			const MaterialBinding& material_binding = object.getMaterialBinding(material_index);

			if(material->displacing())
			{
				const Vec3d N_g = triGeomNormal(verts_in, triangles[t]);
	

				//hitinfo.hitobject = &object;

				//const int uv_set_index = material->getDisplacementTextureUVSetIndex();
				//assert(uv_set_index >= 0 && uv_set_index < (int)num_uv_sets);

				float min_displacement = std::numeric_limits<float>::infinity();

				// For each vertex
				for(unsigned int i=0; i<3; ++i)
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

					HitInfo hitinfo(std::numeric_limits<unsigned int>::max(), Vec2d(-666.0, -666.0));

					//hitinfo.hitpos = toVec3d(verts_in[triangles[t].vertex_indices[i]].pos);

					for(unsigned int z=0; z<num_uv_sets; ++z)
						du_texcoord_evaluator.texcoords[z] = toVec2d(getUVs(uvs, num_uv_sets, triangles[t].uv_indices[i], z));

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

				if(unclipped_out)
				{
					(*unclipped_out)[t] = true;//min_displacement > 0.2f;
				}
			}
		}
	}

	// If any vertex is anchored, then set its position to the average of its 'parent' vertices
	for(unsigned int v=0; v<verts_out.size(); ++v)
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


static Vec2f screenSpacePosForCameraSpacePos(const CoordFramed& camera_coordframe_os, const Vec3f& cam_space_pos)
{
	return Vec2f(cam_space_pos.x / cam_space_pos.y, cam_space_pos.z / cam_space_pos.y);
}


class DUEdgeInfo
{
public:
	DUEdgeInfo()
	:	midpoint_vert_index(0), 
		num_adjacent_subdividing_tris(0), 
		border(false) 
	{}
	DUEdgeInfo(unsigned int midpoint_vert_index_, unsigned int num_adjacent_subdividing_tris_) :
		midpoint_vert_index(midpoint_vert_index_), num_adjacent_subdividing_tris(num_adjacent_subdividing_tris_)
		{}
	~DUEdgeInfo(){}
	unsigned int midpoint_vert_index;
	unsigned int num_adjacent_subdividing_tris;
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
		return y * t + ((Real)1.0 - t) * x;
	}
	else
	{
		if(x < y)
		{
			// interpolate between 1 + x and y
			return y * t + ((Real)1.0 - t) * (x + (Real)1.0);
		}
		else
		{
			// Interpolate between x and 1 + y
			return ((Real)1.0 + y) * t + ((Real)1.0 - t) * x;
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
		wrap_u ? fmod(wrappedLerp(a.x, b.x, t), 1.0f) : Maths::uncheckedLerp(a.x, b.x, t),
		wrap_v ? fmod(wrappedLerp(a.y, b.y, t), 1.0f) : Maths::uncheckedLerp(a.y, b.y, t)
		);


	//return a * (1.0f - t) + b * t;
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
	ThreadContext& context,
	const Object& object,
	//const std::vector<Reference<Material> >& materials,						
	//const CoordFramed& camera_coordframe_os, 
	//const std::vector<Plane<float> >& camera_clip_planes,
	const std::vector<DUTriangle>& tris_in, 
	const std::vector<DUVertex>& verts_in,
	const std::vector<Vec2f>& uvs_in,
	unsigned int num_uv_sets,
	const DUOptions& options,
	std::vector<DUTriangle>& tris_out, 
	std::vector<DUVertex>& verts_out,
	std::vector<Vec2f>& uvs_out
	)
{
	verts_out = verts_in; // Copy over original vertices
	uvs_out = uvs_in; // Copy over uvs

	tris_out.resize(0);


	// Get displaced vertices, which are needed for testing if subdivision is needed
	std::vector<DUVertex> displaced_in_verts;
	displace(
		context,
		object,
		true, // use anchoring
		//materials, 
		tris_in, 
		verts_in,
		uvs_in,
		num_uv_sets,
		displaced_in_verts, // verts out
		NULL // unclipped out
		);

	// Calculate normals of the displaced vertices
	computeVertexNormals(tris_in, displaced_in_verts);

	std::map<DUVertIndexPair, DUEdgeInfo> edge_info_map;
	std::map<DUVertIndexPair, unsigned int> uv_edge_map;

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
	for(unsigned int t=0; t<tris_in.size(); ++t)
	{
		if(tris_in[t].dimension == 2) // if 2-d (triangle)
		{
			// Decide if we are going to subdivide the triangle
			bool subdivide_triangle = false;

			if(options.view_dependent_subdivision)
			{
				// Build vector of displaced triangle vertex positions. (in object space)
				for(unsigned int i=0; i<3; ++i)
					tri_verts_pos_os[i] = displaced_in_verts[tris_in[t].vertex_indices[i]].pos;

				// Clip triangle against frustrum planes
				TriBoxIntersection::clipPolyToPlaneHalfSpaces(options.camera_clip_planes, tri_verts_pos_os, clipped_tri_verts_os);

				if(clipped_tri_verts_os.size() > 0) // If the triangle has not been completely clipped away
				{
					// Convert clipped verts to camera space
					clipped_tri_verts_cs.resize(clipped_tri_verts_os.size());
					for(unsigned int i=0; i<clipped_tri_verts_cs.size(); ++i)
						clipped_tri_verts_cs[i] = toVec3f(options.camera_coordframe_os.transformPointToLocal(toVec3d(clipped_tri_verts_os[i])));

					// Compute 2D bounding box of clipped triangle in screen space
					const Vec2f v0_ss = screenSpacePosForCameraSpacePos(options.camera_coordframe_os, clipped_tri_verts_cs[0]);
					Rect2f rect_ss(v0_ss, v0_ss);

					for(unsigned int i=1; i<clipped_tri_verts_cs.size(); ++i)
						rect_ss.enlargeToHoldPoint(
							screenSpacePosForCameraSpacePos(options.camera_coordframe_os, clipped_tri_verts_cs[i])
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
							const float displacement_error = displacementError(context, object, tris_in[t], displaced_in_verts, uvs_in, num_uv_sets, res);

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
					const float displacment_error = displacementError(context, object, tris_in[t], displaced_in_verts, uvs_in, num_uv_sets, RES);

					subdivide_triangle = displacment_error >= options.displacement_error_threshold;
				}
			}




			// Check curvature:
			/*TEMP const float tri_curvature = triangleMaxCurvature(
				displaced_in_verts[tris_in[t].vertex_indices[0]].normal, 
				displaced_in_verts[tris_in[t].vertex_indices[1]].normal, 
				displaced_in_verts[tris_in[t].vertex_indices[2]].normal);

			if(tri_curvature >= (float)subdivide_curvature_threshold) // If triangle is more curved than the threshold:
			{
				// Build vector of displaced triangle vertex positions. (in object space)
				for(unsigned int i=0; i<3; ++i)
					tri_verts_pos_os[i] = displaced_in_verts[tris_in[t].vertex_indices[i]].pos;

				// Clip triangle against frustrum planes
				TriBoxIntersection::clipPolyToPlaneHalfSpaces(camera_clip_planes, tri_verts_pos_os, clipped_tri_verts_os);

				if(clipped_tri_verts_os.size() > 0) // If the triangle has not been completely clipped away
				{
					// Convert clipped verts to camera space
					clipped_tri_verts_cs.resize(clipped_tri_verts_os.size());
					for(unsigned int i=0; i<clipped_tri_verts_cs.size(); ++i)
						clipped_tri_verts_cs[i] = toVec3f(camera_coordframe_os.transformPointToLocal(toVec3d(clipped_tri_verts_os[i])));

					// Compute 2D bounding box of clipped triangle in screen space
					const Vec2f v0_ss = screenSpacePosForCameraSpacePos(camera_coordframe_os, clipped_tri_verts_cs[0]);
					Rect2f rect_ss(v0_ss, v0_ss);

					for(unsigned int i=1; i<clipped_tri_verts_cs.size(); ++i)
						rect_ss.enlargeToHoldPoint(
							screenSpacePosForCameraSpacePos(camera_coordframe_os, clipped_tri_verts_cs[i])
							);

					// Subdivide only if the width of height of the screen space triangle bounding rectangle is bigger than the pixel height threshold
					subdivide_triangle = myMax(rect_ss.getWidths().x, rect_ss.getWidths().y) > pixel_height_at_dist_one * subdivide_pixel_threshold;
				}
			}*/

			/*if(!subdivide_triangle)
			{
				const float displacement_error_threshold = 0.01f;
				const int RES = 10;
				const float displacment_error = displacementError(context, object, tris_in[t], displaced_in_verts, uvs_in, num_uv_sets, RES);

				subdivide_triangle = displacment_error >= displacement_error_threshold;
			}*/

			subdividing_tri[t] = subdivide_triangle;

			if(subdivide_triangle)
			{
				for(unsigned int v=0; v<3; ++v)
				{
					{
					const unsigned int v_i = tris_in[t].vertex_indices[v];
					const unsigned int v_i1 = tris_in[t].vertex_indices[(v + 1) % 3];

					// Get vertex at the midpoint of this edge, or create it if it doesn't exist yet.

					const DUVertIndexPair edge(myMin(v_i, v_i1), myMax(v_i, v_i1)); // Key for the edge

					DUEdgeInfo& edge_info = edge_info_map[edge];

					if(edge_info.num_adjacent_subdividing_tris == 0)
					{
						// Create the edge midpoint vertex
						const unsigned int new_vert_index = verts_out.size();

						verts_out.push_back(DUVertex(
							(verts_in[v_i].pos + verts_in[v_i1].pos) * 0.5f,
							(verts_in[v_i].normal + verts_in[v_i1].normal) * 0.5f
							));

						verts_out.back().adjacent_vert_0 = edge.v_a;
						verts_out.back().adjacent_vert_1 = edge.v_b;

						edge_info.midpoint_vert_index = new_vert_index;
					}

					edge_info.num_adjacent_subdividing_tris++;
					}

					// Create new uvs at edge midpoint, if not already created.

					if(num_uv_sets > 0)
					{
						const unsigned int uv_i = tris_in[t].uv_indices[v];
						const unsigned int uv_i1 = tris_in[t].uv_indices[(v + 1) % 3];

						const DUVertIndexPair edge_key(myMin(uv_i, uv_i1), myMax(uv_i, uv_i1)); // Key for the edge

						const std::map<DUVertIndexPair, unsigned int>::const_iterator result = uv_edge_map.find(edge_key);
						if(result == uv_edge_map.end())
						{
							uv_edge_map.insert(std::make_pair(edge_key, uvs_out.size() / num_uv_sets));

							for(unsigned int z=0; z<num_uv_sets; ++z)
								uvs_out.push_back(
									lerpUVs(
										getUVs(uvs_in, num_uv_sets, uv_i, z), 
										getUVs(uvs_in, num_uv_sets, uv_i1, z), 
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

	// Mark border edges
	for(unsigned int t=0; t<tris_in.size(); ++t)
		if(tris_in[t].dimension == 1) // border edge
		{
			const unsigned int v_i = tris_in[t].vertex_indices[0];
			const unsigned int v_i1 = tris_in[t].vertex_indices[1];
			const DUVertIndexPair edge(myMin(v_i, v_i1), myMax(v_i, v_i1)); // Key for the edge
			DUEdgeInfo& edge_info = edge_info_map[edge];
			edge_info.border = true;
		}

	// Mark any new edge midpoint vertices as anchored that need to be
	for(std::map<DUVertIndexPair, DUEdgeInfo>::const_iterator i=edge_info_map.begin(); i != edge_info_map.end(); ++i)
		if(!(*i).second.border && (*i).second.num_adjacent_subdividing_tris == 1)
		{
			// This edge has a subdivided triangle on one side, and a non-subdivided triangle on the other side.
			// So we will 'anchor' the midpoint vertex to the average position of it's neighbours to avoid splitting.
			verts_out[(*i).second.midpoint_vert_index].anchored = true;
		}

	// Update polygons

	// Counters
	unsigned int num_tris_subdivided = 0;
	unsigned int num_tris_unchanged = 0;

	// For each triangle
	for(unsigned int t=0; t<tris_in.size(); ++t)
	{

		if(tris_in[t].dimension == 0) // vertex polygon
		{
			// Can't subdivide it, so just copy it over
			tris_out.push_back(tris_in[t]);
		}
		else if(tris_in[t].dimension == 1) // edge polygon
		{
			const unsigned int v_i = tris_in[t].vertex_indices[0];
			const unsigned int v_i1 = tris_in[t].vertex_indices[1];
			const DUVertIndexPair edge(myMin(v_i, v_i1), myMax(v_i, v_i1)); // Key for the edge
			
			assert(edge_info_map.find(edge) != edge_info_map.end());

			const std::map<DUVertIndexPair, DUEdgeInfo>::iterator result = edge_info_map.find(edge);
			const DUEdgeInfo& edge_info = (*result).second;
			if(edge_info.num_adjacent_subdividing_tris > 0)
			{
				// A triangle adjacent to this edge is being subdivided.  So subdivide this edge 1-D polygon as well.
				assert(edge_info.midpoint_vert_index > 0);

				// Get the midpoint uv index
				unsigned int midpoint_uv_index = 0;
				if(num_uv_sets > 0)
				{
					const unsigned int uv_i = tris_in[t].uv_indices[0];
					const unsigned int uv_i1 = tris_in[t].uv_indices[1];
					const DUVertIndexPair uv_edge_key(myMin(uv_i, uv_i1), myMax(uv_i, uv_i1)); // Key for the edge
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
				unsigned int midpoint_vert_indices[3]; // Indices of edge midpoint vertices in verts_out
				unsigned int midpoint_uv_indices[3] = {0, 0, 0};

				// For each edge (v_i, v_i+1)
				for(unsigned int v=0; v<3; ++v)
				{
					{
					const unsigned int v_i = tris_in[t].vertex_indices[v];
					const unsigned int v_i1 = tris_in[t].vertex_indices[(v + 1) % 3];
					const DUVertIndexPair edge(myMin(v_i, v_i1), myMax(v_i, v_i1)); // Key for the edge

					const std::map<DUVertIndexPair, DUEdgeInfo>::iterator result = edge_info_map.find(edge);
					assert(result != edge_info_map.end());

					midpoint_vert_indices[v] = (*result).second.midpoint_vert_index;
					}
					if(num_uv_sets > 0)
					{
						const unsigned int uv_i = tris_in[t].uv_indices[v];
						const unsigned int uv_i1 = tris_in[t].uv_indices[(v + 1) % 3];
						const DUVertIndexPair edge(myMin(uv_i, uv_i1), myMax(uv_i, uv_i1)); // Key for the edge

						const std::map<DUVertIndexPair, unsigned int>::iterator result = uv_edge_map.find(edge);
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

	conPrint("\t\tnum triangles subdivided: " + toString(num_tris_subdivided));
	conPrint("\t\tnum triangles unchanged: " + toString(num_tris_unchanged));
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
	for(unsigned int v=0; v<new_verts_out.size(); ++v)
		new_verts_out[v].pos = new_verts_out[v].normal = Vec3f(0.f, 0.f, 0.f);

	// Init vertex UVs to (0,0)
	uvs_out = uvs_in;
	for(unsigned int v=0; v<uvs_out.size(); ++v)
		uvs_out[v] = Vec2f(0.f, 0.f);

	std::vector<unsigned int> dim(verts.size(), 2); // array containing dimension of each vertex
	std::vector<float> total_weight(verts.size(), 0.0f); // total weights per vertex
	//std::vector<float> total_uvs_weight(uvs_in.size() / num_uv_sets, 0.0f); // total weights per vertex
	std::vector<unsigned int> n_t(verts.size(), 0); // array containing number of triangles touching each vertex
	//std::vector<unsigned int> uv_n_t(uvs_in.size() / num_uv_sets, 0); // array containing number of triangles touching each UV group
	//std::vector<unsigned int> n_q(verts.size(), 0); // array containing number of quads touching each vertex

	// For each vertex, we will store a list of the indices of UV coord pairs associated with the vertex
	std::vector<VertexUVSetIndices> vert_uv_set_indices(verts.size());

	std::vector<Vec2f> uv_offsets(uvs_in.size()); //verts.size() * num_uv_sets);
	
	// Init uv_offsets
	for(unsigned int i=0; i<uv_offsets.size(); ++i)
	{
		uv_offsets[i].x = options.wrap_u ? ((uvs_in[i].x < 0.25f || uvs_in[i].x > 0.75f) ? 0.5f : 0.0f) : 0.0f;
		uv_offsets[i].y = options.wrap_v ? ((uvs_in[i].y < 0.25f || uvs_in[i].y > 0.75f) ? 0.5f : 0.0f) : 0.0f;

		//uv_offsets[i] = Vec2f(0.f, 0.f);
	}

	// Initialise dim
	for(unsigned int t=0; t<tris.size(); ++t)
		for(unsigned int v=0; v<tris[t].dimension+1; ++v)
			dim[tris[t].vertex_indices[v]] = myMin(dim[tris[t].vertex_indices[v]], tris[t].dimension);

	// Initialise n_t
	for(unsigned int t=0; t<tris.size(); ++t)
		for(unsigned int v=0; v<tris[t].dimension+1; ++v)
			n_t[tris[t].vertex_indices[v]]++;

	// Initialise uv_n_t
	/*if(num_uv_sets > 0)
		for(unsigned int t=0; t<tris.size(); ++t)
			for(unsigned int v=0; v<tris[t].dimension+1; ++v)
				uv_n_t[tris[t].uv_indices[v]]++;*/


	// Initialise vert_uv_set_indices
	for(unsigned int t=0; t<tris.size(); ++t) // For each poly
		if(tris[t].dimension == 2) // If it's a triangle
			for(unsigned int i=0; i<3; ++i) // For each vertex
			{
				const unsigned int v_i = tris[t].vertex_indices[i]; // vertex index

				// If it's not in the list, and the list isn't full yet...
				if(!UVSetIndexPresent(vert_uv_set_indices[v_i], tris[t].uv_indices[i]) && vert_uv_set_indices[v_i].num_uv_set_indices < VertexUVSetIndices::MAX_NUM_UV_SET_INDICES)
					vert_uv_set_indices[v_i].uv_set_indices[vert_uv_set_indices[v_i].num_uv_set_indices++] = tris[t].uv_indices[i]; // Store the index of the UV pair.
			}


	std::vector<Vec2f> uv_cent(num_uv_sets);

	for(unsigned int t=0; t<tris.size(); ++t) // For each polyon
	{
		for(unsigned int v=0; v<tris[t].dimension+1; ++v) // For each vertex
		{
			const unsigned int v_i = tris[t].vertex_indices[v];

			if(dim[v_i] == tris[t].dimension) // Only add centroid if vertex has same dimension as polygon
			{
				Vec3f cent;
				float weight;
				if(tris[t].dimension == 0) // t is a vertex
				{
					cent = verts[v_i].pos;
					for(unsigned int z=0; z<num_uv_sets; ++z)
						uv_cent[z] = getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[v], z);
					weight = 1.0f;
				}
				else if(tris[t].dimension == 1) // t is an edge
				{
					cent = (verts[v_i].pos + verts[tris[t].vertex_indices[(v + 1) % 2]].pos) * 0.5f;
					for(unsigned int z=0; z<num_uv_sets; ++z)
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
					const unsigned int v_i_plus_1 = tris[t].vertex_indices[(v + 1) % 3];
					const unsigned int v_i_minus_1 = tris[t].vertex_indices[(v + 2) % 3];

					cent = verts[v_i].pos * (1.0f / 4.0f) + (verts[v_i_plus_1].pos + verts[v_i_minus_1].pos) * (3.0f / 8.0f);
					

					//uv_cent = uvs_in[tris[t].uv_indices[v]] * (1.0f / 4.0f) + (uvs_in[tris[t].uv_indices[(v + 1) % 3]] + uvs_in[tris[t].uv_indices[(v + 2) % 3]]) * (3.0f / 8.0f);
					
					/*for(unsigned int z=0; z<num_uv_sets; ++z)
						uv_cent[z] = 
							getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[v], z) * (1.0f / 4.0f) + 
							(getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[(v + 1) % 3], z) + getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[(v + 2) % 3], z)) * (3.0f / 8.0f);*/

					for(unsigned int z=0; z<num_uv_sets; ++z)
						uv_cent[z] = lerpUVs(
							getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[v], z),
							lerpUVs(
								getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[(v + 1) % 3], z),
								getUVs(uvs_in, num_uv_sets, tris[t].uv_indices[(v + 2) % 3], z),
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

					
					weight = (float)NICKMATHS_PI / 3.0f;
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
				for(unsigned int i=0; i<vert_uv_set_indices[v_i].num_uv_set_indices; ++i)
					for(unsigned int z=0; z<num_uv_sets; ++z)
					{
						const unsigned int uv_index = uvIndex(num_uv_sets, vert_uv_set_indices[v_i].uv_set_indices[i], z);

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

	for(unsigned int v=0; v<new_verts_out.size(); ++v)
	{
		const float w_val = w(n_t[v], 0);

		new_verts_out[v].pos /= total_weight[v];
		new_verts_out[v].pos = Maths::uncheckedLerp(
			verts[v].pos, 
			new_verts_out[v].pos, 
			w_val
			); //verts[v].pos + (new_verts_out[v].pos - verts[v].pos) * w_val;

		//new_verts_out[v].normal /= total_weight[v];
		//new_verts_out[v].normal = verts[v].normal + (new_verts_out[v].normal - verts[v].normal) * w_val;
		//new_verts_out[v].normal.normalise();


		for(unsigned int z=0; z<num_uv_sets; ++z)
			for(unsigned int i=0; i<vert_uv_set_indices[v].num_uv_set_indices; ++i) // for each UV set at this vertex
			{
				const unsigned int uv_index = uvIndex(num_uv_sets, vert_uv_set_indices[v].uv_set_indices[i], z);

				uvs_out[uv_index] /= total_weight[v];

				uvs_out[uv_index] -= uv_offsets[uv_index];

				uvs_out[uv_index] = lerpUVs(//Maths::uncheckedLerp(
					uvs_in[uv_index], 
					uvs_out[uv_index], 
					w_val,
					options.wrap_u, options.wrap_v
					);
/*
				getUVs(uvs_out, num_uv_sets, vert_uv_set_indices[v].uv_set_indices[i], z) /= total_weight[v];

				getUVs(uvs_out, num_uv_sets, vert_uv_set_indices[v].uv_set_indices[i], z) = Maths::uncheckedLerp(
					getUVs(uvs_in, num_uv_sets, vert_uv_set_indices[v].uv_set_indices[i], z), 
					getUVs(uvs_out, num_uv_sets, vert_uv_set_indices[v].uv_set_indices[i], z), 
					w_val
					);*/
			}



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


	for(unsigned int t=0; t<tris.size(); ++t) // For each poly
		if(tris[t].dimension == 2) // If it's a triangle
		{
			for(unsigned int z=0; z<num_uv_sets; ++z)
			{
				bool u_wraps = false;
				bool v_wraps = false;

				for(unsigned int i=0; i<3; ++i) // For each vertex
				{
					for(int q=0; q<3; ++q) // For each other vertex
					{
						const Vec2f dist = getUVs(uvs_out, num_uv_sets, tris[t].uv_indices[i], z) - getUVs(uvs_out, num_uv_sets, tris[t].uv_indices[q], z);
						if(fabs(dist.x) > 0.5)
							u_wraps = true;
						if(fabs(dist.y) > 0.5)
							v_wraps = true;
					}
				}

				if(options.wrap_u && u_wraps)
					for(unsigned int i=0; i<3; ++i) // For each vertex
						if(getUVs(uvs_out, num_uv_sets, tris[t].uv_indices[i], z).x < 0.25f)
							getUVs(uvs_out, num_uv_sets, tris[t].uv_indices[i], z).x += 1.0f;
				if(options.wrap_v && v_wraps)
					for(unsigned int i=0; i<3; ++i) // For each vertex
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
	for(unsigned int v=0; v<new_verts_out.size(); ++v)
		if(verts[v].anchored)
			new_verts_out[v].pos = (new_verts_out[verts[v].adjacent_vert_0].pos + new_verts_out[verts[v].adjacent_vert_1].pos) * 0.5f;

	// TEMP HACK:
	//uvs_out = uvs_in;
}


void DisplacementUtils::test()
{
	double z = fmod(-1.25, 1.0);

	testAssert(epsEqual(wrappedLerp(0.1, 0.3, 0.5), 0.2));
	testAssert(epsEqual(wrappedLerp(0.3, 0.1, 0.5), 0.2));

	testAssert(epsEqual(wrappedLerp(0.9, 0.1, 0.25), 0.95));
	testAssert(epsEqual(wrappedLerp(0.1, 0.9, 0.75), 0.95));

	testAssert(epsEqual(wrappedLerp(0.9, 0.1, 0.75), 0.05));
	testAssert(epsEqual(wrappedLerp(0.1, 0.9, 0.25), 0.05));

	testAssert(epsEqual(wrappedLerp(-0.1, 0.1, 0.5), 0.0));
	testAssert(epsEqual(wrappedLerp(0.1, -0.1, 0.5), 0.0));
}

