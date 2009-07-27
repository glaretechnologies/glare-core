#include "ImageErrorMetric.h"


#include "image.h"


float ImageErrorMetric::standardDeviation(const Image& a, const Image& b)
{
	assert(a.getWidth() == b.getWidth() && a.getHeight() == b.getHeight());

	double r_variance = 0.0;
	double g_variance = 0.0;
	double b_variance = 0.0;
	for(unsigned int i=0; i<a.numPixels(); ++i)
	{
		r_variance += Maths::square(a.getPixel(i).r - b.getPixel(i).r);
		g_variance += Maths::square(a.getPixel(i).g - b.getPixel(i).g);
		b_variance += Maths::square(a.getPixel(i).b - b.getPixel(i).b);
	}
	const double r_stddev = std::sqrt(r_variance / (double)a.numPixels());
	const double g_stddev = std::sqrt(g_variance / (double)a.numPixels());
	const double b_stddev = std::sqrt(b_variance / (double)a.numPixels());

	return (float)(r_stddev + g_stddev + b_stddev);
}
