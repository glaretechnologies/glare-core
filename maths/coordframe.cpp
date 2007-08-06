#include "coordframe.h"



#include <windows.h>
#include <GL/gl.h>










void CoordFrame::openGLMult() const
{

	float ogl_matrix[16];


	ogl_matrix[0] = basis.getMatrix().e[0];
	ogl_matrix[1] = basis.getMatrix().e[3];
	ogl_matrix[2] = basis.getMatrix().e[6];

	ogl_matrix[3] = 0;

	ogl_matrix[4] = basis.getMatrix().e[1];
	ogl_matrix[5] = basis.getMatrix().e[4];
	ogl_matrix[6] = basis.getMatrix().e[7];

	ogl_matrix[7] = 0;

	ogl_matrix[8] = basis.getMatrix().e[2];
	ogl_matrix[9] = basis.getMatrix().e[5];
	ogl_matrix[10] = basis.getMatrix().e[8];

	ogl_matrix[11] = 0;

	ogl_matrix[12] = origin.x;
	ogl_matrix[13] = origin.y;
	ogl_matrix[14] = origin.z;

	ogl_matrix[15] = 1;

#ifndef DEDICATED_SERVER
	glMultMatrixf(ogl_matrix);
#else
	assert(0);
#endif

}

void CoordFrame::getMatrixData(float* matrix_out) const
{
	matrix_out[0] = basis.getMatrix().e[0];
	matrix_out[1] = basis.getMatrix().e[3];
	matrix_out[2] = basis.getMatrix().e[6];

	matrix_out[3] = 0;

	matrix_out[4] = basis.getMatrix().e[1];
	matrix_out[5] = basis.getMatrix().e[4];
	matrix_out[6] = basis.getMatrix().e[7];

	matrix_out[7] = 0;

	matrix_out[8] = basis.getMatrix().e[2];
	matrix_out[9] = basis.getMatrix().e[5];
	matrix_out[10] = basis.getMatrix().e[8];

	matrix_out[11] = 0;

	matrix_out[12] = origin.x;
	matrix_out[13] = origin.y;
	matrix_out[14] = origin.z;

	matrix_out[15] = 1;
}


void CoordFrame::invert()
{
	origin *= -1, 
		
	basis.invert();

	//origin.set(0,0,0);//TEMP
}
