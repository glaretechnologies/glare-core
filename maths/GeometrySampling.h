/*=====================================================================
GeometrySampling.h
------------------
File created by ClassTemplate on Fri May 22 13:23:14 2009
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __GEOMETRYSAMPLING_H_666_
#define __GEOMETRYSAMPLING_H_666_


#include "../maths/vec2.h"
#include "../maths/vec3.h"
#include "../maths/Vec4f.h"
#include "../maths/Matrix4f.h"
#include "../indigo/SampleTypes.h"


namespace GeometrySampling
{
	///// Unit Disc ///////////

	template <class Real> const Vec2<Real> sampleUnitDisc(const SamplePair& unit_samples);
	
	template <class Real> inline const Vec2<Real> shirleyUnitSquareToDisk(const Vec2<Real>& unitsamples);
	template <class Real> inline const Vec2<Real> shirleyDiskToUnitSquare(const Vec2<Real>& on_disk);
	template <class Real> inline const Vec3<Real> unitSquareToHemisphere(const Vec2<Real>& unitsamples);
	template <class Real> inline const Vec3<Real> unitSquareToSphere(const Vec2<Real>& unitsamples);
	template <class Real> inline const Vec2<Real> hemisphereToUnitSquare(const Vec3<Real>& on_unit_sphere);
	template <class Real> inline const Vec2<Real> sphereToUnitSquare(const Vec3<Real>& on_unit_sphere);

	///// Sphere  /////

	template <class Real> inline const Vec3<Real> uniformlySampleSphere(const SamplePair& unit_samples); // Returns point on surface of sphere with radius 1
	inline const Vec4f uniformlySampleSphere(const Vec2f& unitsamples); // Returns point on surface of sphere with radius 1
	template <class Real> inline Real spherePDF();


	///// Hemisphere  /////

	template <class VecType> const VecType sampleHemisphereCosineWeighted(const Matrix4f& to_world, const SamplePair& unitsamples);
	template <class VecType> inline typename VecType::RealType hemisphereCosineWeightedPDF(const VecType& normal, const VecType& unitdir);


	////// Cone /////////

	// k() of the basis should point towards center of cone.
	inline const Vec4f sampleSolidAngleCone(const SamplePair& samples, const Matrix4f& basis, float angle);
	template <class Real> inline Real solidAngleConePDF(Real angle);



	////// Measure conversion /////////

	template <class Real> inline Real solidAngleToAreaPDF(Real solid_angle_pdf, Real dist2, Real costheta);
	template <class Real> inline Real areaToSolidAnglePDF(Real area_pdf, Real dist2, Real costheta);


	////// Cartesian -> Spherical polar angle conversion //////////

	// Dir need not be normalised.
	// Returns (phi, theta)
	template <class Real> inline const Vec2<Real> sphericalCoordsForDir(const Vec3<Real>& dir, Real recip_dir_length);
	inline const Vec2f sphericalCoordsForDir(const Vec4f& dir, float recip_dir_length);
	inline void sphericalCoordsForDir(const Vec4f& dir, float recip_dir_length, float& theta_out, float& phi_out);
	template <class Real> inline void sphericalCoordsForDir(const Vec3<Real>& dir, Real recip_dir_length, Real& theta_out, Real& phi_out);


	////// Spherical polar angle -> Cartesian coordinates conversion //////////

	inline const Vec4f sphericalToCartesianCoords(float phi, float cos_theta, const Matrix4f& basis);
	template <class Real> inline const Vec3<Real> dirForSphericalCoords(Real phi, Real theta); // Returns unit length vector
	inline const Vec4f dirForSphericalCoords(float phi, float theta);

	void doTests();


template <class Real> Real solidAngleToAreaPDF(Real solid_angle_pdf, Real dist2, Real costheta)
{
	return solid_angle_pdf * costheta / dist2;
}


template <class Real> Real areaToSolidAnglePDF(Real area_pdf, Real dist2, Real costheta)
{
	return area_pdf * dist2 / costheta;
}


template <class VecType> typename VecType::RealType hemisphereCosineWeightedPDF(const VecType& normal, const VecType& unitdir)
{
	return myMax(/*(VecType::RealType)*/0.0f, dot(normal, unitdir)) * /*(VecType::RealType)*/(float)NICKMATHS_RECIP_PI;
}

