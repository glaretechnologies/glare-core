/*=====================================================================
aabox.cpp
---------
File created by ClassTemplate on Sun Aug 25 03:34:18 2002
Code By Nicholas Chapman.
=====================================================================*/
#include "aabox.h"




AABox::AABox()
{
	/*minx = 100000000;
	maxx = -100000000;
	miny = 100000000;
	maxy = -100000000;
	minz = 100000000;
	maxz = -100000000;*/

	/*inited = false;

	corners.resize(8);
	//setCorners();
	cornersbuilt = false;*/

	reset();
}


AABox::~AABox()
{
	
}


void AABox::enlargeToHold(const AABox& other)
{
	if(!other.inited)
		return;

	enlargeToHold(other.getCorners());
}


void AABox::enlargeToHold(const Vec3d& point)
{
	if(!inited)
	{
		minx = maxx = point.x;
		miny = maxy = point.y;
		minz = maxz = point.z;
		inited = true;
	}
	else
	{
		if(point.x < minx)
			minx = point.x;
		if(point.x > maxx)
			maxx = point.x;

		if(point.y < miny)
			miny = point.y;
		if(point.y > maxy)
			maxy = point.y;

		if(point.z < minz)
			minz = point.z;
		if(point.z > maxz)
			maxz = point.z;
	}
	
	//-----------------------------------------------------------------
	//rebuild corners
	//-----------------------------------------------------------------
	//if(corners.size() != 8)
	//	corners.resize(8);

	//setCorners();//NOTE: only do this on change?

	cornersbuilt = false;

}

void AABox::enlargeToHold(const std::vector<Vec3d>& points)
{
	if(!inited && !points.empty())
	{
		minx = maxx = points[0].x;
		miny = maxy = points[0].y;
		minz = maxz = points[0].z;
		inited = true;
	}


	for(unsigned int i=0; i<points.size(); ++i)
	{
		if(points[i].x < minx)
			minx = points[i].x;
		if(points[i].x > maxx)
			maxx = points[i].x;

		if(points[i].y < miny)
			miny = points[i].y;
		if(points[i].y > maxy)
			maxy = points[i].y;

		if(points[i].z < minz)
			minz = points[i].z;
		if(points[i].z > maxz)
			maxz = points[i].z;
	}

	cornersbuilt = false;
	//setCorners();
}
const std::vector<Vec3d>& AABox::getCorners() const
{
	if(!cornersbuilt)
		calcCorners();

	assert(cornersbuilt);

	return corners;
}


void AABox::calcCorners() const
{
	corners[0].set(minx, miny, minz);
	corners[1].set(minx, miny, maxz);
	corners[2].set(minx, maxy, minz);
	corners[3].set(minx, maxy, maxz);
	corners[4].set(maxx, miny, minz);
	corners[5].set(maxx, miny, maxz);
	corners[6].set(maxx, maxy, minz);
	corners[7].set(maxx, maxy, maxz);

	cornersbuilt = true;
}


void AABox::transformToParentFrame(const CoordFramed& frame)
{

	if(inited)
	{	
		AABox newbox;

		for(unsigned int i=0; i<getCorners().size(); ++i)
		{
			const Vec3d corner_parentspace = frame.transformPointToParent(getCorners()[i]);
			newbox.enlargeToHold(corner_parentspace);
		}

		*this = newbox;
	}
}

void AABox::scale(float scalefactor)
{
	minx *= scalefactor;
	maxx *= scalefactor;
	miny *= scalefactor;
	maxy *= scalefactor;
	minz *= scalefactor;
	maxz *= scalefactor;

	cornersbuilt = false;
}


/*bool AABox::onFrontSideOfPlane(const Plane<double>& plane) const
{						//returns true if all corners on front side of plane
	if(!inited)
		return false;


	for(unsigned int i=0; i<corners.size(); ++i)
		if(plane.pointOnBackSide(getCorners()[i]))//NOTE: slightly inefficient mutiple lazy checks
			return false;

	return true;

}*/

