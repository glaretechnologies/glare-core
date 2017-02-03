/*=====================================================================
Matrix4f.h
----------
Copyright Glare Technologies Limited 2013 -
=====================================================================*/
#include "Matrix4f.h"


#include "matrix3.h"
#include "vec3.h"
#include "../utils/StringUtils.h"


Matrix4f::Matrix4f(const float* data)
{
	for(unsigned int i=0; i<16; ++i)
		e[i] = data[i];
}


Matrix4f::Matrix4f(const Matrix3f& upper_left_mat, const Vec3f& translation)
{
	/*
	0	4	8	12
	1	5	9	13
	2	6	10	14
	3	7	11	15
	*/

	e[0] = upper_left_mat.e[0];
	e[4] = upper_left_mat.e[1];
	e[8] = upper_left_mat.e[2];
	e[1] = upper_left_mat.e[3];
	e[5] = upper_left_mat.e[4];
	e[9] = upper_left_mat.e[5];
	e[2] = upper_left_mat.e[6];
	e[6] = upper_left_mat.e[7];
	e[10] = upper_left_mat.e[8];

	e[12] = translation.x;
	e[13] = translation.y;
	e[14] = translation.z;

	e[3] = e[7] = e[11] = 0.0f;
	e[15] = 1.0f;
}


Matrix4f::Matrix4f(const Vec4f& col0, const Vec4f& col1, const Vec4f& col2, const Vec4f& col3)
{
	_mm_store_ps(e + 0,  col0.v);
	_mm_store_ps(e + 4,  col1.v);
	_mm_store_ps(e + 8,  col2.v);
	_mm_store_ps(e + 12, col3.v);
}


