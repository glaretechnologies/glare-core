/*=====================================================================
matutils.cpp
------------
File created by ClassTemplate on Tue May 04 18:40:22 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "matutils.h"


#include "../utils/MTwister.h"
#include "../indigo/globals.h"
#include "../indigo/SpectralVector.h"
#include "../graphics/colour3.h"
#include "../maths/basis.h"
#include "../maths/vec2.h"
#include "../indigo/TestUtils.h"
#include "../utils/timer.h"
#include "../utils/stringutils.h"
#include <fstream>


// Explicit template instantiation
template void MatUtils::refractInSurface(const Vec4f& surface_normal, const Vec4f& incident_raydir, float src_refindex, float dest_refindex, Vec4f& exit_raydir_out, bool& totally_internally_reflected_out);
template float MatUtils::dielectricFresnelReflectance(float srcn, float destn, float incident_cos_theta);
template void MatUtils::dielectricAmplitudeReflectionAndTransmissionCoefficients(float srcn, float destn, float incident_cos_theta, float& r_perp_out, float& r_par_out, float& t_perp_out, float& t_par_out);


namespace MatUtils
{


/*
Compute the refracted ray.

see http://en.wikipedia.org/wiki/Snell's_law#Vector_form for a similar derivation.  
It's not exactly the same though, as I don't negate the incident_raydir when computing the cosine.

*/
template <class Real, class VecType>
void refractInSurface(const VecType& normal,
		const VecType& incident_raydir, Real src_refindex, Real dest_refindex,
		VecType& exit_raydir_out, bool& totally_internally_reflected_out)
{
	assert(normal.isUnitLength());
	assert(incident_raydir.isUnitLength());
	assert(src_refindex >= 1.0);
	assert(dest_refindex >= 1.0);

	const Real n1_over_n2 = src_refindex / dest_refindex; // n_1 / n_2
	const Real n_dot_r = dot(normal, incident_raydir); // negative if ray towards interface is against normal

	const Real cos_theta_2_sqd = 1 - n1_over_n2*n1_over_n2 * (1 - n_dot_r*n_dot_r);

	if(cos_theta_2_sqd < 0)
	{
		totally_internally_reflected_out = true; // Total internal reflection occurred.

		// Make the exit ray the reflected vector.
		exit_raydir_out = incident_raydir;
		exit_raydir_out -= normal * (n_dot_r * 2);
	}
	else
	{
		totally_internally_reflected_out = false;
		//exit_raydir_out = n_dot_r > 0 ?
		//	normal * ( sqrt(cos_theta_2_sqd) - n1_over_n2*n_dot_r) + incident_raydir * n1_over_n2 : // n.r positive
		//	normal * (-sqrt(cos_theta_2_sqd) - n1_over_n2*n_dot_r) + incident_raydir * n1_over_n2 ; // n.r negative		

		exit_raydir_out = n_dot_r > 0 ?
			incident_raydir * n1_over_n2 + normal * (- n1_over_n2*n_dot_r + sqrt(cos_theta_2_sqd) ) : // n.r positive
			incident_raydir * n1_over_n2 + normal * (- n1_over_n2*n_dot_r - sqrt(cos_theta_2_sqd) ) ; // n.r negative		
	}


	//assert(epsEqual(exit_raydir_out.length(), (Real)1.0, (Real)0.0001));
	

	// Normalising here because there seems to be quite a lot of error introduced.
	exit_raydir_out = normalise(exit_raydir_out);

#ifndef NDEBUG
	Real normal_len = normal.length();
	Real incident_raydir_len = incident_raydir.length();
	Real exit_raydir_out_len = exit_raydir_out.length();
	//assert(epsEqual(exit_raydir_out_len, 1.0, 0.0001));
#endif

	assert(epsEqual(exit_raydir_out.length(), (Real)1.0));
}





/*const Vec3d MatUtils::sampleHemisphere(const Basisd& basis, const Vec2d& unitsamples, double& pdf_out)
{
	const double z = unitsamples.x;
	const double theta = unitsamples.y * NICKMATHS_2PI;

	const double r = sqrt(1.0 - z*z);

	Vec3d dir(cos(theta) * r, sin(theta) * r, z);

 	pdf_out = NICKMATHS_RECIP_2PI;

	return basis.transformVectorToParent(dir);
}*/

