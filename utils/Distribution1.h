/*=====================================================================
Distribution1.h
---------------
Copyright Glare Technologies Limited 2010 -
Generated at Sun Aug 15 02:19:14 +1200 2010
=====================================================================*/
#pragma once

#include <vector>
#include "platform.h"


/*=====================================================================
Distribution1
-------------

=====================================================================*/
class Distribution1
{
public:
	typedef float Real;

	Distribution1();
	Distribution1(const std::vector<Real>& pdf);
	~Distribution1();

	inline uint32 sample(const Real x) const;

	static void test();

private:

	uint32 table_size;
	Real* cdfTable;
	uint32* guideTable;
};
