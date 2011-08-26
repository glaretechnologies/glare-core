/*=====================================================================
DeltaSigmaSampler.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Fri Jul 22 18:12:30 +0100 2011
=====================================================================*/
#pragma once

#include <vector>
#include <math.h>
#include <assert.h>



class RampFunction
{
public:
	double eval(const std::vector<double>& s) const { return s[0]; }
};


/*=====================================================================
DeltaSigmaSampler
-----------------

=====================================================================*/
template<class Importance, class Integrand>
class DeltaSigmaSampler
{
public:
	DeltaSigmaSampler()
	{
	}

	~DeltaSigmaSampler()
	{
	}

	class samplePoint
	{
	public:
		double importance;
		double error;

		int sample_number;

		bool diffused;
	};

	template<typename int_type, typename real_type>
	inline double RadicalInverse(int_type n, const int_type base, const real_type inv_base)
	{
		real_type val = 0;
		real_type inv_B_i = inv_base;
		while (n > 0)
		{
			const int_type div = n / base;
			const int_type d_i = n - base * div;

			n = div;
			val += d_i * inv_B_i;
			inv_B_i *= inv_base;
		}

		return val;
	}

	double integrate(const Importance& importance, const Integrand& integrand,
					const std::vector<int>& bases, const std::vector<int>& dim_res, const int final_samples)
	{
		const int num_dims = (int)bases.size();
		const int num_samples = numSamples(dim_res);

		std::cout << "initialising delta sigma sampler with ";
		for (int i = 0; i < num_dims - 1; ++i)
			std::cout << dim_res[i] << " x ";
		std::cout << dim_res[num_dims - 1] << " = " << num_samples << " importance samples" << std::endl;

		sample_points.resize((size_t)num_samples);
		std::vector<double> sample(num_dims);
		std::vector<int> cell_indices(num_dims);
		std::cout << "estimating total importance... " << std::endl;

		for (int i = 0; i < num_samples; ++i)
			sample_points[i].error = -666;

		double importance_sum = 0;
		for (int i = 1; i <= num_samples; ++i)
		{
			int sample_idx = 0, dim_scale = 1;
			for (int d = 0; d < num_dims; ++d)
			{
				const double u = RadicalInverse<int, double>(i, bases[d], 1.0 / bases[d]);

				const int c_d = (int)(u * dim_res[d]);
				sample_idx += c_d * dim_scale;
				dim_scale *= dim_res[d];

				cell_indices[d] = c_d;
				sample[d] = u;
			}

			const double I = importance.eval(sample);
			samplePoint& s = sample_points[sample_idx];

			if (s.error != -666)
				std::cout << "cell " << sample_idx << " processed more than once!" << std::endl;

			s.importance = I;
			s.error = I;
			s.sample_number = i;
			s.diffused = false;

			importance_sum += I;
		}
		std::cout << "importance sum: " << importance_sum << std::endl << std::endl;

		for (int i = 0; i < num_samples; ++i)
			if (sample_points[i].error == -666)
				std::cout << "cell " << i << " is uninitialised!" << std::endl;
			else if (false)
				std::cout << "cell " << i << " holds sample " << sample_points[i].sample_number << std::endl;

		std::cout << "estimating integral with " << final_samples << " samples (allegedly)... " << std::endl;

		const double error_thresh = importance_sum / final_samples;

		double integral = 0;
		for (int j = 0; j < num_samples; ++j)
		{
			int sample_idx = 0, dim_scale = 1, j_s = j;
			for (int d = 0; d < num_dims; ++d)
			{
				const int div = j_s / dim_res[d];
				const int c_d = j_s - dim_res[d] * div;

				cell_indices[d] = c_d;

				sample_idx += c_d * dim_scale;
				dim_scale *= dim_res[d];
				j_s = div;
			}

			samplePoint& s = sample_points[j];

			if (s.error >= error_thresh * 0.5)
			{
				s.error -= error_thresh;

				const int sample_number = s.sample_number;
				for (int d = 0; d < num_dims; ++d)
					sample[d] = RadicalInverse<int, double>(sample_number, bases[d], 1.0 / bases[d]);

				const double sample_weight = error_thresh / s.importance;

				//std::cout << "sample " << sample_number << " at ";
				//for (int i = 0; i < num_dims - 1; ++i) std::cout << sample[i] << " x ";
				//std::cout << sample[num_dims - 1] << " has weight " << sample_weight << std::endl;

				integral += sample_weight * integrand.eval(sample);
			}

			// Get normalised filter sum (1 / number of valid neighbours)
			int w_sum = 0;
			for (int d = 0; d < num_dims; ++d)
				if (cell_indices[d] + 1 < dim_res[d]) ++w_sum;
			//std::cout << "cell " << j << " has " << w_sum << " neighbours" << std::endl;

			if (w_sum > 0)
			{
				const double diffused_error = s.error / w_sum;

				dim_scale = 1;
				for (int d = 0; d < num_dims; ++d)
				{
					if (cell_indices[d] + 1 >= dim_res[d]/* || sample_points[sample_idx + dim_scale].diffused*/)
						continue;

					sample_points[sample_idx + dim_scale].error += diffused_error;
					sample_points[sample_idx + dim_scale].diffused = true;

					dim_scale *= dim_res[d];
				}
			}
		}

		const double norm = 1.0 / num_samples; // TEMP HACK

		return integral * norm;
	}

	static void test();

private:

	inline int numSamples(const std::vector<int>& dim_res) const
	{
		assert(dim_res.size() > 0);
		int total = dim_res[0];
		for (int i = 1; i < (int)dim_res.size(); ++i)
			total *= dim_res[i];
		return total;
	}

	std::vector<samplePoint> sample_points;
};
