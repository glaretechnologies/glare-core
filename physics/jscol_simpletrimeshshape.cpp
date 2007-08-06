#include "jscol_simpletrimeshshape.h"

	
//#include "../cyberspace/globals.h"


js::SimpleTriMeshShape::SimpleTriMeshShape(const TriMesh& trimesh)
:	tris(trimesh)	//NOTE: expensive triangle copy operation here
{

	//assert(trimesh.getNumTris() > 0);
	if(trimesh.getNumTris() == 0)
	{
		bbox_os.enlargeToHold(Vec3(0,0,0));//just make some kind of NULL bbox
	}
	else
	{
		//-----------------------------------------------------------------
		//calc bounding box
		//-----------------------------------------------------------------
		for(TriMesh::const_tri_iterator i = tris.trisBegin(); i != tris.trisEnd(); ++i)
			for(int t=0; t<3; ++t)
			{
				bbox_os.enlargeToHold((*i).getVertex(t));
			}
	}


	//------------------------------------------------------------------------
	//construct kd tree
	//------------------------------------------------------------------------
	for(TriMesh::const_tri_iterator i = tris.trisBegin(); i != tris.trisEnd(); ++i)
		tritree.insertTri(*i);

	tritree.build();
}

js::SimpleTriMeshShape::~SimpleTriMeshShape()
{
}


float js::SimpleTriMeshShape::getRayDist(const ::Ray& ray)
{
	//js::Triangle* hit_tri = NULL;
	//js::EdgeTri* hit_tri = NULL;
	unsigned int hit_tri_index;
	float u, v;
	return tritree.traceRay(ray, 1e20f, hit_tri_index, u, v);
}


/*float js::SimpleTriMeshShape::traceRay(const ::Ray& ray, unsigned int& hit_tri_out, 
									   float& u_out, float& v_out, Vec3& normal_out)
{

	//js::EdgeTri* hit_tri = NULL;
	const float dist = tritree.traceRay(ray, hit_tri_out, u_out, v_out);

	if(dist >= 0.0f)
	{
		//assert(hit_tri);

		//normal_out = hit_tri->getNormal();
		normal_out = tritree.getEdgeTri(hit_tri_out).getNormal();
	}

	return dist;
}*/



/*bool js::SimpleTriMeshShape::lineStabsShape(float& t_out, Vec3& normal_os_out, const Vec3& raystart_os, const Vec3& rayend_os)
{
	//-----------------------------------------------------------------
	//check against bounding sphere
	//-----------------------------------------------------------------
	//if(!boundingsphere.lineStabsShape(raystart_os, rayend_os))
	//	return false;

	//debugPrint("line hit bounding sphere");



	//-----------------------------------------------------------------
	//check against each triangle in the mesh
	//-----------------------------------------------------------------
	
	float smallest_t = 1.0f;
	bool ray_hit_a_tri = false;
	Vec3 normal;

	for(TriMesh::const_tri_iterator i = tris.trisBegin(); i != tris.trisEnd(); ++i)
	{
		float t;
		const Triangle& tri = (*i);
		if(tri.hitByRay(t, normal, raystart_os, rayend_os))
		{
			ray_hit_a_tri = true;

			if(t < smallest_t)
			{
				smallest_t = t; 
				normal_os_out = normal;
			}
		}
	}


	t_out = smallest_t;
	return ray_hit_a_tri;
}*/



