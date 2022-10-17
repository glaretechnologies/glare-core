/*=====================================================================
Quat.h
------
Copyright Glare Technologies Limited 2018 - 
File created by ClassTemplate on Thu Mar 12 16:27:07 2009
=====================================================================*/
#pragma once


#include "Vec4f.h"
#include "vec3.h"
#include "Matrix4f.h"
#include <string>


/*=====================================================================
Quat
----
Quaternion.
A lot of the definitions below are from 'Quaternions' by Ken Shoemake:
http://www.cs.ucr.edu/~vbz/resources/quatut.pdf

A unit quaternion corresponds to a rotation of a given angle around a
unit axis - 
The vector part of the quaternion is unit_axis * sin(angle/2), 
scalar part is cos(angle/2).
=====================================================================*/
template <class Real>
class Quat
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	inline Quat() {}
	inline Quat(Real x, Real y, Real z, Real w);
	inline Quat(const Vec3<Real>& v, Real w);
	explicit inline Quat(const Vec4f& v_) : v(v_) {}

	static inline const Quat identity() { return Quat(Vec4f(0,0,0,1)); } // Quaternion corresponding to identity (null) rotation.

	static inline const Quat fromAxisAndAngle(const Vec4f& unit_axis, Real angle);
	static inline const Quat fromAxisAndAngle(const Vec3<Real>& unit_axis, Real angle);

	inline void toAxisAndAngle(Vec4f& unit_axis_out, Real& angle_out) const;

	inline const Quat operator + (const Quat& other) const;
	inline const Quat operator - (const Quat& other) const;
	inline const Quat operator * (const Quat& other) const;
	inline const Quat operator * (Real factor) const;
	inline const Quat operator / (Real factor) const;
	
	inline bool operator == (const Quat& other) const;
	inline bool operator != (const Quat& other) const;

	inline Real norm() const; // length()^2
	inline Real length() const;

	inline const Quat inverse() const;

	inline const Quat conjugate() const;

	inline Matrix4f toMatrix() const; // Assumes norm() = 1
	inline void toMatrix(Matrix4f& mat_out) const; // Assumes norm() = 1

	static inline Quat fromMatrix(const Matrix4f& mat);

	// Assumes norm() = 1
	inline const Vec4f rotateVector(const Vec4f& v) const;
	// Assumes norm() = 1
	inline const Vec4f inverseRotateVector(const Vec4f& v) const;

	// a and b should be unit length for slerp and nlerp.
	static const Quat slerp(const Quat& a, const Quat& b, Real t); 
	static const Quat nlerp(const Quat& a, const Quat& b, Real t);

	const std::string toString() const;

	Vec4f v; // Complex vector first, then scalar part.
};


void quaternionTests();


template <class Real> inline const Quat<Real> normalise(const Quat<Real>& q)
{
	return q / q.length();
}


template <class Real> inline Real dotProduct(const Quat<Real>& a, const Quat<Real>& b)
{
	return dot(a.v, b.v);
}


template <class Real> inline bool epsEqual(const Quat<Real>& a, const Quat<Real>& b)
{
	return ::epsEqual(a.v, b.v);
}


// Unary -
template <class Real> inline const Quat<Real> operator - (const Quat<Real>& q)
{
	return Quat<Real>(-q.v);
}


template <class Real> Quat<Real>::Quat(Real x, Real y, Real z, Real w)
:	v(x, y, z, w)
{}


template <class Real> Quat<Real>::Quat(const Vec3<Real>& v_, Real w_)
:	v(v_.x, v_.y, v_.z, w_)
{}


template <class Real> const Quat<Real> Quat<Real>::fromAxisAndAngle(const Vec4f& unit_axis, Real angle)
{
	assert(unit_axis.isUnitLength());

	const Real omega = angle * (Real)0.5;

	return Quat<Real>(setW(unit_axis * sin(omega), cos(omega)));
}


template <class Real> const Quat<Real> Quat<Real>::fromAxisAndAngle(const Vec3<Real>& unit_axis, Real angle)
{
	assert(unit_axis.isUnitLength());

	const Real omega = angle * (Real)0.5;
	return Quat(unit_axis * sin(omega), cos(omega));
}


