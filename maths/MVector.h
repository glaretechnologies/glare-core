#ifndef __VECTOR_H__
#define __VECTOR_H__


#include "../maths/mathstypes.h"
#include <cassert>
#include <vector>


template <class Real>
class MVector
{
public:
	inline MVector(size_t dimension);
	inline MVector(size_t dimension, Real initial_value);
	inline MVector();
	inline MVector(size_t dimension, const Real* data);
	inline MVector(const MVector& rhs);

	inline ~MVector();

	//aliases:
//	inline int getDim() const { return dimension; }
	inline int size() const { return data.size(); }
//	inline int getSize() const { return dimension; }

	inline Real operator[] (size_t index) const;
	inline Real& operator[] (size_t index);

	inline MVector& operator = (const MVector& rhs);
	inline bool operator == (const MVector& rhs) const;


	inline MVector operator - (const MVector& rhs) const;
	inline MVector operator + (const MVector& rhs) const;

	inline MVector& operator -= (const MVector& rhs);
	inline MVector& operator += (const MVector& rhs);

	//inline MVector& addFirstNElements(const Vector& rhs, int N);

	inline MVector operator * (Real factor) const;

	inline MVector& operator *= (Real factor);
	inline MVector& scale(Real factor);//same thing

	inline MVector operator / (Real factor) const;
	inline MVector& operator /= (Real factor);

	inline MVector& normalise();
	inline Real length() const;
	inline Real length2() const;
	inline Real dotProduct(const MVector& rhs) const;

	inline Real angle(const MVector& rhs) const;
	inline Real angleNormized(const MVector& rhs) const;

