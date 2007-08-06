#include "jscol_worldcollisionsys.h"

#include "jscol_modeltraceresult.h"
#include "jscol_boundingsphere.h"
#include "jscol_model.h"
#include "jscol_raytraceresult.h"
#include "../utils/lock.h"



js::WorldCollisionSys::WorldCollisionSys()
:	voxelhash(1.0f, 10000)
{}

js::WorldCollisionSys::~WorldCollisionSys()
{}

void js::WorldCollisionSys::insertStaticModel(::js::Model* model)
{

#ifdef JSCOL_LOCKS
	Lock mutexlock(mutex);
#endif

	//TEMP
	voxelhash.getBucketForVoxel(0, 0, 0).insert(model);
/*	
	TEMP TEMP 
  
	//-----------------------------------------------------------------
	//get the model's bounding sphere
	//-----------------------------------------------------------------
	const BoundingSphere& bounding_sphere_ws = model->getBoundingSphereWS();

	float radius = bounding_sphere_ws.getRadius();

	//-----------------------------------------------------------------
	//find all voxels partially or all inside the sphere
	//-----------------------------------------------------------------
	int half_voxelbox_length = (int)floor(radius / voxelhash.getVoxelLength()) + 1;
	int voxelbox_length = half_voxelbox_length * 2;

	//assert(half_voxelbox_length > 0);

	int startvoxelx = (bounding_sphere_ws.getCenter().x / voxelhash.getVoxelLength()) - half_voxelbox_length;
	int startvoxely = (bounding_sphere_ws.getCenter().y / voxelhash.getVoxelLength()) - half_voxelbox_length;
	int startvoxelz = (bounding_sphere_ws.getCenter().z / voxelhash.getVoxelLength()) - half_voxelbox_length;

	int endvoxelx = startvoxelx + voxelbox_length;
	int endvoxely = startvoxely + voxelbox_length;
	int endvoxelz = startvoxelz + voxelbox_length;

	for(int x = startvoxelx; x < endvoxelx; x++)
		for(int y = startvoxely; y < endvoxely; y++) 
			for(int z = startvoxelz; z < endvoxelz; z++)
			{
				voxelhash.getBucketForVoxel(x, y, z).insert(model);
			}
*/
}


void js::WorldCollisionSys::removeStaticModel(Model* model)
{
#ifdef JSCOL_LOCKS
	Lock mutexlock(mutex);
#endif

	//-----------------------------------------------------------------
	//remove model from the voxel hash
	//-----------------------------------------------------------------
	//TEMP
	voxelhash.getBucketForVoxel(0, 0, 0).remove(model);

}


int sign(float a)
{
	if(a >= 0)
		return 1;
	else
		return -1;
}

