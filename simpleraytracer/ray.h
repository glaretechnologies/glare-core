/*=====================================================================
Code By Nicholas Chapman.

  nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may *not* use this code for any commercial project.
=====================================================================*/
#ifndef __RAY_H_666_
#define __RAY_H_666_


//#include "../maths/vec3.h"
#include "../maths/Vec4f.h"
#include "../maths/mathstypes.h"
#include "../maths/SSE.h"


SSE_CLASS_ALIGN Ray
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
	INDIGO_STRONG_INLINE Ray() {}

	INDIGO_STRONG_INLINE Ray(const Vec4f& startpos_, const Vec4f& unitdir_, float min_t_ = 0.0f)
	:	startpos_f(startpos_),
		unitdir_f(unitdir_),
		min_t(min_t_)
	{
		assert(epsEqual(startpos_.x[3], 1.0f));
		assert(epsEqual(unitdir_.x[3], 0.0f));
		assert(unitdir_.isUnitLength());
		assert(SSE::isSSEAligned(this));

		//buildRecipRayDir();

		/*
			TEMP HACK:
			We want to avoid 1/+0 = +Inf or 1/-0 = -Inf
			so we set a maximum value that the reciprocal can take, and set
			use_recip = clamp(recip, MIN_RECIP, MAX_RECIP),
		*/
		const float MAX_RECIP = 1.0e26f;
		const SSE_ALIGN float MAX_RECIP_vec[4] = {MAX_RECIP, MAX_RECIP, MAX_RECIP, MAX_RECIP};

		this->recip_unitdir_f = Vec4f(
			_mm_min_ps(
				_mm_load_ps(MAX_RECIP_vec),
				_mm_div_ps(
					_mm_load_ps(one_4vec),
					unitdir_.v
					)
				)
			);

#ifdef DEBUG
		Vec4f r(_mm_div_ps(_mm_load_ps(one_4vec), unitdir_.v));
		if(r.x[0] != recip_unitdir_f.x[0])
		{
			int a = 9;
		}
#endif
	}

	INDIGO_STRONG_INLINE ~Ray(){}

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

	INDIGO_STRONG_INLINE const float& minT() const { return min_t; }
	
private:
	//INDIGO_STRONG_INLINE void buildRecipRayDir();

	Vec4f startpos_f;
	Vec4f unitdir_f;
	Vec4f recip_unitdir_f;
	float min_t;

	//SSE_ALIGN Vec3d startpos;
	//SSE_ALIGN Vec3d unitdir;
	//SSE_ALIGN Vec3d recip_unitdir;
};


/*const Vec3d& Ray::getRecipRayDir() const
{
	//assert(built_recip_unitdir);
	return recip_unitdir;
}*/


#if 0
void Ray::buildRecipRayDir()
{
	//const SSE_ALIGN float raydir[4] = unitdir_f; // {(float)unitdir.x, (float)unitdir.y, (float)unitdir.z, 1.0f};
	const float MAX_RECIP = 1.0e26f;
	const SSE_ALIGN float MAX_RECIP_vec[4] = {MAX_RECIP, MAX_RECIP, MAX_RECIP, MAX_RECIP};

	this->recip_unitdir_f = Vec4f(
		_mm_min_ps(
			_mm_load_ps(MAX_RECIP_vec),
			_mm_div_ps(
				_mm_load_ps(one_4vec),
				unitdir_f.v
				)
			)
		);

	/*_mm_store_ps(
		&recip_unitdir_f.x,
		_mm_min_ps(
			_mm_load_ps(MAX_RECIP_vec),
			_mm_div_ps(
				_mm_load_ps(one_4vec),
				_mm_load_ps(raydir)
				)
			)
		);*/

	/*const SSE_ALIGN float raydir[4] = {(float)unitdir.x, (float)unitdir.y, (float)unitdir.z, 1.0f};

	_mm_store_ps(
		&recip_unitdir_f.x,
		_mm_div_ps(
			_mm_load_ps(one_4vec),
			_mm_load_ps(raydir)
			)
		);*/
}
#endif


#endif //__RAY_H_666_
