/*=====================================================================
raybundle.h
-----------
File created by ClassTemplate on Sun Jun 12 04:34:47 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __RAYBUNDLE_H_666_
#define __RAYBUNDLE_H_666_


#include "../maths/vec3.h"
#include "../maths/PaddedVec3.h"
#include "../maths/SSE.h"

const unsigned int DIRS_X_INDEX = 0;
const unsigned int DIRS_Y_INDEX = 4;
const unsigned int DIRS_Z_INDEX = 8;

/*=====================================================================
RayBundle
---------
For SSE ray/mesh intersection
=====================================================================*/
class RayBundle
{
public:
	/*=====================================================================
	RayBundle
	---------
	
	=====================================================================*/
	RayBundle();

	~RayBundle();


	/*Vec3 origin;
	Vec3[4] unitdirs;*/
	//Vec3 origin;
	//float padding;//pad to 64 bytes
	SSE_ALIGN PaddedVec3f origin;

	SSE_ALIGN float dirs[12];//x components packed, then y components, then z components

	//Vec3 unitdirs[4];
	//SSE_ALIGN PaddedVec3 unitdirs[4];


};



#endif //__RAYBUNDLE_H_666_




