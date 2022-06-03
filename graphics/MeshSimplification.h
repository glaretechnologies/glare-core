/*=====================================================================
MeshSimplification.h
--------------------
Copyright Glare Technologies Limited 2022
=====================================================================*/
#pragma once


#include "BatchedMesh.h"


/*=====================================================================
MeshSimplification
------------------

=====================================================================*/
namespace MeshSimplification
{


// target_error represents the error relative to mesh extents that can be tolerated, e.g. 0.01 = 1% deformation
BatchedMeshRef buildSimplifiedMesh(const BatchedMesh& mesh, float target_reduction_ratio, float target_error, bool sloppy);


void test();


};
