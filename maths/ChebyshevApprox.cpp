/*=====================================================================
ChebyshevApprox.cpp
-------------------
Copyright Glare Technologies Limited 2016 -
Generated at 2016-04-19 12:08:37 +0100
=====================================================================*/
#include "ChebyshevApprox.h"



ChebyshevApprox::ChebyshevApprox()
{

}


ChebyshevApprox::~ChebyshevApprox()
{

}


#if BUILD_TESTS


#include "TestUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/Plotter.h"


template <class Func>
void testOnFunction(Func func, const std::string& func_name)
{
	std::vector<Plotter::DataSet> data;
	data.resize(2);
	data[0].key = "Reference";
	data[1].key = "Chebyshev Approx";

	ChebyshevApprox cheb;
	cheb.build(func);
	
	const int N = 100;
	for(int i=0; i<N; ++i)
	{
		const float x = (float)i/(N-1);
		const float fx = func(x);
		data[0].points.push_back(Vec2f(x, fx));

		const float cheb_fx = cheb.eval(x);
		data[1].points.push_back(Vec2f(x, cheb_fx));
	}

	Plotter::PlotOptions options;

	Plotter::plot(
		"chebyshev_" + func_name + ".png",
		"chebyshev" + func_name,
		"x",
		"y",
		data,
		options
	);
}


struct x_sqrd
{
	float operator() (float x) { return x*x; }
};

struct linear
{
	float operator() (float x) { return x; }
};

struct constant
{
	float operator() (float x) { return 0.5f; }
};

void ChebyshevApprox::test()
{
	testOnFunction(constant(), "constant");
	testOnFunction(linear(), "linear");
	testOnFunction(x_sqrd(), "x_sqrd");
}


#endif
