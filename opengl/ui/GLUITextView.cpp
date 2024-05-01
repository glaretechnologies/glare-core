/*=====================================================================
GLUITextView.cpp
----------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "GLUITextView.h"


#include "GLUI.h"
#include "TextEditingUtils.h"
#include "../graphics/SRGBUtils.h"
#include "../OpenGLMeshRenderData.h"
#include "../utils/FileUtils.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/UTF8Utils.h"


GLUITextView::GLUITextView(GLUI& glui_, Reference<OpenGLEngine>& opengl_engine_, const std::string& text_, const Vec2f& botleft_, const GLUITextViewCreateArgs& args_)
{
	glui = &glui_;
	args = args_;
	opengl_engine = opengl_engine_;
	tooltip = args.tooltip;

	selection_start = selection_end = -1;
	text = text_;
	botleft = botleft_;

	background_overlay_ob = new OverlayObject();
	background_overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	background_overlay_ob->material.albedo_linear_rgb = args.background_colour;
	background_overlay_ob->material.alpha = args.background_alpha;

	selection_overlay_ob = new OverlayObject();
	selection_overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	selection_overlay_ob->material.albedo_linear_rgb = Colour3f(1,1,1);
	selection_overlay_ob->material.alpha = 0.5f;
	selection_overlay_ob->draw = false;

	opengl_engine->addOverlayObject(selection_overlay_ob);

	GLUIText::GLUITextCreateArgs text_create_args;
	text_create_args.colour = args.text_colour;
	text_create_args.font_size_px = args.font_size_px;
	text_create_args.alpha = args.text_alpha;
	glui_text = new GLUIText(glui_, opengl_engine_, text, botleft, text_create_args);

	rect = glui_text->getRect();

	updateOverlayObTransforms();

	opengl_engine->addOverlayObject(background_overlay_ob);
}


GLUITextView::~GLUITextView()
{
	destroy();
}


void GLUITextView::destroy()
{
	if(glui_text.nonNull())
		glui_text->destroy();
	glui_text = NULL;

	if(background_overlay_ob.nonNull())
		opengl_engine->removeOverlayObject(background_overlay_ob);
	background_overlay_ob = NULL;

	if(selection_overlay_ob.nonNull())
		opengl_engine->removeOverlayObject(selection_overlay_ob);
	selection_overlay_ob = NULL;

	opengl_engine = NULL;
}


void GLUITextView::setText(GLUI& glui_, const std::string& new_text)
{
	if(new_text != text)
	{
		text = new_text;

		// Re-create glui_text
		if(glui_text.nonNull())
			glui_text->destroy();

		GLUIText::GLUITextCreateArgs text_create_args;
		text_create_args.font_size_px = args.font_size_px;
		text_create_args.alpha = args.text_alpha;
		glui_text = new GLUIText(glui_, opengl_engine, text, botleft, text_create_args);

		updateOverlayObTransforms();
	}
}


void GLUITextView::updateOverlayObTransforms()
{
	if(background_overlay_ob.nonNull())
	{
		const float margin_x = glui->getUIWidthForDevIndepPixelWidth((float)args.padding_px);
		const float margin_y = margin_x;
		const float background_w = glui_text->getDims().x + margin_x*2;
		const float background_h = glui_text->getDims().y + margin_y*2;

		const Vec2f text_lower_left_pos = glui_text->getRect().getMin();
		const Vec2f lower_left_pos = text_lower_left_pos - Vec2f(margin_x, margin_y);

		const float y_scale = opengl_engine->getViewPortAspectRatio(); // scale from GL UI to opengl coords
		const float z = 0.1f;
		background_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(lower_left_pos.x, lower_left_pos.y * y_scale, z) * Matrix4f::scaleMatrix(background_w, background_h * y_scale, 1);
	}

	// Update selection ob
	if(selection_overlay_ob.nonNull())
	{
		const float extra_half_h_factor = 0.2f; // Make the cursor a bit bigger than the font size
		const float h = (float)glui->getUIWidthForDevIndepPixelWidth((float)args.font_size_px * (1 + extra_half_h_factor*2));

		if(selection_start != -1 && selection_end != -1)
		{
			Vec2f selection_lower_left_pos  = glui_text->getCharPos(*glui, selection_start);
			Vec2f selection_upper_right_pos = glui_text->getCharPos(*glui, selection_end) + Vec2f(0, h);

			if(selection_lower_left_pos.x > selection_upper_right_pos.x)
				mySwap(selection_lower_left_pos, selection_upper_right_pos);

			selection_lower_left_pos.y  = myMin(selection_lower_left_pos.y, selection_upper_right_pos.y);
			selection_upper_right_pos.y = myMax(selection_lower_left_pos.y, selection_upper_right_pos.y);

			const float w = std::fabs(selection_upper_right_pos.x - selection_lower_left_pos.x);

			try
			{
				const float y_scale = opengl_engine->getViewPortAspectRatio(); // scale from GL UI to opengl coords
				const float z = -0.1f;
				selection_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(selection_lower_left_pos.x, (selection_lower_left_pos.y - h * extra_half_h_factor) * y_scale, z) * Matrix4f::scaleMatrix(w, h * y_scale, 1);

				selection_overlay_ob->draw = true;
			}
			catch(glare::Exception& e)
			{
				conPrint("Excep: " + e.what());
			}
		}
		else
			selection_overlay_ob->draw = false;
	}
}


void GLUITextView::setColour(const Colour3f& col)
{
	args.text_colour = col;
	
	if(glui_text.nonNull() && glui_text->overlay_ob.nonNull())
		glui_text->overlay_ob->material.albedo_linear_rgb = col;
}


void GLUITextView::updateGLTransform(GLUI& /*glui_*/)
{
	if(glui_text.nonNull())
		glui_text->updateGLTransform();

	updateOverlayObTransforms();
}


