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
//class Vec2d;
//class Colour3;
//class Basisd;
class MTwister;
class SpectralVector;


/*=====================================================================
MatUtils
--------

=====================================================================*/
class MatUtils
{
public:
	MatUtils();

	~MatUtils();

	typedef float IORType;
	typedef float DefaultReal;
	typedef Vec3<DefaultReal> DefaultVec3Type;


	//static void getRandomSphereUnitVec(Vec3d& vec_out);

	//static void getRandomHemisUnitVec(const Vec3d& surface_normal, int const_comp_index,
	//			int otherindex_1, int otherindex_2, Vec3d& vec_out);

	template <class Real> static inline Real solidAngleToAreaPDF(Real solid_angle_pdf, Real dist2, Real costheta);
	template <class Real> static inline Real areaToSolidAnglePDF(Real area_pdf, Real dist2, Real costheta);



	template <class Real> static const Vec3<Real> sphericalToCartesianCoords(Real phi, Real cos_theta, const Basis<Real>& basis);



	static void getRandomHemisUnitVec(const Vec3d& surface_normal, Vec3d& vec_out);

	static void getCosineWeightedHemisUnitVec(const Vec3d& surface_normal, Vec3d& vec_out);

	template <class Real> static void reflectInSurface(const Vec3<Real>& surface_normal, Vec3<Real>& vec_out);

	static void refractInSurface(const DefaultVec3Type& surface_normal, 
		const DefaultVec3Type& incident_raydir, IORType src_refindex, IORType dest_refindex,
		DefaultVec3Type& exit_raydir_out, bool& totally_internally_reflected_out);

	static double getFresnelReflForCosAngle(double cos_angle_incidence,
				double fresnel_fraction);

	/*static double getReflForCosAngleSchlick(double cos_angle_incidence,
				double normal_reflectance);

	static void getReflForCosAngleSchlick(double cos_angle_incidence,
				const Colour3d& normal_reflectance, Colour3d& reflectance_out);*/

	//static double schlickFresnelFraction(double cos_angle_incidence);

	static double schlickFresnelReflectance(double R_0, double cos_theta);



	static const Vec2d sampleUnitDisc(const SamplePair& unitsamples);
	
	static const Vec3d sampleHemisphere(const Basisd& basis, const Vec2d& unitsamples, double& pdf_out);
	static inline double hemispherePDF();
	
	//static const Vec3d sampleSphere(const Vec2d& unitsamples, const Vec3d& normal, double& pdf_out);
	template <class Real> static inline const Vec3<Real> uniformlySampleSphere(const SamplePair& unitsamples); // returns point on surface of sphere with radius 1
	static inline double spherePDF();

	
	static const Vec3<DefaultReal> sampleHemisphereCosineWeighted(const Basis<DefaultReal>& basis, const SamplePair& unitsamples/*, double& pdf_out*/);
	//static const Vec3d sampleHemisphereCosineWeighted(const Vec2d& unitsamples, const Vec3d& normal, double& pdf_out);
	template <class Real> static inline Real hemisphereCosineWeightedPDF(const Vec3<Real>& normal, const Vec3<Real>& unitdir);



	//samples a bivariate Gaussian distribution, with mean (0,0) and given standard_deviation
	static const Vec2d boxMullerGaussian(double standard_deviation, MTwister& rng);

	static double evalNormalDist(double x, double mean, double standard_dev);

	//k() of the basis should point towards center of cone.
	template <class Real> static const Vec3<Real> sampleSolidAngleCone(const SamplePair& unitsamples, const Basis<Real>& basis, Real angle);
	template <class Real> static Real solidAngleConePDF(Real angle);

	static void conductorFresnelReflectance(const SpectralVector& n, const SpectralVector& k, double cos_incident_angle, SpectralVector& F_R_out);
	static double conductorFresnelReflectance(double n, double k, double cos_incident_angle);
	static const Vec2d polarisedConductorFresnelReflectance(double n, double k, double cos_theta);


	static double dialetricFresnelReflectance(double srcn, double destn, double incident_cos_theta);
	static const Vec2d polarisedDialetricFresnelReflectance(double srcn, double destn, double incident_cos_theta);

	inline static double pow5(double x);


	template <class Real> inline static Real smoothingFactor(Real in_dot_Ns, Real in_dot_Ng);
	inline static FullHitInfo::Vec3RealType smoothingFactor(const FullHitInfo::Vec3Type& omega_in, const FullHitInfo& hitinfo);
	//inline static double smoothingFactor(const Vec3d& omega_in, const Vec3d& omega_out, const FullHitInfo& hitinfo, bool adjoint);
	//inline static double smoothingFactor(const Vec3d& omega_in, const Vec3d& omega_out, const FullHitInfo& hitinfo, bool adjoint, double in_dot_Ng, double out_dot_Ng);

	inline static bool raysOnOppositeGeometricSides(const FullHitInfo::Vec3Type& a, const FullHitInfo::Vec3Type& b, const FullHitInfo& hitinfo);
	inline static bool raysOnOppositeGeometricSides(FullHitInfo::Vec3RealType a_dot_orig_Ng, FullHitInfo::Vec3RealType b_dot_orig_Ng);


	// Dir need not be normalised.
	// Returns (phi, theta)
	template <class Real> inline static const Vec2<Real> sphericalCoordsForDir(const Vec3<Real>& dir, Real recip_dir_length);

	// Returns unit length vector
	template <class Real> inline static const Vec3<Real> dirForSphericalCoords(Real phi, Real theta);

	/*
	V is a vector away from the surface.
	n1 will be set to the IOR of the medium that V points into.
	*/
	inline static void getN1AndN2(double external_ior, double internal_ior, double v_dot_orig_Ng, double& n1_out, double& n2_out);

