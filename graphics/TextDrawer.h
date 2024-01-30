/*=====================================================================
TextDrawer.h
------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include <string>
#include <vector>
#include "image.h"
#include "Image4f.h"
#include "bitmap.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"


class TextDrawerExcep
{
public:
	TextDrawerExcep(const std::string& text_) : text(text_) {}
	~TextDrawerExcep(){}

	const std::string& what() const { return text; }
private:
	std::string text;
};


/*=====================================================================
TextDrawer
----------
Draws using a fixed width font
=====================================================================*/
class TextDrawer : public RefCounted
{
public:
	/*=====================================================================
	TextDrawer
	----------
	
	=====================================================================*/
	TextDrawer(const std::string& font_image_path, const std::string& font_widths_path); // throws TextDrawerExcep

	~TextDrawer();


	void drawText(const std::string& msg, Image& target, int target_x, int target_y) const;
	void drawText(const std::string& msg, Image4f& target, int target_x, int target_y) const;
	void drawText(const std::string& msg, Bitmap& target, int target_x, int target_y) const;


private:
	Bitmap font_bmp;
	Image font;
	Image4f font_image4;
	std::vector<int> char_widths;
};


typedef Reference<TextDrawer> TextDrawerRef;
