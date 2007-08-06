#ifndef __CORESTATE_H__
#define __CORESTATE_H__

namespace js
{



class CoreState
{
public:
	CoreState()
	:	mat(Vec3(0,0,0))
	{
		vel.zero();
		angvel.zero();
	}

	CoordFrame mat;//position and rotation

	Vec3 vel;//vel of center of mass in world coords

	Vec3 angvel;//angular vel in body coords.
};








}//end namespace js
#endif //__CORESTATE_H__