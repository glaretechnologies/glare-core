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
#include "../MeshPrimitiveBuilding.h"
#include "../utils/FileUtils.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/UTF8Utils.h"
#include "../utils/RuntimeCheck.h"


GLUITextView::CreateArgs::CreateArgs():
	background_colour(0.f),
	background_alpha(1),
	background_corner_radius_px(0),
	text_colour(1.f),
	text_alpha(1.f),
	padding_px((int)std::ceil(GLUI::getDefaultFontSizePx() * 0.6f)),
	font_size_px(GLUI::getDefaultFontSizePx()),
	line_0_x_offset(0),
	max_width(100000),
	text_selectable(true),
	z(0.f)
{
	sizing_type_x = GLUIWidget::SizingType::SizingType_FixedSizePx;
	sizing_type_y = GLUIWidget::SizingType::SizingType_FixedSizePx;
	fixed_size = Vec2f(100, std::ceil(GLUI::getDefaultFontSizePx() * 2.4f));
}


GLUITextView::GLUITextView(GLUI& glui_, const std::string& text_, const Vec2f& botleft_, const CreateArgs& args_)
{
	this->clip_rect = Rect2f(Vec2f(-1.0e10f), Vec2f(1.0e10f));

	glui = &glui_;
	args = args_;
	opengl_engine = glui_.opengl_engine.ptr();
	tooltip = args.tooltip;
	m_z = args_.z;
	last_rounded_background_dims = Vec2f(-1.f);

	sizing_type_x = args.sizing_type_x;
	sizing_type_y = args.sizing_type_y;
	fixed_size = args.fixed_size;

	selection_start = selection_end = -1;
	selection_start_text_index = selection_end_text_index = 0;
	visible = true;

	if(args.background_alpha != 0)
	{
		background_overlay_ob = new OverlayObject();
		background_overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
		background_overlay_ob->material.albedo_linear_rgb = args.background_colour;
		background_overlay_ob->material.alpha = args.background_alpha;

		opengl_engine->addOverlayObject(background_overlay_ob);
	}

	selection_overlay_ob = new OverlayObject();
	selection_overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	selection_overlay_ob->material.albedo_linear_rgb = Colour3f(1,1,1);
	selection_overlay_ob->material.alpha = 0.5f;
	selection_overlay_ob->draw = false;

	opengl_engine->addOverlayObject(selection_overlay_ob);

	this->rect = Rect2f(botleft_, botleft_ + Vec2f(0.1f));

	setText(text_); // Sets rect

	updateOverlayObTransforms();
}


GLUITextView::~GLUITextView()
{
	if(!glui) // If this widget was leaked:
		for(size_t i=0; i<glui_texts.size(); ++i)
		{
			glui_texts[i]->setGLUI(nullptr);
			glui_texts[i]->setOpenGLEngine(nullptr);
		}

	checkRemoveOverlayObAndSetRefToNull(opengl_engine, background_overlay_ob);
	checkRemoveOverlayObAndSetRefToNull(opengl_engine, selection_overlay_ob);
}