/*const Vec3 MatUtils::sampleHemisphere(const Vec2& unitsamples, const Vec3& normal, float& pdf_out)
{
	Basis basis;
	basis.constructFromVector(normal);

	const float z = unitsamples.x;
	const float theta = unitsamples.y * NICKMATHS_2PI;

	const float r = sqrtf(1.0f - z*z);

	Vec3 dir(cos(theta) * r, sin(theta) * r, z);

 	pdf_out = NICKMATHS_RECIP_2PI;

	return basis.transformVectorToParent(dir);
}*/


/*
const Vec3d MatUtils::sampleSphere(const Vec2d& unitsamples, const Vec3d& normal, double& pdf_out)
{
	Basisd basis;
	basis.constructFromVector(normal);

	const double z = -1.0f + unitsamples.x * 2.0f;
	const double theta = unitsamples.y * NICKMATHS_2PI;

	const double r = sqrt(1.0 - z*z);

	Vec3d dir(cos(theta) * r, sin(theta) * r, z);

 	pdf_out = NICKMATHS_RECIP_4PI;

	return basis.transformVectorToParent(dir);
}
*/





// Samples a bivariate Gaussian distribution, with mean (0,0) and given standard_deviation
const Vec2d boxMullerGaussian(double standard_deviation, MTwister& rng)
{
	//http://www.taygeta.com/random/gaussian.html
	double w, x1, x2;
	do
	{
		x1 = 2.0 * rng.unitRandom() - 1.0;
		x2 = 2.0 * rng.unitRandom() - 1.0;
		w = x1 * x1 + x2 * x2;
	}
	while(w >= 1.0);

	w = sqrt((-2.0 * log(w)) / w) * standard_deviation;

	return Vec2d(x1 * w, x2 * w);
	//y1 = x1 * w;
    //y2 = x2 * w;
}


double evalNormalDist(double x, double mean, double standard_dev)
{
	return exp(-(x-mean)*(x-mean) / (2.0*standard_dev*standard_dev)) / (standard_dev * sqrt(NICKMATHS_2PI));
}


void conductorFresnelReflectance(const SpectralVector& n, const SpectralVector& k, float cos_incident_angle, SpectralVector& F_R_out)
{
	//NOTE: SSE this up?
	for(unsigned int i=0; i<F_R_out.size(); ++i)
		F_R_out[i] = conductorFresnelReflectance(n[i], k[i], cos_incident_angle);
}


// This is an approximate expression for the reflectance.
// It seems reasonably close to that given by polarisedConductorFresnelReflectanceExact() below however.
template <class Real>
Real conductorFresnelReflectance(Real n, Real k, Real cos_theta)
{
	assert(n >= 0.0 && k >= 0.0);
	assert(cos_theta >= 0.0f);

	const Real n2_k2 = n*n + k*k;
	const Real costheta2 = cos_theta*cos_theta;

	const Real r_par = (n2_k2*costheta2 - (Real)2.0*n*cos_theta + (Real)1.0) / (n2_k2*costheta2 + (Real)2.0*n*cos_theta + (Real)1.0);
	const Real r_perp = (n2_k2 - (Real)2.0*n*cos_theta + costheta2) / (n2_k2 + (Real)2.0*n*cos_theta + costheta2);

	return (r_par + r_perp) * (Real)0.5;
}


//NOTE TEMP: one of these is wrong, probably the polarised version
/*template <class Real>
const Vec2<Real> MatUtils::polarisedConductorFresnelReflectance(Real n, Real k, Real cos_theta)
{
	assert(cos_theta >= 0.0f);

	const Real n2_k2 = n*n + k*k;
	const Real costheta2 = cos_theta*cos_theta;

	const Real r_par = (n2_k2*costheta2 - (Real)2.0*n*cos_theta + (Real)1.0) / (n2_k2*costheta2 + (Real)2.0*n*cos_theta + (Real)1.0);
	const Real r_perp = (n2_k2 - (Real)2.0*n*cos_theta + costheta2) / (n2_k2 + (Real)2.0*n*cos_theta + costheta2);

	return Vec2<Real>(r_perp, r_par);
}*/


