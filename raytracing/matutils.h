/*=====================================================================
matutils.h
----------
File created by ClassTemplate on Tue May 04 18:40:22 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __MATUTILS_H_666_
#define __MATUTILS_H_666_


#include "../indigo/material.h"
#include "../indigo/FullHitInfo.h"
#include "../indigo/SampleTypes.h"
#include "../maths/vec2.h"
#include "../maths/vec3.h"
class MTwister;
class SpectralVector;
class ThreadContext;



/*=====================================================================
MatUtils
--------

=====================================================================*/
namespace MatUtils
{


	//template <class Real> const Vec3<Real> sphericalToCartesianCoords(Real phi, Real cos_theta, const Basis<Real>& basis);
	//inline const Vec4f sphericalToCartesianCoords(float phi, float cos_theta, const Matrix4f& basis);

	template <class Real> const Vec3<Real> reflectInSurface(const Vec3<Real>& surface_normal, const Vec3<Real>& in);

	template <class Real, class VecType> void refractInSurface(const VecType& surface_normal,
		const VecType& incident_raydir, Real src_refindex, Real dest_refindex,
		VecType& exit_raydir_out, bool& totally_internally_reflected_out);

	//static double getFresnelReflForCosAngle(double cos_angle_incidence,
	//			double fresnel_fraction);

	/*static double getReflForCosAngleSchlick(double cos_angle_incidence,
				double normal_reflectance);

	static void getReflForCosAngleSchlick(double cos_angle_incidence,
				const Colour3d& normal_reflectance, Colour3d& reflectance_out);*/

	//static double schlickFresnelFraction(double cos_angle_incidence);

	template <class Real> inline Real schlickFresnelReflectance(Real R_0, Real cos_theta);




	//template <class Real> static const Vec3<Real> sampleHemisphere(const Basis<Real>& basis, const SamplePair& unitsamples, Real& pdf_out);
	//template <class Real> static inline Real hemispherePDF();

	//static const Vec3d sampleSphere(const Vec2d& unitsamples, const Vec3d& normal, double& pdf_out);



	double evalNormalDist(double x, double mean, double standard_dev);



	void conductorFresnelReflectance(const SpectralVector& n, const SpectralVector& k, float cos_incident_angle, SpectralVector& F_R_out);
	template <class Real> Real conductorFresnelReflectance(Real n, Real k, Real cos_incident_angle);
	//template <class Real> const Vec2<Real> polarisedConductorFresnelReflectance(Real n, Real k, Real cos_theta);

	// Returns (F_perp, F_par)
	template <class Real> const Vec2<Real> polarisedConductorFresnelReflectanceExact(Real n, Real k, Real cos_theta);

	template <class Real> Real dielectricFresnelReflectance(Real srcn, Real destn, Real incident_cos_theta);
	template <class Real> Real dielectricFresnelReflectanceForSrcIOROne(Real n2, Real recip_n2, Real cos_theta_i_);

	// Returns (r_perp, r_par)
	template <class Real> void dielectricAmplitudeReflectionAndTransmissionCoefficients(Real srcn, Real destn, Real incident_cos_theta,
		Real& r_perp_out, Real& r_par_out, Real& t_perp_out, Real& t_par_out);

	template <class Real> inline Real pow5(Real x);


	template <class Real> inline Real smoothingFactor(Real in_dot_prebump_Ns, Real in_dot_Ns, Real in_dot_Ng);
	//inline FullHitInfo::Vec3RealType smoothingFactor(const FullHitInfo::Vec3Type& omega_in, const FullHitInfo& hitinfo);
	//inline static double smoothingFactor(const Vec3d& omega_in, const Vec3d& omega_out, const FullHitInfo& hitinfo, bool adjoint);
	//inline static double smoothingFactor(const Vec3d& omega_in, const Vec3d& omega_out, const FullHitInfo& hitinfo, bool adjoint, double in_dot_Ng, double out_dot_Ng);

	inline bool raysOnOppositeGeometricSides(const FullHitInfo::Vec3Type& a, const FullHitInfo::Vec3Type& b, const FullHitInfo& hitinfo);
	inline bool raysOnOppositeGeometricSides(FullHitInfo::Vec3RealType a_dot_orig_Ng, FullHitInfo::Vec3RealType b_dot_orig_Ng);


	// V is a vector away from the surface.
	// n1 will be set to the IOR of the medium that V points into.
	template <class Real> inline void getN1AndN2(Real external_ior, Real internal_ior, Real v_dot_orig_Ng, Real& n1_out, Real& n2_out);

