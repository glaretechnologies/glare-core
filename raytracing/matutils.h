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


	//static void getRandomSphereUnitVec(Vec3d& vec_out);

	//static void getRandomHemisUnitVec(const Vec3d& surface_normal, int const_comp_index,
	//			int otherindex_1, int otherindex_2, Vec3d& vec_out);

	static inline double solidAngleToAreaPDF(double solid_angle_pdf, double dist2, double costheta);
	static inline double areaToSolidAnglePDF(double area_pdf, double dist2, double costheta);


	static const Vec3d sphericalToCartesianCoords(double phi, double cos_theta, const Basisd& basis);



	static void getRandomHemisUnitVec(const Vec3d& surface_normal, Vec3d& vec_out);

	static void getCosineWeightedHemisUnitVec(const Vec3d& surface_normal, Vec3d& vec_out);

	static void reflectInSurface(const Vec3d& surface_normal, Vec3d& vec_out);

	static void refractInSurface(const Vec3d& surface_normal, 
		const Vec3d& incident_raydir, double src_refindex, double dest_refindex,
		Vec3d& exit_raydir_out, bool& totally_internally_reflected_out);

	static double getFresnelReflForCosAngle(double cos_angle_incidence,
				double fresnel_fraction);

	/*static double getReflForCosAngleSchlick(double cos_angle_incidence,
				double normal_reflectance);

	static void getReflForCosAngleSchlick(double cos_angle_incidence,
				const Colour3d& normal_reflectance, Colour3d& reflectance_out);*/

	//static double schlickFresnelFraction(double cos_angle_incidence);

	static double schlickFresnelReflectance(double R_0, double cos_theta);



	static const Vec2d sampleUnitDisc(const Vec2d& unitsamples);
	
	static const Vec3d sampleHemisphere(const Basisd& basis, const Vec2d& unitsamples, double& pdf_out);
	static inline double hemispherePDF();
	
	//static const Vec3d sampleSphere(const Vec2d& unitsamples, const Vec3d& normal, double& pdf_out);
	static const Vec3d uniformlySampleSphere(const Vec2d& unitsamples); // returns point on surface of sphere with radius 1
	static inline double spherePDF();
	
	static const Vec3d sampleHemisphereCosineWeighted(const Basisd& basis, const Vec2d& unitsamples/*, double& pdf_out*/);
	//static const Vec3d sampleHemisphereCosineWeighted(const Vec2d& unitsamples, const Vec3d& normal, double& pdf_out);
	static inline double hemisphereCosineWeightedPDF(const Vec3d& normal, const Vec3d& unitdir);



	//samples a bivariate Gaussian distribution, with mean (0,0) and given standard_deviation
	static const Vec2d boxMullerGaussian(double standard_deviation, MTwister& rng);

	static double evalNormalDist(double x, double mean, double standard_dev);

	//k() of the basis should point towards center of cone.
	static const Vec3d sampleSolidAngleCone(const Vec2d& unitsamples, const Basisd& basis, double angle);
	static double solidAngleConePDF(double angle);

	static void conductorFresnelReflectance(const SpectralVector& n, const SpectralVector& k, double cos_incident_angle, SpectralVector& F_R_out);
	static double conductorFresnelReflectance(double n, double k, double cos_incident_angle);
	static const Vec2d polarisedConductorFresnelReflectance(double n, double k, double cos_theta);


	static double dialetricFresnelReflectance(double srcn, double destn, double incident_cos_theta);
	static const Vec2d polarisedDialetricFresnelReflectance(double srcn, double destn, double incident_cos_theta);

	inline static double pow5(double x);


	inline static double smoothingFactor(double in_dot_Ns, double in_dot_Ng);
	inline static double smoothingFactor(const Vec3d& omega_in, const FullHitInfo& hitinfo);
	//inline static double smoothingFactor(const Vec3d& omega_in, const Vec3d& omega_out, const FullHitInfo& hitinfo, bool adjoint);
	//inline static double smoothingFactor(const Vec3d& omega_in, const Vec3d& omega_out, const FullHitInfo& hitinfo, bool adjoint, double in_dot_Ng, double out_dot_Ng);

	inline static bool raysOnOppositeGeometricSides(const Vec3d& a, const Vec3d& b, const FullHitInfo& hitinfo);
	inline static bool raysOnOppositeGeometricSides(double a_dot_orig_Ng, double b_dot_orig_Ng);


	// Dir need not be normalised.
	// Returns (phi, theta)
	inline static const Vec2d sphericalCoordsForDir(const Vec3d& dir, double recip_dir_length);

	// Returns unit length vector
	inline static const Vec3d dirForSphericalCoords(double phi, double theta);

	/*
	V is a vector away from the surface.
	n1 will be set to the IOR of the medium that V points into.
	*/
	inline static void getN1AndN2(double external_ior, double internal_ior, double v_dot_orig_Ng, double& n1_out, double& n2_out);

	static void unitTest();
};


double MatUtils::solidAngleToAreaPDF(double solid_angle_pdf, double dist2, double costheta)
{
	return solid_angle_pdf * costheta / dist2;
}

double MatUtils::areaToSolidAnglePDF(double area_pdf, double dist2, double costheta)
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

double MatUtils::hemisphereCosineWeightedPDF(const Vec3d& normal, const Vec3d& unitdir)
{
	return myMax(0.0, dot(normal, unitdir)) * NICKMATHS_RECIP_PI;
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

double MatUtils::smoothingFactor(double in_dot_Ns, double in_dot_Ng)
{
	return myMin(
		MAX_SMOOTHING_FACTOR, 
		fabs(in_dot_Ns / in_dot_Ng)
		);
}

// NOTE: This is rather slow :)
double MatUtils::smoothingFactor(const Vec3d& omega_in, const FullHitInfo& hitinfo)
{
	return myMin(
		MAX_SMOOTHING_FACTOR, 
		fabs(dot(omega_in, hitinfo.N_s()) / dot(omega_in, hitinfo.N_g()))
		);
}


bool MatUtils::raysOnOppositeGeometricSides(const Vec3d& a, const Vec3d& b, const FullHitInfo& hitinfo)
{
	return dot(a, hitinfo.original_N_g()) * dot(b, hitinfo.original_N_g()) < 0.0;
}

bool MatUtils::raysOnOppositeGeometricSides(double a_dot_orig_Ng, double b_dot_orig_Ng)
{
	return a_dot_orig_Ng * b_dot_orig_Ng < 0.0;
}


// Returns (phi, theta)
// where phi is the azimuthal coordinate and theta is the zenith coordinate.
const Vec2d MatUtils::sphericalCoordsForDir(const Vec3d& dir, double recip_dir_length)
{
	assert(epsEqual(1.0 / dir.length(), recip_dir_length));

	//NOTE: the clamp is in there to avoid the spitting out of a NaN
	return Vec2d(
		atan2(dir.y, dir.x), // phi
		acos(myClamp(dir.z * recip_dir_length, -1.0, 1.0)) // theta
		);
}


const Vec3d MatUtils::dirForSphericalCoords(double phi, double theta)
{
	// http://mathworld.wolfram.com/SphericalCoordinates.html

	/*const double sin_theta = sin(theta);
	return Vec3d(
		cos(phi) * sin_theta,
		sin(phi) * sin_theta,
		cos(theta)
		);*/

	assert(theta >= 0.0 && theta <= NICKMATHS_PI);
	const double cos_theta = cos(theta);
	const double sin_theta = sqrt(1.0 - cos_theta * cos_theta);

	return Vec3d(
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

#endif //__MATUTILS_H_666_




