/*=====================================================================
TransformGizmo.cpp
------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "TransformGizmo.h"


#include "OpenGLEngine.h"
#include "MeshPrimitiveBuilding.h"
#include "../maths/vec2.h"
#include "../maths/plane.h"
#include "../maths/mathstypes.h"
#include "../maths/LineSegment4f.h"
#include "../graphics/SRGBUtils.h"


static const Colour3f axis_arrows_default_cols[]   = { Colour3f(0.6f,0.2f,0.2f), Colour3f(0.2f,0.6f,0.2f), Colour3f(0.2f,0.2f,0.6f) };
static const Colour3f axis_arrows_mouseover_cols[] = { Colour3f(1,0.45f,0.3f),   Colour3f(0.3f,1,0.3f),    Colour3f(0.3f,0.45f,1) };

// For each direction x, y, z, the two other basis vectors. 
static const Vec4f basis_vectors[6] = { Vec4f(0,1,0,0), Vec4f(0,0,1,0), Vec4f(0,0,1,0), Vec4f(1,0,0,0), Vec4f(1,0,0,0), Vec4f(0,1,0,0) };

static const float arc_handle_half_angle = 1.5f;


TransformGizmo::TransformGizmo(OpenGLEngine* engine_, const Vec4f& gizmo_centre)
:	engine(engine_),
	grabbed_axis(-1),
	grabbed_angle(0),
	original_grabbed_angle(0),
	grabbed_arc_angle_offset(0)
{
	axis_arrow_objects[0] = engine->makeArrowObject(Vec4f(0,0,0,1), Vec4f(1, 0, 0, 1), Colour4f(0.6, 0.2, 0.2, 1.f), 1.f);
	axis_arrow_objects[1] = engine->makeArrowObject(Vec4f(0,0,0,1), Vec4f(0, 1, 0, 1), Colour4f(0.2, 0.6, 0.2, 1.f), 1.f);
	axis_arrow_objects[2] = engine->makeArrowObject(Vec4f(0,0,0,1), Vec4f(0, 0, 1, 1), Colour4f(0.2, 0.2, 0.6, 1.f), 1.f);

	for(int i=0; i<NUM_AXIS_ARROWS; ++i)
	{
		axis_arrow_objects[i]->materials[0].albedo_linear_rgb = toLinearSRGB(axis_arrows_default_cols[i]);
		axis_arrow_objects[i]->always_visible = true;
		engine->addObject(axis_arrow_objects[i]);
	}

	for(int i=0; i<3; ++i)
	{
		GLObjectRef ob = engine->allocateObject();
		ob->ob_to_world_matrix = Matrix4f::translationMatrix((float)i * 3, 0, 2);
		ob->mesh_data = MeshPrimitiveBuilding::makeRotationArcHandleMeshData(*engine->vert_buf_allocator, arc_handle_half_angle * 2);
		ob->materials.resize(1);
		ob->materials[0].albedo_linear_rgb = toLinearSRGB(axis_arrows_default_cols[i]);
		ob->always_visible = true;
		rot_handle_arc_objects[i] = ob;
		engine->addObject(rot_handle_arc_objects[i]);
	}

	updateGizmoDrawTransform(gizmo_centre);
}


TransformGizmo::~TransformGizmo()
{
	for(int i=0; i<NUM_AXIS_ARROWS; ++i)
		checkRemoveObAndSetRefToNull(*engine, axis_arrow_objects[i]);

	for(int i=0; i<NUM_AXIS_ARROWS; ++i)
		checkRemoveObAndSetRefToNull(*engine, rot_handle_arc_objects[i]);
}


// Avoids NaNs
static float safeATan2(float y, float x)
{
	const float a = std::atan2(y, x);
	if(!isFinite(a))
		return 0.f;
	else
		return a;
}


void TransformGizmo::update(const Vec4f& ob_pos_ws)
{
	// If grabbed something, don't update from external changes
	if(grabbed_axis == -1)
	{
		updateGizmoDrawTransform(ob_pos_ws);
	}
}


// Project a world-space point to pixel coordinates.
// Returns false if the point is behind the camera.
bool TransformGizmo::worldToPixel(const Vec4f& ws_pos, OpenGLEngine* engine, Vec2f& px_out)
{
	return engine->worldSpacePosToPixel(ws_pos, px_out);
}


/*
Given a world-space line (p_a_ws, p_b_ws) and the pixel coordinates of the mouse,
returns the world-space point on the line whose screen-space projection is closest to px.

Let line coords in ws be p_ws(t) = a + b * t

pixel coords for a point p_ws are

cam_to_p = p_ws - cam_origin

r_x =  dot(cam_to_p, cam_right) / dot(cam_to_p, cam_forw)
r_y = -dot(cam_to_p, cam_up)    / dot(cam_to_p, cam_forw)

and

pixel_x = gl_w * (lens_sensor_dist / sensor_width  * r_x + 1/2)
pixel_y = gl_h * (lens_sensor_dist / sensor_height * r_y + 1/2)

let R = lens_sensor_dist / sensor_width

so 

pixel_x = gl_w * (R *  dot(p_ws - cam_origin, cam_right) / dot(p_ws - cam_origin, cam_forw) + 1/2)
pixel_y = gl_h * (R * -dot(p_ws - cam_origin, cam_up)    / dot(p_ws - cam_origin, cam_forw) + 1/2)

pixel_x = gl_w * (R *  dot(a + b * t - cam_origin, cam_right) / dot(a + b * t - cam_origin, cam_forw) + 1/2)
pixel_y = gl_h * (R * -dot(a + b * t - cam_origin, cam_up)    / dot(a + b * t - cam_origin, cam_forw) + 1/2)

We know pixel_x and pixel_y, want to solve for t.

pixel_x = gl_w * (R * dot(a + b * t - cam_origin, cam_right) / dot(a + b * t - cam_origin, cam_forw) + 1/2)
pixel_x/gl_w = R * dot(a + b * t - cam_origin, cam_right) / dot(a + b * t - cam_origin, cam_forw) + 1/2
pixel_x/gl_w = R * [dot(a - cam_origin, cam_right) + dot(b * t, cam_right)] / [dot(a - cam_origin, cam_forw) + dot(b * t, cam_forw)] + 1/2
pixel_x/gl_w - 1/2 = R  * [dot(a - cam_origin, cam_right) + dot(b * t, cam_right)] / [dot(a - cam_origin, cam_forw) + dot(b * t, cam_forw)]
(pixel_x/gl_w - 1/2) / R = [dot(a - cam_origin, cam_right) + dot(b * t, cam_right)] / [dot(a - cam_origin, cam_forw) + dot(b * t, cam_forw)]

let A = dot(a - cam_origin, cam_forw)
let B = dot(b, cam_forw)
let C = (pixel_x/gl_w - 1/2) / R
let D = dot(a - cam_origin, cam_right)
let E = dot(b, cam_right)

so we get

C = [D + dot(b * t, cam_right)] / [A + dot(b * t, cam_forw)]
C = [D + dot(b, cam_right) * t] / [A + dot(b, cam_forw) * t]
C = [D + E * t] / [A + B * t]
[A + B * t] C = D + E * t
AC + BCt = D + Et
BCt - Et = D - AC
t(BC - E) = D - AC
t = (D - AC) / (BC - E)


For y (used when all x coordinates are ~ the same)
pixel_y = gl_h * (R * -dot(a + b * t - cam_origin, cam_up) / dot(a + b * t - cam_origin, cam_forw) + 1/2)
pixel_y/gl_h = R * -dot(a + b * t - cam_origin, cam_up) / dot(a + b * t - cam_origin, cam_forw) + 1/2
pixel_y/gl_h = R * -[dot(a - cam_origin, cam_up) + dot(b * t, cam_up)] / [dot(a - cam_origin, cam_forw) + dot(b * t, cam_forw)] + 1/2
pixel_x/gl_w - 1/2 = R  * -[dot(a - cam_origin, cam_up) + dot(b * t, cam_up)] / [dot(a - cam_origin, cam_forw) + dot(b * t, cam_forw)]
(pixel_x/gl_w - 1/2) / R = -[dot(a - cam_origin, cam_right) + dot(b * t, cam_right)] / [dot(a - cam_origin, cam_forw) + dot(b * t, cam_forw)]

let A = dot(a - cam_origin, cam_forw)
let B = dot(b, cam_forw)
let C = (pixel_y/gl_h - 1/2) / R
let D = dot(a - cam_origin, cam_up)
let E = dot(b, cam_up)

C = -[D + dot(b * t, cam_up)] / [A + dot(b * t, cam_forw)]
C = -[D + dot(b, cam_right) * t] / [A + dot(b, cam_forw) * t]
C = -[D + E * t] / [A + B * t]
[A + B * t] C = -[D + E * t]
AC + BCt = -D - Et
BCt + Et = -D - AC
t(BC + E) = -D - AC
t = (-D - AC) / (BC + E)
*/
Vec4f TransformGizmo::pointOnLineWorldSpace(const Vec4f& p_a_ws, const Vec4f& p_b_ws, const Vec2f& px) const
{
	const Vec4f cam_pos      = engine->getCurrentScene()->cam_to_world.getColumn(3); // = cam_to_world * Vec4f(0,0,0,1);
	const Vec4f cam_forwards = engine->getCurrentScene()->cam_to_world.getColumn(1); // = cam_to_world * Vec4f(0,1,0,0);
	const Vec4f cam_right    = engine->getCurrentScene()->cam_to_world.getColumn(0); // = cam_to_world * Vec4f(1,0,0,0);
	const Vec4f cam_up       = engine->getCurrentScene()->cam_to_world.getColumn(2); // = cam_to_world * Vec4f(0,0,1,0);

	const int viewport_w = engine->getViewPortWidth(); // = gl_w
	const int viewport_h = engine->getViewPortHeight();
	const float sensor_w = engine->getCurrentScene()->use_sensor_width;
	const float sensor_h = engine->getCurrentScene()->use_sensor_height;
	const float lens_sensor_dist = engine->getCurrentScene()->lens_sensor_dist;

	//const float sensor_height = sensor_w * (float)viewport_h / (float)viewport_w;

	const Vec4f a = p_a_ws;
	const Vec4f b = normalise(p_b_ws - p_a_ws);

	float A = dot(a - cam_pos, cam_forwards);
	float B = dot(b, cam_forwards);
	float C = (px.x / (float)viewport_w - 0.5f) * sensor_w / lens_sensor_dist;
	float D = dot(a - cam_pos, cam_right);
	float E = dot(b, cam_right);

	const float denom = B*C - E;
	float t;
	if(std::fabs(denom) > 1.0e-4f)
	{
		t = (D - A*C) / denom;
	}
	else
	{
		// Work with y instead

		A = dot(a - cam_pos, cam_forwards);
		B = dot(b, cam_forwards);
		C = (px.y / (float)viewport_h - 0.5f) * sensor_h / lens_sensor_dist;
		D = dot(a - cam_pos, cam_up);
		E = dot(b, cam_up);

		t = (-D - A*C) / (B*C + E);
	}

	return a + b * t;
}


