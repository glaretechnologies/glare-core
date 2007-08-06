#ifndef __VECTOR_H__
#define __VECTOR_H__

#include <math.h>
#include <assert.h>

//class Graphics2d;
class VectorIterator;
class ConstVectorIterator;
//class Vec3;

template <class Real>
class Vector
{
public:
	inline Vector(int dimension);
	inline Vector();
	inline Vector(int dimension, const Real* data);
	inline Vector(const Vector& rhs);

	inline ~Vector();

	//inline void checkResize(int newdimension);

	//aliases:
	inline int getDim() const { return dimension; }
	inline int size() const { return dimension; }
	inline int getSize() const { return dimension; }

	inline Real operator[] (int index) const;
	inline Real& operator[] (int index);

	inline Vector& operator = (const Vector& rhs);
	inline bool operator == (const Vector& rhs) const;


	inline Vector operator - (const Vector& rhs) const;
	inline Vector operator + (const Vector& rhs) const;

	inline Vector& operator -= (const Vector& rhs);
	inline Vector& operator += (const Vector& rhs);

	inline Vector& addFirstNElements(const Vector& rhs, int N);

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

	typedef VectorIterator iterator;
	inline VectorIterator begin();
	inline VectorIterator end();

	typedef ConstVectorIterator const_iterator;
	inline ConstVectorIterator const_begin() const;
	inline ConstVectorIterator const_end() const;

//	inline ConstVectorIterator const_begin() const; { return begin(); }

	//void draw(Graphics2d& graphics, int xpos, int ypos, int maxbaramp, float scale, 
	//	float width, const Vec3& colour) const;

	inline Real averageValue() const;
	inline Real variance() const;

	inline Real Min() const;
	inline Real Max() const;

	//static void test();

private:
	int dimension;
	Real* data;
};





/*
class VectorIterator
{
public:
	inline VectorIterator(Vector* vec, int index);
	inline VectorIterator(const VectorIterator& rhs);
	inline ~VectorIterator();


	inline VectorIterator& operator = (const VectorIterator& rhs);
	inline float& operator * () const;
	inline VectorIterator& operator ++ ();
	inline void operator ++ (int);
	inline bool operator != (const VectorIterator& rhs) const; 
	int getIndex() const { return index; }

private:
	Vector* vec;
	int index;
};



class ConstVectorIterator
{
public:
	inline ConstVectorIterator(const Vector* vec, int index);
	inline ConstVectorIterator(const ConstVectorIterator& rhs);
	inline ~ConstVectorIterator();


	inline ConstVectorIterator& operator = (const ConstVectorIterator& rhs);
	inline float operator * () const;
	inline ConstVectorIterator& operator ++ ();
	inline void operator ++ (int);
	inline bool operator != (const ConstVectorIterator& rhs) const; 
	int getIndex() const { return index; }

private:
	const Vector* vec;
	int index;
};



VectorIterator::VectorIterator(Vector* vec_, int index_)
{
	vec = vec_;
	index = index_;
}

VectorIterator::VectorIterator(const VectorIterator& rhs)
{
	vec = rhs.vec;
	index = rhs.index;
}

VectorIterator::~VectorIterator()
{
}


VectorIterator& VectorIterator::operator = (const VectorIterator& rhs)
{
	vec = rhs.vec;
	index = rhs.index;
}

float& VectorIterator::operator * () const
{
	return (*vec)[index];
}

VectorIterator& VectorIterator::operator ++ ()
{
	index++;
	return *this;
}
void VectorIterator::operator ++ (int)
{
	index++;
}


bool VectorIterator::operator != (const VectorIterator& rhs) const
{
	assert(rhs.vec == vec);
	return index != rhs.index;
}





ConstVectorIterator::ConstVectorIterator(const Vector* vec_, int index_)
{
	vec = vec_;
	index = index_;
}

ConstVectorIterator::ConstVectorIterator(const ConstVectorIterator& rhs)
{
	vec = rhs.vec;
	index = rhs.index;
}

ConstVectorIterator::~ConstVectorIterator()
{
}


ConstVectorIterator& ConstVectorIterator::operator = (const ConstVectorIterator& rhs)
{
	vec = rhs.vec;
	index = rhs.index;
}

float ConstVectorIterator::operator * () const
{
	return (*vec)[index];
}

ConstVectorIterator& ConstVectorIterator::operator ++ ()
{
	index++;
	return *this;
}
void ConstVectorIterator::operator ++ (int)
{
	index++;
}


bool ConstVectorIterator::operator != (const ConstVectorIterator& rhs) const
{
	assert(rhs.vec == vec);
	return index != rhs.index;
}
*/




