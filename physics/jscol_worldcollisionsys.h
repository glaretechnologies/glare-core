#ifndef __JSCOL_WORLDCOLLISIONSYS_H__
#define __JSCOL_WORLDCOLLISIONSYS_H__



#include "C:\programming\maths\maths.h"
#include "jscol_voxelhash.h"
#include "js_worldcolsys.h"
#include "../utils/singleton.h"
#include "../utils/mutex.h"
class Ray;


namespace js
{

class RayTraceResult;
class ModelTraceResult;
class Model;
class BoundingSphere;

//make singleton for cyberspace project

class WorldCollisionSys : public WorldColSys, public Singleton<WorldCollisionSys>
{
public:
	WorldCollisionSys();
	virtual ~WorldCollisionSys();


	virtual void insertStaticModel(Model* model);
	
	virtual void removeStaticModel(Model* model);



	//virtual void traceModelPath(Model& dynamic_model, const CoordFrame& ini_transformation, 
	//	const Vec3& final_translation, ModelTraceResult& results_out);

	virtual float getRayDist(const ::Ray& ray);

	virtual void traceRay(const ::Ray& ray, RayTraceResult& results_out);

	//virtual void traceRay(const Vec3& inipos, const Vec3& endpos, RayTraceResult& results_out);

	virtual void traceSphere(const BoundingSphere& start, const Vec3& translation, 
					RayTraceResult& results_out);//, bool& embedded_out, Vec3& disembed_vec_out);

	virtual void getCollPoints(std::vector<Vec3>& points_out, const BoundingSphere& sphere);

private:
	bool checkCollision(::js::Model& dyn_model, ::js::Model& static_model, 
		const CoordFrame& ini_transformation, const Vec3& final_translation, float &tracefraction_out);


	VoxelHash voxelhash;
	Mutex mutex;

};








}//end namespace js
#endif //__JSCOL_WORLDCOLLISIONSYS_H__