static LineSegment4f clipLineSegmentToCameraFrontHalfSpace(const LineSegment4f& segment, const Planef& cam_front_plane)
{
	const float d_a = cam_front_plane.signedDistToPoint(segment.a);
	const float d_b = cam_front_plane.signedDistToPoint(segment.b);

	// If both endpoints are in front half-space, no clipping is required.  If both points are in back half-space, line segment is completely clipped.
	// In this case just return the unclipped line segment.
	if((d_a < 0 && d_b < 0) || (d_a > 0 && d_b > 0))
		return segment;

	/*
	
	a                  /         b
	------------------/----------
	d_a              /   d_b

	*/
	if(d_a > 0)
	{
		assert(d_b < 0);
		const float frac = d_a / (d_a - d_b); // = d_a / (d_a + |d_b|)
		return LineSegment4f(segment.a, Maths::lerp(segment.a, segment.b, frac));
	}
	else
	{
		assert(d_a < 0);
		assert(d_b >= 0);
		const float frac = -d_a / (-d_a + d_b); // = |d_a| / (|d_a| + d_b)
		return LineSegment4f(segment.b, Maths::lerp(segment.a, segment.b, frac));
	}
}

// See https://math.stackexchange.com/questions/1036959/midpoint-of-the-shortest-distance-between-2-rays-in-3d
// In particular this answer: https://math.stackexchange.com/a/2371053
static Vec4f closestPointOnLineToRay(const LineSegment4f& line, const Vec4f& origin, const Vec4f& unitdir)
{
	const Vec4f a = line.a;
	const Vec4f b = normalise(line.b - line.a);
	const Vec4f c = origin;
	const Vec4f d = unitdir;

	const float t = (dot(c - a, b) + dot(a - c, d) * dot(b, d)) / (1.f - Maths::square(dot(b, d)));
	return a + b * t;
}


