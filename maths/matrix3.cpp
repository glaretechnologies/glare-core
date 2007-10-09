/*===================================================================

  
  digital liberation front 2001
  
  _______    ______      _______
 /______/\  |______|    /\______\  
|       \ \ |      |   / /       |    
|	      \| |      |  |/         |  
|_____    \ |      |_ /    ______|       
 ____|    | |      |_||    |_____          
     |____| |________||____|                
           



Code by Nicholas Chapman
nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may not use this code for any commercial project.
====================================================================*/
#include "matrix3.h"

#include <stdio.h>
#include "Matrix2.h"
#include "mathstypes.h"
#include "../indigo/TestUtils.h"

//#include <windows.h>//just for some stupid definition in gl.h
//#include <GL/gl.h>
//#include <GL/glu.h>

/*void Matrix3::print() const
{
	printf("%1.2f	%1.2f	%1.2f\n", e[0], e[1], e[2]);
	printf("%1.2f	%1.2f	%1.2f\n", e[3], e[4], e[5]);
	printf("%1.2f	%1.2f	%1.2f\n", e[6], e[7], e[8]);
}

void Matrix3::writeToFile(FILE* f) const
{
	for(int i=0; i<9; i++)
	{
		fwrite(&e[i], sizeof(float), 1, f);
	}
}

void Matrix3::readFromFile(FILE* f)
{
	for(int i=0; i<9; i++)
	{
		fread(&e[i], sizeof(float), 1, f);
	}
}*/
/*
void Matrix3::rotAroundAxis(const Vec3& axis, float angle)
{
	//-----------------------------------------------------------------
	//build the rotation matrix

	//see http://mathworld.wolfram.com/RodriguesRotationFormula.html
	//-----------------------------------------------------------------
	Matrix3 m;
	const float a = axis.x;
	const float b = axis.y;
	const float c = axis.z;

	const float cost = cos(angle);
	const float sint = sin(angle);

	const float asint = a*sint;
	const float bsint = b*sint;
	const float csint = c*sint;


	float one_minus_cost = 1 - cost;

	m.setColumn0(
		Vec3(a*a*one_minus_cost + cost, a*b*one_minus_cost + csint, a*c*one_minus_cost - bsint) );

	m.setColumn1(
		Vec3(a*b*one_minus_cost - csint, b*b*one_minus_cost + cost, b*c*one_minus_cost + asint) );
	
	m.setColumn2(
		Vec3(a*c*one_minus_cost + bsint, b*c*one_minus_cost - asint, c*c*one_minus_cost + cost) );

	//-----------------------------------------------------------------
	//multiply this matrix by the rotation matrix
	//-----------------------------------------------------------------
	*this *= m;
}




Vec3 Matrix3::getAngles(const Vec3& ws_forwards, const Vec3& ws_up, const Vec3& ws_right) const
{
	//-----------------------------------------------------------------
	//transform the ws_forwards vector by this matrix
	//-----------------------------------------------------------------
	const Vec3 ws_forwards_transformed = *this * ws_forwards;

	return ws_forwards_transformed.getAngles(ws_forwards, ws_up, ws_right);
}
*/

/*const Vec3 Matrix3::getAngles() const //around i, j, k
{
	//NASTY SLOW HACK:
	const Vec3 ws_forwards_transformed = *this * ws_forwards;



}*/
#if 0

