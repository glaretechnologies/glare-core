/*=====================================================================
GaussianSplatData.h
-------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "../maths/vec3.h"
#include "../maths/Vec4f.h"
#include "../physics/jscol_aabbox.h"
#include "../utils/Vector.h"
#include "../utils/Reference.h"
#include "../utils/ThreadSafeRefCounted.h"


/*=====================================================================
GaussianSplatData
-----------------
A decoded Gaussian splat cloud in object space: one entry per splat in each
of the parallel arrays below.

This is the CPU-side, GPU-agnostic representation - it says nothing about how
the splats are packed into textures for rendering (see
opengl/GaussianSplatRenderer.h for that), and nothing about the file format
they were decoded from (see SOGDecoder.h).

Note that only the DC term (sh0) of the spherical harmonics is kept, so
colours are view-independent.
=====================================================================*/
class GaussianSplatData : public ThreadSafeRefCounted
{
public:
	size_t numSplats() const { return positions.size(); }

	js::Vector<Vec3f, 16> positions;
	js::Vector<Vec3f, 16> scales; // Per-axis linear scale factors (already exponentiated out of the log domain the file stores them in).
	js::Vector<Vec4f, 16> rotations; // Unit quaternions, stored as (x, y, z, w).
	js::Vector<Vec4f, 16> colours; // (r, g, b, opacity), all in [0, 1].

	js::AABBox aabb_os; // Bounds the splat centres only - does not account for the extent of the splats themselves.
};


typedef Reference<GaussianSplatData> GaussianSplatDataRef;
