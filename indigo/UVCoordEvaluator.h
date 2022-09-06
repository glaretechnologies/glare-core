/*=====================================================================
UVCoordEvaluator.h
-------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "../maths/vec2.h"
#include "../maths/Matrix2.h"
#include "../maths/Vec4f.h"
class HitInfo;


/*=====================================================================
UVCoordEvaluator
----------------
Represents a function from intrinsic coordinates (such as barycentric triangle coordinates) to (u, v) coordinates.
Can evaluate the function, or evaluate the Jacobian of the function or its inverse.
=====================================================================*/
class UVCoordEvaluator
{
public:
	UVCoordEvaluator() {}
	virtual ~UVCoordEvaluator() {}

	typedef float UVCoordsRealType;
	typedef Vec2<UVCoordsRealType> UVCoordsType;
	typedef float Vec3RealType;
	typedef Vec4f Vec3Type;


	// uv_set_index may be out of bounds.
	virtual const UVCoordsType getUVCoords(const HitInfo& hitinfo, unsigned int uv_set_index) const = 0;

	//virtual unsigned int getNumUVCoordSets() const = 0;

	/*
		(alpha, beta) are the 'intrinsic' coordinates of the given surface patch.  For example, for the mesh case, they are the hit triangle barycentric coords.
		(u, v) are the uv-coordinates for the given uv-set.

		(u, v) in general is an affine function of (alpha, beta).
		This function returns (u, v) and the partial derivatives of that function.
	*/
	virtual const UVCoordsType getUVCoordsAndPartialDerivs(const HitInfo& hitinfo, unsigned int texcoord_set, Matrix2f& duv_dalphabeta_out) const = 0;


	virtual void getPosAndGeomNormal(const HitInfo& hitinfo, Vec3Type& pos_out, Vec3RealType& pos_os_abs_error_out, Vec3Type& N_g_out) const = 0;

	//virtual float meanCurvature(const HitInfo& hitinfo) const = 0;
};