template <class Real> void Quat<Real>::toAxisAndAngle(Vec4f& unit_axis_out, Real& angle_out) const
{
	angle_out = 2 * std::acos(v[3]);
	assert(isFinite(angle_out));
	const Vec4f vec = maskWToZero(v);
	const float v_len = vec.length();
	if(v_len == 0) // If the vector part of the quaternion was zero:
		unit_axis_out = Vec4f(1, 0, 0, 0);
	else
		unit_axis_out = vec / v_len;
}


template <class Real> const Quat<Real> Quat<Real>::operator + (const Quat<Real>& other) const
{
	return Quat(v + other.v);
}


template <class Real> const Quat<Real> Quat<Real>::operator - (const Quat<Real>& other) const
{
	return Quat(v - other.v);
}


inline Vec4f negateX(const Vec4f& v)
{
	return Vec4f(_mm_xor_ps(v.v, bitcastToVec4f(Vec4i(0x80000000, 0, 0, 0)).v)); // Flip sign bit
}
inline Vec4f negateY(const Vec4f& v)
{
	return Vec4f(_mm_xor_ps(v.v, bitcastToVec4f(Vec4i(0, 0x80000000, 0, 0)).v));
}
inline Vec4f negateZ(const Vec4f& v)
{
	return Vec4f(_mm_xor_ps(v.v, bitcastToVec4f(Vec4i(0, 0, 0x80000000, 0)).v));
}
inline Vec4f negateW(const Vec4f& v)
{
	return Vec4f(_mm_xor_ps(v.v, bitcastToVec4f(Vec4i(0, 0, 0, 0x80000000)).v));
}


template <class Real> const Quat<Real> Quat<Real>::operator * (const Quat<Real>& other) const
{
	/*
	From Quaternions by Shoemake, http://www.cs.ucr.edu/~vbz/resources/quatut.pdf, Qt_Mul definition on page 7:
	qq.x = qL.w*qR.x + qL.x*qR.w + qL.y*qR.z - qL.z*qR.y;
	qq.y = qL.w*qR.y + qL.y*qR.w + qL.z*qR.x - qL.x*qR.z;
	qq.z = qL.w*qR.z + qL.z*qR.w + qL.x*qR.y - qL.y*qR.x;
	qq.w = qL.w*qR.w - qL.x*qR.x - qL.y*qR.y - qL.z*qR.z;

	qq.x = (+ qL.x*qR.w + qL.y*qR.z) + (qL.w*qR.x - qL.z*qR.y);    // moving first col to col 3
	qq.y = (+ qL.y*qR.w + qL.z*qR.x) + (qL.w*qR.y - qL.x*qR.z);
	qq.z = (+ qL.z*qR.w + qL.x*qR.y) + (qL.w*qR.z - qL.y*qR.x);
	qq.w = (- qL.x*qR.x - qL.y*qR.y) + (qL.w*qR.w - qL.z*qR.z);

	qq.x =  (qL.x*qR.w + qL.y*qR.z) + (qL.w*qR.x - qL.z*qR.y);
	qq.y =  (qL.y*qR.w + qL.z*qR.x) + (qL.w*qR.y - qL.x*qR.z);
	qq.z =  (qL.z*qR.w + qL.x*qR.y) + (qL.w*qR.z - qL.y*qR.x);
	qq.w = -(qL.x*qR.x + qL.y*qR.y) + (qL.w*qR.w - qL.z*qR.z);
	*/
	return Quat<Real>(negateW(
			mul(swizzle<0,1,2,0>(v), swizzle<3,3,3,0>(other.v)) + 
			mul(swizzle<1,2,0,1>(v), swizzle<2,0,1,1>(other.v))
		) +
		(mul(swizzle<3,3,3,3>(v), other.v) - 
		 mul(swizzle<2,0,1,2>(v), swizzle<1,2,0,2>(other.v))
	));
}


template <class Real> const Quat<Real> Quat<Real>::operator * (Real factor) const
{
	return Quat(v * factor);
}


template <class Real> const Quat<Real> Quat<Real>::operator / (Real factor) const
{
	return Quat(v / factor);
}


template <class Real> bool Quat<Real>::operator == (const Quat<Real>& other) const
{
	return v == other.v;
}