// NOTE: pretty much the same as clipLineSegmentToCameraFrontHalfSpace, remove
inline static bool clipLineToPlaneBackHalfSpace(const Planef& plane, Vec4f& a, Vec4f& b)
{
	const float ad = plane.signedDistToPoint(a);
	const float bd = plane.signedDistToPoint(b);
	if(ad > 0 && bd > 0) // If both endpoints not in back half space:
		return false;

	if(ad <= 0 && bd <= 0) // If both endpoints in back half space:
		return true;

	// Else line straddles plane
	// ad + (bd - ad) * t = 0
	// t = -ad / (bd - ad)
	// t = ad / -(bd - ad)
	// t = ad / (-bd + ad)
	// t = ad / (ad - bd)

	const float t = ad / (ad - bd);
	const Vec4f on_plane_p = a + (b - a) * t;
	//assert(epsEqual(plane.signedDistToPoint(on_plane_p), 0.f));

	if(ad <= 0) // If point a lies in back half space:
		b = on_plane_p; // update point b
	else
		a = on_plane_p; // else point b lies in back half space, so update point a
	return true;
}


void TransformGizmo::updateGizmoDrawTransform(const Vec4f& new_gizmo_centre)
{
	const Vec4f cam_pos = engine->getCurrentScene()->cam_to_world.getColumn(3); // = cam_to_world * Vec4f(0,0,0,1);

	const Vec4f gizmo_centre = new_gizmo_centre;
	const Vec4f cam_to_gizmo = gizmo_centre - cam_pos;
	const float control_scale = cam_to_gizmo.length() * 0.2f;

	const float arrow_len = control_scale;

	// Flip each arrow to point toward the camera.
	axis_arrow_segments[0] = LineSegment4f(gizmo_centre, gizmo_centre + Vec4f(cam_to_gizmo[0] > 0 ? -arrow_len : arrow_len, 0, 0, 0));
	axis_arrow_segments[1] = LineSegment4f(gizmo_centre, gizmo_centre + Vec4f(0, cam_to_gizmo[1] > 0 ? -arrow_len : arrow_len, 0, 0));
	axis_arrow_segments[2] = LineSegment4f(gizmo_centre, gizmo_centre + Vec4f(0, 0, cam_to_gizmo[2] > 0 ? -arrow_len : arrow_len, 0));

	for(int i=0; i<NUM_AXIS_ARROWS; ++i)
	{
		axis_arrow_objects[i]->ob_to_world_matrix = OpenGLEngine::arrowObjectTransform(axis_arrow_segments[i].a, axis_arrow_segments[i].b, arrow_len);
		engine->updateObjectTransformData(*axis_arrow_objects[i]);
	}

	//----------------------- Update rotation control handle arcs -----------------------
	const float arc_radius = control_scale * 0.7f;

	for(int i=0; i<3; ++i)
	{
		const Vec4f basis_a = basis_vectors[i*2];
		const Vec4f basis_b = basis_vectors[i*2 + 1];

		const Vec4f to_cam = cam_pos - gizmo_centre;
		const float to_cam_angle = safeATan2(dot(basis_b, to_cam), dot(basis_a, to_cam));

		// Position the rotation arc so its oriented towards the camera, unless the user is currently holding and dragging the arc.
		float angle = to_cam_angle;
		if(grabbed_axis >= NUM_AXIS_ARROWS)
		{
			const int grabbed_rot_axis = grabbed_axis - NUM_AXIS_ARROWS;
			if(i == grabbed_rot_axis)
				angle = grabbed_angle + grabbed_arc_angle_offset;
		}

		// Position the arc line segments used for mouse picking.
		const float start_angle = angle - arc_handle_half_angle - 0.1f;; // Extend a little so the arrow heads can be selected
		const float end_angle   = angle + arc_handle_half_angle + 0.1f;

		const size_t N = 32;
		rot_handle_lines[i].resize(N);
		for(size_t z=0; z<N; ++z)
		{
			const float theta_0 = start_angle + (end_angle - start_angle) * (float)z       / (float)N;
			const float theta_1 = start_angle + (end_angle - start_angle) * (float)(z + 1) / (float)N;

			const Vec4f p0 = gizmo_centre + basis_a * (cos(theta_0) * arc_radius) + basis_b * (sin(theta_0) * arc_radius);
			const Vec4f p1 = gizmo_centre + basis_a * (cos(theta_1) * arc_radius) + basis_b * (sin(theta_1) * arc_radius);

			rot_handle_lines[i][z] = LineSegment4f(p0, p1);
		}

		rot_handle_arc_objects[i]->ob_to_world_matrix =
			Matrix4f::translationMatrix(gizmo_centre) *
			Matrix4f::rotationMatrix(crossProduct(basis_a, basis_b), angle - arc_handle_half_angle) *
			Matrix4f(basis_a, basis_b, crossProduct(basis_a, basis_b), Vec4f(0,0,0,1)) *
			Matrix4f::uniformScaleMatrix(arc_radius);

		engine->updateObjectTransformData(*rot_handle_arc_objects[i]);
	}
}


