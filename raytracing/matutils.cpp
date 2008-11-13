/*=====================================================================
matutils.cpp
------------
File created by ClassTemplate on Tue May 04 18:40:22 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "matutils.h"


#include "../utils/random.h"
#include "../utils/MTwister.h"
//#include "../indigo/sphereunitvecgenerator.h"
#include "../indigo/globals.h"
#include "../indigo/SpectralVector.h"
#include "../graphics/colour3.h"
#include "../maths/basis.h"
#include "../maths/vec2.h"
#include "../indigo/SpectrumGraph.h"
#include "../indigo/IrregularSpectrum.h"
#include "../indigo/TestUtils.h"
#include "../utils/timer.h"

//TEMP:
//RandNumPool rand_num_pool(65536);

//TEMP:
//SphereUnitVecGenerator vecpool(10000);

//TEMP:
/*inline float matUtilsUnitRandom()
{
	return unitRandom();
}*/


MatUtils::MatUtils()
{
	
}


MatUtils::~MatUtils()
{
	
}


const Vec3d MatUtils::sphericalToCartesianCoords(double phi, double cos_theta, const Basisd& basis)
{
	assert(cos_theta >= -1.0 && cos_theta <= 1.0);

	const double sin_theta = sqrt(1.0 - cos_theta*cos_theta);

	assert(Vec3d(cos(phi)* sin_theta, sin(phi) * sin_theta, cos_theta).isUnitLength());
	return basis.transformVectorToParent(Vec3d(cos(phi)* sin_theta, sin(phi) * sin_theta, cos_theta));
}



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

