/*=====================================================================
DouglasPeuckerAlg.h
-------------------
File created by ClassTemplate on Thu May 21 11:05:42 2009
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __DOUGLASPEUCKERALG_H_666_
#define __DOUGLASPEUCKERALG_H_666_


#include "../maths/vec2.h"
#include <vector>
#include <list>


/*=====================================================================
DouglasPeuckerAlg
-----------------
http://en.wikipedia.org/wiki/Ramer-Douglas-Peucker_algorithm
=====================================================================*/
class DouglasPeuckerAlg
{
public:
	DouglasPeuckerAlg();

	~DouglasPeuckerAlg();


	typedef float Real;

	static void approximate(const std::vector<Vec2<Real> >& points, 
		std::vector<Vec2<Real> >& points_out, 
		std::vector<unsigned int>& simplified_indices_out,
		Real epsilon);

	static void test();

private:
	static void doApproximate(const std::vector<Vec2<Real> >& points, 
		int start_i, int end_i,
		std::vector<Vec2<Real> >& points_out, 
		std::vector<unsigned int>& simplified_indices_out,
		Real epsilon);

};



#endif //__DOUGLASPEUCKERALG_H_666_




