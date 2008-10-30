/*=====================================================================
TreeUtils.cpp
-------------
File created by ClassTemplate on Tue Jul 17 18:46:24 2007
Code By Nicholas Chapman.
=====================================================================*/
#include "TreeUtils.h"


#include "../simpleraytracer/raymesh.h"


namespace js
{


TreeUtils::TreeUtils()
{
}


TreeUtils::~TreeUtils()
{
}


void TreeUtils::buildRootAABB(const RayMesh& raymesh, AABBox& aabb_out)
{
	// NOTE: could do this faster by looping over vertices instead.  But what if there is an unused vertex?

	assert(raymesh.getNumTris() > 0);
	conPrint("\tCalcing root AABB.");

	aabb_out.min_ = raymesh.triVertPos(0, 0);
	aabb_out.min_.padding = 1.0f;
	aabb_out.max_ = raymesh.triVertPos(0, 0);
	aabb_out.max_.padding = 1.0f;

	for(unsigned int i=0; i<raymesh.getNumTris(); ++i)
	{
		const SSE_ALIGN PaddedVec3f v0(raymesh.triVertPos(i, 0));
		const SSE_ALIGN PaddedVec3f v1(raymesh.triVertPos(i, 1));
		const SSE_ALIGN PaddedVec3f v2(raymesh.triVertPos(i, 2));

		aabb_out.enlargeToHoldAlignedPoint(v0);
		aabb_out.enlargeToHoldAlignedPoint(v1);
		aabb_out.enlargeToHoldAlignedPoint(v2);
	}

	conPrint("\t\tDone.");
	conPrint("\t\tRoot AABB min: " + aabb_out.min_.toString());
	conPrint("\t\tRoot AABB max: " + aabb_out.max_.toString());
}


} //end namespace js
