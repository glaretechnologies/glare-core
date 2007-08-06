/*=====================================================================
TestUtils.h
-----------
File created by ClassTemplate on Wed Jul 18 14:25:06 2007
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TESTUTILS_H_666_
#define __TESTUTILS_H_666_


#define testAssert(expr) (doTestAssert((expr), (#expr), (__LINE__), (__FILE__)))


void doTestAssert(bool expr, const char* test, long line, const char* file);



/*=====================================================================
TestUtils
---------

=====================================================================*/
/*class TestUtils
{
public:

	TestUtils();

	~TestUtils();
};*/



#endif //__TESTUTILS_H_666_




