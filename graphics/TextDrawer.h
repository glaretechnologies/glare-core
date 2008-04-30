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
class TextDrawer
{
public:
	/*=====================================================================
	TextDrawer
	----------
	
	=====================================================================*/
	TextDrawer(const std::string& font_image_path, const std::string& font_widths_path); // throws TextDrawerExcep

	~TextDrawer();


	void drawText(const std::string& msg, Image& target, int target_x, int target_y) const;


private:
	Image font;
	std::vector<int> char_widths;
};



#endif //__TEXTDRAWER_H_666_




