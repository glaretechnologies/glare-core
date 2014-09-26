/*=====================================================================
camera.cpp
----------
File created by ClassTemplate on Sun Nov 14 04:06:01 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "camera.h"


Camera::Camera(CameraType camera_type_)
:	Geometry(
		false // sub-elements curved
	),
	camera_type(camera_type_)
{}


Camera::~Camera()
{}


void Camera::setContainingMedia(const std::vector<const Medium*>& media_)
{
	containing_media = media_;
}
