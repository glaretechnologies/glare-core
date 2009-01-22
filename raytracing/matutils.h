/*=====================================================================
matutils.h
----------
File created by ClassTemplate on Tue May 04 18:40:22 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __MATUTILS_H_666_
#define __MATUTILS_H_666_


#include "../maths/vec2.h"
#include "../maths/vec3.h"
#include "../maths/basis.h"
#include "../indigo/FullHitInfo.h"
#include "../indigo/SampleTypes.h"
class MTwister;
class SpectralVector;


/*=====================================================================
MatUtils
--------

=====================================================================*/
namespace MatUtils
{

	template <class Real> inline Real solidAngleToAreaPDF(Real solid_angle_pdf, Real dist2, Real costheta);
	template <class Real> inline Real areaToSolidAnglePDF(Real area_pdf, Real dist2, Real costheta);

	template <class Real> const Vec3<Real> sphericalToCartesianCoords(Real phi, Real cos_theta, const Basis<Real>& basis);


	template <class Real> const Vec3<Real> reflectInSurface(const Vec3<Real>& surface_normal, const Vec3<Real>& in);

	template <class Real> void refractInSurface(const Vec3<Real>& surface_normal,
		const Vec3<Real>& incident_raydir, Real src_refindex, Real dest_refindex,
		Vec3<Real>& exit_raydir_out, bool& totally_internally_reflected_out);

	//static double getFresnelReflForCosAngle(double cos_angle_incidence,
	//			double fresnel_fraction);

	/*static double getReflForCosAngleSchlick(double cos_angle_incidence,
				double normal_reflectance);

	static void getReflForCosAngleSchlick(double cos_angle_incidence,
				const Colour3d& normal_reflectance, Colour3d& reflectance_out);*/

	//static double schlickFresnelFraction(double cos_angle_incidence);

	template <class Real> inline Real schlickFresnelReflectance(Real R_0, Real cos_theta);



	template <class Real> const Vec2<Real> sampleUnitDisc(const SamplePair& unitsamples);

	//template <class Real> static const Vec3<Real> sampleHemisphere(const Basis<Real>& basis, const SamplePair& unitsamples, Real& pdf_out);
	//template <class Real> static inline Real hemispherePDF();

	//static const Vec3d sampleSphere(const Vec2d& unitsamples, const Vec3d& normal, double& pdf_out);
	template <class Real> inline const Vec3<Real> uniformlySampleSphere(const SamplePair& unitsamples); // returns point on surface of sphere with radius 1
	template <class Real> inline Real spherePDF();


	template <class Real> const Vec3<Real> sampleHemisphereCosineWeighted(const Basis<Real>& basis, const SamplePair& unitsamples);
	template <class Real> inline Real hemisphereCosineWeightedPDF(const Vec3<Real>& normal, const Vec3<Real>& unitdir);



	// Samples a bivariate Gaussian distribution, with mean (0,0) and given standard_deviation
	const Vec2d boxMullerGaussian(double standard_deviation, MTwister& rng);

	double evalNormalDist(double x, double mean, double standard_dev);

	//k() of the basis should point towards center of cone.
	template <class Real> const Vec3<Real> sampleSolidAngleCone(const SamplePair& unitsamples, const Basis<Real>& basis, Real angle);
	template <class Real> Real solidAngleConePDF(Real angle);

	void conductorFresnelReflectance(const SpectralVector& n, const SpectralVector& k, float cos_incident_angle, SpectralVector& F_R_out);
	template <class Real> Real conductorFresnelReflectance(Real n, Real k, Real cos_incident_angle);
	template <class Real> const Vec2<Real> polarisedConductorFresnelReflectance(Real n, Real k, Real cos_theta);

	// Returns (F_perp, F_par)
	template <class Real> const Vec2<Real> polarisedConductorFresnelReflectanceExact(Real n, Real k, Real cos_theta);

	template <class Real> Real dielectricFresnelReflectance(Real srcn, Real destn, Real incident_cos_theta);
	template <class Real> const Vec2<Real> polarisedDielectricFresnelReflectance(Real srcn, Real destn, Real incident_cos_theta);

	template <class Real> inline Real pow5(Real x);


	template <class Real> inline Real smoothingFactor(Real in_dot_Ns, Real in_dot_Ng);
	inline FullHitInfo::Vec3RealType smoothingFactor(const FullHitInfo::Vec3Type& omega_in, const FullHitInfo& hitinfo);
	//inline static double smoothingFactor(const Vec3d& omega_in, const Vec3d& omega_out, const FullHitInfo& hitinfo, bool adjoint);
	//inline static double smoothingFactor(const Vec3d& omega_in, const Vec3d& omega_out, const FullHitInfo& hitinfo, bool adjoint, double in_dot_Ng, double out_dot_Ng);

