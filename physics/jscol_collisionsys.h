#ifndef __JSCOL_WORLDCOLLISIONSYS_H__
#define __JSCOL_WORLDCOLLISIONSYS_H__

namespace js
{

class WorldCollisionSys
{
public:
	WorldCollisionSys();
	~WorldCollisionSys();


	void insertStaticModel(Model* model);
	
	void removeStaticModel(Model* model);



	void traceModelPath(const Model& model, const CoordFrame& initial_trans, 
					const CoordFrame& final_trans, ModelTraceResult& results_out);



private:
};






}//end namespace js
#endif //__JSCOL_WORLDCOLLISIONSYS_H__