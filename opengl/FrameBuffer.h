/*=====================================================================
FrameBuffer.h
-------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "OpenGLTexture.h"
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

	// attachment_point is GL_DEPTH_ATTACHMENT, GL_COLOR_ATTACHMENT0 etc..
	void attachTexture(OpenGLTexture& tex, GLenum attachment_point);
	void detachTexture(OpenGLTexture& tex, GLenum attachment_point); // detach the attached texture

	void attachRenderBuffer(RenderBuffer& render_buffer, GLenum attachment_point);

	// NOTE: Framebuffer must be bound before calling this.
	GLuint getAttachedRenderBufferName(GLenum attachment_point);

	// NOTE: Framebuffer must be bound before calling this.
	GLuint getAttachedTextureName(GLenum attachment_point);
	
	GLenum checkCompletenessStatus();
	bool isComplete();

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