	inline bool raysOnOppositeGeometricSides(const FullHitInfo::Vec3Type& a, const FullHitInfo::Vec3Type& b, const FullHitInfo& hitinfo);
	inline bool raysOnOppositeGeometricSides(FullHitInfo::Vec3RealType a_dot_orig_Ng, FullHitInfo::Vec3RealType b_dot_orig_Ng);


	// Dir need not be normalised.
	// Returns (phi, theta)
	template <class Real> inline const Vec2<Real> sphericalCoordsForDir(const Vec3<Real>& dir, Real recip_dir_length);

	// Returns unit length vector
	template <class Real> inline const Vec3<Real> dirForSphericalCoords(Real phi, Real theta);

	// V is a vector away from the surface.
	// n1 will be set to the IOR of the medium that V points into.
	template <class Real> inline void getN1AndN2(Real external_ior, Real internal_ior, Real v_dot_orig_Ng, Real& n1_out, Real& n2_out);

	void unitTest();





template <class Real> Real solidAngleToAreaPDF(Real solid_angle_pdf, Real dist2, Real costheta)
{
	return solid_angle_pdf * costheta / dist2;
}


template <class Real> Real areaToSolidAnglePDF(Real area_pdf, Real dist2, Real costheta)
{
	return area_pdf * dist2 / costheta;
}


template <class Real> Real spherePDF()
{
	return (Real)NICKMATHS_RECIP_4PI;
}


template <class Real> Real hemisphereCosineWeightedPDF(const Vec3<Real>& normal, const Vec3<Real>& unitdir)
{
	return myMax((Real)0.0, dot(normal, unitdir)) * (Real)NICKMATHS_RECIP_PI;
}


template <class Real> Real pow5(Real x)
{
	const Real x2 = x*x;
	return x2*x2*x;
	//return x*x*x*x*x;
}


//see Veach page 158
//double MatUtils::smoothingFactor(const Vec3d& omega_in, const Vec3d& omega_out, const FullHitInfo& hitinfo, bool adjoint)
//{
	/*the w_i.N_g factors in the denominators (extra from Veach) are to cancel with the same factor (cos theta) outside this call.

	K(w_i, w_o) = fs(w_i, w_o) * w_i.N_s			[figure 5.6]
	but K(w_i, w_o) = f-s(w_i, w_o) * w_i.N_g		[defn of K, 5.20]
	therfore
	f-s(w_i, w_o) * w_i.N_g = fs(w_i, w_o) * w_i.N_s
	or
	f-s(w_i, w_o) = fs(w_i, w_o) * w_i.N_s / w_i.N_g

	for Adjoint:

	K*(w_i, w_o) = fs(w_o, w_i) * w_o.N_s * w_i.N_g / w_o.N_g
	but K*(w_i, w_o) = f-s*(w_i, w_o) * w_i.N_g					[defn of K*, 5.21]
	therefore
	f-s*(w_i, w_o) * w_i.N_g = fs(w_o, w_i) * w_o.N_s * w_i.N_g / w_o.N_g
	or
	f-s*(w_i, w_o) = fs(w_o, w_i) * w_o.N_s / w_o.N_g
	*/

/*	double geom_factor;
	if(adjoint)
		geom_factor = fabs(dot(omega_out, hitinfo.N_s()) / dot(omega_out, hitinfo.N_g()));
	else
		geom_factor = fabs(dot(omega_in, hitinfo.N_s()) / dot(omega_in, hitinfo.N_g()));
	return geom_factor;*/
/*	return adjoint ?
		fabs(dot(omega_out, hitinfo.N_s()) / dot(omega_out, hitinfo.N_g())) :
		fabs(dot(omega_in, hitinfo.N_s()) / dot(omega_in, hitinfo.N_g()));
}*/

/*double MatUtils::smoothingFactor(const Vec3d& omega_in, const Vec3d& omega_out, const FullHitInfo& hitinfo, bool adjoint, double in_dot_Ng, double out_dot_Ng)
{
	return adjoint ?
		fabs(dot(omega_out, hitinfo.N_s()) / out_dot_Ng) :
		fabs(dot(omega_in, hitinfo.N_s()) / in_dot_Ng);
}*/


const double MAX_SMOOTHING_FACTOR = 2.0;


template <class Real>
Real smoothingFactor(Real in_dot_Ns, Real in_dot_Ng)
{
	return myMin(
		(Real)MAX_SMOOTHING_FACTOR,
		(Real)fabs(in_dot_Ns / in_dot_Ng)
		);
}


// NOTE: This is rather slow :)
FullHitInfo::Vec3RealType smoothingFactor(const FullHitInfo::Vec3Type& omega_in, const FullHitInfo& hitinfo)
{
	return myMin(
		(FullHitInfo::Vec3RealType)MAX_SMOOTHING_FACTOR,
		(FullHitInfo::Vec3RealType)fabs((FullHitInfo::Vec3RealType)(dot<FullHitInfo::Vec3RealType>(omega_in, hitinfo.N_s()) / dot<FullHitInfo::Vec3RealType>(omega_in, hitinfo.N_g())))
		);
}


bool raysOnOppositeGeometricSides(const FullHitInfo::Vec3Type& a, const FullHitInfo::Vec3Type& b, const FullHitInfo& hitinfo)
{
	return dot(a, hitinfo.original_N_g()) * dot(b, hitinfo.original_N_g()) < 0.0;
}


bool raysOnOppositeGeometricSides(FullHitInfo::Vec3RealType a_dot_orig_Ng, FullHitInfo::Vec3RealType b_dot_orig_Ng)
{
	return a_dot_orig_Ng * b_dot_orig_Ng < 0.0;
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
	const Real cos_theta = cos(theta);
	const Real sin_theta = sqrt((Real)1.0 - cos_theta * cos_theta);

	return Vec3<Real>(
		cos(phi) * sin_theta,
		sin(phi) * sin_theta,
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


/*
V is a vector away from the surface.
n1 will be set to the IOR of the medium that V points into.
*/
template <class Real>
void getN1AndN2(Real external_ior, Real internal_ior, Real v_dot_orig_Ng, Real& n1_out, Real& n2_out)
{
	if(v_dot_orig_Ng >= 0.0)
	{
		// V points into the external medium
		n1_out = external_ior;
		n2_out = internal_ior;
	}
	else
	{
		// V points into the internal medium
		n1_out = internal_ior;
		n2_out = external_ior;
	}
}


template <class Real> const Vec3<Real> reflectInSurface(const Vec3<Real>& surface_normal, const Vec3<Real>& in)
{
	return in + surface_normal * dot(surface_normal, in) * (Real)-2.0;
}


template <class Real> const Vec3<Real> sphericalToCartesianCoords(Real phi, Real cos_theta, const Basis<Real>& basis)
{
	assert(cos_theta >= -1.0 && cos_theta <= 1.0);

	const Real sin_theta = sqrt((Real)1.0 - cos_theta*cos_theta);

	assert(Vec3<Real>(cos(phi)* sin_theta, sin(phi) * sin_theta, cos_theta).isUnitLength());
	return basis.transformVectorToParent(Vec3<Real>(cos(phi)* sin_theta, sin(phi) * sin_theta, cos_theta));
}


template <class Real> const Vec3<Real> uniformlySampleSphere(const SamplePair& unitsamples) // returns point on surface of sphere with radius 1
{
	const Real z = (Real)-1.0 + unitsamples.x * (Real)2.0;
	const Real theta = unitsamples.y * (Real)NICKMATHS_2PI;

	const Real r = sqrt((Real)1.0 - z*z);

	return Vec3<Real>(cos(theta) * r, sin(theta) * r, z);
}


//See Monte Carlo Ray Tracing siggraph course 2003 page 33.
template <class Real> const Vec3<Real> sampleSolidAngleCone(const SamplePair& samples, const Basis<Real>& basis, Real angle)
{
	assert(angle > 0.0);

	const Real phi = samples.x * NICKMATHS_2PI;
	const Real alpha = sqrt(samples.y) * angle;

	//const float r = sqrt(unitsamples.x);
	//const float phi = unitsamples.y * NICKMATHS_2PI;

	//Vec3d dir(disc.x*sin(alpha), disc.y*sin(alpha), cos(alpha));
	const Vec3<Real> dir(cos(phi)*sin(alpha), sin(phi)*sin(alpha), cos(alpha));

	return basis.transformVectorToParent(dir);
}


template <class Real> Real solidAngleConePDF(Real angle)
{
	assert(angle > 0.0);

	// The sampling is uniform over the solid angle subtended by the cone.
	const Real solid_angle = NICKMATHS_2PI * (1.0 - cos(angle));
	return (Real)1.0 / solid_angle;
}


template <class Real> Real schlickFresnelReflectance(Real R_0, Real cos_theta)
{
	assert(Maths::inUnitInterval(cos_theta));

	return R_0 + ((Real)1.0 - R_0) * MatUtils::pow5<Real>((Real)1.0 - cos_theta);
}


} // End namespace MatUtils


#endif //__MATUTILS_H_666_
