#include "jscol_triangle.h"


#if 0
/*void js::Triangle::calcNormal()
{
	//Vec3 edge0 = v0() - v1();
	//Vec3 edge1 = v2() - v1();

	//counter clockwise winding order means looking at *front* face.

	const Vec3 edge0 = v1() - v0();
	const Vec3 edge1 = v2() - v0();

	//NEWCODE: swapped order of edges

	normal = crossProduct(edge0, edge1);

	normal.normalise();

//	assert( epsEqual(normal.length(), 1) );

	if(normal == Vec3(0,0,0))
		normal = Vec3(0,0,1);//bit of a hack

	//-----------------------------------------------------------------
	//build edge plane normals
	//-----------------------------------------------------------------
	*//*planenormal[0] = crossProduct(normal, v1()-v0());

	planenormal[1] = crossProduct(normal, v2()-v1());

	planenormal[2] = crossProduct(normal, v0()-v2());*//*

	//ncalced = true;
}*/
/*

void js::Triangle::buildPlanes()
{
	edgeplanes[0].setUnnormalised(v[0], crossProduct(v[1]-v[0], normal));

	edgeplanes[1].setUnnormalised(v[1], crossProduct(v[2]-v[1], normal));

	edgeplanes[2].setUnnormalised(v[2], crossProduct(v[0]-v[2], normal));

	triplane.set(v[0], normal);

	//NOTE: check me
}*/



 //closest point on line ab to p
const Vec3f closestPointOnLine(const Vec3f& a, const Vec3f& b, const Vec3f& p)
{
	// Determine t (the length of the vector from ‘a’ to ‘p’)
	Vec3f c = p - a;
	Vec3f V = b - a;//Normalized vector [b - a];
	V.normalise();
	float d = a.getDist(b);//distance from a to b;
	float t = dotProduct(V, c);

	// Check to see if ‘t’ is beyond the extents of the line segment
	
	if (t < 0) return a;
	if (t > d) return b;
 
	// Return the point between ‘a’ and ‘b’

	V *= t;//set length of V to t;
	return a + V;

	/*Vec3f c = p - a;
	Vec3f V = b - a;//Normalized vector [b - a];
	V.normalise();
   double d = a.getDist(b);//distance from a to b;
   double t = dotProduct(V, c);

	// Check to see if ‘t’ is beyond the extents of the line segment
	
	if (t < 0) return a;
	if (t > d) return b;
 
	// Return the point between ‘a’ and ‘b’

	V *= t;//set length of V to t;
	return a + V;*/
}

//returns closest point out of {Rab, Rbc, Rca} to p
const Vec3f& closest(const Vec3f& Rab, const Vec3f& Rbc, const Vec3f& Rca, const Vec3f& p)
{
	float d1 = p.getDist2(Rab);
	float d2 = p.getDist2(Rbc);
	float d3 = p.getDist2(Rca);

	if(d1<=d2 && d1<=d3)
		return Rab;

	if(d2<=d1 && d2<=d3)
		return Rbc;

	if(d3<=d1 && d3<=d2)
    return Rca;

//  ASSERT(0); // the same
	return Rab;
}


//closest point on triangle to target.  target must be in triangle plane
const Vec3f js::Triangle::closestPointOnTriangle(const Vec3f& target) const
{
	const Vec3f Rab = closestPointOnLine(v[0], v[1], target);
	const Vec3f Rbc = closestPointOnLine(v[1], v[2], target);
	const Vec3f Rca = closestPointOnLine(v[2], v[0], target);
    
	return closest(Rab, Rbc, Rca, target);
}


inline void crossProduct(const Vec3f& v1, const Vec3f& v2, Vec3f& vout)
{
	vout.x = (v1.y * v2.z) - (v1.z * v2.y);
	vout.y = (v1.z * v2.x) - (v1.x * v2.z);
	vout.z = (v1.x * v2.y) - (v1.y * v2.x);
}

inline void sub(const Vec3f& v1, const Vec3f& v2, Vec3f& vout)
{
	vout.x = v1.x - v2.x;
	vout.y = v1.y - v2.y;
	vout.z = v1.z - v2.z;
}

//http://www.acm.org/jgt/papers/MollerTrumbore97/

//#define EPSILON 0.000001
const float EPSILON = 0.0000001f;