// Also updates rect
void GLUITextView::setText(const string_view new_text)
{
	if(new_text != text)
	{
		// Remove any old text
		glui_texts.clear();

		text = new_text;

		// Re-create glui_text

		// Work out line break positions
		size_t last_line_start_byte_i = 0;
		std::vector<std::pair<size_t, size_t>> line_begin_end_bytes;
		{
			const float x_scale = 2.f / opengl_engine->getViewPortWidth(); // see GLUIText::updateGLTransform

			const size_t num_codepoints = UTF8Utils::numCodePointsInString(text);

			size_t cur_string_byte_i = 0;
			float cur_char_pos_x = args.line_0_x_offset / x_scale; // in pixel coords
			for(size_t i=0; i<num_codepoints; ++i)
			{
				runtimeCheck(cur_string_byte_i < text.size());
				const size_t char_num_bytes = UTF8Utils::numBytesForChar(text[cur_string_byte_i]);
				if(cur_string_byte_i + char_num_bytes > text.size())
					throw glare::Exception("Invalid UTF-8 string.");
				const string_view substring(text.data() + cur_string_byte_i, char_num_bytes);
				if(substring == "\n")
				{
					// Record last line
					line_begin_end_bytes.push_back(std::make_pair(last_line_start_byte_i, cur_string_byte_i));

					// Start new line after the newline char
					last_line_start_byte_i = cur_string_byte_i + 1;
					cur_char_pos_x = 0;
				}
				else
				{
					const CharTexInfo info = glui->font_char_text_cache->getCharTexture(opengl_engine, glui->getFonts(), glui->getEmojiFonts(), substring, args.font_size_px, /*render_SDF=*/false);
		
					const float vert_pos_scale = 1.f;
					const Vec3f max_pos_px = (Vec3f(cur_char_pos_x + (float)info.size_info.max_bounds.x, (float)info.size_info.max_bounds.y, 0)) * vert_pos_scale;

					const float max_pos_x_ui = max_pos_px.x * x_scale;

					if(max_pos_x_ui >= args.max_width)
					{
						// This char needs to start on a new line
						if(cur_string_byte_i > last_line_start_byte_i)
						{
							// Record last line
							line_begin_end_bytes.push_back(std::make_pair(last_line_start_byte_i, cur_string_byte_i));

							// Start new line
							last_line_start_byte_i = cur_string_byte_i;
							cur_char_pos_x = 0;
						}
					}

					cur_char_pos_x += info.size_info.hori_advance;
				}

				cur_string_byte_i += char_num_bytes;
			}

			if(cur_string_byte_i > last_line_start_byte_i)
			{
				line_begin_end_bytes.push_back(std::make_pair(last_line_start_byte_i, cur_string_byte_i));
			}
		}

		// Create the line text objects
		const float padding = glui->getUIWidthForDevIndepPixelWidth((float)args.padding_px);

		// See getOffsetToTopLine()
		const float offset_to_top_line = glui->getUIWidthForDevIndepPixelWidth(args.font_size_px + 4.f) * myMax(0, (int)line_begin_end_bytes.size() - 1);

		float cur_line_y_offset = -offset_to_top_line;
		for(size_t i=0; i<line_begin_end_bytes.size(); ++i)
		{
			runtimeCheck(line_begin_end_bytes[i].first < text.size());
			runtimeCheck(line_begin_end_bytes[i].second <= text.size());
			const string_view line_string(text.data() + line_begin_end_bytes[i].first, line_begin_end_bytes[i].second - line_begin_end_bytes[i].first);

			GLUIText::CreateArgs text_create_args;
			text_create_args.colour = args.text_colour;
			text_create_args.font_size_px = args.font_size_px;
			text_create_args.alpha = args.text_alpha;
			text_create_args.z = m_z;
			GLUITextRef glui_text = new GLUIText(*glui, toString(line_string), /*botleft=*/getRect().getMin(), text_create_args);
			glui_text->setVisible(visible);

			// Compute position of current text line
			Vec2f textpos = getRect().getMin() + Vec2f(padding, padding - cur_line_y_offset);
			if(i == 0)
				textpos.x += args.line_0_x_offset;

			glui_text->setPos(textpos);
			glui_texts.push_back(glui_text);

			cur_line_y_offset += glui->getUIWidthForDevIndepPixelWidth(args.font_size_px + 4.f); // TEMP HACK line spacing
		}

		this->rect = recomputeRect();
		updateOverlayObTransforms();
	}
}


