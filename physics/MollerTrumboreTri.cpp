/*=====================================================================
MollerTrumboreTri.cpp
---------------------
File created by ClassTemplate on Mon Nov 03 16:40:40 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "MollerTrumboreTri.h"


#include "../simpleraytracer/ray.h"


namespace js
{


void MollerTrumboreTri::set(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2)
{
	const Vec3f e1 = v1 - v0;
	const Vec3f e2 = v2 - v0;
	//const Vec3f n = normalise(crossProduct(e1, e2));

	/*data[0] = v0.x;
	data[1] = v0.y;
	data[2] = v0.z;
	data[3] = n.x;

	data[4] = e1.x;
	data[5] = e1.y;
	data[6] = e1.z;
	data[7] = n.y;

	data[8] = e2.x;
	data[9] = e2.y;
	data[10] = e2.z;
	data[11] = n.z;*/

	data[0] = v0.x;
	data[1] = v0.y;
	data[2] = v0.z;

	data[3] = e1.x;
	data[4] = e1.y;
	data[5] = e1.z;

	data[6] = e2.x;
	data[7] = e2.y;
	data[8] = e2.z;

#if USE_LAUNCH_NORMAL
	data[9] = 1.0f / crossProduct(e1, e2).length();
#endif
}


} //end namespace js
