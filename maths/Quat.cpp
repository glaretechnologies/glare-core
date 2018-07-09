/*=====================================================================
Quat.cpp
--------
Copyright Glare Technologies Limited 2018 - 
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
#include "../utils/Vector.h"
#include "../indigo/TestUtils.h"
#include "../indigo/globals.h"
#include "Vec4f.h"


// Reference non-SSE code for toMatrix().
// Adapted from Quaternions - Ken Shoemake
// http://www.cs.ucr.edu/~vbz/resources/quatut.pdf
template <class Real> static void toMatrixRef(const Quatf& quat, Matrix4f& mat)
{
	const Vec4f v = quat.v;

	const Real Nq = quat.norm();
	const Real s = (Nq > (Real)0.0) ? ((Real)2.0 / Nq) : (Real)0.0;
	const Real xs = v.x[0]*s, ys = v.x[1]*s, zs = v.x[2]*s;
	const Real wx = v.x[3]*xs, wy = v.x[3]*ys, wz = v.x[3]*zs;
	const Real xx = v.x[0]*xs, xy = v.x[0]*ys, xz = v.x[0]*zs;
	const Real yy = v.x[1]*ys, yz = v.x[1]*zs, zz = v.x[2]*zs;

	/*
	Matrix4f layout:
	0	4	8	12
	1	5	9	13
	2	6	10	14
	3	7	11	15
	*/
	mat.e[0] = (Real)1.0 - (yy + zz);
	mat.e[4] = xy - wz;
	mat.e[8] = xz + wy;
	mat.e[1] = xy + wz;
	mat.e[5] = (Real)1.0 - (xx + zz);
	mat.e[9] = yz - wx;
	mat.e[2] = xz - wy;
	mat.e[6] = yz + wx;
	mat.e[10] = (Real)1.0 - (xx + yy);

	mat.e[3] = mat.e[7] = mat.e[11] = mat.e[12] = mat.e[13] = mat.e[14] = 0.0f;
	mat.e[15] = 1.0f;
}


