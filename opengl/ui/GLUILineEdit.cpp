/*=====================================================================
GLUILineEdit.cpp
----------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "GLUILineEdit.h"


#include "GLUI.h"
#include "../OpenGLMeshRenderData.h"
#include "../MeshPrimitiveBuilding.h"
#include "../../graphics/SRGBUtils.h"
#include "../../utils/Exception.h"
#include "../../utils/ConPrint.h"
#include "../../utils/StringUtils.h"
#include "../../utils/UTF8Utils.h"


GLUILineEdit::GLUILineEdit()
{
}


GLUILineEdit::~GLUILineEdit()
{
	destroy();
}


void GLUILineEdit::create(GLUI& glui_, Reference<OpenGLEngine>& opengl_engine_, const Vec2f& botleft_, 
	const GLUILineEditCreateArgs& args_)
{
	glui = &glui_;

	args = args_;
	opengl_engine = opengl_engine_;
	tooltip = args.tooltip;

	botleft = botleft_;

	cursor_pos = 0;
	last_cursor_update_time = 0;

	this->height_px = (float)args.font_size_px + (float)args.padding_px*2;
	
	// Create background quad to go behind text
	background_overlay_ob = new OverlayObject();
	background_overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData(); // Dummy, to be replaced in updateOverlayObTransforms().
	background_overlay_ob->material.albedo_linear_rgb = args.background_colour;
	background_overlay_ob->material.alpha = args.background_alpha;
	background_overlay_ob->ob_to_world_matrix = Matrix4f::identity(); // Dummy data, to be replaced in updateOverlayObTransforms().
	opengl_engine->addOverlayObject(background_overlay_ob);
	
	this->last_viewport_dims = Vec2i(0); // Dummy

	recreateTextWidget();

	cursor_overlay_ob = new OverlayObject();
	cursor_overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	cursor_overlay_ob->material.albedo_linear_rgb = Colour3f(1,1,1);
	cursor_overlay_ob->material.alpha = 1;

	opengl_engine->addOverlayObject(cursor_overlay_ob);

	updateOverlayObTransforms();
}


void GLUILineEdit::destroy()
{
	if(glui_text.nonNull())
		glui_text->destroy();
	glui_text = NULL;

	if(background_overlay_ob.nonNull())
		opengl_engine->removeOverlayObject(background_overlay_ob);
	background_overlay_ob = NULL;
	
	if(cursor_overlay_ob.nonNull())
		opengl_engine->removeOverlayObject(cursor_overlay_ob);
	cursor_overlay_ob = NULL;

	opengl_engine = NULL;
}


void GLUILineEdit::think(GLUI& /*glui_*/)
{
	if(cursor_overlay_ob.nonNull())
	{
		// If this widget has keyboard focus, show blinking cursor, otherwise hide it.
		if(glui->getKeyboardFocusWidget().ptr() == this)
		{
			// If the cursor updated recently we want to show it.
			cursor_overlay_ob->draw = Maths::fract((Clock::getCurTimeRealSec() - last_cursor_update_time) / 1.06) < 0.5;
		}
		else
		{
			cursor_overlay_ob->draw = false;
		}
	}
}


void GLUILineEdit::setText(GLUI& /*glui_*/, const std::string& new_text)
{
	if(new_text != text)
	{
		recreateTextWidget();
		updateOverlayObTransforms();
	}
}


const std::string& GLUILineEdit::getText() const
{
	return text;
}


void GLUILineEdit::clear()
{
	text.clear();

	cursor_pos = 0;

	recreateTextWidget();
	updateOverlayObTransforms();
}


