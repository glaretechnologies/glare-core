/*=====================================================================
Hilbert.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Fri Nov 26 14:59:10 +1300 2010
=====================================================================*/
#pragma once


#include <vector>


/*=====================================================================
Hilbert
-------------------

=====================================================================*/
class Hilbert
{
public:
	Hilbert();
	~Hilbert();


	static void test();

	static void generate(int depth, std::vector<std::pair<int, int> >& indices_out);

private:

};
