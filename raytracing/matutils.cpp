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
template const Vec3<float> MatUtils::sampleHemisphereCosineWeighted(const Basis<float>& basis, const SamplePair& unitsamples);
template void MatUtils::refractInSurface(const Vec3<float>& surface_normal, const Vec3<float>& incident_raydir, float src_refindex, float dest_refindex, Vec3<float>& exit_raydir_out, bool& totally_internally_reflected_out);
template const Vec2<float> MatUtils::sampleUnitDisc(const SamplePair& unitsamples);
template const Vec2<double> MatUtils::sampleUnitDisc(const SamplePair& unitsamples);
template float MatUtils::dielectricFresnelReflectance(float srcn, float destn, float incident_cos_theta);


/*-----------------------------------------------------------------------------------------------
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
static inline void shirleyUnitSquareToDisk(const SamplePair& unitsamples, Vec2<Real>& spheresamples_out)
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
	spheresamples_out.x = r * cos(phi);
	spheresamples_out.y = r * sin(phi);

	assert(spheresamples_out.x >= -1.f && spheresamples_out.x <= 1.f);
	assert(spheresamples_out.y >= -1.f && spheresamples_out.y <= 1.f);
}


/*
Compute the refracted ray.

see http://en.wikipedia.org/wiki/Snell's_law#Vector_form

*/
template <class Real>
void MatUtils::refractInSurface(const Vec3<Real>& normal, 
		const Vec3<Real>& incident_raydir, Real src_refindex, Real dest_refindex,
		Vec3<Real>& exit_raydir_out, bool& totally_internally_reflected_out)
{
	assert(normal.isUnitLength());
	assert(incident_raydir.isUnitLength());
	assert(src_refindex >= 1.0);
	assert(dest_refindex >= 1.0);

	const Real n_ratio = src_refindex / dest_refindex; // n_1 / n_2
	const Real n_dot_r = dot(normal, incident_raydir); // negative if ray towards interface is against normal

	const Real a = (Real)1.0 - n_ratio*n_ratio * ((Real)1.0 - n_dot_r*n_dot_r);
	// a is the expression inside the sqrt - is actually cos(theta_2) ^ 2

	if(a < 0.0)
	{
		totally_internally_reflected_out = true; // Total internal reflection occurred
		
		// Make the exit ray the reflected vector.
		exit_raydir_out = incident_raydir;
		exit_raydir_out.subMult(normal, n_dot_r * (Real)2.0);
	}
	else
	{
		totally_internally_reflected_out = false;
		exit_raydir_out = n_dot_r < 0.0 ? 
			normal * (-sqrt(a) - n_ratio*n_dot_r) + incident_raydir * n_ratio : // n.r negative
			normal * (sqrt(a) - n_ratio*n_dot_r) + incident_raydir * n_ratio;   // n.r positive
	}


	assert(epsEqual(exit_raydir_out.length(), (Real)1.0, (Real)0.0001));
	
	// Normalising here because there seems to be quite a lot of error introduced.
	exit_raydir_out.normalise();

#ifdef DEBUG
	double len = exit_raydir_out.length();
	assert(epsEqual(len, 1.0, 0.0001));
#endif
}


