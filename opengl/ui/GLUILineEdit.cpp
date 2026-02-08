/*=====================================================================
GLUILineEdit.cpp
----------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "GLUILineEdit.h"


#include "GLUI.h"
#include "TextEditingUtils.h"
#include "../OpenGLMeshRenderData.h"
#include "../MeshPrimitiveBuilding.h"
#include "../../graphics/SRGBUtils.h"
#include "../../utils/Exception.h"
#include "../../utils/ConPrint.h"
#include "../../utils/StringUtils.h"
#include "../../utils/UTF8Utils.h"


GLUILineEdit::GLUILineEdit(GLUI& glui_, Reference<OpenGLEngine>& opengl_engine_, const Vec2f& botleft_, const CreateArgs& args_)
{
	glui = &glui_;

	args = args_;
	this->m_z = args_.z;
	opengl_engine = opengl_engine_;
	tooltip = args.tooltip;

	botleft = botleft_;

	cursor_pos = 0;
	last_cursor_update_time = 0;

	selection_start = -1;
	selection_end = -1;

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


	selection_overlay_ob = new OverlayObject();
	selection_overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	selection_overlay_ob->material.albedo_linear_rgb = Colour3f(1,1,1);
	selection_overlay_ob->material.alpha = 0.5f;
	selection_overlay_ob->draw = false;

	opengl_engine->addOverlayObject(selection_overlay_ob);


	updateOverlayObTransforms();
}


GLUILineEdit::~GLUILineEdit()
{
	if(background_overlay_ob.nonNull())
		opengl_engine->removeOverlayObject(background_overlay_ob);
	
	if(cursor_overlay_ob.nonNull())
		opengl_engine->removeOverlayObject(cursor_overlay_ob);
	
	if(selection_overlay_ob.nonNull())
		opengl_engine->removeOverlayObject(selection_overlay_ob);
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


void GLUILineEdit::setWidth(float width)
{
	if(width != args.width)
	{
		args.width = width;

		this->last_viewport_dims = Vec2i(0); // Force recreate rounded-corner rect
		updateOverlayObTransforms();
	}
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
			//conPrint("GLUILineEdit: Viewport has changed, recreate rounded-corner rect.");
			
			background_overlay_ob->mesh_data = MeshPrimitiveBuilding::makeRoundedCornerRect(*opengl_engine->vert_buf_allocator, /*i=*/Vec4f(1,0,0,0), /*j=*/Vec4f(0,1,0,0), /*w=*/background_w, /*h=*/background_h, 
				/*corner radius=*/glui->getUIWidthForDevIndepPixelWidth(args.rounded_corner_radius_px), /*tris_per_corner=*/8);

			this->last_viewport_dims = opengl_engine->getViewportDims();
		}

		const float y_scale = opengl_engine->getViewPortAspectRatio(); // scale from GL UI to opengl coords
		const float z = args.z + 0.001f;
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
			const float z = args.z - 0.001f;
			cursor_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(cursor_lower_left_pos.x, (cursor_lower_left_pos.y - h * extra_half_h_factor) * y_scale, z) * Matrix4f::scaleMatrix(w, h * y_scale, 1);
		}
		catch(glare::Exception& e)
		{
			conPrint("Excep: " + e.what());
		}
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
				const float z = args.z - 0.001f;
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


// Re-create glui_text
void GLUILineEdit::recreateTextWidget()
{
	// Destroy existing text, if any
	glui_text = NULL;

	GLUIText::CreateArgs text_create_args;
	text_create_args.colour = args.text_colour;
	text_create_args.font_size_px = args.font_size_px;
	text_create_args.alpha = args.text_alpha;
	text_create_args.z = args.z;
	glui_text = new GLUIText(*glui, opengl_engine, text, /*botleft=*/Vec2f(0.f), text_create_args);

	// Set clip region so text doesn't draw outside of line edit.
	glui_text->setClipRegion(Rect2f(botleft, botleft + Vec2f(args.width, glui->getUIWidthForDevIndepPixelWidth(this->height_px))));

	updateTextTransform();
}


void GLUILineEdit::updateTextTransform()
{
	if(glui_text.nonNull())
	{
		const float margin_x = glui->getUIWidthForDevIndepPixelWidth((float)args.padding_px);
		const float margin_y = margin_x;

		Vec2f rel_cursor_pos = glui_text->getRelativeCharPos(*glui, cursor_pos);

		// Shift text left if cursor position would be to right of line edit.
		const float max_rel_cursor_x = args.width - margin_x * 2; // glui->getUIWidthForDevIndepPixelWidth(15);
		const float left_shift_amount = myMax(0.f, rel_cursor_pos.x - max_rel_cursor_x);
		const Vec2f text_botleft = botleft + Vec2f(margin_x, margin_y) - Vec2f(left_shift_amount, 0);

		glui_text->setPos(text_botleft);

		// Set clip region so text doesn't draw outside of line edit.
		glui_text->setClipRegion(Rect2f(botleft, botleft + Vec2f(args.width, glui->getUIWidthForDevIndepPixelWidth(this->height_px))));
	}
}


