/*=====================================================================
Hilbert.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Fri Nov 26 14:59:10 +1300 2010
=====================================================================*/
#include "Hilbert.h"


#include <vector>
#include "../maths/Matrix2.h"
#include "../utils/stringutils.h"
#include "../indigo/globals.h"


Hilbert::Hilbert()
{

}


Hilbert::~Hilbert()
{

}


// Mirror along x=y
static const Matrix2<int> mirror(const Matrix2<int>& m)
{
	return Matrix2<int>(0, 1, 1, 0) * m;
}


static const Matrix2<int> negMirror(const Matrix2<int>& m)
{
	return Matrix2<int>(0, -1, -1, 0) * m;
}


static int run(int depth, int maxdepth, Matrix2<int> m, int x, int y, int index, std::vector<std::pair<int, int> >& indices_out)
{
	const int step = 1 << (maxdepth - depth);

	int i_x = m.e[0];
	int i_y = m.e[2];
	int j_x = m.e[1];
	int j_y = m.e[3];


	if(depth == maxdepth)
	{
		indices_out.push_back(std::make_pair(x, y));
		indices_out.push_back(std::make_pair(x + j_x, y + j_y));
		indices_out.push_back(std::make_pair(x + i_x + j_x, y + i_y + j_y));
		indices_out.push_back(std::make_pair(x + i_x, y + i_y));
		return index + 4;
	}
	else
	{
		index = run(depth + 1, maxdepth, mirror(m),    x,                                   y,                                   index, indices_out);
		index = run(depth + 1, maxdepth, m,            x + j_x*step,                        y + j_y*step,                        index, indices_out);
		index = run(depth + 1, maxdepth, m,            x + (i_x + j_x)*step,                y + (i_y + j_y)*step,                index, indices_out);
		index = run(depth + 1, maxdepth, negMirror(m), x + (2*i_x + j_x)*step - i_x - j_x,  y + (2*i_y + j_y)*step - i_y - j_y,  index, indices_out);
		return index;
	}
}


void Hilbert::generate(int maxdepth, std::vector<std::pair<int, int> >& indices_out)
{
	run(
		0, // depth
		maxdepth,
		Matrix2<int>::identity(), // direction
		0, // x
		0, // y
		0, // index
		indices_out
	);
}


void Hilbert::test()
{
	std::vector<std::pair<int, int> > indices;
	run(
		0,
		2,
		Matrix2<int>::identity(), // direction
		0, // x
		0, // y
		0, // index
		indices
	);

	for(int i=0; i<indices.size(); ++i)
	{
		conPrint("index " + ::toString(i) + ": " + toString(indices[i].first) + ", " + toString(indices[i].second));
	}
}