void Matrix3::setToAngles(const Vec3& angles, const Vec3& ws_forwards, const Vec3& ws_up, const Vec3& ws_right)
{
	//-----------------------------------------------------------------
	//get an identity matrix
	//-----------------------------------------------------------------
	//Matrix3 m;
	//m.init();
	init();//set this matrix to the identity matrix

	//-----------------------------------------------------------------
	//rotate it by angles.yaw
	//-----------------------------------------------------------------
	rotAroundAxis(ws_up, angles.getYaw());

	rotAroundAxis(ws_right, /*-TEMP NO MINUS*/angles.getPitch());//do pitch

	rotAroundAxis(ws_forwards, /*-TEMP NO MINUS*/angles.getRoll());//finally, do roll

	//-----------------------------------------------------------------
	//rotate it by angles.yaw
	//-----------------------------------------------------------------
	/*m.rotAroundAxis(Vec3::ws_up, angles.getYaw());

	//-----------------------------------------------------------------
	//find the vector orthogonal to the forwards vector define by angles
	//that lies on the ws_right-ws_forwards plane
	//-----------------------------------------------------------------
	Vec3 angles_right = m * Vec3::ws_right;

	//-----------------------------------------------------------------
	//rotate the matrix around this new vector by angles.pitch
	//-----------------------------------------------------------------
	m.rotAroundAxis(angles_right, angles.getPitch());

	//-----------------------------------------------------------------
	//find the forwarsds vector defined by angles.
	//-----------------------------------------------------------------
	Vec3 angles_forwards = m * Vec3::ws_forwards;

	//-----------------------------------------------------------------
	//rotate the matrix around this forwards vec by angles.roll
	//-----------------------------------------------------------------
	m.rotAroundAxis(angles_forwards, angles.getRoll());*/

	//*this = m;

	//NOTE: this is ultra slow :)
}
#endif


/*void Matrix3::openGLMult() const
{
	float ogl_matrix[16];

	ogl_matrix[0] = e[0];
	ogl_matrix[1] = e[1];
	ogl_matrix[2] = e[2];

	ogl_matrix[3] = 0;

	ogl_matrix[4] = e[3];
	ogl_matrix[5] = e[4];
	ogl_matrix[6] = e[5];

	ogl_matrix[7] = 0;

	ogl_matrix[8] = e[6];
	ogl_matrix[9] = e[7];
	ogl_matrix[10] = e[8];

	ogl_matrix[11] = 0;

	ogl_matrix[12] = ogl_matrix[13] = ogl_matrix[14] = 0;

	ogl_matrix[15] = 1;

#ifndef DEDICATED_SERVER
	glMultMatrixf(ogl_matrix);
#endif
}*/

//const Matrix3 Matrix3::identity = Matrix3(Vec3(1, 0, 0), Vec3(0, 1, 0), Vec3(0, 0, 1));

/*
const Matrix3 Matrix3::buildMatrixFromRows(const Vec3& r0, 
							const Vec3& r1, const Vec3& r2)
{
	return Matrix3(Vec3(r0.x, r1.x, r2.x), Vec3(r0.y, r1.y, r2.y), Vec3(r0.z, r1.z, r2.z));
}


*/

/*void Matrix3::invert()
{
	Matrix3 r = Matrix3::identity();

	for(r=0; r<3; ++r)
	{
		int c = r;
		elem(r, 0) /= elem(r, c);
		elem(r, 1) /= elem(r, c);
		elem(r, 2) /= elem(r, c);

		r.elem(r, 0) /= elem(r, c);
		r.elem(r, 1) /= elem(r, c);
		r.elem(r, 2) /= elem(r, c);

		if(r < 2)
		{
			//subtract from next row
			elem(r+1, 0) -= elem(r, 0);
			elem(r+1, 1) -= elem(r, 1);
			elem(r+1, 2) -= elem(r, 2);

			elem(r+1, 0) -= elem(r, 0);
			elem(r+1, 1) -= elem(r, 1);
			elem(r+1, 2) -= elem(r, 2);

}*/

template <>
void Matrix3<float>::test()
{
	const float e[9] = {1,2,3,-4,5,6,7,-8,9};
	Matrix3<float> m(e);
	testAssert(epsEqual((float)m.determinant(), 240.f));

	const float a_e[9] = {3,1,-4,2,5,6,1,4,8};
	Matrix3<float> a(a_e);
	testAssert(epsEqual(a.cofactor(0,0), 16.f));
	testAssert(epsEqual(a.cofactor(2,1), -26.f));

	Matrix3<float> a_inverse;
	bool invertible = a.inverse(a_inverse);
	testAssert(invertible);
	testAssert(epsEqual(a * a_inverse, Matrix3<float>::identity(), (float)NICKMATHS_EPSILON));
	testAssert(epsEqual(a_inverse * a, identity(), (float)NICKMATHS_EPSILON));

	Matrix3<float> identity_inverse;
	invertible = identity().inverse(identity_inverse);
	testAssert(invertible);
	testAssert(epsEqual(identity(), identity_inverse, (float)NICKMATHS_EPSILON));
}










