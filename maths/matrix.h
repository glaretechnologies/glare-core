#ifndef __MATRIX_H__
#define __MATRIX_H__

#include "vec3.h"
#include "matrix3.h"

class Matrix
{
public:
	Matrix(){}
	inline Matrix(const Matrix& rhs);
	inline Matrix(const Vec3& column0, const Vec3& column1, const Vec3& column2, const Vec3& pos);

	~Matrix(){}

	/*========================================================================
	init
	----
	sets rot to the identity matrix, sets pos to zero.
	========================================================================*/
	inline void init();

	inline Matrix& operator = (const Matrix& rhs);


	inline Matrix& invert();

	/*========================================================================
	setOpenGLMatrix
	---------------
	Writes the matrix in OpenGL format to the array 'ogl_matrix_out'.
	========================================================================*/	
	inline void setOpenGLMatrix(float* ogl_matrix_out) const;



	Vec3 pos;
	Matrix3 rot;
};




Matrix::Matrix(const Matrix& rhs)
{
	*this = rhs;
}

Matrix::Matrix(const Vec3& column0, const Vec3& column1, const Vec3& column2, const Vec3& pos_)
:	rot(column0, column1, column2),
	pos(pos_)
{}

void Matrix::init()
{
	rot = Matrix3::identity();
	pos.zero();
}

Matrix& Matrix::operator = (const Matrix& rhs)
{
	rot = rhs.rot;
	pos = rhs.pos;

	return *this;
}


Matrix& Matrix::invert()
{
	rot.invert();
	pos.scale(-1);

	return *this;
}





void Matrix::setOpenGLMatrix(float* ogl_matrix_out) const
{
	//NOTE: check this
	/*
	0 1 2

	3 4 5

	6 7 8
	*/

	//opengl format:
	//0 4 8 12
	//1 5 9 13
	//2 6 1014	
	//3 7 1115

	/*ogl_matrix_out[0] = rot.e[0];
	ogl_matrix_out[1] = rot.e[3];
	ogl_matrix_out[2] = rot.e[6];

	ogl_matrix_out[3] = 0;

	ogl_matrix_out[4] = rot.e[1];
	ogl_matrix_out[5] = rot.e[4];
	ogl_matrix_out[6] = rot.e[7];

	ogl_matrix_out[7] = 0;

	ogl_matrix_out[8] = rot.e[2];
	ogl_matrix_out[9] = rot.e[5];
	ogl_matrix_out[10] = rot.e[8];

	ogl_matrix_out[11] = 0;

	ogl_matrix_out[12] = pos.x;
	ogl_matrix_out[13] = pos.y;
	ogl_matrix_out[14] = pos.z;

	ogl_matrix_out[15] = 1;*/

	ogl_matrix_out[0] = rot.e[0];
	ogl_matrix_out[1] = rot.e[1];
	ogl_matrix_out[2] = rot.e[2];

	ogl_matrix_out[3] = pos.x;

	ogl_matrix_out[4] = rot.e[3];
	ogl_matrix_out[5] = rot.e[4];
	ogl_matrix_out[6] = rot.e[5];

	ogl_matrix_out[7] = pos.y;

	ogl_matrix_out[8] = rot.e[6];
	ogl_matrix_out[9] = rot.e[7];
	ogl_matrix_out[10] = rot.e[8];

	ogl_matrix_out[11] = pos.z;

	ogl_matrix_out[12] = ogl_matrix_out[13] = ogl_matrix_out[14] = 0;

	ogl_matrix_out[15] = 1;

}

















#endif //__MATRIX_H__