void GLUILineEdit::updateOverlayObTransforms()
{
	if(background_overlay_ob.nonNull())
	{
		const float background_w = args.width;
		const float background_h = glui->getUIWidthForDevIndepPixelWidth(this->height_px);

		if(this->last_viewport_dims != opengl_engine->getViewportDims())
		{
			// Viewport has changed, recreate rounded-corner rect.
			conPrint("GLUILineEdit: Viewport has changed, recreate rounded-corner rect.");
			
			background_overlay_ob->mesh_data = MeshPrimitiveBuilding::makeRoundedCornerRect(*opengl_engine->vert_buf_allocator, /*i=*/Vec4f(1,0,0,0), /*j=*/Vec4f(0,1,0,0), /*w=*/background_w, /*h=*/background_h, 
				/*corner radius=*/glui->getUIWidthForDevIndepPixelWidth(args.rounded_corner_radius_px), /*tris_per_corner=*/8);

			this->last_viewport_dims = opengl_engine->getViewportDims();
		}

		const float y_scale = opengl_engine->getViewPortAspectRatio(); // scale from GL UI to opengl coords
		const float z = 0.1f;
		background_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, z) * Matrix4f::scaleMatrix(1, y_scale, 1);

		rect = Rect2f(botleft, botleft + Vec2f(args.width, background_h));
	}

	

	// Update cursor ob
	if(cursor_overlay_ob.nonNull())
	{
		const float w = glui->getUIWidthForDevIndepPixelWidth(1);
		const float extra_half_h_factor = 0.2f; // Make the cursor a bit bigger than the font size
		const float h = (float)glui->getUIWidthForDevIndepPixelWidth((float)args.font_size_px * (1 + extra_half_h_factor*2));

		try
		{
			const Vec2f cursor_lower_left_pos = glui_text->getCharPos(*glui, cursor_pos); // Work out position of cursor in text
			const float y_scale = opengl_engine->getViewPortAspectRatio(); // scale from GL UI to opengl coords
			const float z = -0.1f;
			cursor_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(cursor_lower_left_pos.x, (cursor_lower_left_pos.y - h * extra_half_h_factor) * y_scale, z) * Matrix4f::scaleMatrix(w, h * y_scale, 1);
		}
		catch(glare::Exception& e)
		{
			conPrint("Excep: " + e.what());
		}
	}
}


// Re-create glui_text
void GLUILineEdit::recreateTextWidget()
{
	// Destroy existing text, if any
	if(glui_text.nonNull())
		glui_text->destroy();
	glui_text = NULL;

	GLUIText::GLUITextCreateArgs text_create_args;
	text_create_args.colour = args.text_colour;
	text_create_args.font_size_px = args.font_size_px;
	text_create_args.alpha = args.text_alpha;
	glui_text = new GLUIText(*glui, opengl_engine, text, /*botleft=*/Vec2f(0.f), text_create_args);

	// Set clip region so text doesn't draw outside of line edit.
	glui_text->overlay_ob->clip_region = Rect2f(glui->OpenGLCoordsForUICoords(botleft), glui->OpenGLCoordsForUICoords(botleft + Vec2f(args.width, glui->getUIWidthForDevIndepPixelWidth(this->height_px))));

	updateTextTransform();
}


void GLUILineEdit::updateTextTransform()
{
	if(glui_text.nonNull())
	{
		const float margin_x = glui->getUIWidthForDevIndepPixelWidth((float)args.padding_px);
		const float margin_y = margin_x;

		const Vec2f text_botleft = botleft + Vec2f(margin_x, margin_y);

		glui_text->setPos(text_botleft);

		// Set clip region so text doesn't draw outside of line edit.
		glui_text->overlay_ob->clip_region = Rect2f(glui->OpenGLCoordsForUICoords(botleft), glui->OpenGLCoordsForUICoords(botleft + Vec2f(args.width, glui->getUIWidthForDevIndepPixelWidth(this->height_px))));
	}
}


bool GLUILineEdit::doHandleMouseClick(const Vec2f& coords)
{
	//conPrint("GLUILineEdit::doHandleMouseClick, coords: " + coords.toString());

	if(rect.inClosedRectangle(coords))
	{
		//conPrint("GLUILineEdit taking keyboard focus");
		glui->setKeyboardFocusWidget(this);

		// Find where cursor should go
		{
			const std::vector<GLUIText::CharPositionInfo> char_positions = glui_text->getCharPositions(*glui);

			cursor_pos = 0;
			float closest_dist = 1000000;
			for(size_t i=0; i<char_positions.size(); ++i)
			{
				const float char_left_x = char_positions[i].pos.x;
				const float dist = fabs(char_left_x - coords.x);
				if(dist < closest_dist)
				{
					cursor_pos = (int)i;
					closest_dist = dist;
				}

				if(char_positions[i].pos.x < coords.x) // If character i left coord is to the left of the mouse click position
					cursor_pos = (int)i;
			}

			if(!char_positions.empty())
			{
				const float last_char_right_x = char_positions.back().pos.x + char_positions.back().hori_advance;
				const float dist = fabs(last_char_right_x - coords.x);
				if(dist < closest_dist)
				{
					cursor_pos = (int)char_positions.size();
					assert(cursor_pos == UTF8Utils::numCodePointsInString(text));
					closest_dist = dist;
				}
			}

			this->last_cursor_update_time = Clock::getCurTimeRealSec();

			updateOverlayObTransforms(); // Redraw since cursor visible will have changed
		}

		return true;
	}
	return false;
}


