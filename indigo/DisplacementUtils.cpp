/*=====================================================================
DisplacementUtils.cpp
---------------------
File created by ClassTemplate on Thu May 15 20:31:26 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "DisplacementUtils.h"


#include "../maths/rect2.h"
#include "../graphics/TriBoxIntersection.h"


DisplacementUtils::DisplacementUtils()
{	
}


DisplacementUtils::~DisplacementUtils()
{	
}


static inline bool operator < (const DUVertex& a, const DUVertex& b)
{
	return a.pos < b.pos;
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


void DisplacementUtils::subdivideAndDisplace(const std::vector<Material*>& materials,
											 const CoordFramed& camera_coordframe_os, double pixel_height_at_dist_one, double subdivide_pixel_threshold, double subdivide_curvature_threshold,
								  unsigned int num_subdivisions,
								  const std::vector<Plane<float> >& camera_clip_planes,
								  bool smooth,
		const std::vector<RayMeshTriangle>& triangles_in, 
		const std::vector<RayMeshVertex>& vertices_in, 
		std::vector<RayMeshTriangle>& tris_out, 
		std::vector<RayMeshVertex>& verts_out
		)
{
	//conPrint("Subdividing mesh: (num subdivisions = " + toString(num_subdivisions) + ")");

	
	// Convert RayMeshTriangle's to DUTriangle's
	std::vector<DUTriangle> temp_tris(triangles_in.size());
	
	for(unsigned int i=0; i<temp_tris.size(); ++i)
		temp_tris[i] = DUTriangle(
			triangles_in[i].vertex_indices[0], triangles_in[i].vertex_indices[1], triangles_in[i].vertex_indices[2], triangles_in[i].tri_mat_index, 
			2, // dimension
			0 // num subdivs
			);

	// Convert RayMeshVertex's to DUVertex's
	std::vector<DUVertex> temp_verts(vertices_in.size());
	for(unsigned int i=0; i<temp_verts.size(); ++i)
	{
		temp_verts[i] = DUVertex(vertices_in[i].pos, vertices_in[i].normal);

		for(unsigned int t=0; t<RayMeshVertex::MAX_NUM_RAYMESH_TEXCOORD_SETS; ++t)
			temp_verts[i].texcoords[t] = vertices_in[i].texcoords[t];
	}


	// Add edge and vertex polygons
	{
		std::map<DUVertIndexPair, unsigned int> num_adjacent_tris; // edge -> num adjacent tris

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
		
		for(unsigned int t=0; t<initial_num_temp_tris; ++t)
		{
			for(unsigned int v=0; v<3; ++v)
			{
				const unsigned int v_i = temp_tris[t].vertex_indices[v];
				const unsigned int v_i1 = temp_tris[t].vertex_indices[(v + 1) % 3];
				
				const DUVertIndexPair edge(myMin(v_i, v_i1), myMax(v_i, v_i1)); // Key for the edge

				if(num_adjacent_tris[edge] == 1)
				{
					// this is an edge edge aka. boundary !!! :)
					temp_tris.push_back(DUTriangle(v_i, v_i1, 666, 666, 1, 0));
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


	std::vector<DUTriangle> temp_tris2;
	std::vector<DUVertex> temp_verts2;

	for(unsigned int i=0; i<num_subdivisions; ++i)
	{
		conPrint("\tDoing subdivision level " + toString(i) + "...");

		linearSubdivision(
			materials,
			camera_coordframe_os, 
			pixel_height_at_dist_one, 
			subdivide_pixel_threshold, 
			subdivide_curvature_threshold,
			camera_clip_planes,
			temp_tris, // tris in
			temp_verts, // verts in
			temp_tris2, // tris out
			temp_verts2 // verts out
			);
		
		if(smooth)
			averagePass(temp_tris2, temp_verts2, temp_verts);
		else
			temp_verts = temp_verts2;
		temp_tris = temp_tris2;

		conPrint("\t\tresulting num vertices: " + toString(temp_verts.size()));
		conPrint("\t\tresulting num triangles: " + toString(temp_tris.size()));
		conPrint("\t\tDone.");
	}

	// Apply the final displacement
	displace(
		true, // use anchoring
		materials, 
		temp_tris, // tris in
		temp_verts, // verts in
		temp_verts2 // verts out
		);
	temp_verts = temp_verts2;

	// Build tris_out
	tris_out.resize(0);
	for(unsigned int i=0; i<temp_tris.size(); ++i)
		if(temp_tris[i].dimension == 2)
			tris_out.push_back(RayMeshTriangle(temp_tris[i].vertex_indices[0], temp_tris[i].vertex_indices[1], temp_tris[i].vertex_indices[2], temp_tris[i].tri_mat_index));


	// Recompute all vertex normals, as they will be completely wrong by now due to any displacement.
	for(unsigned int i=0; i<temp_verts.size(); ++i)
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
		temp_verts[i].normal.normalise();


	// Convert DUVertex's back into RayMeshVertex and store in verts_out.
	verts_out.resize(temp_verts.size());
	for(unsigned int i=0; i<verts_out.size(); ++i)
	{
		verts_out[i] = RayMeshVertex(temp_verts[i].pos, temp_verts[i].normal);

		for(unsigned int t=0; t<RayMeshVertex::MAX_NUM_RAYMESH_TEXCOORD_SETS; ++t)
			verts_out[i].texcoords[t] = temp_verts[i].texcoords[t];
	}
}


/*
Apply displacement to the given vertices, storing the displaced vertices in verts_out


*/
void DisplacementUtils::displace(bool use_anchoring, const std::vector<Material*>& materials, const std::vector<DUTriangle>& triangles, 
								 const std::vector<DUVertex>& verts_in, std::vector<DUVertex>& verts_out)
{
	verts_out = verts_in;

	std::vector<bool> vert_displaced(verts_in.size(), false); // Only displace each vertex once

	for(unsigned int t=0; t<triangles.size(); ++t)
	{
		if(triangles[t].dimension == 2)
		{
			const Material* material = materials[triangles[t].tri_mat_index]; // Get the material assigned to this triangle

			if(material->displacing())
			{
				const int uv_set_index = material->getDisplacementTextureUVSetIndex();
				assert(uv_set_index >= 0 && uv_set_index < RayMeshVertex::MAX_NUM_RAYMESH_TEXCOORD_SETS);

				for(unsigned int i=0; i<3; ++i)
				{
					if(!vert_displaced[triangles[t].vertex_indices[i]])
					{
						// Translate vertex position along vertex shading normal
						verts_out[triangles[t].vertex_indices[i]].pos += verts_out[triangles[t].vertex_indices[i]].normal * (float)material->displacement(
							verts_out[triangles[t].vertex_indices[i]].texcoords[uv_set_index].x,
							verts_out[triangles[t].vertex_indices[i]].texcoords[uv_set_index].y
							);

						//vertices[triangles[t].vertex_indices[i]].pos.x += sin(vertices[triangles[t].vertex_indices[i]].pos.z * 50.0f) * 0.03f;

						vert_displaced[triangles[t].vertex_indices[i]] = true;
					}
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
	DUEdgeInfo() : midpoint_vert_index(0), num_adjacent_subdividing_tris(0), num_adjacent_non_subdividing_tris(0), left_tri_index(-1), right_tri_index(-1), border(false) {}
	DUEdgeInfo(unsigned int midpoint_vert_index_, unsigned int num_adjacent_subdividing_tris_, unsigned int num_adjacent_non_subdividing_tris_) :
		midpoint_vert_index(midpoint_vert_index_), num_adjacent_subdividing_tris(num_adjacent_subdividing_tris_), num_adjacent_non_subdividing_tris(num_adjacent_non_subdividing_tris_)
		{}
	~DUEdgeInfo(){}
	unsigned int midpoint_vert_index;
	unsigned int num_adjacent_subdividing_tris;
	unsigned int num_adjacent_non_subdividing_tris;
	int left_tri_index, right_tri_index;
	bool border;
};


/*
DUEdgeInfo& getEdgeInfo(std::map<DUVertIndexPair, DUEdgeInfo>& edge_info_map, unsigned int v_i, unsigned int v_i_1)
{
	
}*/


// tri_index may be -1
static inline int triDisplacementLevel(const std::vector<DUTriangle>& tris, int tri_index)
{
	if(tri_index == -1)
		return -1;
	else
		return tris[tri_index].num_subdivs;
}

void DisplacementUtils::linearSubdivision(
	const std::vector<Material*>& materials,						
	const CoordFramed& camera_coordframe_os, 
	double pixel_height_at_dist_one,
	double subdivide_pixel_threshold,
	double subdivide_curvature_threshold,
	const std::vector<Plane<float> >& camera_clip_planes,
	const std::vector<DUTriangle>& tris_in, 
	const std::vector<DUVertex>& verts_in, 
	std::vector<DUTriangle>& tris_out, 
	std::vector<DUVertex>& verts_out
	)
{
	std::map<DUVertIndexPair, DUEdgeInfo> edge_info_map;


	// Copy over original vertices
	verts_out = verts_in;

//	for(unsigned int i=0; i<verts_out.size(); ++i)
//		verts_out[i].adjacent_subdivided_tris = 0;

//	patch_start_indices_out.resize(0);
	tris_out.resize(0);


	// Get displaced vertices, which are needed for testing if subdivision is needed
	std::vector<DUVertex> displaced_in_verts;
	displace(
		false, // use anchoring
		materials, 
		tris_in, 
		verts_in, 
		displaced_in_verts // verts out
		);

	// Calculate normals of the displaced vertices, as generated from the surrounding triangles
	std::vector<Vec3f> displaced_vert_normals(verts_in.size(), Vec3f(0.f, 0.f, 0.f));
	for(unsigned int t=0; t<tris_in.size(); ++t)
	{
		if(tris_in[t].dimension == 2)
		{
			const Vec3f tri_normal = triGeometricNormal(
					displaced_in_verts, 
					tris_in[t].vertex_indices[0], 
					tris_in[t].vertex_indices[1], 
					tris_in[t].vertex_indices[2]
				);
			for(int i=0; i<3; ++i)
				displaced_vert_normals[tris_in[t].vertex_indices[i]] += tri_normal;
		}
	}
	for(unsigned int i=0; i<displaced_vert_normals.size(); ++i)
		displaced_vert_normals[i].normalise();

	/*std::vector<Vec3f> vert_K(verts_in.size(), 0.0f);

	for(unsigned int t=0; t<tris_in.size(); ++t)
		for(int i=0; i<3; ++i)
		{
			const Vec3f v0v1 = verts_in[tris_in[t].vertex_indices[1]].pos - verts_in[tris_in[t].vertex_indices[0]].pos;
			const Vec3f v1v2 = verts_in[tris_in[t].vertex_indices[2]].pos - verts_in[tris_in[t].vertex_indices[1]].pos;
			const Vec3f v2v0 = verts_in[tris_in[t].vertex_indices[0]].pos - verts_in[tris_in[t].vertex_indices[2]].pos;
			const float cot_val = 1.0f / tan(::angleBetween(v0v1, v1v2);*/


	// Create some temporary buffers
	std::vector<Vec3f> tri_verts_pos_os(3);

	std::vector<Vec3f> clipped_tri_verts_os;
	clipped_tri_verts_os.reserve(32);

	std::vector<Vec3f> clipped_tri_verts_cs;
	clipped_tri_verts_cs.reserve(32);

	// Do a pass to decide whether or not to subdivide each triangle, and create new vertices if subdividing.

	std::vector<bool> subdividing_tri(tris_in.size(), false);

	// For each triangle
	for(unsigned int t=0; t<tris_in.size(); ++t)
	{
		if(tris_in[t].dimension == 2) // triangle
		{
			bool do_subdivide = false;

			// Check curvature:
			const float tri_curvature = triangleMaxCurvature(
				displaced_vert_normals[tris_in[t].vertex_indices[0]], 
				displaced_vert_normals[tris_in[t].vertex_indices[1]], 
				displaced_vert_normals[tris_in[t].vertex_indices[2]]);
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
					do_subdivide = myMax(rect_ss.getWidths().x, rect_ss.getWidths().y) > pixel_height_at_dist_one * subdivide_pixel_threshold;
				}
			}

			subdividing_tri[t] = do_subdivide;

			// Mark edges
			for(unsigned int v=0; v<3; ++v)
			{
				const unsigned int v_i = tris_in[t].vertex_indices[v];
				const unsigned int v_i1 = tris_in[t].vertex_indices[(v + 1) % 3];

				// Get vertex at the midpoint of this edge, or create it if it doesn't exist yet.

				const DUVertIndexPair edge(myMin(v_i, v_i1), myMax(v_i, v_i1)); // Key for the edge

				DUEdgeInfo& edge_info = edge_info_map[edge];

				if(do_subdivide)
				{
					if(edge_info.num_adjacent_subdividing_tris == 0)
					{
						// Create the edge midpoint vertex
						const unsigned int new_vert_index = verts_out.size();

						verts_out.push_back(DUVertex(
							(verts_in[v_i].pos + verts_in[v_i1].pos) * 0.5f,
							(verts_in[v_i].normal + verts_in[v_i1].normal) * 0.5f
							));

						for(unsigned int z=0; z<RayMeshVertex::MAX_NUM_RAYMESH_TEXCOORD_SETS; ++z)
							verts_out.back().texcoords[z] = (verts_in[v_i].texcoords[z] + verts_in[v_i1].texcoords[z]) * 0.5f,

						//verts_out.back().adjacent_subdivided_tris = 1;
						verts_out.back().adjacent_vert_0 = edge.v_a;
						verts_out.back().adjacent_vert_1 = edge.v_b;
						// If either of the parent vertices are anchored, then this vertex must be as well.
						//verts_out.back().anchored = verts_in[v_i].anchored || verts_in[v_i1].anchored;

						edge_info.midpoint_vert_index = new_vert_index;
					}

					edge_info.num_adjacent_subdividing_tris++;
				}
				else
				{
					edge_info.num_adjacent_non_subdividing_tris++;
				}

				if(v_i < v_i1)
					edge_info.left_tri_index = t;
				else
					edge_info.right_tri_index = t;

				/*if(edge_info.num_adjacent_subdividing_tris == 1 && edge_info.num_adjacent_non_subdividing_tris == 1)
				{
					// This edge has a subdivided triangle on one side, and a non-subdivided triangle on the other side.
					// So we will 'anchor' the midpoint vertex to the average position of it's neighbours to avoid splitting.
					verts_out[edge_info.midpoint_vert_index].anchored = true;
				}*/

				assert(edge_info.num_adjacent_subdividing_tris + edge_info.num_adjacent_non_subdividing_tris <= 2);

				/*const std::map<DUVertIndexPair, unsigned int>::iterator result = edge_info_map.find(edge);
				if(result == edge_info_map.end())
				{
					// Create new vertex
					//e[v] = verts_out.size(); // store index of new vertex
					const unsigned int new_vert_index = verts_out.size();

					verts_out.push_back(DUVertex(
						(verts_in[v_i].pos + verts_in[v_i1].pos) * 0.5f,
						(verts_in[v_i].normal + verts_in[v_i1].normal) * 0.5f
						));

					for(unsigned int z=0; z<RayMeshVertex::MAX_NUM_RAYMESH_TEXCOORD_SETS; ++z)
						verts_out.back().texcoords[z] = (verts_in[v_i].texcoords[z] + verts_in[v_i1].texcoords[z]) * 0.5f,

					verts_out.back().adjacent_subdivided_tris = 1;
					verts_out.back().adjacent_vert_0 = edge.v_a;
					verts_out.back().adjacent_vert_1 = edge.v_b;

					// Add new vertex to map
					edge_to_new_vert_map.insert(std::make_pair(
						edge,
						DUEdgeInfo(
							new_vert_index, 
							1, // num_adjacent_subidividing_tris: 1 because this tri is being subdivided, and this edge is new
							0
							)
						));
				}
				else
				{
					e[v] = (*result).second; // else vertex has already been created
				
					verts_out[e[v]].adjacent_subdivided_tris++;
				}*/			
			}
		}
	}

	for(unsigned int t=0; t<tris_in.size(); ++t)
	{
		if(tris_in[t].dimension == 1) // border edge
		{
			const unsigned int v_i = tris_in[t].vertex_indices[0];
			const unsigned int v_i1 = tris_in[t].vertex_indices[1];
			const DUVertIndexPair edge(myMin(v_i, v_i1), myMax(v_i, v_i1)); // Key for the edge
			DUEdgeInfo& edge_info = edge_info_map[edge];
			edge_info.border = true;
		}
	}




	for(std::map<DUVertIndexPair, DUEdgeInfo>::const_iterator i=edge_info_map.begin(); i != edge_info_map.end(); ++i)
	{
		const DUEdgeInfo& edge_info = (*i).second;

		if(!edge_info.border)
			if(edge_info.num_adjacent_subdividing_tris == 1)// && edge_info.num_adjacent_non_subdividing_tris == 1)
			{
				// This edge has a subdivided triangle on one side, and a non-subdivided triangle on the other side.
				// So we will 'anchor' the midpoint vertex to the average position of it's neighbours to avoid splitting.
				verts_out[edge_info.midpoint_vert_index].anchored = true;
			}

			//if(triDisplacementLevel(tris_out, (*i).second.left_tri_index) != triDisplacementLevel(tris_out, (*i).second.right_tri_index))
		//verts_out[(*i).second.midpoint_vert_index].anchored = true;
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

				// Create new edges
				tris_out.push_back(DUTriangle(
						tris_in[t].vertex_indices[0],
						edge_info.midpoint_vert_index,
						666,
						tris_in[t].tri_mat_index,
						1, // dimension
						tris_in[t].num_subdivs + 1
					));

				tris_out.push_back(DUTriangle(
						edge_info.midpoint_vert_index,
						tris_in[t].vertex_indices[1],
						666,
						tris_in[t].tri_mat_index,
						1, // dimension
						tris_in[t].num_subdivs + 1
					));
			}
			else
			{
				// not being subdivided, so just copy over polygon
				tris_out.push_back(tris_in[t]);
			}


			/*const unsigned int v_i = tris_in[t].vertex_indices[0];
			const unsigned int v_i1 = tris_in[t].vertex_indices[1];

			unsigned int e; // Index of edge midpoint vertices in verts_out

			// Get vertex at the midpoint of this edge, or create it if it doesn't exist yet.

			const DUVertIndexPair edge(myMin(v_i, v_i1), myMax(v_i, v_i1)); // Key for the edge

			const std::map<DUVertIndexPair, unsigned int>::iterator result = edge_to_new_vert_map.find(edge);
			if(result == edge_to_new_vert_map.end())
			{
				// Create new vertex
				e = verts_out.size(); // store index of new vertex

				verts_out.push_back(DUVertex(
					(verts_in[v_i].pos + verts_in[v_i1].pos) * 0.5f,
					(verts_in[v_i].normal + verts_in[v_i1].normal) * 0.5f
					));

				for(unsigned int z=0; z<RayMeshVertex::MAX_NUM_RAYMESH_TEXCOORD_SETS; ++z)
					verts_out.back().texcoords[z] = (verts_in[v_i].texcoords[z] + verts_in[v_i1].texcoords[z]) * 0.5f,

				verts_out.back().adjacent_subdivided_tris = 1;
				verts_out.back().adjacent_vert_0 = edge.v_a;
				verts_out.back().adjacent_vert_1 = edge.v_b;

				// Add new vertex to map
				edge_to_new_vert_map.insert(std::make_pair(
					edge,
					e
					));
			}
			else
			{
				e = (*result).second; // else vertex has already been created
			
				verts_out[e].adjacent_subdivided_tris++;
			}	

			// Create new edges
			tris_out.push_back(DUTriangle(
					tris_in[t].vertex_indices[0],
					e,
					666,
					tris_in[t].tri_mat_index,
					1 // dimension
				));

			tris_out.push_back(DUTriangle(
					e,
					tris_in[t].vertex_indices[1],
					666,
					tris_in[t].tri_mat_index,
					1 // dimension
				));*/
		}
		else
		{
			//bool do_subdivide = true; // TEMP

			// Build vector of displaced triangle vertex positions. (in object space)
			/*for(unsigned int i=0; i<3; ++i)
				tri_verts_pos_os[i] = displaced_in_verts[tris_in[t].vertex_indices[i]].pos;

			// Check curvature:
			const float tri_curvature = triangleMaxCurvature(vert_normals[tris_in[t].vertex_indices[0]], vert_normals[tris_in[t].vertex_indices[1]], vert_normals[tris_in[t].vertex_indices[2]]);
			if(tri_curvature >= (float)subdivide_curvature_threshold) // If triangle is more curved than the threshold:
			{
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
					do_subdivide = myMax(rect_ss.getWidths().x, rect_ss.getWidths().y) > pixel_height_at_dist_one * subdivide_pixel_threshold;
				}
			}*/


			if(subdividing_tri[t]) // If we are subdividing this triangle...
			{
				unsigned int e[3]; // Indices of edge midpoint vertices in verts_out

				// For each edge (v_i, v_i+1)
				for(unsigned int v=0; v<3; ++v)
				{
					const unsigned int v_i = tris_in[t].vertex_indices[v];
					const unsigned int v_i1 = tris_in[t].vertex_indices[(v + 1) % 3];
					const DUVertIndexPair edge(myMin(v_i, v_i1), myMax(v_i, v_i1)); // Key for the edge

					const std::map<DUVertIndexPair, DUEdgeInfo>::iterator result = edge_info_map.find(edge);
					assert(result != edge_info_map.end());

					e[v] = (*result).second.midpoint_vert_index;

					// Set edge midpoint vertex anchoring.  NOTE: could do this somewhere else.
					//if(triDisplacementLevel(tris_out, (*result).second.left_tri_index) != triDisplacementLevel(tris_out, (*i).second.right_tri_index))
					//	verts_out[(*i).second.midpoint_vert_index].anchored = true;


					/*const unsigned int v_i = tris_in[t].vertex_indices[v];
					const unsigned int v_i1 = tris_in[t].vertex_indices[(v + 1) % 3];

					// Get vertex at the midpoint of this edge, or create it if it doesn't exist yet.

					const DUVertIndexPair edge(myMin(v_i, v_i1), myMax(v_i, v_i1)); // Key for the edge

					const std::map<DUVertIndexPair, unsigned int>::iterator result = edge_to_new_vert_map.find(edge);
					if(result == edge_to_new_vert_map.end())
					{
						// Create new vertex
						e[v] = verts_out.size(); // store index of new vertex

						verts_out.push_back(DUVertex(
							(verts_in[v_i].pos + verts_in[v_i1].pos) * 0.5f,
							(verts_in[v_i].normal + verts_in[v_i1].normal) * 0.5f
							));

						for(unsigned int z=0; z<RayMeshVertex::MAX_NUM_RAYMESH_TEXCOORD_SETS; ++z)
							verts_out.back().texcoords[z] = (verts_in[v_i].texcoords[z] + verts_in[v_i1].texcoords[z]) * 0.5f,

						verts_out.back().adjacent_subdivided_tris = 1;
						verts_out.back().adjacent_vert_0 = edge.v_a;
						verts_out.back().adjacent_vert_1 = edge.v_b;

						// Add new vertex to map
						edge_to_new_vert_map.insert(std::make_pair(
							edge,
							e[v]
							));
					}
					else
					{
						e[v] = (*result).second; // else vertex has already been created
					
						verts_out[e[v]].adjacent_subdivided_tris++;
					}	*/		
				}

				//patch_start_indices_out.push_back(tris_out.size());

				tris_out.push_back(DUTriangle(
						tris_in[t].vertex_indices[0],
						e[0],
						e[2],
						tris_in[t].tri_mat_index,
						2, // dimension
						tris_in[t].num_subdivs + 1
				));

				tris_out.push_back(DUTriangle(
						e[1],
						e[2],
						e[0],
						tris_in[t].tri_mat_index,
						2, // dimension
						tris_in[t].num_subdivs + 1
				));

				tris_out.push_back(DUTriangle(
						e[0],
						tris_in[t].vertex_indices[1],
						e[1],
						tris_in[t].tri_mat_index,
						2, // dimension
						tris_in[t].num_subdivs + 1
				));

				tris_out.push_back(DUTriangle(
						e[2],
						e[1],
						tris_in[t].vertex_indices[2],
						tris_in[t].tri_mat_index,
						2, // dimension
						tris_in[t].num_subdivs + 1
				));

				num_tris_subdivided++;
			}
			else
			{
				//patch_start_indices_out.push_back(tris_out.size());

				// Else don't subdivide triangle.
				// Original vertices are already copied over.
				// So just copy the triangle
				tris_out.push_back(tris_in[t]);

				num_tris_unchanged++;
			}
		}
	}

//	for(unsigned int i=0; i<verts_out.size(); ++i)
//		verts_out[i].anchored = verts_out[i].anchored || verts_out[i].adjacent_subdivided_tris == 1;

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

void DisplacementUtils::averagePass(const std::vector<DUTriangle>& tris, const std::vector<DUVertex>& verts, std::vector<DUVertex>& new_verts_out)
{
	/*new_verts_out = std::vector<DUVertex>(verts.size(), DUVertex(
		Vec3f(0.0f, 0.0f, 0.0f), // pos
		Vec3f(0.0f ,0.0f, 0.0f) // normal
		)); // new vertices*/
	new_verts_out = verts;
	for(unsigned int v=0; v<new_verts_out.size(); ++v)
	{
		new_verts_out[v].pos = new_verts_out[v].normal = Vec3f(0.f, 0.f, 0.f);
		
		for(unsigned int z=0; z<RayMeshVertex::MAX_NUM_RAYMESH_TEXCOORD_SETS; ++z)
			new_verts_out[v].texcoords[z] = Vec2f(0.f, 0.f);
	}

	std::vector<unsigned int> dim(verts.size(), 2); // array containing dimension of each vertex
	std::vector<float> total_weight(verts.size(), 0.0f); // total weights per vertex
	std::vector<unsigned int> n_t(verts.size(), 0); // array containing number of triangles touching each vertex
	//std::vector<unsigned int> n_q(verts.size(), 0); // array containing number of quads touching each vertex

	// Initialise dim
	for(unsigned int t=0; t<tris.size(); ++t)
		for(unsigned int v=0; v<tris[t].dimension+1; ++v)
			dim[tris[t].vertex_indices[v]] = myMin(dim[tris[t].vertex_indices[v]], tris[t].dimension);

	// Initialise n_t
	for(unsigned int t=0; t<tris.size(); ++t)
		for(unsigned int v=0; v<tris[t].dimension+1; ++v)
			n_t[tris[t].vertex_indices[v]]++;


	for(unsigned int t=0; t<tris.size(); ++t)
	{
		for(unsigned int v=0; v<tris[t].dimension+1; ++v)
		{
			const unsigned int v_i = tris[t].vertex_indices[v];

			if(dim[v_i] == tris[t].dimension) // Only add centroid if vertex has same dimension as polygon
			{
				Vec3f cent;
				float weight;
				if(tris[t].dimension == 0) // t is a vertex
				{
					cent = verts[v_i].pos;
					weight = 1.0f;
				}
				else if(tris[t].dimension == 1) // t is an edge
				{
					cent = (verts[v_i].pos + verts[tris[t].vertex_indices[(v + 1) % 2]].pos) * 0.5f;
					weight = 1.0f;
				}
				else
				{
					// t is a triangle
					assert(tris[t].dimension == 2);
					const unsigned int v_i_plus_1 = tris[t].vertex_indices[(v + 1) % 3];
					const unsigned int v_i_minus_1 = tris[t].vertex_indices[(v + 2) % 3];

					cent = verts[v_i].pos * (1.0f / 4.0f) + (verts[v_i_plus_1].pos + verts[v_i_minus_1].pos) * (3.0f / 8.0f);
					weight = (float)NICKMATHS_PI / 3.0f;
				}
			
				total_weight[v_i] += weight;
				new_verts_out[v_i].pos += cent * weight;
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
		new_verts_out[v].pos = verts[v].pos + (new_verts_out[v].pos - verts[v].pos) * w_val;

		new_verts_out[v].normal /= total_weight[v];
		new_verts_out[v].normal = verts[v].normal + (new_verts_out[v].normal - verts[v].normal) * w_val;
		new_verts_out[v].normal.normalise();

		for(unsigned int z=0; z<4; ++z)
		{
			new_verts_out[v].texcoords[z] /= total_weight[v];
			new_verts_out[v].texcoords[z] = verts[v].texcoords[z] + (new_verts_out[v].texcoords[z] - verts[v].texcoords[z]) * w_val;
		}
	}

	// Set all anchored vertex positions back to the midpoint between the vertex's 'parent positions'
	for(unsigned int v=0; v<new_verts_out.size(); ++v)
	{
		// If any vertex only has 1 adjacent subdivided tri, then just reset its position.
		//assert(verts[v].adjacent_subdivided_tris >= 0 && verts[v].adjacent_subdivided_tris <= 2);

		if(verts[v].anchored)
		{
			new_verts_out[v].pos = (new_verts_out[verts[v].adjacent_vert_0].pos + new_verts_out[verts[v].adjacent_vert_1].pos) * 0.5f;
		}

		/*new_verts_out[v].anchored = verts[v].anchored;
		new_verts_out[v].adjacent_vert_0 = verts[v].adjacent_vert_0;
		new_verts_out[v].adjacent_vert_1 = verts[v].adjacent_vert_1;*/
	}

}