	inline MVector& zero();

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
MVector<Real>::MVector(size_t dimension_)
:	data(dimension_)
{
}


template <class Real>
MVector<Real>::MVector(size_t dimension_, Real initial_value)
:	data(dimension_, initial_value)
{
}

template <class Real>
MVector<Real>::MVector(const MVector& rhs)
{
	data = rhs.data;
}

template <class Real>
MVector<Real>::MVector()
{
}

template <class Real>
MVector<Real>::MVector(size_t dimension_, const Real* newdata)
{
	assert(newdata);
	data.resize(dimension_);

	for(size_t i=0; i<dimension_; i++)
		data[i] = newdata[i];
}


template <class Real>
MVector<Real>::~MVector()
{
}


template <class Real>
Real MVector<Real>::operator[] (size_t index) const
{
	//assert(index >= 0 && index < dimension);
	
	return data[index];
}

template <class Real>
Real& MVector<Real>::operator[] (size_t index)
{
	return data[index];
}

template <class Real>
MVector<Real>& MVector<Real>::operator = (const MVector& rhs)
{
	if(this == &rhs)
		return *this;

	data = rhs.data;
	return *this;
}




template <class Real>
bool MVector<Real>::operator == (const MVector& rhs) const
{
	if(data.size() != rhs.size())
		return false;

	for(size_t i=0; i<data.size(); ++i)
		if(data[i] != rhs.data[i])
			return false;

	return true;
}


// template <class Real>
// MVector<Real> MVector<Real>::operator - (const MVector& rhs) const
// {
// 	assert(dimension == rhs.dimension);
// 	MVector newvec(dimension);
// 
// 	for(int i=0; i<dimension; i++)
// 		newvec[i] = data[i] - rhs.data[i];
// 
// 	return newvec;
// }


// template <class Real>
// MVector<Real> MVector<Real>::operator + (const MVector& rhs) const
// {
// 	assert(dimension == rhs.dimension);
// 	MVector newvec(dimension);
// 
// 	for(int i=0; i<dimension; i++)
// 		newvec[i] = data[i] + rhs.data[i];
// 
// 	return newvec;
// }


// template <class Real>
// MVector<Real>& MVector<Real>::operator -= (const MVector& rhs)
// {
// 	assert(dimension == rhs.dimension);
// 
// 	for(int i=0; i<dimension; i++)
// 		data[i] -= rhs.data[i];
// 
// 	return *this;
// }


// template <class Real>
// MVector<Real>& MVector<Real>::operator += (const MVector& rhs)
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
MVector<Real>& MVector<Real>::addFirstNElements(const MVector& rhs, int N)
{
	assert(N >= 0 && N < dimension);

	for(int i=0; i<N; i++)
		data[i] += rhs.data[i];

	return *this;
}
*/
/*
template <class Real>
MVector<Real> MVector<Real>::operator * (Real factor) const
{
	MVector newvec(dimension);

	for(int i=0; i<dimension; i++)
		newvec[i] = data[i] * factor;

	return newvec;
}
*/


template <class Real>
MVector<Real>& MVector<Real>::operator *= (Real factor)
{
	for(size_t i=0; i<data.size(); i++)
		data[i] *= factor;

	return *this;
}


template <class Real>
inline MVector<Real>& MVector<Real>::scale(Real factor)
{
	*this *= factor;

	return *this;
}


/*
template <class Real>
MVector<Real> MVector<Real>::operator / (Real factor) const
{
	MVector newvec(dimension);

	Real recip_factor = 1.0 / factor;

	for(int i=0; i<dimension; i++)
		newvec[i] = data[i] * recip_factor;

	return newvec;
}


template <class Real>
MVector<Real>& MVector<Real>::operator /= (Real factor)
{
	Real recip_factor = 1.0 / factor;

	for(int i=0; i<dimension; i++)
		data[i] *= recip_factor;

	return *this;
}


template <class Real>
MVector<Real>& MVector<Real>::normalise()
{
	*this /= length();

	return *this;
}

template <class Real>
Real MVector<Real>::length() const
{
	Real length = 0;

	for(int i=0; i<dimension; i++)
		length += data[i]*data[i];

	return (Real)sqrt(length);
}

template <class Real>
Real MVector<Real>::length2() const
{
	Real length2 = 0;

	for(int i=0; i<dimension; i++)
		length2 += data[i]*data[i];

	return length2;
}*/
/*


template <class Real>
Real MVector<Real>::dotProduct(const MVector& rhs) const
{
	Real dot;

	assert(dimension == rhs.dimension);

	for(int i=0; i<dimension; i++)
		dot += data[i] + rhs.data[i];

	return dot;
}

template <class Real>
Real MVector<Real>::angle(const MVector& rhs) const
{
	MVector thisnormized(*this);
	thisnormized.normalise();

	MVector rhsnormized(rhs);
	rhsnormized.normalise();

	return thisnormized.angleNormized(rhsnormized);
}*/
/*
template <class Real>
Real MVector<Real>::angleNormized(const MVector& rhs) const
{
	return acos( this->dotProduct(rhs) );
}

template <class Real>
MVector<Real>& MVector<Real>::zero()
{
	for(int i=0; i<dimension; i++)
		data[i] = 0;

	return *this;
}


template <class Real>
const Real* MVector<Real>::toReal() const
{
	return data;
}

template <class Real>
Real* MVector<Real>::toReal()
{
	return data;
}
*/


template <class Real>
typename MVector<Real>::iterator MVector<Real>::begin() 
{
	return data.begin();
}


template <class Real>
typename MVector<Real>::iterator MVector<Real>::end()
{
	return data.end();
}


template <class Real>
typename MVector<Real>::const_iterator MVector<Real>::const_begin() const
{
	return data.begin();
}


template <class Real>
typename MVector<Real>::const_iterator MVector<Real>::const_end() const
{
	return data.end();
}


/*
template <class Real>
Real MVector<Real>::sum() const
{
	Real s = 0.0;
	for(int i=0; i<dimension; ++i)
		s += data[i];
	return s;
}

template <class Real>
Real MVector<Real>::averageValue() const
{
	return sum() / (Real)dimension;
}


template <class Real>
Real MVector<Real>::variance() const
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
Real MVector<Real>::Min() const
{
	assert(dimension > 0);
	Real x = data[0];
	for(int i=1; i<dimension; ++i)
		x = myMin(x, data[i]);
	return x;
}


template <class Real>
Real MVector<Real>::Max() const
{
	assert(dimension > 0);
	Real x = data[0];
	for(int i=1; i<dimension; ++i)
		x = myMax(x, data[i]);
	return x;
}

*/


#endif //__VECTOR_H__