template <class Real>
Vector<Real>::Vector(int dimension_)
{
	dimension = dimension_;
	data = new Real[dimension];
}

template <class Real>
Vector<Real>::Vector(const Vector& rhs)
{
	dimension = rhs.dimension;

	data = new Real[dimension];

	for(int i=0; i<dimension; i++)
		data[i] = rhs.data[i];
}

template <class Real>
Vector<Real>::Vector()
{
	dimension = 0;
	data = 0;
}

template <class Real>
Vector<Real>::Vector(int dimension_, const Real* newdata)
{
	assert(newdata);
	dimension = dimension_;

	data = new Real[dimension];

	for(int i=0; i<dimension; i++)
		data[i] = newdata[i];
}

	




template <class Real>
Vector<Real>::~Vector()
{
	delete[] data;
}


template <class Real>
Real Vector<Real>::operator[] (int index) const
{
	assert(index >= 0 && index < dimension);
	
	return data[index];
}

template <class Real>
Real& Vector<Real>::operator[] (int index)
{
	assert(index >= 0 && index < dimension);
	
	return data[index];
}

template <class Real>
Vector<Real>& Vector<Real>::operator = (const Vector& rhs)
{
	if(dimension != rhs.dimension)
	{
		delete[] data;

		dimension = rhs.dimension;

		data = new Real[dimension];
	}

	for(int i=0; i<dimension; i++)
	{
		data[i] = rhs.data[i];
	}

	return *this;
}




template <class Real>
bool Vector<Real>::operator == (const Vector& rhs) const
{
	if(dimension != rhs.dimension)
		return false;

	for(int i=0; i<dimension; i++)
	{
		if(data[i] != rhs.data[i])
			return false;
	}

	return true;
}


template <class Real>
Vector<Real> Vector<Real>::operator - (const Vector& rhs) const
{
	assert(dimension == rhs.dimension);
	Vector newvec(dimension);

	for(int i=0; i<dimension; i++)
		newvec[i] = data[i] - rhs.data[i];

	return newvec;
}





template <class Real>
Vector<Real> Vector<Real>::operator + (const Vector& rhs) const
{
	assert(dimension == rhs.dimension);
	Vector newvec(dimension);

	for(int i=0; i<dimension; i++)
		newvec[i] = data[i] + rhs.data[i];

	return newvec;
}





template <class Real>
Vector<Real>& Vector<Real>::operator -= (const Vector& rhs)
{
	assert(dimension == rhs.dimension);

	for(int i=0; i<dimension; i++)
		data[i] -= rhs.data[i];

	return *this;
}

template <class Real>
Vector<Real>& Vector<Real>::operator += (const Vector& rhs)
{
	assert(dimension == rhs.dimension);

	for(int i=0; i<dimension; i++)
		data[i] += rhs.data[i];

	return *this;
}


template <class Real>
Vector<Real>& Vector<Real>::addFirstNElements(const Vector& rhs, int N)
{
	assert(N >= 0 && N < dimension);

	for(int i=0; i<N; i++)
		data[i] += rhs.data[i];

	return *this;
}


template <class Real>
Vector<Real> Vector<Real>::operator * (Real factor) const
{
	Vector newvec(dimension);

	for(int i=0; i<dimension; i++)
		newvec[i] = data[i] * factor;

	return newvec;
}


template <class Real>
Vector<Real>& Vector<Real>::operator *= (Real factor)
{
	for(int i=0; i<dimension; i++)
		data[i] *= factor;

	return *this;
}

template <class Real>
inline Vector<Real>& Vector<Real>::scale(Real factor)
{
	*this *= factor;

	return *this;
}


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
}



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
}

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



/*
VectorIterator Vector::begin() 
{
	return VectorIterator(this, 0);
}

VectorIterator Vector::end()
{
	return VectorIterator(this, dimension);
}

ConstVectorIterator Vector::const_begin() const
{
	return ConstVectorIterator(this, 0);
}

ConstVectorIterator Vector::const_end() const
{
	return ConstVectorIterator(this, dimension);
}
*/

template <class Real>
Real Vector<Real>::averageValue() const
{
	Real sum = 0.0;
	for(int i=0; i<dimension; ++i)
		sum += data[i];
	return sum / (Real)dimension;
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






#endif //__VECTOR_H__