const Matrix4f Matrix4f::identity()
{
	const float data[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	return Matrix4f(data);
}


void Matrix4f::setToIdentity()
{
	e[0] = 1.f;
	e[1] = 0.f;
	e[2] = 0.f;
	e[3] = 0.f;
	e[4] = 0.f;
	e[5] = 1.f;
	e[6] = 0.f;
	e[7] = 0.f;
	e[8] = 0.f;
	e[9] = 0.f;
	e[10] = 1.f;
	e[11] = 0.f;
	e[12] = 0.f;
	e[13] = 0.f;
	e[14] = 0.f;
	e[15] = 1.f;
}


void mul(const Matrix4f& a, const Matrix4f& b, Matrix4f& result_out)
{
	/*
	0	4	8	12
	1	5	9	13
	2	6	10	14
	3	7	11	15
	*/
	for(unsigned int outcol=0; outcol<4; ++outcol)
		for(unsigned int outrow=0; outrow<4; ++outrow)
		{
			float x = 0.0f;

			for(unsigned int i=0; i<4; ++i)
				x += a.elem(outrow, i) * b.elem(i, outcol);
		
			result_out.elem(outrow, outcol) = x;
		}
}


const Matrix4f Matrix4f::operator * (const Matrix4f& rhs) const
{
	Matrix4f res;
	mul(*this, rhs, res);
	return res;
}


/*
	0	4	8	12
	1	5	9	13
	2	6	10	14
	3	7	11	15

	0 1 2
	3 4 5
	6 7 8
*/
void Matrix4f::getUpperLeftMatrix(Matrix3<float>& upper_left_mat_out) const
{
	upper_left_mat_out.e[0] = e[0];
	upper_left_mat_out.e[1] = e[4];
	upper_left_mat_out.e[2] = e[8];

	upper_left_mat_out.e[3] = e[1];
	upper_left_mat_out.e[4] = e[5];
	upper_left_mat_out.e[5] = e[9];

	upper_left_mat_out.e[6] = e[2];
	upper_left_mat_out.e[7] = e[6];
	upper_left_mat_out.e[8] = e[10];
}


void Matrix4f::setUpperLeftMatrix(const Matrix3<float>& upper_left_mat)
{
	e[0] = upper_left_mat.e[0];
	e[4] = upper_left_mat.e[1];
	e[8] = upper_left_mat.e[2];

	e[1] = upper_left_mat.e[3];
	e[5] = upper_left_mat.e[4];
	e[9] = upper_left_mat.e[5];

	e[2] = upper_left_mat.e[6];
	e[6] = upper_left_mat.e[7];
	e[10] = upper_left_mat.e[8];
}



bool Matrix4f::isInverse(const Matrix4f& A, const Matrix4f& B)
{
	Matrix4f AB, BA;
	mul(A, B, AB);
	mul(B, A, BA);
	return (epsEqual(AB, identity()) || approxEq(AB, identity())) && (epsEqual(BA, identity()) || approxEq(BA, identity()));
}


bool Matrix4f::getInverseForRandTMatrix(Matrix4f& inverse_out) const 
{
	/*
	We are assuming the matrix is a concatenation of a translation and rotation/scale/shear matrix.  In other words we are assuming the bottom row is (0,0,0,1).
	M = T R
	M^-1 = (T R)^-1 = R^-1 T^-1
	*/
	assert(e[3] == 0.f && e[7] == 0.f && e[11] == 0.f);
	
	// Get inverse of upper left matrix (R).
	Matrix3f upper_left;
	getUpperLeftMatrix(upper_left);
	Matrix3f upper_left_inverse;
	if(!upper_left.inverse(upper_left_inverse))
		return false;

	Matrix4f R_inv(upper_left_inverse, Vec3f(0,0,0));
	Matrix4f T_inv;
	T_inv.setToTranslationMatrix(-e[12], -e[13], -e[14]);
	mul(R_inv, T_inv, inverse_out);
	//assert(Matrix4f::isInverse(*this, inverse_out)); // This can fail due to imprecision for large translations.
	return true;
}


bool Matrix4f::getUpperLeftInverseTranspose(Matrix4f& inverse_trans_out) const 
{
	// Get inverse of upper left matrix (R).
	Matrix3f upper_left;
	getUpperLeftMatrix(upper_left);
	Matrix3f upper_left_inverse;
	if(!upper_left.inverse(upper_left_inverse))
		return false;

	upper_left_inverse.transpose();
	inverse_trans_out = Matrix4f(upper_left_inverse, Vec3f(0.f));

	return true;
}


void Matrix4f::setToRotationMatrix(const Vec4f& unit_axis, float angle)
{
	assert(unit_axis.isUnitLength());

	//-----------------------------------------------------------------
	//build the rotation matrix
	//see http://mathworld.wolfram.com/RodriguesRotationFormula.html
	//-----------------------------------------------------------------
	const float a = unit_axis[0];
	const float b = unit_axis[1];
	const float c = unit_axis[2];

	const float cost = std::cos(angle);
	const float sint = std::sin(angle);

	const float asint = a*sint;
	const float bsint = b*sint;
	const float csint = c*sint;

	const float one_minus_cost = 1 - cost;

	e[ 0] = a*a*one_minus_cost + cost;
	e[ 1] = a*b*one_minus_cost + csint;
	e[ 2] = a*c*one_minus_cost - bsint;
	e[ 3] = 0;
	e[ 4] = a*b*one_minus_cost - csint;
	e[ 5] = b*b*one_minus_cost + cost;
	e[ 6] = b*c*one_minus_cost + asint;
	e[ 7] = 0;
	e[ 8] = a*c*one_minus_cost + bsint;
	e[ 9] = b*c*one_minus_cost - asint;
	e[10] = c*c*one_minus_cost + cost;
	e[11] = 0;
	e[12] = 0;
	e[13] = 0;
	e[14] = 0;
	e[15] = 1;
}


const Matrix4f Matrix4f::rotationMatrix(const Vec4f& unit_axis, float angle)
{
	Matrix4f m;
	m.setToRotationMatrix(unit_axis, angle);
	return m;
}


void Matrix4f::applyUniformScale(float scale)
{
	const Vec4f scalev(scale);

	_mm_store_ps(e + 0,  mul(Vec4f(_mm_load_ps(e + 0)), scalev).v);
	_mm_store_ps(e + 4,  mul(Vec4f(_mm_load_ps(e + 4)), scalev).v);
	_mm_store_ps(e + 8,  mul(Vec4f(_mm_load_ps(e + 8)), scalev).v);
	// Last column is not scaled.
}


void Matrix4f::setToUniformScaleMatrix(float scale)
{
	e[ 0] = scale;
	e[ 1] = 0;
	e[ 2] = 0;
	e[ 3] = 0;
	e[ 4] = 0;
	e[ 5] = scale;
	e[ 6] = 0;
	e[ 7] = 0;
	e[ 8] = 0;
	e[ 9] = 0;
	e[10] = scale;
	e[11] = 0;
	e[12] = 0;
	e[13] = 0;
	e[14] = 0;
	e[15] = 1;
}


const Matrix4f Matrix4f::uniformScaleMatrix(float scale)
{
	Matrix4f m;
	m.setToUniformScaleMatrix(scale);
	return m;
}


void Matrix4f::setToScaleMatrix(float xscale, float yscale, float zscale)
{
	e[ 0] = xscale;
	e[ 1] = 0;
	e[ 2] = 0;
	e[ 3] = 0;
	e[ 4] = 0;
	e[ 5] = yscale;
	e[ 6] = 0;
	e[ 7] = 0;
	e[ 8] = 0;
	e[ 9] = 0;
	e[10] = zscale;
	e[11] = 0;
	e[12] = 0;
	e[13] = 0;
	e[14] = 0;
	e[15] = 1;
}


const Matrix4f Matrix4f::scaleMatrix(float x, float y, float z)
{
	Matrix4f m;
	m.setToScaleMatrix(x, y, z);
	return m;
}


const std::string Matrix4f::rowString(int row_index) const
{
	return ::toString(e[row_index + 0]) + " " + ::toString(e[row_index + 4]) + " " + ::toString(e[row_index + 8]) + " " + ::toString(e[row_index + 12]);
}


const std::string Matrix4f::toString() const
{
	return rowString(0) + "\n" + rowString(1) + "\n" + rowString(2) + "\n" + rowString(3);
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/Timer.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../indigo/globals.h"


static void testConstructFromVectorAndMulForVec(const Vec4f& v)
{
	const Vec4f M_i = Matrix4f::constructFromVectorAndMul(v, Vec4f(1,0,0,0));
	const Vec4f M_j = Matrix4f::constructFromVectorAndMul(v, Vec4f(0,1,0,0));
	const Vec4f M_k = Matrix4f::constructFromVectorAndMul(v, Vec4f(0,0,1,0));
	testEpsEqual(M_i.length(), 1.0f);
	testEpsEqual(M_j.length(), 1.0f);
	testEpsEqual(M_k.length(), 1.0f);
	testEpsEqual(dot(M_i, M_j), 0.f);
	testEpsEqual(dot(M_j, M_k), 0.f);
	testEpsEqual(dot(M_i, M_k), 0.f);
}


void Matrix4f::test()
{
	conPrint("Matrix4f::test()");

	// Test getInverseForRandTMatrix
	{
		Matrix4f m;
		m.setToRotationMatrix(normalise(Vec4f(0.5f, 0.6f, 0.7f, 0)), 0.3f);
		m.e[12] = 1.f;
		m.e[13] = 2.f;
		m.e[14] = 3.f;

		Matrix4f inv;
		testAssert(m.getInverseForRandTMatrix(inv));
		testAssert(isInverse(m, inv));
	}
	{
		Matrix4f m(Matrix3f(Vec3f(1.f, 0.2f, 0.1f), Vec3f(0.2f, 2.0f, 0.35f), Vec3f(0.6f, 0.1f, 3.1f)), Vec3f(1.f, 4.f, 5.f));
		Matrix4f inv;
		testAssert(m.getInverseForRandTMatrix(inv));
		testAssert(isInverse(m, inv));
	}
	{
		Matrix4f m(Matrix3f(Vec3f(1.f, 0.2f, 0.1f), Vec3f(0.2f, 2.0f, 0.35f), Vec3f(0.6f, 0.1f, 3.1f)), Vec3f(1.f, 4.f, 5.f));
		m.e[12] = 1.f;
		m.e[13] = 2.f;
		m.e[14] = 3.f;
		Matrix4f inv;
		testAssert(m.getInverseForRandTMatrix(inv));
		testAssert(isInverse(m, inv));
	}
	{
		Matrix4f m(Matrix3f(Vec3f(1.f, 0.2f, 0.1f), Vec3f(-0.2f, 2.0f, 0.35f), Vec3f(0.6f, 0.1f, 3.1f)), Vec3f(1.f, 4.f, 5.f));
		m.e[12] = 100.f;
		m.e[13] = 200.f;
		m.e[14] = 300.f;
		Matrix4f inv;
		testAssert(m.getInverseForRandTMatrix(inv));
		testAssert(isInverse(m, inv));
	}
	//{
	//	const float e[16] = {
	//		0.949394524f, 0.31114384f, 0.0428893045f, 0.000000000f, 
	//		-0.314085960f, 0.940527320f, 0.129453972f, 0.000000000f,
	//		-5.97569488e-005f, -0.136373818f, 0.990657449f, 0.000000000f,
	//		//0, 0, 0, 1.00000000f};
	//		-18.0982876f, -109.567505f, -111.967888f, 1.00000000f};

	//	Matrix4f m(e);
	//	Matrix4f inv;
	//	testAssert(m.getInverseForRandTMatrix(inv));
	//	testAssert(isInverse(m, inv));
	//}

	// Test setToRotationMatrix
	{
		Matrix4f m;
		m.setToRotationMatrix(normalise(Vec4f(0,0,1,0)), Maths::pi_2<float>());
		testAssert(epsEqual(m * Vec4f(1,0,0,0), Vec4f(0,1,0,0)));
		testAssert(epsEqual(m * Vec4f(0,1,0,0), Vec4f(-1,0,0,0)));
	}
	{
		Matrix4f m;
		m.setToRotationMatrix(normalise(Vec4f(1,0,0,0)), Maths::pi_2<float>());
		testAssert(epsEqual(m * Vec4f(0,1,0,0), Vec4f(0,0,1,0)));
		testAssert(epsEqual(m * Vec4f(0,0,1,0), Vec4f(0,-1,0,0)));
	}
	{
		Matrix4f m;
		m.setToRotationMatrix(normalise(Vec4f(0,1,0,0)), Maths::pi_2<float>());
		testAssert(epsEqual(m * Vec4f(1,0,0,0), Vec4f(0,0,-1,0)));
		testAssert(epsEqual(m * Vec4f(0,0,1,0), Vec4f(1,0,0,0)));
	}


	// Perf test //
	if(false)
	{
		// Test speed of constructFromVector()
		{
			Timer timer;

			const float e[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

			const Matrix4f m(e);

			int N = 1000000;
			Vec4f sum(0.0f);
			for(int i=0; i<N; ++i)
			{
				const Vec4f v((float)i, (float)i + 2, (float)i + 3, (float)i + 4);
				
				Matrix4f m;
				m.constructFromVector(v);
				sum += m * Vec4f(1,0,0,0);
			}

			double elapsed = timer.elapsed();
			double scalarsum = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];

			conPrint("constructFromVector time: " + ::toString(1.0e9 * elapsed / N) + " ns");
			TestUtils::silentPrint(::toString(scalarsum));
		}



		// Test speed of transposeMult()
		{
			Timer timer;

			const float e[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

			const Matrix4f m(e);

			int N = 1000000;
			Vec4f sum(0.0f);
			for(int i=0; i<N; ++i)
			{
				const Vec4f v((float)i, (float)i + 2, (float)i + 3, (float)i + 4);
				const Vec4f res = m.transposeMult(v);
				sum += res;
			}

			double elapsed = timer.elapsed();
			double scalarsum = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];

			conPrint("transposeMult time: " + ::toString(1.0e9 * elapsed / N) + " ns");
			TestUtils::silentPrint(::toString(scalarsum));
		}


		// Test speed of matrix / vec mul
		{
			Timer timer;

			const float e[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

			const Matrix4f m(e);

			int N = 1000000;
			Vec4f sum(0.0f);
			for(int i=0; i<N; ++i)
			{
				const Vec4f v((float)i, (float)i + 2, (float)i + 3, (float)i + 4);
				const Vec4f res = m * v;
				sum += res;
			}

			double elapsed = timer.elapsed();
			double scalarsum = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];

			conPrint("mul time: " + ::toString(1.0e9 * elapsed / N) + " ns");
			TestUtils::silentPrint(::toString(scalarsum));
		}
	}


	//------------- Matrix4f::identity() ----------------
	{
		const Matrix4f m = Matrix4f::identity();

		const Vec4f v(1, 2, 3, 4);
		const Vec4f res = m * v;
		testAssert(epsEqual(v, res));
	}


	//------------- Matrix4f::setToIdentity() ----------------
	{
		Matrix4f m;
		m.setToIdentity();
		testAssert(epsEqual(m, Matrix4f::identity()));

		const Vec4f v(1, 2, 3, 4);
		const Vec4f res = m * v;
		testAssert(epsEqual(v, res));
	}

	

	//-------------------------- Test setToUniformScaleMatrix() ----------------------
	{
		Matrix4f m;
		m.setToUniformScaleMatrix(2.0f);

		const Vec4f res = m * Vec4f(1, 2, 3, 4);
		testAssert(epsEqual(res, Vec4f(2, 4, 6, 4)));
	}

	//-------------------------- Test applyUniformScale() ----------------------
	{
		Matrix4f m = Matrix4f::identity();
		m.applyUniformScale(2.0f);

		const Vec4f res = m * Vec4f(1, 2, 3, 4);
		testAssert(epsEqual(res, Vec4f(2, 4, 6, 4)));
	}
	{
		Matrix4f m = Matrix4f::identity();
		m.applyUniformScale(2.0f);
		m.applyUniformScale(3.0f);

		const Vec4f res = m * Vec4f(1, 2, 3, 4);
		testAssert(epsEqual(res, Vec4f(6, 12, 18, 4)));
	}


	{
		const float e[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

		const Matrix4f m(e);

		Matrix4f transpose;
		m.getTranspose(transpose);

		/*
		0	4	8	12
		1	5	9	13
		2	6	10	14
		3	7	11	15
		*/

		//------------- test operator * (const Matrix4f& m, const Vec4f& v) ----------------
		{
			const Vec4f v(1, 0, 0, 0);
			const Vec4f res(m * v);
			testAssert(epsEqual(res, Vec4f(0,1,2,3)));
		}
		{
			const Vec4f v(0, 1, 0, 0);
			const Vec4f res(m * v);
			testAssert(epsEqual(res, Vec4f(4,5,6,7)));
		}
		{
			const Vec4f v(0, 0, 1, 0);
			const Vec4f res(m * v);
			testAssert(epsEqual(res, Vec4f(8,9,10,11)));
		}
		{
			const Vec4f v(0, 0, 0, 1);
			const Vec4f res(m * v);
			testAssert(epsEqual(res, Vec4f(12,13,14,15)));
		}

		//------------- test mul3Vector -------------
		testAssert(epsEqual(m.mul3Vector(Vec4f(1,0,0,0)), Vec4f(0,1,2,3)));
		testAssert(epsEqual(m.mul3Vector(Vec4f(1,0,0,1)), Vec4f(0,1,2,3)));
		testAssert(epsEqual(m.mul3Vector(Vec4f(0,1,0,0)), Vec4f(4,5,6,7)));
		testAssert(epsEqual(m.mul3Vector(Vec4f(0,1,0,1)), Vec4f(4,5,6,7)));
		testAssert(epsEqual(m.mul3Vector(Vec4f(0,0,1,0)), Vec4f(8,9,10,11)));
		testAssert(epsEqual(m.mul3Vector(Vec4f(0,0,1,1)), Vec4f(8,9,10,11)));
		testAssert(epsEqual(m.mul3Vector(Vec4f(0,0,0,1)), Vec4f(0,0,0,0)));

		//------------- test tranpose mult ----------------
		{
			const Vec4f v(1, 0, 0, 0);
			const Vec4f res(m.transposeMult(v));
			testAssert(epsEqual(res, Vec4f(0,4,8,12)));
		}
		{
			const Vec4f v(0, 1, 0, 0);
			const Vec4f res(m.transposeMult(v));
			testAssert(epsEqual(res, Vec4f(1,5,9,13)));
		}
		{
			const Vec4f v(0, 0, 1, 0);
			const Vec4f res(m.transposeMult(v));
			testAssert(epsEqual(res, Vec4f(2,6,10,14)));
		}
		{
			const Vec4f v(0, 0, 0, 1);
			const Vec4f res(m.transposeMult(v));
			testAssert(epsEqual(res, Vec4f(3,7,11,15)));
		}
		{
			const Vec4f v(1, 2, 3, 4);
			const Vec4f res(m.transposeMult(v));
			testAssert(epsEqual(res, Vec4f(transpose * v)));
		}

		//------------- test transposeMult3Vector -------------
		/*
		m =
		0	4	8	12
		1	5	9	13
		2	6	10	14
		3	7	11	15
		*/
		testAssert(epsEqual(m.transposeMult3Vector(Vec4f(1,0,0,0)), Vec4f(0,4,8,0)));
		testAssert(epsEqual(m.transposeMult3Vector(Vec4f(1,0,0,1)), Vec4f(0,4,8,0)));
		testAssert(epsEqual(m.transposeMult3Vector(Vec4f(0,1,0,0)), Vec4f(1,5,9,0)));
		testAssert(epsEqual(m.transposeMult3Vector(Vec4f(0,1,0,1)), Vec4f(1,5,9,0)));
		testAssert(epsEqual(m.transposeMult3Vector(Vec4f(0,0,1,0)), Vec4f(2,6,10,0)));
		testAssert(epsEqual(m.transposeMult3Vector(Vec4f(0,0,1,1)), Vec4f(2,6,10,0)));
		testAssert(epsEqual(m.transposeMult3Vector(Vec4f(0,0,0,1)), Vec4f(0,0,0,0)));
		testAssert(epsEqual(m.transposeMult3Vector(Vec4f(1,1,1,1)), Vec4f(3,15,27,0)));
	}
	{
		// Test affine transformation
		const Matrix4f m(Matrix3f::identity(), Vec3f(1, 2, 3));

		{
			const Vec4f v(5, 6, 7, 1);
			const Vec4f res(m * v);
			testAssert(epsEqual(res, Vec4f(6, 8, 10, 1)));
		}
		{
			const Vec4f v(5, 6, 7, 0);
			const Vec4f res(m * v);
			testAssert(epsEqual(res, Vec4f(5, 6, 7, 0)));
		}
	}

	{
		// Test matrix multiplication
		const Matrix4f m = Matrix4f::identity();
		const Matrix4f m2 = Matrix4f::identity();

		Matrix4f product;
		mul(m2, m, product);
		testAssert(product == Matrix4f::identity());

		mul(m, m2, product);
		testAssert(product == Matrix4f::identity());
	}

	{
		// Test matrix multiplication
		const float e[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
		const Matrix4f m(e);

		const float e2[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
		const Matrix4f m2(e2);

		{
			const Vec4f v(5, 6, 7, 1);
			const Vec4f res(m2 * Vec4f(m * v));

			Matrix4f product;
			mul(m2, m, product);

			const Vec4f res2(product * v);

			testAssert(epsEqual(res, res2));
		}
	}

	{
		// Test translation matrix
		Matrix4f m;
		m.setToTranslationMatrix(1, 2, 3);

		{
		const Vec4f v(10, 20, 30, 1.0);
		const Vec4f res(m * v);
		
		testAssert(epsEqual(res, Vec4f(11, 22, 33, 1.0f)));
		}
		{
		const Vec4f v(10, 20, 30, 0.0);
		const Vec4f res(m * v);
		
		testAssert(epsEqual(res, Vec4f(10, 20, 30, 0.0f)));
		}
	}



	//-------------------------- Test constructFromVectorAndMul() ----------------------
	{
		{
			const Vec4f v = Vec4f(0.482021928f, -0.00631375983f, -0.876137257f, 0.000000000);
			//printVar(v.length());
			testConstructFromVectorAndMulForVec(v);
		}
		//testConstructFromVectorAndMulForVec(normalise(Vec4f(0.482021928f, -0.00631375983f, -0.876137257f, 0.000000000)));

		testConstructFromVectorAndMulForVec(Vec4f(1,0,0,0));
		testConstructFromVectorAndMulForVec(Vec4f(0,1,0,0));
		testConstructFromVectorAndMulForVec(Vec4f(0,0,1,0));
		testConstructFromVectorAndMulForVec(Vec4f(-1,0,0,0));
		testConstructFromVectorAndMulForVec(Vec4f(0,-1,0,0));
		testConstructFromVectorAndMulForVec(Vec4f(0,0,-1,0));// This should trigger the special case in constructFromVectorAndMul().

		// More vectors around special case threshold (if(vec[2] > -0.999999f)):
		testConstructFromVectorAndMulForVec(Vec4f(0,0,-0.999999f,0));
		testConstructFromVectorAndMulForVec(Vec4f(0,0,-0.9999991f,0));
		testConstructFromVectorAndMulForVec(Vec4f(0,0,-0.9999998f,0));

		testConstructFromVectorAndMulForVec(normalise(Vec4f(0.00001f,0,-0.9998f,0)));
		testConstructFromVectorAndMulForVec(normalise(Vec4f(0.00001f,0,-0.99998f,0)));
		testConstructFromVectorAndMulForVec(normalise(Vec4f(0.00001f,0,-0.999998f,0)));
		testConstructFromVectorAndMulForVec(normalise(Vec4f(0.00001f,0,-0.9999998f,0)));

		// Test around v.z = 0.999f
		testConstructFromVectorAndMulForVec(Vec4f((float)std::sin(std::acos(0.9989)), 0.f, 0.9989f, 0.f));
		testConstructFromVectorAndMulForVec(Vec4f((float)std::sin(std::acos(0.999)), 0.f, 0.999f, 0.f));
		testConstructFromVectorAndMulForVec(Vec4f((float)std::sin(std::acos(0.9991)), 0.f, 0.9991f, 0.f));

		for(float theta = Maths::pi<float>() - 0.01f; theta <= Maths::pi<float>(); theta += 0.00001f)
		{
			const Vec4f v = Vec4f(sin(theta), 0.f, cos(theta), 0.0f);
			testConstructFromVectorAndMulForVec(v);
		}
	}
}


#endif // BUILD_TESTS
