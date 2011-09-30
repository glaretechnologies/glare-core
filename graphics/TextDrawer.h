/*=====================================================================
TextDrawer.h
------------
File created by ClassTemplate on Tue Apr 29 21:08:21 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TEXTDRAWER_H_666_
#define __TEXTDRAWER_H_666_


#include <string>
#include <vector>
#include "image.h"
#include "bitmap.h"
#include "../utils/refcounted.h"
#include "../utils/reference.h"


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
	void drawText(const std::string& msg, Bitmap& target, int target_x, int target_y) const;


private:
	Bitmap font_bmp;
	Image font;
	std::vector<int> char_widths;
};


typedef Reference<TextDrawer> TextDrawerRef;


#endif //__TEXTDRAWER_H_666_
