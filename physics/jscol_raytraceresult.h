#ifndef __JSCOL_RAYTRACERESULT_H__
#define __JSCOL_RAYTRACERESULT_H__

#include "C:\programming\maths\maths.h"



namespace js
{

class Model;

class RayTraceResult
{
public:
	bool rayhitsomething;
	Model* obstructor;
	Vec3 ob_normal;
	//float fraction;
	float dist;
	unsigned int hit_tri_index;
	float u, v;//tex coords
};




}//end namespace js
#endif //__JSCOL_RAYTRACERESULT_H__