void AABox::setToLargeBox()
{
	inited = true;
	minx = -1e9;
	maxx = 1e9;
	miny = -1e9;
	maxy = 1e9;
	minz = -1e9;
	maxz = 1e9;

	cornersbuilt = false;
	//setCorners();
}

//returns 0 if point in box
double AABox::getDistToPoint(const Vec3d& point) const
{
	assert(inited);

	if(pointInBox(point))
		return 0;


	Vec3d closest;//closest point on box surface

	if(point.x < minx)
		closest.x = minx;
	else if(point.x > maxx)
		closest.x = maxx;
	else
		closest.x = point.x;

	if(point.y < miny)
		closest.y = miny;
	else if(point.y > maxy)
		closest.y = maxy;
	else
		closest.y = point.y;

	if(point.z < minz)
		closest.z = minz;
	else if(point.z > maxz)
		closest.z = maxz;
	else
		closest.z = point.z;

	assert(pointInBox(closest));

	return closest.getDist(point);
}
	

bool AABox::pointInBox(const Vec3d& point) const
{
	assert(inited);

	return	point.x >= minx && point.x <= maxx && 
			point.y >= miny && point.y <= maxy && 
			point.z >= minz && point.z <= maxz;
}


bool AABox::hitByRay(const Vec3d& raystart, const Vec3d& rayend, const Vec3d& rayunitdir) const
{
	float largestneardist;
	float smallestfardist;

	//-----------------------------------------------------------------
	//test x
	//-----------------------------------------------------------------
	if(rayunitdir.x > 0)
	{
		largestneardist = (minx - raystart.x) / rayunitdir.x;
		smallestfardist = (maxx - raystart.x) / rayunitdir.x;
	}
	else
	{
		smallestfardist = (minx - raystart.x) / rayunitdir.x;
		largestneardist = (maxx - raystart.x) / rayunitdir.x;
	}
		
	//-----------------------------------------------------------------
	//test y
	//-----------------------------------------------------------------
	float ynear;
	float yfar;

	if(rayunitdir.y > 0)
	{
		ynear = (miny - raystart.y) / rayunitdir.y;
		yfar = (maxy - raystart.y) / rayunitdir.y;
	}
	else
	{
		yfar = (miny - raystart.y) / rayunitdir.y;
		ynear = (maxy - raystart.y) / rayunitdir.y;
	}

	if(ynear > largestneardist)
		largestneardist = ynear;

	if(yfar < smallestfardist)
		smallestfardist = yfar;

	if(largestneardist > smallestfardist)
		return false;

	//-----------------------------------------------------------------
	//test z
	//-----------------------------------------------------------------
	float znear;
	float zfar;

	if(rayunitdir.z > 0)
	{
		znear = (minz - raystart.z) / rayunitdir.z;
		zfar = (maxz - raystart.z) / rayunitdir.z;
	}
	else
	{
		zfar = (minz - raystart.z) / rayunitdir.z;
		znear = (maxz - raystart.z) / rayunitdir.z;
	}

	if(znear > largestneardist)
		largestneardist = znear;

	if(zfar < smallestfardist)
		smallestfardist = zfar;

	if(largestneardist > smallestfardist)
		return false;
	else
	{
		return largestneardist <= raystart.getDist(rayend);//Vec3(rayend - raystart).length());
	}
}


//returns 0 if not inited
float AABox::getVolume() const
{
	if(!inited)
		return 0;

	return (maxx - minx) * (maxy - miny) * (maxz - minz);
}

bool AABox::doesOverlap(const AABox& other) const
{
	//NOTE: check this logic
	if(other.minx > maxx)
		return false;

	if(other.maxx < minx)
		return false;


	if(other.miny > maxy)
		return false;

	if(other.maxy < miny)
		return false;


	if(other.minz > maxz)
		return false;

	if(other.maxz < minz)
		return false;

	return true;
}

void AABox::reset()
{
	inited = false;

	corners.resize(8);
	//setCorners();
	cornersbuilt = false;
}
