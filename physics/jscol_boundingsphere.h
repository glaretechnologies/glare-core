#pragma once


#include "../maths/maths.h"


namespace js
{



class BoundingSphere
{
public:
	BoundingSphere()
	{
		//center.zero();
		//setRadius(10000);
	}

	typedef Vec4f Vec3;


	BoundingSphere(const Vec3 center_, float radius_)
	{
		center = center_;
		setRadius(radius_);
	}

	const Vec3& getCenter() const { return center; }
	float getRadius() const { return radius; }
	float getRadius2() const { return radius2; }

	void setRadius(float r)
	{
		radius = r;
		radius2 = r * r;
	}

	void setCenter(const Vec3& p){ center = p; }

	bool overlapsWith(const BoundingSphere& other) const
	{
		Vec3 this_to_other = other.getCenter() - center;

		float dist2 = dot(this_to_other, this_to_other);

		if(dist2 <= radius2 + other.getRadius2())
			return true;

		return false;
	}

	//returns dist till hit sphere or -1 if missed
	inline float rayIntersect(const Vec3& raystart_os, const Vec3& rayunitdir) const;

	inline bool lineStabsShape(const Vec3& raystart_os, const Vec3& rayunitdir, 
															float raylength) const;


private:
	Vec3 center;
	float radius;
	float radius2;
};


float BoundingSphere::rayIntersect(const Vec3& raystart_os, const Vec3& rayunitdir) const
{
	Vec3 raystarttosphere = center;
	raystarttosphere -= raystart_os;

	if(raystarttosphere.length2() <= radius2)
		return 0;//ray origin inside sphere

	const float dist_to_rayclosest = dot(raystarttosphere, rayunitdir);


	//-----------------------------------------------------------------
	//check if center of sphere lies behind ray startpos (in dir of ray)
	//-----------------------------------------------------------------
	if(dist_to_rayclosest < 0)//will think of rays as having infinite length || q_closest > ray.length)
		return -1;

	//if(dist_to_rayclosest < radius)//ray origin inside sphere
	//	return true;
	//else if(dist_to_rayclosest < 0)
	//	return false;//sphere behind ray origin


	const float sph_cen_to_ray_closest_len2 = raystarttosphere.length2() - 
		dist_to_rayclosest*dist_to_rayclosest;

	//-----------------------------------------------------------------
	//ray has missed sphere?
	//-----------------------------------------------------------------
	if(sph_cen_to_ray_closest_len2 > radius2)
		return -1;

	const float hitdist = dist_to_rayclosest - sqrt(radius2 - sph_cen_to_ray_closest_len2);

	return hitdist;
}




bool BoundingSphere::lineStabsShape(const Vec3& raystart_os, const Vec3& rayunitdir, 
															float raylength) const		
{
	const float hitdist = rayIntersect(raystart_os, rayunitdir);
	return hitdist >=0 && hitdist <= raylength;
}


}//end namespace js