/*void js::WorldCollisionSys::traceModelPath(Model& dynamic_model, const CoordFrame& ini_transformation, 
										   const Vec3& final_translation, ModelTraceResult& results_out)
{

	Lock mutexlock(mutex);

	Vec3 modelpath = final_translation - ini_transformation.getOrigin();
	float length = modelpath.length();
	modelpath.normalise();
	//these should be combined


	int voxelx = (int)floor(ini_transformation.getOrigin().x / voxelhash.getVoxelLength()) + 1;
	int voxely = (int)floor(ini_transformation.getOrigin().y / voxelhash.getVoxelLength()) + 1;
	int voxelz = (int)floor(ini_transformation.getOrigin().z / voxelhash.getVoxelLength()) + 1;

	
	float absxgrad = fabs(modelpath.x);
	float absygrad = fabs(modelpath.y);
	float abszgrad = fabs(modelpath.z);


	int sign_xgrad = sign(modelpath.x);
	int sign_ygrad = sign(modelpath.y);
	int sign_zgrad = sign(modelpath.z);


	float x_disp = ini_transformation.getOrigin().x - (voxelx * voxelhash.getVoxelLength());
	float y_disp = ini_transformation.getOrigin().y - (voxely * voxelhash.getVoxelLength());
	float z_disp = ini_transformation.getOrigin().z - (voxelz * voxelhash.getVoxelLength());

	
	float t = 0;

	while(t <= length)
	{






		//-----------------------------------------------------------------
		//visit the voxel
		//-----------------------------------------------------------------


		//-----------------------------------------------------------------
		//get the bucket this voxel hashes to
		//-----------------------------------------------------------------
		VHashBucket& bucket = voxelhash.getBucketForVoxel(voxelx, voxely, voxelz);

		//-----------------------------------------------------------------
		//for each pointer in the bucket
		//-----------------------------------------------------------------
		
		float smallesttracefraction = 1.0;
		Model* closest_collider = NULL;
		MMColResults mm_colresults;


		for(VHashBucket::pointer_iterator i = bucket.begin(); i != bucket.end(); ++i)
		{
			//-----------------------------------------------------------------
			//check for a collision with the object
			//-----------------------------------------------------------------
	
			//bool col_ocurred = checkCollision(dynamic_model, *(*i), ini_transformation, final_translation, mm_colresults);
		
		//	virtual bool checkCollisionWith(Model& dyn_model, const CoordFrame& ini_transformation, 
		//							const Vec3& final_translation, MMColResults& results_out) = 0;

			//-----------------------------------------------------------------
			//check for collision only if bounding spheres overlap
			//-----------------------------------------------------------------
			if( (*i)->getBoundingSphereWS().overlapsWith(  dynamic_model.getBoundingSphereWS()  ) )
			{


				bool col_ocurred = (*i)->checkCollisionWith(dynamic_model, ini_transformation, 
															final_translation, mm_colresults);

				if(col_ocurred)
				{
					if(mm_colresults.fraction < smallesttracefraction)
					{
						smallesttracefraction = mm_colresults.fraction;
						closest_collider = (*i);

						//-----------------------------------------------------------------
						//save collision info
						//-----------------------------------------------------------------
						results_out.setColPoint(mm_colresults.colpoint);
						results_out.setStaticModNormal(mm_colresults.thisnormal);
						results_out.setDynModNormal(mm_colresults.othernormal);


					}
				}
			}
		}

		if(closest_collider)
		{
			//-----------------------------------------------------------------
			//build collision result info
			//-----------------------------------------------------------------
			results_out.setColOcurred(true);
			results_out.setStaticModel(closest_collider);	
			results_out.setDynModel(&dynamic_model);
			results_out.setT(smallesttracefraction);
			
			return;
		}














		//-----------------------------------------------------------------
		//advance the point by 1 unit
		//-----------------------------------------------------------------
		x_disp += absxgrad;

		y_disp += absygrad;

		z_disp += abszgrad;

		//-----------------------------------------------------------------
		//find if any of the displacements have moved to a neighbouring voxel
		//-----------------------------------------------------------------
		if(x_disp > y_disp)
		{
			// x > y

			if(x_disp > z_disp)
			{
				//x_disp is biggest
				if(x_disp >= voxelhash.getVoxelLength())
				{
					voxelx += sign_xgrad;//advance the x voxel coord
					x_disp -= voxelhash.getVoxelLength();
				}
			}
			else
			{
				//x > y && z > x, so z biggest
				if(z_disp >= voxelhash.getVoxelLength())
				{
					voxelz += sign_zgrad;//advance the z voxel coord
					z_disp -= voxelhash.getVoxelLength();

				}
			}
		}
		else
		{
			// y > x

			if(y_disp > z_disp)
			{
				//y > x && y > z
				if(y_disp >= voxelhash.getVoxelLength())
				{
					voxely += sign_ygrad;//advance the y voxel coord
					y_disp -= voxelhash.getVoxelLength();
				}
			}
			else
			{
				//y > x && y < z, so z biggest

				if(z_disp >= voxelhash.getVoxelLength())
				{
					voxelz += sign_zgrad;//advance the z voxel coord
					z_disp -= voxelhash.getVoxelLength();
				}
			}
		}

		t++;
	}
}

*/


float js::WorldCollisionSys::getRayDist(const ::Ray& ray)
{
#ifdef JSCOL_LOCKS
	Lock mutexlock(mutex);
#endif

	VHashBucket& bucket = voxelhash.getBucketForVoxel(0, 0, 0);

	float leastdist = 1e9f;
	bool hitsomething = false;

	for(VHashBucket::pointer_iterator i = bucket.begin(); i != bucket.end(); ++i)
	{
		const float dist = (*i)->getRayDist(ray);

		if(dist >= 0.0f)
		{
			if(dist < leastdist)
				leastdist = dist;

			hitsomething = true;
		}
	}

	if(!hitsomething)
		leastdist = -1.0f;

	return leastdist;
}



void js::WorldCollisionSys::traceRay(const ::Ray& ray, RayTraceResult& results_out)
{
#ifdef JSCOL_LOCKS
	Lock mutexlock(mutex);
#endif

	VHashBucket& bucket = voxelhash.getBucketForVoxel(0, 0, 0);

	float leastdist = 1e9f;
	bool hitsomething = false;

	for(VHashBucket::pointer_iterator i = bucket.begin(); i != bucket.end(); ++i)
	{
		unsigned int hit_tri_index;
		float u, v;
		Vec3 normal;
		const float dist = (*i)->traceRay(ray, hit_tri_index, u, v, normal);

		if(dist >= 0.0f)
		{
			if(dist < leastdist)
			{
				leastdist = dist;
				results_out.obstructor = *i;
				results_out.ob_normal = normal;
				results_out.hit_tri_index = hit_tri_index;
				results_out.u = u;
				results_out.v = v;
				hitsomething = true;
			}

		}
	}

	if(hitsomething)
	{
		results_out.rayhitsomething = true;
		results_out.dist = leastdist;
		//results_out.fraction = smallestt;
		//results_out.obstructor = closest_hit_model;
	}
	else
	{
		results_out.rayhitsomething = false;
		results_out.dist = -1.0f;	
	//	results_out.fraction = 1.0f;//these 2 not strictly needed
		results_out.obstructor = NULL;
	}

	return;
}


