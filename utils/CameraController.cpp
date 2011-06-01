/*=====================================================================
CameraController.cpp
--------------------
Copyright Glare Technologies Limited 2010 -
Generated at Thu Dec 09 17:24:15 +1300 2010
=====================================================================*/
#include "CameraController.h"

#include <math.h>

#include "../maths/mathstypes.h"


CameraController::CameraController()
{
	initialise(Vec3d(0.0), Vec3d(0, 1, 0), Vec3d(0, 0, 1));

	base_move_speed   = 0.035;
	base_rotate_speed = 0.005;

	move_speed_scale = 1;
	mouse_sensitivity_scale = 1;

	invert_mouse = false;
}


CameraController::~CameraController()
{

}


void CameraController::initialise(const Vec3d& cam_pos, const Vec3d& cam_forward, const Vec3d& cam_up)
{
	// Copy camera position and provided camera up vector
	position = cam_pos;
	initialised_up = cam_up;

	// Construct basis assuming world up direction is <0,0,1> if possible
	Vec3d world_up = Vec3d(0, 0, 1);
	Vec3d camera_up, camera_forward, camera_right;

	camera_forward = normalise(cam_forward);
	camera_up = getUpForForwards(cam_forward, initialised_up);
	camera_right = crossProduct(camera_forward, camera_up);

	rotation.x = atan2(cam_forward.y, cam_forward.x);
	rotation.y = acos(cam_forward.z / cam_forward.length());

	const Vec3d rollplane_x_basis = crossProduct(camera_forward, cam_up);
	const Vec3d rollplane_y_basis = crossProduct(rollplane_x_basis, camera_forward);
	const double rollplane_x = dot(camera_right, rollplane_x_basis);
	const double rollplane_y = dot(camera_right, rollplane_y_basis);

	rotation.z = atan2(rollplane_y, rollplane_x);
}


void CameraController::update(const Vec3d& pos_delta, const Vec2d& rot_delta)
{
	const double rotate_speed = base_rotate_speed * mouse_sensitivity_scale;
	const double move_speed   = base_move_speed * mouse_sensitivity_scale * move_speed_scale;

	// Accumulate rotation angles, taking into account mouse speed and invertedness.
	rotation.x += rot_delta.y * -rotate_speed;
	rotation.y += rot_delta.x * -rotate_speed * (invert_mouse ? -1 : 1);

	const double pi = 3.1415926535897932384626433832795, cap = 1e-4;
	rotation.y = std::max(cap, std::min(pi - cap, rotation.y));

	// Construct camera basis.
	Vec3d forwards(	sin(rotation.y) * cos(rotation.x),
					sin(rotation.y) * sin(rotation.x),
					cos(rotation.y));

	Vec3d up(0, 0, 1); up.removeComponentInDir(forwards); up.normalise();
	Vec3d right = ::crossProduct(forwards, up);

	position += right		* pos_delta.x * move_speed +
				forwards	* pos_delta.y * move_speed +
				up			* pos_delta.z * move_speed;
}


Vec3d CameraController::getPosition() const
{
	return position;
}


void CameraController::setPosition(const Vec3d& pos)
{
	position = pos;
}


void CameraController::setMouseSensitivity(double sensitivity)
{
	const double speed_base = (1 + std::sqrt(5.0)) * 0.5;
	mouse_sensitivity_scale = (float)pow(speed_base, sensitivity);
}


void CameraController::setMoveScale(double move_scale)
{
	move_speed_scale = move_scale;
}


void CameraController::getBasis(Vec3d& right_out, Vec3d& up_out, Vec3d& forward_out) const
{
	getBasisForAngles(rotation, right_out, up_out, forward_out);
}


void CameraController::getAngles(Vec3d& angles_out)
{
	angles_out = rotation;
}


Vec3d CameraController::getUpForForwards(const Vec3d& forwards, const Vec3d& singular_up)
{
	Vec3d up_out;
	Vec3d world_up = Vec3d(0, 0, 1);

	if(::epsEqual(absDot(world_up, forwards), 1.0))
	{
		up_out = singular_up;
	}
	else
	{
		up_out = world_up;
		up_out.removeComponentInDir(forwards);
	}

	return normalise(up_out);
}


void CameraController::getBasisForAngles(const Vec3d& angles_in, Vec3d& right_out, Vec3d& up_out, Vec3d& forward_out)
{
	// Get un-rolled basis
	forward_out = Vec3d(sin(angles_in.y) * cos(angles_in.x),
						sin(angles_in.y) * sin(angles_in.x),
						cos(angles_in.y));
	up_out = getUpForForwards(forward_out, Vec3d(0, 1, 0));
	right_out = ::crossProduct(forward_out, up_out);

	// Apply camera roll
	const Matrix3d roll_basis = Matrix3d::rotationMatrix(forward_out, angles_in.z) * Matrix3d(right_out, forward_out, up_out);
	right_out	= roll_basis.getColumn0();
	forward_out	= roll_basis.getColumn1();
	up_out		= roll_basis.getColumn2();
}


#if BUILD_TESTS

#include "../maths/mathstypes.h"
#include "../indigo/TestUtils.h"

void CameraController::test()
{
	CameraController cc;
	Vec3d r, f, u, angles;


	// Initialise canonical viewing system - camera at origin, looking along y+ with z+ up
	cc.initialise(Vec3d(0.0), Vec3d(0, 1, 0), Vec3d(0, 0, 1));
	cc.getBasis(r, u, f);
	testAssert(::epsEqual(r.x, 1.0)); testAssert(::epsEqual(r.y, 0.0)); testAssert(::epsEqual(r.z, 0.0));
	testAssert(::epsEqual(f.x, 0.0)); testAssert(::epsEqual(f.y, 1.0)); testAssert(::epsEqual(f.z, 0.0));
	testAssert(::epsEqual(u.x, 0.0)); testAssert(::epsEqual(u.y, 0.0)); testAssert(::epsEqual(u.z, 1.0));


	// Initialise camera to look down along z-, with y+ up
	cc.initialise(Vec3d(0.0), Vec3d(0, 0, -1), Vec3d(0, 1, 0));
	cc.getBasis(r, u, f);
	testAssert(::epsEqual(r.x, 1.0)); testAssert(::epsEqual(r.y, 0.0)); testAssert(::epsEqual(r.z,  0.0));
	testAssert(::epsEqual(f.x, 0.0)); testAssert(::epsEqual(f.y, 0.0)); testAssert(::epsEqual(f.z, -1.0));
	testAssert(::epsEqual(u.x, 0.0)); testAssert(::epsEqual(u.y, 1.0)); testAssert(::epsEqual(u.z,  0.0));


	// Initialise canonical viewing system and test that the viewing angles are correct
	cc.initialise(Vec3d(0.0), Vec3d(0, 0, -1), Vec3d(0, 1, 0));
	cc.getAngles(angles);
	testAssert(::epsEqual(angles.x, 0.0)); testAssert(::epsEqual(angles.y, NICKMATHS_PI)); testAssert(::epsEqual(angles.z,  0.0));

	// Apply a rotation along z (roll) of 90 degrees, or pi/4 radians
	angles.z = -NICKMATHS_PI_2;
	CameraController::getBasisForAngles(angles, r, u, f);

	//sdfgdfghd
}

#endif // BUILD_TESTS
