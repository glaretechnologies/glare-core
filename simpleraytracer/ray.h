/*=====================================================================
Code By Nicholas Chapman.

  nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may *not* use this code for any commercial project.
=====================================================================*/
#ifndef __RAY_H_666_
#define __RAY_H_666_


#include "../maths/vec3.h"
#include "../maths/Vec4f.h"
#include "../maths/mathstypes.h"
#include "../maths/SSE.h"


SSE_ALIGN
class Ray
{
public:
	/*inline Ray(const Vec3d& startpos_, const Vec3d& unitdir_)
	:	startpos(startpos_),
		unitdir(unitdir_)
	{
		assert(unitdir_.isUnitLength());
		assert((void*)&startpos.x == (void*)&startpos);

		startpos_f = Vec4f((float)startpos_.x, (float)startpos_.y, (float)startpos_.z, 1.f);
		unitdir_f = Vec4f((float)unitdir_.x, (float)unitdir_.y, (float)unitdir_.z, 0.f);

		buildRecipRayDir();
	}*/
	inline Ray(const Vec4f& startpos_, const Vec4f& unitdir_)
	{
		assert(unitdir_.isUnitLength());
		assert(SSE::isSSEAligned(this));

		startpos_f = startpos_;
		unitdir_f = unitdir_;

		buildRecipRayDir();
	}

	inline ~Ray(){}

	//INDIGO_STRONG_INLINE const Vec3d& startPos() const { return startpos; }
	//INDIGO_STRONG_INLINE const Vec3d& unitDir() const { return unitdir; }

	INDIGO_STRONG_INLINE const Vec4f& startPos() const { return startpos_f; }
	INDIGO_STRONG_INLINE const Vec4f& unitDir() const { return unitdir_f; }

	INDIGO_STRONG_INLINE const Vec4f& startPosF() const { return startpos_f; }
	INDIGO_STRONG_INLINE const Vec4f& unitDirF() const { return unitdir_f; }

	//inline void setStartPos(const Vec3d& p) { startpos = p; startpos_f = Vec4f((float)p.x, (float)p.y, (float)p.z, 1.0f); }
	//inline void setUnitDir(const Vec3d& p) { unitdir = p; unitdir_f = Vec4f((float)p.x, (float)p.y, (float)p.z, 0.0f); buildRecipRayDir(); }

	//inline const Vec3d& getRecipRayDir() const;
	INDIGO_STRONG_INLINE const Vec4f& getRecipRayDirF() const { return recip_unitdir_f; }

	//inline void translateStartPos(const Vec3d& delta) { setStartPos(startPos() + delta); }

	//inline const Vec3d point(const double t) const { return startPos() + unitDir() * t; }
	INDIGO_STRONG_INLINE const Vec4f pointf(const float t) const { return Vec4f(startpos_f + Vec4f(unitdir_f * t)); }
	
private:
	void buildRecipRayDir();

	SSE_ALIGN Vec4f startpos_f;
	SSE_ALIGN Vec4f unitdir_f;
	SSE_ALIGN Vec4f recip_unitdir_f;

	//SSE_ALIGN Vec3d startpos;
	//SSE_ALIGN Vec3d unitdir;
	//SSE_ALIGN Vec3d recip_unitdir;
};


/*const Vec3d& Ray::getRecipRayDir() const
{
	//assert(built_recip_unitdir);
	return recip_unitdir;
}*/


#endif //__RAY_H_666_
