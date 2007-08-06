#ifndef __JSCOL_SIMPLEMODEL_H__
#define __JSCOL_SIMPLEMODEL_H__

#include "jscol_model.h"
#include "jscol_boundingsphere.h"
#include "../simple3D/aabox.h"
class Ray;


namespace js
{


class Shape;

//a shape, coordframe pair basically

class SimpleModel : public Model
{
public:

	//NEWCODE: now deletes shape.
	SimpleModel(Shape* shape, const CoordFrame& frame);
	virtual ~SimpleModel();


	virtual float getRayDist(const ::Ray& ray);

	virtual float traceRay(const ::Ray& ray, unsigned int& hit_tri_out, float& u_out,
							float& v_out, Vec3& normal_out);


	//virtual bool lineStabsModel(float& t_out, Vec3& normal_ws_out, 
	//	const Vec3& raystart_ws, const Vec3& rayend_ws, const Vec3& rayunitdir, 
	//														float raylength);

	//returns true if sphere hit object.
	virtual bool traceSphere(const BoundingSphere& sphere, const Vec3& translation,	
										float& dist_out, Vec3& normal_ws_out);//, bool& embedded_out,
										//Vec3& disembedded_pos_out);

	virtual void appendCollPoints(std::vector<Vec3>& points, const BoundingSphere& sphere);


	//virtual const BoundingSphere& getBoundingSphereWS(){ return boundingsphere; }


	virtual bool checkCollisionWith(const Model& dyn_model, const CoordFrame& ini_transformation, 
									const Vec3& final_translation, MMColResults& results_out);

	virtual const CoordFrame& getCoordFrame() const { return frame; } 
	virtual void setCoordFrame(const CoordFrame& newframe);

	//void deleteShape();


private:
	void calcBBox();
	Shape* shape;
	CoordFrame frame;
	//BoundingSphere boundingsphere;
	AABox bbox_os;

};





}//end namespace js
#endif //__JSCOL_SIMPLEMODEL_H__
