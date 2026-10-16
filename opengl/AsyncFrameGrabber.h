/*=====================================================================
AsyncFrameGrabber.h
-------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "FrameBuffer.h"
#include "OpenGLTexture.h"
#include "PBO.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Vector.h"
#include <vector>
class OpenGLEngine;
struct __GLsync;
typedef struct __GLsync *GLsync;


/*=====================================================================
AsyncFrameGrabber
-----------------
Reads back rendered frames from the GPU without stalling, e.g. for video recording.

startFrameGrab() blits the source framebuffer into a framebuffer of the output size (scaling with bilinear filtering,
and resolving MSAA if needed), then starts an async glReadPixels into a PBO from a small ring of PBOs, with a fence.
pollCompletedFrames() returns grabbed frames, in submission order, once their fences have signalled, typically 1-2 frames later.

Output frames are 8-bit RGB, top row first, tightly packed (row stride = width * 3).
=====================================================================*/
class AsyncFrameGrabber : public RefCounted
{
public:
	AsyncFrameGrabber(OpenGLEngine* opengl_engine);
	~AsyncFrameGrabber();

	struct GrabbedFrame
	{
		js::Vector<uint8, 16> data; // RGB, top row first
		int width, height;
		double frame_time; // As passed to startFrameGrab()
	};

	// Start grabbing the colour buffer of src_framebuffer_name (0 for the default framebuffer), scaled to dest_w x dest_h.
	// Call after the frame has been drawn.  Returns false if all PBOs are still in flight, in which case the frame is skipped.
	// Restores the framebuffer bindings on return.
	bool startFrameGrab(GLuint src_framebuffer_name, int src_w, int src_h, int dest_w, int dest_h, double frame_time);

	// Call once per frame.  Appends any completed grabs to frames_out, in submission order.  Doesn't block.
	void pollCompletedFrames(std::vector<GrabbedFrame>& frames_out);

	// Blocks until all in-flight grabs have completed, and appends them to frames_out.  For use when stopping recording.
	// Not supported in WebGL, which doesn't allow glClientWaitSync with a non-zero timeout.
	void waitForAllFrames(std::vector<GrabbedFrame>& frames_out);

	int numGrabsInFlight() const { return num_in_flight; }

private:
	GLARE_DISABLE_COPY(AsyncFrameGrabber)

	void reallocBuffers(int dest_w, int dest_h);
	void retrieveOldestFrame(GrabbedFrame& frame_out);

	struct Slot
	{
		PBORef pbo;
		GLsync fence;
		uint64 submit_frame_num;
		double frame_time;
	};

	static const int NUM_SLOTS = 4;
	Slot grab_slots[NUM_SLOTS]; // Note: can't be called 'slots', as Qt defines that as a macro.
	int next_submit_i; // Slot index the next grab will use.
	int num_in_flight; // Number of grabs submitted but not yet retrieved.  The oldest is at (next_submit_i - num_in_flight) mod NUM_SLOTS.
	uint64 frame_num; // Incremented by pollCompletedFrames().

	OpenGLEngine* opengl_engine;

	int dest_w, dest_h;
	OpenGLTextureRef scaled_tex;
	FrameBufferRef scaled_framebuffer;

	// For resolving a multisampled source framebuffer before scaling.  Allocated if needed.
	OpenGLTextureRef resolve_tex;
	FrameBufferRef resolve_framebuffer;
};


typedef Reference<AsyncFrameGrabber> AsyncFrameGrabberRef;
