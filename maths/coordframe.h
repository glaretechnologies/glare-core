/*===================================================================

  
  digital liberation front 2001
  
  _______    ______      _______
 /______/\  |______|    /\______\  
|       \ \ |      |   / /       |    
|        \| |      |  |/         |  
|_____    \ |      |_ /    ______|       
 ____|    | |      |_||    |_____          
     |____| |________||____|                
           



Code by Nicholas Chapman
nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may not use this code for any commercial project.
====================================================================*/
#ifndef __COORDFRAME_H__
#define __COORDFRAME_H__


// A coordinate frame (basis and origin) with respect to a parent

//
//http://www.gamasutra.com/features/19990702/data_structures_05.htm
#include "vec3.h"
#include "basis.h"

template <class Real>
class CoordFrame
{
public:
	inline CoordFrame(){}
	inline CoordFrame(const Vec3<Real>& origin);
	inline CoordFrame(const CoordFrame& rhs);
	inline CoordFrame(const Vec3<Real>& origin, const Basis<Real>& basis);
	inline ~CoordFrame(){}

	inline void init();

	inline CoordFrame& operator = (const CoordFrame& rhs);



	inline const Vec3<Real> transformPointToLocal(const Vec3<Real>& p) const;

	inline const Vec3<Real> transformPointToParent(const Vec3<Real>& p) const;

	inline const Vec3<Real> transformVecToParent(const Vec3<Real>& vec) const;

	inline const Vec3<Real> transformVecToLocal(const Vec3<Real>& vec) const;


	inline void translate(const Vec3<Real>& translation);

	Vec3<Real>& getOrigin(){ return origin; }
	const Vec3<Real>& getOrigin() const { return origin; }

	Basis<Real>& getBasis(){ return basis; }
	const Basis<Real>& getBasis() const { return basis; }


	inline const CoordFrame operator * (const CoordFrame& rhs) const;

	//inline void setOpenGLMatrix(float* ogl_matrix_out) const;
	//void openGLMult() const;
	void getMatrixData(Real* matrix_out) const;

	void invert();

	inline bool isIdentity() const;//i.e. no rotation and at origin

private:
	Vec3<Real> origin;
	Basis<Real> basis;
};


template <class Real>
CoordFrame<Real>::CoordFrame(const Vec3<Real>& origin_, const Basis<Real>& basis_)
:	origin(origin_), basis(basis_)
{}
	
template <class Real>
inline CoordFrame<Real>::CoordFrame(const CoordFrame& rhs)
{
	*this = rhs;
}

template <class Real>
inline CoordFrame<Real>::CoordFrame(const Vec3<Real>& origin_)
{
	origin = origin_;
	//basis.init();
}


template <class Real>
inline CoordFrame<Real>& CoordFrame<Real>::operator = (const CoordFrame& rhs)
{
	origin = rhs.origin;
	basis = rhs.basis;
	return *this;
}

template <class Real>
inline void CoordFrame<Real>::init()
{
	origin.zero();
	basis.init();
}

template <class Real>
const Vec3<Real> CoordFrame<Real>::transformPointToLocal(const Vec3<Real>& p) const
{
	//translate to this frame’s origin, then project onto this basis

	return basis.transformVectorToLocal(p - origin);
}


template <class Real>
const Vec3<Real> CoordFrame<Real>::transformPointToParent(const Vec3<Real>& p) const
{
	//transform the coordinate vector and translate by this origin

	return basis.transformVectorToParent(p) + origin;
}

template <class Real>
const Vec3<Real> CoordFrame<Real>::transformVecToParent(const Vec3<Real>& vec) const
{
	return basis.transformVectorToParent(vec);
}

template <class Real>
const Vec3<Real> CoordFrame<Real>::transformVecToLocal(const Vec3<Real>& vec) const
{
	return basis.transformVectorToLocal(vec);
}


template <class Real>
void CoordFrame<Real>::translate(const Vec3<Real>& translation)
{
	origin += translation; 
}

//here rhs is the parent coord frame
template <class Real>
const CoordFrame<Real> CoordFrame<Real>::operator * (const CoordFrame& rhs) const
{
	//FIXME
	return CoordFrame(rhs.transformVecToParent(origin) + rhs.getOrigin(), 
								basis.getThisBasisWRTParent(rhs.basis));
}




template <class Real>
bool CoordFrame<Real>::isIdentity() const//i.e. no rotation and at origin
{
	return origin.x == 0 && origin.y == 0 && origin.z == 0
				&& basis.getMatrix().isIdentity();
}



/*void CoordFrame<Real>::setOpenGLMatrix(float* ogl_matrix_out) const
{
	//NOTE: check this
	
//	0 1 2

//	3 4 5

/	6 7 8
	

	//opengl format:
	//0 4 8 12
	//1 5 9 13
	//2 6 1014	
	//3 7 1115

	ogl_matrix_out[0] = basis.e[0];
	ogl_matrix_out[1] = basis.e[3];
	ogl_matrix_out[2] = basis.e[6];

	ogl_matrix_out[3] = 0;

	ogl_matrix_out[4] = basis.e[1];
	ogl_matrix_out[5] = basis.e[4];
	ogl_matrix_out[6] = basis.e[7];

	ogl_matrix_out[7] = 0;

	ogl_matrix_out[8] = basis.e[2];
	ogl_matrix_out[9] = basis.e[5];
	ogl_matrix_out[10] = basis.e[8];

	ogl_matrix_out[11] = 0;

	ogl_matrix_out[12] = origin.x;
	ogl_matrix_out[13] = origin.y;
	ogl_matrix_out[14] = origin.z;

	ogl_matrix_out[15] = 1;*/

	/*ogl_matrix_out[0] = basis.e[0];
	ogl_matrix_out[1] = basis.e[1];
	ogl_matrix_out[2] = basis.e[2];

	ogl_matrix_out[3] =	origin.x;

	ogl_matrix_out[4] = basis.e[3];
	ogl_matrix_out[5] = basis.e[4];
	ogl_matrix_out[6] = basis.e[5];

	ogl_matrix_out[7] = origin.y;

	ogl_matrix_out[8] = basis.e[6];
	ogl_matrix_out[9] = basis.e[7];
	ogl_matrix_out[10] = basis.e[8];

	ogl_matrix_out[11] = origin.z;

	ogl_matrix_out[12] = ogl_matrix_out[13] = ogl_matrix_out[14] = 0;

	ogl_matrix_out[15] = 1;*/

//}

typedef CoordFrame<float> CoordFramef;
typedef CoordFrame<double> CoordFramed;


#endif //__COORDFRAME_H__
