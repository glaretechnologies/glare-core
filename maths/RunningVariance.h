/*=====================================================================
RunningVariance.h
-----------------
File created by ClassTemplate on Thu Jun 04 15:54:34 2009
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include <cmath>


/*=====================================================================
RunningVariance
---------------

=====================================================================*/
template <class Real>
class RunningVariance
{
public:
	/*=====================================================================
	RunningVariance
	---------------
	
	=====================================================================*/
	RunningVariance()
	:	//s_j_sq(0.0),
		//j(0),
		//mu_j(0.0)
		M_k(0.0),
		S_k(0.0),
		k(0)
	{}

	~RunningVariance()
	{}

	Real variance() const
	{
		//return s_j_sq;
		return k <= 1 ? 0 : S_k / (Real)(k - 1);
	}

	Real standardDev() const
	{
		return std::sqrt(variance());
	}

	Real mean() const
	{
		return M_k; // mu_j;
	}

	void update(Real x)
	{
		const Real M_k_1 = M_k;
		const Real S_k_1 = S_k;
		k++;

		M_k = M_k_1 + (x - M_k_1) / (Real)k;
		S_k = S_k_1 + (x - M_k_1) * (x - M_k);

		/*if(j == 0)
		{
			mu_j = x;
			s_j_sq = 0.0;
		}
		else
		{
			const Real mu_j_1 = mu_j + (x - mu_j) / (Real)(j + 1);

			const Real s_j_1_sq = ((Real)1.0 - (Real)1.0 / (Real)j) * s_j_sq + (Real)(j + 1) * Maths::square(mu_j_1 - mu_j);

			// update
			mu_j = mu_j_1;
			s_j_sq = s_j_1_sq;
		}
		j++;*/
	}


	// Computes the standard deviation of the estimator formed by taking the mean of the samples taken.
	Real meanEstimatorStdDev() const
	{
		/*
			VAR(SUM(X_i)/N)
			= VAR(SUM(X_I))/N^2
			= SUM(VAR(X_I))/N^2
			= SUM(V)/N^2
			= (NV)/N^2
			= V/N

			sigma(SUM(X_i)/N) = sqrt(VAR(SUM(X_i)/N)) = sqrt(V/N)
		*/
		return std::sqrt(variance() / k);
	}



private:
	//Real s_j; // Variance of the first j samples
	//unsigned int j;
	//Real mu_j; // Mean of the first j samples

	Real M_k, S_k;
	int k;

};


void testRunningVariance();