inline const Vec4f dirForSphericalCoords(float phi, float theta)
{
	//assert(theta >= 0.0 && theta <= NICKMATHS_PI);
	const float cos_theta = std::cos(theta);
	const float sin_theta = std::sqrt(1.0f - cos_theta * cos_theta);

	return Vec4f(
		std::cos(phi) * sin_theta,
		std::sin(phi) * sin_theta,
		cos_theta,
		0.0f
		);
}
template <class Real> const Vec3<Real> dirForSphericalCoords(Real phi, Real theta)
{
	// http://mathworld.wolfram.com/SphericalCoordinates.html

	/*const double sin_theta = sin(theta);
	return Vec3d(
		cos(phi) * sin_theta,
		sin(phi) * sin_theta,
		cos(theta)
		);*/

	assert(theta >= 0.0 && theta <= NICKMATHS_PI);
	const Real cos_theta = std::cos(theta);
	const Real sin_theta = std::sqrt((Real)1.0 - cos_theta * cos_theta);

	return Vec3<Real>(
		std::cos(phi) * sin_theta,
		std::sin(phi) * sin_theta,
		cos_theta
		);

	// Use sqrt() instead of trig functions here.
	// cos() and sin() take about 109 cycles on a Core2,
	// sqrt() takes more like 57

	// cos(theta)^2 + sin(theta)^2 = 1 => cos(theta) = +-sqrt(1 - sin(theta)^2)

	/*const double sin_theta = sin(theta);
	const double sin_phi = sin(phi);

	return Vec3d(
		sqrt(1.0 - sin_phi*sin_phi) * sin_theta, // x = cos(phi) * sin(theta)
		sin_phi * sin_theta,
		sqrt(1.0 - sin_theta*sin_theta) // z = cos(theta)
		);*/
}
	
// Returns (phi, theta)
// where phi is the azimuthal coordinate and theta is the zenith coordinate.
template <class Real> const Vec2<Real> sphericalCoordsForDir(const Vec3<Real>& dir, Real recip_dir_length)
{
	//const Real recip_len = (Real)1.0 / dir.length();
	//assert(Maths::approxEq((Real)1.0 / dir.length(), recip_dir_length, (Real)0.001));

	//NOTE: the clamp is in there to avoid the spitting out of a NaN
	return Vec2<Real>(
		atan2(dir.y, dir.x), // phi
		acos(myClamp(dir.z * recip_dir_length, (Real)-1.0, (Real)1.0)) // theta
		);
}


const Vec2f sphericalCoordsForDir(const Vec4f& dir, float recip_dir_length)
{
	//const Real recip_len = (Real)1.0 / dir.length();
	//assert(Maths::approxEq((Real)1.0 / dir.length(), recip_dir_length, (Real)0.001));

	//NOTE: the clamp is in there to avoid the spitting out of a NaN
	return Vec2f(
		atan2(dir[1], dir[0]), // phi
		acos(myClamp(dir[2] * recip_dir_length, -1.0f, 1.0f)) // theta
		);
}


void sphericalCoordsForDir(const Vec4f& dir, float recip_dir_length, float& theta_out, float& phi_out)
{
	//NOTE: the clamp is in there to avoid the spitting out of a NaN
	phi_out = std::atan2(dir.x[1], dir.x[0]);
	theta_out = std::acos(myClamp(dir.x[2] * recip_dir_length, -1.0f, 1.0f));
}


template <class Real>
void sphericalCoordsForDir(const Vec3<Real>& dir, Real recip_dir_length, Real& theta_out, Real& phi_out)
{
	//NOTE: the clamp is in there to avoid the spitting out of a NaN
	phi_out = std::atan2(dir.y, dir.x);
	theta_out = std::acos(myClamp(dir.z * recip_dir_length, (Real)-1.0, (Real)1.0));
}


const Vec4f sphericalToCartesianCoords(float phi, float cos_theta, const Matrix4f& basis)
{
	assert(cos_theta >= -1.0 && cos_theta <= 1.0);

	const float sin_theta = std::sqrt(1.0f - cos_theta*cos_theta);

	assert(Vec3<float>(cos(phi)* sin_theta, sin(phi) * sin_theta, cos_theta).isUnitLength());

	return basis * Vec4f(cos(phi)* sin_theta, sin(phi) * sin_theta, cos_theta, 0.0f);
}


template <class Real> const Vec3<Real> uniformlySampleSphere(const SamplePair& unitsamples) // returns point on surface of sphere with radius 1
{
	const Real z = (Real)-1.0 + unitsamples.x * (Real)2.0;
	const Real theta = unitsamples.y * (Real)NICKMATHS_2PI;

	const Real r = sqrt((Real)1.0 - z*z);

	return Vec3<Real>(cos(theta) * r, sin(theta) * r, z);
}


