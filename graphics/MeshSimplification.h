/*=====================================================================
MeshSimplification.h
--------------------
Copyright Glare Technologies Limited 2024
=====================================================================*/
#pragma once


#include "BatchedMesh.h"
namespace glare { class TaskManager; }


/*=====================================================================
MeshSimplification
------------------

=====================================================================*/
namespace MeshSimplification
{

// target_reduction_ratio should be > 1.  Note that this is just a limit on how far the mesh is simplified: simplification stops as soon as either
// target_reduction_ratio or target_error is reached.
// The units of target_error depend on sloppy:
// If sloppy is false, target_error is an absolute error in object-space units, e.g. 0.02 = 2 cm of deformation.  (meshopt_SimplifyErrorAbsolute is passed to meshoptimizer)
// If sloppy is true, target_error is the error relative to mesh extents (the longest side of the mesh AABB), e.g. 0.01 = 1% deformation.  (meshopt_simplifySloppy has no absolute error option)
BatchedMeshRef buildSimplifiedMesh(const BatchedMesh& mesh, float target_reduction_ratio, float target_error, bool sloppy);

BatchedMeshRef removeSmallComponents(const BatchedMeshRef mesh, float target_error);

// index_map_out is a map from old to new index.
BatchedMeshRef removeInvisibleTriangles(const BatchedMeshRef mesh, std::vector<uint32>& index_map_out, glare::TaskManager& task_manager);

void test();


};
