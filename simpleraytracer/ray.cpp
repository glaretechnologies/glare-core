#include "ray.h"



void Ray::buildRecipRayDir()
{
	/*SSE_ALIGN PaddedVec3 raydir = unitdir;
	//raydir.padding = 1.0f;

	//TEMP HACK:
	//this is to avoid the inverse raydirs from being INFs.
	//Otherwise if the ray origin lies on a splitting plane along the same axis then u get 0*INF = NAN
	const float VERY_SMALL_VAL = 1e-20f;
	if(raydir.x == 0.0f)
		raydir.x = VERY_SMALL_VAL;
	if(raydir.y == 0.0f)
		raydir.y = VERY_SMALL_VAL;
	if(raydir.z == 0.0f)
		raydir.z = VERY_SMALL_VAL;

	//SSE recip vector calc
	store4Vec( div4Vec( load4Vec(one_4vec), load4Vec(&raydir.x) ), &recip_unitdir.x);

	built_recip_unitdir = true;*/

	SSE_ALIGN Vec3d raydir = unitdir;
	//raydir.padding = 1.0f;

	//TEMP HACK:
	//this is to avoid the inverse raydirs from being INFs.
	//Otherwise if the ray origin lies on a splitting plane along the same axis then u get 0*INF = NAN
	const double VERY_SMALL_VAL = 1e-20;
	if(raydir.x == 0.0)
		raydir.x = VERY_SMALL_VAL;
	if(raydir.y == 0.0)
		raydir.y = VERY_SMALL_VAL;
	if(raydir.z == 0.0)
		raydir.z = VERY_SMALL_VAL;

	//SSE recip vector calc
	//store4Vec( div4Vec( load4Vec(one_4vec), load4Vec(&raydir.x) ), &recip_unitdir.x);
	//TEMP NO SSE
	recip_unitdir.set(
		1.0 / raydir.x,
		1.0 / raydir.y,
		1.0 / raydir.z
		);

	recip_unitdir_f = PaddedVec3((float)recip_unitdir.x, (float)recip_unitdir.y, (float)recip_unitdir.z);

	//built_recip_unitdir = true;
}