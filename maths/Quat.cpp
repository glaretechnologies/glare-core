/*=====================================================================
Quat.cpp
--------
Copyright Glare Technologies Limited 2014 - 
File created by ClassTemplate on Thu Mar 12 16:27:07 2009
=====================================================================*/
#include "Quat.h"


#include "../utils/StringUtils.h"


template <>
const std::string Quat<float>::toString() const
{
	return "(" + ::toString(v.x[0]) + "," + ::toString(v.x[1]) + "," + ::toString(v.x[2]) + "," + ::toString(v.x[3]) + ")";
}


template <>
const std::string Quat<double>::toString() const
{
	return "(" + ::toString(v.x[0]) + "," + ::toString(v.x[1]) + "," + ::toString(v.x[2]) + "," + ::toString(v.x[3]) + ")";
}


#if BUILD_TESTS


#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include "../utils/MTwister.h"
#include "../indigo/TestUtils.h"
#include "../indigo/globals.h"
#include "Vec4f.h"


void quaternionTests()
{
	//================== identity() ====================
	{
		const Quatf q = Quatf::identity();
		testAssert(q == Quatf(Vec3f(0, 0, 0), 1));
		testAssert(q.length() == 1);
	}

	//================== operator + ====================
	{
		Quatf a(Vec3f(1, 2, 3), 4);
		Quatf b(Vec3f(5, 6, 7), 8);
		testAssert(epsEqual(a + b, Quatf(Vec3f(6, 8, 10), 12)));
	}

	//================== operator - ====================
	{
		Quatf a(Vec3f(1, 2, 3), 4);
		Quatf b(Vec3f(5, 6, 3), 1);
		testAssert(epsEqual(a - b, Quatf(Vec3f(-4, -4, 0), 3)));
	}

	//================== operator * ====================
	{
		Quatf a(Vec3f(1, 2, 3), 4);
		testAssert(epsEqual(a * 2.0f, Quatf(Vec3f(2, 4, 6), 8)));
	}

	//================== operator == ====================
	{
		Quatf a(Vec3f(1, 2, 3), 4);
		testAssert(a == a);
	}

	//================== operator != ====================
	{
		Quatf a(Vec3f(1, 2, 3), 4);
		Quatf b(Vec3f(1, 2, 4), 5);
		testAssert(a != b);
	}

	//================== norm() ====================
	{
		Quatf a(Vec3f(1, 2, 3), 4);
		testAssert(epsEqual(a.norm(), 30.f));
	}

	//================== length() ====================
	{
		Quatf a(Vec3f(1, 2, 3), 4);
		testAssert(epsEqual(a.length(), std::sqrt(30.f)));
	}

	//================== inverse() ====================
	{
		//TODO
	}

	//================== conjugate() ====================
	{
		Quatf a(Vec3f(1, 2, 3), 4);
		testAssert(a.conjugate() == Quatf(Vec3f(-1, -2, -3), 4));
	}

	//================== normalise() ====================
	{
		Quatf a(Vec3f(1, 2, 3), 4);
		testAssert(epsEqual(normalise(a), Quatf(Vec3f(1/std::sqrt(30.f), 2/std::sqrt(30.f), 3/std::sqrt(30.f)), 4/std::sqrt(30.f))));
	}

	//================== dotProduct() ====================
	{
		Quatf a(Vec3f(1, 2, 3), 4);
		Quatf b(Vec3f(5, 6, 3), 2);
		testAssert(epsEqual(dotProduct(a, b), 34.f));
	}


	// Test a rotation
	/*{
		const Vec3f unit_axis(0,0,1);
		const float angle = Maths::pi_2<float>(); // 90 degrees
		const float omega = angle * 0.5f;
		Quatf q(unit_axis * sin(omega), cos(omega)); // Make complex vector twice as long

		Matrix4f m;
		q.toMatrix(m);
		Vec4f rotated = m * Vec4f(1,0,0,0);

		printVar(rotated); // Should be (0, 1, 0, 0)
		const float theta = std::atan2(rotated.x[1], rotated.x[0]);
		printVar(theta);
	}*/


	// Test an interpolated rotation
	{
		Quatf identity = Quatf::identity();
		Quatf q = Quatf::fromAxisAndAngle(Vec3f(0,0,1), 20.0);

		Matrix3f mat;
		Maths::lerp(identity, q, 0.5f).toMatrix(mat);

		Matrix3f rot = Matrix3f::rotationMatrix(Vec3f(0,0,1), 10.0);

		testAssert(epsMatrixEqual(mat, rot));
	}

	// Test rotateVector and toMatrix give the same rotations.
	{
		MTwister rng(1);
		const int num = 1000;
		for(int i=0; i<num; ++i)
		{
			const Vec4f axis = normalise(Vec4f(-1 + 2*rng.unitRandom(), -1 + 2*rng.unitRandom(), -1  + 2*rng.unitRandom(), 0));
			const float angle = -10 + 20*rng.unitRandom();
			const Quatf q = Quatf::fromAxisAndAngle(Vec3f(axis), angle);

			const Matrix3f ref_rot_matrix = Matrix3f::rotationMatrix(Vec3f(axis), angle);

			Matrix4f m;
			q.toMatrix(m);

			const Vec4f a(-1 + 2*rng.unitRandom(), -1 + 2*rng.unitRandom(), -1 + 2*rng.unitRandom(), 0);
			const Vec4f m_a = m * a;
			const Vec4f rotated_a = q.rotateVector(a);
			testAssert(epsEqual(m_a, rotated_a));
			testAssert(epsEqual(ref_rot_matrix * Vec3f(a), Vec3f(rotated_a))); // Test against the rotation with ref_rot_matrix.

			// Test with a position vector
			const Vec4f b(-1 + 2*rng.unitRandom(), -1 + 2*rng.unitRandom(), -1 + 2*rng.unitRandom(), 1);
			const Vec4f m_b = m * b;
			const Vec4f rotated_b = q.rotateVector(b);
			testAssert(epsEqual(m_b, rotated_b));

			// Test inverse rotation
			const Vec4f inv_m_a = m.transposeMult(a);
			const Vec4f inv_rotated_a = q.inverseRotateVector(a);
			testAssert(epsEqual(inv_m_a, inv_rotated_a));
			testAssert(epsEqual(ref_rot_matrix.transposeMult(Vec3f(a)), Vec3f(inv_rotated_a))); // Test against the rotation with ref_rot_matrix.

			// Test inverse rotation with position vector
			const Vec4f inv_m_b = m.transposeMult(b);
			const Vec4f inv_rotated_b = q.inverseRotateVector(b);
			testAssert(epsEqual(inv_m_b, inv_rotated_b));
		}

	}

	// Time some lerps
	{
		Timer timer;
		const int num = 1000;
		float sum = 0;
		for(int i=0; i<num; ++i)
		{
			float x = (float)i;
			Quatf a(Vec3f(x, x, x + 1), x + 2);
			Quatf b(Vec3f(x + 1, x + 2, x + 3), x + 4);

			Quatf c = Quatf::nlerp(a, b, x * 0.00001f);
			sum += c.norm();
		}

		conPrint("nlerp() Elapsed: " + doubleToStringNDecimalPlaces(1.0e9 * timer.elapsed() / num, 4) + " ns");
		printVar(sum);
	}

	// Time some vector rotations done with toMatrix()
	{
		Timer timer;
		const int num = 1000;
		float sum = 0;
		for(int i=0; i<num; ++i)
		{
			float x = (float)i;
			Quatf q(Vec3f(x + 1, x + 2, x + 3), x + 4);

			const Vec4f v(x, x+1, x+2, 0);

			Matrix4f m;
			q.toMatrix(m);

			const Vec4f rot = m * v;
			sum += dot(rot, rot);
		}

		conPrint("rotateVector() with matrix Elapsed: " + doubleToStringNDecimalPlaces(1.0e9 * timer.elapsed() / num, 4) + " ns");
		printVar(sum);
	}

	// Time some vector rotations
	{
		Timer timer;
		const int num = 1000;
		float sum = 0;
		for(int i=0; i<num; ++i)
		{
			float x = (float)i;
			Quatf q(Vec3f(x + 1, x + 2, x + 3), x + 4);

			const Vec4f v(x, x+1, x+2, 0);

			const Vec4f rot = q.rotateVector(v);
			sum += dot(rot, rot);
		}

		conPrint("rotateVector() Elapsed: " + doubleToStringNDecimalPlaces(1.0e9 * timer.elapsed() / num, 4) + " ns");
		printVar(sum);
	}
}


#endif
