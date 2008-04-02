/*=====================================================================
aabbox.cpp
----------
File created by ClassTemplate on Thu Nov 18 03:48:29 2004Code By Nicholas Chapman.
=====================================================================*/
#include "jscol_aabbox.h"

#include "../maths/mathstypes.h"
#include "../indigo/TestUtils.h"


namespace js
{

/*
bool AABBox::slowRayAABBTrace(const Ray& ray, float& near_hitd_out, float& far_hitd_out) const
{
	float largestneardist;
	float smallestfardist;

	//-----------------------------------------------------------------
	//test x
	//-----------------------------------------------------------------
	if(ray.unitDir().x > 0.0f)
	{
		//dist along ray = dist to travel / speed in that dir
		largestneardist = (min.x - ray.startPosF().x) / ray.unitDirF().x;
		smallestfardist = (max.x - ray.startPosF().x) / ray.unitDirF().x;
	}
	else
	{
		smallestfardist = (min.x - ray.startPosF().x) / ray.unitDirF().x;
		largestneardist = (max.x - ray.startPosF().x) / ray.unitDirF().x;
	}
		
	//-----------------------------------------------------------------
	//test y
	//-----------------------------------------------------------------
	float ynear;
	float yfar;

	if(ray.unitDir().y > 0.0f)
	{
		ynear = (min.y - ray.startPosF().y) / ray.unitDirF().y;
		yfar = (max.y - ray.startPosF().y) / ray.unitDirF().y;
	}
	else
	{
		yfar = (min.y - ray.startPosF().y) / ray.unitDirF().y;
		ynear = (max.y - ray.startPosF().y) / ray.unitDirF().y;
	}

	if(ynear > largestneardist)
		largestneardist = ynear;

	if(yfar < smallestfardist)
		smallestfardist = yfar;

	if(largestneardist > smallestfardist)
		return false;//-1.0f;//false;

	//-----------------------------------------------------------------
	//test z
	//-----------------------------------------------------------------
	float znear;
	float zfar;

	if(ray.unitDir().z > 0.0f)
	{
		znear = (min.z - ray.startPosF().z) / ray.unitDirF().z;
		zfar = (max.z - ray.startPosF().z) / ray.unitDirF().z;
	}
	else
	{
		zfar = (min.z - ray.startPosF().z) / ray.unitDirF().z;
		znear = (max.z - ray.startPosF().z) / ray.unitDirF().z;
	}

	if(znear > largestneardist)
		largestneardist = znear;

	if(zfar < smallestfardist)
		smallestfardist = zfar;

	if(largestneardist > smallestfardist)
		return false;//return -1.0f;//return false;
	else
	{
		//return myMax(0.0f, largestneardist);//largestneardist <= raystart.getDist(rayend);//Vec3(rayend - raystart).length());
		near_hitd_out = myMax(0.0f, largestneardist);

		//NOTE: this correct?
		far_hitd_out = myMax(near_hitd_out, smallestfardist);

		return true;
	}
}
*/



/*bool AABBox::ray_box_intersects_slabs_geimer_muller_sse_ss(
							const Vec3& raystartpos, const Vec3& recip_unitraydir, 
						  float& near_hitd_out, float& far_hitd_out) 
{
	__m128 lmin,lmax;

	{
		__m128 l1 = mulss(subss(loadss(&this->min.x), loadss(&raystartpos.x)), loadss(&recip_unitraydir.x));
		__m128 l2 = mulss(subss(loadss(&this->max.x), loadss(&raystartpos.x)), loadss(&recip_unitraydir.x));
		lmin = minss(l1,l2);
		lmax = maxss(l1,l2);
	}
	{
		__m128 l1 = mulss(subss(loadss(&this->min.y), loadss(&raystartpos.y)), loadss(&recip_unitraydir.y));
		__m128 l2 = mulss(subss(loadss(&this->max.y), loadss(&raystartpos.y)), loadss(&recip_unitraydir.y));
		lmin = maxss(lmin, minss(l1,l2));
		lmax = minss(lmax, maxss(l1,l2));
	}
 
	bool ret;
	{
		__m128 l1 = mulss(subss(loadss(&this->min.z), loadss(&raystartpos.z)), loadss(&recip_unitraydir.z));
		__m128 l2 = mulss(subss(loadss(&this->max.z), loadss(&raystartpos.z)), loadss(&recip_unitraydir.z));
		lmax = minss(lmax, maxss(l1,l2));
		ret = _mm_comigt_ss(lmax, _mm_setzero_ps());
		lmin = maxss(lmin, minss(l1,l2));
		ret &= _mm_comige_ss(lmax,lmin); 
	}
	return  ret;
}*/
/*
bool_t AABBox::ray_box_intersects_slabs_geimer_muller_sse_ss(const rt::ray_t &ray) 
{
		__m128 lmin,lmax;
		{
			__m128 l1 = mulss(subss(loadss(&box.minX), loadss(&ray.pos.x)), loadss(&ray.rcp_dir.x));
			__m128 l2 = mulss(subss(loadss(&box.maxX), loadss(&ray.pos.x)), loadss(&ray.rcp_dir.x));
			lmin = minss(l1,l2);
			lmax = maxss(l1,l2);
		}
		{
			__m128 l1 = mulss(subss(loadss(&box.minY), loadss(&ray.pos.y)), loadss(&ray.rcp_dir.y));
			__m128 l2 = mulss(subss(loadss(&box.maxY), loadss(&ray.pos.y)), loadss(&ray.rcp_dir.y));
			lmin = maxss(lmin, minss(l1,l2));
			lmax = minss(lmax, maxss(l1,l2));
		}
 
		bool_t ret;
		{
			__m128 l1 = mulss(subss(loadss(&box.minY), loadss(&ray.pos.z)), loadss(&ray.rcp_dir.z));
			__m128 l2 = mulss(subss(loadss(&box.maxY), loadss(&ray.pos.z)), loadss(&ray.rcp_dir.z));
			lmax = minss(lmax, maxss(l1,l2));
			ret = _mm_comigt_ss(lmax, _mm_setzero_ps());
			lmin = maxss(lmin, minss(l1,l2));
			ret &= _mm_comige_ss(lmax,lmin); 
		}
		return  ret;
	}
*/






bool AABBox::invariant() const
{
	return max_.x >= min_.x && max_.y >= min_.y && max_.z >= min_.z;
}

/*
void AABBox::writeModel(std::ostream& stream, int& num_models) const
{
	
	const AABBox& aabb = *this;

	const std::string matname = "mat_" + ::toString(num_models);
	const std::string meshname = "mesh_" + ::toString(num_models);

	stream << "<material>\n";
	stream << "	<name>" + matname + "</name>\n";
	stream << "	<diffuse>\n";
	stream << "		<colour>" << unitRandom() << " " << unitRandom() << " " << unitRandom() << "</colour>\n";
	stream << "	</diffuse>\n";
	stream << "</material>\n";

	stream << "\t<mesh>\n";
	stream << "\t\t<name>" << meshname << "</name>\n";
	stream << "\t\t<embedded>\n";

	stream << "\t\t<vertex pos=\"" << aabb.min.x << " " << aabb.min.y << " " << aabb.max.z << "\" normal=\"0 0 1\"/>\n";
	stream << "\t\t<vertex pos=\"" << aabb.min.x << " " << aabb.max.y << " " << aabb.max.z << "\" normal=\"0 0 1\"/>\n";
	stream << "\t\t<vertex pos=\"" << aabb.max.x << " " << aabb.max.y << " " << aabb.max.z << "\" normal=\"0 0 1\"/>\n";
	stream << "\t\t<vertex pos=\"" << aabb.max.x << " " << aabb.min.y << " " << aabb.max.z << "\" normal=\"0 0 1\"/>\n";

	stream << "\t\t<vertex pos=\"" << aabb.min.x << " " << aabb.min.y << " " << aabb.min.z << "\" normal=\"0 0 -1\"/>\n";
	stream << "\t\t<vertex pos=\"" << aabb.min.x << " " << aabb.max.y << " " << aabb.min.z << "\" normal=\"0 0 -1\"/>\n";
	stream << "\t\t<vertex pos=\"" << aabb.max.x << " " << aabb.max.y << " " << aabb.min.z << "\" normal=\"0 0 -1\"/>\n";
	stream << "\t\t<vertex pos=\"" << aabb.max.x << " " << aabb.min.y << " " << aabb.min.z << "\" normal=\"0 0 -1\"/>\n";

	stream << "\t\t<triangle_set>\n";
	stream << "\t\t\t<material_name>" + matname + "</material_name>\n";

	const int num_verts = 0;

	stream << "\t\t\t<tri>" << num_verts << " " << num_verts + 1 << " " << num_verts + 2 << "</tri>\n";
	stream << "\t\t\t<tri>" << num_verts << " " << num_verts + 2 << " " << num_verts + 3 << "</tri>\n";

	//bottom faces
	stream << "\t\t\t<tri>" << num_verts + 4 << " " << num_verts + 5 << " " << num_verts + 6 << "</tri>\n";
	stream << "\t\t\t<tri>" << num_verts + 4 << " " << num_verts + 6 << " " << num_verts + 7 << "</tri>\n";

	//front
	stream << "\t\t\t<tri>" << num_verts + 4 << " " << num_verts + 0 << " " << num_verts + 3 << "</tri>\n";
	stream << "\t\t\t<tri>" << num_verts + 4 << " " << num_verts + 3 << " " << num_verts + 7 << "</tri>\n";

	//back
	stream << "\t\t\t<tri>" << num_verts + 5 << " " << num_verts + 1 << " " << num_verts + 2 << "</tri>\n";
	stream << "\t\t\t<tri>" << num_verts + 5 << " " << num_verts + 2 << " " << num_verts + 6 << "</tri>\n";

	//left
	stream << "\t\t\t<tri>" << num_verts + 1 << " " << num_verts + 0 << " " << num_verts + 4 << "</tri>\n";
	stream << "\t\t\t<tri>" << num_verts + 1 << " " << num_verts + 4 << " " << num_verts + 5 << "</tri>\n";

	//right
	stream << "\t\t\t<tri>" << num_verts + 7 << " " << num_verts + 3 << " " << num_verts + 2 << "</tri>\n";
	stream << "\t\t\t<tri>" << num_verts + 7 << " " << num_verts + 2 << " " << num_verts + 6 << "</tri>\n";

	//top faces
	stream << "\t\t\t<tri>" << num_verts << " " << num_verts + 1 << " " << num_verts + 2 << "</tri>\n";
	stream << "\t\t\t<tri>" << num_verts << " " << num_verts + 2 << " " << num_verts + 3 << "</tri>\n";

	//bottom faces
	stream << "\t\t\t<tri>" << num_verts + 4 << " " << num_verts + 5 << " " << num_verts + 6 << "</tri>\n";
	stream << "\t\t\t<tri>" << num_verts + 4 << " " << num_verts + 6 << " " << num_verts + 7 << "</tri>\n";

	//front
	stream << "\t\t\t<tri>" << num_verts + 4 << " " << num_verts + 0 << " " << num_verts + 3 << "</tri>\n";
	stream << "\t\t\t<tri>" << num_verts + 4 << " " << num_verts + 3 << " " << num_verts + 7 << "</tri>\n";

	//back
	stream << "\t\t\t<tri>" << num_verts + 5 << " " << num_verts + 1 << " " << num_verts + 2 << "</tri>\n";
	stream << "\t\t\t<tri>" << num_verts + 5 << " " << num_verts + 2 << " " << num_verts + 6 << "</tri>\n";

	//left
	stream << "\t\t\t<tri>" << num_verts + 1 << " " << num_verts + 0 << " " << num_verts + 4 << "</tri>\n";
	stream << "\t\t\t<tri>" << num_verts + 1 << " " << num_verts + 4 << " " << num_verts + 5 << "</tri>\n";

	//right
	stream << "\t\t\t<tri>" << num_verts + 7 << " " << num_verts + 3 << " " << num_verts + 2 << "</tri>\n";
	stream << "\t\t\t<tri>" << num_verts + 7 << " " << num_verts + 2 << " " << num_verts + 6 << "</tri>\n";
	


	stream << "\t\t</triangle_set>\n";

	//num_verts += 8;

	
	stream << "\t\t</embedded>\n";
	stream << "\t</mesh>\n";

	stream << "\t<model>\n";
	stream << "\t\t<pos>0.1 0.2 0.1</pos>\n";
	stream << "\t\t<scale>1.0</scale>\n";
	stream << "\t\t<rotation>\n";
	stream << "\t\t\t<matrix>\n";
	stream << "\t\t\t\t1 0 0 0 1 0 0 0 1\n";
	stream << "\t\t\t</matrix>\n";
	stream << "\t\t</rotation>\n";
	stream << "\t\t<mesh_name>" << meshname << "</mesh_name>\n";
	stream << "\t\t<enable_normal_smoothing>false</enable_normal_smoothing>\n";
	stream << "\t</model>\n";

	num_models++;
}
*/


static inline float myMin(float a, float b, float c)
{
	return ::myMin(a, ::myMin(b, c));
}
static inline float myMax(float a, float b, float c)
{
	return ::myMax(a, ::myMax(b, c));
}

/*
int AABBox::triangleBoxOverlap(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2) const
{
	//if(myMin(v0.x, v1.x, v2.x) >= max.x) //is tri min x >= aabb max x?
//		return 0;
//	if(myMax(v0.x, v1.x, v2.x) < min.x) //is tri min x >= aabb max x?
//		return 0;

	//------------------------------------------------------------------------
	//try axis aligned separating planes
	//------------------------------------------------------------------------
	for(int i=0; i<3; ++i)//for each axis
	{
		if(myMin(v0[i], v1[i], v2[i]) >= max[i]) //is tri min >= aabb max?
			return 0;
		if(myMax(v0[i], v1[i], v2[i]) < min[i])
			return 0;
	}

	Plane p(toVec3d(v0), 


	return 1;
}
*/



void AABBox::test()
{
	{
	SSE_ALIGN AABBox box(Vec3f(0,0,0), Vec3f(1,2,3));

	testAssert(box == AABBox(Vec3f(0,0,0), Vec3f(1,2,3)));
	testAssert(::epsEqual(box.getSurfaceArea(), 22.f));
	testAssert(::epsEqual(box.axisLength(0), 1.f));
	testAssert(::epsEqual(box.axisLength(1), 2.f));
	testAssert(::epsEqual(box.axisLength(2), 3.f));
	testAssert(box.longestAxis() == 2);

	box.enlargeToHoldPoint(Vec3f(10, 20, 0));
	testAssert(box == AABBox(Vec3f(0,0,0), Vec3f(10,20,3)));

	box.enlargeToHoldPoint(Vec3f(-10, -20, 0));
	testAssert(box == AABBox(Vec3f(-10,-20,0), Vec3f(10,20,3)));


	}
	//TODO: test tracing rays

	{
	const SSE_ALIGN AABBox box(Vec3f(0,0,0), Vec3f(1,1,1));
	const Vec3f dir = normalise(Vec3f(0.0f, 0.0f, 1.0f));
	const SSE_ALIGN PaddedVec3 recip_dir(1.0f / dir.x, 1.0f / dir.y, 1.0f / dir.z);
	const SSE_ALIGN PaddedVec3 raystart(0,0,-1.0f);
	float near, far;
	const int hit = box.rayAABBTrace(raystart, recip_dir, near, far);
	testAssert(hit != 0);
	testAssert(::epsEqual(near, 1.0f));
	testAssert(::epsEqual(far, 2.0f));
	}
	{
	const SSE_ALIGN AABBox box(Vec3f(0,0,0), Vec3f(1,1,1));
	const Vec3f dir = normalise(Vec3f(0.0f, 0.0f, 1.0f));
	const SSE_ALIGN PaddedVec3 recip_dir(1.0f / dir.x, 1.0f / dir.y, 1.0f / dir.z);
	const SSE_ALIGN PaddedVec3 raystart(2.0,0,-1.0f);
	float near, far;
	const int hit = box.rayAABBTrace(raystart, recip_dir, near, far);
	testAssert(hit == 0);
	}

	{
	const SSE_ALIGN AABBox box(Vec3f(0,0,0), Vec3f(1,1,1));
	const Vec3f dir = normalise(Vec3f(0.0f, 0.0f, 1.0f));
	const SSE_ALIGN PaddedVec3 recip_dir(1.0f / dir.x, 1.0f / dir.y, 1.0f / dir.z);
	const SSE_ALIGN PaddedVec3 raystart(1.0, 0, -1.0f);
	float near, far;
	const int hit = box.rayAABBTrace(raystart, recip_dir, near, far);
	testAssert(hit != 0);
	testAssert(::epsEqual(near, 1.0f));
	testAssert(::epsEqual(far, 2.0f));
	}

	
}




















} //end namespace js