void GLUITextView::updateOverlayObTransforms()
{
	if(background_overlay_ob)
	{
		const Rect2f background_rect = this->getRect();

		const float y_scale = opengl_engine->getViewPortAspectRatio(); // scale from GL UI to opengl coords
		const float z = this->m_z + 0.001f;

		const bool use_rounded_corners = args.background_corner_radius_px > 0;

		background_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(background_rect.getMin().x, background_rect.getMin().y * y_scale, z) * 
			(use_rounded_corners ? Matrix4f::scaleMatrix(1, y_scale, 1) : // For rounded corner geometry
				Matrix4f::scaleMatrix(background_rect.getWidths().x, background_rect.getWidths().y * y_scale, 1));

		if((args.background_corner_radius_px > 0) && (last_rounded_background_dims != background_rect.getWidths()))
		{
			//conPrint("GLUITextView: recreating rounded-corner rect for text '" + this->text + "'");

			background_overlay_ob->mesh_data = MeshPrimitiveBuilding::makeRoundedCornerRect(*opengl_engine->vert_buf_allocator, /*i=*/Vec4f(1,0,0,0), /*j=*/Vec4f(0,1,0,0), 
				/*w=*/background_rect.getWidths().x, /*h=*/background_rect.getWidths().y, 
				/*corner radius=*/glui->getUIWidthForDevIndepPixelWidth(args.background_corner_radius_px), /*tris_per_corner=*/8);
	
			this->last_rounded_background_dims = background_rect.getWidths();
		}
	}

	// Update selection ob
	if(selection_overlay_ob.nonNull())
	{
		const float extra_half_h_factor = 0.2f; // Make the cursor a bit bigger than the font size
		const float h = (float)glui->getUIWidthForDevIndepPixelWidth((float)args.font_size_px * (1 + extra_half_h_factor*2));

		if(selection_start != -1 && selection_end != -1 && (selection_start_text_index < glui_texts.size()))
		{
			Vec2f selection_lower_left_pos  = glui_texts[selection_start_text_index]->getCharPos(*glui, selection_start);
			Vec2f selection_upper_right_pos = glui_texts[selection_start_text_index]->getCharPos(*glui, selection_end) + Vec2f(0, h);

			if(selection_lower_left_pos.x > selection_upper_right_pos.x)
				mySwap(selection_lower_left_pos, selection_upper_right_pos);

			selection_lower_left_pos.y  = myMin(selection_lower_left_pos.y, selection_upper_right_pos.y);
			selection_upper_right_pos.y = myMax(selection_lower_left_pos.y, selection_upper_right_pos.y);

			const float w = std::fabs(selection_upper_right_pos.x - selection_lower_left_pos.x);

			try
			{
				const float y_scale = opengl_engine->getViewPortAspectRatio(); // scale from GL UI to opengl coords
				const float z = this->m_z - 0.001f;
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

Rect2f GLUITextView::recomputeRect() const
{
	// fixed_size will be max(fixed_size, computeTextDimsWithPadding())

	Vec2f text_dims = computeTextDimsWithPadding();
	Vec2f dims(0.f);
	if(sizing_type_x == SizingType_FixedSizePx)
	{
		dims.x = myMax(glui->getUIWidthForDevIndepPixelWidth(fixed_size.x), text_dims.x);
	}
	else if(sizing_type_x == SizingType_FixedSizeUICoords)
	{
		dims.x = myMax(fixed_size.x, text_dims.x);
	}
	else if(sizing_type_x == SizingType_Expanding)
	{
		dims.x = text_dims.x;
	}

	if(sizing_type_y == SizingType_FixedSizePx)
	{
		dims.y = myMax(glui->getUIWidthForDevIndepPixelWidth(fixed_size.y), text_dims.y);
	}
	else if(sizing_type_y == SizingType_FixedSizeUICoords)
	{
		dims.y = myMax(fixed_size.y, text_dims.y);
	}
	else if(sizing_type_y == SizingType_Expanding)
	{
		dims.y = text_dims.y;
	}

	return Rect2f::fromMinAndSpan(getRect().getMin(), dims);
}


Vec2f GLUITextView::computeTextDimsWithPadding() const
{
	Vec2f text_dims(0.f);
	if(!glui_texts.empty())
	{
		Rect2f temp_rect = glui_texts[0]->getRect();
		for(size_t i=1; i<glui_texts.size(); ++i)
			temp_rect.enlargeToHoldRect(glui_texts[i]->getRect());
		text_dims = temp_rect.getWidths();
	}

	const float padding = glui->getUIWidthForDevIndepPixelWidth((float)args.padding_px);
	return text_dims + Vec2f(padding * 2);
}


void GLUITextView::setTextColour(const Colour3f& col)
{
	args.text_colour = col;
	
	for(size_t i=0; i<glui_texts.size(); ++i)
		glui_texts[i]->setColour(col);
}


void GLUITextView::setTextAlpha(float alpha)
{
	for(size_t i=0; i<glui_texts.size(); ++i)
		glui_texts[i]->setAlpha(alpha);
}


void GLUITextView::setBackgroundColour(const Colour3f& col)
{
	if(background_overlay_ob)
		background_overlay_ob->material.albedo_linear_rgb = col;
}


void GLUITextView::setBackgroundAlpha(float alpha)
{
	if(background_overlay_ob.nonNull())
		background_overlay_ob->material.alpha = alpha;
}


Vec2f GLUITextView::getMinDims() const
{
	// NOTE: same code as recomputeRect() currently...
	Vec2f text_dims = computeTextDimsWithPadding();
	Vec2f dims(0.f);
	if(sizing_type_x == SizingType_FixedSizePx)
	{
		dims.x = myMax(glui->getUIWidthForDevIndepPixelWidth(fixed_size.x), text_dims.x);
	}
	else if(sizing_type_x == SizingType_FixedSizeUICoords)
	{
		dims.x = myMax(fixed_size.x, text_dims.x);
	}
	else if(sizing_type_x == SizingType_Expanding)
	{
		dims.x = text_dims.x;
	}

	if(sizing_type_y == SizingType_FixedSizePx)
	{
		dims.y = myMax(glui->getUIWidthForDevIndepPixelWidth(fixed_size.y), text_dims.y);
	}
	else if(sizing_type_y == SizingType_FixedSizeUICoords)
	{
		dims.y = myMax(fixed_size.y, text_dims.y);
	}
	else if(sizing_type_y == SizingType_Expanding)
	{
		dims.y = text_dims.y;
	}

	return dims;
}


void GLUITextView::viewportResized()
{
	for(size_t i=0; i<glui_texts.size(); ++i)
		glui_texts[i]->updateGLTransform();

	this->rect = recomputeRect();

	updateOverlayObTransforms();
}


void GLUITextView::handleCutEvent(std::string& /*clipboard_contents_out*/)
{
	// Since GLUITextView is a read-only widget, we can't cut. So do nothing.
}


void GLUITextView::handleCopyEvent(std::string& clipboard_contents_out)
{
	if(selection_start != -1 && selection_end != -1 && (selection_start_text_index < glui_texts.size()))
	{
		if(selection_start > selection_end)
			mySwap(selection_start, selection_end);

		const size_t start_byte_index = TextEditingUtils::getCursorByteIndex(glui_texts[selection_start_text_index]->getText(), selection_start);
		const size_t end_byte_index   = TextEditingUtils::getCursorByteIndex(glui_texts[selection_start_text_index]->getText(), selection_end);
		
		clipboard_contents_out = text.substr(start_byte_index, end_byte_index - start_byte_index);
	}
}


// For multi-line text, the top line will be some vertical offset above the bottom line.  Compute this offset and return it.
float GLUITextView::getOffsetToTopLine() const
{
	return glui->getUIWidthForDevIndepPixelWidth(args.font_size_px + 4.f) * myMax(0, (int)glui_texts.size() - 1);
}


void GLUITextView::setPos(const Vec2f& new_botleft)
{
	const float padding = glui->getUIWidthForDevIndepPixelWidth((float)args.padding_px);

	const float total_line_y_offset = getOffsetToTopLine();

	float cur_line_y_offset = 0; // (Positive) offset down from top line
	for(size_t i=0; i<glui_texts.size(); ++i)
	{
		// Compute position of current text line
		Vec2f textpos = new_botleft + Vec2f(padding, padding + total_line_y_offset - cur_line_y_offset);
		if(i == 0)
			textpos.x += args.line_0_x_offset;

		glui_texts[i]->setPos(textpos);
		cur_line_y_offset += glui->getUIWidthForDevIndepPixelWidth(args.font_size_px + 4.f); // TEMP HACK line spacing
	}

	this->rect = Rect2f::fromMinAndSpan(new_botleft, this->getDims());
	updateOverlayObTransforms();
}


void GLUITextView::setAvailableRegionDims(const Vec2f& available_dims)
{
	Vec2f dims = this->getDims();

	if(sizing_type_x == SizingType_Expanding)
		dims.x = myMax(dims.x, available_dims.x);
	if(sizing_type_y == SizingType_Expanding)
		dims.y = myMax(dims.y, available_dims.y);

	rect = Rect2f::fromMinAndSpan(this->getRect().getMin(), dims);

	updateOverlayObTransforms();
}


void GLUITextView::setZ(float new_z)
{
	for(size_t i=0; i<glui_texts.size(); ++i)
		glui_texts[i]->setZ(new_z);

	this->m_z = new_z;
}


float GLUITextView::getPaddingWidth() const
{
	return glui->getUIWidthForDevIndepPixelWidth((float)args.padding_px);
}


void GLUITextView::setClipRegion(const Rect2f& clip_rect_)
{
	this->clip_rect = clip_rect_;

	for(size_t i=0; i<glui_texts.size(); ++i)
		glui_texts[i]->setClipRegion(clip_rect);

	if(background_overlay_ob)
		background_overlay_ob->clip_region = glui->OpenGLRectCoordsForUICoords(clip_rect);

	if(selection_overlay_ob)
		selection_overlay_ob->clip_region = glui->OpenGLRectCoordsForUICoords(clip_rect);
}


void GLUITextView::setVisible(bool visible_)
{
	visible = visible_;

	for(size_t i=0; i<glui_texts.size(); ++i)
		glui_texts[i]->setVisible(visible);

	if(background_overlay_ob.nonNull())
		background_overlay_ob->draw = visible;
	
	if(selection_overlay_ob.nonNull())
		selection_overlay_ob->draw = visible && (selection_start != -1 && selection_end != -1);
}


bool GLUITextView::isVisible()
{
	return visible;
}


void GLUITextView::handleMousePress(MouseEvent& event)
{
	if(!visible)
		return;

	if(args.text_selectable)
	{
		const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
		if(rect.inClosedRectangle(coords) && clip_rect.inClosedRectangle(coords))
		{
			//conPrint("GLUITextView taking keyboard focus");
			glui->setKeyboardFocusWidget(this);

			for(size_t i=0; i<glui_texts.size(); ++i)
			{
				GLUIText* glui_text = glui_texts[i].ptr();
				if(glui_text->getRect().inClosedRectangle(coords))
				{
					this->selection_start = glui_text->cursorPosForUICoords(*glui, coords);
					this->selection_end = -1;
					this->selection_start_text_index = i;
				}
			}

			updateOverlayObTransforms(); // Redraw since cursor visible will have changed

			event.accepted = true;
		}
	}
}


void GLUITextView::handleMouseRelease(MouseEvent& /*event*/)
{
}


void GLUITextView::handleMouseDoubleClick(MouseEvent& event)
{
	if(!visible)
		return;

	if(args.text_selectable)
	{
		const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
		if(rect.inClosedRectangle(coords) && clip_rect.inClosedRectangle(coords))
		{
			for(size_t i=0; i<glui_texts.size(); ++i)
			{
				GLUIText* glui_text = glui_texts[i].ptr();
				if(glui_text->getRect().inClosedRectangle(coords))
				{
					const int click_cursor_pos = glui_text->cursorPosForUICoords(*glui, coords);
				
					this->selection_start = TextEditingUtils::getPrevStartOfNonWhitespaceCursorPos(glui_text->getText(), click_cursor_pos);
				
					this->selection_end = TextEditingUtils::getNextEndOfNonWhitespaceCursorPos(glui_text->getText(), click_cursor_pos);

					this->selection_start_text_index = i;
				
					updateOverlayObTransforms(); // Redraw since selection region has changed.
				}
			}
		}
	}
}


void GLUITextView::doHandleMouseMoved(MouseEvent& mouse_event)
{
	if(!visible)
		return;

	if(args.text_selectable)
	{
		// If left mouse button is down, update selection region
		if((this->selection_start != -1) && (mouse_event.button_state & (uint32)MouseButton::Left) != 0)
		{
			if(selection_start_text_index < glui_texts.size())
			{
				const Vec2f coords = glui->UICoordsForOpenGLCoords(mouse_event.gl_coords);
			
				this->selection_end = glui_texts[selection_start_text_index]->cursorPosForUICoords(*glui, coords);
			
				updateOverlayObTransforms(); // Redraw since selection region may have changed.
			}
		}
	}
}


void GLUITextView::handleLosingKeyboardFocus()
{
	if(args.text_selectable)
	{
		// Clear selection
		this->selection_start = this->selection_end = -1;
		updateOverlayObTransforms(); // Redraw
	}
}