// Returns (F_perp, F_par)
// Where F is the reflectance.
// From 'Digital Modelling of Material Appearance', page 99, eq 5.24
// Also in 'Ray tracing with polarization parameters', Wolff and Kurlander
template <class Real>
const Vec2<Real> polarisedConductorFresnelReflectanceExact(Real n, Real k, Real cos_theta)
{
	assert(cos_theta >= 0.0f && cos_theta <= 1.0f);

	const Real cos2_theta = cos_theta*cos_theta;
	const Real sin2_theta = 1.0f - cos_theta*cos_theta;
	const Real sin_theta = std::sqrt(sin2_theta);
	const Real tan_theta = sin_theta / cos_theta;

	const Real a2 = 0.5f * (std::sqrt(Maths::square(n*n - k*k - sin2_theta) + 4*n*n*k*k) + n*n - k*k - sin2_theta);

	const Real b2 = 0.5f * (std::sqrt(Maths::square(n*n - k*k - sin2_theta) + 4*n*n*k*k) - (n*n - k*k - sin2_theta));

	const Real a = std::sqrt(a2);
	//const Real b = sqrt(b2);
	const Real F_perp = (a2 + b2 - 2.0f*a*cos_theta + cos2_theta) / (a2 + b2 + 2.0f*a*cos_theta + cos2_theta);
	const Real F_par = F_perp * (a2 + b2 - 2.0f*a*sin_theta*tan_theta + sin2_theta*tan_theta*tan_theta) / (a2 + b2 + 2.0f*a*sin_theta*tan_theta + sin2_theta*tan_theta*tan_theta);

	return Vec2<Real>(F_perp, F_par);
}


template <class Real>
Real dielectricFresnelReflectance(Real n1, Real n2, Real cos_theta_i_)
{
	assert(n1 >= 1 && n2 >= 1);
	assert(cos_theta_i_ >= 0);
	const Real cos_theta_i = myMin<Real>(1, cos_theta_i_);

	// Get transmitted cos theta using Snell's law
	// http://en.wikipedia.org/wiki/Snell%27s_law

	const Real sintheta_i = sqrt(1 - cos_theta_i*cos_theta_i); // Get sin(theta_i)
	const Real sintheta_t = sintheta_i * n1 / n2; // Use Snell's law to get sin(theta_t)

	if(sintheta_t >= 1)
		return 1; // Total internal reflection

	const Real costheta_t = sqrt(1 - sintheta_t*sintheta_t); // Get cos(theta_t)

	// Now get the fraction reflected vs refracted with the Fresnel equations: http://en.wikipedia.org/wiki/Fresnel_equations

	const Real r_perp = (n1*cos_theta_i - n2*costheta_t) / (n1*cos_theta_i + n2*costheta_t); // R_s in wikipedia, electric field polarised along material surface
	const Real r_par = (n2*cos_theta_i - n1*costheta_t) / (n1*costheta_t + n2*cos_theta_i); // R_p in wikipedia, electric field polarised on plane of incidence

	const Real r = (r_par*r_par + r_perp*r_perp) * (Real)0.5;
	assert(Maths::inUnitInterval(r));
	return r;
}


