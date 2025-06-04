/*=====================================================================
SSAODebugging.h
---------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "FrameBuffer.h"
#include "OpenGLTexture.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../maths/Matrix4f.h"
#include "../maths/vec2.h"
#include "../maths/vec3.h"
class OpenGLShader;
class OpenGLEngine;




/*=====================================================================
SSAODebugging
-------------

=====================================================================*/
class SSAODebugging : public RefCounted
{
public:
	class DepthQuerier
	{
	public:
		virtual float depthForPosSS(const Vec2f& pos_ss) = 0; // returns positive depth
		//virtual Vec3f positionForPosSS(const Vec2f& pos_ss);
		virtual Vec3f normalForPosSS(const Vec2f& pos_ss) = 0;
	};

	float computeReferenceAO(OpenGLEngine& gl_engine, DepthQuerier& depth_querier);
	void addDebugObjects(OpenGLEngine& gl_engine);
	void drawSectors(OpenGLEngine& gl_engine, Vec3f p, uint32 bits_changed, Vec3f sampling_plane_n, Vec3f projected_n);
	void drawSamplingPlane(OpenGLEngine& gl_engine, Vec3f p, Vec3f sampling_plane_n, Vec3f projected_n);
	void drawPoint(OpenGLEngine& gl_engine, Vec3f p_cs, const Colour4f& col);

	static void test();
};

