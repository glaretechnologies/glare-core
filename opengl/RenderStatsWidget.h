/*=====================================================================
RenderStatsWidget.h
-------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include <opengl/ui/GLUI.h>
#include <opengl/ui/GLUIButton.h>
#include <opengl/ui/GLUITextButton.h>
#include <opengl/ui/GLUICallbackHandler.h>
#include <opengl/ui/GLUITextView.h>
#include <opengl/ui/GLUIInertWidget.h>
#include <utils/ThreadSafeRefCounted.h>
#include <vector>


class GUIClient;


/*=====================================================================
RenderStatsWidget
-----------------
A dynamic bar graph of frame times or CPU usage time per frame.
=====================================================================*/
class RenderStatsWidget : public GLUICallbackHandler, public ThreadSafeRefCounted
{
public:
	RenderStatsWidget(Reference<OpenGLEngine>& opengl_engine_, GLUIRef gl_ui_);
	~RenderStatsWidget();

	void setVisible(bool visible);
	void think();
	void viewportResized(int w, int h);

	virtual void eventOccurred(GLUICallbackEvent& event) override; // From GLUICallbackHandler

	void addFrameTime(float frame_time_s);
private:
	void updateWidgetPositions();

	GLUIRef gl_ui;
	Reference<OpenGLEngine> opengl_engine;

	std::vector<OverlayObjectRef> bars;

	CircularBuffer<float> frame_times;

	GLUIInertWidgetRef background_widget;

	std::vector<OverlayObjectRef> horizontal_rules;
};
