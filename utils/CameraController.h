/*=====================================================================
CameraController.h
------------------
Copyright Glare Technologies Limited 2010 -
Generated at Thu Dec 09 17:24:15 +1300 2010
=====================================================================*/
#pragma once

#include "../maths/vec3.h"
#include "../maths/matrix3.h"
#include "../maths/Quat.h"


/*=====================================================================
CameraController
----------------

=====================================================================*/
class CameraController
{
public:
	CameraController();
	~CameraController();

	void initialise(const Vec3d& cam_pos, const Vec3d& cam_forwards, const Vec3d& cam_up);

	void update(const Vec3d& pos_delta, const Vec2d& rot_delta);

	Vec3d getPosition() const;
	void setPosition(const Vec3d& pos);

	void setMouseSensitivity(double sensitivity);
	void setMoveScale(double move_scale);

	void getBasis(Vec3d& right_out, Vec3d& up_out, Vec3d& forward_out) const;

	bool invert_mouse;

	static void test();

private:

	Vec3d position;
	Vec3d rotation; // Specified as (pitch, heading, roll).

	double base_move_speed, base_rotate_speed;
	double move_speed_scale, rotate_speed_scale;
};
