#ifndef __JSCOL_MODELTRACERESULT_H__
#define __JSCOL_MODELTRACERESULT_H__

#include "C:\programming\maths\maths.h"

namespace js
{

class ModelTraceResult
{
public:
	ModelTraceResult();
	~ModelTraceResult();

	//-----------------------------------------------------------------
	//gets
	//-----------------------------------------------------------------
	bool colOcurred() const { return col_occured; }

	const ::Vec3& getColPoint() const { return col_point; }

	Model* getStaticModel() const { return staticmodel; }
	Model* getDynModel() const { return dynmodel; }

	float getPathFraction() const { return t; }

	const ::Vec3& getStaticModelNormal() const { return static_mod_normal; }
	const ::Vec3& getDynModelNormal() const { return dyn_model_normal; }


	//-----------------------------------------------------------------
	//sets
	//-----------------------------------------------------------------
	void setColOcurred(bool col){ col_occured = col; }
	void setColPoint(const ::Vec3& p){ col_point = p; }
	void setStaticModel(Model* staticmodel_){ staticmodel = staticmodel_; }
	void setDynModel(Model* dynmodel_){ dynmodel = dynmodel_; }
	void setT(float t_){ t = t_; }
	void setStaticModNormal(const ::Vec3& n){ static_mod_normal = n; }
	void setDynModNormal(const ::Vec3& n){ dyn_model_normal = n; }

private:
	bool col_occured;
	::Vec3 col_point;
	Model* staticmodel;
	Model* dynmodel;
	float t;
	::Vec3 static_mod_normal;
	::Vec3 dyn_model_normal;
};






}//end namespace js
#endif //__JSCOL_MODELTRACERESULT_H__