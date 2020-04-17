/*=====================================================================
Matrix2.cpp
-----------
File created by ClassTemplate on Sun Mar 05 17:55:13 2006
Code By Nicholas Chapman.
=====================================================================*/
#include "Matrix2.h"


#include "Complex.h"
#include "../utils/StringUtils.h"
#include <complex>

template <>
const std::string Matrix2<float>::toString() const
{
	std::string s;
	for(int i=0; i<2; ++i)
		s += "|" + floatToString(elem(i, 0)) + "," + floatToString(elem(i, 1)) + "|" + ((i < 1) ? "\n" : "");
	return s;
}

template <>
const std::string Matrix2<double>::toString() const
{
	std::string s;
	for(int i=0; i<2; ++i)
		s += "|" + doubleToString(elem(i, 0)) + "," + doubleToString(elem(i, 1)) + "|" + ((i < 1) ? "\n" : "");
	return s;
}



template <>
const std::string Matrix2<std::complex<float>>::toString() const
{
	std::string s;
	for(int i=0; i<2; ++i)
		s += "|" + doubleToString(elem(i, 0).real()) + " + " + doubleToString(elem(i, 0).imag()) + "," + doubleToString(elem(i, 1).real()) + " + " + doubleToString(elem(i, 1).imag()) + "|" + ((i < 1) ? "\n" : "");
	return s;
}


template <>
const std::string Matrix2<Complex<float>>::toStringNSigFigs(int n) const
{
	std::string s;
	for(int i=0; i<2; ++i)
		s += "|" + doubleToStringNSigFigs(elem(i, 0).real(), n) + " + " + doubleToStringNSigFigs(elem(i, 0).imag(), n) + "," +
			doubleToStringNSigFigs(elem(i, 1).real(), n) + " + " + doubleToStringNSigFigs(elem(i, 1).imag(), n) + "|" + ((i < 1) ? "\n" : "");
	return s;
}





