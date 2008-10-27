#include "ray.h"


void Ray::buildRecipRayDir()
{
	const SSE_ALIGN float raydir[4] = {(float)unitdir.x, (float)unitdir.y, (float)unitdir.z, 1.0f};
	const float MAX_RECIP = 1.0e26f;
	const SSE_ALIGN float MAX_RECIP_vec[4] = {MAX_RECIP, MAX_RECIP, MAX_RECIP, MAX_RECIP};

	_mm_store_ps(
		&recip_unitdir_f.x,
		_mm_min_ps(
			_mm_load_ps(MAX_RECIP_vec),
			_mm_div_ps(
				_mm_load_ps(one_4vec),
				_mm_load_ps(raydir)
				)
			)
		);

	/*const SSE_ALIGN float raydir[4] = {(float)unitdir.x, (float)unitdir.y, (float)unitdir.z, 1.0f};

	_mm_store_ps(
		&recip_unitdir_f.x,
		_mm_div_ps(
			_mm_load_ps(one_4vec),
			_mm_load_ps(raydir)
			)
		);*/
}
