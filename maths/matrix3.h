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
#ifndef __MATRIX3_H__
#define __MATRIX3_H__

#include "vec3.h"
#include "Matrix2.h"


#include <stdio.h>//for readFromFile(), writeToFile()

template <class Real>
class Matrix3
{
public:
	inline Matrix3(){}
	inline Matrix3(const Matrix3& rhs);
	inline Matrix3(const Vec3<Real>& column0, const Vec3<Real>& column1, const Vec3<Real>& column2);
	inline Matrix3(const Real* entries);

	//NOTE: fixme
	/*================================================================================
	Matrix3
	-------
	Builds a rotation matrix from some axis rotations.

	A rotation around an axis specifies a rotation counter-clockwise when looking along the
	axis towards the origin.  
	All rotation angles are in radians.
	================================================================================*/
	inline Matrix3(Real rot_around_xaxis, Real rot_around_y_axis, Real rot_around_z_axis);

	inline ~Matrix3();

	static const Matrix3 buildMatrixFromRows(const Vec3<Real>& row0, 
							const Vec3<Real>& row1, const Vec3<Real>& row2);


	inline void set(const Matrix3& rhs);
	inline void set(const Vec3<Real>& column0, const Vec3<Real>& column1, const Vec3<Real>& column2);
	inline void set(const Real* newentries);
	inline void set(Real rot_around_xaxis, Real rot_around_y_axis, Real rot_around_z_axis);

	inline void init(){ *this = identity(); }

	inline Matrix3& operator = (const Matrix3& rhs);
	inline bool operator == (const Matrix3& rhs) const;


	inline Matrix3& operator += (const Matrix3& rhs);
	inline Matrix3& operator -= (const Matrix3& rhs);
	inline Matrix3& operator *= (const Matrix3& rhs);

	inline const Matrix3 operator + (const Matrix3& rhs) const;
	inline const Matrix3 operator - (const Matrix3& rhs) const;
	//return value = this * rhs;
	inline const Matrix3 operator * (const Matrix3& rhs) const;
	

	
	//return value = this * rhs;
	inline Vec3<Real> operator * (const Vec3<Real>& rhs) const;
	inline Vec3<Real> transposeMult(const Vec3<Real>& rhs) const;


	/*================================================================================
	rotAroundAxis
	-------------
	rotates the matrix around the axis specified.

	The rotation is counter-clockwise when looking along the axis towards the origin.  
	All rotation angles are in radians.

	The specified rotation axis should have length 1
	================================================================================*/
	//void rotAroundAxis(const Vec3<Real>& axis, Real angle);
	static const Matrix3 rotationMatrix(const Vec3<Real>& unit_axis, Real angle);


	
	inline const Matrix3 getTranspose() const;
	inline void transpose();
	
	//-----------------------------------------------------------------
	//this assumes matrix is orthogonal. ie. returns transpose
	//-----------------------------------------------------------------
	//inline Matrix3 getInverse() const;
	//void invert();


	//-----------------------------------------------------------------
	//orthognalise matrix and then normalise each column vector
	//-----------------------------------------------------------------
	inline void orthoganalise();

	inline Vec3<Real> getXUnitVector() const;
	inline Vec3<Real> getYUnitVector() const;
	inline Vec3<Real> getZUnitVector() const;

	inline Vec3<Real> getColumn0() const;// == GetXUnitVector
	inline Vec3<Real> getColumn1() const;// == GetYUnitVector
	inline Vec3<Real> getColumn2() const;// == GetZUnitVector

	inline void setColumn0(const Vec3<Real>& v);
	inline void setColumn1(const Vec3<Real>& v);
	inline void setColumn2(const Vec3<Real>& v);

	inline Vec3<Real> getRow0() const;
	inline Vec3<Real> getRow1() const;
	inline Vec3<Real> getRow2() const;

	Vec3<Real> getAngles(const Vec3<Real>& ws_forwards, const Vec3<Real>& ws_up, const Vec3<Real>& ws_right) const;


