/*=====================================================================
TransformGizmo.h
----------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "../maths/Vec4f.h"
#include "../maths/vec2.h"
#include "../maths/LineSegment4f.h"
#include "../utils/Reference.h"
#include <vector>

struct GLObject;
typedef Reference<GLObject> GLObjectRef;
class OpenGLEngine;


/*=====================================================================
IGizmoDelegate
--------------
Implement this to receive transform events from TransformGizmo and to
supply any policy the gizmo cannot determine itself.
=====================================================================*/
class GizmoDelegateInterface
{
public:
	// Called each mouseMoved tick during a translation drag.
	// total_translation is the total translation since the axis was grabbed.
	// desired_new_ob_pos is the new world-space origin of the edited object.
	virtual void onTranslationDrag(const Vec4f& total_translation, const Vec4f& desired_new_ob_pos) = 0;

	// Called each mouseMoved tick during a rotation drag.
	// total_angle_change is the total angle change since the arc was grabbed.
	// delta_angle is the angle change since onRotationDrag was last called.
	// axis is a world-space unit vector; delta_angle is a signed radian increment.
	virtual void onRotationDrag(const Vec4f& axis, float total_angle_change, float delta_angle) = 0;

	// Called when the user starts a drag (mouse down on an arrow/arc).
	// is_rotation: true if grabbing a rotation arc, false if a translation arrow.
	virtual void onGrabStart(bool is_rotation) = 0;

	// Called when the user releases the mouse after a drag.
	virtual void onGrabEnd() = 0;

	virtual ~GizmoDelegateInterface() {}
};


/*=====================================================================
TransformGizmo
--------------
Renders and handles interaction for a 3-axis translation + 3-arc + 3-plane translation
rotation gizmo. Owns the OpenGL objects for the arrows and arcs.

Usage:
  Call update() each frame while enabled to reposition the handles.
  Forward mouse events; each returns true when the gizmo consumed them.
=====================================================================*/
class TransformGizmo : public ThreadSafeRefCounted
{
public:
	TransformGizmo(OpenGLEngine* engine, const Vec4f& gizmo_centre);
	~TransformGizmo();

	// Reposition the arrows and arcs based on the object origin and camera.
	// Call each frame while enabled.
	void update(const Vec4f& ob_pos_ws);

	// Forward mouse events.  Each returns true when the gizmo consumed the event.
	// ob_pos_ws: current world-space origin of the thing being edited.
	// grid_spacing: snap increment for translation (0 = no snap).
	bool mousePressed(const Vec2f& px, const Vec4f& ob_pos_ws, GizmoDelegateInterface* delegate);
	bool mouseMoved(const Vec2f& px, const Vec4f& ob_pos_ws, GizmoDelegateInterface* delegate, float grid_spacing = 0.f);
	bool mouseReleased(GizmoDelegateInterface* delegate);

	// Highlight the arrow/arc under the cursor (call from mouseMoved when not dragging).
	void updateMouseoverHighlight(const Vec2f& px);

	bool isGrabbed() const;

private:
	void updateGizmoDrawTransform(const Vec4f& new_gizmo_centre);

	SSE_CLASS_ALIGN MouseOverResult
	{
	public:
		enum MouseOver_type
		{
			MouseOver_none,
			MouseOver_axis,
			MouseOver_rot_handle,
			MouseOver_plane_quad
		};
		MouseOver_type type;
		int axis_index;
		Vec4f closest_p_ws;
	};
	// Returns the index of the closest axis arrow, or the axis index of the closest rotation arc handle,
	// or the index of the plane translation quad.
	// Also returns world space coords of the closest point.
	MouseOverResult mouseOverAxisArrowOrRotArcOrPlaneQuad(const Vec2f& px);

	// Projection helpers (use GizmoViewState instead of a live camera object).
	static bool worldToPixel(const Vec4f& ws_pos, OpenGLEngine* engine, Vec2f& px_out);
	Vec4f pointOnLineWorldSpace(const Vec4f& p_a_ws, const Vec4f& p_b_ws, const Vec2f& px) const;

	OpenGLEngine* engine;

	static const int NUM_AXIS_ARROWS = 3;
	LineSegment4f  axis_arrow_segments[NUM_AXIS_ARROWS];
	GLObjectRef    axis_arrow_objects[NUM_AXIS_ARROWS];
	std::vector<LineSegment4f> rot_handle_lines[3];
	GLObjectRef    rot_handle_arc_objects[3];
	GLObjectRef    plane_quad_objects[3];

	Vec4f cur_gizmo_centre;

	MouseOverResult::MouseOver_type grabbed_type;
	int   grabbed_axis;             // -1 = none, [0,3) = translation, [3,6) = rotation
	Vec4f grabbed_point_ws;
	Vec4f ob_origin_at_grab;
	float grabbed_angle;
	float original_grabbed_angle;
	float grabbed_arc_angle_offset;

	Vec4f quad_start[3];
	float quad_w;
};