template <class Real> bool Quat<Real>::operator != (const Quat<Real>& other) const
{
	return v != other.v;
}


template <class Real> Real Quat<Real>::norm() const
{
	return dot(v, v);
}


template <class Real> Real Quat<Real>::length() const
{
	return v.length();
}


template <class Real> const Quat<Real> Quat<Real>::inverse() const
{
	return conjugate() / norm();
}


template <class Real> const Quat<Real> Quat<Real>::conjugate() const
{
	// Flip sign bits for v.
	const Vec4f mask = bitcastToVec4f(Vec4i(0x80000000, 0x80000000, 0x80000000, 0x0));
	return Quat(_mm_xor_ps(v.v, mask.v));
}


template <class Real> Matrix4f Quat<Real>::toMatrix() const
{
	Matrix4f res;
	toMatrix(res);
	return res;
}


// Adapted from Quaternions - Ken Shoemake
// http://www.cs.ucr.edu/~vbz/resources/quatut.pdf
// Assumes norm() = 1
template <class Real> void Quat<Real>::toMatrix(Matrix4f& mat) const
{
	assert(epsEqual(norm(), (Real)1));

	const Vec4f scale(2.f, 2.f, 2.f, 0.f);
	const Vec4f c0 = mul(negateX(mul(swizzle<1,0,0,0>(v), swizzle<1,1,2,0>(v)) + negateZ(mul(swizzle<2,3,3,0>(v), swizzle<2,2,1,0>(v)))), scale);
	const Vec4f c1 = mul(negateY(mul(swizzle<0,0,1,0>(v), swizzle<1,0,2,0>(v)) + negateX(mul(swizzle<3,2,3,0>(v), swizzle<2,2,0,0>(v)))), scale);
	const Vec4f c2 = mul(negateZ(mul(swizzle<0,1,0,0>(v), swizzle<2,2,0,0>(v)) + negateY(mul(swizzle<3,3,1,0>(v), swizzle<1,0,1,0>(v)))), scale);

	mat.setColumn(0, Vec4f(1,0,0,0) + c0);
	mat.setColumn(1, Vec4f(0,1,0,0) + c1);
	mat.setColumn(2, Vec4f(0,0,1,0) + c2);
	mat.setColumn(3, Vec4f(0,0,0,1));
}


// Convert a rotation matrix to a unit quaternion.
// Input matrix must be a rotation matrix.
// From Quaternions - Ken Shoemake ( http://www.cs.ucr.edu/~vbz/resources/quatut.pdf )
// And http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
template <class Real> Quat<Real> Quat<Real>::fromMatrix(const Matrix4f& mat)
{
	const float m00 = mat.elem(0, 0);
	const float m11 = mat.elem(1, 1);
	const float m22 = mat.elem(2, 2);
	const float tr = m00 + m11 + m22; // Trace
	if(tr >= 0.0)
	{
		float s = std::sqrt(tr + 1);
		float recip_2s = 0.5f / s;
		return Quat<Real>(Vec4f(
			(mat.elem(2, 1) - mat.elem(1, 2)) * recip_2s,
			(mat.elem(0, 2) - mat.elem(2, 0)) * recip_2s,
			(mat.elem(1, 0) - mat.elem(0, 1)) * recip_2s,
			s * 0.5f
		));
	}
	else if(m00 > m11 && m00 > m22)
	{
		float s = std::sqrt(1 + m00 - m11 - m22);
		float recip_2s = 0.5f / s;
		return Quat<Real>(Vec4f(
			s * 0.5f,
			(mat.elem(0, 1) + mat.elem(1, 0)) * recip_2s,
			(mat.elem(0, 2) + mat.elem(2, 0)) * recip_2s,
			(mat.elem(2, 1) - mat.elem(1, 2)) * recip_2s
		));
	}
	else if(m11 > m22)
	{
		float s = std::sqrt(1 + m11 - m00 - m22);
		float recip_2s = 0.5f / s;
		return Quat<Real>(Vec4f(
			(mat.elem(0, 1) + mat.elem(1, 0)) * recip_2s,
			s * 0.5f,
			(mat.elem(1, 2) + mat.elem(2, 1)) * recip_2s,
			(mat.elem(0, 2) - mat.elem(2, 0)) * recip_2s
		));
	}
	else
	{
		// m22 > m00 > m11
		float s = std::sqrt(1 + m22 - m00 - m11);
		float recip_2s = 0.5f / s;
		return Quat<Real>(Vec4f(
			(mat.elem(0, 2) + mat.elem(2, 0)) * recip_2s,
			(mat.elem(1, 2) + mat.elem(2, 1)) * recip_2s,
			s*0.5f,
			(mat.elem(1, 0) - mat.elem(0, 1)) * recip_2s
		));
	}
}