// Returns the axis index (integer in [0, 3)) of the closest axis arrow, or the axis index of the closest rotation arc handle (integer in [3, 6))
// or -1 if no arrow or rotation arc close to pixel coords.
// Also returns world space coords of the closest point.
int TransformGizmo::mouseOverAxisArrowOrRotArc(const Vec2f& px, Vec4f& closest_ws_out)
{
	const Vec4f cam_pos      = engine->getCurrentScene()->cam_to_world.getColumn(3); // = cam_to_world * Vec4f(0,0,0,1);
	const Vec4f cam_forwards = engine->getCurrentScene()->cam_to_world.getColumn(1); // = cam_to_world * Vec4f(0,1,0,0);

	const int viewport_w = engine->getViewPortWidth(); // = gl_w

	const float max_selection_dist = 12.f;
	float closest_dist = 10000.f;
	int closest_axis = -1;

	const Planef cam_front_plane(cam_pos + cam_forwards * 0.01f, cam_forwards);
	const Vec4f ray_dir = engine->pixelToRayDirWS(px);

	// Test translation arrows.
	for(int i=0; i<NUM_AXIS_ARROWS; ++i)
	{
		const LineSegment4f segment = clipLineSegmentToCameraFrontHalfSpace(axis_arrow_segments[i], cam_front_plane);

		Vec2f start_px, end_px;
		if(!worldToPixel(segment.a, engine, start_px)) continue;
		if(!worldToPixel(segment.b, engine, end_px))   continue;

		const float d = pointLineSegmentDist(px, start_px, end_px);

		const Vec4f closest_pt = closestPointOnLineToRay(segment, cam_pos, ray_dir);
		const float cam_dist = closest_pt.getDist(cam_pos);
		const float approx_radius_px = 0.03f * (float)viewport_w / cam_dist;
		const float use_max_dist = myMax(max_selection_dist, approx_radius_px);

		if(d <= closest_dist && d < use_max_dist)
		{
			closest_ws_out = closest_pt;
			closest_dist = d;
			closest_axis = i;
		}
	}

	// Test rotation arc line segments.
	for(int i=0; i<3; ++i)
	{
		for(size_t z=0; z<rot_handle_lines[i].size(); ++z)
		{
			const LineSegment4f& segment = rot_handle_lines[i][z];

			Vec2f start_px, end_px;
			if(!worldToPixel(segment.a, engine, start_px)) continue;
			if(!worldToPixel(segment.b, engine, end_px))   continue;

			const float d = pointLineSegmentDist(px, start_px, end_px);

			const Vec4f closest_pt = closestPointOnLineToRay(segment, cam_pos, ray_dir);
			const float cam_dist = closest_pt.getDist(cam_pos);
			const float approx_radius_px = 0.02f * (float)viewport_w / cam_dist;
			const float use_max_dist = myMax(max_selection_dist, approx_radius_px);

			if(d <= closest_dist && d < use_max_dist)
			{
				closest_ws_out = closest_pt;
				closest_dist = d;
				closest_axis = NUM_AXIS_ARROWS + i;
			}
		}
	}

	return closest_axis;
}


