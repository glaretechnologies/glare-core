/*#include "js_ball.h"


#include <math.h>





bool js::Ball::lineStabsObject(float& t_out, Vec3& normal_out, const Vec3& raystart, const Vec3& rayend)
{


	//-----------------------------------------------------------------
	//calculate the normalised direction of the ray.//NOTE: this could be provided as an arg
	//-----------------------------------------------------------------
	Vec3 raydir = rayend - raystart;
	float raylength = raydir.length();
	raydir.normalise();

	//let ray be also thought of as 'point = raystart + q*raydir'

	Vec3 raystarttosphere = getPos() - raystart;

	//-----------------------------------------------------------------
	//calc the value of q at the closest the ray comes to the centre of the sphere
	//-----------------------------------------------------------------
	float q_closest = dotProduct(raystarttosphere, raydir);

	if(q_closest < 0 || q_closest > raylength)
		return false;//check this, VERY dodgy



	float raytosphere_dist2 = dotProduct(raystarttosphere, raystarttosphere);

	float radius2 = radius * radius;//precompute this


	Vec3 rayclosest = raystart + (raydir * q_closest);

	Vec3 centre_to_closest = rayclosest - getPos();

	//if(centre_to_closest.length() > radius)
	//	return false;
	float centre_to_closest_len = centre_to_closest.length();

	float intsectpos_to_closest_len2 = radius2 - centre_to_closest_len*centre_to_closest_len;

	if(intsectpos_to_closest_len2 < 0)
	{
		//then centre_to_closest > radius
		return false;
	}

	//-----------------------------------------------------------------
	//intersect q = q_closest - 
	//-----------------------------------------------------------------
	//float intersect_q = q_closest - sqr(intsectpos_to_closest_len2);

	float intersect_to_closest_len = sqrt(intsectpos_to_closest_len2);

	Vec3 intersection = rayclosest - (raydir * intersect_to_closest_len);

	t_out = intersect_to_closest_len / raylength;


	return true;

}
*/