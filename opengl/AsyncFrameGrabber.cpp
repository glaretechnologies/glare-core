/*=====================================================================
AsyncFrameGrabber.cpp
---------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "AsyncFrameGrabber.h"


#include "IncludeOpenGL.h"
#include "OpenGLEngine.h"
#include "../utils/RuntimeCheck.h"
#include "../utils/Exception.h"
#include <tracy/Tracy.hpp>
#include <cstring>


AsyncFrameGrabber::AsyncFrameGrabber(OpenGLEngine* opengl_engine_)
:	opengl_engine(opengl_engine_),
	next_submit_i(0),
	num_in_flight(0),
	frame_num(0),
	dest_w(0),
	dest_h(0)
{
	for(int i=0; i<NUM_SLOTS; ++i)
	{
		grab_slots[i].fence = nullptr;
		grab_slots[i].submit_frame_num = 0;
		grab_slots[i].frame_time = 0;
	}
}


AsyncFrameGrabber::~AsyncFrameGrabber()
{
	for(int i=0; i<NUM_SLOTS; ++i)
		if(grab_slots[i].fence)
			glDeleteSync(grab_slots[i].fence);
}


// Discards any in-flight grabs.
void AsyncFrameGrabber::reallocBuffers(int new_dest_w, int new_dest_h)
{
	for(int i=0; i<NUM_SLOTS; ++i)
	{
		if(grab_slots[i].fence)
		{
			glDeleteSync(grab_slots[i].fence);
			grab_slots[i].fence = nullptr;
		}
		grab_slots[i].pbo = new PBO((size_t)new_dest_w * new_dest_h * 3, /*for_upload=*/false);
	}
	next_submit_i = 0;
	num_in_flight = 0;

	scaled_tex = new OpenGLTexture(new_dest_w, new_dest_h, opengl_engine, ArrayRef<uint8>(), OpenGLTextureFormat::Format_RGBA_Linear_Uint8,
		OpenGLTexture::Filtering_Nearest, OpenGLTexture::Wrapping_Clamp, /*has_mipmaps=*/false, /*MSAA_samples=*/1);
	scaled_framebuffer = new FrameBuffer();
	scaled_framebuffer->attachTexture(*scaled_tex, GL_COLOR_ATTACHMENT0);
	assert(scaled_framebuffer->isComplete());

	dest_w = new_dest_w;
	dest_h = new_dest_h;
}


bool AsyncFrameGrabber::startFrameGrab(GLuint src_framebuffer_name, int src_w, int src_h, int new_dest_w, int new_dest_h, double frame_time)
{
	ZoneScoped; // Tracy profiler

	runtimeCheck(src_w > 0 && src_h > 0 && new_dest_w > 0 && new_dest_h > 0);

	if(num_in_flight == NUM_SLOTS)
		return false;

	// Save framebuffer bindings so we can restore them at the end.
	GLint old_draw_framebuffer = 0, old_read_framebuffer = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_draw_framebuffer);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_read_framebuffer);

	if(new_dest_w != dest_w || new_dest_h != dest_h)
		reallocBuffers(new_dest_w, new_dest_h);

	// Find out if the source framebuffer is multisampled.  GL_SAMPLES applies to the bound draw framebuffer.
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, src_framebuffer_name);
	GLint src_samples = 0;
	glGetIntegerv(GL_SAMPLES, &src_samples);

	GLuint scale_src_framebuffer_name = src_framebuffer_name;
	if(src_samples > 1)
	{
		// A multisampled framebuffer can't be blitted with scaling, so resolve to a same-size framebuffer first.
		if(resolve_tex.isNull() || (int)resolve_tex->xRes() != src_w || (int)resolve_tex->yRes() != src_h)
		{
			resolve_tex = new OpenGLTexture(src_w, src_h, opengl_engine, ArrayRef<uint8>(), OpenGLTextureFormat::Format_RGBA_Linear_Uint8,
				OpenGLTexture::Filtering_Nearest, OpenGLTexture::Wrapping_Clamp, /*has_mipmaps=*/false, /*MSAA_samples=*/1);
			resolve_framebuffer = new FrameBuffer();
			resolve_framebuffer->attachTexture(*resolve_tex, GL_COLOR_ATTACHMENT0);
			assert(resolve_framebuffer->isComplete());
		}

		glBindFramebuffer(GL_READ_FRAMEBUFFER, src_framebuffer_name);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolve_framebuffer->buffer_name);
		glBlitFramebuffer(0, 0, src_w, src_h, 0, 0, src_w, src_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		scale_src_framebuffer_name = resolve_framebuffer->buffer_name;
	}

	// Blit to scaled_framebuffer, scaling with bilinear filtering.
	glBindFramebuffer(GL_READ_FRAMEBUFFER, scale_src_framebuffer_name);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, scaled_framebuffer->buffer_name);
	glBlitFramebuffer(0, 0, src_w, src_h, 0, 0, dest_w, dest_h, GL_COLOR_BUFFER_BIT, GL_LINEAR);

	// Start async read of scaled_framebuffer into the slot's PBO.
	Slot& slot = grab_slots[next_submit_i];

	glBindFramebuffer(GL_READ_FRAMEBUFFER, scaled_framebuffer->buffer_name);
	glReadBuffer(GL_COLOR_ATTACHMENT0);

	GLint old_pack_alignment = 4;
	glGetIntegerv(GL_PACK_ALIGNMENT, &old_pack_alignment);
	glPixelStorei(GL_PACK_ALIGNMENT, 1); // Rows are width * 3 bytes, which may not be a multiple of 4.

	slot.pbo->bind();
	glReadPixels(0, 0, dest_w, dest_h, GL_RGB, GL_UNSIGNED_BYTE, /*offset into PBO=*/nullptr);
	slot.pbo->unbind();

	glPixelStorei(GL_PACK_ALIGNMENT, old_pack_alignment);

	slot.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, /*flags=*/0);
	slot.submit_frame_num = frame_num;
	slot.frame_time = frame_time;

	next_submit_i = (next_submit_i + 1) % NUM_SLOTS;
	num_in_flight++;

	// Restore framebuffer bindings
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)old_draw_framebuffer);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)old_read_framebuffer);

	return true;
}


