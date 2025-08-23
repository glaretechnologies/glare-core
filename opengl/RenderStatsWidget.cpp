/*=====================================================================
RenderStatsWidget.cpp
---------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "RenderStatsWidget.h"


RenderStatsWidget::RenderStatsWidget(Reference<OpenGLEngine>& opengl_engine_, GLUIRef gl_ui_)
{
	opengl_engine = opengl_engine_;
	gl_ui = gl_ui_;

	const int num_bars = 100;

	bars.resize(num_bars);
	for(int i=0; i<num_bars; ++i)
	{
		bars[i] = new OverlayObject();
		bars[i]->mesh_data = opengl_engine->getUnitQuadMeshData();
		opengl_engine->addOverlayObject(bars[i]);
	}

	{
		GLUIInertWidget::CreateArgs args;
		args.background_colour = Colour3f(0.1f);
		args.background_alpha = 0.9f;
		args.z = 0.1f;
		background_widget = new GLUIInertWidget(*gl_ui, opengl_engine_, args);
	}

	horizontal_rules.resize(20);
	for(size_t i=0; i<horizontal_rules.size(); ++i)
	{
		horizontal_rules[i] = new OverlayObject();
		horizontal_rules[i]->material.albedo_linear_rgb = Colour3f(0.5f);
		horizontal_rules[i]->mesh_data = opengl_engine->getUnitQuadMeshData();
		opengl_engine->addOverlayObject(horizontal_rules[i]);
	}

	updateWidgetPositions();
}


RenderStatsWidget::~RenderStatsWidget()
{
	for(size_t i=0; i<bars.size(); ++i)
		opengl_engine->removeOverlayObject(bars[i]);
	
	for(size_t i=0; i<horizontal_rules.size(); ++i)
		opengl_engine->removeOverlayObject(horizontal_rules[i]);

	checkRemoveAndDeleteWidget(gl_ui, background_widget);
}


void RenderStatsWidget::setVisible(bool visible)
{
	
}


void RenderStatsWidget::think()
{
	if(gl_ui.nonNull())
	{
	}
}


void RenderStatsWidget::updateWidgetPositions()
{
	if(gl_ui)
	{
		const float min_max_y = gl_ui->getViewportMinMaxY();
		const float y_scale = 1.f / min_max_y; // UI to opengl y coord scale

		const float graph_h = gl_ui->getUIWidthForDevIndepPixelWidth(200);
		const float bar_w = gl_ui->getUIWidthForDevIndepPixelWidth(2);
		const float graph_w = bar_w * (float)bars.size();
		const float max_displayed_frame_time = 0.020f; // 20 ms
		const float bar_h_per_s = graph_h / max_displayed_frame_time;

		auto it = frame_times.beginIt();
		for(size_t i=0; i<bars.size(); ++i)
		{
			float frame_time = 0;
			if(it != frame_times.endIt())
			{
				frame_time = *it;
				++it;
			}

			// Set bar transform
			bars[i]->ob_to_world_matrix = translationMulScaleMatrix(/*translation=*/Vec4f(-0.9f + bar_w * i, (min_max_y - 0.1f - graph_h) * y_scale, 0.f, 0.f), bar_w, frame_time * bar_h_per_s * y_scale, 1.f);

			// Set bar colour
			Colour3f col;
			if(frame_time < 1 / 144.f)
				col = Colour3f(0,1,0);
			else if(frame_time < 2 / 144.f)
				col = Colour3f(1,0.5,0.1);
			else
				col = Colour3f(1,0,0);
			bars[i]->material.albedo_linear_rgb = col;
		}

		// Set horizontal rule positions
		const float rule_h = gl_ui->getUIWidthForDevIndepPixelWidth(1);
		for(size_t i=0; i<horizontal_rules.size(); ++i)
		{
			horizontal_rules[i]->ob_to_world_matrix = translationMulScaleMatrix(/*translation=*/Vec4f(-0.9f, (min_max_y - 0.1f - graph_h + bar_h_per_s * 0.001f * i) * y_scale, /*z=*/0.05f, 0.f), /*scale x=*/graph_w, /*scale_y*/rule_h*y_scale, 1.f);
		}

		background_widget->setPosAndDims(/*botleft=*/Vec2f(-0.9f, min_max_y - 0.1f - graph_h), Vec2f(graph_w, graph_h));
	}
}


void RenderStatsWidget::viewportResized(int w, int h)
{
	if(gl_ui)
		updateWidgetPositions();
}


void RenderStatsWidget::eventOccurred(GLUICallbackEvent& event)
{
	
}


void RenderStatsWidget::addFrameTime(float frame_time_s)
{
	frame_times.push_back(frame_time_s);
	if(frame_times.size() > bars.size())
		frame_times.pop_front();

	updateWidgetPositions();
}
