/*=====================================================================
FrameBuffer.h
-------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "OpenGLTexture.h"
#include "../graphics/colour3.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/ArrayRef.h"
class OpenGLShader;
class QGLContext;
class RenderBuffer;


// #define CHECK_GL_CONTEXT 1


/*=====================================================================
FrameBuffer
-----------

=====================================================================*/
class FrameBuffer : public RefCounted
{
public:
	FrameBuffer();
	explicit FrameBuffer(GLuint buffer_name); // Does not take ownership of framebuffer.
	~FrameBuffer();

	void bindForReading();
	void bindForDrawing();

	static void unbind();
	static void unbindFromDrawing();

	// attachment_point is GL_DEPTH_ATTACHMENT, GL_COLOR_ATTACHMENT0 etc..
	void attachTexture(OpenGLTexture& tex, GLenum attachment_point);  // Unbinds the framebuffer at end of function.
	void attachTextures(
		OpenGLTexture& tex_0, GLenum attachment_point_0,
		OpenGLTexture& tex_1, GLenum attachment_point_1);  // Unbinds the framebuffer at end of function.
	void attachTextures(
		OpenGLTexture& tex_0, GLenum attachment_point_0,
		OpenGLTexture& tex_1, GLenum attachment_point_1,
		OpenGLTexture& tex_2, GLenum attachment_point_2); // Unbinds the framebuffer at end of function.

	void detachTexture(OpenGLTexture& tex, GLenum attachment_point); // detach the attached texture.  Unbinds the framebuffer at end of function.

	void attachRenderBuffer(RenderBuffer& render_buffer, GLenum attachment_point); // Unbinds the framebuffer at end of function.
	void attachRenderBuffers(
		RenderBuffer& render_buffer_0, GLenum attachment_point_0,
		RenderBuffer& render_buffer_1, GLenum attachment_point_1); // Unbinds the framebuffer at end of function.
	void attachRenderBuffers(
		RenderBuffer& render_buffer_0, GLenum attachment_point_0,
		RenderBuffer& render_buffer_1, GLenum attachment_point_1,
		RenderBuffer& render_buffer_2, GLenum attachment_point_2); // Unbinds the framebuffer at end of function.

	void attachRenderBufferAndBindForDrawing(RenderBuffer& render_buffer, GLenum attachment_point);
	void attachRenderBuffersAndBindForDrawing(
		RenderBuffer& render_buffer_0, GLenum attachment_point_0, 
		RenderBuffer& render_buffer_1, GLenum attachment_point_1);

	GLuint getAttachedRenderBufferName(GLenum attachment_point);

	GLuint getAttachedTextureName(GLenum attachment_point);



	void setSingleDrawBuffer(GLenum buffer); // NOTE: requires that this frame buffer is bound already.
	void setTwoDrawBuffers(GLenum buffer_0, GLenum buffer_1); // NOTE: requires that this frame buffer is bound already.


	// draw_buffer is the index into the colour buffers bound with setSingleDrawBuffer() or setTwoDrawBuffers().
	void clearFloatColourBuffer(int draw_buffer, const Colour3f& rgb, float alpha); // NOTE: requires that this frame buffer is bound already.

	static void clearCurrentlyBoundFloatColourBuffer(int draw_buffer, const Colour3f& rgb, float alpha); // Applies to the currently bound framebuffer. (which may be framebuffer 0, i.e. not correspond to a FrameBuffer object)

	// draw_buffer is the index into the colour buffers bound with setSingleDrawBuffer() or setTwoDrawBuffers().
	void clearUIntColourBuffer(int draw_buffer, GLuint r, GLuint g, GLuint b, GLuint a); // NOTE: requires that this frame buffer is bound already.

	static void clearCurrentlyBoundDepthBuffer(float depth); // Applies to the currently bound framebuffer. (which may be framebuffer 0, i.e. not correspond to a FrameBuffer object)


	static GLuint getCurrentlyBoundDrawFrameBuffer();
	
	GLenum checkCompletenessStatus(); // Returns GL_FRAMEBUFFER_COMPLETE or some other OpenGL enum.  Does not modify bound framebuffer. (e.g. restores old binding at end of function)
	bool isComplete(); // Does not modify bound framebuffer. (e.g. restores old binding at end of function)

	void discardContents(ArrayRef<GLenum> attachments);
	void discardContents(GLenum attachment_a);
	void discardContents(GLenum attachment_a, GLenum attachment_b);
	void discardContents(GLenum attachment_a, GLenum attachment_b, GLenum attachment_c);

	static void discardDefaultFrameBufferContents(); // Discards colour and depth from default framebuffer.

	// Will return 0 if texture has not been bound yet.
	size_t xRes() const { return xres; }
	size_t yRes() const { return yres; }
private:
	GLARE_DISABLE_COPY(FrameBuffer);
public:
	
	GLuint buffer_name;
	size_t xres, yres; // Will be set after bindTextureAsTarget() is called, and 0 beforehand.
	bool own_buffer; // If true, will call glDeleteFramebuffers on destruction.

#if CHECK_GL_CONTEXT
	const QGLContext* context;
#endif
};


typedef Reference<FrameBuffer> FrameBufferRef;


// Blit the entire contents of src_framebuffer to dest_framebuffer.
// num_buffers_to_copy can be 1 or 2.
// copy_buf0_colour: copy the colour buffer of buffer 0.
// copy_buf0_depth: copy the depth buffer of buffer 0.
// 
// NOTE: leaves dest_framebuffer bound for drawing upon completion.
void blitFrameBuffer(FrameBuffer& src_framebuffer, FrameBuffer& dest_framebuffer, int num_buffers_to_copy, bool copy_buf0_colour, bool copy_buf0_depth);
