#ifndef __OBJECT_H__
#define __OBJECT_H__

#include "C:\programming\maths\maths.h"//NOTE: not strictly needed
//#include <vector>

namespace js
{

class WorldColSys;


class Object
{
public:
	Object(bool moveable_)
	{
		moveable = moveable_;
	}
	virtual ~Object(){}

//	virtual void impulseOS(const Vec3& impulse_point_os, const Vec3& impulse) = 0;
	virtual void impulseWS(const Vec3& impulse_point_ws, const Vec3& impulse) = 0;


	virtual void writeState(Vector::iterator& iter) = 0;
	virtual void writeDeriv(Vector::iterator& iter) = 0;
	virtual void setState(Vector::const_iterator& iter) = 0;

	virtual bool checkForColAndRebound(WorldColSys& col_sys) = 0;
	virtual void saveState() = 0;
	virtual void undo() = 0;
//	virtual void setStateCheckForCol(Vector::const_iterator& iter, WorldColSys& col_sys) = 0;

	bool isMoveable() const { return moveable; }
private:
	bool moveable;
};



}//end namespace js
#endif //__OBJECT_H__











/*#ifndef __OBJECT_H__
#define __OBJECT_H__

#include "C:\programming\maths\maths.h"
#include "js_corestate.h"

namespace js
{

class Object
{
public:
//	inline Object(const CoordFrame& coordframe, bool moveable, float mass);
	virtual ~Object(){};

	

//	virtual bool lineStabsObject(float& t_out, Vec3& normal_out, const Vec3& raystart, const Vec3& rayend) = 0;

	virtual void impulseOS(const Vec3& impulse_point_os, const Vec3& impulse) = 0;

	virtual void impulseWS(const Vec3& impulse_point_ws, const Vec3& impulse) = 0;


	virtual void writeState(std::vector<float>::iterator& iter);

//	const Vec3& getVel(){ return corestate.vel; }

//	const Vec3& getPos(){ return corestate.mat.getOrigin(); }
//	CoordFrame& getMat(){ return corestate.mat; }


//	float getMass() const { return mass; }

//	bool isMoveable() const { return moveable; }


//	virtual const Vec3 getSelfForces() { return Vec3(0,0,0); }
	
	//const Matrix3& getRot() const { return rot; }

protected:
	//Matrix3& getRot(){ return rot; }
	//void setMoveable(bool m){ moveable = m; }

private:
	//Matrix3 rot;
//	bool moveable;
//	float mass;

public://TEMP public
	//CoordFrame mat;
//	CoreState corestate;
};



Object::Object(const CoordFrame& coordframe, bool moveable_, float mass_)
{ 
	corestate.mat = coordframe;
	
	corestate.angvel.zero();
	moveable = moveable_;
	mass = mass_;
}


}//end namespace js*/