// See http://forwardscattering.org/post/54 - 'Fast rotation of a vector by a quaternion'
// Assumes norm() = 1
template <class Real> const Vec4f Quat<Real>::rotateVector(const Vec4f& vec) const
{
	assert(epsEqual(norm(), (Real)1));

	// Note that crossProduct returns a vector with 0 w component, even if its arguments have non-zero w component.
	const Vec4f v_cross_vec = crossProduct(v, vec);
	const Vec4f sum = mul(v_cross_vec, copyToAll<3>(v)) + crossProduct(v, v_cross_vec);
	return vec + (sum + sum);
}


// Assumes norm() = 1
template <class Real> const Vec4f Quat<Real>::inverseRotateVector(const Vec4f& vec) const
{
	assert(epsEqual(norm(), (Real)1));
	const Quat<Real> inv = this->conjugate(); // Should be pretty cheap.  Since norm() = 1, the inverse is the conjugate.
	return inv.rotateVector(vec);
}


// Adapted from http://number-none.com/product/Understanding%20Slerp,%20Then%20Not%20Using%20It/
template <class Real> const Quat<Real> Quat<Real>::slerp(const Quat<Real>& q0, const Quat<Real>& q1_, Real t)
{
	Quat<Real> q1 = q1_;
	
	// q0 and q1 should be unit length or else
	// something broken will happen.
	assert(epsEqual(q0.norm(), (Real)1.0));
	assert(epsEqual(q1.norm(), (Real)1.0));

	// Compute the cosine of the angle between the two vectors.
	Real dot = dotProduct(q0, q1);

	// If the quaternions have a negative dot product, the angle between them is > pi/2, which means
	// the angle between the corresponding rotations is > pi.  This means that it's shorter to interpolate the other
	// way around the space of rotations. So replace q1 with -q1, which is the antipodal quaternion that still corresponds
	// to the same rotation.
	if(dot < 0)
	{
		q1 = -q1;
		dot = -dot;
		assert(epsEqual(dot, dotProduct(q0, q1)));
	}

	const Real DOT_THRESHOLD = (Real)0.9995;
	if(dot > DOT_THRESHOLD)
	{
		// If the inputs are too close for comfort, linearly interpolate
		// and normalize the result.
		return normalise(Maths::lerp(q0, q1, t));
	}

	assert(dot >= 0 && dot <= DOT_THRESHOLD); // dot should be in domain of acos() now.

	const Real theta_0 = std::acos(dot);  // theta_0 = angle between input vectors
	const Real theta = theta_0 * t; // theta = angle between v0 and result 

	const Quat<Real> q2(normalise(q1 - q0*dot)); // { q0, q2 } is now an orthonormal basis

	return q0*std::cos(theta) + q2*std::sin(theta);
}


// Adapted from http://number-none.com/product/Hacking%20Quaternions/index.html
template <class Real> const Quat<Real> Quat<Real>::nlerp(const Quat<Real>& q0, const Quat<Real>& q1_, Real t)
{
	Quat<Real> q1 = q1_;
	const Real dot = dotProduct(q0, q1);

	// If the quaternions have a negative dot product, the angle between them is > pi/2, which means
	// the angle between the corresponding rotations is > pi.  This means that it's shorter to interpolate the other
	// way around the space of rotations. So replace q1 with -q1, which is the antipodal quaternion that still corresponds
	// to the same rotation.
	if(dot < 0.f)
		q1 = -q1;

	return normalise(Maths::lerp(q0, q1, t));
}


typedef Quat<float> Quatf;
typedef Quat<double> Quatd;
