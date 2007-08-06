/*=====================================================================
winbitmap.h
-----------
File created by ClassTemplate on Wed Jul 14 01:03:29 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __WINBITMAP_H_666_
#define __WINBITMAP_H_666_


#include <windows.h>
#include <string>
#include "../maths/vec2.h"
#include "../maths/vec3.h"


/*=====================================================================
WinBitmap
---------
32 bpp Windows bitmap.
Can draw TrueType text on itself.
=====================================================================*/
class WinBitmap
{
public:
	/*=====================================================================
	WinBitmap
	---------
	
	=====================================================================*/
	WinBitmap(int width, int height/*, HWND windowHandle*/);

	~WinBitmap();


	void drawText(const std::string& text, const Vec2& normalised_pos, const Vec3& colour);

	void saveAsJPGFile(const std::string& pathname);

	//32bpp data
	const void* getImageData();

	int getBytesPP() const;

private:
	//HWND windowhandle;
	HBITMAP bitmap;
	int width;
	int height;
	int bytespp;

	void* bitmapdata;
};



#endif //__WINBITMAP_H_666_