static inline void shirleyUnitSquareToDisk(const SamplePair& unitsamples, Vec2d& spheresamples_out)
{
	const double a = 2.0*unitsamples.x - 1.0;   /* (a,b) is now on [-1,1]^2 */
	const double b = 2.0*unitsamples.y - 1.0;

	double phi, r;

	if (a > -b) {     // region 1 or 2
		if (a > b) {  // region 1, also |a| > |b|
			r = a;
			phi = NICKMATHS_PI_4 * (b/a);
		}
		else {  // region 2, also |b| > |a| 
			r = b;
			phi = NICKMATHS_PI_4 * (2.0 - (a/b));
		}
	}
	else {        // region 3 or 4 
		if (a < b) {  // region 3, also |a| >= |b|, a != 0
			r = -a;
			phi = NICKMATHS_PI_4 * (4.0 + (b/a));
		}
		else {  // region 4, |b| >= |a|, but a==0 and b==0 could occur.
			r = -b;
			if (b != 0)
				phi = NICKMATHS_PI_4 * (6.0 - (a/b));
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


/*void MatUtils::getRandomSphereUnitVec(Vec3& vec_out)
{
	while(1)
	{
		vec_out.x = -1.0f + matUtilsUnitRandom() * 2.0f;
		vec_out.y = -1.0f + matUtilsUnitRandom() * 2.0f;
		vec_out.z = -1.0f + matUtilsUnitRandom() * 2.0f;

		if(vec_out.length2() <= 1.0f)
			break;
	}

	vec_out.normalise();
}*/


/*void MatUtils::getRandomHemisUnitVec(const Vec3& surface_normal, int const_comp_index,
				int otherindex_1, int otherindex_2, Vec3& vec_out)
{
	assert(surface_normal.isUnitLength());

	//TEMP:
	//getRandomHemisUnitVec(surface_normal, vec_out);

	while(1)
	{
		vec_out[const_comp_index] = matUtilsUnitRandom() * surface_normal[const_comp_index];
		vec_out[otherindex_1] = -1.0f + matUtilsUnitRandom() * 2.0f;
		vec_out[otherindex_2] = -1.0f + matUtilsUnitRandom() * 2.0f;

		assert(vec_out.dot(surface_normal) >= 0);

		if(vec_out.length2() <= 1.0f)
			break;
	}

	vec_out.normalise();

}*/

/*
void MatUtils::getRandomHemisUnitVec(const Vec3& surface_normal, Vec3& vec_out)
{
	assert(surface_normal.isUnitLength());

	while(1)
	{
		getRandomSphereUnitVec(vec_out);


		if(dot(surface_normal, vec_out) > 0.0f)
			break;
	}

	assert(vec_out.dot(surface_normal) >= 0);
	assert(vec_out.isUnitLength());

}*/
/*
void MatUtils::getCosineWeightedHemisUnitVec(const Vec3& surface_normal, Vec3& vec_out)
{
	Vec3 v;
	while(1)
	{
		//v = vecpool.getNextVec();
		getRandomHemisUnitVec(surface_normal, v);

		if(unitRandom() < surface_normal.dot(v))
		{
			vec_out = v;
			return;
		}
	}
*/
	

	
	/*
	Vec3 v;
	while(1)
	{
		//gen vec in cube around unit sphere
		v.x = -1.0f + Random::unit() * 2.0f;
		v.y = -1.0f + Random::unit() * 2.0f;
		v.z = -1.0f + Random::unit() * 2.0f;

		const float length2 = v.length2();

		if(length2 <= 1.0f)
		{
			//we have a (non-normalised) vec on the unit sphere

			const float normdot_times_len = surface_normal.dot(v);

			const float veclength = sqrtf(length2);

			if(Random::unit() * veclength <= normdot_times_len)
			{
				//normalise vec
				v.x /= veclength;
				v.y /= veclength;
				v.z /= veclength;
		
				assert(v.isUnitLength());
				assert(v.dot(surface_normal) >= 0);


				return;
			}
		}
	}

	vec_out = v;
*/
	/*while(1)
	{
		//gen vec in cube around unit sphere
		vec_out.x = -1.0f + Random::unit() * 2.0f;
		vec_out.y = -1.0f + Random::unit() * 2.0f;
		vec_out.z = -1.0f + Random::unit() * 2.0f;

		const float length2 = vec_out.x*vec_out.x + vec_out.y*vec_out.y + vec_out.z*vec_out.z;

		if(length2 <= 1.0f)
		{
			//we have a (non-normalised) vec on the unit sphere

			const float normdot_times_len = surface_normal.x*vec_out.x + 
											surface_normal.y*vec_out.y +
											surface_normal.z*vec_out.z;

			const float veclength = sqrtf(length2);

			if(Random::unit() * veclength <= normdot_times_len)
			{
				//normalise vec
				vec_out.x /= veclength;
				vec_out.y /= veclength;
				vec_out.z /= veclength;
		
				assert(vec_out.isUnitLength());
				assert(vec_out.dot(surface_normal) >= 0);


				return;
			}
		}
	}
}*/


void MatUtils::reflectInSurface(const Vec3d& surface_normal, Vec3d& vec_out)
{
//	assert(vec_out.isUnitLength());
//	assert(surface_normal.isUnitLength());

	vec_out.addMult(surface_normal, dot(surface_normal, vec_out) * -2.0);

//	assert(vec_out.dot(surface_normal) >= 0);
//	assert(vec_out.isUnitLength());

}




/*void MatUtils::refractInSurface(const Vec3& surface_normal, 
		const Vec3& incident_raydir, float src_refindex, float dest_refindex,
		Vec3& exit_raydir_out, bool& totally_internally_reflected_out)
{
	//ratio of refractive indices.
	const float n = src_refindex / dest_refindex;

	//const float neg_raydir_on_normal = -dot(incident_raydir, surface_normal);
	float c1 = dot(incident_raydir, surface_normal);

	float abs_c1 = c1;
	if(abs_c1 < 0.0f)
		abs_c1 *= -1.0f;


	const float c2_sqrd = 1.0f - n*n * (1.0f - c1*c1);

	if(c2_sqrd < 0.0f)
	{
		totally_internally_reflected_out = true;

		//make the exit ray the reflected vector.
		exit_raydir_out = incident_raydir;
		exit_raydir_out.subMult(surface_normal, c1 * 2.0f);

		//check rays going in opposite dirs relative to normal.
		assert(dot(exit_raydir_out, surface_normal) * 
				dot(incident_raydir, surface_normal) <= 0.0f);

		//exit_raydir_out = incident_raydir;
		return;
	}

	const float c2 = sqrt(c2_sqrd);//sqrt(1.0f - n*n * (1.0f - c1*c1));

	exit_raydir_out = incident_raydir * n +
				surface_normal * (n * abs_c1 - c2) * -1.0f;

	totally_internally_reflected_out = false;
}*/


/*
Compute the refracted ray.

see http://en.wikipedia.org/wiki/Snell's_law#Vector_form

*/
void MatUtils::refractInSurface(const Vec3d& normal, 
		const Vec3d& incident_raydir, double src_refindex, double dest_refindex,
		Vec3d& exit_raydir_out, bool& totally_internally_reflected_out)
{
	assert(normal.isUnitLength());
	assert(incident_raydir.isUnitLength());
	assert(src_refindex >= 1.0);
	assert(dest_refindex >= 1.0);


	
	const double n_ratio = src_refindex / dest_refindex; // n_1 / n_2
	const double n_dot_r = dot(normal, incident_raydir); // negative if ray towards interface is against normal

	const double a = 1.0 - n_ratio*n_ratio * (1.0 - n_dot_r*n_dot_r);
	// a is the expression inside the sqrt - is actually cos(theta_2) ^ 2

	if(a < 0.0)
	{
		totally_internally_reflected_out = true;//total internal reflection occurred
		
		//make the exit ray the reflected vector.
		exit_raydir_out = incident_raydir;
		exit_raydir_out.subMult(normal, n_dot_r * 2.0);
	}
	else
	{
		totally_internally_reflected_out = false;
		exit_raydir_out = n_dot_r < 0.0 ? 
			normal * (-sqrt(a) - n_ratio*n_dot_r) + incident_raydir * n_ratio : // n.r negative
			normal * (sqrt(a) - n_ratio*n_dot_r) + incident_raydir * n_ratio;   // n.r positive
	}


	assert(epsEqual(exit_raydir_out.length(), 1.0, 0.0001));
	//normalising here because there seems to be quite a lot of error introduced.
	exit_raydir_out.normalise();


#ifdef DEBUG
	//double len = exit_raydir_out.length();
	//assert(epsEqual(len, 1.0, 0.0001));
#endif
	
	assert(exit_raydir_out.isUnitLength());
	

/*


	Vec3d normal = normal_;

	const double n_ratio = src_refindex / dest_refindex;
	double n_dot_r = dot(normal, incident_raydir);

	//Swap normal so that is pointing against incident_raydir
	if(n_dot_r > 0.0)
	{
		normal *= -1.0;
		n_dot_r *= -1.0;//update the dot product
	}
	assert(::epsEqual(dot(normal, incident_raydir), n_dot_r));

	const double a = 1.0 - n_ratio*n_ratio * (1.0 - n_dot_r*n_dot_r);
	//a is the expression inside the sqrt.
	//a is actually cos(theta_2) ^ 2

	if(a < 0.0)
	{
		totally_internally_reflected_out = true;//total internal reflection occurred
		
		//make the exit ray the reflected vector.
		exit_raydir_out = incident_raydir;
		exit_raydir_out.subMult(normal, n_dot_r * 2.0);
	}
	else
	{
		totally_internally_reflected_out = false;
		exit_raydir_out = normal * (-sqrt(a) - n_ratio*n_dot_r) + incident_raydir * n_ratio;
	}

#ifdef DEBUG
	double len = exit_raydir_out.length();
#endif
	assert(exit_raydir_out.isUnitLength());*/
}


/*
void MatUtils::refractInSurface(const Vec3& surface_normal, 
		const Vec3& incident_raydir, float src_refindex, float dest_refindex,
		Vec3& exit_raydir_out, bool& totally_internally_reflected_out)
{
	assert(::epsEqual(surface_normal.length(), 1.0f));
	assert(::epsEqual(incident_raydir.length(), 1.0f));

	const float n = src_refindex / dest_refindex;

	//const float neg_raydir_on_normal = -dot(incident_raydir, surface_normal);
	float c1 = -dot(incident_raydir, surface_normal);

	//c1 is positive in the case that light is heading in opposite dir to normal
	//ie. from outside to inside the surface.

	Vec3 normal = surface_normal;
	if(c1 < 0.0f)
	{
		normal *= -1.0f;
		c1 *= -1.0f;
	}

	assert(c1 >= 0.0f);

	const float c2_sqrd = 1.0f - n*n * (1.0f - c1*c1);

	if(c2_sqrd < 0.0f)
	{
		totally_internally_reflected_out = true;

		//make the exit ray the reflected vector.
		exit_raydir_out = incident_raydir;
		exit_raydir_out.addMult(normal, c1 * 2.0f);

		//check rays going in opposite dirs relative to normal.
		assert(dot(exit_raydir_out, normal) * 
				dot(incident_raydir, normal) <= 0.0f);
		return;
	}

	const float c2 = sqrt(c2_sqrd);

	exit_raydir_out = incident_raydir * n + normal * (n * c1 - c2);

	totally_internally_reflected_out = false;

	//TEMP: renormalise to avoid loss of normalistion issues
	assert(::epsEqual(exit_raydir_out.length(), 1.0f, 0.001f));
	exit_raydir_out.normalise();

	assert(::epsEqual(exit_raydir_out.length(), 1.0f));
}*/

/*void MatUtils::refractInSurface(const Vec3& surface_normal, 
		const Vec3& incident_raydir, float src_refindex, float dest_refindex,
		Vec3& exit_raydir_out, bool& totally_internally_reflected_out)
{
	assert(::epsEqual(surface_normal.length(), 1.0f));
	assert(::epsEqual(incident_raydir.length(), 1.0f));


	const float n = src_refindex / dest_refindex;

	//const float neg_raydir_on_normal = -dot(incident_raydir, surface_normal);
	float c1 = -dot(incident_raydir, surface_normal);

	//c1 is positive in the case that light is heading in opposite dir to normal
	//ie. from outside to inside the surface.

	if(c1 < 0.0f)
	{
		//light dir is from inside -> outside

		c1 = -c1;

		const float c2_sqrd = 1.0f - n*n * (1.0f - c1*c1);

		if(c2_sqrd < 0.0f)
		{
			totally_internally_reflected_out = true;

			//make the exit ray the reflected vector.
			exit_raydir_out = incident_raydir;
			exit_raydir_out.subMult(surface_normal, c1 * 2.0f);

			//check rays going in opposite dirs relative to normal.
			assert(dot(exit_raydir_out, surface_normal) * 
					dot(incident_raydir, surface_normal) <= 0.0f);

			//exit_raydir_out = incident_raydir;
			return;
		}

		const float c2 = sqrt(c2_sqrd);//sqrt(1.0f - n*n * (1.0f - c1*c1));

		exit_raydir_out = incident_raydir * n +
					surface_normal * (n * c1 - c2) * -1.0f;
	}
	else
	{
		const float c2_sqrd = 1.0f - n*n * (1.0f - c1*c1);

		if(c2_sqrd < 0.0f)
		{
			totally_internally_reflected_out = true;

			//make the exit ray the reflected vector.
			exit_raydir_out = incident_raydir;
			exit_raydir_out.addMult(surface_normal, c1 * 2.0f);

			//check rays going in opposite dirs relative to normal.
			assert(dot(exit_raydir_out, surface_normal) * 
					dot(incident_raydir, surface_normal) <= 0.0f);



			//exit_raydir_out = incident_raydir;
			return;
		}
		const float c2 = sqrt(c2_sqrd);//sqrt(1.0f - n*n * (1.0f - c1*c1));

		exit_raydir_out = incident_raydir * n +
					surface_normal * (n * c1 - c2);
	}

	totally_internally_reflected_out = false;

	//TEMP: to avoid loss of normalistion issues
	assert(::epsEqual(exit_raydir_out.length(), 1.0f, 0.001f));
	exit_raydir_out.normalise();

	const float len = exit_raydir_out.length();
	assert(::epsEqual(len, 1.0f));
	

	//const float n = src_refindex / dest_refindex;

	//const float neg_raydir_on_normal = -dot(incident_raydir, surface_normal);
	//const float c1 = -dot(incident_raydir, surface_normal);

	//const float c2 = sqrt(1.0f - n*n * (1.0f - c1*c1));

	//exit_raydir_out = incident_raydir * n +
	//			surface_normal * (n * c1 - c2);
}*/

double MatUtils::schlickFresnelReflectance(double R_0, double cos_theta)
{
	assert(Maths::inRange(cos_theta, 0.0, 1.0));
	return R_0 + (1.0 - R_0) * MatUtils::pow5(1.0 - cos_theta);
}


/*
double MatUtils::getFresnelReflForCosAngle(double cos_angle_incidence,
				double fresnel_fraction)
{
	assert(cos_angle_incidence >= 0.0f);

	const double a = (1.0 - fresnel_fraction * cos_angle_incidence);

	return a * a;
}

double MatUtils::getReflForCosAngleSchlick(double cos_angle_incidence,
				double normal_reflectance)
{
	const double x = (1.0 - cos_angle_incidence);
	double t = x * x;//t = x^2
	t *= t;//t = x^4
	t *= x;//t = x^5
	return normal_reflectance + t * (1.0 - normal_reflectance);

	//const float t = pow((1.0f - cos_angle_incidence), 5.0f);
	//return normal_reflectance + t * (1.0f - normal_reflectance);
}

void MatUtils::getReflForCosAngleSchlick(double cos_angle_incidence,
				const Colour3& normal_reflectance, Colour3& reflectance_out)
{
	//const double t = pow((1.0f - cos_angle_incidence), 5.0f);
	const double x = (1.0f - cos_angle_incidence);
	double t = x * x;//t = x^2
	t *= t;//t = x^4
	t *= x;//t = x^5

	reflectance_out = normal_reflectance;
	reflectance_out.r += (1.0f - reflectance_out.r) * t;
	reflectance_out.g += (1.0f - reflectance_out.g) * t;
	reflectance_out.b += (1.0f - reflectance_out.b) * t;
}
*/

const Vec2d MatUtils::sampleUnitDisc(const SamplePair& unitsamples)
{
	/*const float r = sqrt(unitsamples.x);
	const float theta = unitsamples.y * NICKMATHS_2PI;
	return Vec2(cos(theta)*r, sin(theta)*r);*/

	Vec2d disc;
	shirleyUnitSquareToDisk(unitsamples, disc);
	return disc;
}

const Vec3d MatUtils::sampleHemisphere(const Basisd& basis, const Vec2d& unitsamples, double& pdf_out)
{
	const double z = unitsamples.x;
	const double theta = unitsamples.y * NICKMATHS_2PI;

	const double r = sqrt(1.0 - z*z);

	Vec3d dir(cos(theta) * r, sin(theta) * r, z);

 	pdf_out = NICKMATHS_RECIP_2PI;

	return basis.transformVectorToParent(dir);
}

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


const Vec3d MatUtils::uniformlySampleSphere(const SamplePair& unitsamples) // returns point on surface of sphere with radius 1
{
	const double z = -1.0 + unitsamples.x * 2.0;
	const double theta = unitsamples.y * NICKMATHS_2PI;

	const double r = sqrt(1.0 - z*z);

	return Vec3d(cos(theta) * r, sin(theta) * r, z);
}



const Vec3d MatUtils::sampleHemisphereCosineWeighted(const Basisd& basis, const SamplePair& unitsamples/*, double& pdf_out*/)
{
	//sample unit disc
	/*const float r = sqrtf(unitsamples.x);
	const float theta = unitsamples.y * NICKMATHS_2PI;

	Vec3d dir(cos(theta)*r, sin(theta)*r, sqrt(1.0f - r*r));

	pdf_out = dir.z * NICKMATHS_RECIP_PI;

	return basis.transformVectorToParent(dir);*/

	Vec2d disc;
	shirleyUnitSquareToDisk(unitsamples, disc);
	const Vec3d dir(disc.x, disc.y, sqrt(myMax(0., 1.0 - (disc.x*disc.x + disc.y*disc.y))));
	assert(dir.isUnitLength());
	//pdf_out = dir.z * NICKMATHS_RECIP_PI;
	return basis.transformVectorToParent(dir);
}
/*const Vec3d MatUtils::sampleHemisphereCosineWeighted(const Vec2d& unitsamples, const Vec3d& normal, float& pdf_out)
{
	Basis basis;
	basis.constructFromVector(normal);

	//sample unit disc
	const float r = sqrtf(unitsamples.x);
	const float theta = unitsamples.y * NICKMATHS_2PI;

	Vec3d dir(cos(theta)*r, sin(theta)*r, sqrt(1.0f - r*r));

	pdf_out = dir.z * NICKMATHS_RECIP_PI;

	return basis.transformVectorToParent(dir);
}*/

// samples a bivariate Gaussian distribution, with mean (0,0) and given standard_deviation
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


//See Monte Carlo Ray Tracing siggraph course 2003 page 33.
const Vec3d MatUtils::sampleSolidAngleCone(const SamplePair& samples, const Basisd& basis, double angle)
{
	assert(angle > 0.0);

	const double phi = samples.x * NICKMATHS_2PI;
	const double alpha = sqrt(samples.y) * angle;

	//const float r = sqrt(unitsamples.x);
	//const float phi = unitsamples.y * NICKMATHS_2PI;

	//Vec3d dir(disc.x*sin(alpha), disc.y*sin(alpha), cos(alpha));
	Vec3d dir(cos(phi)*sin(alpha), sin(phi)*sin(alpha), cos(alpha));

	return basis.transformVectorToParent(dir);
}


double MatUtils::solidAngleConePDF(double angle)
{
	assert(angle > 0.0);

	// The sampling is uniform over the solid angle subtended by the cone.
	const double solid_angle = NICKMATHS_2PI * (1.0 - cos(angle));
	return 1.0 / solid_angle;
}


void MatUtils::conductorFresnelReflectance(const SpectralVector& n, const SpectralVector& k, double cos_incident_angle, SpectralVector& F_R_out)
{
	//NOTE: SSE this up?
	for(unsigned int i=0; i<F_R_out.size(); ++i)
		F_R_out[i] = conductorFresnelReflectance(n[i], k[i], cos_incident_angle);
}


double MatUtils::conductorFresnelReflectance(double n, double k, double cos_theta)
{
	assert(n >= 0.0 && k >= 0.0);
	assert(cos_theta >= 0.0f);

	const double n2_k2 = n*n + k*k;
	const double costheta2 = cos_theta*cos_theta;

	const double r_par = (n2_k2*costheta2 - 2*n*cos_theta + 1.0) / (n2_k2*costheta2 + 2*n*cos_theta + 1.0);
	const double r_perp = (n2_k2 - 2*n*cos_theta + costheta2) / (n2_k2 + 2*n*cos_theta + costheta2);

	return (r_par + r_perp) * 0.5;
}

//NOTE TEMP: one of these is wrong, probably the polarised version

const Vec2d MatUtils::polarisedConductorFresnelReflectance(double n, double k, double cos_theta)
{
	assert(cos_theta >= 0.0f);

	const double n2_k2 = n*n + k*k;
	const double costheta2 = cos_theta*cos_theta;

	const double r_par = (n2_k2*costheta2 - 2*n*cos_theta + 1.0) / (n2_k2*costheta2 + 2*n*cos_theta + 1.0);
	const double r_perp = (n2_k2 - 2*n*cos_theta + costheta2) / (n2_k2 + 2*n*cos_theta + costheta2);

	return Vec2d(r_perp, r_par);
}


double MatUtils::dialetricFresnelReflectance(double n1, double n2, double cos_theta_i)
{
	//get transmitted cos theta using Snell's law
	//http://en.wikipedia.org/wiki/Snell%27s_law

	const double sintheta_i = sqrt(1.0 - cos_theta_i*cos_theta_i);//get sin(theta_i)
	const double sintheta_t = sintheta_i * n1 / n2;//use Snell's law to get sin(theta_t)
	if(sintheta_t >= 1.0)
		return 1.0;//total internal reflection
	const double costheta_t = sqrt(1.0 - sintheta_t*sintheta_t);//get cos(theta_t)

	//Now get the fraction reflected vs refracted with the Fresnel equations: http://en.wikipedia.org/wiki/Fresnel_equations
	const double r_perp = (n1*cos_theta_i - n2*costheta_t) / (n1*cos_theta_i + n2*costheta_t);//R_s in wikipedia, electric field polarised along material surface
	const double r_par = (n2*cos_theta_i - n1*costheta_t) / (n1*costheta_t + n2*cos_theta_i);//R_p in wikipedia, electric field polarised on plane of incidence

	const double r = (r_par*r_par + r_perp*r_perp) * 0.5;
	assert(r >= 0.0 && r <= 1.0);
	return r;
}

const Vec2d MatUtils::polarisedDialetricFresnelReflectance(double n1, double n2, double cos_theta_i)
{
	//get transmitted cos theta using Snell's law
	//http://en.wikipedia.org/wiki/Snell%27s_law

	assert(cos_theta_i >= 0.f && cos_theta_i <= 1.f);

	const double sintheta = sqrt(1.0 - cos_theta_i*cos_theta_i);//get sin(theta_i)
	const double sintheta_t = sintheta * n1 / n2;//use Snell's law to get sin(theta_t)
	if(sintheta_t >= 1.0)
		return Vec2d(1.0, 1.0);//total internal reflection
	const double costheta_t = sqrt(1.0f - sintheta_t*sintheta_t);//get cos(theta_t)

	//Now get the fraction reflected vs refracted with the Fresnel equations: http://en.wikipedia.org/wiki/Fresnel_equations

	//component with electric field lying along interface (on surface)
	const double r_perp = (n1*cos_theta_i - n2*costheta_t) / (n1*cos_theta_i + n2*costheta_t);//R_s in wikipedia, electric field polarised along material surface

	//this is component with electric field in plane of incident, reflected and normal vectors. (plane of incidence)
	const double r_par = (n2*cos_theta_i - n1*costheta_t) / (n1*costheta_t + n2*cos_theta_i);//R_p in wikipedia, electric field polarised on plane of incidence

	//return Vec2d(r_par*r_par, r_perp*r_perp);
	return Vec2d(r_perp*r_perp, r_par*r_par);
}








void MatUtils::unitTest()
{
	conPrint("MatUtils::unitTest()");


	{
		const Vec3d d = sphericalToCartesianCoords(NICKMATHS_PI * (3.0/2.0), cos(NICKMATHS_PI_2), Basisd(Matrix3d::identity()));
		assert(epsEqual(d, Vec3d(0, -1, 0)));
	}

/*
	const int N = 1e7;
	const double recip_N = 1.0 / (double)N;

	// target is integral of cos(x) over 0...1 = sin(1) - sin(0) = sin(1) = 0.017452406437283512819418978516316
	const double target = sin(1.0) * N;
	{
		Timer t;
		double sum = 0.0;
		double sin_x_sum = 0.0;
		for(int i=0; i<N; ++i)
		{
			const double x = (double)i * recip_N;
			const double sin_x = sin(x);
			sin_x_sum += sin_x;
			sum += cos(x);
		}

		const double elapsed = t.getSecondsElapsed();
		const double cycles = 2.4e9 * t.getSecondsElapsed() * recip_N;
		printVar(sin_x_sum);
		printVar(sum);
		printVar(target);
		printVar(elapsed);
		printVar(cycles);
	}
	{
		Timer t;
		double sum = 0.0;
		double sin_x_sum = 0.0;
		for(int i=0; i<N; ++i)
		{
			const double x = (double)i * recip_N;
			const double sin_x = sin(x);
			sin_x_sum += sin_x;
			sum += sqrt(1.0 - sin_x*sin_x);
		}

		const double elapsed = t.getSecondsElapsed();
		const double cycles = 2.4e9 * t.getSecondsElapsed() * recip_N;
		printVar(sin_x_sum);
		printVar(sum);
		printVar(elapsed);
		printVar(cycles);
	}

	{
		Timer t;
		double sum = 0.0;
		double x = 0.0;
		for(int i=0; i<N; ++i)
		{
			//const double x = (double)i * recip_N;
			x += 0.001;
			sum += sin(x);
		}
		const double sin_cycles = 2.4e9 * t.getSecondsElapsed() * recip_N;
		printVar(sum);
		printVar(sin_cycles);
	}

	{
		Timer t;
		double sum = 0.0;
		double x = 0.0;
		for(int i=0; i<N; ++i)
		{
			//const double x = (double)i * recip_N;
			x += 0.001;
			sum += cos(x);
		}
		const double cos_cycles = 2.4e9 * t.getSecondsElapsed() * recip_N;
		printVar(sum);
		printVar(cos_cycles);
	}

	{
		Timer t;
		double sum = 0.0;
		double x = 0.0;
		for(int i=0; i<N; ++i)
		{
			//const double x = (double)i * recip_N;
			x += 0.001;
			sum += sqrt(x);
		}
		const double sqrt_cycles = 2.4e9 * t.getSecondsElapsed() * recip_N;
		printVar(sum);
		printVar(sqrt_cycles);
	}
*/



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



	Basisd basis;
	const Vec3d v = normalise(Vec3d(1,1,1));
	basis.constructFromVector(v);

	const Vec3d res = sampleSolidAngleCone(SamplePair(0.5, 0.5), basis, 0.01f);

	testAssert(dot(v, res) > 0.95);

	double r;
	double n1 = 1.0f;
	double n2 = 1.5f;
	r = dialetricFresnelReflectance(n1, n2, 1.0f);
	r = dialetricFresnelReflectance(n1, n2, 0.8f);
	r = dialetricFresnelReflectance(n1, n2, 0.6f);
	r = dialetricFresnelReflectance(n1, n2, 0.4f);
	r = dialetricFresnelReflectance(n1, n2, 0.2f);
	r = dialetricFresnelReflectance(n1, n2, 0.1f);
	r = dialetricFresnelReflectance(n1, n2, 0.05f);
	r = dialetricFresnelReflectance(n1, n2, 0.01f);
	r = dialetricFresnelReflectance(n1, n2, 0.0f);

	n1 = 1.5f;
	n2 = 1.0f;
	
	r = dialetricFresnelReflectance(n1, n2, 1.0f);
	r = dialetricFresnelReflectance(n1, n2, 0.8f);
	r = dialetricFresnelReflectance(n1, n2, 0.6f);
	r = dialetricFresnelReflectance(n1, n2, 0.4f);
	r = dialetricFresnelReflectance(n1, n2, 0.2f);
	r = dialetricFresnelReflectance(n1, n2, 0.1f);
	r = dialetricFresnelReflectance(n1, n2, 0.05f);
	r = dialetricFresnelReflectance(n1, n2, 0.01f);
	r = dialetricFresnelReflectance(n1, n2, 0.0f);


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











