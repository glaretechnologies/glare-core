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

	void update(const Vec3d& pos_delta, const Vec2d& rot_delta);

	Vec3d getPosition() const;
	void setPosition(const Vec3d& pos);

	void getBasis(Vec3d& right_out, Vec3d& up_out, Vec3d& forward_out) const;
	void setBasis(const Vec3d& up, const Vec3d& forward);

private:

	//Vec3d current_position;
	//double move_speed;

	//double roll_angle, roll_speed;
	//double pitch_angle, pitch_speed;
	//double yaw_angle, yaw_speed;

	//Vec3d roll_vector;
	//Vec3d pitch_vector;
	//Vec3d yaw_vector;

	//Quatd current_orientation;
	//Vec3d rotation_axes[3];

	Vec3d camera_up;

	Vec3d position;
	Vec2d rotation; // Specified as (theta, phi).

	float move_speed, rotate_speed;

	bool invert_mouse;
};
