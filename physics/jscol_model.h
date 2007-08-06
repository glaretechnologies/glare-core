#ifndef __JSCOL_MODEL_H__
#define __JSCOL_MODEL_H__


#include "../maths/maths.h"
#include <vector>
class Ray;


namespace js
{



class MMColResults
{
public:
	Vec3 thisnormal;
	Vec3 othernormal;
	Vec3 colpoint;
	float fraction;
};
	
class BoundingSphere;

/*============================================================================
Model
-----
Abstract base class.
Reprazents an object that can be placed into the world.
It has 2 parts:
*	the shape of the object
*	the orientation and position of the object
============================================================================*/
class Model
{
public:
	Model() : userdata(NULL) {}
	virtual ~Model(){}



	virtual float getRayDist(const ::Ray& ray) = 0;

	//virtual bool lineStabsModel(float& t_out, Vec3& normal_out, 
	//	const Vec3& raystart_ws, const Vec3& rayend_ws, const Vec3& rayunitdir, 
	//														float raylength) = 0;

	virtual float traceRay(const ::Ray& ray, unsigned int& hit_tri_out, float& u_out,
							float& v_out, Vec3& normal_out) = 0;

	//returns true if sphere hit object.
	virtual bool traceSphere(const BoundingSphere& sphere, const Vec3& translation,	
										float& dist_out, Vec3& normal_out) = 0;//, bool& embedded_out,
										//Vec3& disembed_vec_out) = 0;


	virtual void appendCollPoints(std::vector<Vec3>& points, const BoundingSphere& sphere) = 0;


	//virtual const BoundingSphere& getBoundingSphereWS() = 0;

	virtual bool checkCollisionWith(const Model& dyn_model, const CoordFrame& ini_transformation, 
									const Vec3& final_translation, MMColResults& results_out) = 0;

	virtual const CoordFrame& getCoordFrame() const = 0;
	virtual void setCoordFrame(const CoordFrame& newframe) = 0;


	void* getUserdata(){ return userdata; }
	void setUserdata(void* userdata_){ userdata = userdata_; }

private:
	void* userdata;//pointer to a user defined class
};





}//end namespace js
#endif //__JSCOL_MODEL_H__