	static void unitTest();
};


template <class Real> Real MatUtils::solidAngleToAreaPDF(Real solid_angle_pdf, Real dist2, Real costheta)
{
	return solid_angle_pdf * costheta / dist2;
}

template <class Real> Real MatUtils::areaToSolidAnglePDF(Real area_pdf, Real dist2, Real costheta)
{
	return area_pdf * dist2 / costheta;
}

double MatUtils::hemispherePDF()
{
	return NICKMATHS_RECIP_2PI;
}

double MatUtils::spherePDF()
{
	return NICKMATHS_RECIP_4PI;
}

template <class Real> Real MatUtils::hemisphereCosineWeightedPDF(const Vec3<Real>& normal, const Vec3<Real>& unitdir)
{
	return myMax((Real)0.0, dot(normal, unitdir)) * (Real)NICKMATHS_RECIP_PI;
}

double MatUtils::pow5(double x)
{
	const double x2 = x*x;
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
Real MatUtils::smoothingFactor(Real in_dot_Ns, Real in_dot_Ng)
{
	return myMin(
		(Real)MAX_SMOOTHING_FACTOR, 
		(Real)fabs(in_dot_Ns / in_dot_Ng)
		);
}

// NOTE: This is rather slow :)
FullHitInfo::Vec3RealType MatUtils::smoothingFactor(const FullHitInfo::Vec3Type& omega_in, const FullHitInfo& hitinfo)
{
	return myMin(
		(FullHitInfo::Vec3RealType)MAX_SMOOTHING_FACTOR, 
		(FullHitInfo::Vec3RealType)fabs((FullHitInfo::Vec3RealType)(dot<FullHitInfo::Vec3RealType>(omega_in, hitinfo.N_s()) / dot<FullHitInfo::Vec3RealType>(omega_in, hitinfo.N_g())))
		);
}


bool MatUtils::raysOnOppositeGeometricSides(const FullHitInfo::Vec3Type& a, const FullHitInfo::Vec3Type& b, const FullHitInfo& hitinfo)
{
	return dot(a, hitinfo.original_N_g()) * dot(b, hitinfo.original_N_g()) < 0.0;
}

bool MatUtils::raysOnOppositeGeometricSides(FullHitInfo::Vec3RealType a_dot_orig_Ng, FullHitInfo::Vec3RealType b_dot_orig_Ng)
{
	return a_dot_orig_Ng * b_dot_orig_Ng < 0.0;
}


// Returns (phi, theta)
// where phi is the azimuthal coordinate and theta is the zenith coordinate.
template <class Real> const Vec2<Real> MatUtils::sphericalCoordsForDir(const Vec3<Real>& dir, Real recip_dir_length)
{
	const Real recip_len = (Real)1.0 / dir.length();
	assert(Maths::approxEq((Real)1.0 / dir.length(), recip_dir_length, (Real)0.001));

	//NOTE: the clamp is in there to avoid the spitting out of a NaN
	return Vec2<Real>(
		atan2(dir.y, dir.x), // phi
		acos(myClamp(dir.z * recip_dir_length, (Real)-1.0, (Real)1.0)) // theta
		);
}


template <class Real> const Vec3<Real> MatUtils::dirForSphericalCoords(Real phi, Real theta)
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
	const Real sin_theta = sqrt(1.0 - cos_theta * cos_theta);

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
void MatUtils::getN1AndN2(double external_ior, double internal_ior, double v_dot_orig_Ng, double& n1_out, double& n2_out)
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


template <class Real> void MatUtils::reflectInSurface(const Vec3<Real>& surface_normal, Vec3<Real>& vec_out)
{
//	assert(vec_out.isUnitLength());
//	assert(surface_normal.isUnitLength());

	vec_out.addMult(surface_normal, dot(surface_normal, vec_out) * (Real)-2.0);

//	assert(vec_out.dot(surface_normal) >= 0);
//	assert(vec_out.isUnitLength());

}


template <class Real> const Vec3<Real> MatUtils::sphericalToCartesianCoords(Real phi, Real cos_theta, const Basis<Real>& basis)
{
	assert(cos_theta >= -1.0 && cos_theta <= 1.0);

	const Real sin_theta = sqrt((Real)1.0 - cos_theta*cos_theta);

	assert(Vec3<Real>(cos(phi)* sin_theta, sin(phi) * sin_theta, cos_theta).isUnitLength());
	return basis.transformVectorToParent(Vec3<Real>(cos(phi)* sin_theta, sin(phi) * sin_theta, cos_theta));
}


template <class Real> const Vec3<Real> MatUtils::uniformlySampleSphere(const SamplePair& unitsamples) // returns point on surface of sphere with radius 1
{
	const Real z = (Real)-1.0 + unitsamples.x * (Real)2.0;
	const Real theta = unitsamples.y * (Real)NICKMATHS_2PI;

	const Real r = sqrt((Real)1.0 - z*z);

	return Vec3<Real>(cos(theta) * r, sin(theta) * r, z);
}


//See Monte Carlo Ray Tracing siggraph course 2003 page 33.
template <class Real> const Vec3<Real> MatUtils::sampleSolidAngleCone(const SamplePair& samples, const Basis<Real>& basis, Real angle)
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


template <class Real> Real MatUtils::solidAngleConePDF(Real angle)
{
	assert(angle > 0.0);

	// The sampling is uniform over the solid angle subtended by the cone.
	const Real solid_angle = NICKMATHS_2PI * (1.0 - cos(angle));
	return (Real)1.0 / solid_angle;
}


#endif //__MATUTILS_H_666_