//returns true if sphere hit object.
bool js::SimpleTriMeshShape::traceSphere(const BoundingSphere& sphere, const Vec3& translation_os,	
										float& dist_out, Vec3& normal_os_out)//, bool& embedded_out,
										//Vec3& disembed_vec_out)
{
//	embedded_out = false;
//	disembed_vec_out = Vec3(0,0,0);

	Vec3 unitdir = translation_os;
	const float translation_len = unitdir.normalise_ret_length();

	const Vec3 sourcePoint = sphere.getCenter();

	float smallest_dist = 1e9;//smallest dist until hit a tri
	bool sphere_hit_a_tri = false;

	for(TriMesh::const_tri_iterator i = tris.trisBegin(); i != tris.trisEnd(); ++i)
	{
		const Triangle& tri = (*i);

		//-----------------------------------------------------------------
		//Determine the distance from the plane to the sphere center
		//-----------------------------------------------------------------
		float pDist = tri.getTriPlane().signedDistToPoint(sourcePoint);

		//-----------------------------------------------------------------
		//Invert normal if doing backface collision, so 'usenormal' is always facing
		//towards sphere center.
		//-----------------------------------------------------------------
		Vec3 usenormal = tri.getNormal();

		if(pDist < 0)
		{
			usenormal *= -1;
			pDist *= -1;
		}


		assert(pDist >= 0);

		//-----------------------------------------------------------------
		//check if sphere is heading away from tri
		//-----------------------------------------------------------------
		const float approach_rate = -usenormal.dot(unitdir);
		if(approach_rate <= 0)
			continue;
	

		assert(approach_rate > 0);

		//trans_len_needed = dist to approach / dist approached per unit translation len
		const float trans_len_needed = (pDist - sphere.getRadius()) / approach_rate;

		if(translation_len < trans_len_needed)
			continue;//then sphere will never get to plane



		//-----------------------------------------------------------------
		//calc the point where the sphere intersects with the triangle plane (planeIntersectionPoint)
		//-----------------------------------------------------------------
		Vec3 planeIntersectionPoint;


		// Is the plane embedded in the sphere?
		if(trans_len_needed <= 0)//pDist <= sphere.getRadius())//make == trans_len_needed < 0
      {
			// Calculate the plane intersection point
			planeIntersectionPoint = tri.getTriPlane().closestPointOnPlane(sourcePoint);

		}
      else
      {
        
			assert(trans_len_needed >= 0);

			planeIntersectionPoint = sourcePoint + (unitdir * trans_len_needed) -
																(sphere.getRadius() * usenormal);

			//assert point is actually on plane
			assert(epsEqual(tri.getTriPlane().signedDistToPoint(planeIntersectionPoint), 0.0f, 0.0001f));
		}



		//-----------------------------------------------------------------
		//now restrict collision point on tri plane to inside tri if neccessary.
		//-----------------------------------------------------------------
		Vec3 triIntersectionPoint = planeIntersectionPoint;
		
		const bool point_in_tri = tri.pointInTri(triIntersectionPoint);
		if(!point_in_tri)
		{
			//-----------------------------------------------------------------
			//restrict to inside tri
			//-----------------------------------------------------------------
			triIntersectionPoint = tri.closestPointOnTriangle(triIntersectionPoint);
		}


		if(triIntersectionPoint.getDist2(sphere.getCenter()) < sphere.getRadius2())
		{
#ifndef NOT_CYBERSPACE
			//::debugPrint("jscol: WARNING: tri embedded in sphere");
#endif
			//-----------------------------------------------------------------
			//problem, so just ignore this tri :)
			//-----------------------------------------------------------------
			//continue;

			smallest_dist = 0;
			sphere_hit_a_tri = true;
			normal_os_out = usenormal;
			break;//don't test against other tris?
		}

		
		//-----------------------------------------------------------------
		//Using the triIntersectionPoint, we need to reverse-intersect
		//with the sphere
		//-----------------------------------------------------------------
		
		//returns dist till hit sphere or -1 if missed
		//inline float rayIntersect(const Vec3& raystart_os, const Vec3& rayunitdir) const;
		const float dist = sphere.rayIntersect(triIntersectionPoint, unitdir * -1.0f);
		
		if(dist >= 0 && dist < smallest_dist && dist < translation_len)
		{
			smallest_dist = dist;
			sphere_hit_a_tri = true;

			//-----------------------------------------------------------------
			//calc hit normal
			//-----------------------------------------------------------------
			if(point_in_tri)
				normal_os_out = usenormal;
			else
			{
				//-----------------------------------------------------------------
				//calc point sphere will be when it hits edge of tri
				//-----------------------------------------------------------------
				const Vec3 hit_spherecenter = sourcePoint + unitdir * dist;

				assert(epsEqual(hit_spherecenter.getDist(triIntersectionPoint), sphere.getRadius()));

				normal_os_out = (hit_spherecenter - triIntersectionPoint) / sphere.getRadius(); 
			}
		}

	}//end for each triangle

	dist_out = smallest_dist;

	return sphere_hit_a_tri;

}






