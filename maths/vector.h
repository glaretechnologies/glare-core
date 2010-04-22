#ifndef __VECTOR_H__
#define __VECTOR_H__


#include "../maths/mathstypes.h"
#include <cassert>
#include <vector>


template <class Real>
class Vector
{
public:
	inline Vector(int dimension);
	inline Vector(int dimension, Real initial_value);
	inline Vector();
	inline Vector(int dimension, const Real* data);
	inline Vector(const Vector& rhs);

	inline ~Vector();

	//aliases:
//	inline int getDim() const { return dimension; }
	inline int size() const { return data.size(); }
//	inline int getSize() const { return dimension; }

	inline Real operator[] (int index) const;
	inline Real& operator[] (int index);

	inline Vector& operator = (const Vector& rhs);
	inline bool operator == (const Vector& rhs) const;


	inline Vector operator - (const Vector& rhs) const;
	inline Vector operator + (const Vector& rhs) const;

	inline Vector& operator -= (const Vector& rhs);
	inline Vector& operator += (const Vector& rhs);

	//inline Vector& addFirstNElements(const Vector& rhs, int N);

	inline Vector operator * (Real factor) const;

	inline Vector& operator *= (Real factor);
	inline Vector& scale(Real factor);//same thing

	inline Vector operator / (Real factor) const;
	inline Vector& operator /= (Real factor);

	inline Vector& normalise();
	inline Real length() const;
	inline Real length2() const;
	inline Real dotProduct(const Vector& rhs) const;

	inline Real angle(const Vector& rhs) const;
	inline Real angleNormized(const Vector& rhs) const;

	inline Vector& zero();

	inline const Real* toReal() const;
	inline Real* toReal();

	typedef typename std::vector<Real>::iterator iterator;
	inline iterator begin();
	inline iterator end();

	typedef typename std::vector<Real>::const_iterator const_iterator;
	inline const_iterator const_begin() const;
	inline const_iterator const_end() const;

	inline Real sum() const;
	inline Real averageValue() const;
	inline Real variance() const;

	inline Real Min() const;
	inline Real Max() const;

	//static void test();

private:
	std::vector<Real> data;
};


template <class Real>
Vector<Real>::Vector(int dimension_)
:	data(dimension_)
{
}


template <class Real>
Vector<Real>::Vector(int dimension_, Real initial_value)
:	data(dimension_, initial_value)
{
}

template <class Real>
Vector<Real>::Vector(const Vector& rhs)
{
	data = rhs.data;
}

template <class Real>
Vector<Real>::Vector()
{
}

template <class Real>
Vector<Real>::Vector(int dimension_, const Real* newdata)
{
	assert(newdata);
	data.resize(dimension_);

	for(int i=0; i<dimension_; i++)
		data[i] = newdata[i];
}


template <class Real>
Vector<Real>::~Vector()
{
}


template <class Real>
Real Vector<Real>::operator[] (int index) const
{
	//assert(index >= 0 && index < dimension);
	
	return data[index];
}

template <class Real>
Real& Vector<Real>::operator[] (int index)
{
	return data[index];
}

template <class Real>
Vector<Real>& Vector<Real>::operator = (const Vector& rhs)
{
	if(this == &rhs)
		return *this;

	data = rhs.data;
	return *this;
}




template <class Real>
bool Vector<Real>::operator == (const Vector& rhs) const
{
	if(data.size() != rhs.size())
		return false;

	for(int i=0; i<data.size(); ++i)
		if(data[i] != rhs.data[i])
			return false;

	return true;
}


// template <class Real>
// Vector<Real> Vector<Real>::operator - (const Vector& rhs) const
// {
// 	assert(dimension == rhs.dimension);
// 	Vector newvec(dimension);
// 
// 	for(int i=0; i<dimension; i++)
// 		newvec[i] = data[i] - rhs.data[i];
// 
// 	return newvec;
// }


// template <class Real>
// Vector<Real> Vector<Real>::operator + (const Vector& rhs) const
// {
// 	assert(dimension == rhs.dimension);
// 	Vector newvec(dimension);
// 
// 	for(int i=0; i<dimension; i++)
// 		newvec[i] = data[i] + rhs.data[i];
// 
// 	return newvec;
// }


// template <class Real>
// Vector<Real>& Vector<Real>::operator -= (const Vector& rhs)
// {
// 	assert(dimension == rhs.dimension);
// 
// 	for(int i=0; i<dimension; i++)
// 		data[i] -= rhs.data[i];
// 
// 	return *this;
// }


