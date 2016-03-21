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
	Vec3d getAngles() const; // Specified as (heading, pitch, roll).

	Vec3d getForwardsVec() const;
	Vec3d getRightVec() const;

	void setAllowPitching(bool allow_pitching);

	static Vec3d getUpForForwards(const Vec3d& forwards, const Vec3d& singular_up);
	static void getBasisForAngles(const Vec3d& angles_in, const Vec3d& singular_up, Vec3d& right_out, Vec3d& up_out, Vec3d& forward_out);

	static void getAxisAngleForAngles(const Vec3d& euler_angles_in, Vec3d& axis_out, double& angle_out);

	bool invert_mouse;

	static void test();

private:

	Vec3d position;
	Vec3d rotation; // Specified as (heading, pitch, roll).
	// pitch is theta. 0 = looking straight up, pi = looking straight down.

	Vec3d initialised_up;

	double base_move_speed, base_rotate_speed;
	double move_speed_scale, mouse_sensitivity_scale;

	// Spherical camera doesn't allow looking up or down. So for spherical camera, allow_pitching should be set to false.
	bool allow_pitching;
};