	/*========================================================================
	getYPRAngles
	---------
	returns the vector (yaw, pitch, roll) where yaw pitch roll are as defined
	in Vec3.h
	========================================================================*/
	//const Vec3 getYPRAngles() const; //around i, j, k

	
	/*========================================================================
	setToAngles
	-----------
	??????????????

	//where angles is (yaw, pitch, roll), relative to ws_*.
	yaw is a counterclockwise rotation around ws_up, and is applied first.
	The basis is then rotated counterclockwise by the pitch around the new forwards vector.
	The basis is then rotated counterclockwise by the roll around the new right vector.
	========================================================================*/






	void setToAngles(const Vec3<Real>& angles, const Vec3<Real>& ws_forwards, const Vec3<Real>& ws_up, const Vec3<Real>& ws_right);

	/*float getYaw() const;
	float getPitch() const;
	float getRoll() const;*/

	
	void print() const;
	//void writeToFile(FILE* f) const;
	//void readFromFile(FILE* f);

	inline const static Matrix3<Real> identity();
	void openGLMult() const;
	inline bool isIdentity() const;
	void scale(Real factor);
	Real determinant() const;
	bool inverse(Matrix3& inverse_out) const;
	bool invert();
	const Matrix3 adjoint() const;
	const Matrix3 cofactorMatrix() const;
	Real cofactor(unsigned int i, unsigned int j) const;

	inline Real& elem(unsigned int i, unsigned int j);
	inline Real elem(unsigned int i, unsigned int j) const;

	const std::string toString() const;

	static void test();


	//-----------------------------------------------------------------
	//
	//-----------------------------------------------------------------

	Real e[9];//entries
	/*
	0 1 2

	3 4 5

	6 7 8
	*/
};




template <class Real>
Matrix3<Real>::Matrix3(const Matrix3<Real>& rhs)
{
	set(rhs);
}

template <class Real>
Matrix3<Real>::Matrix3(const Vec3<Real>& column0, const Vec3<Real>& column1, const Vec3<Real>& column2)
{
	set(column0, column1, column2);
}


template <class Real>
Matrix3<Real>::Matrix3(const Real* entries)
{
	set(entries);
}

template <class Real>
Matrix3<Real>::Matrix3(Real rot_around_xaxis, Real rot_around_y_axis, Real rot_around_z_axis)
{
	set(rot_around_xaxis, rot_around_y_axis, rot_around_z_axis);
}




template <class Real>
Matrix3<Real>::~Matrix3()
{}

template <class Real>
Real& Matrix3<Real>::elem(unsigned int i, unsigned int j)
{
	assert(i < 3);
	assert(j < 3);
	return e[i*3 + j];
}

template <class Real>
Real Matrix3<Real>::elem(unsigned int i, unsigned int j) const
{
	assert(i < 3);
	assert(j < 3);
	return e[i*3 + j];
}

template <class Real>
const Matrix3<Real> Matrix3<Real>::identity()
{
	//Matrix3(const Vec3& column0, const Vec3& column1, const Vec3& column2);
	return Matrix3<Real>(Vec3<Real>(1,0,0), Vec3<Real>(0,1,0), Vec3<Real>(0,0,1));
}

template <class Real>
void Matrix3<Real>::set(const Matrix3& rhs)
{
	//NOTE: which is faster, this or a for loop?
	e[0] = rhs.e[0];
	e[1] = rhs.e[1];
	e[2] = rhs.e[2];
	e[3] = rhs.e[3];
	e[4] = rhs.e[4];
	e[5] = rhs.e[5];
	e[6] = rhs.e[6];
	e[7] = rhs.e[7];
	e[8] = rhs.e[8];
}



template <class Real>
void Matrix3<Real>::set(const Vec3<Real>& column0, const Vec3<Real>& column1, const Vec3<Real>& column2)
{
	e[0] = column0.x;
	e[3] = column0.y;
	e[6] = column0.z;

	e[1] = column1.x;
	e[4] = column1.y;
	e[7] = column1.z;

	e[2] = column2.x;
	e[5] = column2.y;
	e[8] = column2.z;
}

template <class Real>
void Matrix3<Real>::set(const Real* newentries)
{
	e[0] = newentries[0];
	e[1] = newentries[1];
	e[2] = newentries[2];
	e[3] = newentries[3];
	e[4] = newentries[4];
	e[5] = newentries[5];
	e[6] = newentries[6];
	e[7] = newentries[7];
	e[8] = newentries[8];
}