void AsyncFrameGrabber::retrieveOldestFrame(GrabbedFrame& frame_out)
{
	ZoneScoped; // Tracy profiler

	const int oldest_i = (next_submit_i - num_in_flight + NUM_SLOTS) % NUM_SLOTS;
	Slot& slot = grab_slots[oldest_i];

	glDeleteSync(slot.fence);
	slot.fence = nullptr;

	const size_t row_B = (size_t)dest_w * 3;
	frame_out.width = dest_w;
	frame_out.height = dest_h;
	frame_out.frame_time = slot.frame_time;
	frame_out.data.resizeNoCopy(row_B * dest_h);

	// glReadPixels returns the bottom row first, so flip rows while copying.
	if(PBO::isMapForReadingSupported())
	{
		const uint8* mapped = (const uint8*)slot.pbo->mapForReading();
		runtimeCheck(mapped != nullptr);
		for(int y=0; y<dest_h; ++y)
			std::memcpy(frame_out.data.data() + (size_t)y * row_B, mapped + (size_t)(dest_h - 1 - y) * row_B, row_B);
		slot.pbo->unmap();
	}
	else
	{
		slot.pbo->readData(/*offset=*/0, frame_out.data.data(), row_B * dest_h);

		js::Vector<uint8, 16> temp_row(row_B);
		for(int y=0; y<dest_h/2; ++y)
		{
			uint8* row_a = frame_out.data.data() + (size_t)y * row_B;
			uint8* row_b = frame_out.data.data() + (size_t)(dest_h - 1 - y) * row_B;
			std::memcpy(temp_row.data(), row_a, row_B);
			std::memcpy(row_a, row_b, row_B);
			std::memcpy(row_b, temp_row.data(), row_B);
		}
	}

	num_in_flight--;
}


void AsyncFrameGrabber::pollCompletedFrames(std::vector<GrabbedFrame>& frames_out)
{
	frame_num++;

	while(num_in_flight > 0)
	{
		const int oldest_i = (next_submit_i - num_in_flight + NUM_SLOTS) % NUM_SLOTS;
		Slot& slot = grab_slots[oldest_i];

		// Don't call glClientWaitSync on the fence unless a frame has elapsed, otherwise it may block.  See AsyncGeometryUploader.
		if(slot.submit_frame_num + 1 >= frame_num)
			return;

		const GLenum wait_ret = glClientWaitSync(slot.fence, /*wait flags=*/0, /*timeout=*/0);
		assert(wait_ret != GL_WAIT_FAILED);
		if(!(wait_ret == GL_ALREADY_SIGNALED || wait_ret == GL_CONDITION_SATISFIED))
			return;

		frames_out.emplace_back();
		retrieveOldestFrame(frames_out.back());
	}
}


void AsyncFrameGrabber::waitForAllFrames(std::vector<GrabbedFrame>& frames_out)
{
	while(num_in_flight > 0)
	{
		const int oldest_i = (next_submit_i - num_in_flight + NUM_SLOTS) % NUM_SLOTS;
		Slot& slot = grab_slots[oldest_i];

		while(1)
		{
			const GLenum wait_ret = glClientWaitSync(slot.fence, GL_SYNC_FLUSH_COMMANDS_BIT, /*timeout (ns)=*/100000000);
			if(wait_ret == GL_ALREADY_SIGNALED || wait_ret == GL_CONDITION_SATISFIED)
				break;
			if(wait_ret == GL_WAIT_FAILED)
				throw glare::Exception("AsyncFrameGrabber: glClientWaitSync failed.");
			// else GL_TIMEOUT_EXPIRED: keep waiting.
		}

		frames_out.emplace_back();
		retrieveOldestFrame(frames_out.back());
	}
}