template <class Real>
const Vec2<Real> MatUtils::sampleUnitDisc(const SamplePair& unitsamples)
{
	/*const float r = sqrt(unitsamples.x);
	const float theta = unitsamples.y * NICKMATHS_2PI;
	return Vec2(cos(theta)*r, sin(theta)*r);*/

	Vec2<Real> disc;
	shirleyUnitSquareToDisk(unitsamples, disc);
	return disc;
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


template <class Real>
const Vec3<Real> MatUtils::sampleHemisphereCosineWeighted(const Basis<Real>& basis, const SamplePair& unitsamples/*, double& pdf_out*/)
{
	//sample unit disc
	/*const float r = sqrtf(unitsamples.x);
	const float theta = unitsamples.y * NICKMATHS_2PI;

	Vec3d dir(cos(theta)*r, sin(theta)*r, sqrt(1.0f - r*r));

	pdf_out = dir.z * NICKMATHS_RECIP_PI;

	return basis.transformVectorToParent(dir);*/

	Vec2<Real> disc;
	shirleyUnitSquareToDisk<Real>(unitsamples, disc);
	
	const Vec3<Real> dir(disc.x, disc.y, sqrt(myMax((Real)0.0, (Real)1.0 - (disc.x*disc.x + disc.y*disc.y))));
	assert(dir.isUnitLength());
	
	return basis.transformVectorToParent(dir);
}


// Samples a bivariate Gaussian distribution, with mean (0,0) and given standard_deviation
const Vec2d MatUtils::boxMullerGaussian(double standard_deviation, MTwister& rng)
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

double MatUtils::evalNormalDist(double x, double mean, double standard_dev)
{
	return exp(-(x-mean)*(x-mean) / (2.0*standard_dev*standard_dev)) / (standard_dev * sqrt(NICKMATHS_2PI));
}


void MatUtils::conductorFresnelReflectance(const SpectralVector& n, const SpectralVector& k, float cos_incident_angle, SpectralVector& F_R_out)
{
	//NOTE: SSE this up?
	for(unsigned int i=0; i<F_R_out.size(); ++i)
		F_R_out[i] = conductorFresnelReflectance(n[i], k[i], cos_incident_angle);
}


template <class Real>
Real MatUtils::conductorFresnelReflectance(Real n, Real k, Real cos_theta)
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
template <class Real>
const Vec2<Real> MatUtils::polarisedConductorFresnelReflectance(Real n, Real k, Real cos_theta)
{
	assert(cos_theta >= 0.0f);

	const Real n2_k2 = n*n + k*k;
	const Real costheta2 = cos_theta*cos_theta;

	const Real r_par = (n2_k2*costheta2 - (Real)2.0*n*cos_theta + (Real)1.0) / (n2_k2*costheta2 + (Real)2.0*n*cos_theta + (Real)1.0);
	const Real r_perp = (n2_k2 - (Real)2.0*n*cos_theta + costheta2) / (n2_k2 + (Real)2.0*n*cos_theta + costheta2);

	return Vec2<Real>(r_perp, r_par);
}


// Returns (F_perp, F_par)
// From 'Digital Modelling of Material Appearance', page 99, eq 5.24
template <class Real>
const Vec2<Real> MatUtils::polarisedConductorFresnelReflectanceExact(Real n, Real k, Real cos_theta)
{
	assert(cos_theta >= 0.0f && cos_theta <= 1.0f);

	const Real cos2_theta = cos_theta*cos_theta;
	const Real sin2_theta = 1.0f - cos_theta*cos_theta;
	const Real sin_theta = sqrt(sin2_theta);
	const Real tan_theta = sin_theta / cos_theta;

	const Real a2 = 0.5f * (sqrt(Maths::square(n*n - k*k - sin2_theta) + 4*n*n*k*k) + n*n - k*k - sin2_theta);

	const Real b2 = 0.5f * (sqrt(Maths::square(n*n - k*k - sin2_theta) + 4*n*n*k*k) - (n*n - k*k - sin2_theta));

	const Real a = sqrt(a2);
	const Real b = sqrt(b2);
	const Real F_perp = (a2 + b2 - 2.0f*a*cos_theta + cos2_theta) / (a2 + b2 + 2.0f*a*cos_theta + cos2_theta);
	const Real F_par = F_perp * (a2 + b2 - 2.0f*a*sin_theta*tan_theta + sin2_theta*tan_theta*tan_theta) / (a2 + b2 + 2.0f*a*sin_theta*tan_theta + sin2_theta*tan_theta*tan_theta);

	return Vec2<Real>(F_perp, F_par);
}



template <class Real>
Real MatUtils::dielectricFresnelReflectance(Real n1, Real n2, Real cos_theta_i)
{
	//Get transmitted cos theta using Snell's law
	//http://en.wikipedia.org/wiki/Snell%27s_law

	const Real sintheta_i = sqrt((Real)1.0 - cos_theta_i*cos_theta_i); // Get sin(theta_i)
	const Real sintheta_t = sintheta_i * n1 / n2; // Use Snell's law to get sin(theta_t)

	if(sintheta_t >= (Real)1.0)
		return (Real)1.0; // Total internal reflection

	const Real costheta_t = sqrt((Real)1.0 - sintheta_t*sintheta_t); // Get cos(theta_t)

	// Now get the fraction reflected vs refracted with the Fresnel equations: http://en.wikipedia.org/wiki/Fresnel_equations

	const Real r_perp = (n1*cos_theta_i - n2*costheta_t) / (n1*cos_theta_i + n2*costheta_t); // R_s in wikipedia, electric field polarised along material surface
	const Real r_par = (n2*cos_theta_i - n1*costheta_t) / (n1*costheta_t + n2*cos_theta_i); // R_p in wikipedia, electric field polarised on plane of incidence

	const Real r = (r_par*r_par + r_perp*r_perp) * (Real)0.5;
	assert(Maths::inUnitInterval(r));
	return r;
}


template <class Real>
const Vec2<Real> MatUtils::polarisedDielectricFresnelReflectance(Real n1, Real n2, Real cos_theta_i)
{
	//Get transmitted cos theta using Snell's law
	//http://en.wikipedia.org/wiki/Snell%27s_law

	assert(cos_theta_i >= 0.0f && cos_theta_i <= 1.0f);

	const Real sintheta = sqrt((Real)1.0 - cos_theta_i*cos_theta_i); // Get sin(theta_i)
	const Real sintheta_t = sintheta * n1 / n2; // Use Snell's law to get sin(theta_t)

	if(sintheta_t >= (Real)1.0)
		return Vec2<Real>((Real)1.0); // Total internal reflection

	const double costheta_t = sqrt(1.0f - sintheta_t*sintheta_t); // Get cos(theta_t)

	// Now get the fraction reflected vs refracted with the Fresnel equations: http://en.wikipedia.org/wiki/Fresnel_equations

	// Component with electric field lying along interface (on surface)
	const Real r_perp = (n1*cos_theta_i - n2*costheta_t) / (n1*cos_theta_i + n2*costheta_t); // R_s in wikipedia, electric field polarised along material surface

	// This is component with electric field in plane of incident, reflected and normal vectors. (plane of incidence)
	const Real r_par = (n2*cos_theta_i - n1*costheta_t) / (n1*costheta_t + n2*cos_theta_i); // R_p in wikipedia, electric field polarised on plane of incidence

	//return Vec2d(r_par*r_par, r_perp*r_perp);
	return Vec2<Real>(r_perp*r_perp, r_par*r_par);
}


void MatUtils::unitTest()
{
	conPrint("MatUtils::unitTest()");


	{
		std::ofstream f("c:/temp/fresnel.txt");

		const float n = 1.5f;
		const float k = 0.0f;
		for(float theta = 0.0f; theta <= NICKMATHS_PI_2; theta += 0.001f)
		{
			const Vec2f F = MatUtils::polarisedConductorFresnelReflectanceExact<float>(n, k, cos(theta));

			f << theta << " " << F.x << " " << F.y << " " << (0.5f * (F.x + F.y)) << "\n";
		}
	}

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
		for(float theta = 0.0f; theta <= NICKMATHS_PI_2; theta += 0.001f)
		{
			const Vec2f F = MatUtils::polarisedConductorFresnelReflectanceExact<float>(n, k, cos(theta));

			const float R = 0.5f * (F.x + F.y);

			const float R2 = MatUtils::dielectricFresnelReflectance(1.0f, 1.5f, cos(theta));

			printVar(R);
			printVar(R2);
			testAssert(epsEqual(R, R2));
		}
	}


	{
		const Vec3d d = sphericalToCartesianCoords(NICKMATHS_PI * (3.0/2.0), cos(NICKMATHS_PI_2), Basisd(Matrix3d::identity()));
		assert(epsEqual(d, Vec3d(0, -1, 0)));
	}

	testAssert(epsEqual(MatUtils::sphericalCoordsForDir(Vec3d(1,0,0), 1.0), Vec2d(0.0, NICKMATHS_PI_2)));
	testAssert(epsEqual(MatUtils::sphericalCoordsForDir(Vec3d(0,1,0), 1.0), Vec2d(NICKMATHS_PI_2, NICKMATHS_PI_2)));
	testAssert(epsEqual(MatUtils::sphericalCoordsForDir(Vec3d(0,-1,0), 1.0), Vec2d(-NICKMATHS_PI_2, NICKMATHS_PI_2)));
	testAssert(epsEqual(MatUtils::sphericalCoordsForDir(Vec3d(0,0,1), 1.0), Vec2d(0.0, 0.0)));
	testAssert(epsEqual(MatUtils::sphericalCoordsForDir(Vec3d(0,0,-1), 1.0), Vec2d(0.0, NICKMATHS_PI)));

	testAssert(epsEqual(MatUtils::dirForSphericalCoords(0.0, NICKMATHS_PI_2), Vec3d(1,0,0)));
	testAssert(epsEqual(MatUtils::dirForSphericalCoords(NICKMATHS_PI_2, NICKMATHS_PI_2), Vec3d(0,1,0)));
	testAssert(epsEqual(MatUtils::dirForSphericalCoords(-NICKMATHS_PI_2, NICKMATHS_PI_2), Vec3d(0,-1,0)));
	testAssert(epsEqual(MatUtils::dirForSphericalCoords(0.0, 0.0), Vec3d(0,0,1)));
	testAssert(epsEqual(MatUtils::dirForSphericalCoords(0.0, NICKMATHS_PI), Vec3d(0,0,-1)));



	Basis<double> basis;
	const Vec3d v = normalise(Vec3d(1,1,1));
	basis.constructFromVector(v);

	const Vec3d res = sampleSolidAngleCone<double>(SamplePair(0.5, 0.5), basis, 0.01f);

	testAssert(dot(v, res) > 0.95);

	double r;
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
	r = dielectricFresnelReflectance<double>(n1, n2, 0.0f);


	n1 = 1.0f;
	n2 = 1.5f;
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