template <class Real>
void Matrix3<Real>::set(Real rot_around_xaxis, Real rot_around_y_axis, Real rot_around_z_axis)
{
	//DO some magic here :)

	const Real A       = cos(rot_around_xaxis);
	const Real B       = sin(rot_around_xaxis);
	const Real C       = cos(rot_around_y_axis);
	const Real D       = sin(rot_around_y_axis);
	const Real E       = cos(rot_around_z_axis);
	const Real F       = sin(rot_around_z_axis);

	const  Real AD      =   A * D;
	const Real BD      =   B * D;

	//fill in top row
    e[0]  =   C * E;
    e[1]  =  -C * F;
    e[2]  =  -D;

	//fill in middle row
    e[3]  = -BD * E + A * F;
    e[4]  =  BD * F + A * E;
    e[5]  =  -B * C;

	//fill in bottom row
    e[6]  =  AD * E + B * F;
    e[7]  = -AD * F + B * E;
    e[8] =   A * C;

	//NOTE: check this works

}

template <class Real>
void Matrix3<Real>::scale(Real factor)
{
	for(int i=0; i<9; ++i)
		e[i] *= factor;
}



template <class Real>
Matrix3<Real>& Matrix3<Real>::operator = (const Matrix3& rhs)
{
	set(rhs);

	return *this;
}



template <class Real>
bool Matrix3<Real>::operator == (const Matrix3& rhs) const
{
	return (e[0] == rhs.e[0] &&
			e[1] == rhs.e[1] &&
			e[2] == rhs.e[2] &&
			e[3] == rhs.e[3] &&
			e[4] == rhs.e[4] &&
			e[5] == rhs.e[5] &&
			e[6] == rhs.e[6] &&
			e[7] == rhs.e[7] &&
			e[8] == rhs.e[8]);
}
			

template <class Real>
void Matrix3<Real>::orthoganalise()
{
	//Do some magic
}





template <class Real>
Vec3<Real> Matrix3<Real>::getXUnitVector() const
{
	return Vec3<Real>(e[0], e[3], e[6]);
}
	
template <class Real>
Vec3<Real> Matrix3<Real>::getYUnitVector() const
{
	return Vec3<Real>(e[1], e[4], e[7]);
}

template <class Real>
Vec3<Real> Matrix3<Real>::getZUnitVector() const
{
	return Vec3<Real>(e[2], e[5], e[8]);
}




template <class Real>
Vec3<Real> Matrix3<Real>::getColumn0() const
{
	return getXUnitVector();
}

template <class Real>
Vec3<Real> Matrix3<Real>::getColumn1() const
{
	return getYUnitVector();
}

template <class Real>
Vec3<Real> Matrix3<Real>::getColumn2() const
{
	return getZUnitVector();
}


template <class Real>
void Matrix3<Real>::setColumn0(const Vec3<Real>& v)
{
	e[0] = v.x;
	e[3] = v.y;
	e[6] = v.z;
}

template <class Real>
void Matrix3<Real>::setColumn1(const Vec3<Real>& v)
{
	e[1] = v.x;
	e[4] = v.y;
	e[7] = v.z;
}

template <class Real>
void Matrix3<Real>::setColumn2(const Vec3<Real>& v)
{
	e[2] = v.x;
	e[5] = v.y;
	e[8] = v.z;
}


template <class Real>
Vec3<Real> Matrix3<Real>::getRow0() const
{
	return Vec3<Real>(e[0], e[1], e[2]);
}

template <class Real>
Vec3<Real> Matrix3<Real>::getRow1() const
{
	return Vec3<Real>(e[3], e[4], e[5]);
}

template <class Real>
Vec3<Real> Matrix3<Real>::getRow2() const
{
	return Vec3<Real>(e[6], e[7], e[8]);
}