/*void js::WorldCollisionSys::traceRay(const Vec3& inipos, const Vec3& endpos, RayTraceResult& results_out)
{
#ifdef JSCOL_LOCKS
	Lock mutexlock(mutex);
#endif

	float smallestt = 1.0f;
	Model* closest_hit_model = NULL;
	float t;
	Vec3 normal;

	Vec3 rayunitdir = endpos - inipos;
	const float raylength = rayunitdir.normalise_ret_length();

	VHashBucket& bucket = voxelhash.getBucketForVoxel(0, 0, 0);


	//for(VoxelHash::bucket_iter b = voxelhash.bucketBegin(); b != voxelhash.bucketEnd(); ++b)
	//{
		//	virtual bool lineStabsModel(float& t_out, Vec3& normal_out, const Vec3& raystart_os, const Vec3& rayend_os) = 0;
		for(VHashBucket::pointer_iterator i = bucket.begin(); i != bucket.end(); ++i)
		{
		
			const bool hit = (*i)->lineStabsModel(t, normal, inipos, endpos, rayunitdir, raylength);

			if(hit)
			{
				if(t < smallestt)
				{
					smallestt = t;
					closest_hit_model = (*i);
					results_out.ob_normal = normal;
				}
			}
		}
	//}

	if(closest_hit_model)
	{
		results_out.fraction = smallestt;
		results_out.obstructor = closest_hit_model;
		results_out.rayhitsomething = true;
	}
	else
	{
		results_out.rayhitsomething = false;
		
		results_out.fraction = 1.0f;//these 2 not strictly needed
		results_out.obstructor = NULL;
	}

	return;
}*/



void js::WorldCollisionSys::traceSphere(const BoundingSphere& sphere, const Vec3& translation, 
								 RayTraceResult& results_out)//, bool& embedded_out, 
																		// Vec3& disembed_vec_out)
{
#ifdef JSCOL_LOCKS
	Lock mutexlock(mutex);
#endif

	if(translation.length() == 0)
	{
		results_out.rayhitsomething = false;
		return;
	}


	//embedded_out = false;
	//disembed_vec_out = Vec3(0,0,0);

	float smallestdist = 1e9f;
	Model* closest_hit_model = NULL;


	VHashBucket& bucket = voxelhash.getBucketForVoxel(0, 0, 0);
	for(VHashBucket::pointer_iterator i = bucket.begin(); i != bucket.end(); ++i)
	{
		
		//returns true if sphere hit object.
		//virtual bool traceSphere(const BoundingSphere& sphere, const Vec3& translation,	
		//									float& dist_out) = 0;

		float hitdist;
		Vec3 normal;
		//bool embedded;
		//Vec3 disembed_vec;
		const bool hit = (*i)->traceSphere(sphere, translation, hitdist, normal);//, embedded,
																//disembed_vec_out);

		/*if(embedded)
		{
			embedded_out = true;
			//disembed_vec_out += disembed_vec;
		}*/


		if(hit)
		{
			assert(hitdist >= 0);

			if(hitdist < smallestdist)
			{
				smallestdist = hitdist;
				closest_hit_model = (*i);
				results_out.ob_normal = normal;
			}
		}
	}


	if(closest_hit_model && smallestdist <= translation.length())
	{
		assert(smallestdist >= 0);

		results_out.rayhitsomething = true;
		//results_out.fraction = smallestdist / translation.length();
		results_out.dist = smallestdist;
		results_out.obstructor = closest_hit_model;
	}
	else
	{
		results_out.rayhitsomething = false;
		
		//results_out.fraction = 1.0f;//these 2 not strictly needed
		results_out.dist = -1.0f;//these 2 not strictly needed
		results_out.obstructor = NULL;
	}
}


void js::WorldCollisionSys::getCollPoints(std::vector<Vec3>& points_out, const BoundingSphere& sphere)
{
#ifdef JSCOL_LOCKS
	Lock mutexlock(mutex);
#endif
	

	VHashBucket& bucket = voxelhash.getBucketForVoxel(0, 0, 0);
	for(VHashBucket::pointer_iterator i = bucket.begin(); i != bucket.end(); ++i)
	{		
		(*i)->appendCollPoints(points_out, sphere);		
	}
}
