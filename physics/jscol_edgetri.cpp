/*=====================================================================
edgetri.cpp
-----------
File created by ClassTemplate on Mon Nov 22 04:32:53 2004Code By Nicholas Chapman.
=====================================================================*/
#include "jscol_edgetri.h"



namespace js
{


EdgeTri::EdgeTri(const Vec3& v0_, const Vec3& v1, const Vec3& v2)
{
	set(v0_, v1, v2);

//	num_leaf_tris = 0;
}


EdgeTri::~EdgeTri()
{
	
}


void EdgeTri::set(const Vec3& v0_, const Vec3& v1, const Vec3& v2)
{
	vert0 = v0_;
	edge1 = v1 - vert0;
	edge2 = v2 - vert0;
}



inline void crossProduct(const Vec3& v1, const Vec3& v2, Vec3& vout)
{
	vout.x = (v1.y * v2.z) - (v1.z * v2.y);
	vout.y = (v1.z * v2.x) - (v1.x * v2.z);
	vout.z = (v1.x * v2.y) - (v1.y * v2.x);
}

inline void sub(const Vec3& v1, const Vec3& v2, Vec3& vout)
{
	vout.x = v1.x - v2.x;
	vout.y = v1.y - v2.y;
	vout.z = v1.z - v2.z;
}


//#define dotSSE(v1, v2, result) store4Vec( mult4Vec((v1), (v2)), temp_f); \
//	result = temp_f[0] + temp_f[1] + temp_f[2];


//inline float dotSSE(const SSE4Vec& v1, const SSE4Vec& v2)
//{
	/*SSE_ALIGN float temp[4];
	store4Vec( mult4Vec((v1), (v2)), temp );
	return temp[0] + temp[1] + temp[2];*/

	/*SSE4Vec prod = mult4Vec((v1), (v2));
	
	SSE4Vec shiftedprod = shiftLeftOneWord(prod);

	float result;
	storeLowWord( add4Vec(add4Vec(prod, shiftedprod), shiftLeftOneWord(prod)) , &result );
	return result;*/

	/*const SSE4Vec prod = mult4Vec(v1, v2);
	
	return prod.m128_f32[0] + prod.m128_f32[1] + prod.m128_f32[2];
}
*/




} //end namespace js