void GLUILineEdit::handleMousePress(MouseEvent& event)
{
	if(!background_overlay_ob->draw) // If not visible:
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
	{
		//conPrint("GLUILineEdit taking keyboard focus");
		glui->setKeyboardFocusWidget(this);

		this->cursor_pos = glui_text->cursorPosForUICoords(*glui, coords);
		this->last_cursor_update_time = Clock::getCurTimeRealSec();

		this->selection_start = this->cursor_pos;
		this->selection_end = -1;

		updateOverlayObTransforms(); // Redraw since cursor visible will have changed

		event.accepted = true;
	}
}


void GLUILineEdit::handleMouseRelease(MouseEvent& /*event*/)
{
}


void GLUILineEdit::handleMouseDoubleClick(MouseEvent& event)
{
	if(!background_overlay_ob->draw) // If not visible:
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
	{
		const int click_cursor_pos = glui_text->cursorPosForUICoords(*glui, coords);

		this->selection_start = TextEditingUtils::getPrevStartOfNonWhitespaceCursorPos(text, click_cursor_pos);

		this->selection_end = TextEditingUtils::getNextEndOfNonWhitespaceCursorPos(text, click_cursor_pos);

		updateOverlayObTransforms(); // Redraw since selection region has changed.
	}
}


void GLUILineEdit::doHandleMouseMoved(MouseEvent& mouse_event)
{
	if(!background_overlay_ob->draw) // If not visible:
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(mouse_event.gl_coords);

	if(background_overlay_ob.nonNull())
	{
		if(rect.inOpenRectangle(coords)) // If mouse over widget:
			background_overlay_ob->material.albedo_linear_rgb = args.mouseover_background_colour;
		else
			background_overlay_ob->material.albedo_linear_rgb = args.background_colour;

		// If left mouse button is down, update selection region
		if((this->selection_start != -1) && (mouse_event.button_state & (uint32)MouseButton::Left) != 0)
		{
			this->selection_end = glui_text->cursorPosForUICoords(*glui, coords);

			updateOverlayObTransforms(); // Redraw since selection region may have changed.
		}
	}
}


void GLUILineEdit::deleteSelectedTextAndClearSelection()
{
	try
	{
		if(selection_start != -1 && selection_end != -1)
		{
			if(selection_start > selection_end)
				mySwap(selection_start, selection_end);

			const size_t start_byte_index = TextEditingUtils::getCursorByteIndex(text, selection_start);
			const size_t end_byte_index   = TextEditingUtils::getCursorByteIndex(text, selection_end);
			text.erase(/*offset=*/start_byte_index, /*count=*/end_byte_index - start_byte_index);

			// Move cursor left if was to right of deleted text
			if(cursor_pos >= selection_end)
				cursor_pos -= (selection_end - selection_start);
		}

		selection_start = selection_end = -1;
	}
	catch(glare::Exception& e)
	{
		conPrint("excep: " + e.what());
	}
}


