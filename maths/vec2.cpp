/*=====================================================================
vec2.cpp
--------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#include "vec2.h"


#include "../utils/StringUtils.h"


template <>
const std::string Vec2<int>::toString() const
{
	return "(" + ::toString(x) + ", " + ::toString(y) + ")";
}

template <>
const std::string Vec2<float>::toString() const
{
	return "(" + floatToString(x) + ", " + floatToString(y) + ")";
}

template <>
const std::string Vec2<double>::toString() const
{
	return "(" + doubleToString(x) + ", " + doubleToString(y) + ")";
}


template <>
const std::string Vec2<float>::toStringNSigFigs(int n) const
{
	return "(" + floatToStringNSigFigs(x, n) + ", " + floatToStringNSigFigs(y, n) + ")";
}

template <>
const std::string Vec2<double>::toStringNSigFigs(int n) const
{
	return "(" + doubleToStringNSigFigs(x) + ", " + doubleToStringNSigFigs(y) + ")";
}


template <>
const std::string Vec2<float>::toStringMaxNDecimalPlaces(int n) const
{
	return "(" + ::doubleToStringMaxNDecimalPlaces(x, n) + ", " + ::doubleToStringMaxNDecimalPlaces(y, n) + ")";
}

template <>
const std::string Vec2<double>::toStringMaxNDecimalPlaces(int n) const
{
	return "(" + doubleToStringMaxNDecimalPlaces(x, n) + ", " + doubleToStringMaxNDecimalPlaces(y, n) + ")";
}
