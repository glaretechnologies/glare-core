/*=====================================================================
DisplacementUtils.h
-------------------
File created by ClassTemplate on Thu May 15 20:31:26 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __DISPLACEMENTUTILS_H_666_
#define __DISPLACEMENTUTILS_H_666_


#include "../simpleraytracer/raymesh.h"
class ThreadContext;


class DUVertex
{
public:
	DUVertex(){ anchored = false; }
	DUVertex(const Vec3f& pos_, const Vec3f& normal_) : pos(pos_), normal(normal_) { 
		//texcoords[0] = texcoords[1] = texcoords[2] = texcoords[3] = Vec2f(0.f, 0.f); 
		anchored = false; }
	Vec3f pos;
	Vec3f normal;
	//Vec2f texcoords[4];
	bool anchored;
	//int adjacent_subdivided_tris;
	int adjacent_vert_0, adjacent_vert_1;
	float displacement; // will be set after displace() is called.
	
	//static const unsigned int MAX_NUM_UV_SET_INDICES = 8;
	//unsigned int uv_set_indices[MAX_NUM_UV_SET_INDICES];
	//unsigned int num_uv_set_indices;
};

class DUTriangle
{
public:
	DUTriangle(){}
	DUTriangle(unsigned int v0_, unsigned int v1_, unsigned int v2_, unsigned int uv0, unsigned int uv1, unsigned int uv2, unsigned int matindex, unsigned int dimension_) : tri_mat_index(matindex), dimension(dimension_)//, num_subdivs(num_subdivs_)
	{
		vertex_indices[0] = v0_;
		vertex_indices[1] = v1_;
		vertex_indices[2] = v2_;

		uv_indices[0] = uv0;
		uv_indices[1] = uv1;
		uv_indices[2] = uv2;
	}
	unsigned int vertex_indices[3];
	unsigned int uv_indices[3];
	unsigned int tri_mat_index;
	unsigned int dimension;
	//unsigned int num_subdivs;
};


class DUOptions
{
public:
	bool wrap_u;
	bool wrap_v;
};

/*=====================================================================
DisplacementUtils
-----------------

=====================================================================*/
class DisplacementUtils
{
public:
	/*=====================================================================
	DisplacementUtils
	-----------------
	
	=====================================================================*/
	DisplacementUtils();

	~DisplacementUtils();

	


	static void subdivideAndDisplace(
		ThreadContext& context,
		const Object& object,
		const CoordFramed& camera_coordframe_os, double pixel_height_at_dist_one, double subdivide_pixel_threshold, double subdivide_curvature_threshold,
		unsigned int num_subdivisions,
		const std::vector<Plane<float> >& camera_clip_planes,
		bool smooth,
		const std::vector<RayMeshTriangle>& tris_in, 
		const std::vector<RayMeshVertex>& verts_in, 
		const std::vector<Vec2f>& uvs_in,
		unsigned int num_uv_sets,
		const DUOptions& options,
		std::vector<RayMeshTriangle>& tris_out, 
		std::vector<RayMeshVertex>& verts_out,
		std::vector<Vec2f>& uvs_out
		);


	static void test();

private:
	static void displace(
		ThreadContext& context, 
		const Object& object,
		bool use_anchoring, 
		const std::vector<DUTriangle>& tris, 
		const std::vector<DUVertex>& verts_in, 
		const std::vector<Vec2f>& uvs,
		unsigned int num_uv_sets,
		std::vector<DUVertex>& verts_out,
		std::vector<bool>* unclipped_out
		);
	static void linearSubdivision(
		ThreadContext& context,
		const Object& object,
		const CoordFramed& camera_coordframe_os, 
		double pixel_height_at_dist_one,
		double subdivide_pixel_threshold,
		double subdivide_curvature_threshold,
		const std::vector<Plane<float> >& camera_clip_planes,
		const std::vector<DUTriangle>& tris_in, 
		const std::vector<DUVertex>& verts_in, 
		const std::vector<Vec2f>& uvs_in,
		unsigned int num_uv_sets,
		const DUOptions& options,
		std::vector<DUTriangle>& tris_out, 
		std::vector<DUVertex>& verts_out,
		std::vector<Vec2f>& uvs_out
		);
	static void averagePass(
		const std::vector<DUTriangle>& tris, 
		const std::vector<DUVertex>& verts,
		const std::vector<Vec2f>& uvs_in,
		unsigned int num_uv_sets,
		const DUOptions& options,
		std::vector<DUVertex>& new_verts_out,
		std::vector<Vec2f>& uvs_out
		);



};



#endif //__DISPLACEMENTUTILS_H_666_




