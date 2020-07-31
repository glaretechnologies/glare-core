/*=====================================================================
matrix3.cpp
-----------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#include "matrix3.h"


#include "../indigo/TestUtils.h"
#include "../utils/StringUtils.h"


// Do explicit template instantiation
template void Matrix3<float>::rotationMatrixToAxisAngle(Vec3<float>& unit_axis_out, float& angle_out) const;
template void Matrix3<double>::rotationMatrixToAxisAngle(Vec3<double>& unit_axis_out, double& angle_out) const;


template <>
const std::string Matrix3<float>::toString() const
{
	std::string s;
	for(int i=0; i<3; ++i)
		s += "|" + floatToString(elem(i, 0)) + "," + floatToString(elem(i, 1)) + "," + floatToString(elem(i, 2)) + "|" + ((i < 2) ? "\n" : "");
	return s;
}


template <>
const std::string Matrix3<double>::toString() const
{
	std::string s;
	for(int i=0; i<3; ++i)
		s += "|" + doubleToString(elem(i, 0)) + "," + doubleToString(elem(i, 1)) + "," + doubleToString(elem(i, 2)) + "|" + ((i < 2) ? "\n" : "");
	return s;
}


template <>
const std::string Matrix3<float>::toStringPlain() const
{
	std::string s;
	for(int i=0; i<8; ++i)
		s += floatToString(e[i]) + " ";
	s += floatToString(e[8]);
	return s;
}


template <class Real>
void Matrix3<Real>::rotationMatrixToAxisAngle(Vec3<Real>& unit_axis_out, Real& angle_out) const
{
	// See also Quat::fromMatrix in Quat.h which is very similar.
	// Adapted from Quat::fromMatrix() and http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
	
	const Real m00 = elem(0, 0);
	const Real m11 = elem(1, 1);
	const Real m22 = elem(2, 2);
	const Real tr = m00 + m11 + m22; // Trace
	if(tr >= 0.0)
	{
		const Real s = std::sqrt(tr + 1);

		unit_axis_out = normalise(Vec3<Real>( // NOTE: we could eliminate these normalises with some work.  Using a sin() call might not be a perf win however.
			(elem(2, 1) - elem(1, 2)), // recip_2s applied to all vector elements is not needed due to normalise.
			(elem(0, 2) - elem(2, 0)),
			(elem(1, 0) - elem(0, 1))
		));

		// quaternion w = s/2 = cos(angle/2)
		// acos(s/2) = angle/2
		// angle = 2 acos(s/2)
		angle_out = 2 * std::acos(s * (Real)0.5);
	}
	else if(m00 > m11 && m00 > m22)
	{
		const Real s = std::sqrt(1 + m00 - m11 - m22);
		const Real recip_2s = (Real)0.5 / s;
		
		unit_axis_out = normalise(Vec3<Real>(
			s * (Real)0.5,
			(elem(0, 1) + elem(1, 0)) * recip_2s,
			(elem(0, 2) + elem(2, 0)) * recip_2s
		));

		// w = (elem(2, 1) - elem(1, 2)) * 0.5 / s = cos(angle/2)
		// acos((elem(2, 1) - elem(1, 2)) * 0.5 / s) = angle/2
		// angle = 2 * acos((elem(2, 1) - elem(1, 2)) * 0.5 / s)
		angle_out = 2 * std::acos((elem(2, 1) - elem(1, 2)) * recip_2s);
	}
	else if(m11 > m22)
	{
		const Real s = std::sqrt(1 + m11 - m00 - m22);
		const Real recip_2s = (Real)0.5 / s;

		unit_axis_out = normalise(Vec3<Real>(
			(elem(0, 1) + elem(1, 0)) * recip_2s,
			s * (Real)0.5,
			(elem(1, 2) + elem(2, 1)) * recip_2s
		));

		angle_out = 2 * std::acos((elem(0, 2) - elem(2, 0)) * recip_2s);
	}
	else
	{
		// m22 > m00 > m11
		const Real s = std::sqrt(1 + m22 - m00 - m11);
		const Real recip_2s = (Real)0.5 / s;

		unit_axis_out = normalise(Vec3<Real>(
			(elem(0, 2) + elem(2, 0)) * recip_2s,
			(elem(1, 2) + elem(2, 1)) * recip_2s,
			s*(Real)0.5
		));

		angle_out = 2 * std::acos((elem(1, 0) - elem(0, 1)) * recip_2s);
	}
}


#if BUILD_TESTS


#include "PCG32.h"
#include "../utils/ConPrint.h"


template <>
void Matrix3<float>::test()
{
	{
		const float e[9] ={ 1,2,3,-4,5,6,7,-8,9 };
		Matrix3<float> m(e);
		testAssert(epsEqual((float)m.determinant(), 240.f));
	}

	const float a_e[9] = {3,1,-4,2,5,6,1,4,8};
	Matrix3<float> a(a_e);

	Matrix3<float> a_inverse;
	bool invertible = a.inverse(a_inverse);
	testAssert(invertible);
	testAssert(epsMatrixEqual(a * a_inverse, Matrix3<float>::identity(), (float)NICKMATHS_EPSILON));
	testAssert(epsMatrixEqual(a_inverse * a, identity(), (float)NICKMATHS_EPSILON));

	Matrix3<float> identity_inverse;
	invertible = identity().inverse(identity_inverse);
	testAssert(invertible);
	testAssert(epsMatrixEqual(identity(), identity_inverse, (float)NICKMATHS_EPSILON));


	//=================== rotationMatrixToAxisAngle() ======================
	{
		Vec3f axis(0,0,1);
		float angle = 0.5f;
		Matrix3f m = Matrix3f::rotationMatrix(axis, angle);

		Vec3f new_axis;
		float new_angle;
		m.rotationMatrixToAxisAngle(new_axis, new_angle);
		testAssert(epsEqual(axis, new_axis));
		testAssert(epsEqual(angle, new_angle));
	}

	{
		PCG32 rng(1);

		for(int i=0; i<1000; ++i)
		{
			// Generate a random rot matrix.
			const Vec3f axis = normalise(Vec3f(-1 + 2 * rng.unitRandom(), -1 + 2 * rng.unitRandom(), -1 + 2 * rng.unitRandom()));
			const float angle = rng.unitRandom() * Maths::get2Pi<float>();
			const Matrix3f rot_matrix = Matrix3f::rotationMatrix(axis, angle);

			// Convert to axis/angle
			Vec3f new_axis;
			float new_angle;
			rot_matrix.rotationMatrixToAxisAngle(new_axis, new_angle);

			testAssert(new_axis.isUnitLength());
			
			// Convert back to a rot matrix and compare with original rot matrix.
			const Matrix3f rot_matrix_2 = Matrix3f::rotationMatrix(new_axis, new_angle);

			//conPrint("--------rot_matrix  ------------\n" + rot_matrix.toString());
			//conPrint("--------rot_matrix_2------------\n" + rot_matrix_2.toString());

			testAssert(epsMatrixEqual(rot_matrix, rot_matrix_2, 5.0e-4f));
		}
	}
}


#endif // BUILD_TESTS
