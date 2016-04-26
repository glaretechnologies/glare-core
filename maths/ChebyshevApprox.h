/*=====================================================================
ChebyshevApprox.h
-------------------
Copyright Glare Technologies Limited 2016 -
Generated at 2016-04-19 12:08:37 +0100
=====================================================================*/
#pragma once


#include "mathstypes.h"
#include "../utils/Timer.h"
#include <assert.h>


/*=====================================================================
ChebyshevApprox
-------------------

=====================================================================*/
class ChebyshevApprox
{
public:
	ChebyshevApprox();
	~ChebyshevApprox();

	template <class T> inline void build(T func);

	inline float eval(float x) const;

	static void test();
	
	//float s[6]; // Coefficients
	float t0, t1, t2, t3, t4, t5;
};


float ChebyshevApprox::eval(float x) const
{
	//return t0 + t1*x + t2*x*x + t3*x*x*x + t4*x*x*x*x + t5*x*x*x*x*x;
	//return t0 + x*(t1 + x*(t2 + x*(t3 + x*(t4 + x*t5))));
	const float x2 = x*x;
	const float x4 = x2*x2;
	return t0 + t1*x + t2*x2 + t3*x2*x + t4*x4 + t5*x4*x;
}


template <class T> 
void ChebyshevApprox::build(T func)
{
	// Compute innner product with Chebyshev polynomials.
	Timer timer;
	const int N = 1024;
	double s0 = 0; double s1 = 0; double s2 = 0; double s3 = 0; double s4 = 0; double s5 = 0;

	for(int j=0; j<=N; ++j)
	{
		//const float x = (float)j / (N-1);
		//const float theta = x * Maths::pi<float>();
		const float theta_j = (j + 0.5f) * Maths::pi<float>() / (N + 1);
		const float cos_theta = std::cos(theta_j); // cos(theta_j) will be in [-1, 1]
		const float x = 0.5f*cos_theta + 0.5f; // Map to [0, 1]

		const float fx = func(x);

		// http://www.siam.org/books/ot99/OT99SampleChapter.pdf eqn 3.55, and 3.56.
		// 3.21
		// See http://en.wikipedia.org/wiki/De_Moivre's_formula also
		// T_0(x_j) = T_0(cos(theta_j)) = 1
		// T_1(x_j) = T_1(cos(theta_j)) = cos(theta_j)
		// T_2(x_j) = T_2(cos(theta_j)) = 2*cos(theta_j)^2 - 1
		// etc..
		// Note: the method in 3.56 has more cosine calls (one for each k as well as one for each j), but the paper says more numerically stable.  Use?
		const float T_0 = 1; // cos(0*theta);
		const float T_1 = cos_theta; // cos(1*theta);
		const float T_2 = 2 * cos_theta*cos_theta - 1; // cos(2*theta);
		const float T_3 = 4 * cos_theta*cos_theta*cos_theta - 3*cos_theta; // cos(3*theta);
		const float T_4 = 8 * cos_theta*cos_theta*cos_theta*cos_theta - 8*cos_theta*cos_theta + 1; // cos(4*theta);
		const float T_5 = 16 * cos_theta*cos_theta*cos_theta*cos_theta*cos_theta - 20*cos_theta*cos_theta*cos_theta + 5*cos_theta; // cos(5*theta);

		/*const float T_0 = 1;
		const float T_1 = 2*x - 1;
		const float T_2 = 8*x*x - 8*x + 1;
		const float T_3 = 32*x*x*x - 48*x*x + 18*x - 1;
		const float T_4 = 128*x*x*x*x - 256*x*x*x + 160*x*x - 32*x + 1;
		const float T_5 = 512*x*x*x*x*x - 1280*x*x*x*x + 1120*x*x*x - 400*x*x + 50*x - 1;*/

		assert(epsEqual(T_0,  std::cos(0*theta_j)));
		assert(epsEqual(T_1,  std::cos(1*theta_j)));
		assert(epsEqual(T_2,  std::cos(2*theta_j)));
		assert(epsEqual(T_3,  std::cos(3*theta_j)));
		assert(epsEqual(T_4,  std::cos(4*theta_j)));
		assert(epsEqual(T_5,  std::cos(5*theta_j)));

		s0 += fx*T_0;
		s1 += fx*T_1;
		s2 += fx*T_2;
		s3 += fx*T_3;
		s4 += fx*T_4;
		s5 += fx*T_5;
	}

	//// Get the function in terms of the shifted Chebyshev polynomials, then group by power of x.
	//// So t_i is the coefficient of x^i.
	//// See 'Chebyshev Expansions', http://www.siam.org/books/ot99/OT99SampleChapter.pdf, eqn 3.37.

	// Compute coefficients
	const float c0 = (float)(0.5 * s0 * 2 / (N + 1)); // c0 has a 0.5 factor, see eqn 3.99.
	const float c1 = (float)(s1 * 2 / (N + 1));
	const float c2 = (float)(s2 * 2 / (N + 1));
	const float c3 = (float)(s3 * 2 / (N + 1));
	const float c4 = (float)(s4 * 2 / (N + 1));
	const float c5 = (float)(s5 * 2 / (N + 1));

	// Group by power of x
	t0 = (float)(c0 - c1 + c2 - c3 + c4 - c5);
	t1 = (float)(2*c1 - 8*c2 + 18*c3 - 32*c4 + 50*c5);
	t2 = (float)(8*c2 - 48*c3 + 160*c4 - 400*c5);
	t3 = (float)(32*c3 - 256*c4 + 1120*c5);
	t4 = (float)(128*c4 - 1280*c5);
	t5 = (float)(512*c5);
}
