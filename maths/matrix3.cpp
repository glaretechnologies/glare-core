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
	//TEMP: http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToAngle/
	angle_out = acos((elem(0, 0) + elem(1, 1) + elem(2, 2) - 1) / 2);

	Real x = (elem(2, 1) - elem(1, 2));
	Real y = (elem(0, 2) - elem(2, 0));
	Real z = (elem(1, 0) - elem(0, 1));

	unit_axis_out = normalise(Vec3<Real>(x, y, z));
}


#if BUILD_TESTS


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
}


#endif // BUILD_TESTS
