/*=====================================================================
TreeUtils.cpp
-------------
File created by ClassTemplate on Tue Jul 17 18:46:24 2007
Code By Nicholas Chapman.
=====================================================================*/
#include "TreeUtils.h"


#include "../simpleraytracer/raymesh.h"
#include "../indigo/PrintOutput.h"


namespace js
{


TreeUtils::TreeUtils()
{
}


TreeUtils::~TreeUtils()
{
}


static inline void convertPos(const Vec3f& p, Vec4f& pos_out)
{
	pos_out.x[0] = p.x;
	pos_out.x[1] = p.y;
	pos_out.x[2] = p.z;
	pos_out.x[3] = 1.0f;
}


void TreeUtils::buildRootAABB(const RayMesh& raymesh, AABBox& aabb_out, PrintOutput& print_output)
{
	// NOTE: could do this faster by looping over vertices instead.  But what if there is an unused vertex?

	assert(raymesh.getNumTris() > 0);
	print_output.print("\tCalcing root AABB.");

	SSE_ALIGN Vec4f p;

	convertPos(raymesh.triVertPos(0, 0), p);

	aabb_out.min_ = p;
	aabb_out.max_ = p;

	for(unsigned int i=0; i<raymesh.getNumTris(); ++i)
	{
		convertPos(raymesh.triVertPos(i, 0), p);
		aabb_out.enlargeToHoldPoint(p);

		convertPos(raymesh.triVertPos(i, 1), p);
		aabb_out.enlargeToHoldPoint(p);

		convertPos(raymesh.triVertPos(i, 2), p);
		aabb_out.enlargeToHoldPoint(p);

		/*const SSE_ALIGN Vec4f v0(raymesh.triVertPos(i, 0));
		const SSE_ALIGN Vec4f v1(raymesh.triVertPos(i, 1));
		const SSE_ALIGN Vec4f v2(raymesh.triVertPos(i, 2));

		aabb_out.enlargeToHoldAlignedPoint(v0);
		aabb_out.enlargeToHoldAlignedPoint(v1);
		aabb_out.enlargeToHoldAlignedPoint(v2);*/
	}

	print_output.print("\t\tDone.");
	print_output.print("\t\tRoot AABB min: " + aabb_out.min_.toString());
	print_output.print("\t\tRoot AABB max: " + aabb_out.max_.toString());
}


float TreeUtils::getTreeSpecificMinT(const AABBox& aabb)
{
	return 0.0; // TEMP HACK 
	//return aabb.axisLength(aabb.longestAxis()) * 5.0e-7f;
	//return aabb.axisLength(aabb.longestAxis()) * 1.0e-5f;
}


} //end namespace js