void js::SimpleTriMeshShape::appendCollPoints(std::vector<Vec3>& points_ws, const BoundingSphere& sphere_os,
											const CoordFrame& frame)
{
	const Vec3& sphere_center = sphere_os.getCenter();


	for(TriMesh::const_tri_iterator i = tris.trisBegin(); i != tris.trisEnd(); ++i)
	{
		const Triangle& tri = (*i);

		//const Plane triplane(tri.v0(), tri.getNormal());

		//-----------------------------------------------------------------
		//see if sphere is touching plane
		//-----------------------------------------------------------------
		const float disttoplane = tri.getTriPlane().signedDistToPoint(sphere_center);

		if(fabs(disttoplane) > sphere_os.getRadius())
			continue;


		//-----------------------------------------------------------------
		//get closest point on plane to sphere center
		//-----------------------------------------------------------------
		Vec3 planepoint = tri.getTriPlane().closestPointOnPlane(sphere_center);

		//-----------------------------------------------------------------
		//restrict point to inside tri
		//-----------------------------------------------------------------
		if(!tri.pointInTri(planepoint))
		{
			planepoint = tri.closestPointOnTriangle(planepoint);
		}

		if(planepoint.getDist(sphere_center) <= sphere_os.getRadius())
			points_ws.push_back(frame.transformPointToParent(planepoint));
	
	}//end for each triangle
}




/*const js::BoundingSphere& js::SimpleTriMeshShape::getBoundingSphereOS()
{
	return boundingsphere;
}*/

const AABox js::SimpleTriMeshShape::getBBoxOS()
{
	return bbox_os;
}






