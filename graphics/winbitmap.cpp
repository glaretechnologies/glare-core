/*=====================================================================
winbitmap.cpp
-------------
File created by ClassTemplate on Wed Jul 14 01:03:29 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "winbitmap.h"


#include <assert.h>
#include "jpegdecoder.h"
#include <fstream>
#include "../utils/fileutils.h"

WinBitmap::WinBitmap(int width_, int height_/*, HWND windowHandle_*/)
:	width(width_),
	height(height_)
	//windowhandle(windowHandle_)
{
	bitmapdata = NULL;

	bytespp = 3;


	const int actual_bitmapinfo_size = sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD)*3;

	BITMAPINFO* bitmapinfo = (BITMAPINFO*)new char[actual_bitmapinfo_size];
	
	//HANDLE heap = GetProcessHeap();

	//BITMAPINFO* bitmapinfo = (BITMAPINFO*)HeapAlloc(heap, 0, actual_bitmapinfo_size);

	memset(bitmapinfo, 0, actual_bitmapinfo_size);

	BITMAPINFOHEADER* bitmapinfoheader = (BITMAPINFOHEADER*)bitmapinfo;

	bitmapinfoheader->biSize = sizeof(BITMAPINFOHEADER);
	bitmapinfoheader->biWidth = width;
	bitmapinfoheader->biHeight = -height; // Height of the bitmap. Negative values indicate a top-down bitmap instead of a bottom-up
	bitmapinfoheader->biPlanes = 1;
	bitmapinfoheader->biBitCount = bytespp * 8;//TEMP was 32;
	bitmapinfoheader->biCompression = BI_RGB;
	bitmapinfoheader->biSizeImage = 0;//compressed size, 0 for uncompressed
	bitmapinfoheader->biXPelsPerMeter = 0;
	bitmapinfoheader->biYPelsPerMeter = 0;
	bitmapinfoheader->biClrUsed = 0;
	bitmapinfoheader->biClrImportant = 0;

	//HDC windowDC = GetDC(windowHandle); // Get the DC for this window
	//HDC dc = CreateCompatibleDC(windowDC); // Create a compatible one in memory

	bitmap = CreateDIBSection(NULL, 
							bitmapinfo, 
							DIB_RGB_COLORS,
							&bitmapdata,
							NULL,
							0);

	if(!bitmap)
	{
		int lasterror = GetLastError();
	}

	assert(bitmap != 0);
	assert(bitmapdata != 0);

	delete[] bitmapinfo;;
}


WinBitmap::~WinBitmap()
{
	if(bitmap)
	{
		const bool result = DeleteObject(bitmap);
		assert(result);
	}
}

void WinBitmap::drawText(const std::string& text, const Vec2& normalised_pos, 
						 const Vec3& colour)
{
	const HDC windowDC = GetDC(0);//windowhandle); // Get the DC for this window
	const HDC dc = CreateCompatibleDC(windowDC); // Create a compatible one in memory


	// get the bitmap selected for the dc, select new bitmap
	const HBITMAP old_bitmap = (HBITMAP)SelectObject(dc, bitmap);

	RECT drawregion;
	drawregion.left = 10;
	drawregion.top = 10;
	drawregion.right = width - 10;
	drawregion.bottom = height - 10;

	LOGFONT logfont;
	memset(&logfont, 1, sizeof(LOGFONT));

	logfont.lfHeight = 30;
	logfont.lfWidth = 0;//16;
	logfont.lfEscapement = 0;
	logfont.lfOrientation = 0;
	logfont.lfWeight = FW_NORMAL;//FW_BOLD;//FW_DONTCARE;
	logfont.lfItalic = 0;
	logfont.lfUnderline = 0;
	logfont.lfStrikeOut = 0;
	logfont.lfCharSet = DEFAULT_CHARSET;
	logfont.lfOutPrecision = OUT_DEFAULT_PRECIS;
	logfont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	logfont.lfQuality = DEFAULT_QUALITY;
	logfont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	strcpy(logfont.lfFaceName, "Times New Roman");


	HFONT font = CreateFontIndirect(&logfont);
	
	HGDIOBJ oldfont = SelectObject(dc, font);

	//bool result = SetTextColor(dc, RGB(200, 200, 200));
	bool result = SetTextColor(dc, RGB((int)(colour.x * 255.0f), (int)(colour.y * 255.0f), (int)(colour.z * 255.0f)));
	result = SetBkMode(dc, TRANSPARENT);


	//bool result = SetTextColor(dc, RGB((unsigned char)(colour.x*255.0f), (unsigned char)(colour.y*255.0f), (unsigned char)(colour.z*255.0f)));

	result = DrawText(dc, text.c_str(), text.length(), &drawregion, DT_LEFT | DT_WORDBREAK);


	SelectObject(dc, oldfont);
	DeleteObject(font);

	/*// set the colors for the text
	bool result = SetTextColor(dc, RGB(colour.x*255.0f, colour.y*255.0f, colour.z*255.0f));
	//assert(result);

	// set background mode to transparent so black isn't copied
	result = SetBkMode(dc, TRANSPARENT);
	assert(result);

	//-----------------------------------------------------------------
	//draw text
	//-----------------------------------------------------------------
	result = TextOut(dc, (int)(normalised_pos.x * (float)width), 
				(int)(normalised_pos.y * (float)height), text.c_str(), strlen(text.c_str()));
	assert(result);*/

	//-----------------------------------------------------------------
	//restore old DC
	//-----------------------------------------------------------------
	result = SelectObject(dc, old_bitmap);
	assert(result);

	DeleteObject(dc);





}

const void* WinBitmap::getImageData()
{
	return bitmapdata;
}


void WinBitmap::saveAsJPGFile(const std::string& pathname)
{
	FileUtils::createDirForPathname(pathname);
	std::ofstream file(pathname.c_str(), std::ios::binary);

	//------------------------------------------------------------------------
	//pack the data NOTE: tempish hackish
	//------------------------------------------------------------------------
	unsigned char* packeddata = new unsigned char[width*height*3];

	int i = 0;
	for(int y=0; y<height; ++y)
		for(int x=0; x<width; ++x)
		{
			packeddata[i++] = ((unsigned char*)bitmapdata)[(x + y*width)*bytespp];
			packeddata[i++] = ((unsigned char*)bitmapdata)[(x + y*width)*bytespp + 1];
			packeddata[i++] = ((unsigned char*)bitmapdata)[(x + y*width)*bytespp + 2];
		}


	int compressed_len;
	void* comrepsseddata = JPEGDecoder::Compress(packeddata, width, height, 3, 
									compressed_len, 75);

	assert(comrepsseddata);
	assert(compressed_len >= 0);

	file.write((const char*)comrepsseddata, compressed_len);

	delete[] packeddata;
	delete[] comrepsseddata;

}


int WinBitmap::getBytesPP() const
{
	return bytespp;
}