/*
Vec3 Matrix3::getAngles() const
{
   // float angle_y = D = -asin( e[2]);
    float C =  cos( angle_y );

	float angle_x;
	float angle_z;
	float trx_;
	float try_;

    if(fabs( angle_y ) > 0.0005 )
	{
		trx_      =  e[8] / C;
		try_      = -e[5]  / C;

		angle_x  = atan2( try_, trx_ );

		trx_      =  e[0] / C;
		try_      = -e[1] / C;

		angle_z  = atan2( try_, trx_);
	}
    else
	{
		angle_x  = 0;

		trx_      = e[4];
		try_      = e[3];

		angle_z  = atan2( try_, trx_ );
	}

	//-----------------------------------------------------------------
	//put the angles between -Pi and Pi
	//-----------------------------------------------------------------
	while(angle_x < -NICK_PI)
		angle_x += 2*NICK_PI;
	while(angle_x > NICK_PI)
		angle_x -= 2*NICK_PI;

	while(angle_y < -NICK_PI)
		angle_y += 2*NICK_PI;
	while(angle_y > NICK_PI)
		angle_y -= 2*NICK_PI;

	while(angle_z < -NICK_PI)
		angle_z += 2*NICK_PI;
	while(angle_z > NICK_PI)
		angle_z -= 2*NICK_PI;

	return Vec3(angle_x, angle_y, angle_z);

}*/

/*
float Matrix3::getYaw() const
{
	//magic
}
	
float Matrix3::getPitch() const
{

}


float Matrix3::getRoll() const
{

}*/








	

template <class Real>
Vec3<Real> Matrix3<Real>::operator * (const Vec3<Real>& rhs) const
{
	return Vec3<Real>(
		rhs.x*e[0] + rhs.y*e[1] + rhs.z*e[2], 
		rhs.x*e[3] + rhs.y*e[4] + rhs.z*e[5],
		rhs.x*e[6] + rhs.y*e[7] + rhs.z*e[8]
	);
	//return transformVectorToLocal(rhs);
	//return Vec3<Real>(dot(getRow0(),rhs), dot(getRow1(), rhs), dot(getRow2(), rhs));
}

template <class Real>
Vec3<Real> Matrix3<Real>::transposeMult(const Vec3<Real>& rhs) const
{
	return Vec3<Real>(
		rhs.x*e[0] + rhs.y*e[3] + rhs.z*e[6], 
		rhs.x*e[1] + rhs.y*e[4] + rhs.z*e[7],
		rhs.x*e[2] + rhs.y*e[5] + rhs.z*e[8]
	);
}





//NOTE: make me un-inline?
template <class Real>
const Matrix3<Real> Matrix3<Real>::operator * (const Matrix3& rhs) const
{
	//return Matrix3(e[0]*rhs.e[0] + e[1]*e[3] + e[2]*e[6],	e[0]*	

	//NOTE: checkme

	const Real r0 = dotProduct(getRow0(), rhs.getColumn0());
	const Real r1 = dotProduct(getRow0(), rhs.getColumn1());
	const Real r2 = dotProduct(getRow0(), rhs.getColumn2());
	const Real r3 = dotProduct(getRow1(), rhs.getColumn0());
	const Real r4 = dotProduct(getRow1(), rhs.getColumn1());
	const Real r5 = dotProduct(getRow1(), rhs.getColumn2());
	const Real r6 = dotProduct(getRow2(), rhs.getColumn0());
	const Real r7 = dotProduct(getRow2(), rhs.getColumn1());
	const Real r8 = dotProduct(getRow2(), rhs.getColumn2());
	
	return Matrix3(Vec3<Real>(r0, r3, r6), Vec3<Real>(r1, r4, r7), Vec3<Real>(r2, r5, r8));

}



template <class Real>
Matrix3<Real>& Matrix3<Real>::operator *= (const Matrix3& rhs)
{
	const Real r0 = dotProduct(getRow0(), rhs.getColumn0());
	const Real r1 = dotProduct(getRow0(), rhs.getColumn1());
	const Real r2 = dotProduct(getRow0(), rhs.getColumn2());
	const Real r3 = dotProduct(getRow1(), rhs.getColumn0());
	const Real r4 = dotProduct(getRow1(), rhs.getColumn1());
	const Real r5 = dotProduct(getRow1(), rhs.getColumn2());
	const Real r6 = dotProduct(getRow2(), rhs.getColumn0());
	const Real r7 = dotProduct(getRow2(), rhs.getColumn1());
	const Real r8 = dotProduct(getRow2(), rhs.getColumn2());

	e[0] = r0;
	e[1] = r1;
	e[2] = r2;
	e[3] = r3;
	e[4] = r4;
	e[5] = r5;
	e[6] = r6;
	e[7] = r7;
	e[8] = r8;

	return *this;
}




