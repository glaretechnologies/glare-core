/*=====================================================================
Distribution1.h
---------------
Copyright Glare Technologies Limited 2010 -
Generated at Sun Aug 15 02:19:14 +1200 2010
=====================================================================*/
#pragma once

#include <vector>
#include "Vector.h"
#include "platform.h"


/*=====================================================================
Distribution1
-------------

=====================================================================*/
template<typename Real>
class Distribution1
{
public:
	 Distribution1() { }
	~Distribution1() { }

	Distribution1(const std::vector<Real>& pdf)
	{
		const uint32 table_size = (uint32)pdf.size();

		cdfTable.resize(table_size);
		pdfInvTable.resize(table_size);
		guideTable.resize(table_size + 1);

		// Compute PDF normalisation factor using 4 partial sums.
		double running_cdf[4] = { 0, 0, 0, 0 };
		for(uint32 i = 0; i < table_size; ++i) running_cdf[i & 3] += pdf[i];

		// Construct normalised CDF table
		const double CDF_normalise = 1 / (running_cdf[0] + running_cdf[1] + running_cdf[2] + running_cdf[3]);
		running_cdf[0] = running_cdf[1] = running_cdf[2] = running_cdf[3] = 0;
		for(uint32 i = 0; i < table_size; ++i)
		{
			running_cdf[i & 3] += pdf[i];

			cdfTable[i] = (Real)((running_cdf[0] + running_cdf[1] + running_cdf[2] + running_cdf[3]) * CDF_normalise);
			pdfInvTable[i] = (Real)(pdf[i] * CDF_normalise);
		}

		// Build the guide table.
		for(uint32 i = 0, j = 0; i < guideTable.size(); ++i)
		{
			// Find the largest j st. CDF[j] < target CDF[i]
			const double target_cdf = i / (double)(guideTable.size() - 1);
			while(cdfTable[j] < target_cdf) ++j;
			guideTable[i] = j;
		}
	}

	inline uint32 sample(const Real x) const
	{
		uint32 i = guideTable[(uint32)(x * (guideTable.size() - 1))]; while(cdfTable[i] < x) ++i; assert(i < cdfTable.size());
		return i;
	}

	inline uint32 sampleLerp(const Real x, Real& x_lerp) const
	{
		uint32 i = sample(x); x_lerp = (cdfTable[i] - x) * pdfInvTable[i]; assert(x_lerp >= 0 && x_lerp <= 1);
		return i;
	}

	static void test();

private:

	js::Vector<Real, 64> cdfTable;
	js::Vector<Real, 64> pdfInvTable;
	js::Vector<Real, 64> guideTable;
};
