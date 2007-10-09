/*=====================================================================
Code By Nicholas Chapman.

  nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may *not* use this code for any commercial project.
=====================================================================*/
#ifndef __RAY_H_666_
#define __RAY_H_666_

#include "../maths/PaddedVec3.h"
#include "../maths/mathstypes.h"
#include "../maths/SSE.h"

class Ray
{
public:
	/*Ray()
	{
		built_recip_unitdir = false;
	}
	*/

	inline Ray(const Vec3d& startpos_, const Vec3d& unitdir_)
	:	startpos(startpos_),
		unitdir(unitdir_)
	{
		assert(unitdir_.isUnitLength());
		assert(sizeof(PaddedVec3) == 16);
		assert((void*)&startpos.x == (void*)&startpos);
		//built_recip_unitdir = false;

		startpos_f = PaddedVec3((float)startpos_.x, (float)startpos_.y, (float)startpos_.z);
		unitdir_f = PaddedVec3((float)unitdir_.x, (float)unitdir_.y, (float)unitdir_.z);

		buildRecipRayDir();
	}

	inline ~Ray(){}


	inline const Vec3d& startPos() const { return startpos; }
	inline const Vec3d& unitDir() const { return unitdir; }

	inline const PaddedVec3& startPosF() const { return startpos_f; }
	inline const PaddedVec3& unitDirF() const { return unitdir_f; }

	inline void setStartPos(const Vec3d& p) { startpos = p; startpos_f = PaddedVec3((float)p.x, (float)p.y, (float)p.z); }
	inline void setUnitDir(const Vec3d& p) { unitdir = p; unitdir_f = PaddedVec3((float)p.x, (float)p.y, (float)p.z); buildRecipRayDir(); }

	inline const Vec3d& getRecipRayDir() const;
	inline const PaddedVec3& getRecipRayDirF() const;

	inline bool builtRecipRayDir() const { return true; }//built_recip_unitdir; }

	inline void translateStartPos(const Vec3d& delta) { setStartPos(startPos() + delta); }

	inline const Vec3d point(const double t) const { return startPos() + unitDir() * t; }
	
private:
	void buildRecipRayDir();

	SSE_ALIGN PaddedVec3 startpos_f;
	SSE_ALIGN PaddedVec3 unitdir_f;
	SSE_ALIGN PaddedVec3 recip_unitdir_f;

	SSE_ALIGN Vec3d startpos;
	SSE_ALIGN Vec3d unitdir;
	SSE_ALIGN Vec3d recip_unitdir;

	//bool built_recip_unitdir;
};

const Vec3d& Ray::getRecipRayDir() const
{
	//assert(built_recip_unitdir);
	return recip_unitdir;
}

const PaddedVec3& Ray::getRecipRayDirF() const
{ 
	//assert(built_recip_unitdir);
	assert(epsEqual(recip_unitdir_f.x, (float)recip_unitdir.x));
	return recip_unitdir_f;
}



#endif //__RAY_H_666_