template <class Real>
Matrix3<Real>& Matrix3<Real>::operator += (const Matrix3& rhs)
{
	e[0] += rhs.e[0];
	e[1] += rhs.e[1];
	e[2] += rhs.e[2];
	e[3] += rhs.e[3];
	e[4] += rhs.e[4];
	e[5] += rhs.e[5];
	e[6] += rhs.e[6];
	e[7] += rhs.e[7];
	e[8] += rhs.e[8];

	return *this;
}

template <class Real>
Matrix3<Real>& Matrix3<Real>::operator -= (const Matrix3& rhs)
{
	e[0] -= rhs.e[0];
	e[1] -= rhs.e[1];
	e[2] -= rhs.e[2];
	e[3] -= rhs.e[3];
	e[4] -= rhs.e[4];
	e[5] -= rhs.e[5];
	e[6] -= rhs.e[6];
	e[7] -= rhs.e[7];
	e[8] -= rhs.e[8];

	return *this;
}




template <class Real>
const Matrix3<Real> Matrix3<Real>::operator + (const Matrix3& rhs) const
{
	return Matrix3( getColumn0() + rhs.getColumn0(), 
					getColumn1() + rhs.getColumn1(),
					getColumn2() + rhs.getColumn2());
}

template <class Real>
const Matrix3<Real> Matrix3<Real>::operator - (const Matrix3& rhs) const
{
	return Matrix3( getColumn0() - rhs.getColumn0(), 
					getColumn1() - rhs.getColumn1(),
					getColumn2() - rhs.getColumn2());
}


template <class Real>
const Matrix3<Real> Matrix3<Real>::getTranspose() const
{
	//-----------------------------------------------------------------
	//make a new matrix using the rows as the new columns
	//-----------------------------------------------------------------
	return Matrix3( getRow0(), getRow1(), getRow2());
}



template <class Real>
void Matrix3<Real>::transpose()
{
	Matrix3 temp = getTranspose();
	set(temp);
}





/*Matrix3 Matrix3::getInverse() const
{
	return getTranspose();
}*/
/*
void Matrix3::invert()
{
	Matrix3 temp = getTranspose();
	set(temp);
}
*/



template <class Real>
bool Matrix3<Real>::isIdentity() const//i.e. no rotation and at origin
{
	return getColumn0() == Vec3<Real>(1,0,0) && 
			getColumn1() == Vec3<Real>(0,1,0) &&
			getColumn2() == Vec3<Real>(0,0,1);
}

//assumes v is a row vector.
template <class Real>
inline const Vec3<Real> operator * (const Vec3<Real>& v, const Matrix3<Real>& m)
{
	return Vec3<Real>(dot(v, m.getColumn0()), dot(v, m.getColumn1()), dot(v, m.getColumn2())); 
}

template <class Real>
inline bool epsMatrixEqual(const Matrix3<Real>& a, const Matrix3<Real>& b, Real eps = NICKMATHS_EPSILON)
{
	for(unsigned int i=0; i<9; ++i)
		if(!epsEqual(a.e[i], b.e[i], eps))
			return false;
	return true;
}



/*Matrix3 Matrix3::identity()
{
	return Matrix3( Vec3(1, 0, 0), Vec3(0, 1, 0), Vec3(0, 0, 1) );
}*/


//see Anton page 83
template <class Real>
Real Matrix3<Real>::determinant() const
{
	return elem(0,0)*elem(1,1)*elem(2,2) + elem(0,1)*elem(1,2)*elem(2,0) + elem(0,2)*elem(1,0)*elem(2,1)
		- elem(0,2)*elem(1,1)*elem(2,0) - elem(0,1)*elem(1,0)*elem(2,2)  - elem(0,0)*elem(1,2)*elem(2,1);
}

