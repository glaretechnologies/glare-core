/*=====================================================================
matrix3.cpp
-----------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#include "matrix3.h"


#include "../utils/TestUtils.h"
#include "../utils/StringUtils.h"


// Do explicit template instantiation
template void Matrix3<float >::rotationMatrixToAxisAngle(Vec3<float>& unit_axis_out, float& angle_out) const;
template void Matrix3<double>::rotationMatrixToAxisAngle(Vec3<double>& unit_axis_out, double& angle_out) const;

template Vec3<float > Matrix3<float >::getAngles() const;
template Vec3<double> Matrix3<double>::getAngles() const;

template Matrix3<float > Matrix3<float >::fromAngles(const Vec3<float >& angles);
template Matrix3<double> Matrix3<double>::fromAngles(const Vec3<double>& angles);


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


// Angles = (a, b, c)
// Where the transformation is
// rot(vec3(0,0,1), a, rot(vec3(0,1,0), b, rot(vec3(1,0,0), c, v)))
//
// Where rot(axis, angle, v) is a rotation of the vector v around the axis by angle.

template <class Real>
Vec3<Real> Matrix3<Real>::getAngles() const // Get Euler angles.  Assumes this is a rotation matrix.
{
	const Vec3<Real> x_primed = getColumn0();
	const Vec3<Real> y_primed = getColumn1();

	if(fabs(x_primed.y) < (Real)1.0e-7 && fabs(x_primed.x) < (Real)1.0e-7)
	{
		// x-axis x and y components are both (almost) zero.  Therefore x-axis is oriented along the parent z-axis or -z axis.
		// In this case both the final rotation rot(vec3(0,0,1), a, v)  and the initial rotation rot(vec3(1,0,0), c, v)
		// result in a rotation around vec3(0,0,1).
		// Just use y' to determine the rotation and assign it to a, and let c = 0.
		// y' will lie in x-y plane.  y' pointing along parent y axis should be 0 rotation, so subtract pi/2.

		const Real z_angle = std::atan2(y_primed.y, y_primed.x) - Maths::pi_2<Real>();

		if(x_primed.z > 0) // x axis is pointing along +z:
		{
			return Vec3<Real>(z_angle, -Maths::pi_2<Real>(), 0);
		}
		else // else x axis is pointing along -z:
		{
			return Vec3<Real>(z_angle, Maths::pi_2<Real>(), 0);
		}
	} 
	else
	{
		const Real z_angle = std::atan2(x_primed.y, x_primed.x); // Get rotation angle around the z axis

		// Undo z rotation - get inverse z rotation matrix, e.g. the function rot(vec3(0,0,1), -a, v)
		const Matrix3<Real> inv_z_rot = Matrix3<Real>::rotationAroundZAxis(-z_angle);

		const Vec3<Real> x_primed_2 = inv_z_rot * x_primed; // x axis without z rotation, e.g.
		// rot(vec3(0,0,1), -a, rot(vec3(0,0,1), a, rot(vec3(0, 1, 0), b, rot(vec3(1,0,0), c, vec3(1,0,0))))
		// = rot(vec3(0, 1, 0), b, rot(vec3(1,0,0), c, vec3(1,0,0)))
		// It should lie in the x-z plane.
		assert(::epsEqual<Real>(x_primed_2.y, 0));

		// y angle is now given by rotation in x-z plane
		const Real y_angle = -std::atan2(x_primed_2.z, x_primed_2.x); // Rot around y axis

		const Matrix3<Real> inv_y_rot = Matrix3<Real>::rotationAroundYAxis(-y_angle);

		const Vec3<Real> y_primed_2 = inv_y_rot * (inv_z_rot * y_primed); // y axis with z rotation and y rotation, e.g.
		//   rot(vec3(0,1,0), -b, rot(vec3(0,0,1), -a, rot(vec3(0,0,1), a, rot(vec3(0,1,0), b, rot(vec3(1,0,0), c, vec3(0,1,0)))))
		// = rot(vec3(1,0,0), c, vec3(0,1,0))
		// It should lie in the y-z plane
		assert(::epsEqual<Real>(y_primed_2.x, 0));

		// x angle is now given by rotation angle in y-z plane.
		const Real x_angle = std::atan2(y_primed_2.z, y_primed_2.y); // Rot around x axis
	
		return Vec3<Real>(z_angle, y_angle, x_angle);
	}
}


template <class Real>
Matrix3<Real> Matrix3<Real>::fromAngles(const Vec3<Real>& angles) // Construct rotation matrix from Euler angles.
{
	return Matrix3<Real>::rotationAroundZAxis(angles.x) * Matrix3<Real>::rotationAroundYAxis(angles.y) * Matrix3<Real>::rotationAroundXAxis(angles.z);
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


	// Test rotationAroundXAxis
	{
		for(float theta = -10.0f; theta < 10.0f; theta += 0.1f)
			testAssert(epsMatrixEqual(Matrix3f::rotationAroundXAxis(theta), Matrix3f::rotationMatrix(Vec3f(1, 0, 0), theta)));
	}
	// Test rotationAroundYAxis
	{
		for(float theta = -10.0f; theta < 10.0f; theta += 0.1f)
			testAssert(epsMatrixEqual(Matrix3f::rotationAroundYAxis(theta), Matrix3f::rotationMatrix(Vec3f(0, 1, 0), theta)));
	}
	// Test rotationAroundZAxis
	{
		for(float theta = -10.0f; theta < 10.0f; theta += 0.1f)
			testAssert(epsMatrixEqual(Matrix3f::rotationAroundZAxis(theta), Matrix3f::rotationMatrix(Vec3f(0, 0, 1), theta)));
	}

	//=================== getAngles and fromAngles ======================
	{
		testEpsEqual(Matrix3f::identity().getAngles(), Vec3f(0, 0, 0));

		testEpsEqual(Matrix3f::rotationAroundZAxis(1.0).getAngles(), Vec3f(1, 0, 0)); // a = rot around z axis (yaw)

		testEpsEqual(Matrix3f::rotationAroundYAxis(1.0).getAngles(), Vec3f(0, 1, 0)); // b = rot around y axis (pitch)

		testEpsEqual(Matrix3f::rotationAroundXAxis(1.0).getAngles(), Vec3f(0, 0, 1)); // c = rot around x axis (roll)

		// Test pitching angle of pi/2 and -pi/2
		testEpsEqual(Matrix3f::rotationAroundYAxis(Maths::pi_2<float>()).getAngles(), Vec3f(0, Maths::pi_2<float>(), 0)); // b = rot around y axis
		testEpsEqual(Matrix3f::rotationAroundYAxis(-Maths::pi_2<float>()).getAngles(), Vec3f(0, -Maths::pi_2<float>(), 0)); // b = rot around y axis

		// TODO: test these more.

		// Test with some random rotations
		{
			PCG32 rng(1);

			for(int i=0; i<1000; ++i)
			{
				// Generate a random rot matrix.
				const Vec3f axis = normalise(Vec3f(-1 + 2 * rng.unitRandom(), -1 + 2 * rng.unitRandom(), -1 + 2 * rng.unitRandom()));
				const float angle = rng.unitRandom() * Maths::get2Pi<float>();
				const Matrix3f rot_matrix = Matrix3f::rotationMatrix(axis, angle);

				// Get angles from matrix
				const Vec3f angles = rot_matrix.getAngles();

				// Convert angles back to a rotation matrix
				const Matrix3f m2 = Matrix3f::fromAngles(angles);

				// Check round trip results in the same matrix.
				testAssert(epsMatrixEqual(rot_matrix, m2));
			}
		}
	}


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