bool GLUILineEdit::doHandleMouseMoved(const Vec2f& coords)
{
	if(background_overlay_ob.nonNull())
	{
		if(rect.inOpenRectangle(coords)) // If mouse over widget:
		{
			background_overlay_ob->material.albedo_linear_rgb = args.mouseover_background_colour;
		}
		else
		{
			background_overlay_ob->material.albedo_linear_rgb = args.background_colour;
		}
	}

	return false;
}


static size_t getCursorByteIndex(const std::string& text, int cursor_pos)
{
	if(cursor_pos == UTF8Utils::numCodePointsInString(text)) // If cursor is at end of text:
	{
		return text.size();
	}
	else
	{
		return UTF8Utils::byteIndex((const uint8*)text.data(), text.size(), (size_t)cursor_pos);
	}
}


void GLUILineEdit::doHandleKeyPressedEvent(KeyEvent& key_event)
{
	if(key_event.key == Key::Key_Backspace)
	{
		if(cursor_pos > 0)
		{
			try
			{
				const size_t old_cursor_byte_index = getCursorByteIndex(text, cursor_pos);
				cursor_pos--;
				const size_t new_cursor_byte_index = getCursorByteIndex(text, cursor_pos);

				// Remove bytes from string
				text.erase(/*offset=*/new_cursor_byte_index, /*count=*/old_cursor_byte_index - new_cursor_byte_index);

				this->last_cursor_update_time = Clock::getCurTimeRealSec();
			}
			catch(glare::Exception& e)
			{
				conPrint("excep: " + e.what());
			}
		}
	}
	if(key_event.key == Key::Key_Delete)
	{
		try
		{
			const size_t old_cursor_byte_index  = getCursorByteIndex(text, cursor_pos);
			const size_t next_cursor_byte_index = getCursorByteIndex(text, cursor_pos + 1);

			// Remove bytes from string
			text.erase(/*offset=*/old_cursor_byte_index, /*count=*/next_cursor_byte_index - old_cursor_byte_index);
		}
		catch(glare::Exception& e)
		{
			conPrint("excep: " + e.what());
		}
	}
	else if(key_event.key == Key::Key_Left)
	{
		if(cursor_pos > 0)
			cursor_pos--;

		this->last_cursor_update_time = Clock::getCurTimeRealSec();
	}
	else if(key_event.key == Key::Key_Right)
	{
		if(cursor_pos < (int)UTF8Utils::numCodePointsInString(text))
			cursor_pos++;

		this->last_cursor_update_time = Clock::getCurTimeRealSec();
	}
	else if(key_event.key == Key::Key_Return || key_event.key == Key::Key_Enter)
	{
		if(on_enter_pressed)
			on_enter_pressed();
	}
	else if(key_event.key == Key::Key_Escape)
	{
		glui->setKeyboardFocusWidget(NULL); // Remove keyboard focus from this widget
	}
	else
	{
	}
	
	recreateTextWidget(); // Re-create glui_text to show new text
	updateOverlayObTransforms();

	key_event.accepted = true;
}


void GLUILineEdit::doHandleTextInputEvent(TextInputEvent& text_input_event)
{
	{
		// Insert text at cursor position
		const size_t old_cursor_byte_index = getCursorByteIndex(text, cursor_pos);

		text.insert(old_cursor_byte_index, text_input_event.text);

		cursor_pos += (int)UTF8Utils::numCodePointsInString(text_input_event.text); // There may have been multiple chars/code-points in text_input_event string.
		this->last_cursor_update_time = Clock::getCurTimeRealSec();
	}

	recreateTextWidget(); // Re-create glui_text to show new text
	updateOverlayObTransforms();

	text_input_event.accepted = true;
}


void GLUILineEdit::updateGLTransform(GLUI& /*glui*/)
{
	updateTextTransform();
	updateOverlayObTransforms();
}


void GLUILineEdit::setVisible(bool visible)
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


const Vec2f GLUILineEdit::getDims() const
{
	if(glui_text.nonNull())
		return glui_text->getDims();
	else
		return Vec2f(0.f);
}


void GLUILineEdit::setPos(const Vec2f& botleft_)
{
	botleft = botleft_;

	updateTextTransform();
	updateOverlayObTransforms();
}


GLUILineEdit::GLUILineEditCreateArgs::GLUILineEditCreateArgs():
	background_colour(0.f),
	mouseover_background_colour(0.1f),
	background_alpha(1),
	text_colour(1.f),
	text_alpha(1.f),
	padding_px(10),
	font_size_px(14),
	width(0.2f),
	rounded_corner_radius_px(8)
{}
