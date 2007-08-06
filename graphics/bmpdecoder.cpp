/*=====================================================================
bmpdecoder.cpp
--------------
File created by ClassTemplate on Mon May 02 22:00:30 2005
Code By Nicholas Chapman.
=====================================================================*/
#include "bmpdecoder.h"


#include "bitmap.h"
#include "imformatdecoder.h"
#include <assert.h>
#include "../utils/stringutils.h"

BMPDecoder::BMPDecoder()
{
	
}


BMPDecoder::~BMPDecoder()
{
	
}


#define BITMAP_ID        0x4D42       // this is the universal id for a bitmap


#pragma pack (push, 1)

typedef struct {
   unsigned short int type;                 /* Magic identifier            */
   unsigned int size;                       /* File size in bytes          */
   unsigned short int reserved1, reserved2;
   unsigned int offset;                     /* Offset to image data, bytes */
} BITMAP_HEADER;



typedef struct {
   unsigned int size;               /* Header size in bytes      */
   int width,height;                /* Width and height of image */
   unsigned short int planes;       /* Number of colour planes   */
   unsigned short int bits;         /* Bits per pixel            */
   unsigned int compression;        /* Compression type          */
   unsigned int imagesize;          /* Image size in bytes       */
   int xresolution,yresolution;     /* Pixels per meter          */
   unsigned int ncolours;           /* Number of colours         */
   unsigned int importantcolours;   /* Important colours         */
} BITMAP_INFOHEADER;

#pragma pack (pop)


	//these throw ImFormatExcep
void BMPDecoder::decode(const std::vector<unsigned char>& encoded_img,//const void* encoded_image, int numencodedimagebytes,
						Bitmap& bitmap_out)
{

	int srcindex = 0;
	//-----------------------------------------------------------------
	//read bitmap header
	//-----------------------------------------------------------------
	if(encoded_img.size() < sizeof(BITMAP_HEADER))
		throw ImFormatExcep("numimagebytes < sizeof(BITMAP_HEADER)");


	BITMAP_HEADER header;
	const int header_size = sizeof(BITMAP_HEADER);
	assert(header_size == 14);

	memcpy(&header, &encoded_img[srcindex], sizeof(BITMAP_HEADER));
	srcindex += sizeof(BITMAP_HEADER);

	
	//-----------------------------------------------------------------
	//read bitmap info-header
	//-----------------------------------------------------------------
	assert(sizeof(BITMAP_INFOHEADER) == 40);
	BITMAP_INFOHEADER infoheader;

	if(encoded_img.size() < sizeof(BITMAP_HEADER) + sizeof(BITMAP_INFOHEADER))
		throw ImFormatExcep("EOF before BITMAP_INFOHEADER");

	memcpy(&infoheader, &encoded_img[srcindex], sizeof(BITMAP_INFOHEADER));
	srcindex += sizeof(BITMAP_INFOHEADER);



	if(infoheader.bits != 24)
		throw ImFormatExcep("unsupported bitdepth of " + ::toString(infoheader.bits));

//	debugPrint("number of planes: %i\n", infoheader.planes);
//	debugPrint("compression mode: %i\n", infoheader.compression);

	const int width = infoheader.width;
	const int height = infoheader.height;
	const int bytespp = 3;

	const int MAX_DIMS = 10000;
	if(width < 0 || width > MAX_DIMS) 
		throw ImFormatExcep("bad image width.");
	if(height < 0 || height > MAX_DIMS) 
		throw ImFormatExcep("bad image height.");

	bitmap_out.setBytesPP(bytespp);
	bitmap_out.resize(width, height);

	int rowpaddingbytes = 4 - ((width*3) % 4);
	if(rowpaddingbytes == 4)
		rowpaddingbytes = 0;

	//-----------------------------------------------------------------
	//convert into colour array
	//-----------------------------------------------------------------
	//int i = 0;
	for(int y=infoheader.height-1; y>=0; --y)
	{
		int i = y * width * bytespp;//destination pixel index at start of row
		for(int x=0; x<infoheader.width; ++x)
		{
			assert(srcindex >= 0 && srcindex + 2 < (int)encoded_img.size());
			assert(i >= 0 && i + 2 < bitmap_out.getWidth() * bitmap_out.getHeight() * bitmap_out.getBytesPP());

			unsigned char r, g, b;
			b = encoded_img[srcindex++];
			g = encoded_img[srcindex++];
			r = encoded_img[srcindex++];

			bitmap_out.getData()[i++] = r;
			bitmap_out.getData()[i++] = g;
			bitmap_out.getData()[i++] = b;
			/*bitmap_out.getData()[i++] = encoded_img[srcindex++];
			bitmap_out.getData()[i++] = encoded_img[srcindex++];
			bitmap_out.getData()[i++] = encoded_img[srcindex++];*/
		}
		//i += rowpaddingbytes;
		srcindex += rowpaddingbytes;
	}

}


void BMPDecoder::encode(const Bitmap& bitmap, std::vector<unsigned char>& encoded_img_out)
{
	throw ImFormatExcep("saving BMPs unsupported.");
}





