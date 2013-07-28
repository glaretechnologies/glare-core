#include "ImageErrorMetric.h"


#include "image.h"
#include "Image4f.h"


// See http://en.wikipedia.org/wiki/Root-mean-square_deviation
float ImageErrorMetric::rootMeanSquaredError(const Image4f& a, const Image4f& b)
{
	assert(a.getWidth() == b.getWidth() && a.getHeight() == b.getHeight());

	double r_variance = 0.0;
	double g_variance = 0.0;
	double b_variance = 0.0;
	for(unsigned int i=0; i<a.numPixels(); ++i)
	{
		// NOTE: we will ignore the alpha channel.

		r_variance += Maths::square(a.getPixel(i).x[0] - b.getPixel(i).x[0]);
		g_variance += Maths::square(a.getPixel(i).x[1] - b.getPixel(i).x[1]);
		b_variance += Maths::square(a.getPixel(i).x[2] - b.getPixel(i).x[2]);
	}
	const double r_stddev = std::sqrt(r_variance / (double)a.numPixels());
	const double g_stddev = std::sqrt(g_variance / (double)a.numPixels());
	const double b_stddev = std::sqrt(b_variance / (double)a.numPixels());

	return (float)(r_stddev + g_stddev + b_stddev);
}
