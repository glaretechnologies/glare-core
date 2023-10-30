/*=====================================================================
IndigoGeometryUtils.h
---------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "IndigoOStream.h"
#include "IndigoString.h"
#include <limits>
#include <cmath>


namespace Indigo
{


template<typename T>
inline const T indigoMin(const T a, const T b)
{
	return (a < b) ? a : b;
}


template<typename T>
inline const T indigoMax(const T a, const T b)
{
	return (a > b) ? a : b;
}


inline bool isFinite(float x)
{
#if defined(_WIN32)
	return _finite(x) != 0;
#else
	return std::isfinite(x) != 0;
#endif
}


inline bool isFinite(double x)
{
#if defined(_WIN32)
	return _finite(x) != 0;
#else
	return std::isfinite(x) != 0;
#endif
}


inline static const String tabsString(unsigned int num)
{
	return String(num, '\t');
}


template <class T>
class Vec2
{
public:
	USE_INDIGO_SDK_ALLOCATOR

	Vec2() : x(0), y(0) { }
	explicit Vec2(T v) : x(v), y(v) { }
	Vec2(T x_, T y_) : x(x_), y(y_) { }
	inline void set(T v) { x = v; y = v; }
	inline void set(T x_, T y_) { x = x_; y = y_; }
	inline void set(const Vec2<T>& v) { x = v.x; y = v.y; }

	void writeXML(OStream& stream, unsigned int depth) const
	{
		stream << tabsString(depth);
		stream.writeDouble(x);
		stream << " ";
		stream.writeDouble(y);
		stream << tabsString(depth);
		stream << "\n";
	}

	inline T& operator[] (unsigned int index)
	{
		assert(index >= 0 && index < 2);
		return ((T*)(&x))[index];
	}

	inline const T& operator[] (unsigned int index) const
	{
		assert(index >= 0 && index < 2);
		return ((T*)(&x))[index];
	}

	inline bool operator == (const Vec2<T>& rhs) const
	{
		return x == rhs.x && y == rhs.y;
	}

	inline bool operator != (const Vec2<T>& rhs) const
	{
		return x != rhs.x || y != rhs.y;
	}

	inline const Vec2<T> operator - (const Vec2<T>& rhs) const
	{
		return Vec2<T>(x - rhs.x, y - rhs.y);
	}

	inline const Vec2<T> operator * (T factor) const
	{
		return Vec2<T>(x * factor, y * factor);
	}

	inline Vec2<T>& operator *= (T factor)
	{
		x *= factor;
		y *= factor;
		return *this;
	}

	inline T length() const
	{
		return sqrt(x*x + y*y);
	}

	T x, y;
};

typedef Vec2<double> Vec2d;
typedef Vec2<float> Vec2f;


template<typename T>
inline bool operator < (const Vec2<T>& a, const Vec2<T>& b)
{
	if(a.x < b.x)
		return true;
	else if(a.x > b.x)
		return false;
	else	//else if x == rhs.x
	{
		return a.y < b.y;
	}
}


template <class T>
class Vec3
{
public:
	USE_INDIGO_SDK_ALLOCATOR

	Vec3() : x(0), y(0), z(0) { }
	explicit Vec3(T v) : x(v), y(v), z(v) { }
	Vec3(T x_, T y_, T z_) : x(x_), y(y_), z(z_) { }

	inline void set(T v) { x = v; y = v; z = v; }
	inline void set(T x_, T y_, T z_) { x = x_; y = y_; z = z_; }
	inline void set(const Vec3<T>& v) { x = v.x; y = v.y; z = v.z; }

	inline T& operator[] (const unsigned int index)
	{
		assert(index >= 0 && index < 3);
		return ((T*)(&x))[index];
	}

	inline const T& operator[] (const unsigned int index) const
	{
		assert(index >= 0 && index < 3);
		return ((T*)(&x))[index];
	}

	inline const Vec3<T> operator + (const Vec3<T>& rhs) const
	{
		return Vec3<T>(x + rhs.x, y + rhs.y, z + rhs.z);
	}

	inline const Vec3<T> operator - (const Vec3<T>& rhs) const
	{
		return Vec3<T>(x - rhs.x, y - rhs.y, z - rhs.z);
	}

	inline const Vec3<T> operator * (const T factor) const
	{
		return Vec3<T>(x * factor, y * factor, z * factor);
	}

	inline Vec3<T>& operator *= (const T factor)
	{
		x *= factor;
		y *= factor;
		z *= factor;
		return *this;
	}

	inline T length2() const { return x*x + y*y + z*z; }

	inline T length() const { return std::sqrt(length2()); }

	inline bool operator == (const Vec3<T>& b) const { return x == b.x && y == b.y && z == b.z; }
	inline bool operator != (const Vec3<T>& b) const { return x != b.x || y != b.y || z != b.z; }


	T x, y, z;
};

typedef Vec3<double> Vec3d;
typedef Vec3<float> Vec3f;


template <class T>
inline const Vec3<T> normalise(const Vec3<T>& v)
{
	return v * ((T)1.0 / v.length());
}


template <class T>
inline T dotProduct(const Vec3<T>& a, const Vec3<T>& b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}


template <class T>
inline const Vec3<T> crossProduct(const Vec3<T>& v1, const Vec3<T>& v2)
{
	return Vec3<T>(
	(v1.y * v2.z) - (v1.z * v2.y),
	(v1.z * v2.x) - (v1.x * v2.z),
	(v1.x * v2.y) - (v1.y * v2.x));
}


template<typename T>
inline const Vec3<T> vec3Min(const Vec3<T>& v1, const Vec3<T>& v2)
{
	return Vec3<T>(indigoMin(v1.x, v2.x), indigoMin(v1.y, v2.y), indigoMin(v1.z, v2.z));
}


template<typename T>
inline const Vec3<T> vec3Max(const Vec3<T>& v1, const Vec3<T>& v2)
{
	return Vec3<T>(indigoMax(v1.x, v2.x), indigoMax(v1.y, v2.y), indigoMax(v1.z, v2.z));
}


template<typename T>
inline bool operator < (const Vec3<T>& a, const Vec3<T>& b)
{
	if(a.x < b.x)
		return true;
	else if(a.x > b.x)
		return false;
	else // else x == rhs.x
	{
		if(a.y < b.y)
			return true;
		else if(a.y > b.y)
			return false;
		else
		{
			return a.z < b.z;
		}
	}
}


template<typename T>
class AABB
{
public:
	USE_INDIGO_SDK_ALLOCATOR

	AABB() { setNull(); }
	AABB(const Vec3<T>& minbound, const Vec3<T>& maxbound) { bound[0] = minbound; bound[1] = maxbound; }

	void setNull()
	{
		bound[0] = Vec3<T>( std::numeric_limits<T>::infinity()); // min bound is positive infinity
		bound[1] = Vec3<T>(-std::numeric_limits<T>::infinity()); // max bound is negative infinity
	}

	bool isNull() const
	{
		return
			bound[0] == Vec3<T>( std::numeric_limits<T>::infinity()) && 
			bound[1] == Vec3<T>(-std::numeric_limits<T>::infinity());
	}

	void contain(const Vec3<T>& v)
	{
		bound[0] = vec3Min(bound[0], v);
		bound[1] = vec3Max(bound[1], v);
	}

	void contain(const AABB<T>& aabb)
	{
		bound[0] = vec3Min(bound[0], aabb.bound[0]);
		bound[1] = vec3Max(bound[1], aabb.bound[1]);
	}

	Vec3<T> bound[2];
};


template <class T>
inline static T getTriArea(const Vec3<T>& v0, const Vec3<T>& v1, const Vec3<T>& v2)
{
	return crossProduct<T>(v1 - v0, v2 - v0).length() * 0.5f;
}


class Matrix2
{
public:
	USE_INDIGO_SDK_ALLOCATOR

	Matrix2() { e[0] = 1; e[1] = 0; e[2] = 0; e[3] = 1; }
	Matrix2(double e0, double e1, double e2, double e3) { e[0] = e0; e[1] = e1; e[2] = e2; e[3] = e3; }
	explicit Matrix2(const double* elems) { e[0] = elems[0]; e[1] = elems[1]; e[2] = elems[2]; e[3] = elems[3]; }
	explicit Matrix2(const float* elems)  { e[0] = elems[0]; e[1] = elems[1]; e[2] = elems[2]; e[3] = elems[3]; }

	inline bool isIdentity() const { return e[0] == 1.0 && e[1] == 0.0 && e[2] == 0.0 && e[3] == 1.0; }

	double e[4]; // In row-major order.
};


template<typename T>
class Matrix3
{
public:
	USE_INDIGO_SDK_ALLOCATOR

	Matrix3() { setToIdentity(); }
	Matrix3(const Vec3<T>& column0, const Vec3<T>& column1, const Vec3<T>& column2)
	{
		set(column0, column1, column2);
	}
	Matrix3(const Vec3<T>& axis, const T angle) { fromAxisAngle(axis, angle); }

	void set(const Vec3<T>& column0, const Vec3<T>& column1, const Vec3<T>& column2)
	{
		e[0] = column0.x; e[1] = column1.x; e[2] = column2.x;
		e[3] = column0.y; e[4] = column1.y; e[5] = column2.y;
		e[6] = column0.z; e[7] = column1.z; e[8] = column2.z;
	}

	void setToIdentity()
	{
		set(Vec3<T>(1, 0, 0), Vec3<T>(0, 1, 0), Vec3<T>(0, 0, 1));
	}

	bool isIdentity() const { return e[0] == 1 && e[1] == 0 && e[2] == 0 && e[3] == 0 && e[4] == 1 && e[5] == 0 && e[6] == 0 && e[7] == 0 && e[8] == 1; }

	static Matrix3<T> getIdentity() { return Matrix3<T>(Vec3<T>(1, 0, 0), Vec3<T>(0, 1, 0), Vec3<T>(0, 0, 1)); }
	
	void fromAxisAngle(const Vec3<T>& axis, const T angle)
	{
		assert(axis.isUnitLength());

		//-----------------------------------------------------------------
		//build the rotation matrix
		//see http://mathworld.wolfram.com/RodriguesRotationFormula.html
		//-----------------------------------------------------------------
		const T a = axis.x;
		const T b = axis.y;
		const T c = axis.z;
		const T cost = std::cos(angle);
		const T sint = std::sin(angle);
		const T asint = a * sint;
		const T bsint = b * sint;
		const T csint = c * sint;
		const T one_minus_cost = (T)1.0 - cost;

		set(
			Vec3<T>(a*a*one_minus_cost + cost, a*b*one_minus_cost + csint, a*c*one_minus_cost - bsint), // column 0
			Vec3<T>(a*b*one_minus_cost - csint, b*b*one_minus_cost + cost, b*c*one_minus_cost + asint), // column 1
			Vec3<T>(a*c*one_minus_cost + bsint, b*c*one_minus_cost - asint, c*c*one_minus_cost + cost)  // column 2
			);
	}

	bool operator == (const Matrix3<T>& b) const
	{
		for(int i=0; i<9; ++i)
			if(e[i] != b.e[i])
				return false;
		return true;
	}

	Vec3<T> operator * (const Vec3<T>& rhs) const
	{
		return Vec3<T>(
			rhs.x * e[0] + rhs.y * e[1] + rhs.z * e[2], 
			rhs.x * e[3] + rhs.y * e[4] + rhs.z * e[5],
			rhs.x * e[6] + rhs.y * e[7] + rhs.z * e[8]
		);
	}


	T e[9]; // In row-major order.
};


///
/// An axis/angle class.
/// Specifies rotation as an axis/angle pair.
///
class AxisAngle
{
public:
	USE_INDIGO_SDK_ALLOCATOR

	/// Does not initialise with identity, use AxisAngle::identity() instead.
	AxisAngle() { }
	AxisAngle(const Vec3d& v_, double w_) : axis(v_), angle(w_) { }

	/// Returns the identity.
	static inline const AxisAngle identity() { return AxisAngle(Vec3d(0.0, 0.0, 1.0), 0.0); }

	void writeXML(OStream& stream, unsigned int depth) const;

	inline bool operator == (const AxisAngle& b) const { return axis == b.axis && angle == b.angle; }


	Vec3d axis;
	double angle;
};


///
/// A single keyframe for the object to world transformation for a single object.
/// Object to world transformation:
/// vector_ws = translate(rotate(scale(vector_os)))
///
class KeyFrame
{
public:
	USE_INDIGO_SDK_ALLOCATOR

	/// Initialises with the identity transformation.
	KeyFrame() : offset(0, 0, 0), time(0), rotation_inverse(AxisAngle::identity()) { }

	/// Initialise with the given time, offset and inverse rotation.
	KeyFrame(double time_, const Vec3d& offset_, const AxisAngle& inverse_rotation)
		: offset(offset_), time(time_), rotation_inverse(inverse_rotation) { }

	/// Serialise to stream.
	void writeXML(OStream& stream, unsigned int depth) const;

	inline bool operator == (const KeyFrame& b) const { return offset == b.offset && time == b.time && rotation_inverse == b.rotation_inverse && S == b.S; }
	inline bool operator != (const KeyFrame& b) const { return !(*this == b); }

	/// Translation vector for object to world transformation.
	Vec3d offset; 

	/// Time of the keyframe.  Exposure start is t=0.  Exposure end is t=1.
	double time;

	/// Clockwise rotation for object to world transformation.  Called inverse as it rotates clockwise instead of the usual counter-clockwise.
	AxisAngle rotation_inverse; 

	/// Object to world scale and shear matrix.  Can have rotation information as well if Indigo is required to do the rotation/scale+shear decomposition.
	Matrix3<double> S; 
};


void serialiseVec2d(OStream& stream, unsigned int depth, const char* name, const Vec2d& val);
void serialiseVec3d(OStream& stream, unsigned int depth, const char* name, const Vec3d& val);
void serialiseMatrix2(OStream& stream, unsigned int depth, const char* name, const Matrix2& val);
void serialiseMatrix3d(OStream& stream, unsigned int depth, const char* name, const Matrix3<double>& val);


} // end namespace Indigo
