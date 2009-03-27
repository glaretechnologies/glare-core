#include "Matrix4f.h"


#include "../indigo/TestUtils.h"


Matrix4f::Matrix4f(const float* data)
{
	for(unsigned int i=0; i<16; ++i)
		e[i] = data[i];
}


const Matrix4f Matrix4f::identity()
{
	const float data[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	return Matrix4f(data);
}


void Matrix4f::test()
{
	SSE_ALIGN Matrix4f m = Matrix4f::identity();

	SSE_ALIGN Vec4f v(1, 2, 3, 4);

//	SSE_ALIGN Vec4f res = m * v;


}
