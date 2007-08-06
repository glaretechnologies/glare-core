/*=====================================================================
lineseg.h
---------
File created by ClassTemplate on Sat Dec 08 15:50:30 2001
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __LINESEG_H_666_
#define __LINESEG_H_666_


//#include "maths.h"


/*=====================================================================
LineSeg
-------

=====================================================================*/
class LineSeg
{
public:
	/*=====================================================================
	LineSeg
	-------
	
	=====================================================================*/
	inline LineSeg(const Vec3& start, const Vec3 end);

	inline ~LineSeg(){}


	inline const Vec3 getClosestPointOnLineTo(const Vec3& p) const;

	inline const Vec3& getStart() const { return start; }
	inline Vec3& getStart(){ return start; }

	inline const Vec3& getEnd() const { return end; }
	inline Vec3& getEnd(){ return end; }

private:
	Vec3 start;
	Vec3 end;
};



LineSeg::LineSeg(const Vec3& start_, const Vec3 end_)
:	start(start_),
	end(end_)
{}


const Vec3 LineSeg::getClosestPointOnLineTo(const Vec3& p) const
{
	const Vec3 unitline = normalise(end - start);

	Vec3 closest_point = start - p;
	closest_point.removeComponentInDir(unitline);
	closest_point += p;


	//-----------------------------------------------------------------
	//clamp to endpoints
	//-----------------------------------------------------------------
	if(dot(closest_point, unitline) < dot(start, unitline))
		return start;
	else if(dot(closest_point, unitline) > dot(end, unitline))
		return end;

	return closest_point;
}

#endif //__LINESEG_H_666_




