/*=====================================================================
CameraController.cpp
--------------------
Copyright Glare Technologies Limited 2010 -
Generated at Thu Dec 09 17:24:15 +1300 2010
=====================================================================*/
#include "CameraController.h"

#include <math.h>


CameraController::CameraController()
{
	initialise(Vec3d(0.0), Vec3d(0, 1, 0), Vec3d(0, 0, 1));

	base_move_speed   = 0.035;
	base_rotate_speed = 0.005;

	move_speed_scale   = 1;
	rotate_speed_scale = 1;

	invert_mouse = false;
}


CameraController::~CameraController()
{

}


void CameraController::initialise(const Vec3d& cam_pos, const Vec3d& cam_forward, const Vec3d& cam_up)
{
	// Copy camera position
	position = cam_pos;

	// Construct basis assuming world up direction is <0,0,1> if possible
	Vec3d world_up = Vec3d(0, 0, 1);
	Vec3d camera_up, camera_forward, camera_right;

	// If camera forward dir is collinear with world up just use provided basis
	if(::epsEqual(absDot(world_up, normalise(cam_forward)), 1.0))
	{
		camera_forward = normalise(cam_forward);
		camera_up = normalise(cam_up);
		camera_right = crossProduct(camera_forward, camera_up);
	}
	else
	{
		camera_forward = normalise(cam_forward);
		camera_up = world_up; camera_up.removeComponentInDir(camera_forward); camera_up.normalise();
		camera_right = crossProduct(camera_forward, camera_up);
	}

	rotation.x = acos(cam_forward.z / cam_forward.length());
	rotation.y = atan2(cam_forward.y, cam_forward.x);

	const Vec3d rollplane_x_basis = crossProduct(camera_forward, world_up);
	const Vec3d rollplane_y_basis = crossProduct(rollplane_x_basis, camera_forward);
	const double rollplane_x = dot(camera_right, rollplane_x_basis);
	const double rollplane_y = dot(camera_right, rollplane_y_basis);

	rotation.z = atan2(rollplane_y, rollplane_x);
}


void CameraController::update(const Vec3d& pos_delta, const Vec2d& rot_delta)
{
	const double rotate_speed = base_rotate_speed * rotate_speed_scale;
	const double move_speed   = base_move_speed * move_speed_scale;

	// Accumulate rotation angles, taking into account mouse speed and invertedness.
	rotation.x += rot_delta.x * -rotate_speed * (invert_mouse ? -1 : 1);
	rotation.y += rot_delta.y * -rotate_speed;

	const double pi = 3.1415926535897932384626433832795, cap = 1e-4;
	rotation.x = std::max(cap, std::min(pi - cap, rotation.x));

	// Construct camera basis.
	Vec3d forwards(	sin(rotation.x) * cos(rotation.y),
					sin(rotation.x) * sin(rotation.y),
					cos(rotation.x));

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
	rotate_speed_scale = (float)pow(2.0, sensitivity);
}


void CameraController::setMoveScale(double move_scale)
{
	move_speed_scale = move_scale;
}


void CameraController::getBasis(Vec3d& right_out, Vec3d& up_out, Vec3d& forward_out) const
{
	forward_out = Vec3d(sin(rotation.x) * cos(rotation.y),
						sin(rotation.x) * sin(rotation.y),
						cos(rotation.x));

	up_out = Vec3d(0, 0, 1);
	up_out.removeComponentInDir(forward_out);
	up_out.normalise();

	right_out = ::crossProduct(forward_out, up_out);


	const Matrix3d roll_basis = Matrix3d::rotationMatrix(forward_out, rotation.z) * Matrix3d(right_out, forward_out, up_out);

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

	// Initialise canonical viewing system
	cc.initialise(Vec3d(0.0), Vec3d(0, 1, 0), Vec3d(0, 0, 1));

	Vec3d basis[3];
	cc.getBasis(basis[0], basis[1], basis[2]);

	testAssert(::epsEqual(basis[0].x, 1.0)); testAssert(::epsEqual(basis[0].y, 0.0)); testAssert(::epsEqual(basis[0].z, 0.0));
	testAssert(::epsEqual(basis[1].x, 0.0)); testAssert(::epsEqual(basis[1].y, 0.0)); testAssert(::epsEqual(basis[1].z, 1.0));
	testAssert(::epsEqual(basis[2].x, 0.0)); testAssert(::epsEqual(basis[2].y, 1.0)); testAssert(::epsEqual(basis[2].z, 0.0));
}

#endif // BUILD_TESTS
