/*=====================================================================
TriBoxIntersection.h
--------------------
File created by ClassTemplate on Fri Mar 21 16:49:56 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TRIBOXINTERSECTION_H_666_
#define __TRIBOXINTERSECTION_H_666_


#include "../maths/vec3.h"
#include "../maths/plane.h"
#include "../physics/jscol_aabbox.h"
#include <vector>


/*=====================================================================
TriBoxIntersection
------------------

=====================================================================*/
class TriBoxIntersection
{
public:
	/*=====================================================================
	TriBoxIntersection
	------------------
	
	=====================================================================*/
	//TriBoxIntersection();

	//~TriBoxIntersection();


	/*static void clipPolyAgainstPlane(const Vec3f* points, unsigned int num_points, unsigned int plane_axis, float d, float normal, Vec3f* points_out, unsigned int& num_points_out);

	static int triBoxOverlap(float boxcenter[3], float boxhalfsize[3], float triverts[3][3]);



	static void slowGetClippedTriAABB(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2, const js::AABBox& aabb, js::AABBox& clipped_tri_aabb_out);
	static void getClippedTriAABB(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2, const js::AABBox& aabb, js::AABBox& clipped_tri_aabb_out);
	//static void getClippedTriAABB_SSE(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2, const js::AABBox& aabb, js::AABBox& clipped_tri_aabb_out);

*/
	// clips to half space in the other half from the plane normal.
	//static void clipPolyToPlaneHalfSpace(const Planef& plane, const Vec3f* points, unsigned int num_points, unsigned int max_num_points_out, Vec3f* points_out, unsigned int& num_points_out);
	static void clipPolyToPlaneHalfSpace(const Planef& plane, const std::vector<Vec3f>& polygon_verts, std::vector<Vec3f>& polygon_verts_out);

	static void clipPolyToPlaneHalfSpaces(const std::vector<Planef>& planes, const std::vector<Vec3f>& polygon_verts, std::vector<Vec3f>& temp_vert_buffer, std::vector<Vec3f>& polygon_verts_out);

	static void test();

};


#endif //__TRIBOXINTERSECTION_H_666_




