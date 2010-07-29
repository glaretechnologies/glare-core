/*=====================================================================
Code By Nicholas Chapman.

  nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may *not* use this code for any commercial project.
=====================================================================*/
#ifndef __RAY_H_666_
#define __RAY_H_666_


#include "../maths/Vec4f.h"
#include "../maths/mathstypes.h"
#include "../maths/SSE.h"


//#define USE_LAUNCH_NORMAL 1


SSE_CLASS_ALIGN Ray
{
public:
	INDIGO_STRONG_INLINE Ray() {}

	INDIGO_STRONG_INLINE Ray(const Vec4f& startpos_, const Vec4f& unitdir_
#if USE_LAUNCH_NORMAL
		, const Vec4f& launch_normal_
#endif
		)
	:	startpos_f(startpos_),
		unitdir_f(unitdir_)
#if USE_LAUNCH_NORMAL
		,launch_normal(launch_normal_)//,
#endif
	{
		assert(epsEqual(startpos_.x[3], 1.0f));
		assert(epsEqual(unitdir_.x[3], 0.0f));
		//assert(unitdir_.isUnitLength()); // NOTE: taken out because rays don't always use unit vector directions
		assert(SSE::isSSEAligned(this));

		origin_error = startpos_.length() * 2.0e-5f;

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

	//INDIGO_STRONG_INLINE const float& minT() const { return min_t; }
	
#if USE_LAUNCH_NORMAL
	INDIGO_STRONG_INLINE const Vec4f& launchNormal() const { return launch_normal; }
#endif

	float origin_error;
	float padding[3];

private:
	Vec4f startpos_f;
	Vec4f unitdir_f;
#if USE_LAUNCH_NORMAL
	Vec4f launch_normal;
#endif
	Vec4f recip_unitdir_f;
};


#endif //__RAY_H_666_
