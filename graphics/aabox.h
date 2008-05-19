/*=====================================================================
aabox.h
-------
File created by ClassTemplate on Sun Aug 25 03:34:18 2002
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __AABOX_H_666_
#define __AABOX_H_666_



#include <vector>
#include "../maths/vec3.h"
#include "../maths/coordframe.h"
#include "../maths/plane.h"

/*=====================================================================
AABox
-----
axis-aligned box.
=====================================================================*/
class AABox
{
public:
	/*=====================================================================
	AABox
	-----
	
	=====================================================================*/
	AABox();

	~AABox();

	//std::vector<Vec3>& getCorners();
	const std::vector<Vec3d>& getCorners() const;
	//void getCorners(std::vector<Vec3>& corners_out) const;

	void enlargeToHold(const AABox& other);
	void enlargeToHold(const Vec3d& point);
	void enlargeToHold(const std::vector<Vec3d>& points);

	//NOTE: spacially inefficient to use this
	void transformToParentFrame(const CoordFramed& frame);

	void scale(float scalefactor);

	bool onFrontSideOfPlane(const Plane<double>& plane) const;
		//returns true if all corners on front side of plane

	void setToLargeBox();

	//returns 0 if point in box
	double getDistToPoint(const Vec3d& point) const;

	bool pointInBox(const Vec3d& point) const;

	//
	bool hitByRay(const Vec3d& raystart, const Vec3d& rayend, const Vec3d& rayunitdir) const;

	//returns 0 if not inited
	float getVolume() const;

	bool doesOverlap(const AABox& other) const;

	bool isInited() const { return inited; }

	void reset();

	double minx;
	double maxx;
	double miny;
	double maxy;
	double minz;
	double maxz;

private:
	void calcCorners() const;
	mutable std::vector<Vec3d> corners;


	bool inited;

	mutable bool cornersbuilt;
};



#endif //__AABOX_H_666_




