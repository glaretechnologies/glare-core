/*=====================================================================
Quat.h
------
File created by ClassTemplate on Thu Mar 12 16:27:07 2009
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __QUAT_H_666_
#define __QUAT_H_666_


#include "vec3.h"
#include "matrix3.h"


/*=====================================================================
Quat
----
Quaternion.
=====================================================================*/
template <class Real>
class Quat
{
public:
	inline Quat() {}
	inline Quat(const Quat& other);
	inline Quat(const Vec3<Real>& v, Real w);
	inline ~Quat() {}

	static inline const Quat identity() { return Quat(Vec3<Real>(0.0), (Real)1.0); }

	static inline const Quat fromAxisAndAngle(const Vec3<Real>& unit_axis, Real angle);

	inline const Quat operator + (const Quat& other) const;
	inline const Quat operator - (const Quat& other) const;
	inline const Quat operator * (const Quat& other) const;
	inline const Quat operator * (Real factor) const;
	
	inline bool operator == (const Quat& other) const;
	inline bool operator != (const Quat& other) const;

	inline Real norm() const;
	inline Real length() const;

	inline void setToNormalised();

	inline const Quat inverse() const;

	inline const Quat conjugate() const;

	inline void toMatrix(Matrix3<Real>& mat_out) const;

	static const Quat slerp(const Quat& a, const Quat& b, Real t);

	Vec3<Real> v;
	Real w;
};


template <class Real> inline const Quat<Real> normalise(const Quat<Real>& q)
{
	return q * ((Real)1.0 / q.length());
}


template <class Real> inline Real dotProduct(const Quat<Real>& a, const Quat<Real>& b)
{
	return a.w * b.w + dot(a.v, b.v);
}


template <class Real> Quat<Real>::Quat(const Quat<Real>& other)
:	v(other.v), w(other.w)
{}


template <class Real> Quat<Real>::Quat(const Vec3<Real>& v_, Real w_)
:	v(v_), w(w_)
{}


template <class Real> const Quat<Real> Quat<Real>::fromAxisAndAngle(const Vec3<Real>& unit_axis, Real angle)
{
	assert(unit_axis.isUnitLength());

	const Real omega = angle * (Real)0.5;
	return Quat(unit_axis * sin(omega), cos(omega));
}


template <class Real> const Quat<Real> Quat<Real>::operator + (const Quat<Real>& other) const
{
	return Quat(v + other.v, w + other.w);
}


template <class Real> const Quat<Real> Quat<Real>::operator - (const Quat<Real>& other) const
{
	return Quat(v - other.v, w - other.w);
}


template <class Real> const Quat<Real> Quat<Real>::operator * (const Quat<Real>& other) const
{
	return Quat(
		::crossProduct(v, other.v) + other.v * w + v * other.w, 
		w * other.w - ::dot(v, other.v)
		);
}


template <class Real> const Quat<Real> Quat<Real>::operator * (Real factor) const
{
	return Quat(
		v * factor,
		w * factor
		);
}


template <class Real> bool Quat<Real>::operator == (const Quat<Real>& other) const
{
	return v == other.v && w == other.w;
}


template <class Real> bool Quat<Real>::operator != (const Quat<Real>& other) const
{
	return v != other.v || w != other.w;
}


template <class Real> Real Quat<Real>::norm() const
{
	return w*w + dot(v, v);
}


template <class Real> Real Quat<Real>::length() const
{
	return std::sqrt(w*w + dot(v, v));
}


template <class Real> void Quat<Real>::setToNormalised()
{
	*this = normalise(*this);
}


template <class Real> const Quat<Real> Quat<Real>::inverse() const
{
	return conjugate() * ((Real)1.0 / norm());
}


template <class Real> const Quat<Real> Quat<Real>::conjugate() const
{
	return Quat(v * (Real)-1.0, w);
}


// From Quaternions - Ken Shoemake
// http://www.sfu.ca/~jwa3/cmpt461/files/quatut.pdf
template <class Real> void Quat<Real>::toMatrix(Matrix3<Real>& mat) const
{
	const Real Nq = norm();
	const Real s = (Nq > (Real)0.0) ? ((Real)2.0 / Nq) : (Real)0.0;
	const Real xs = v.x*s, ys = v.y*s, zs = v.z*s;
	const Real wx = w*xs, wy = w*ys, wz = w*zs;
	const Real xx = v.x*xs, xy = v.x*ys, xz = v.x*zs;
	const Real yy = v.y*ys, yz = v.y*zs, zz = v.z*zs;

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
	const double theta_0 = std::acos(myClamp(dot, (Real)-1.0, (Real)1.0));  // theta_0 = angle between input vectors
    const double theta = theta_0 * t;    // theta = angle between v0 and result 

    const Quat<Real> q2(normalise(q1 - q0*dot)); // { q0, q2 } is now an orthonormal basis

	return q0*std::cos(theta) + q2*std::sin(theta);
}


typedef Quat<float> Quatf;
typedef Quat<double> Quatd;


#endif //__QUAT_H_666_