// template <class Real>
// Vector<Real>& Vector<Real>::operator += (const Vector& rhs)
// {
// 	assert(dimension == rhs.dimension);
// 
// 	for(int i=0; i<dimension; i++)
// 		data[i] += rhs.data[i];
// 
// 	return *this;
// }


/*
template <class Real>
Vector<Real>& Vector<Real>::addFirstNElements(const Vector& rhs, int N)
{
	assert(N >= 0 && N < dimension);

	for(int i=0; i<N; i++)
		data[i] += rhs.data[i];

	return *this;
}
*/
/*
template <class Real>
Vector<Real> Vector<Real>::operator * (Real factor) const
{
	Vector newvec(dimension);

	for(int i=0; i<dimension; i++)
		newvec[i] = data[i] * factor;

	return newvec;
}
*/


template <class Real>
Vector<Real>& Vector<Real>::operator *= (Real factor)
{
	for(int i=0; i<data.size(); i++)
		data[i] *= factor;

	return *this;
}


template <class Real>
inline Vector<Real>& Vector<Real>::scale(Real factor)
{
	*this *= factor;

	return *this;
}


/*
template <class Real>
Vector<Real> Vector<Real>::operator / (Real factor) const
{
	Vector newvec(dimension);

	Real recip_factor = 1.0 / factor;

	for(int i=0; i<dimension; i++)
		newvec[i] = data[i] * recip_factor;

	return newvec;
}


template <class Real>
Vector<Real>& Vector<Real>::operator /= (Real factor)
{
	Real recip_factor = 1.0 / factor;

	for(int i=0; i<dimension; i++)
		data[i] *= recip_factor;

	return *this;
}


template <class Real>
Vector<Real>& Vector<Real>::normalise()
{
	*this /= length();

	return *this;
}

template <class Real>
Real Vector<Real>::length() const
{
	Real length = 0;

	for(int i=0; i<dimension; i++)
		length += data[i]*data[i];

	return (Real)sqrt(length);
}

template <class Real>
Real Vector<Real>::length2() const
{
	Real length2 = 0;

	for(int i=0; i<dimension; i++)
		length2 += data[i]*data[i];

	return length2;
}*/
/*


template <class Real>
Real Vector<Real>::dotProduct(const Vector& rhs) const
{
	Real dot;

	assert(dimension == rhs.dimension);

	for(int i=0; i<dimension; i++)
		dot += data[i] + rhs.data[i];

	return dot;
}

template <class Real>
Real Vector<Real>::angle(const Vector& rhs) const
{
	Vector thisnormized(*this);
	thisnormized.normalise();

	Vector rhsnormized(rhs);
	rhsnormized.normalise();

	return thisnormized.angleNormized(rhsnormized);
}*/
/*
template <class Real>
Real Vector<Real>::angleNormized(const Vector& rhs) const
{
	return acos( this->dotProduct(rhs) );
}

template <class Real>
Vector<Real>& Vector<Real>::zero()
{
	for(int i=0; i<dimension; i++)
		data[i] = 0;

	return *this;
}


template <class Real>
const Real* Vector<Real>::toReal() const
{
	return data;
}

template <class Real>
Real* Vector<Real>::toReal()
{
	return data;
}
*/


template <class Real>
typename Vector<Real>::iterator Vector<Real>::begin() 
{
	return data.begin();
}


template <class Real>
typename Vector<Real>::iterator Vector<Real>::end()
{
	return data.end();
}


template <class Real>
typename Vector<Real>::const_iterator Vector<Real>::const_begin() const
{
	return data.begin();
}


template <class Real>
typename Vector<Real>::const_iterator Vector<Real>::const_end() const
{
	return data.end();
}


/*
template <class Real>
Real Vector<Real>::sum() const
{
	Real s = 0.0;
	for(int i=0; i<dimension; ++i)
		s += data[i];
	return s;
}

template <class Real>
Real Vector<Real>::averageValue() const
{
	return sum() / (Real)dimension;
}


template <class Real>
Real Vector<Real>::variance() const
{
	const Real expected = averageValue();
	
	Real diffsum = 0.0;
	for(int i=0; i<dimension; ++i)
		diffsum += (data[i] - expected) * (data[i] - expected);
	
	return diffsum / (Real)dimension;
}
*/
/*
template <class Real>
Real Vector<Real>::Min() const
{
	assert(dimension > 0);
	Real x = data[0];
	for(int i=1; i<dimension; ++i)
		x = myMin(x, data[i]);
	return x;
}


template <class Real>
Real Vector<Real>::Max() const
{
	assert(dimension > 0);
	Real x = data[0];
	for(int i=1; i<dimension; ++i)
		x = myMax(x, data[i]);
	return x;
}

*/


#endif //__VECTOR_H__
