/*=====================================================================
BadouelTri.cpp
--------------
File created by ClassTemplate on Sun Jul 03 08:28:36 2005
Code By Nicholas Chapman.
=====================================================================*/
#include "jscol_BadouelTri.h"




namespace js
{




BadouelTri::BadouelTri()
{
	assert(sizeof(BadouelTri) == 48);

	project_axis_1 = 0;
	project_axis_2 = 1;
}


BadouelTri::~BadouelTri()
{

}

void BadouelTri::set(const Vec3f& vert0, const Vec3f& vert1, const Vec3f& vert2)
{
	//NOTE: counterclockwise winding order for front faces.
	normal = normalise(::crossProduct(vert1-vert0, vert2-vert0));

	assert(!isNAN(normal.x));
	assert(!isNAN(normal.y));
	assert(!isNAN(normal.z));

	assert(::epsEqual(normal.length(), 1.0f));

	dist = normal.dot(vert0);

	if(std::fabs(normal.x) >= std::fabs(normal.y))
	{
		if(std::fabs(normal.x) >= std::fabs(normal.z))
		{
			project_axis_1 = 1;
			project_axis_2 = 2;
		}
		else
		{
			assert(std::fabs(normal.z) >= std::fabs(normal.x) && std::fabs(normal.z) >= std::fabs(normal.y));
			project_axis_1 = 0;
			project_axis_2 = 1;
		}
	}
	else
	{
		if(fabs(normal.y) >= fabs(normal.z))
		{
			assert(std::fabs(normal.y) >= std::fabs(normal.x) && std::fabs(normal.y) >= std::fabs(normal.z));
			project_axis_1 = 0;
			project_axis_2 = 2;
		}
		else
		{
			assert(std::fabs(normal.z) >= std::fabs(normal.x) && std::fabs(normal.z) >= std::fabs(normal.y));
			project_axis_1 = 0;
			project_axis_2 = 1;
		}
	}

	/*u = vert0[project_axis_1];
	v = vert0[project_axis_2];

	u1 = vert1[project_axis_1] - vert0[project_axis_1];
	v1 = vert1[project_axis_2] - vert0[project_axis_2];

	u2 = vert2[project_axis_1] - vert0[project_axis_1];
	v2 = vert2[project_axis_2] - vert0[project_axis_2];*/

	v0_1 = vert0[project_axis_1];
	v0_2 = vert0[project_axis_2];

	const float u1 = vert1[project_axis_1] - vert0[project_axis_1];
	const float v1 = vert1[project_axis_2] - vert0[project_axis_2];

	const float u2 = vert2[project_axis_1] - vert0[project_axis_1];
	const float v2 = vert2[project_axis_2] - vert0[project_axis_2];

	const float det = u1*v2 - u2*v1;

	if(std::fabs(det) < 0.00000001f)
	{
		//make a transform that will always return out of bound barycentric coords.
		t11 = t12 = t21 = t22 = 1e9f;
	}
	else
	{
		const float inv_det = 1.0f / (u1*v2 - u2*v1);

		//WAS FAILING assert(!isInf(inv_det));
		//TEMP:
		if(isInf(inv_det))
		    t11 = t12 = t21 = t22 = 1e9f;
		else
		{

		t11 = inv_det * v2;
		t12 = inv_det * -u2;
		t21 = inv_det * -v1;
		t22 = inv_det * u1;
		}
	}
}





} //end namespace js