const Vec4f uniformlySampleSphere(const SamplePair& unitsamples) // returns point on surface of sphere with radius 1
{
	const float z = (float)-1.0 + unitsamples.x * (float)2.0;
	const float theta = unitsamples.y * (float)NICKMATHS_2PI;

	const float r = sqrt((float)1.0 - z*z);

	return Vec4f(cos(theta) * r, sin(theta) * r, z, 1.0f);
}


// See Monte Carlo Ray Tracing siggraph course 2003 page 33.
const Vec4f sampleSolidAngleCone(const SamplePair& samples, const Matrix4f& basis, float angle)
{
	assert(angle > 0.0);

	const float phi = samples.x * (float)NICKMATHS_2PI;
	const float alpha = std::sqrt(samples.y) * angle;

	//const float r = sqrt(unitsamples.x);
	//const float phi = unitsamples.y * NICKMATHS_2PI;

	//Vec3d dir(disc.x*sin(alpha), disc.y*sin(alpha), cos(alpha));

	const float sin_alpha = std::sin(alpha);
	const SSE_ALIGN Vec4f dir(std::cos(phi)*sin_alpha, std::sin(phi)*sin_alpha, std::cos(alpha), 0.0f);

	return Vec4f(basis * dir); //basis.transformVectorToParent(dir);
}


template <class Real> Real solidAngleConePDF(Real angle)
{
	assert(angle > 0.0);

	// The sampling is uniform over the solid angle subtended by the cone.
	const Real solid_angle = NICKMATHS_2PI * (1.0 - std::cos(angle));
	return (Real)1.0 / solid_angle;
}



/*-----------------------------------------------------------------------------------------------
A Low Distortion Map Between Disk and Square

http://jgt.akpeters.com/papers/ShirleyChiu97/

from http://jgt.akpeters.com/papers/ShirleyChiu97/code.html

 This transforms points on [0,1]^2 to points on unit disk centered at
 origin.  Each "pie-slice" quadrant of square is handled as a seperate
 case.  The bad floating point cases are all handled appropriately.
 The regions for (a,b) are:

        phi = pi/2
       -----*-----
       |\       /|
       |  \ 2 /  |
       |   \ /   |
 phi=pi* 3  *  1 *phi = 0
       |   / \   |
       |  / 4 \  |
       |/       \|
       -----*-----
        phi = 3pi/2

change log:
    26 feb 2004.  bug fix in computation of phi (now this matches the paper,
                  which is correct).  thanks to Atilim Cetin for catching this.

*/

template <class Real>
inline const Vec2<Real> shirleyUnitSquareToDisk(const Vec2<Real>& unitsamples)
{
	const Real a = (Real)2.0*unitsamples.x - (Real)1.0;   /* (a,b) is now on [-1,1]^2 */
	const Real b = (Real)2.0*unitsamples.y - (Real)1.0;

	Real phi, r;

	if (a > -b) {     // region 1 or 2
		if (a > b) {  // region 1, also |a| > |b|
			r = a;
			phi = (Real)NICKMATHS_PI_4 * (b/a);
		}
		else {  // region 2, also |b| > |a|
			r = b;
			phi = (Real)NICKMATHS_PI_4 * ((Real)2.0 - (a/b));
		}
	}
	else {        // region 3 or 4
		if (a < b) {  // region 3, also |a| >= |b|, a != 0
			r = -a;
			phi = (Real)NICKMATHS_PI_4 * ((Real)4.0 + (b/a));
		}
		else {  // region 4, |b| >= |a|, but a==0 and b==0 could occur.
			r = -b;
			if (b != 0)
				phi = (Real)NICKMATHS_PI_4 * ((Real)6.0 - (a/b));
			else
				phi = 0;
		}
	}

	assert(r >= 0.0 && r <= 1.0);
	//spheresamples_out.x = r * cos(phi);
	//spheresamples_out.y = r * sin(phi);
	return Vec2<Real>(r * std::cos(phi), r * std::sin(phi));

	//assert(spheresamples_out.x >= -1.f && spheresamples_out.x <= 1.f);
	//assert(spheresamples_out.y >= -1.f && spheresamples_out.y <= 1.f);
}


