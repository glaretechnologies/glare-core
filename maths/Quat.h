/*=====================================================================
Quat.h
------
Copyright Glare Technologies Limited 2014 - 
File created by ClassTemplate on Thu Mar 12 16:27:07 2009
=====================================================================*/
#pragma once


#include "SSE.h"
#include "Vec4f.h"
#include "vec3.h"
#include "matrix3.h"
#include "Matrix4f.h"
#include <string>


/*=====================================================================
Quat
----
Quaternion.
A lot of the definitions below are from 'Quaternions' by Ken Shoemake:
http://www.cs.ucr.edu/~vbz/resources/quatut.pdf
=====================================================================*/
template <class Real>
class Quat
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	inline Quat() {}
	inline Quat(const Vec3<Real>& v, Real w);
	explicit inline Quat(const Vec4f& v_) : v(v_) {}

	static inline const Quat identity() { return Quat(Vec3<Real>(0.0), (Real)1.0); }

	static inline const Quat fromAxisAndAngle(const Vec3<Real>& unit_axis, Real angle);

	inline const Quat operator + (const Quat& other) const;
	inline const Quat operator - (const Quat& other) const;
	//inline const Quat operator * (const Quat& other) const;
	inline const Quat operator * (Real factor) const;
	
	inline bool operator == (const Quat& other) const;
	inline bool operator != (const Quat& other) const;

	inline Real norm() const; // length()^2
	inline Real length() const;

	inline const Quat inverse() const;

	inline const Quat conjugate() const;

	inline void toMatrix(Matrix3<Real>& mat_out) const;
	inline void toMatrix(Matrix4f& mat_out) const;

	static inline Quat fromMatrix(const Matrix3<Real>& mat);
	static inline Quat fromMatrix(const Matrix4f& mat);

	// Assumes norm() = 1
	const Vec4f rotateVector(const Vec4f& v) const;
	// Assumes norm() = 1
	const Vec4f inverseRotateVector(const Vec4f& v) const;

	static const Quat slerp(const Quat& a, const Quat& b, Real t);
	static const Quat nlerp(const Quat& a, const Quat& b, Real t);

	const std::string toString() const;

	Vec4f v; // Complex vector first, then scalar part.
};


void quaternionTests();


template <class Real> inline const Quat<Real> normalise(const Quat<Real>& q)
{
	return q * (1 / q.length());
}


template <class Real> inline Real dotProduct(const Quat<Real>& a, const Quat<Real>& b)
{
	return dot(a.v, b.v);
}


template <class Real> inline bool epsEqual(const Quat<Real>& a, const Quat<Real>& b)
{
	return ::epsEqual(a.v, b.v);
}


template <class Real> Quat<Real>::Quat(const Vec3<Real>& v_, Real w_)
:	v(v_.x, v_.y, v_.z, w_)
{}


template <class Real> const Quat<Real> Quat<Real>::fromAxisAndAngle(const Vec3<Real>& unit_axis, Real angle)
{
	assert(unit_axis.isUnitLength());

	const Real omega = angle * (Real)0.5;
	return Quat(unit_axis * sin(omega), cos(omega));
}


template <class Real> const Quat<Real> Quat<Real>::operator + (const Quat<Real>& other) const
{
	return Quat(v + other.v);
}


template <class Real> const Quat<Real> Quat<Real>::operator - (const Quat<Real>& other) const
{
	return Quat(v - other.v);
}


//template <class Real> const Quat<Real> Quat<Real>::operator * (const Quat<Real>& other) const
//{
//	return Quat(
//		::crossProduct(v, other.v) + other.v * w + v * other.w, 
//		w * other.w - ::dot(v, other.v)
//		);
//}


template <class Real> const Quat<Real> Quat<Real>::operator * (Real factor) const
{
	return Quat(v * factor);
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
	return conjugate() * (1 / norm());
}


template <class Real> const Quat<Real> Quat<Real>::conjugate() const
{
	return Quat(Vec4f(-v.x[0], -v.x[1], -v.x[2], v.x[3]));
}


// From Quaternions - Ken Shoemake
// http://www.cs.ucr.edu/~vbz/resources/quatut.pdf
template <class Real> void Quat<Real>::toMatrix(Matrix3<Real>& mat) const
{
	const Real Nq = norm();
	const Real s = (Nq > (Real)0.0) ? ((Real)2.0 / Nq) : (Real)0.0;
	const Real xs = v.x[0]*s, ys = v.x[1]*s, zs = v.x[2]*s;
	const Real wx = v.x[3]*xs, wy = v.x[3]*ys, wz = v.x[3]*zs;
	const Real xx = v.x[0]*xs, xy = v.x[0]*ys, xz = v.x[0]*zs;
	const Real yy = v.x[1]*ys, yz = v.x[1]*zs, zz = v.x[2]*zs;

	mat.e[0] = (Real)1.0 - (yy + zz);
	mat.e[1] = xy - wz;
	mat.e[2] = xz + wy;
	mat.e[3] = xy + wz;
	mat.e[4] = (Real)1.0 - (xx + zz);
	mat.e[5] = yz - wx;
	mat.e[6] = xz - wy;
	mat.e[7] = yz + wx;
	mat.e[8] = (Real)1.0 - (xx + yy);
}


