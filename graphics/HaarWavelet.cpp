/*=====================================================================
HaarWavelet.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2011-07-27 23:09:38 +0100
=====================================================================*/
#include "HaarWavelet.h"


#include "../maths/mathstypes.h"


/*

6	8	4	6	2	6	4	8

7	5	4	6	-1	-1	-2	-2

6	5	1	-1	-1	-1	-2	-2

5.5	0.5	1	-1	-1	-1	-2	-2


x = (a + b)/C
y = (a - b)/C

xC = a + b
yC = a - b

xC + yC = 2a
a = (xC + yC)/2
a = (x + y)C/2

xC- yC = 2b
b = (xC - yC)/2
b = (x - y)C/2

*/

void HaarWavelet::transform(const std::vector<Real>& v, std::vector<Real>& out)
{
	assert(Maths::isPowerOfTwo(v.size()));

	out.resize(v.size());

	std::vector<float> temp = v;

	const Real f = 1 / std::sqrt((Real)2);

	size_t N = v.size();
	while(N > 1)
	{
		for(size_t i=0; i<N/2; ++i)
		{
			float average = (temp[i*2] + temp[i*2+1]) * f;
			float diff = (temp[i*2] - temp[i*2+1]) * f;

			out[i] = average;
			out[i + N/2] = diff;
		}

		for(size_t i=0; i<N; ++i)
			temp[i] = out[i];

		N = N/2;
	}
}


void HaarWavelet::transform(const Array2d<Real>& a, Array2d<Real>& out)
{
	assert(Maths::isPowerOfTwo(a.getWidth()));
	assert(Maths::isPowerOfTwo(a.getHeight()));

	out.resize(a.getWidth(), a.getHeight());

	std::vector<float> temp(myMax(a.getWidth(), a.getHeight()));

	// Transform rows
	for(size_t y=0; y<a.getHeight(); ++y)
	{
		size_t N = a.getWidth();

		for(size_t i=0; i<N; ++i)
			temp[i] = a.elem(i, y);

		const Real f = 1 / std::sqrt((Real)2);

		while(N > 1)
		{
			for(size_t i=0; i<N/2; ++i)
			{
				float average = (temp[i*2] + temp[i*2+1]) * f;
				float diff = (temp[i*2] - temp[i*2+1]) * f;

				out.elem(i, y) = average;
				out.elem(i + N/2, y) = diff;
			}

			for(size_t i=0; i<N/2; ++i)
				temp[i] = out.elem(i, y);

			N = N/2;
		}
	}

	// Transform columns
	for(size_t x=0; x<a.getWidth(); ++x)
	{
		size_t N = a.getHeight();

		for(size_t i=0; i<N; ++i)
			temp[i] = a.elem(x, i);

		const Real f = 1 / std::sqrt((Real)2);

		while(N > 1)
		{
			for(size_t i=0; i<N/2; ++i)
			{
				float average = (temp[i*2] + temp[i*2+1]) * f;
				float diff = (temp[i*2] - temp[i*2+1]) * f;

				out.elem(x, i) = average;
				out.elem(x, i + N/2) = diff;
			}

			for(size_t i=0; i<N/2; ++i)
				temp[i] = out.elem(x, i);

			N = N/2;
		}
	}
}


#if BUILD_TESTS


void HaarWavelet::test()
{
	float v[] = {	6,	8,	4,	6,	2,	6,	4,	8 };

	
	std::vector<float> vec(8);
	for(int i=0; i<8; ++i)
		vec[i] = v[i];

	std::vector<float> out;

	transform(vec, out);

}


#endif