void GLUITextView::handleCutEvent(std::string& /*clipboard_contents_out*/)
{
	// Since GLUITextView is a read-only widget, we can't cut. So do nothiong.
}


void GLUITextView::handleCopyEvent(std::string& clipboard_contents_out)
{
	if(selection_start != -1 && selection_end != -1)
	{
		if(selection_start > selection_end)
			mySwap(selection_start, selection_end);

		const size_t start_byte_index = TextEditingUtils::getCursorByteIndex(text, selection_start);
		const size_t end_byte_index   = TextEditingUtils::getCursorByteIndex(text, selection_end);
		
		clipboard_contents_out = text.substr(start_byte_index, end_byte_index - start_byte_index);
	}
}


void GLUITextView::setPos(GLUI& /*glui_*/, const Vec2f& new_botleft)
{
	botleft = new_botleft;

	if(glui_text.nonNull())
		glui_text->setPos(botleft);

	updateOverlayObTransforms();
}


void GLUITextView::setVisible(bool visible)
{
	if(glui_text.nonNull() && glui_text->overlay_ob.nonNull())
	{
		if(glui_text->overlay_ob->draw != visible) // Avoid write if hasn't changed
			glui_text->overlay_ob->draw = visible;
	}

	if(background_overlay_ob.nonNull())
	{
		if(background_overlay_ob->draw != visible) // Avoid write if hasn't changed
			background_overlay_ob->draw = visible;
	}
}


const Vec2f GLUITextView::getDims() const
{
	if(glui_text.nonNull())
		return glui_text->getDims();
	else
		return Vec2f(0.f);
}


void GLUITextView::handleMousePress(MouseEvent& event)
{
	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
	{
		//conPrint("GLUILineEdit taking keyboard focus");
		glui->setKeyboardFocusWidget(this);

		this->selection_start = glui_text->cursorPosForUICoords(*glui, coords);
		this->selection_end = -1;

		updateOverlayObTransforms(); // Redraw since cursor visible will have changed

		event.accepted = true;
	}
}


void GLUITextView::handleMouseRelease(MouseEvent& /*event*/)
{
}


void GLUITextView::handleMouseDoubleClick(MouseEvent& event)
{
	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
	{
		const int click_cursor_pos = glui_text->cursorPosForUICoords(*glui, coords);

		this->selection_start = TextEditingUtils::getPrevStartOfNonWhitespaceCursorPos(text, click_cursor_pos);

		this->selection_end = TextEditingUtils::getNextEndOfNonWhitespaceCursorPos(text, click_cursor_pos);

		updateOverlayObTransforms(); // Redraw since selection region has changed.
	}
}


void GLUITextView::doHandleMouseMoved(MouseEvent& mouse_event)
{
	const Vec2f coords = glui->UICoordsForOpenGLCoords(mouse_event.gl_coords);

	// If left mouse button is down, update selection region
	if((mouse_event.button_state & (uint32)MouseButton::Left) != 0)
	{
		this->selection_end = glui_text->cursorPosForUICoords(*glui, coords);

		updateOverlayObTransforms(); // Redraw since selection region may have changed.
	}
}


void GLUITextView::handleLosingKeyboardFocus()
{
	// Clear selection
	this->selection_start = this->selection_end = -1;
	updateOverlayObTransforms(); // Redraw
}


GLUITextView::GLUITextViewCreateArgs::GLUITextViewCreateArgs():
	background_colour(0.f),
	background_alpha(1),
	text_colour(1.f),
	text_alpha(1.f),
	padding_px(10),
	font_size_px(14)
{}