template <class Real>
inline const Vec2<Real> shirleyDiskToUnitSquare(const Vec2<Real>& on_disk)
{
	const Real r = on_disk.length();

	Real phi = std::atan2(on_disk.y, on_disk.x);

	if(phi < -NICKMATHS_PI_4)
		phi += (Real)NICKMATHS_2PI; // in range [-pi/4, 7pi/4]

	Real a, b;// x, y;
	if(phi < NICKMATHS_PI_4)
	{
		a = r;
		b = phi * r * (Real)(4.0 * NICKMATHS_RECIP_PI); // b = phi * a / (pi/4) = phi * a * (4/pi)
	}
	else if(phi < 3.0 * NICKMATHS_PI / 4.0)
	{
		b = r;
		a = -(phi - (Real)NICKMATHS_PI_2) * r * (Real)(4.0 * NICKMATHS_RECIP_PI);
	}
	else if(phi < 5.0 * NICKMATHS_PI / 4.0)
	{
		a = -r;
		b = (phi - (Real)NICKMATHS_PI) * -r * (Real)(4.0 * NICKMATHS_RECIP_PI);
	}
	else
	{
		b = -r;
		a = -(phi - (Real)3.0 * (Real)NICKMATHS_PI_2) * -r * (Real)(4.0 * NICKMATHS_RECIP_PI);
	}

	return Vec2<Real>((a + (Real)1.0) * (Real)0.5, (b + (Real)1.0) * (Real)0.5);
}


template <class Real>
const Vec2<Real> sampleUnitDisc(const SamplePair& unitsamples)
{
	return shirleyUnitSquareToDisk<Real>(Vec2<Real>(unitsamples.x, unitsamples.y));
}


template <class VecType>
const VecType sampleHemisphereCosineWeighted(const Matrix4f& to_world, const SamplePair& unitsamples)
{
	// Sample unit disc
	Vec2<typename VecType::RealType> disc = shirleyUnitSquareToDisk<typename VecType::RealType>(unitsamples);

	const VecType dir(
		disc.x, 
		disc.y, 
		std::sqrt(myMax((typename VecType::RealType)0.0, (typename VecType::RealType)1.0 - (disc.x*disc.x + disc.y*disc.y))), 
		0.0
		);
	assert(dir.isUnitLength());

	return VecType(to_world * dir);
}


template <class Real> const Vec3<Real> unitSquareToHemisphere(const Vec2<Real>& unitsamples)
{
	const Vec2<Real> on_disk = shirleyUnitSquareToDisk(unitsamples);

	const Real r2 = on_disk.length2();

	const Real r_dash = std::sqrt((Real)2.0 - r2);

	return Vec3<Real>(on_disk.x * r_dash, on_disk.y * r_dash, (Real)1.0 - r2);
}


template <class Real> const Vec3<Real> unitSquareToSphere(const Vec2<Real>& u)
{
	// We will divide the unit square into two wide rectangles.  The bottom rectangle gets mapped to the z > 0 hemisphere.
	if(u.y < (Real)0.5)
	{
		return unitSquareToHemisphere(Vec2<Real>(u.x, u.y * (Real)2.0));
	}
	else
	{
		const Vec3<Real> v = unitSquareToHemisphere(Vec2<Real>(u.x, (u.y - (Real)0.5) * (Real)2.0));
		return Vec3<Real>(v.x, v.y, -v.z);
	}
}


template <class Real> const Vec2<Real> hemisphereToUnitSquare(const Vec3<Real>& on_unit_sphere)
{
	const Real r = std::sqrt(
		myMax((Real)0.0, (Real)1.0 - on_unit_sphere.z)
		);

	const Vec2<Real> on_disk(on_unit_sphere.x / std::sqrt((Real)2.0 - r*r), on_unit_sphere.y / std::sqrt((Real)2.0 - r*r));

	assert(on_disk.length() <= 1.0);

	return shirleyDiskToUnitSquare(on_disk);
}


template <class Real> const Vec2<Real> sphereToUnitSquare(const Vec3<Real>& on_unit_sphere)
{
	if(on_unit_sphere.z > 0.0)
	{
		// Top hemisphere, map back to bottom rectangle
		const Vec2<Real> u = hemisphereToUnitSquare(on_unit_sphere);

		return Vec2<Real>(u.x, u.y * (Real)0.5);
	}
	else
	{
		// z < 0 hemisphere
		const Vec2<Real> u = hemisphereToUnitSquare(Vec3<Real>(on_unit_sphere.x, on_unit_sphere.y, -on_unit_sphere.z));

		return Vec2<Real>(u.x, (Real)0.5 + u.y * (Real)0.5);
	}
}


} // End namespace GeometrySampling


#endif //__GEOMETRYSAMPLING_H_666_
