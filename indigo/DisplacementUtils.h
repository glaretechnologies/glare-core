/*=====================================================================
DisplacementUtils.h
-------------------
File created by ClassTemplate on Thu May 15 20:31:26 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __DISPLACEMENTUTILS_H_666_
#define __DISPLACEMENTUTILS_H_666_



#include "../simpleraytracer/raymesh.h"

class DUVertex
{
public:
	DUVertex(){ anchored = false; }
	DUVertex(const Vec3f& pos_, const Vec3f& normal_) : pos(pos_), normal(normal_) { texcoords[0] = texcoords[1] = texcoords[2] = texcoords[3] = Vec2f(0.f, 0.f); anchored = false; }
	Vec3f pos;
	Vec3f normal;
	Vec2f texcoords[4];
	bool anchored;
	int adjacent_subdivided_tris;
	int adjacent_vert_0, adjacent_vert_1;
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

	


	static void subdivideAndDisplace(const std::vector<Material*>& materials,
		const CoordFramed& camera_coordframe_os, double pixel_height_at_dist_one, double subdivide_pixel_threshold, double subdivide_curvature_threshold,
		unsigned int num_subdivisions,
		const std::vector<Plane<float> >& camera_clip_planes,
		bool smooth,
		const std::vector<RayMeshTriangle>& tris_in, 
		const std::vector<RayMeshVertex>& verts_in, 
		std::vector<RayMeshTriangle>& tris_out, 
		std::vector<RayMeshVertex>& verts_out
		);


private:
	static void displace(const std::vector<Material*>& materials, const std::vector<RayMeshTriangle>& tris, const std::vector<DUVertex>& verts_in, std::vector<DUVertex>& verts_out);
	static void linearSubdivision(const std::vector<Material*>& materials,
								const CoordFramed& camera_coordframe_os, 
								double pixel_height_at_dist_one,
								double subdivide_pixel_threshold,
								double subdivide_curvature_threshold,
								const std::vector<Plane<float> >& camera_clip_planes,
								const std::vector<RayMeshTriangle>& tris_in, 
								const std::vector<DUVertex>& verts_in, 
								std::vector<RayMeshTriangle>& tris_out, 
								std::vector<DUVertex>& verts_out
								);
	static void averagePass(const std::vector<RayMeshTriangle>& tris, const std::vector<DUVertex>& verts, std::vector<DUVertex>& new_verts_out);



};



#endif //__DISPLACEMENTUTILS_H_666_




