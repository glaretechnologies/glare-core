/*=====================================================================
ThreadContext.h
---------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#if IS_INDIGO
#include "../lang/WinterEnv.h"
#endif
#include <hitinfo.h>
#include "../utils/Platform.h"
#include "../utils/Vector.h"
#include <vector>
class Object;


struct EmbreeHitInfo
{
	inline bool operator < (const EmbreeHitInfo& other) const { return hit_t < other.hit_t; }
	HitInfo hitinfo;
	float hit_t;
	const Object* ob;
};


/*=====================================================================
ThreadContext
-------------
Contains per-thread data structures used for stuff like tree traversal and
shader evaluation.
=====================================================================*/
class ThreadContext
{
public:
	ThreadContext();
	~ThreadContext();

	GLARE_ALIGNED_16_NEW_DELETE

#if IS_INDIGO
	GLARE_STRONG_INLINE WinterEnv& getWinterEnv() { return winter_env; }
#endif

	// For MapEnvEmitter::getSpectralRadiance
	std::vector<float> temp_buffer;

	// Used in World::getTransmittanceAlongEdge().
	js::Vector<EmbreeHitInfo, 16> embree_hits;

	static void setThreadLocalContext(ThreadContext* context);
	static ThreadContext* getThreadLocalContext();

private:
	ThreadContext(const ThreadContext& other);
	ThreadContext& operator = (const ThreadContext& other);

#if IS_INDIGO
	WinterEnv winter_env;
#endif
};
