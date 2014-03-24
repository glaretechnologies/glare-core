/*=====================================================================
NVec.h
------
File created by ClassTemplate on Thu Aug 24 19:52:33 2006
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __NVEC_H_666_
#define __NVEC_H_666_


#include "mathstypes.h"
#include <string>
#include "../utils/StringUtils.h"

/*=====================================================================
NVec
----

=====================================================================*/
template <unsigned int N, class RealType>
class NVec
{
public:
	/*=====================================================================
	NVec
	----
	
	=====================================================================*/
	NVec();
	explicit NVec(RealType initval);
	NVec(const NVec& other);

	~NVec();

	inline void set(RealType x);
	inline NVec& operator=(const NVec& other);

	inline RealType& operator[](unsigned int index);
	inline const RealType& operator[](unsigned int index) const;

	inline unsigned int size() const { return N; }

	inline void setToMult(const NVec& other, RealType factor);
	inline void addMult(const NVec& other, RealType factor);
	inline void addLog(const NVec& other); // Add the log of the other components
	inline void addExp(const NVec& other);

	inline NVec& operator+=(const NVec& other);
	inline NVec& operator*=(const NVec& other);
	inline NVec& operator/=(const NVec& other);

	inline NVec& operator+=(RealType x);
	inline NVec& operator*=(RealType factor);
	inline NVec& operator/=(RealType factor);

	inline bool operator==(const NVec& other) const;
	inline bool operator!=(const NVec& other) const;

	inline RealType sum() const;
	inline RealType minVal() const;
	inline RealType maxVal() const;

	inline const std::string toString() const;
	

	RealType e[N];
};

void unitTestNVec();

template <unsigned int N, class RealType>
NVec<N, RealType>::NVec()
{
}

template <unsigned int N, class RealType>
NVec<N, RealType>::NVec(RealType initval)
{
	for(unsigned int i=0; i<N; ++i)
		e[i] = initval;
}

template <unsigned int N, class RealType>
NVec<N, RealType>::NVec(const NVec<N, RealType>& other)
{
	for(unsigned int i=0; i<N; ++i)
		e[i] = other.e[i];
}

template <unsigned int N, class RealType>
NVec<N, RealType>::~NVec()
{}


template <unsigned int N, class RealType>
void NVec<N, RealType>::set(RealType x)
{
	for(unsigned int i=0; i<N; ++i)
		e[i] = x;
}


template <unsigned int N, class RealType>
RealType& NVec<N, RealType>::operator[](unsigned int index)
{
	assert(index < N);
	return e[index];
}
template <unsigned int N, class RealType>
const RealType& NVec<N, RealType>::operator[](unsigned int index) const
{
	assert(index < N);
	return e[index];
}

template <unsigned int N, class RealType>
NVec<N, RealType>& NVec<N, RealType>::operator=(const NVec& other)
{
	for(unsigned int i=0; i<N; ++i)
		e[i] = other.e[i];
	return *this;	
}

template <unsigned int N, class RealType>
void NVec<N, RealType>::setToMult(const NVec& other, RealType factor)
{
	for(unsigned int i=0; i<N; ++i)
		e[i] = other.e[i] * factor;
}

template <unsigned int N, class RealType>
void NVec<N, RealType>::addMult(const NVec& other, RealType factor)
{
	for(unsigned int i=0; i<N; ++i)
		e[i] += other.e[i] * factor;
}

template <unsigned int N, class RealType>
void NVec<N, RealType>::addLog(const NVec& other) // Add the log of the other components
{
	for(unsigned int i=0; i<N; ++i)
	{
		assert(other[i] > 0.0);
		e[i] += log(other.e[i]);
	}
}
template <unsigned int N, class RealType>
void NVec<N, RealType>::addExp(const NVec& other) // Add the log of the other components
{
	for(unsigned int i=0; i<N; ++i)
	{
		e[i] += exp(other.e[i]);
	}
}


template <unsigned int N, class RealType>
NVec<N, RealType>& NVec<N, RealType>::operator+=(const NVec& other)
{
	for(unsigned int i=0; i<N; ++i)
		e[i] += other.e[i];
	return *this;	
}

template <unsigned int N, class RealType>
NVec<N, RealType>& NVec<N, RealType>::operator*=(const NVec& other)
{
	for(unsigned int i=0; i<N; ++i)
		e[i] *= other.e[i];
	return *this;	
}

template <unsigned int N, class RealType>
NVec<N, RealType>& NVec<N, RealType>::operator/=(const NVec& other)
{
	for(unsigned int i=0; i<N; ++i)
		e[i] /= other.e[i];
	return *this;
}

template <unsigned int N, class RealType>
NVec<N, RealType>& NVec<N, RealType>::operator+=(RealType x)
{
	for(unsigned int i=0; i<N; ++i)
		e[i] += x;
	return *this;	
}

template <unsigned int N, class RealType>
NVec<N, RealType>& NVec<N, RealType>::operator*=(RealType factor)
{
	for(unsigned int i=0; i<N; ++i)
		e[i] *= factor;
	return *this;	
}

template <unsigned int N, class RealType>
NVec<N, RealType>& NVec<N, RealType>::operator/=(RealType factor)
{
	const RealType recip = 1.0f / factor;
	for(unsigned int i=0; i<N; ++i)
		e[i] *= recip;
	return *this;	
}

template <unsigned int N, class RealType>	
bool NVec<N, RealType>::operator==(const NVec& other) const
{
	for(unsigned int i=0; i<N; ++i)
		if(e[i] != other.e[i])
			return false;
	return true;
}

template <unsigned int N, class RealType>	
bool NVec<N, RealType>::operator!=(const NVec& other) const
{
	for(unsigned int i=0; i<N; ++i)
		if(e[i] != other.e[i])
			return true;
	return false;
}


template <unsigned int N, class RealType>
RealType NVec<N, RealType>::sum() const
{
	RealType sum = 0.0f;
	for(unsigned int i=0; i<N; ++i)
		sum += e[i];
	return sum;
}


template <unsigned int N, class RealType>
RealType NVec<N, RealType>::minVal() const
{
	RealType minval = e[0];
	for(unsigned int i=1; i<N; ++i)
		minval = myMin(minval, e[i]);
	return minval;
}


template <unsigned int N, class RealType>
RealType NVec<N, RealType>::maxVal() const
{
	RealType maxval = e[0];
	for(unsigned int i=1; i<N; ++i)
		maxval = myMax(maxval, e[i]);
	return maxval;
}

template <unsigned int N, class RealType>
const std::string NVec<N, RealType>::toString() const
{
	std::string s = "(";
	for(unsigned int i=0; i<size(); ++i)
		s += ::toString(e[i]) + (i == size()-1 ? ")" : ", ");
	return s;
}

#endif //__NVEC_H_666_






