bool TransformGizmo::mousePressed(const Vec2f& px, const Vec4f& ob_pos_ws, GizmoDelegateInterface* delegate)
{
	const int axis = mouseOverAxisArrowOrRotArc(px,  grabbed_point_ws);
	if(axis < 0)
		return false;

	grabbed_axis        = axis;
	ob_origin_at_grab   = ob_pos_ws;

	delegate->onGrabStart(grabbed_axis >= NUM_AXIS_ARROWS);

	if(grabbed_axis >= NUM_AXIS_ARROWS)
	{
		// Compute the initial angle on the rotation plane so we can track deltas.
		const Vec4f cam_pos = engine->getCurrentScene()->cam_to_world.getColumn(3); // = cam_to_world * Vec4f(0,0,0,1);
		const int rot_axis = grabbed_axis - NUM_AXIS_ARROWS;
		const Vec4f basis_a = basis_vectors[rot_axis*2];
		const Vec4f basis_b = basis_vectors[rot_axis*2 + 1];
		const Vec4f arc_centre = ob_pos_ws;

		const Vec4f dir = engine->pixelToRayDirWS(px);
		const Planef plane(arc_centre, crossProduct(basis_a, basis_b));
		const float t = plane.rayIntersect(cam_pos, dir);
		const Vec4f plane_p = cam_pos + dir * t;

		const float angle = safeATan2(dot(plane_p - arc_centre, basis_b), dot(plane_p - arc_centre, basis_a));

		const Vec4f to_cam = cam_pos - arc_centre;
		const float to_cam_angle = safeATan2(dot(basis_b, to_cam), dot(basis_a, to_cam));

		grabbed_angle = original_grabbed_angle = angle;
		grabbed_arc_angle_offset = to_cam_angle - original_grabbed_angle;
	}

	return true;
}