/*
//returns true if sphere hit object.
bool js::SimpleTriMeshShape::traceSphere(const BoundingSphere& sphere, const Vec3& translation_os,	
										float& dist_out, Vec3& normal_os_out)//, bool& embedded_out,
										//Vec3& disembed_vec_out)
{
//	embedded_out = false;
//	disembed_vec_out = Vec3(0,0,0);

	Vec3 unitdir = translation_os;
	const float translation_len = unitdir.normalise_ret_length();

	const Vec3 sourcePoint = sphere.getCenter();

	float smallest_dist = 1e9;//smallest dist until hit a tri
	bool sphere_hit_a_tri = false;

	for(TriMesh::const_tri_iterator i = tris.trisBegin(); i != tris.trisEnd(); ++i)
	{
		const Triangle& tri = (*i);


		//-----------------------------------------------------------------
		//Determine the distance from the plane to the sphere center
		//-----------------------------------------------------------------
		//float pDist;

		//{
			//const Plane temptriplane(tri.v0(), tri.getNormal());

			//pDist = temptriplane.signedDistToPoint(sourcePoint);

		float pDist = tri.getTriPlane().signedDistToPoint(sourcePoint);
		//}


		//NOTE: invert normal if doing backface collision, so 'usenormal' is always facing
		//towards sphere center.

		Vec3 usenormal = tri.getNormal();

		if(pDist < 0)//-sphere.getRadius())
		{
			usenormal *= -1;
			pDist *= -1;
		}


		assert(pDist >= 0);

		//-----------------------------------------------------------------
		//check if sphere is heading away from tri
		//-----------------------------------------------------------------
		//const float usenormal_dot_unitdir = usenormal.dot(unitdir);
		//if(usenormal_dot_unitdir >= 0)
		//	continue;

		const float approach_rate = -usenormal.dot(unitdir);
		if(approach_rate <= 0)
			continue;
	
		//const Plane triplane(tri.v0(), tri.getNormal());


		//NOTE: could continue out early if pDist > distanceToTravel + radius ?
		//NEWCODEif(pDist > translation_len + sphere.getRadius())
		//	continue;

		//const float sphere_normtravel_dist = -usenormal_dot_unitdir * translation_len - sphere.getRadius();
		//dist along unitdir until it touches the tri plane
		
		//const float approach_rate = -usenormal_dot_unitdir;

		assert(approach_rate > 0);

		//trans_len_needed = dist to approach / dist approached per unit translation len
		const float trans_len_needed = (pDist - sphere.getRadius()) / approach_rate;

		//if(approach_rate * translation_len < pDist - sphere.getRadius())
		//	continue;
		if(translation_len < trans_len_needed)
			continue;

		//if(-usenormal_dot_unitdir * translation_len //if dist that will be covered by sphere towards plane (in direction of plane normal)
		//	< pDist + sphere.getRadius())//is less than dist to plane plus sphere radius,

		//if(sphere_normtravel_dist < pDist)
		//	continue;//then sphere will never get to plane

		//-----------------------------------------------------------------
		//calc the point where the sphere intersects with the triangle plane (planeIntersectionPoint)
		//-----------------------------------------------------------------
		Vec3 planeIntersectionPoint;




		// Is the plane embedded in the sphere?
		if(trans_len_needed <= 0)//pDist <= sphere.getRadius())//make == trans_len_needed < 0
      {
			// Calculate the plane intersection point
			planeIntersectionPoint = tri.getTriPlane().closestPointOnPlane(sourcePoint);

		}
      else
      {
			//Calculate the point on sphere that will touch plane first
         *//*const Vec3 bodyIntersectionPoint = sourcePoint - (usenormal * sphere.getRadius());

			//-----------------------------------------------------------------
			//Calculate the plane intersection point, ie where the sphere will hit
			//the plane first as it moves
			//-----------------------------------------------------------------
			
			//	inline Plane(const Vec3& origin, const Vec3& normal);
			//Plane triplane(tri.v0(), usenormal);

			//-----------------------------------------------------------------
			//trace towards the plane along the vel vector from the body intersection point
			//-----------------------------------------------------------------
			const float dist = tri.getTriPlane().rayIntersect(bodyIntersectionPoint, unitdir);		
			
			//double t = plane_ray_intersect(pOrigin, pNormal, bodyIntersectionPoint, vel_normised);


			//-----------------------------------------------------------------
			//ignore this poly if we are heading away from it, as can't collide with it
			//-----------------------------------------------------------------
			if(dist < 0.0)
				continue;

			//-----------------------------------------------------------------
			//Calculate the plane intersection point
			//-----------------------------------------------------------------
			//Vec3 V = vel_normised * t;//velocityVector with length set to t;
                    
			planeIntersectionPoint = bodyIntersectionPoint + unitdir * dist;*//*

			//const float sphere_dist = sphere_normtravel_dist / -usenormal_dot_unitdir;

			assert(trans_len_needed >= 0);

			planeIntersectionPoint = sourcePoint + (unitdir * trans_len_needed) -
																(sphere.getRadius() * usenormal);

			//assert point is actually on plane
			assert(epsEqual(tri.getTriPlane().signedDistToPoint(planeIntersectionPoint), 0));
		}



		//-----------------------------------------------------------------
		//now restrict collision point on tri plane to inside tri if neccessary.
		//-----------------------------------------------------------------
		Vec3 triIntersectionPoint = planeIntersectionPoint;
		
		const bool point_in_tri = tri.pointInTri(triIntersectionPoint);
		if(!point_in_tri)
		{
			//-----------------------------------------------------------------
			//restrict to inside tri
			//-----------------------------------------------------------------
			triIntersectionPoint = tri.closestPointOnTriangle(triIntersectionPoint);
		}


		if(triIntersectionPoint.getDist2(sphere.getCenter()) < sphere.getRadius2())
		{
#ifndef NOT_CYBERSPACE
			::debugPrint("jscol: WARNING: tri embedded in sphere");
#endif
			//embedded_out = true;
			//disembed_vec_out += normalise(sphere.getCenter() - triIntersectionPoint) * 
			//						(sphere.getRadius() - triIntersectionPoint.getDist(sphere.getCenter()));

			//-----------------------------------------------------------------
			//problem, so just ignore this tri :)
			//-----------------------------------------------------------------
			//continue;

			smallest_dist = 0;
			sphere_hit_a_tri = true;
			normal_os_out = usenormal;
			break;//don't test against other tris?
		}

		
		//-----------------------------------------------------------------
		//Using the triIntersectionPoint, we need to reverse-intersect
		//with the sphere
		//-----------------------------------------------------------------
				  
		//double intersectSphere(const Vec3& rO, const Vec3& rV, 
		 //						const Vec3& sphereO, double sphereR)

		//TEMP DEBUG:
		//const float use_radius = radius;// + 0.1;//TEMP just radius

			//returns dist till hit sphere or -1 if missed
		//inline float rayIntersect(const Vec3& raystart_os, const Vec3& rayunitdir) const;

		const float dist = sphere.rayIntersect(triIntersectionPoint, unitdir * -1.0f);
		
		//double t = intersectSphere(polygonIntersectionPoint, neg_vel_normised,
		//					sourcePoint, use_radius); 

		if(dist >= 0 && dist < smallest_dist && dist < translation_len)
		{
			smallest_dist = dist;
			sphere_hit_a_tri = true;

			//-----------------------------------------------------------------
			//just make the normal the tri normal
			//-----------------------------------------------------------------

			if(point_in_tri)
				normal_os_out = usenormal;
			else
			{
				//-----------------------------------------------------------------
				//calc point sphere will be when it hits edge of tri
				//-----------------------------------------------------------------
				const Vec3 hit_spherecenter = sourcePoint + unitdir * dist;

				assert(epsEqual(hit_spherecenter.getDist(triIntersectionPoint), sphere.getRadius()));

				normal_os_out = (hit_spherecenter - triIntersectionPoint) / sphere.getRadius(); 
			}
		}

	}//end for each triangle

	dist_out = smallest_dist;

	return sphere_hit_a_tri;

}
*/
