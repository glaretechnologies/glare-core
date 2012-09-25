#ifndef __JS_TRIANGLE
#define __JS_TRIANGLE

#include "../maths/vec3.h"
#include "../maths/plane.h"
//#include "jscol_ray.h"
#include "../simpleraytracer/ray.h"
#include <assert.h>

namespace js
{

class Triangle
{
public:
	inline Triangle(){}
//	inline Triangle(const Triangle& rhs);
	inline Triangle(const Vec3f& v1, const Vec3f& v2, const Vec3f& v3);

	//inline Triangle& operator = (const Triangle& rhs);
	inline bool operator == (const Triangle& rhs) const;

	inline void setVertices(const Vec3f& v1, const Vec3f& v2, const Vec3f& v3);

	inline Vec3f& getVertex(int index);
	inline const Vec3f& getVertex(int index) const;

	inline Vec3f& v0(){ return v[0]; }
	inline Vec3f& v1(){ return v[1]; }
	inline Vec3f& v2(){ return v[2]; }

	inline const Vec3f& v0() const { return v[0]; }
	inline const Vec3f& v1() const { return v[1]; }
	inline const Vec3f& v2() const { return v[2]; }

//	inline Vec3f& getNormal(){ return normal; }

	//NOTE: slow, built on demand right now
	inline const Vec3f getNormal() const;// const { return normal; }

	//virtual inline void draw();

	//virtual inline const Vec3f& getPos();

	//inline bool hitByRay(float& t_out, Vec3f& normal_out, const Vec3f& raystart_os, const Vec3f& rayend_os) const;
	inline float traceRay(const ::Ray& ray);
	float traceRayMolTrum(const ::Ray& ray);

	//point must be in plane
	bool pointInTri(const Vec3f& point) const;


	//closest point on triangle to target.  target must be in triangle plane
	const Vec3f closestPointOnTriangle(const Vec3f& target) const;

//	inline const Plane& getTriPlane() const { return triplane; }

	//TEMP SLOW
	//inline const Plane getTriPlane() const;

private:

	//const Plane getEdgePlane(int index) const;

