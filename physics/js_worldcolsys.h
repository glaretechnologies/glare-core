#ifndef __JS_WORLDCOLSYS_H__
#define __JS_WORLDCOLSYS_H__

class CoordFrame;
class Vec3;



namespace js
{

class Model;
class ModelTraceResult;
class RayTraceResult;

/*=======================================================================================
WorldColSys
-----------
Abstract interface class.
Defines the interface of the world object - dynamic object collision detection subsystem
=======================================================================================*/
class WorldColSys
{
public:
	virtual ~WorldColSys(){}

	virtual void insertStaticModel(Model* model) = 0;
	
	virtual void removeStaticModel(Model* model) = 0;


	//virtual void traceModelPath(Model& dynamic_model, const CoordFrame& ini_transformation, 
	//	const Vec3& final_translation, ModelTraceResult& results_out) = 0;

	//virtual void traceRay(const Vec3& inipos, const Vec3& endpos, RayTraceResult& results_out) = 0;
};





}//end namespace js
#endif //__JS_WORLDCOLSYS_H__