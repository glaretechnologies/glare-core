/*=====================================================================
Complex.h
---------
File created by ClassTemplate on Tue Jan 29 03:09:12 2008
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include <cmath>


/*=====================================================================
Complex
-------

=====================================================================*/
template <class Real>
class Complex
{
public:

	inline Complex(){}
	inline Complex(Real a_, Real b_);
	inline ~Complex(){}

	inline static const Complex fromModulusAndArg(Real modulus, Real arg);


	inline const Complex operator * (Real x) const;
	inline const Complex operator * (const Complex& other) const;
	inline const Complex operator / (const Complex& other) const;
	inline const Complex operator + (const Complex& other) const;
	inline void operator += (const Complex& other);

	inline Real re() const { return a; }
	inline Real im() const { return b; }

	inline Real norm() const { return std::sqrt(a*a + b*b); }
	inline Real norm2() const { return a*a + b*b; }

	//value = a + b*i
	Real a, b;
};

template <class Real>
Complex<Real>::Complex(Real a_, Real b_)
:	a(a_), b(b_)
{}


template <class Real>
const Complex<Real> Complex<Real>::fromModulusAndArg(Real modulus, Real arg)
{
	return Complex(std::cos(arg) * modulus, std::sin(arg) * modulus);
}


template <class Real>
const Complex<Real> Complex<Real>::operator * (Real x) const
{
	return Complex<Real>(a*x, b*x);
}


template <class Real>
const Complex<Real> Complex<Real>::operator * (const Complex<Real>& other) const
{
	return Complex<Real>(a*other.a - b*other.b, a*other.b + b*other.a);
}


template <class Real>
const Complex<Real> Complex<Real>::operator / (const Complex<Real>& other) const
{
	return Complex<Real>(a*other.a + b*other.b, b*other.a - a*other.b) * (1 / other.norm2());
}


template <class Real>
const Complex<Real> Complex<Real>::operator + (const Complex<Real>& other) const
{
	return Complex<Real>(a + other.a, b + other.b) ;
}


template <class Real>
void Complex<Real>::operator += (const Complex<Real>& other)
{
	a += other.a;
	b += other.b;
}


typedef Complex<float> Complexf;
typedef Complex<double> Complexd;
