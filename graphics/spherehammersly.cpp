/*=====================================================================
spherehammersly.cpp
-------------------
File created by ClassTemplate on Tue Dec 14 18:10:55 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "spherehammersly.h"


#include "../maths/vec2.h"
#include "../maths/vec3.h"


SphereHammersly::SphereHammersly(int maxnum_)
:	maxnum(maxnum_)
{
	k = 0;

	recip_maxnum = 1.0f / (float)maxnum;
}


SphereHammersly::~SphereHammersly()
{
	
}

//adapted from http://www.cse.cuhk.edu.hk/~ttwong/papers/udpoint/udpoint.pdf

void SphereHammersly::genNPoints(int n, std::vector<Vec3>& points_out)
{
	for(int k=0, pos=0; k<n; k++)
	{
		float t = 0.0f;
		int kk;
		float p;
		for(p=0.5f, kk=k; kk; p*=0.5f, kk>>=1)
			if(kk & 1) // kk mod 2 == 1
				t += p;

		t = 2.0f * t - 1.0f; // map from [0,1] to [-1,1]
		const float phi = ((float)k + 0.5f) / (float)n; // a slight shift
		const float phirad = phi * 2.0f * NICKMATHS_PI; // map to [0, 2 pi)
		const float st = sqrt(1.0f-t*t);

		points_out.push_back(Vec3(st * cos(phirad), st * sin(phirad), t));
	}
}

void SphereHammersly::getNext(Vec3& point_out)
{
	float t = 0.0f;
	int kk;
	float p;
	for(p=0.5f, kk=k; kk; p*=0.5f, kk>>=1)
		if(kk & 1) // kk mod 2 == 1
			t += p;

	t = 2.0f * t - 1.0f; // map from [0,1] to [-1,1]
	const float phi = ((float)k + 0.5f) / (float)maxnum; // a slight shift
	const float phirad = phi * 2.0f * NICKMATHS_PI; // map to [0, 2 pi)
	const float st = sqrt(1.0f-t*t);

	point_out.set(st * cos(phirad), st * sin(phirad), t);

	k++;
}


void SphereHammersly::getNext(Vec2& point_out)
{
	float t = 0.0f;
	int kk;
	float p;
	for(p=0.5f, kk=k; kk; p*=0.5f, kk>>=1)
		if(kk & 1) // kk mod 2 == 1
			t += p;

	//t lies in range [0, 1)
	//k lies in range [0, maxnum)

	point_out.set(t, ((float)k + 0.5f) * recip_maxnum);

	k++;
}