	//each Vec3f is 12 bytes.. so 36 bytes all up for the verts.
	Vec3f v[3];
//	Vec3 normal;
	//Vec3 planenormal[3];
//	Plane edgeplanes[3];w
//	Plane triplane;//plane triangle lies in

//	void calcNormal();//inline this?
	//void buildPlanes();

};


/*Triangle::Triangle(const Triangle& rhs)
{
	v[0] = rhs.v[0];
	v[1] = rhs.v[1];
	v[2] = rhs.v[2];
	normal = rhs.normal;
	edgeplanes[0] = rhs.edgeplanes[0];
	edgeplanes[1] = rhs.edgeplanes[1];
	edgeplanes[2] = rhs.edgeplanes[2];
	triplane = rhs.triplane;
}*/

Triangle::Triangle(const Vec3f& v1, const Vec3f& v2, const Vec3f& v3)
{
	v[0] = v1;
	v[1] = v2;
	v[2] = v3;
	
//	buildPlanes();
}



/*Triangle& Triangle::operator = (const Triangle& rhs)
{
	v[0] = rhs.v[0];
	v[1] = rhs.v[1];
	v[2] = rhs.v[2];
	normal = rhs.normal;*/
/*	edgeplanes[0] = rhs.edgeplanes[0];
	edgeplanes[1] = rhs.edgeplanes[1];
	edgeplanes[2] = rhs.edgeplanes[2];
	triplane = rhs.triplane;*/
/*
	return *this;
}*/

bool Triangle::operator == (const Triangle& rhs) const
{
	return	v[0] == rhs.v[0] &&
			v[1] == rhs.v[1] &&
			v[2] == rhs.v[2];
}

void Triangle::setVertices(const Vec3f& v1, const Vec3f& v2, const Vec3f& v3)
{
	v[0] = v1;
	v[1] = v2;
	v[2] = v3;
	
	//buildPlanes();
}



const Vec3f Triangle::getNormal() const// const { return normal; }
{
	//NOTE: this defines a counterclockwise winding order for front faces.
	return normalise(::crossProduct(v[1] - v[0], v[2] - v[0]));
}


Vec3f& Triangle::getVertex(int index)
{
	assert(index >= 0 && index < 3);
	return v[index];
}

const Vec3f& Triangle::getVertex(int index) const
{
	assert(index >= 0 && index < 3);
	return v[index];
}

/*void Triangle::draw()
{
	glBegin(GL_POLYGON);

	glNormal3f(getNormal().x, getNormal().y, getNormal().z);

	glColor4f(1, 0.3, 0.2, 1);

	glVertex3f(v0().x, v0().y, v0().z);
	glVertex3f(v1().x, v1().y, v1().z);
	glVertex3f(v2().x, v2().y, v2().z);

	glEnd();
}*/

/*const Vec3f& Triangle::getPos()
{
	//return (v[0] + v[1] + v[2]) * 0.333333f;
	return v[0];
}*/

/*bool Triangle::hitByRay(float& t_out, Vec3f& normal_out, const Vec3f& raystart_os, const Vec3f& rayend_os) const
{

	const Vec3 starttoplane = v0() - raystart_os;
	const Vec3 planetoend = rayend_os - v0();

	const float starttoplane_on_normal = dotProduct(starttoplane, normal);

	const float planetoend_on_normal =  dotProduct(planetoend, normal);

	//if(!planetoend_on_normal)
	//	return true;

	*//*bool raystartbelow = starttoplane_on_normal > 0;

	bool rayendabove = planetoend_on_normal > 0;

	//-----------------------------------------------------------------
	//return false if line does not intersect triangle plane
	//-----------------------------------------------------------------
	if(raystartbelow != rayendabove)
		return false;*//*
	

	if(starttoplane_on_normal + planetoend_on_normal == 0)
		return false;
	//NOTE: make this good

	//fraction [0,1] ray got along path
	const float tracefraction = starttoplane_on_normal / (starttoplane_on_normal + planetoend_on_normal);

	//-----------------------------------------------------------------
	//return false if ray does not intersect triangle plane
	//-----------------------------------------------------------------
	if(tracefraction < 0 || tracefraction >= 1)
		return false;

	//-----------------------------------------------------------------
	//find point of intersection of ray and triangle
	//-----------------------------------------------------------------
	const Vec3 intersect_pos = raystart_os + ((rayend_os - raystart_os) * tracefraction);


	if(!pointInTri(intersect_pos))
		return false;

	//-----------------------------------------------------------------
	//if got here, intersect point is inside all edges, so return true
	//-----------------------------------------------------------------
	//intersect_point_out = intersect_pos;
	t_out = tracefraction;

	normal_out = normal;

	return true;
}*/

/*float Triangle::traceRay(const ::Ray& ray)
{
	//------------------------------------------------------------------------
	//find intersection of ray with triangle plane
	//------------------------------------------------------------------------
	//inline float rayIntersect(const Vec3& raystart, const Vec3 ray_unitdir) const;
	const float dist = triplane.rayIntersect(ray.startpos, ray.unitdir);

	if(dist < 0.0f)
		return -1.0f;

	if(pointInTri(ray.startpos + ray.unitdir * dist))
	{
		return dist;
	}
	else
	{
		return -1.0f;
	}
}


//point must be in plane
bool Triangle::pointInTri(const Vec3& point) const
{
	//-----------------------------------------------------------------
	//check inside edge 0 (v0 to v1)
	//-----------------------------------------------------------------
	if(edgeplanes[0].pointOnFrontSide(point))
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
		return false;

	return true;
}*/


/*const Plane Triangle::getTriPlane() const
{
	return Plane(v0(), Vec3dgetNormal());
}
*/

}//end namespace js
#endif //__JS_TRIANGLE