// Returns (r_perp, r_par)
// See Optics (Hecht) pg 117
template <class Real>
void dielectricAmplitudeReflectionAndTransmissionCoefficients(Real n1, Real n2, Real cos_theta_i, 
																	 Real& r_perp_out, Real& r_par_out, Real& t_perp_out, Real& t_par_out)
{
	// Get transmitted cos theta using Snell's law
	// http://en.wikipedia.org/wiki/Snell%27s_law

	//assert(cos_theta_i >= 0.0f && cos_theta_i <= 1.0f);

	const Real sintheta = std::sqrt(myMax((Real)0.0, (Real)1.0 - cos_theta_i*cos_theta_i)); // Get sin(theta_i)
	const Real sintheta_t = sintheta * n1 / n2; // Use Snell's law to get sin(theta_t)

	if(sintheta_t >= (Real)1.0)
	{
		//return Vec2<Real>((Real)1.0, (Real)1.0); // Total internal reflection
		r_perp_out = 1.0;
		r_par_out = 1;
		t_perp_out = 0.0;
		t_par_out = 0.0;
		return;
	}

	const Real costheta_t = std::sqrt(1.0f - sintheta_t*sintheta_t); // Get cos(theta_t)

	// Now get the fraction reflected vs refracted with the Fresnel equations: http://en.wikipedia.org/wiki/Fresnel_equations

	// Component with electric field lying along interface (on surface)
	// Optics (Hecht) eq 4.34
	r_perp_out = (n1*cos_theta_i - n2*costheta_t) / (n1*cos_theta_i + n2*costheta_t); // R_s in wikipedia, electric field polarised along material surface

	// This is component with electric field in plane of incident, reflected and normal vectors. (plane of incidence)
	// Optics (Hecht) eq 4.40
	r_par_out = (n2*cos_theta_i - n1*costheta_t) / (n2*cos_theta_i + n1*costheta_t); // R_p in wikipedia, electric field polarised on plane of incidence

	t_perp_out = (Real)1.0 + r_perp_out; // Optics (Hecht) eq 4.49

	t_par_out = (Real)2.0 * n1 * cos_theta_i / (n2*cos_theta_i + n1*costheta_t); // Optics (Hecht) eq 4.41

#ifdef DEBUG
	const Real R_perp = r_perp_out*r_perp_out;
	const Real R_par = r_par_out*r_par_out;
	const Real T_perp = t_perp_out*t_perp_out;
	const Real T_par = t_par_out*t_par_out;

	//assert(epsEqual(R_perp + T_perp, (Real)1.0));
	//assert(epsEqual(R_par + T_par, (Real)1.0));
#endif
}


#if BUILD_TESTS


void checkPDF(ThreadContext& context, const FullHitInfo& hitinfo, const Reference<Material>& mat, const Vec4f& a, const Vec4f& b, Material::Real wavelen, bool adjoint, Material::Real target_pd)
{
	Material::Real pd = mat->scatterPDF(context, hitinfo, a, b, wavelen, 
		false, // sampled delta
		adjoint // adjoint
	);
	if(!epsEqual(pd, target_pd))
	{
		printVar(pd);
		printVar(target_pd);
	}
	testAssert(epsEqual(pd, target_pd));
}


void checkBSDF(ThreadContext& context, const FullHitInfo& hitinfo, const Reference<Material>& mat, const Vec4f& a, const Vec4f& b, const SpectralVector& wavelengths, Material::Real target_BSDF)
{
	PolarisationVec bsdfs;
	mat->evaluateBSDF(context, hitinfo, a, b, wavelengths, 
		false, // sampled delta
		bsdfs);

	for(unsigned int i=0; i<bsdfs.size(); ++i)
	{
		if(!epsEqual(bsdfs[i], target_BSDF))
		{
			printVar(bsdfs[i]);
			printVar(target_BSDF);
		}
		testAssert(epsEqual(bsdfs[i], target_BSDF));
	}
}


void checkPDFIsZero(ThreadContext& context, const FullHitInfo& hitinfo, const Reference<Material>& mat, const Vec4f& a, const Vec4f& b, Material::Real wavelen, bool adjoint)
{
	Material::Real pd = mat->scatterPDF(context, hitinfo, a, b, wavelen, 
		false, // sampled delta
		adjoint // adjoint
	);
	testAssert(pd == 0);
}


void checkPDFIsGreaterThanZero(ThreadContext& context, const FullHitInfo& hitinfo, const Reference<Material>& mat, const Vec4f& a, const Vec4f& b, Material::Real wavelen, bool adjoint)
{
	Material::Real pd = mat->scatterPDF(context, hitinfo, a, b, wavelen, 
		false, // sampled delta
		adjoint // adjoint
	);
	testAssert(pd > 0);
}


void checkBSDFIsZero(ThreadContext& context, const FullHitInfo& hitinfo, const Reference<Material>& mat, const Vec4f& a, const Vec4f& b, const SpectralVector& wavelengths)
{
	PolarisationVec bsdfs;
	mat->evaluateBSDF(context, hitinfo, a, b, wavelengths, 
		false, // sampled delta
		bsdfs);

	testAssert(bsdfs.isZero());
}


void checkBSDFIsGreaterThanZero(ThreadContext& context, const FullHitInfo& hitinfo, const Reference<Material>& mat, const Vec4f& a, const Vec4f& b, const SpectralVector& wavelengths)
{
	PolarisationVec bsdfs;
	mat->evaluateBSDF(context, hitinfo, a, b, wavelengths, 
		false, // sampled delta
		bsdfs);

	testAssert(bsdfs.minVal() > 0);
}