bool TransformGizmo::mouseMoved(const Vec2f& px, const Vec4f& ob_pos_ws, GizmoDelegateInterface* delegate, float grid_spacing)
{
	if(grabbed_axis < 0)
		return false;

	const Vec4f cam_pos      = engine->getCurrentScene()->cam_to_world.getColumn(3); // = cam_to_world * Vec4f(0,0,0,1);
	const Vec4f cam_forwards = engine->getCurrentScene()->cam_to_world.getColumn(1); // = cam_to_world * Vec4f(0,1,0,0);

	if(grabbed_axis < NUM_AXIS_ARROWS)
	{
		// Translation drag: project mouse onto the grabbed world-space axis line.
		const float MAX_MOVE_DIST = 100.f;
		const Vec4f line_dir = normalise(axis_arrow_segments[grabbed_axis].b - axis_arrow_segments[grabbed_axis].a);
		Vec4f use_line_start = axis_arrow_segments[grabbed_axis].a - line_dir * MAX_MOVE_DIST;
		Vec4f use_line_end   = axis_arrow_segments[grabbed_axis].a + line_dir * MAX_MOVE_DIST;

		const Planef clip_plane(cam_pos + cam_forwards * 0.1f, cam_forwards * -1.f);
		if(!clipLineToPlaneBackHalfSpace(clip_plane, use_line_start, use_line_end))
			return true;

		Vec2f start_px, end_px;
		const bool sv = worldToPixel(use_line_start, engine, start_px);
		const bool ev = worldToPixel(use_line_end,   engine, end_px);

		if(sv && ev)
		{
			const Vec2f closest_px = closestPointOnLineSegment(px, start_px, end_px);
			Vec4f new_p = pointOnLineWorldSpace(axis_arrow_segments[grabbed_axis].a, axis_arrow_segments[grabbed_axis].b, closest_px);

			Vec4f delta_p = new_p - grabbed_point_ws;
			Vec4f tentative = ob_origin_at_grab + delta_p;

			if(tentative.getDist(ob_origin_at_grab) > MAX_MOVE_DIST)
				tentative = ob_origin_at_grab + (tentative - ob_origin_at_grab) * MAX_MOVE_DIST / (tentative - ob_origin_at_grab).length();

			// Grid snap (grabbed axis only).
			if(grid_spacing > 1.0e-5f)
				tentative[grabbed_axis] = (float)Maths::roundToMultipleFloating((double)tentative[grabbed_axis], (double)grid_spacing);

			updateGizmoDrawTransform(/*new_gizmo_centre=*/tentative);

			const Vec4f total_translation = tentative - ob_origin_at_grab;
			delegate->onTranslationDrag(total_translation, tentative);
		}
	}
	else
	{
		// Rotation drag: intersect mouse ray with the rotation plane.
		const int rot_axis = grabbed_axis - NUM_AXIS_ARROWS;
		const Vec4f basis_a = basis_vectors[rot_axis*2];
		const Vec4f basis_b = basis_vectors[rot_axis*2 + 1];
		const Vec4f arc_centre = ob_origin_at_grab;

		const Vec4f dir = engine->pixelToRayDirWS(px);
		const Planef plane(arc_centre, crossProduct(basis_a, basis_b));
		const float t = plane.rayIntersect(cam_pos, dir);
		const Vec4f plane_p = cam_pos + dir * t;

		const float angle = safeATan2(dot(plane_p - arc_centre, basis_b), dot(plane_p - arc_centre, basis_a));
		const float delta = angle - grabbed_angle;

		updateGizmoDrawTransform(/*new_gizmo_centre=*/ob_origin_at_grab);

		const float total_angle_change = angle - original_grabbed_angle;
		delegate->onRotationDrag(crossProduct(basis_a, basis_b), total_angle_change, delta);

		grabbed_angle = angle;
	}

	return true;
}