template <class Real> static const Quat<Real> refQuatMul(const Quat<Real>& a, const Quat<Real>& b)
{
	const Vec4f a_v = maskWToZero(a.v);
	const Vec4f b_v = maskWToZero(b.v);
	const float a_w = a.v[3];
	const float b_w = b.v[3];

	const Vec4f prod_v = ::crossProduct(a_v, b_v) + b_v * a_w + a_v * b_w;
	const float prod_w = a_w * b_w - ::dot(a_v, b_v);

	return Quat<Real>(Vec4f(prod_v[0], prod_v[1], prod_v[2], prod_w));
}


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

	//================== unary operator - ====================
	{
		Quatf a(Vec3f(1, 2, 3), 4);
		testAssert(epsEqual(-a, Quatf(Vec3f(-1, -2, -3), -4)));
	}

	//================== operator * (float) ====================
	{
		Quatf a(Vec3f(1, 2, 3), 4);
		testAssert(epsEqual(a * 2.0f, Quatf(Vec3f(2, 4, 6), 8)));
	}

	//================== operator * (Quat) ====================
	{
		Quatf a(Vec3f(1, 2, 3), 4);
		Quatf b(Vec3f(5, 6, 7), 8);

		Quatf ref_prod = refQuatMul(a, b);
		Quatf prod = a * b;
		testAssert(epsEqual(ref_prod, prod));
	}
	{
		Quatf a(Vec3f(-10, 20, -30), -40);
		Quatf b(Vec3f(50, -60, 70), 80);

		Quatf ref_prod = refQuatMul(a, b);
		Quatf prod = a * b;
		testAssert(epsEqual(ref_prod, prod));
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
		Quatf a(Vec4f(1, 2, 3, 4));
		testAssert(epsEqual(a.inverse(), Quatf(Vec4f(-1, -2, -3, 4) / 30.f)));
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
	/*{
		Quatf identity = Quatf::identity();
		Quatf q = Quatf::fromAxisAndAngle(Vec3f(0,0,1), 20.0);

		Matrix4f mat;
		//Maths::lerp(identity, q, 0.5f).toMatrix(mat);

		Matrix4f rot = Matrix4f::rotationMatrix(Vec4f(0,0,1,0), 10.0);

		testAssert(epsEqual(mat, rot));
	}*/

	// Test rotateVector and toMatrix give the same rotations.
	{
		MTwister rng(1);
		const int num = 1000;
		for(int i=0; i<num; ++i)
		{
			const Vec4f axis = normalise(Vec4f(-1 + 2*rng.unitRandom(), -1 + 2*rng.unitRandom(), -1  + 2*rng.unitRandom(), 0));
			const float angle = -10 + 20*rng.unitRandom();
			const Quatf q = Quatf::fromAxisAndAngle(Vec3f(axis), angle);

			const Matrix4f ref_rot_matrix = Matrix4f::rotationMatrix(axis, angle);

			Matrix4f m;
			q.toMatrix(m);

			// Test gives same result as toMatrixRef()
			Matrix4f ref_m;
			toMatrixRef<float>(q, ref_m);
			testAssert(epsEqual(m, ref_m));


			const Vec4f a(-1 + 2*rng.unitRandom(), -1 + 2*rng.unitRandom(), -1 + 2*rng.unitRandom(), 0);
			const Vec4f m_a = m * a;
			const Vec4f rotated_a = q.rotateVector(a);
			testAssert(epsEqual(m_a, rotated_a));
			testAssert(epsEqual(ref_rot_matrix * a, rotated_a)); // Test against the rotation with ref_rot_matrix.

			// Test with a position vector
			const Vec4f b(-1 + 2*rng.unitRandom(), -1 + 2*rng.unitRandom(), -1 + 2*rng.unitRandom(), 1);
			const Vec4f m_b = m * b;
			const Vec4f rotated_b = q.rotateVector(b);
			testAssert(epsEqual(m_b, rotated_b));

			// Test inverse rotation
			const Vec4f inv_m_a = m.transposeMult(a);
			const Vec4f inv_rotated_a = q.inverseRotateVector(a);
			testAssert(epsEqual(inv_m_a, inv_rotated_a));
			testAssert(epsEqual(ref_rot_matrix.transposeMult(a), inv_rotated_a)); // Test against the rotation with ref_rot_matrix.

			// Test inverse rotation with position vector
			const Vec4f inv_m_b = m.transposeMult(b);
			const Vec4f inv_rotated_b = q.inverseRotateVector(b);
			testAssert(epsEqual(inv_m_b, inv_rotated_b));
		}
	}

	//================================== fromMatrix() =======================================
	{
		{
			const Matrix4f rot_matrix = Matrix4f::rotationMatrix(Vec4f(0,0,1,0), Maths::pi_2<float>());

			// Convert rotation matrix back to quaternion
			Quatf q2 = Quatf::fromMatrix(rot_matrix);

			// Convert back to matrix again
			Matrix4f rot_matrix2;
			q2.toMatrix(rot_matrix2);

			testAssert(::epsEqual(rot_matrix, rot_matrix2));
		}

		{
			const Matrix4f rot_matrix = Matrix4f::rotationMatrix(Vec4f(0,0,1,0), Maths::pi<float>());

			//Quatf q = Quatf::fromAxisAndAngle(Vec3f(0,0,1), Maths::pi<float>());

			// Convert rotation matrix back to quaternion
			Quatf q2 = Quatf::fromMatrix(rot_matrix);

			// Convert back to matrix again
			Matrix4f rot_matrix2;
			q2.toMatrix(rot_matrix2);

			testAssert(::epsEqual(rot_matrix, rot_matrix2));
		}

		MTwister rng(1);
		const int num = 1000;
		for(int i=0; i<num; ++i)
		{
			// Make random normalised quaternion
			const Vec4f axis = normalise(Vec4f(-1 + 2*rng.unitRandom(), -1 + 2*rng.unitRandom(), -1  + 2*rng.unitRandom(), 0));
			const float angle = -10 + 20*rng.unitRandom();
			const Quatf q = Quatf::fromAxisAndAngle(Vec3f(axis), angle);

			// Convert to rotation matrix
			//const Matrix3f ref_rot_matrix = Matrix3f::rotationMatrix(Vec3f(axis), angle);
			Matrix4f rot_matrix;
			q.toMatrix(rot_matrix);

			// Convert rotation matrix back to quaternion
			Quatf q2 = Quatf::fromMatrix(rot_matrix);

			// Convert back to matrix again
			Matrix4f rot_matrix2;
			q2.toMatrix(rot_matrix2);

			testAssert(::epsEqual(rot_matrix, rot_matrix2));
		}
	}


	//================================== slerp() =======================================
	{
		Quatf a = Quatf::fromAxisAndAngle(Vec3f(1,0,0), 0.2f);
		Quatf b = Quatf::fromAxisAndAngle(Vec3f(1,0,0), 1.2f);

		for(int i=0; i<100; ++i)
		{
			float t = (float)i / (100 - 1);
			Quatf lerped = Quatf::slerp(a, b, t);
			testAssert(epsEqual(lerped, Quatf::fromAxisAndAngle(Vec3f(1,0,0), Maths::lerp(0.2f, 1.2f, t))));
		}
	}

	// Test slerp for identical inputs
	{
		Quatf a = Quatf::fromAxisAndAngle(Vec3f(1,0,0), 0.2f);
		Quatf b = Quatf::fromAxisAndAngle(Vec3f(1,0,0), 0.2f);

		for(int i=0; i<100; ++i)
		{
			float t = (float)i / (100 - 1);
			Quatf lerped = Quatf::slerp(a, b, t);
			testAssert(epsEqual(lerped, Quatf::fromAxisAndAngle(Vec3f(1,0,0), 0.2f)));
		}
	}

	// Test slerp for a pi rotation
	{
		Quatf a = Quatf::fromAxisAndAngle(Vec3f(1,0,0), 0.0f);
		Quatf b = Quatf(Vec4f(1,0,0,0)); // u = axis*sin(omega) = axis*sin(theta/2) = axis*sin(pi/2) = axis
		// w = cos(omega) = cos(pi/2) = 0.

		for(int i=0; i<100; ++i)
		{
			float t = (float)i / (100 - 1);
			Quatf lerped = Quatf::slerp(a, b, t);
			testAssert(epsEqual(lerped, Quatf::fromAxisAndAngle(Vec3f(1,0,0), Maths::lerp(0.0f, Maths::pi<float>(), t))));
		}
	}

	// Test slerp for a 2pi rotation.  We want there to be no rotation done for intermediate t values here.
	// In particular, b should get negated so it ends up equal to a.
	{
		// u = axis*sin(omega) = axis*sin(theta/2) = (1,0,0)*sin(pi/2) = (1,0,0).    w = cos(omega) = cos(pi/2) = 0.
		Quatf a = Quatf::fromAxisAndAngle(Vec3f(1,0,0), Maths::pi<float>());

		// u = axis*sin(omega) = axis*sin(theta/2) = (1,0,0)*sin(-pi/2) = -(1,0,0).    w = cos(omega) = cos(-pi/2) = 0.
		Quatf b = Quatf::fromAxisAndAngle(Vec3f(1,0,0), -Maths::pi<float>());

		for(int i=0; i<100; ++i)
		{
			float t = (float)i / (100 - 1);
			Quatf lerped = Quatf::slerp(a, b, t);
			testAssert(epsEqual(lerped, a));
		}
	}

	//================================== nlerp() =======================================

	// Test nlerp over a smallish angle.  Should give same results as slerp to within a sufficient error bound.
	{
		Quatf a = Quatf::fromAxisAndAngle(Vec3f(1,0,0), 0.2f);
		Quatf b = Quatf::fromAxisAndAngle(Vec3f(1,0,0), 0.3f);

		for(int i=0; i<100; ++i)
		{
			float t = (float)i / (100 - 1);
			Quatf lerped = Quatf::nlerp(a, b, t);
			testAssert(epsEqual(lerped, Quatf::fromAxisAndAngle(Vec3f(1,0,0), Maths::lerp(0.2f, 0.3f, t))));
		}
	}

	// Test nlerp for identical inputs
	{
		Quatf a = Quatf::fromAxisAndAngle(Vec3f(1,0,0), 0.2f);
		Quatf b = Quatf::fromAxisAndAngle(Vec3f(1,0,0), 0.2f);

		for(int i=0; i<100; ++i)
		{
			float t = (float)i / (100 - 1);
			Quatf lerped = Quatf::nlerp(a, b, t);
			testAssert(epsEqual(lerped, Quatf::fromAxisAndAngle(Vec3f(1,0,0), 0.2f)));
		}
	}


	// Test nlerp for a pi rotation?
	// Note sure what to test here.  The interpolated quaternion (pre-normalisation) will pass through the origin
	// so will be undefined at that point.


	// Test nlerp for a 2pi rotation.  We want there to be no rotation done for intermediate t values here.
	// In particular, b should get negated so it ends up equal to a.
	{
		// u = axis*sin(omega) = axis*sin(theta/2) = (1,0,0)*sin(pi/2) = (1,0,0).    w = cos(omega) = cos(pi/2) = 0.
		Quatf a = Quatf::fromAxisAndAngle(Vec3f(1,0,0), Maths::pi<float>());

		// u = axis*sin(omega) = axis*sin(theta/2) = (1,0,0)*sin(-pi/2) = -(1,0,0).    w = cos(omega) = cos(-pi/2) = 0.
		Quatf b = Quatf::fromAxisAndAngle(Vec3f(1,0,0), -Maths::pi<float>());

		for(int i=0; i<100; ++i)
		{
			float t = (float)i / (100 - 1);
			Quatf lerped = Quatf::nlerp(a, b, t);
			testAssert(epsEqual(lerped, a));
		}
	}


	//================================== perf tests =======================================
	if(false)
	{
		// Time some lerps
		{
			Timer timer;
			const int num = 1 << 16;
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

		// Time some vector rotations done with toMatrix() vs rotateVector()
		const int num = 1000;
		js::Vector<Quatf, 16> quats(num);
		for(int i=0; i<num; ++i)
		{
			float x = (float)i;
			quats[i] =  normalise(Quatf(Vec4f(x + 1, x + 2, x + 3, x + 4)));
		}
		const Vec4f v(1.f, 2.f, 3.f, 0);

		{
			Timer timer;
			float sum = 0;
			for(int i=0; i<num; ++i)
			{
				Quatf q = quats[i];
				Matrix4f m;
				q.toMatrix(m);

				const Vec4f rot = m * v;
				sum += dot(rot, rot);
			}

			conPrint("rotation with toMatrix Elapsed: " + doubleToStringNDecimalPlaces(1.0e9 * timer.elapsed() / num, 4) + " ns");
			printVar(sum);
		}

		// Time some vector rotations
		{
			Timer timer;
			float sum = 0;
			for(int i=0; i<num; ++i)
			{
				Quatf q = quats[i];
				const Vec4f rot = q.rotateVector(v);
				sum += dot(rot, rot);
			}

			conPrint("rotateVector() Elapsed:         " + doubleToStringNDecimalPlaces(1.0e9 * timer.elapsed() / num, 4) + " ns");
			printVar(sum);
		}


		// Time refQuatMul
		{
			Timer timer;
			//const int num = 1 << 16;
			Quatf sum(Vec4f(0));
			for(int i=0; i<num; ++i)
			{
				float x = (float)i;
				Quatf a(Vec3f(x, x, x + 1), x + 2);
				Quatf b(Vec3f(x + 1, x + 2, x + 3), x + 4);

				Quatf prod = refQuatMul(a, b);
				sum = sum + prod;
			}

			conPrint("refQuatMul() Elapsed: " + doubleToStringNDecimalPlaces(1.0e9 * timer.elapsed() / num, 4) + " ns");
			conPrint(sum.toString());
		}

		// Time testQuatMul
		{
			Timer timer;
			//const int num = 1 << 16;
			Quatf sum(Vec4f(0));
			for(int i=0; i<num; ++i)
			{
				float x = (float)i;
				Quatf a(Vec3f(x, x, x + 1), x + 2);
				Quatf b(Vec3f(x + 1, x + 2, x + 3), x + 4);

				Quatf prod = a * b;
				sum = sum + prod;
			}

			conPrint("quat mul Elapsed:     " + doubleToStringNDecimalPlaces(1.0e9 * timer.elapsed() / num, 4) + " ns");
			conPrint(sum.toString());
		}
	}
}


#endif
