/*=====================================================================
ConPrint.h
----------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include <string>


/*
These functions should only be used for debugging and test code.
*/
#define printVar(v) conPrint(std::string(#v) + ": " + ::toString(v))

void conPrint(const std::string& s);

void conPrintStr(const std::string& s);

void stdErrPrint(const std::string& s);

void fatalError(const std::string& s);