bool TransformGizmo::mouseReleased(GizmoDelegateInterface* delegate)
{
	if(grabbed_axis < 0)
		return false;

	grabbed_axis = -1;
	delegate->onGrabEnd();
	return true;
}


void TransformGizmo::updateMouseoverHighlight(const Vec2f& px)
{
	// Reset all to default colours.
	for(int i=0; i<NUM_AXIS_ARROWS; ++i)
	{
		axis_arrow_objects[i]->materials[0].albedo_linear_rgb = toLinearSRGB(axis_arrows_default_cols[i % 3]);
		engine->objectMaterialsUpdated(*axis_arrow_objects[i]);
	}
	for(int i=0; i<3; ++i)
	{
		rot_handle_arc_objects[i]->materials[0].albedo_linear_rgb = toLinearSRGB(axis_arrows_default_cols[i]);
		engine->objectMaterialsUpdated(*rot_handle_arc_objects[i]);
	}

	Vec4f dummy;
	const int axis = mouseOverAxisArrowOrRotArc(px, dummy);
	if(axis < 0)
		return;

	if(axis < NUM_AXIS_ARROWS)
	{
		axis_arrow_objects[axis]->materials[0].albedo_linear_rgb = toLinearSRGB(axis_arrows_mouseover_cols[axis]);
		engine->objectMaterialsUpdated(*axis_arrow_objects[axis]);
	}
	else
	{
		const int rot_axis = axis - NUM_AXIS_ARROWS;
		rot_handle_arc_objects[rot_axis]->materials[0].albedo_linear_rgb = toLinearSRGB(axis_arrows_mouseover_cols[rot_axis]);
		engine->objectMaterialsUpdated(*rot_handle_arc_objects[rot_axis]);
	}
}
