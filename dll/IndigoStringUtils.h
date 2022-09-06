/*=====================================================================
IndigoStringUtils.h
-------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "include/IndigoString.h"
#include <string>


//namespace IndigoStringUtils
//{
	inline const std::string toStdString(const Indigo::String& s)
	{
		return std::string(s.dataPtr(), s.length());
	}


	inline const Indigo::String toIndigoString(const std::string& s)
	{
		return Indigo::String(s.c_str(), s.length());
	}
//};
