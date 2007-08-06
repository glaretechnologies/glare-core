/*=====================================================================
spherehammersly.h
-----------------
File created by ClassTemplate on Tue Dec 14 18:10:55 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __SPHEREHAMMERSLY_H_666_
#define __SPHEREHAMMERSLY_H_666_


#include <vector>


/*=====================================================================
SphereHammersly
---------------

=====================================================================*/
class SphereHammersly
{
public:
	/*=====================================================================
	SphereHammersly
	---------------
	
	=====================================================================*/
	SphereHammersly(int maxnum);

	~SphereHammersly();

	void getNext(Vec3& point_out);

	void getNext(Vec2& point_out);


	static void genNPoints(int n, std::vector<Vec3>& points_out);
private:
	int maxnum;
	int k;
	float recip_maxnum;
};



#endif //__SPHEREHAMMERSLY_H_666_