template <class Real>
bool Matrix3<Real>::inverse(Matrix3& inverse_out) const
{
	const Real d = determinant();
	if(fabs(d) < 1.e-9)
		return false;//singular matrix

	inverse_out = adjoint();
	inverse_out.scale((Real)1.0 / d);
	return true;
}

template <class Real>
bool Matrix3<Real>::invert()
{
	Matrix3 inv;
	const bool invertible = inverse(inv);
	if(invertible)
		*this = inv;
	return invertible;
}

template <class Real>
const Matrix3<Real> Matrix3<Real>::adjoint() const
{
	return cofactorMatrix().getTranspose();
}

template <class Real>
const Matrix3<Real> Matrix3<Real>::cofactorMatrix() const
{
	Matrix3 c;
	for(unsigned int i=0; i<3; ++i)
		for(unsigned int j=0; j<3; ++j)
			c.elem(i, j) = cofactor(i, j);
	return c;
}

template <class Real>
Real Matrix3<Real>::cofactor(unsigned int i, unsigned int j) const
{
	Matrix2<Real> submatrix;
	for(unsigned int y=0, sub_i=0; y<3; ++y)
	{
		if(y != i)
		{
			for(unsigned int x=0, sub_j=0; x<3; ++x)
			{
				if(x != j)
				{
					submatrix.elem(sub_i, sub_j) = elem(y, x);
					sub_j++;
				}
			}
			sub_i++;
		}
	}
	return submatrix.determinant() * ((i + j) % 2 == 0 ? 1.f : -1.f);
}

/*template <class Real>
void Matrix3<Real>::rotAroundAxis(const Vec3<Real>& axis, Real angle)
{
	//-----------------------------------------------------------------
	//build the rotation matrix

	//see http://mathworld.wolfram.com/RodriguesRotationFormula.html
	//-----------------------------------------------------------------
	Matrix3<Real> m;
	const Real a = axis.x;
	const Real b = axis.y;
	const Real c = axis.z;

	const Real cost = cos(angle);
	const Real sint = sin(angle);

	const Real asint = a*sint;
	const Real bsint = b*sint;
	const Real csint = c*sint;


	Real one_minus_cost = 1.0 - cost;

	m.setColumn0(
		Vec3<Real>(a*a*one_minus_cost + cost, a*b*one_minus_cost + csint, a*c*one_minus_cost - bsint) );

	m.setColumn1(
		Vec3<Real>(a*b*one_minus_cost - csint, b*b*one_minus_cost + cost, b*c*one_minus_cost + asint) );
	
	m.setColumn2(
		Vec3<Real>(a*c*one_minus_cost + bsint, b*c*one_minus_cost - asint, c*c*one_minus_cost + cost) );

	//-----------------------------------------------------------------
	//multiply this matrix by the rotation matrix
	//-----------------------------------------------------------------
	*this *= m;
}*/


template <class Real>
const Matrix3<Real> Matrix3<Real>::rotationMatrix(const Vec3<Real>& unit_axis, Real angle)
{
	assert(unit_axis.isUnitLength());

	//-----------------------------------------------------------------
	//build the rotation matrix
	//see http://mathworld.wolfram.com/RodriguesRotationFormula.html
	//-----------------------------------------------------------------
	const Real a = unit_axis.x;
	const Real b = unit_axis.y;
	const Real c = unit_axis.z;

	const Real cost = std::cos(angle);
	const Real sint = std::sin(angle);

	const Real asint = a*sint;
	const Real bsint = b*sint;
	const Real csint = c*sint;

	const Real one_minus_cost = (Real)1.0 - cost;

	return Matrix3<Real>(
		Vec3<Real>(a*a*one_minus_cost + cost, a*b*one_minus_cost + csint, a*c*one_minus_cost - bsint), // column 0
		Vec3<Real>(a*b*one_minus_cost - csint, b*b*one_minus_cost + cost, b*c*one_minus_cost + asint), // column 1
		Vec3<Real>(a*c*one_minus_cost + bsint, b*c*one_minus_cost - asint, c*c*one_minus_cost + cost) // column 2
		);
}


template <class Real>
inline const std::string toString(const Matrix3<Real>& x)
{
	return x.toString();
}


typedef Matrix3<float> Matrix3f;
typedef Matrix3<double> Matrix3d;




#endif //__MATRIX3_H__