	void unitTest();








/*template <class Real> Real spherePDF()
{
	return (Real)NICKMATHS_RECIP_4PI;
}*/





template <class Real> Real pow5(Real x)
{
	const Real x2 = x*x;
	return x2*x2*x;
	//return x*x*x*x*x;
}


/*
Compute the 'smoothing factor'.
This is a factor that changes BSDFs to make it look like the shading normal is the actual normal.
See equation 5.17 in Veach's thesis for some further information.

in_dot_prebump_Ns: signed dot product of 'in' (vector to light) with the shading normal without bump-map perturbation.

in_dot_Ns: signed dot product of 'in' (vector to light) with the shading normal.

in_dot_Ng: signed dot product of 'in' (vector to light) with the geometric normal.  It doesn't matter if this is the original or flipped N_g, as
the absolute value of it is taken.
*/
template <class Real>
Real smoothingFactor(Real in_dot_prebump_Ns, Real in_dot_Ns, Real in_dot_Ng)
{
	// Cap the smoothing factor due to the bump-mapped shading normal to N times what we would get from the un-bump-mapped shading normal.
	// This helps to make terminator artifacts on bump mapped objects less severe.
	// Without this you get very abrubt light->dark transitions on bump mapped objects at the terminator.
	const Real use_dot = myMin(in_dot_prebump_Ns*5, in_dot_Ns);

	return myMax<Real>(use_dot / std::fabs(in_dot_Ng), 0);
}

template <class Real>
Real scatterSmoothingFactor(const FullHitInfo& hitinfo, const Vec4f& dir_in, const Vec4f& scattered_dir, bool adjoint)
{
	const Vec4f a = adjoint ? -dir_in : scattered_dir;				// a = vector in light direction

	const Real a_dot_Ns = dot(a, hitinfo.N_s());					// Dot product of a and shading normal
	const Real pre_dot  = dot(a, hitinfo.pre_bump_shading_normal);	// Dot product of incident vector and pre-bump-perturbed shading normal
	const Real a_dot_Ng = dot(a, hitinfo.N_g());

	return MatUtils::smoothingFactor(pre_dot, a_dot_Ns, a_dot_Ng);
}


bool raysOnOppositeGeometricSides(const FullHitInfo::Vec3Type& a, const FullHitInfo::Vec3Type& b, const FullHitInfo& hitinfo)
{
	return dot(a, hitinfo.N_g()) * dot(b, hitinfo.N_g()) < 0;
}


bool raysOnOppositeGeometricSides(FullHitInfo::Vec3RealType a_dot_orig_Ng, FullHitInfo::Vec3RealType b_dot_orig_Ng)
{
	return a_dot_orig_Ng * b_dot_orig_Ng < 0;
}











/*
V is a vector away from the surface.
n1 will be set to the IOR of the medium that V points into.
*/
template <class Real>
void getN1AndN2(Real external_ior, Real internal_ior, Real v_dot_orig_Ng, Real& n1_out, Real& n2_out)
{
	if(v_dot_orig_Ng >= 0)
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


template <class VecType> const VecType reflectInSurface(const VecType& surface_normal, const VecType& in)
{
	return in + surface_normal * (dot(surface_normal, in) * -2.0f);
}


/*template <class Real> const Vec34 sphericalToCartesianCoords(Real phi, Real cos_theta, const Basis<Real>& basis)
{
	assert(cos_theta >= -1.0 && cos_theta <= 1.0);

	const Real sin_theta = sqrt((Real)1.0 - cos_theta*cos_theta);

	assert(Vec3<Real>(cos(phi)* sin_theta, sin(phi) * sin_theta, cos_theta).isUnitLength());
	return basis.transformVectorToParent(Vec3<Real>(cos(phi)* sin_theta, sin(phi) * sin_theta, cos_theta));
}*/


template <class Real> Real schlickFresnelReflectance(Real R_0, Real cos_theta)
{
	assert(Maths::inUnitInterval(cos_theta));

	return R_0 + (1 - R_0) * MatUtils::pow5<Real>(1 - cos_theta);
}



void checkPDF(ThreadContext& context, const FullHitInfo& hitinfo, const Reference<Material>& mat, const Vec4f& a, const Vec4f& b, const WavelengthSamples& wavelengths, bool adjoint, Material::Real target_pd);
void checkBSDF(ThreadContext& context, const FullHitInfo& hitinfo, const Reference<Material>& mat, const Vec4f& a, const Vec4f& b, const WavelengthSamples& wavelengths, Material::Real target_BSDF);
void checkPDFIsZero(ThreadContext& context, const FullHitInfo& hitinfo, const Reference<Material>& mat, const Vec4f& a, const Vec4f& b, const WavelengthSamples& wavelengths, bool adjoint);
void checkPDFIsGreaterThanZero(ThreadContext& context, const FullHitInfo& hitinfo, const Reference<Material>& mat, const Vec4f& a, const Vec4f& b, const WavelengthSamples& wavelengths, bool adjoint);
void checkBSDFIsZero(ThreadContext& context, const FullHitInfo& hitinfo, const Reference<Material>& mat, const Vec4f& a, const Vec4f& b, const WavelengthSamples& wavelengths);
void checkBSDFIsGreaterThanZero(ThreadContext& context, const FullHitInfo& hitinfo, const Reference<Material>& mat, const Vec4f& a, const Vec4f& b, const WavelengthSamples& wavelengths);

void testScatters(const Reference<Material>& material, float epsilon);


// See code_documentation/Visible normal sampling.pdf
template <class Real> Real trowbridgeReitzPDF(Real cos_theta, Real alpha2)
{
	return cos_theta * alpha2 / (Maths::pi<Real>() * Maths::square(Maths::square(cos_theta) * (alpha2 - 1) + 1));
}


template <class Real> Real trowbridgeReitzD(Real cos_theta, Real alpha2)
{
	return alpha2 / (Maths::pi<Real>() * Maths::square(Maths::square(cos_theta) * (alpha2 - 1) + 1));
}


} // End namespace MatUtils


#endif //__MATUTILS_H_666_
