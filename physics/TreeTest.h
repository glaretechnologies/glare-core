/*=====================================================================
TreeTest.h
----------
File created by ClassTemplate on Tue Jun 26 20:19:05 2007
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include <string>


namespace js
{


/*=====================================================================
TreeTest
--------

=====================================================================*/
class TreeTest
{
public:
	static void doTests(const std::string& appdata_path);
	//static void doSpeedTest(int treetype);
	static void doVaryingNumtrisBuildTests();
	//static void buildSpeedTest();
	static void testBuildCorrect();
	static void doRayTests();
	static void doSphereTracingTests(const std::string& appdata_path);
	static void doAppendCollPointsTests(const std::string& appdata_path);
};


} //end namespace js
