/*=====================================================================
matrix3.cpp
-----------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "matrix3.h"


#include "../indigo/TestUtils.h"
#include "../utils/StringUtils.h"


// Do explicit template instantiation
template bool Matrix3<float>::polarDecomposition(Matrix3<float>&, Matrix3<float>&) const;
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


template <class Real>
inline static bool isSignificantDiff(const Matrix3<Real>& a, const Matrix3<Real>& b)
{
	for(int i=0; i<9; ++i)
		if(std::fabs(a.e[i] - b.e[i]) > 2.0e-7f)
			return true;
	return false;
}


// See 'Matrix Animation and Polar Decomposition' by Ken Shoemake & Tom Duff
// http://research.cs.wisc.edu/graphics/Courses/838-s2002/Papers/polar-decomp.pdf
template <class Real>
bool Matrix3<Real>::polarDecomposition(Matrix3<Real>& rot_out, Matrix3<Real>& rest_out) const
{
	Matrix3<Real> Q = *this;

	while(1)
	{
		// Compute inverse transpose of Q
		const Matrix3<Real> T = Q.getTranspose();
		Matrix3<Real> T_inv;
		const bool invertible = T.inverse(T_inv);
		if(!invertible)
			return false;

		Matrix3<Real> new_Q = (Q + T_inv);
		new_Q.scale(0.5f);
		
		if(!isSignificantDiff(new_Q, Q))
		{
			Q = new_Q;
			break;
		}

		Q = new_Q;
	}

	if(Q.determinant() < 0)
		Q.scale(-1);

	rot_out = Q;

	// M = QS
	// Q^-1 M = Q^-1 Q S
	// Q^T M = S			[Q^-1 = Q^T as Q is a rotation matrix]
	rest_out = Q.getTranspose() * *this;
	return true;
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


// Is the matrix orthogonal and does it have determinant 1?
template <class Real>
static bool isSpecialOrthogonal(const Matrix3<Real>& m)
{
	return epsMatrixEqual(m * m.getTranspose(), Matrix3<Real>::identity()) && epsEqual(m.determinant(), 1.0f);
}


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

	//=================== polarDecomposition() ======================

	// Decompose a matrix with negative scale
	{
		Matrix3f m(Vec3f(-1, 0, 0), Vec3f(0, 1, 0), Vec3f(0, 0, 1));

		Matrix3f rot, rest;
		testAssert(m.polarDecomposition(rot, rest));
		
		testAssert(isSpecialOrthogonal(rot));
		testAssert(epsMatrixEqual(rot * rest, m, 1.0e-6f));
	}

	// Decompose the identity matrix
	{
		Matrix3f m = Matrix3f::identity();
		Matrix3f rot, rest;
		testAssert(m.polarDecomposition(rot, rest));
		testAssert(epsMatrixEqual(rot, Matrix3f::identity()));
		testAssert(epsMatrixEqual(rest, Matrix3f::identity()));
	}

	// Decompose a pure rotation matrix
	{
		Matrix3f m = Matrix3f::rotationMatrix(Vec3f(0, 0, 1), 0.6f);
		Matrix3f rot, rest;
		testAssert(m.polarDecomposition(rot, rest));
		testAssert(epsMatrixEqual(rot, m));
		testAssert(epsMatrixEqual(rest, Matrix3f::identity()));
	}

	// Decompose a more complicated pure rotation matrix
	{
		Matrix3f m = Matrix3f::rotationMatrix(normalise(Vec3f(-0.5, 0.6, 1)), 0.6f);
		Matrix3f rot, rest;
		testAssert(m.polarDecomposition(rot, rest));
		testAssert(epsMatrixEqual(rot, m));
		testAssert(epsMatrixEqual(rest, Matrix3f::identity()));
	}

	// Decompose a rotation combined with a uniform scale
	{
		Matrix3f rot_matrix = Matrix3f::rotationMatrix(normalise(Vec3f(-0.5, 0.6, 1)), 0.6f);
		Matrix3f scale_matrix = Matrix3f::identity();
		scale_matrix.scale(0.3f);

		{
		Matrix3f m = rot_matrix * scale_matrix;
		Matrix3f rot, rest;
		testAssert(m.polarDecomposition(rot, rest));
		testAssert(epsMatrixEqual(rot, rot_matrix));
		testAssert(epsMatrixEqual(rest, scale_matrix));

		testAssert(isSpecialOrthogonal(rot));
		testAssert(epsMatrixEqual(rot * rest, m, 1.0e-6f));
		}

		// Try with the rot and scale concatenated in reverse order
		{
		Matrix3f m = scale_matrix * rot_matrix;
		Matrix3f rot, rest;
		testAssert(m.polarDecomposition(rot, rest));
		testAssert(epsMatrixEqual(rot, rot_matrix));
		testAssert(epsMatrixEqual(rest, scale_matrix));

		testAssert(isSpecialOrthogonal(rot));
		testAssert(epsMatrixEqual(rot * rest, m, 1.0e-6f));
		}
	}
}


#endif // BUILD_TESTS