void GLUILineEdit::doHandleKeyPressedEvent(KeyEvent& key_event)
{
	if(!background_overlay_ob->draw) // If not visible:
		return;

	if(key_event.key == Key::Key_Backspace)
	{
		deleteSelectedTextAndClearSelection();

		if(cursor_pos > 0)
		{
			try
			{
				const size_t old_cursor_byte_index = TextEditingUtils::getCursorByteIndex(text, cursor_pos);

				if(BitUtils::isBitSet(key_event.modifiers, (uint32)Modifiers::Ctrl))
					cursor_pos = TextEditingUtils::getPrevStartOfNonWhitespaceCursorPos(text, cursor_pos);
				else
					cursor_pos--;

				const size_t new_cursor_byte_index = TextEditingUtils::getCursorByteIndex(text, cursor_pos);

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
		deleteSelectedTextAndClearSelection();

		try
		{
			const size_t old_cursor_byte_index = TextEditingUtils::getCursorByteIndex(text, cursor_pos);

			int end_cursor_pos = cursor_pos;
			if(BitUtils::isBitSet(key_event.modifiers, (uint32)Modifiers::Ctrl))
				end_cursor_pos = TextEditingUtils::getNextStartOfNonWhitespaceCursorPos(text, cursor_pos);
			else
				end_cursor_pos++;

			const size_t next_cursor_byte_index = TextEditingUtils::getCursorByteIndex(text, end_cursor_pos);

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
		{
			if(BitUtils::isBitSet(key_event.modifiers, (uint32)Modifiers::Ctrl))
				cursor_pos = TextEditingUtils::getPrevStartOfNonWhitespaceCursorPos(text, cursor_pos);
			else
				cursor_pos--;
		}

		this->last_cursor_update_time = Clock::getCurTimeRealSec();
	}
	else if(key_event.key == Key::Key_Right)
	{
		if(cursor_pos < (int)UTF8Utils::numCodePointsInString(text))
		{
			if(BitUtils::isBitSet(key_event.modifiers, (uint32)Modifiers::Ctrl))
				cursor_pos = TextEditingUtils::getNextStartOfNonWhitespaceCursorPos(text, cursor_pos);
			else
				cursor_pos++;
		}

		this->last_cursor_update_time = Clock::getCurTimeRealSec();
	}
	else if(key_event.key == Key::Key_Home)
	{
		cursor_pos = 0;

		this->last_cursor_update_time = Clock::getCurTimeRealSec();
	}
	else if(key_event.key == Key::Key_End)
	{
		cursor_pos = (int)UTF8Utils::numCodePointsInString(text);

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

		this->selection_start = this->selection_end = -1;
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
	deleteSelectedTextAndClearSelection();

	{
		// Insert text at cursor position
		const size_t old_cursor_byte_index = TextEditingUtils::getCursorByteIndex(text, cursor_pos);

		text.insert(old_cursor_byte_index, text_input_event.text);

		cursor_pos += (int)UTF8Utils::numCodePointsInString(text_input_event.text); // There may have been multiple chars/code-points in text_input_event string.
		this->last_cursor_update_time = Clock::getCurTimeRealSec();
	}

	recreateTextWidget(); // Re-create glui_text to show new text
	updateOverlayObTransforms();

	text_input_event.accepted = true;
}


void GLUILineEdit::handleLosingKeyboardFocus()
{
	// Clear selection
	this->selection_start = this->selection_end = -1;
	updateOverlayObTransforms(); // Redraw
}


void GLUILineEdit::handleCutEvent(std::string& clipboard_contents_out)
{
	if(selection_start != -1 && selection_end != -1)
	{
		if(selection_start > selection_end)
			mySwap(selection_start, selection_end);

		const size_t start_byte_index = TextEditingUtils::getCursorByteIndex(text, selection_start);
		const size_t end_byte_index   = TextEditingUtils::getCursorByteIndex(text, selection_end);
		
		clipboard_contents_out = text.substr(start_byte_index, end_byte_index - start_byte_index);

		text.erase(start_byte_index, selection_end - selection_start);

		// Move cursor left if was to right of deleted text
		if(cursor_pos >= selection_end)
			cursor_pos -= (selection_end - selection_start);

		// Clear selection
		selection_start = selection_end = -1;

		recreateTextWidget(); // Re-create glui_text to show new text
		updateOverlayObTransforms(); // Redraw
	}
}


void GLUILineEdit::handleCopyEvent(std::string& clipboard_contents_out)
{
	if(selection_start != -1 && selection_end != -1)
	{
		if(selection_start > selection_end)
			mySwap(selection_start, selection_end);

		const size_t start_byte_index = TextEditingUtils::getCursorByteIndex(text, selection_start);
		const size_t end_byte_index   = TextEditingUtils::getCursorByteIndex(text, selection_end);
		
		clipboard_contents_out = text.substr(start_byte_index, end_byte_index - start_byte_index);

		updateOverlayObTransforms(); // Redraw
	}
}


void GLUILineEdit::updateGLTransform()
{
	updateTextTransform();
	updateOverlayObTransforms();
}


void GLUILineEdit::setVisible(bool visible)
{
	if(glui_text.nonNull())
		glui_text->setVisible(visible);

	if(background_overlay_ob.nonNull())
		background_overlay_ob->draw = visible;

	cursor_overlay_ob->draw = visible;
	selection_overlay_ob->draw = visible && (selection_start != -1 && selection_end != -1);
}


bool GLUILineEdit::isVisible()
{
	return background_overlay_ob->draw;
}


//const Vec2f GLUILineEdit::getDims() const
//{
//	if(glui_text.nonNull())
//		return glui_text->getDims();
//	else
//		return Vec2f(0.f);
//}


void GLUILineEdit::setPos(const Vec2f& botleft_)
{
	botleft = botleft_;

	updateTextTransform();
	updateOverlayObTransforms();
}


void GLUILineEdit::setPosAndDims(const Vec2f& new_botleft, const Vec2f& dims)
{
	setPos(new_botleft);
}


void GLUILineEdit::setClipRegion(const Rect2f& clip_rect)
{
	if(background_overlay_ob)
		background_overlay_ob->clip_region = glui->OpenGLRectCoordsForUICoords(clip_rect);

	// TODO: cursor_overlay_ob etc.

	glui_text->setClipRegion(clip_rect);
}


GLUILineEdit::CreateArgs::CreateArgs():
	background_colour(0.f),
	mouseover_background_colour(0.1f),
	background_alpha(1),
	text_colour(1.f),
	text_alpha(1.f),
	padding_px(10),
	font_size_px(14),
	width(0.2f),
	rounded_corner_radius_px(8),
	z(0.f)
{}
