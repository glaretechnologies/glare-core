/*=====================================================================
ConPrint.h
-------------------
Copyright Glare Technologies Limited 2011 -
Generated at 2012-05-03 12:20:21 +0100
=====================================================================*/
#pragma once


#include <string>


/*
These functions should only be used for debugging and test code.
*/
#define printVar(v) conPrint(std::string(#v) + ": " + ::toString(v))

void conPrint(const std::string& s);

void conPrintStr(const std::string& s);

void fatalError(const std::string& s);

void logPrint(const std::string& s);