float js::Triangle::traceRayMolTrum(const Ray& ray/*, double *t, double *u, double *v*/)
{
	float texu, texv;

	//find vectors for two edges sharing vert0
	//const Vec3f edge1 = v[1] - v[0];
	//const Vec3f edge2 = v[2] - v[0];
	Vec3f edge1, edge2;
	sub(v[1], v[0], edge1);
	sub(v[2], v[0], edge2);

	//begin calculating determinant - also used to calculate U parameter
	//const Vec3f pvec = ::crossProduct(ray.unitdir, edge2);
	Vec3f pvec;
	crossProduct(ray.unitDirF(), edge2, pvec);

	//if determinant is near zero, ray lies in plane of triangle
	const float det = edge1.dot(pvec);

#ifdef TEST_CULL           /* define TEST_CULL if culling is desired */
   if (det < EPSILON)
      return 0;

	/* calculate distance from vert0 to ray origin */
	//SUB(tvec, orig, vert0);
   const Vec3f tvec = orig - vert0;

   /* calculate U parameter and test bounds */
   u = tvec.dot(pvec);//DOT(tvec, pvec);
   if(u < 0.0 || u > det)
      return -1.0f;//0;

   /* prepare to test V parameter */
   //CROSS(qvec, tvec, edge1);
   const Vec3f qvec = ::crossProduct(tvec, edge1);
   //Vec3f qvec;
   //crossProduct(tvec, edge1, qvec);

    /* calculate V parameter and test bounds */
   v = //DOT(dir, qvec);
   if (*v < 0.0 || *u + *v > det)
      return 0;

   /* calculate t, scale parameters, ray intersects triangle */
   float t = edge2.dot(qvec);//DOT(edge2, qvec);
   const float inv_det = 1.0f / det;
   t *= inv_det;
   u *= inv_det;
   v *= inv_det;

#else                    /* the non-culling branch */

	if(det > -EPSILON && det < EPSILON)
		return -1.0f;

	const float inv_det = 1.0f / det;

	//calculate distance from vert0 to ray origin
	//const Vec3f tvec = ray.startpos - v[0];
	Vec3f tvec;
	sub(ray.startPosF(), v[0], tvec);

	//calculate U parameter and test bounds
	texu = tvec.dot(pvec) * inv_det;
	if(texu < 0.0f || texu > 1.0f)
		return -1.0f;//0;

	//prepare to test V parameter
	//const Vec3f qvec = ::crossProduct(tvec, edge1);
	Vec3f qvec;
	crossProduct(tvec, edge1, qvec);

	//calculate V parameter and test bounds
	texv = ray.unitDirF().dot(qvec) * inv_det;
	if(texv < 0.0f || texu + texv > 1.0f)
		return -1.0f;//0;

	//calculate t, ray intersects triangle
	const float t = edge2.dot(qvec) * inv_det;

#endif

   //return 1;
	return t;
}

/*const Plane js::Triangle::getEdgePlane(int index) const
{
	if(index == 0)
		return Plane(v[0], normalise(crossProduct(v[1]-v[0], getNormal())));
	else if(index == 1)
		return Plane(v[1], normalise(crossProduct(v[2]-v[1], getNormal())));
	else if(index == 2)
		return Plane(v[2], normalise(crossProduct(v[0]-v[2], getNormal())));
	else
	{
		assert(0);
		return Plane(Vec3f(0,0,1), 0);
	}

}*/

//point must be in plane
/*
bool js::Triangle::pointInTri(const Vec3f& point) const
{
	//-----------------------------------------------------------------
	//check inside edge 0 (v0 to v1)
	//-----------------------------------------------------------------
	if(getEdgePlane(0).pointOnFrontSide(point))
		return false;

	//-----------------------------------------------------------------
	//check inside edge 1 (v1 to v2)
	//-----------------------------------------------------------------
	if(getEdgePlane(1).pointOnFrontSide(point))
		return false;

	//-----------------------------------------------------------------
	//check inside edge 2 (v2 to v0)
	//-----------------------------------------------------------------
	if(getEdgePlane(2).pointOnFrontSide(point))
		return false;

	//-----------------------------------------------------------------
	//check inside edge 0 (v0 to v1)
	//-----------------------------------------------------------------
*/	/*if(edgeplanes[0].pointOnFrontSide(point))
		return false;

	//-----------------------------------------------------------------
	//check inside edge 1 (v1 to v2)
	//-----------------------------------------------------------------
	if(edgeplanes[1].pointOnFrontSide(point))
		return false;

	//-----------------------------------------------------------------
	//check inside edge 2 (v2 to v0)
	//-----------------------------------------------------------------
	if(edgeplanes[2].pointOnFrontSide(point))
		return false;*/
/*
	return true;
}*/


#endif