template <class Real> void Quat<Real>::toMatrix(Matrix4f& mat) const
{
	const Real Nq = norm();
	const Real s = (Nq > (Real)0.0) ? ((Real)2.0 / Nq) : (Real)0.0;
	const Real xs = v.x[0]*s, ys = v.x[1]*s, zs = v.x[2]*s;
	const Real wx = v.x[3]*xs, wy = v.x[3]*ys, wz = v.x[3]*zs;
	const Real xx = v.x[0]*xs, xy = v.x[0]*ys, xz = v.x[0]*zs;
	const Real yy = v.x[1]*ys, yz = v.x[1]*zs, zz = v.x[2]*zs;

	/*
	Matrix4f layout:
	0	4	8	12
	1	5	9	13
	2	6	10	14
	3	7	11	15
	*/
	mat.e[0] = (Real)1.0 - (yy + zz);
	mat.e[4] = xy - wz;
	mat.e[8] = xz + wy;
	mat.e[1] = xy + wz;
	mat.e[5] = (Real)1.0 - (xx + zz);
	mat.e[9] = yz - wx;
	mat.e[2] = xz - wy;
	mat.e[6] = yz + wx;
	mat.e[10] = (Real)1.0 - (xx + yy);

	mat.e[3] = mat.e[7] = mat.e[11] = mat.e[12] = mat.e[13] = mat.e[14] = 0.0f;
	mat.e[15] = 1.0f;
}


// Convert a rotation matrix to a unit quaternion.
// Input matrix must be a rotation matrix.
// From Quaternions - Ken Shoemake ( http://www.cs.ucr.edu/~vbz/resources/quatut.pdf )
// And http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
template <class Real> Quat<Real> Quat<Real>::fromMatrix(const Matrix3<Real>& mat)
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


// Similar to above, but takes Matrix4f.
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


// Assumes norm() = 1
template <class Real> const Vec4f Quat<Real>::rotateVector(const Vec4f& vec) const
{
	assert(epsEqual(norm(), (Real)1));
	const Real s = 2;
	const Real xs = v.x[0]*s, ys = v.x[1]*s, zs = v.x[2]*s;
	const Real wx = v.x[3]*xs, wy = v.x[3]*ys, wz = v.x[3]*zs;
	const Real xx = v.x[0]*xs, xy = v.x[0]*ys, xz = v.x[0]*zs;
	const Real yy = v.x[1]*ys, yz = v.x[1]*zs, zz = v.x[2]*zs;

	return Vec4f(
		(1 - (yy + zz))*vec.x[0] + (xy - wz)      *vec.x[1] + (xz + wy)      *vec.x[2],
		(xy + wz)      *vec.x[0] + (1 - (xx + zz))*vec.x[1] + (yz - wx)      *vec.x[2],
		(xz - wy)      *vec.x[0] + (yz + wx)      *vec.x[1] + (1 - (xx + yy))*vec.x[2],
		vec.x[3]
	);

	// This code below was slower:
	//const Vec4f& p = vec;
	/*const float w = v[3];
	const Vec4f v3 = maskWToZero(v);
	return vec + crossProduct(v3, vec)*2*w + crossProduct(v3, crossProduct(v3, vec))*2;*/
}


// Assumes norm() = 1
template <class Real> const Vec4f Quat<Real>::inverseRotateVector(const Vec4f& vec) const
{
	assert(epsEqual(norm(), (Real)1));
	const Quat<Real> inv = this->conjugate(); // Should be pretty cheap.  Since norm() = 1, the inverse is the conjugate.
	return inv.rotateVector(vec);
}


// Adapted from http://number-none.com/product/Understanding%20Slerp,%20Then%20Not%20Using%20It/
template <class Real> const Quat<Real> Quat<Real>::slerp(const Quat<Real>& q0, const Quat<Real>& q1, Real t)
{
	// q0 and q1 should be unit length or else
    // something broken will happen.
	assert(epsEqual(q0.norm(), (Real)1.0));
	assert(epsEqual(q1.norm(), (Real)1.0));

    // Compute the cosine of the angle between the two vectors.
    const Real dot = dotProduct(q0, q1);

	const Real DOT_THRESHOLD = (Real)0.9995;
    if(dot > DOT_THRESHOLD)
	{
        // If the inputs are too close for comfort, linearly interpolate
        // and normalize the result.
		return normalise(Maths::lerp(q0, q1, t));
    }

	// Robustness: Stay within domain of acos()
	const Real theta_0 = std::acos(myClamp(dot, (Real)-1.0, (Real)1.0));  // theta_0 = angle between input vectors
    const Real theta = theta_0 * t;    // theta = angle between v0 and result 

    const Quat<Real> q2(normalise(q1 - q0*dot)); // { q0, q2 } is now an orthonormal basis

	return q0*std::cos(theta) + q2*std::sin(theta);
}


// Adapted from http://number-none.com/product/Hacking%20Quaternions/index.html
template <class Real> const Quat<Real> Quat<Real>::nlerp(const Quat<Real>& q0, const Quat<Real>& q1, Real t)
{
	//const Real attenuation = 0.7878088;
	//const Real k = 0.5069269;
	//const Real b =  2 * k;
	//const Real c = -3 * k;
	//const Real d =  1 + k;

	//// Compute the cosine of the angle between the two vectors.
	//const Real dot = dotProduct(q0, q1);

	//Real factor = 1 - attenuation * dot;
	//factor *= factor;
	//k *= factor;

	//double tprime_divided_by_t = t * (b * t + c) + d;
	//return tprime_divided_by_t;

	return normalise(Maths::lerp(q0, q1, t));
}


typedef Quat<float> Quatf;
typedef Quat<double> Quatd;