void unitTest()
{
	conPrint("MatUtils::unitTest()");

	
	// Test refraction from n_a = 1 to n_b = 1.5 with shading normal pointing up
	{
		float n_a = 1;
		float n_b = 1.5f;

		Vec4f n_s(0,0,1,0);

		float theta_a = NICKMATHS_PIf / 4;

		// theta_b should be 0.4908826782893113
		// so b should be (-sin(theta_b), 0, -cos(theta_b)) = (-0.4714045207910316, 0, -0.8819171036881969)

		Vec4f incident_raydir(-sin(theta_a), 0, -cos(theta_a), 0);
		Vec4f b;
		bool totally_internally_reflected;
		MatUtils::refractInSurface(
			n_s,
			incident_raydir,
			n_a,
			n_b,
			b,
			totally_internally_reflected
		);

		testAssert(!totally_internally_reflected);
		testAssert(epsEqual(b, Vec4f(-0.4714045207910316f, 0, -0.8819171036881969f, 0)));
	}

	// Test refraction from n_a = 1 to n_b = 1.5 with shading normal pointing down
	{
		float n_a = 1;
		float n_b = 1.5f;

		Vec4f n_s(0,0,-1,0);

		float theta_a = NICKMATHS_PIf / 4;

		// theta_b should be 0.4908826782893113
		// so b should be (-sin(theta_b), 0, -cos(theta_b)) = (-0.4714045207910316, 0, -0.8819171036881969)

		Vec4f incident_raydir(-sin(theta_a), 0, -cos(theta_a), 0);
		Vec4f b;
		bool totally_internally_reflected;
		MatUtils::refractInSurface(
			n_s,
			incident_raydir,
			n_a,
			n_b,
			b,
			totally_internally_reflected
		);

		testAssert(!totally_internally_reflected);
		testAssert(epsEqual(b, Vec4f(-0.4714045207910316f, 0, -0.8819171036881969f, 0)));
	}


			


	/*{
		std::ofstream f("c:/temp/fresnel.txt");

		const float n = 1.5f;
		const float k = 0.0f;
		for(float theta = 0.0f; theta <= NICKMATHS_PI_2; theta += 0.01f)
		{
			const Vec2f F = MatUtils::polarisedConductorFresnelReflectanceExact<float>(n, k, cos(theta));

			f << theta << " " << F.x << " " << F.y << " " << (0.5f * (F.x + F.y)) << "\n";
		}
	}*/

	/*{
		std::ofstream f("c:/temp/fresnel_approx.txt");

		const float n = 1.5f;
		const float k = 0.0f;
		for(float theta = 0.0f; theta <= NICKMATHS_PI_2; theta += 0.001f)
		{
			const Vec2f F = MatUtils::polarisedConductorFresnelReflectanceExact<float>(n, k, cos(theta));

			f << theta << "\t" << F.x << "\t" << F.y << "\n";
		}
	}*/

	// Check average (unpolarised) reflectance is the same using full (conductor) fresnel equations and dielectric fresnel equations
	{
		const float n = 1.5f;
		const float k = 0.0f;
		for(float theta = 0.0f; theta <= NICKMATHS_PI_2; theta += 0.01f)
		{
			const Vec2f F = MatUtils::polarisedConductorFresnelReflectanceExact<float>(n, k, std::cos(theta));

			const float R = 0.5f * (F.x + F.y);

			const float R2 = MatUtils::dielectricFresnelReflectance(1.0f, 1.5f, std::cos(theta));

			//printVar(R);
			//printVar(R2);
			testAssert(epsEqual(R, R2));
		}
	}







	/*double r;
	double n1 = 1.0f;
	double n2 = 1.5f;
	r = dielectricFresnelReflectance<double>(n1, n2, 1.0f);
	r = dielectricFresnelReflectance<double>(n1, n2, 0.8f);
	r = dielectricFresnelReflectance<double>(n1, n2, 0.6f);
	r = dielectricFresnelReflectance<double>(n1, n2, 0.4f);
	r = dielectricFresnelReflectance<double>(n1, n2, 0.2f);
	r = dielectricFresnelReflectance<double>(n1, n2, 0.1f);
	r = dielectricFresnelReflectance<double>(n1, n2, 0.05f);
	r = dielectricFresnelReflectance<double>(n1, n2, 0.01f);
	r = dielectricFresnelReflectance<double>(n1, n2, 0.0f);

	n1 = 1.5f;
	n2 = 1.0f;

	r = dielectricFresnelReflectance<double>(n1, n2, 1.0f);
	r = dielectricFresnelReflectance<double>(n1, n2, 0.8f);
	r = dielectricFresnelReflectance<double>(n1, n2, 0.6f);
	r = dielectricFresnelReflectance<double>(n1, n2, 0.4f);
	r = dielectricFresnelReflectance<double>(n1, n2, 0.2f);
	r = dielectricFresnelReflectance<double>(n1, n2, 0.1f);
	r = dielectricFresnelReflectance<double>(n1, n2, 0.05f);
	r = dielectricFresnelReflectance<double>(n1, n2, 0.01f);
	r = dielectricFresnelReflectance<double>(n1, n2, 0.0f);*/


	double n1 = 1.0;
	double n2 = 1.5;
	Vec3d normal(0,0,1);
	Vec3d out;
	bool tir;
	Vec3d in(0,0,-1);
	refractInSurface(normal, in, n1, n2, out, tir);
	testAssert(epsEqual(out, in));
	testAssert(!tir);


	double inangle, outangle, left, right;

	in = normalise(Vec3d(1,0,-1));
	refractInSurface(normal, in, n1, n2, out, tir);

	inangle = acos(fabs(dot(in, normal)));
	outangle = acos(fabs(dot(out, normal)));
	left = n1 * sin(inangle);
	right = n2 * sin(outangle);
	testAssert(::epsEqual(left, right));

	in = normalise(Vec3d(3,0,-1));
	refractInSurface(normal, in, n1, n2, out, tir);

	inangle = acos(fabs(dot(in, normal)));
	outangle = acos(fabs(dot(out, normal)));
	left = n1 * sin(inangle);
	right = n2 * sin(outangle);
	testAssert(::epsEqual(left, right));

	in = normalise(Vec3d(-0.2f,0,-1));
	refractInSurface(normal, in, n1, n2, out, tir);

	inangle = acos(fabs(dot(in, normal)));
	outangle = acos(fabs(dot(out, normal)));
	left = n1 * sin(inangle);
	right = n2 * sin(outangle);
	testAssert(::epsEqual(left, right));

	//try coming against normal
	in = normalise(Vec3d(-0.2f,0,1));
	refractInSurface(normal, in, n1, n2, out, tir);

	inangle = acos(fabs(dot(in, normal)));
	outangle = acos(fabs(dot(out, normal)));
	left = n1 * sin(inangle);
	right = n2 * sin(outangle);
	testAssert(::epsEqual(left, right));

	in = normalise(Vec3d(4,0,1));
	refractInSurface(normal, in, n1, n2, out, tir);

	inangle = acos(fabs(dot(in, normal)));
	outangle = acos(fabs(dot(out, normal)));
	left = n1 * sin(inangle);
	right = n2 * sin(outangle);
	testAssert(::epsEqual(left, right));

	//from dense to less dense
	n1 = 1.5f;
	n2 = 1.1f;
	in = normalise(Vec3d(0.3f,0,1));
	refractInSurface(normal, in, n1, n2, out, tir);

	inangle = acos(fabs(dot(in, normal)));
	outangle = acos(fabs(dot(out, normal)));
	left = n1 * sin(inangle);
	right = n2 * sin(outangle);
	testAssert(::epsEqual(left, right));

	//Total internal reflection

	//from dense to less dense
	n1 = 1.5f;
	n2 = 1.1f;
	in = normalise(Vec3d(4,0,1));
	refractInSurface(normal, in, n1, n2, out, tir);
	testAssert(tir);

	inangle = acos(fabs(dot(in, normal)));
	outangle = acos(fabs(dot(out, normal)));
	testAssert(::epsEqual(inangle, outangle));
	/*left = n1 * sin(inangle);
	right = n2 * sin(outangle);
	assert(::epsEqual(left, right));*/
}


#endif // BUILD_TESTS


} // end namespace MatUtils
