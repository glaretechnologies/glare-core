#ifndef __JS_TRIMESHSHAPE_H__
#define __JS_TRIMESHSHAPE_H__

#include "jscol_shape.h"
#include "jscol_trimesh.h"
#include "jscol_boundingsphere.h"
#include "jscol_tritree.h"
#include "../simple3D/aabox.h"

namespace js
{

//triangle mesh collision geometry
	
class SimpleTriMeshShape : public Shape
{
public:
	SimpleTriMeshShape(const TriMesh& trilist);
	virtual ~SimpleTriMeshShape();

	virtual float getRayDist(const ::Ray& ray);

	//virtual float traceRay(const ::Ray& ray, unsigned int& hit_tri_out, float& u_out,
	//						float& v_out, Vec3& normal_out);

	//virtual bool lineStabsShape(float& t_out, Vec3& normal_os_out, const Vec3& raystart_os,
	//											const Vec3& rayend_os);

		//returns true if sphere hit object.
	virtual bool traceSphere(const BoundingSphere& sphere_os, const Vec3& translation_os,	
											float& dist_out, Vec3& normal_os_out);//, bool& embedded_out,
											//Vec3& disembed_vec_out);

	virtual void appendCollPoints(std::vector<Vec3>& points_ws, const BoundingSphere& sphere_os,
											const CoordFrame& frame);


	//virtual const BoundingSphere& getBoundingSphereOS();
	virtual const AABox getBBoxOS();

	const TriTree& getTriTree() const { return tritree; }

private:
	TriMesh tris;
	//BoundingSphere boundingsphere;
	AABox bbox_os;

	TriTree tritree;
};


	

}//end namespace js
#endif //__JS_TRIMESHSHAPE_H__
