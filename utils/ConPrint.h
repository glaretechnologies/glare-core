/*=====================================================================
ConPrint.h
----------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include <string>
#include "string_view.h"


/*
These functions should only be used for debugging and test code.
*/
#define printVar(v) conPrint(std::string(#v) + ": " + ::toString(v))

void conPrint(const string_view s);

void conPrintStr(const string_view s);

void stdErrPrint(const string_view s);

void fatalError(const string_view s);
