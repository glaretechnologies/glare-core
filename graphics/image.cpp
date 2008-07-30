#include "image.h"

//#pragma warning(disable : 4290)//disable exception specification warning in VS2003


#include <stdio.h>
#include <fstream>
#include "../indigo/globals.h"
#include "../utils/stringutils.h"
#include "../maths/vec2.h"
#include <assert.h>
#include <limits>
#include "../indigo/IndigoImage.h"
#include "MitchellNetravali.h"
#include "BoxFilterFunction.h"

#ifndef BASIC_IMAGE

#ifdef OPENEXR_SUPPORT
#include <ImfRgbaFile.h>
#include <ImathBox.h>
#endif

#include <png.h>
extern "C"
{
#include "../hdr/rgbe.h"
}
#endif

Image::Image()
:	pixels(0, 0)
{
	width = 0;
	height = 0;
}

Image::Image(unsigned int width_, unsigned int height_)
:	pixels(width_, height_)
{
	width = width_;
	height = height_;
}



Image::~Image()
{}



Image& Image::operator = (const Image& other)
{
	if(&other == this)
		return *this;

	if(width != other.width || height != other.height)
	{
		width = other.width;
		height = other.height;

		pixels.resize(width, height);
	}

	for(unsigned int x=0; x<width; ++x)
		for(unsigned int y=0; y<height; ++y)
		{
			setPixel(x, y, other.getPixel(x, y));
		}

	return *this;
}


// will throw ImageExcep if bytespp != 3
void Image::setFromBitmap(const Bitmap& bmp) 
{
	if(bmp.getBytesPP() != 3)
		throw ImageExcep("BytesPP != 3");

	resize(bmp.getWidth(), bmp.getHeight());

	const float factor = 1.0f / 255.0f;
	for(unsigned int y=0; y<bmp.getHeight(); ++y)
		for(unsigned int x=0; x<bmp.getWidth(); ++x)
		{
			setPixel(x, y, 
				Colour3f(
					(float)bmp.getPixel(x, y)[0] * factor,
					(float)bmp.getPixel(x, y)[1] * factor,
					(float)bmp.getPixel(x, y)[2] * factor
					)
				);
		}
}

// Will throw ImageExcep if bytespp != 3 && bytespp != 4
void Image::copyRegionToBitmap(Bitmap& bmp_out, int x1, int y1, int x2, int y2) const 
{
	if(bmp_out.getBytesPP() != 3 && bmp_out.getBytesPP() != 4)
		throw ImageExcep("BytesPP != 3");

	if(x1 < 0 || y1 < 0 || x1 >= x2 || y1 >= y2 || x2 > (int)getWidth() || y2 > (int)getHeight())
		throw ImageExcep("Region coordinates are invalid");

	const int out_width = x2 - x1;
	const int out_height = y2 - y1;

	bmp_out.resize(out_width, out_height, bmp_out.getBytesPP());

	for(int y=y1; y<y2; ++y)
		for(int x=x1; x<x2; ++x)
		{
			unsigned char* pixel = bmp_out.getPixelNonConst(x - x1, y - y1);
			pixel[0] = (unsigned char)(getPixel(x, y).r * 255.0f);
			pixel[1] = (unsigned char)(getPixel(x, y).g * 255.0f);
			pixel[2] = (unsigned char)(getPixel(x, y).b * 255.0f);
		}
}





typedef unsigned char BYTE;


static inline float byteToFloat(BYTE c)
{
	return (float)c * (1.0f / 255.0f);
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




//Image* Image::loadFromBitmap(const std::string& pathname)
void Image::loadFromBitmap(const std::string& pathname)
{
	//-----------------------------------------------------------------
	//open file
	//-----------------------------------------------------------------
	FILE* f = fopen(pathname.c_str(), "rb");//create a file

	if(!f)
		throw ImageExcep("could not open file '" + pathname + "'.");

	
	//-----------------------------------------------------------------
	//read bitmap header
	//-----------------------------------------------------------------
	BITMAP_HEADER bitmap_header;
	const int header_size = sizeof(BITMAP_HEADER);
	assert(header_size == 14);

	fread(&bitmap_header, 14, 1, f);

	//debugPrint("file size: %i\n", (int)bitmap_header.size);
	//debugPrint("offset to image data in bytes: %i\n", (int)bitmap_header.offset);


	//-----------------------------------------------------------------
	//read bitmap info-header
	//-----------------------------------------------------------------
	assert(sizeof(BITMAP_INFOHEADER) == 40);
	
	BITMAP_INFOHEADER infoheader;

	fread(&infoheader, sizeof(BITMAP_INFOHEADER), 1, f);

//	debugPrint("width: %i\n", infoheader.width);
//	debugPrint("height: %i\n", infoheader.height);
//	debugPrint("bits per pixel: %i\n", (int)infoheader.bits);

	assert(infoheader.bits == 24);
	if(infoheader.bits != 24)
		throw ImageExcep("unsupported bitdepth.");

//	debugPrint("number of planes: %i\n", infoheader.planes);
//	debugPrint("compression mode: %i\n", infoheader.compression);




	//-----------------------------------------------------------------
	//build image object
	//-----------------------------------------------------------------
	//Image* image = new Image(infoheader.width, infoheader.height);


	this->width = infoheader.width;
	this->height = infoheader.height;

	const int MAX_DIMS = 10000;
	if(width < 0 || width > MAX_DIMS) 
		throw ImageExcep("bad image width.");
	if(height < 0 || height > MAX_DIMS) 
		throw ImageExcep("bad image height.");

	pixels.resize(width, height);

	int rowpaddingbytes = 4 - ((width*3) % 4);
	if(rowpaddingbytes == 4)
		rowpaddingbytes = 0;

	const int DATASIZE = infoheader.height * (infoheader.width * 3 + rowpaddingbytes);

	BYTE* data = new BYTE[DATASIZE];

	fread(data, DATASIZE, 1, f);

	//-----------------------------------------------------------------
	//convert into colour array
	//-----------------------------------------------------------------
	int i = 0;
	for(int y=infoheader.height-1; y>=0; --y)
	{
		for(int x=0; x<infoheader.width; ++x)
		{
			//const float r = byteToFloat(data[i]);
			//const float g = byteToFloat(data[i + 1]);
			//const float b = byteToFloat(data[i + 2]);
			const float b = byteToFloat(data[i]);
			const float g = byteToFloat(data[i + 1]);
			const float r = byteToFloat(data[i + 2]);

			this->setPixel(x, y, ColourType(r, g, b));

			i += 3;
		}

		i += rowpaddingbytes;
	}
		

	delete[] data;



	//-----------------------------------------------------------------
	//close file
	//-----------------------------------------------------------------
	fclose (f);
}






void Image::saveToBitmap(const std::string& pathname)
{
	FILE* f = fopen(pathname.c_str(), "wb");
	assert(f);

	if(!f)
		throw ImageExcep("could not open file");

	BITMAP_HEADER bitmap_header;
	bitmap_header.offset = sizeof(BITMAP_HEADER) + sizeof(BITMAP_INFOHEADER);
	bitmap_header.type = BITMAP_ID;
	bitmap_header.size = sizeof(BITMAP_HEADER) + sizeof(BITMAP_INFOHEADER) + getWidth() * getHeight() * 3;

	fwrite(&bitmap_header, sizeof(BITMAP_HEADER), 1, f);

	BITMAP_INFOHEADER bitmap_infoheader;
	bitmap_infoheader.size = sizeof(BITMAP_INFOHEADER);
	bitmap_infoheader.bits = 24;
	bitmap_infoheader.compression = 0;
	bitmap_infoheader.height = getHeight();
	bitmap_infoheader.width = getWidth();
	bitmap_infoheader.imagesize = 0;//getWidth() * getHeight() * 3;
	bitmap_infoheader.importantcolours = 0;//1 << 24;
	bitmap_infoheader.ncolours = 0;//1 << 24;
	bitmap_infoheader.planes = 1;
	bitmap_infoheader.xresolution = 100;
	bitmap_infoheader.yresolution = 100;

	fwrite(&bitmap_infoheader, sizeof(BITMAP_INFOHEADER), 1, f);


	int rowpaddingbytes = 4 - ((width*3) % 4);
	if(rowpaddingbytes == 4)
		rowpaddingbytes = 0;


	for(int y=getHeight()-1; y>=0; --y)
	{
		for(int x=0; x<getWidth(); ++x)
		{
			ColourType pixelcol = getPixel(x, y);
			pixelcol.positiveClipComponents();

			const BYTE b = (BYTE)(pixelcol.b * 255.0f);
			fwrite(&b, 1, 1, f);

			const BYTE g = (BYTE)(pixelcol.g * 255.0f);
			fwrite(&g, 1, 1, f);
		
			const BYTE r = (BYTE)(pixelcol.r * 255.0f);
			fwrite(&r, 1, 1, f);

		
			/*const BYTE r = (BYTE)(getPixel(x, y).r * 255.0f);
			fwrite(&r, 1, 1, f);
			const BYTE g = (BYTE)(getPixel(x, y).g * 255.0f);
			fwrite(&g, 1, 1, f);
			const BYTE b = (BYTE)(getPixel(x, y).b * 255.0f);
			fwrite(&b, 1, 1, f);*/

		}

		const BYTE zerobyte = 0;
		for(int i=0; i<rowpaddingbytes; ++i)
			fwrite(&zerobyte, 1, 1, f);
	}

	fclose(f);
}


//TODO: error handling
void Image::loadFromRAW(const std::string& pathname, int width_, int height_,
		float load_gain)
{

	std::ifstream file(pathname.c_str(), std::ios::binary);
	if(!file)
		throw ImageExcep("Failed to open file '" + pathname + "' for reading.");

	width = width_;
	height = height_;
	
	pixels.resize(width, height);

	//const float MAX_VAL = 1.0f;

	for(int y=0; y<height; ++y)
		for(int x=0; x<width; ++x)
		{
			const int destx = x;
			const int desty = height - 1 - y;//invert vertically

			float val;
			
			file.read((char*)&val, 4);
			pixels.elem(destx, desty).r = val * load_gain;

			file.read((char*)&val, 4);
			pixels.elem(destx, desty).g = val * load_gain;

			file.read((char*)&val, 4);
			pixels.elem(destx, desty).b = val * load_gain;

			//pixels.elem(destx, desty).positiveClipComponents();

			//assert(file.good());
		}
	
	if(!file.good())
		throw ImageExcep("Error encountered while reading RAW image.");
}

/*
void Image::loadFromNFF(const std::string& pathname)
{
	
	std::vector<float> imgdata;

	try
	{
		NFFio::readNNF(pathname, imgdata, width, height);

		//resize this image
		pixels.resize(width, height);

		for(int x=0; x<width; ++x)
			for(int y=0; y<height; ++y)
			{
				int srcpixelindex = (x + y*width)*3;

				setPixel(x, y, ColourType(imgdata[srcpixelindex],
										imgdata[srcpixelindex+1],
										imgdata[srcpixelindex+2]));
			}
	}
	catch(NFFioExcep& e)
	{
		throw ImageExcep("NFFioExcep: " + e.what());
	}									
}


void Image::saveAsNFF(const std::string& pathname)
{
	std::vector<float> imgdata(width * height * 3);
	for(int x=0; x<width; ++x)
		for(int y=0; y<height; ++y)
		{
			imgdata[(x + y*width)*3] = getPixel(x, y).r;
			imgdata[(x + y*width)*3+1] = getPixel(x, y).g;
			imgdata[(x + y*width)*3+2] = getPixel(x, y).b;
		}

	try
	{
		NFFio::writeNFF(pathname, imgdata, width, height);
	}
	catch(NFFioExcep& e)
	{
		throw ImageExcep("NFFioExcep: " + e.what());
	}
}*/

void Image::zero()
{
	for(int x=0; x<width; ++x)
		for(int y=0; y<height; ++y)
			setPixel(x, y, ColourType(0,0,0));
}

void Image::resize(unsigned int newwidth, unsigned int newheight)
{
	if(width == newwidth && height == newheight)
		return;

	width = newwidth;
	height = newheight;

	pixels.resize(width, height);
}

void Image::posClamp()
{
	for(int x=0; x<width; ++x)
		for(int y=0; y<height; ++y)
			getPixel(x, y).positiveClipComponents();
}

void Image::clampInPlace(float min, float max)
{
	for(unsigned int i=0; i<numPixels(); ++i)
		getPixel(i).clampInPlace(min, max);
}


void Image::gammaCorrect(float exponent)
{
	for(int x=0; x<width; ++x)
		for(int y=0; y<height; ++y)
		{
			ColourType& colour = getPixel(x, y);

			colour.r = pow(colour.r, exponent);
			colour.g = pow(colour.g, exponent);
			colour.b = pow(colour.b, exponent);
		}
}

void Image::scale(float factor)
{
	for(int x=0; x<width; ++x)
		for(int y=0; y<height; ++y)
		{
			getPixel(x, y) *= factor;
		}
}


void Image::blitToImage(Image& dest, int destx, int desty) const
{
	for(int y=0; y<getHeight(); ++y)
		for(int x=0; x<getWidth(); ++x)		
		{
			const int dx = x + destx;
			const int dy = y + desty;
			if(dx >= 0 && dx < dest.getWidth() && dy >= 0 && dy < dest.getHeight())
				dest.setPixel(dx, dy, getPixel(x, y));
		}
}

void Image::blitToImage(int src_start_x, int src_start_y, int src_end_x, int src_end_y, Image& dest, int destx, int desty) const
{
	src_start_x = myMax(0, src_start_x);
	src_start_y = myMax(0, src_start_y);

	src_end_x = myMin(src_end_x, (int)getWidth());
	src_end_y = myMin(src_end_y, (int)getHeight());

	for(int y=src_start_y; y<src_end_y; ++y)
		for(int x=src_start_x; x<src_end_x; ++x)		
		{
			const int dx = (x - src_start_x) + destx;
			const int dy = (y - src_start_y) + desty;
			if(dx >= 0 && dx < dest.getWidth() && dy >= 0 && dy < dest.getHeight())
				dest.setPixel(dx, dy, getPixel(x, y));
		}
}

void Image::addImage(const Image& img, int destx, int desty)
{	
	for(int y=0; y<img.getHeight(); ++y)
		for(int x=0; x<img.getWidth(); ++x)		
		{
			const int dx = x + destx;
			const int dy = y + desty;
			if(dx >= 0 && dx < getWidth() && dy >= 0 && dy < getHeight())
				getPixel(dx, dy) += img.getPixel(x, y);
		}
}

void Image::blendImage(const Image& img, int destx, int desty)
{	
	for(int y=0; y<img.getHeight(); ++y)
		for(int x=0; x<img.getWidth(); ++x)		
		{
			const int dx = x + destx;
			const int dy = y + desty;
			if(dx >= 0 && dx < getWidth() && dy >= 0 && dy < getHeight())
			{
				setPixel(dx, dy, Colour3f(1.0) * img.getPixel(x, y).r + getPixel(dx, dy) * (1.0 - img.getPixel(x, y).r));
			}
		}
}

void Image::subImage(const Image& img, int destx, int desty)
{	
	for(int y=0; y<img.getHeight(); ++y)
		for(int x=0; x<img.getWidth(); ++x)		
		{
			int dx = x + destx;
			int dy = y + desty;
			if(dx >= 0 && dx < getWidth() && dy >= 0 && dy < getHeight())
				getPixel(dx, dy).addMult(img.getPixel(x, y), -1.0f);//NOTE: slow :)
		}
}

void Image::overwriteImage(const Image& img, int destx, int desty)
{
	for(int y=0; y<img.getHeight(); ++y)
		for(int x=0; x<img.getWidth(); ++x)		
		{
			const int dx = x + destx;
			const int dy = y + desty;
			if(dx >= 0 && dx < getWidth() && dy >= 0 && dy < getHeight())
				setPixel(dx, dy, img.getPixel(x, y));
		}
}



/*
const Image::ColourType Image::sample(float u, float v) const
{
	int ut = (int)u;
	int vt = (int)v;

	const float ufrac = u - (float)ut;//(float)floor(u);
	const float vfrac = v - (float)vt;//(float)floor(v);
	const float oneufrac = (1.0f - ufrac);
	const float onevfrac = (1.0f - vfrac);



	if(ut < 0) ut = 0;
	if(vt < 0) vt = 0;
	if(ut + 1 >= getWidth()) ut = getWidth() - 2;
	if(vt + 1 >= getHeight()) vt = getHeight() - 2;

		//const float sum = (ufrac * vfrac) + (ufrac * onevfrac) + 
		//	(oneufrac * onevfrac) + (oneufrac * vfrac);

	ColourType colour_out(0,0,0);
	colour_out.addMult(getPixel(ut, vt), oneufrac * onevfrac);
	colour_out.addMult(getPixel(ut, vt+1), oneufrac * vfrac);
	colour_out.addMult(getPixel(ut+1, vt+1), ufrac * vfrac);
	colour_out.addMult(getPixel(ut+1, vt), ufrac * onevfrac);

	assert(colour_out.r >= 0.0 && colour_out.g >= 0.0 && colour_out.b >= 0.0);

	return colour_out;
}*/
/*
const Image::ColourType Image::sampleTiled(float u, float v) const
{
	double intpart;
	if(u < 0)
	{
		u = -u;

		u = (float)modf(u / (float)width, &intpart);//u is normed after this

		u = 1.0f - u;
		u *= (float)width;
	}
	else
	{
		u = (float)modf(u / (float)width, &intpart);//u is normed after this
		u *= (float)width;
	}

	if(v < 0)
	{
		v = -v;

		v = (float)modf(v / (float)height, &intpart);//v is normed after this

		v = 1.0f - v;
		v *= (float)height;
	}
	else
	{
		v = (float)modf(v / (float)height, &intpart);//v is normed after this
		v *= (float)width;
	}

	int ut = (int)u;//NOTE: redundant
	int vt = (int)v;

	const float ufrac = u - (float)ut;//(float)floor(u);
	const float vfrac = v - (float)vt;//(float)floor(v);
	const float oneufrac = (1.0f - ufrac);
	const float onevfrac = (1.0f - vfrac);



	if(ut < 0) ut = 0;
	if(vt < 0) vt = 0;
	if(ut + 1 >= getWidth()) ut = getWidth() - 2;
	if(vt + 1 >= getHeight()) vt = getHeight() - 2;

		//const float sum = (ufrac * vfrac) + (ufrac * onevfrac) + 
		//	(oneufrac * onevfrac) + (oneufrac * vfrac);

	ColourType colour_out(0,0,0);
	colour_out.addMult(getPixel(ut, vt), oneufrac * onevfrac);
	colour_out.addMult(getPixel(ut, vt+1), oneufrac * vfrac);
	colour_out.addMult(getPixel(ut+1, vt+1), ufrac * vfrac);
	colour_out.addMult(getPixel(ut+1, vt), ufrac * onevfrac);

	return colour_out;
}*/

#ifndef BASIC_IMAGE



#ifdef OPENEXR_SUPPORT
void Image::saveToExr(const std::string& pathname) const
{
	try
	{
		Imf::RgbaOutputFile file(pathname.c_str(), getWidth(), getHeight(), Imf::WRITE_RGBA);

		std::vector<Imf::Rgba> rgba(getWidth() * getHeight());
		//Imf::Rgba* rgba = new Imf::Rgba[getWidth() * getHeight()];

		//------------------------------------------------------------------------
		//fill in the data
		//------------------------------------------------------------------------
		for(int y=0; y<getHeight(); ++y)
			for(int x=0; x<getWidth(); ++x)
			{
				rgba[y*getWidth() + x].r = this->getPixel(x, y).r;
				rgba[y*getWidth() + x].g = this->getPixel(x, y).g;
				rgba[y*getWidth() + x].b = this->getPixel(x, y).b;
				rgba[y*getWidth() + x].a = 1.0f;
			}

		file.setFrameBuffer(&rgba[0], 1, getWidth());

		//Imf::Rgba p(1.0f, 1.0f, 1.0f);
		//file.setFrameBuffer(&p, 1, 1);
	
		file.writePixels(getHeight());

	}
	catch(const std::exception& e)
	{
		throw ImageExcep("Error writing EXR file: " + std::string(e.what()));
	}
}

void Image::loadFromExr(const std::string& pathname)
{
	try
	{
		Imf::RgbaInputFile file(pathname.c_str());
		const Imath::Box2i dw = file.dataWindow();

		const int filewidth = dw.max.x - dw.min.x + 1;
		const int fileheight = dw.max.y - dw.min.y + 1;

		this->resize(filewidth, fileheight);

		std::vector<Imf::Rgba> data(filewidth * fileheight);

		file.setFrameBuffer(&data[0], 1, filewidth); //&pixels[0][0] - dw.min.x - dw.min.y * width, 1, width);
		file.readPixels(dw.min.y, dw.max.y);

		for(int y=0; y<getHeight(); ++y)
			for(int x=0; x<getWidth(); ++x)
			{
				this->getPixel(x, y).r = data[y*getWidth() + x].r;
				this->getPixel(x, y).g = data[y*getWidth() + x].g;
				this->getPixel(x, y).b = data[y*getWidth() + x].b;

				this->getPixel(x, y).lowerClampInPlace(0.0);//TEMP NEW

				assert(this->getPixel(x, y).r >= 0.0);
				//printVar(this->getPixel(x, y).g);
				assert(this->getPixel(x, y).g >= 0.0);
				assert(this->getPixel(x, y).b >= 0.0);
			}
	}
	catch(const std::exception& e)
	{
		throw ImageExcep("Error reading EXR file: " + std::string(e.what()));
	}
}
#else //OPENEXR_SUPPORT

void Image::saveToExr(const std::string& pathname) const
{
	::fatalError("Image::saveToExr: OPENEXR_SUPPORT disabled.");
}

void Image::loadFromExr(const std::string& pathname)
{
	::fatalError("Image::saveToExr: OPENEXR_SUPPORT disabled.");
}

#endif //OPENEXR_SUPPORT




//typedef void (PNGAPI *png_error_ptr) PNGARG((png_structp, png_const_charp));
/*
void error_func(png_structp png, const char* msg)
{
	throw ImageExcep("LibPNG error: " + std::string(msg));

}

void warning_func(png_structp png, const char* msg)
{
	throw ImageExcep("LibPNG warning: " + std::string(msg));
}

void Image::saveToPng(const std::string& pathname, const std::map<std::string, std::string>& metadata, int border_width) const
{
	assert(border_width >= 0);
	assert(getWidth() >= border_width * 2);
	assert(getHeight() >= border_width * 2);

	// open the file
	FILE* fp = fopen(pathname.c_str(), "wb");
	if(fp == NULL)
		throw ImageExcep("Failed to open '" + pathname + "' for writing.");
	

	//Create and initialize the png_struct with the desired error handler functions.
	png_struct* png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
		const_cast<Image*>(this), //error user pointer
		error_func, //error func
		warning_func);//warning func

	if(!png)
		throw ImageExcep("Failed to create PNG object.");

	png_info* info = png_create_info_struct(png);

	if(!info)
	{
		png_destroy_write_struct(&png, (png_infop*)NULL);//free png struct

		throw ImageExcep("Failed to create PNG info object.");
	}
	
	// set up the output control if you are using standard C stream
	png_init_io(png, fp);

	//------------------------------------------------------------------------
	//set some image info
	//------------------------------------------------------------------------
	png_set_IHDR(png, info, getWidth() - border_width*2, getHeight() - border_width*2,
       8, //bit depth of each channel
	   PNG_COLOR_TYPE_RGB, //colour type
	   PNG_INTERLACE_NONE, //interlace type
	   PNG_COMPRESSION_TYPE_BASE,//PNG_COMPRESSION_TYPE_DEFAULT, //compression type
	   PNG_FILTER_TYPE_BASE);//PNG_FILTER_TYPE_DEFAULT);//filter method

	//------------------------------------------------------------------------
	// Write metadata pairs if present
	//------------------------------------------------------------------------
	if(metadata.size() > 0)
	{
		png_text* txt = new png_text[metadata.size()];
		memset(txt, 0, sizeof(png_text)*metadata.size());
		int c = 0;
		for(std::map<std::string, std::string>::const_iterator i = metadata.begin(); i != metadata.end(); ++i)
		{
			txt[c].compression = PNG_TEXT_COMPRESSION_NONE;
			txt[c].key = (png_charp)(*i).first.c_str();
			txt[c].text = (png_charp)(*i).second.c_str();
			txt[c].text_length = (*i).second.length();
			c++;
		}
		png_set_text(png, info, txt, (int)metadata.size());
		delete[] txt;
	}

	//png_set_gAMA(png_ptr, info_ptr, 2.3);

	//write info
	png_write_info(png, info);


	//------------------------------------------------------------------------
	//write image rows
	//------------------------------------------------------------------------
	std::vector<unsigned char> rowdata(getWidth() * 3);

	for(int y=border_width; y<getHeight() - border_width; ++y)
	{
		//------------------------------------------------------------------------
		//copy floating point data to 8bpp format
		//------------------------------------------------------------------------
		for(int x=border_width; x<getWidth() - border_width; ++x)
			for(int i=0; i<3; ++i)
				rowdata[(x - border_width)*3 + i] = (unsigned char)(getPixel(x, y)[i] * 255.0f);

		//------------------------------------------------------------------------
		//write it
		//------------------------------------------------------------------------
		png_bytep row_pointer = (png_bytep)&(*rowdata.begin());

		png_write_row(png, row_pointer);
	}

	//------------------------------------------------------------------------
	//finish writing file
	//------------------------------------------------------------------------
	png_write_end(png, info);

	//------------------------------------------------------------------------
	//free structs
	//------------------------------------------------------------------------
	png_destroy_write_struct(&png, &info);

	//------------------------------------------------------------------------
	//close the file
	//------------------------------------------------------------------------
	fclose(fp);
}*/

#endif

//make the average pixel luminance == 1
void Image::normalise()
{
	if(height == 0 || width == 0)
		return;

	double av_lum = 0.0;
	for(unsigned int i=0; i<numPixels(); ++i)
		av_lum += (double)getPixel(i).luminance();
	av_lum /= (double)(numPixels());
	
	const float factor = (float)(1.0 / av_lum);
	for(unsigned int i=0; i<numPixels(); ++i)
		getPixel(i) *= factor;
}



void Image::loadFromHDR(const std::string& pathname, int width_, int height_)
{
	/*width = width_;
	height = height_; 

	std::ifstream file(pathname.c_str(), std::ios_base::binary);
	if(!file)
		throw ImageExcep("Failed to open file '" + pathname + "' for reading.");

	//rgbe_header_info header_info;
	//char magicnum[2];
	//file.read(magicnum, 2);
	//assert(magicnum[0] = '#');
	//assert(magicnum[1] = '?');
	//file >> header_info.valid;
	//char buf[128];
	//file.read(buf, sizeof(buf));
	//conPrint(buf);
	std::string head;
	char c;
	while(1)
	{
		file >> c;
		if(c == '\0')
			break;
		::concatWithChar(head, c);
	}
	conPrint(head);
	//file >> header_info.gamma;
	//file >> header_info.exposure;

	//TEMP:
	file.seekg(offset, std::ios_base::cur);
	offset += 16;

	//------------------------------------------------------------------------
	//copy data to image buf
	//------------------------------------------------------------------------
	pixels.resize(width, height);
	unsigned int i = 0;
	for(int x=0; x<width; ++x)
		for(int y=0; y<height; ++y)	
		{
			unsigned char rgbe[4];
			//file.get((char*)rgbe, 4);
			//unsigned int dat;
			file.read((char*)rgbe, 4);

			//const int rawr = //(int)rgbe[0];
			//const int rawg = //(int)rgbe[1];
			//const int rawb = //(int)rgbe[2];
			const int rawexp = (int)rgbe[3];

			if(rawexp != 0)
				int a = 9;

			const float factor = 1.0f / 255.0f;//pow(2.0f, (float)(int)rgbe[3] - (int)(128+8));

			ColourType col((float)rgbe[0] * factor, (float)rgbe[1] * factor, (float)rgbe[2] * factor);
			pixels.elem(x, y) = col;

			//assert(i+2 < data.size());
			
			//pixels.elem(x, y).set(data[i], data[i+1], data[i+2]);
			//i += 3;

			//if(!file.good())
			//	throw ImageExcep("Error while reading file");
		}*/


	/*FILE* f = fopen(pathname.c_str(), "rb");
	if(!f)
		throw ImageExcep("Failed to open file '" + pathname + "' for reading.");

	width = width_;
	height = height_; 
	int result;
	rgbe_header_info header_info;
	result = RGBE_ReadHeader(f, &width, &height, &header_info);
	if(result != RGBE_RETURN_SUCCESS)
	{
		fclose(f);
		throw ImageExcep("RGBE_ReadHeader failed while reading file '" + pathname + "'.");
	}

	std::vector<float> data(3 * width * height);

	//image = (float *)malloc(sizeof(float)*3*image_width*image_height);

	result = RGBE_ReadPixels_RLE(f, &(*data.begin()), width, height);
	if(result != RGBE_RETURN_SUCCESS)
	{
		fclose(f);
		throw ImageExcep("RGBE_ReadPixels_RLE failed while processing file '" + pathname + "'.");
	}

	//------------------------------------------------------------------------
	//copy data to image buf
	//------------------------------------------------------------------------
	pixels.resize(width, height);
	unsigned int i = 0;
	for(int y=0; y<height; ++y)
		for(int x=0; x<width; ++x)
		{
			assert(i+2 < data.size());
			pixels.elem(x, y).set(data[i], data[i+1], data[i+2]);
			i += 3;
		}


	fclose(f);*/
}

/*void Image::collapseSizeBoxFilter(int factor, int border_width)
{
	BoxFilterFunction box_filter_func;
	this->collapseImage(factor, border_width, box_filter_func);
}*/


// trims off border before collapsing
void Image::collapseSizeBoxFilter(int factor/*, int border_width*/)
{
	//assert(border_width >= 0);
	//assert(width > border_width * 2);
	//assert(width > border_width * 2);
	//assert((width - (border_width * 2)) % factor == 0);

	//Image out((width - (border_width * 2)) / factor, (height - (border_width * 2)) / factor);

	//const float scale_factor = 1.0f / (float)(factor * factor);

	//for(int y=0; y<out.getHeight(); ++y)
	//	for(int x=0; x<out.getWidth(); ++x)
	//	{
	//		Colour3f c(0.f, 0.f, 0.f);

	//		for(int s=0; s<factor; ++s)
	//			for(int t=0; t<factor; ++t)
	//				c += getPixel(x*factor + s + border_width, y*factor + t + border_width);

	//		out.getPixel(x, y) = c * scale_factor;
	//	}

	//*this = out;

	BoxFilterFunction ff;
	this->collapseImage(
		factor,
		0, // border
		ff
		);
}

/*

// trims off border before collapsing
void Image::collapseSizeMitchellNetravali(int factor, int border_width, double B, double C)
{
	assert(border_width >= 0);
	assert(width > border_width * 2);
	assert(width > border_width * 2);
	assert((width - (border_width * 2)) % factor == 0);

	MitchellNetravali mn(
		B, // B = blur
		C // C = ring
		);

	Image out((width - (border_width * 2)) / factor, (height - (border_width * 2)) / factor);

	const float scale_factor = 1.0f / (float)(factor * factor);

	// Construct filter
	//const int rad = 4;
	const int filter_width = 5 * factor;//rad * factor + 1;
	Array2d<float> filter(filter_width, filter_width);

	// Filter center coordinates in big pixels
	const double center_pos_x = 2.5; 
	const double center_pos_y = 2.5;

	for(int y=0; y<filter_width; ++y)
	{
		const double pos_y = ((double)y + 0.5) / (double)factor; // y coordinate in big pixels
		const double abs_dy = fabs(pos_y - center_pos_y);

		for(int x=0; x<filter_width; ++x)
		{
			const double pos_x = ((double)x + 0.5) / (double)factor; // y coordinate in big pixels
			const double abs_dx = fabs(pos_x - center_pos_x);

			//filter.elem(x, y) = Maths::mitchellNetravali(abs_dx) * Maths::mitchellNetravali(abs_dy) * scale_factor;
			filter.elem(x, y) = mn.eval(abs_dx) * mn.eval(abs_dy) * scale_factor;
		}
	}



	const int rad = factor * 2;

	// For each pixel of result image
	for(int y=0; y<out.getHeight(); ++y)
	{
		const int src_y_center = y * factor;
		const int src_y_min = myMax(0, src_y_center - (2 * factor));
		const int src_y_max = myMin(this->getHeight(), src_y_center + (3 * factor));

		//if(y % 50 == 0)
		//	printVar(y);

		for(int x=0; x<out.getWidth(); ++x)
		{
			const int src_x_center = x * factor; 
			const int src_x_min = myMax(0, src_x_center - (2 * factor));
			const int src_x_max = myMin(this->getWidth(), src_x_center + (3 * factor));

			Colour3f c(0.0f);

			// For each pixel in filter support of source image
			for(int sy=src_y_min; sy<src_y_max; ++sy)
			{
				const int filter_y = (sy - src_y_center) + rad;
				assert(filter_y >= 0 && filter_y < filter_width);		

				for(int sx=src_x_min; sx<src_x_max; ++sx)
				{
					const int filter_x = (sx - src_x_center) + rad;
					assert(filter_x >= 0 && filter_x < filter_width);	

					assert(this->getPixel(sx, sy).r >= 0.0 && this->getPixel(sx, sy).g >= 0.0 && this->getPixel(sx, sy).b >= 0.0);
					assert(isFinite(this->getPixel(sx, sy).r) && isFinite(this->getPixel(sx, sy).g) && isFinite(this->getPixel(sx, sy).b));

					c.addMult(this->getPixel(sx, sy), filter.elem(filter_x, filter_y));
				}
			}


			//assert(c.r >= 0.0 && c.g >= 0.0 && c.b >= 0.0);
			assert(isFinite(c.r) && isFinite(c.g) && isFinite(c.b));

			// Make sure components can't go below zero
			c.clamp(0.0f, 1.0f);

			out.setPixel(x, y, c);
		}
	}



	*this = out;
}
*/




void Image::collapseImage(int factor, int border_width, const FilterFunction& filter_function)
{
	assert(border_width >= 0);
	assert(width > border_width * 2);
	assert(width > border_width * 2);
	assert((width - (border_width * 2)) % factor == 0);

	Image out((width - (border_width * 2)) / factor, (height - (border_width * 2)) / factor);

	//const double scale_factor = 1.0f / (float)(factor * factor);

	// Construct filter
	/*const int filter_width = ((int)ceil(filter_function.supportRadius()) * 2 + 1) * factor;
	Array2d<float> filter(filter_width, filter_width);

	// Filter center coordinates in big pixels
	const double center_pos_x = (double)(filter_width / 2) + 0.5; //2.5; 
	const double center_pos_y = (double)(filter_width / 2) + 0.5; //2.5;*/

	const double radius_src = filter_function.supportRadius() * (double)factor;

	//const int neg_rad_src = (int)floor(radius_src);
	//const int pos_rad_src = (int)ceil(radius_src);

	const int filter_width = (int)ceil(radius_src * 2.0); // neg_rad_src + pos_rad_src;

	Array2d<float> filter(filter_width, filter_width);

	//double sum = 0.0;

	for(int y=0; y<filter_width; ++y)
	{
		const double pos_y = (double)y + 0.5; // y coordinate in src pixels
		const double abs_dy_src = fabs(pos_y - radius_src);

		for(int x=0; x<filter_width; ++x)
		{
			const double pos_x = (double)x + 0.5; // y coordinate in src pixels
			const double abs_dx_src = fabs(pos_x - radius_src);

			filter.elem(x, y) = filter_function.eval(abs_dx_src / (double)factor) * filter_function.eval(abs_dy_src / (double)factor) / (float)(factor * factor);

			//sum += (double)filter.elem(x, y);
		}
	}

	//printVar(sum);

	//TEMP:
	/*for(int y=0; y<filter_width; ++y)
	{
		for(int x=0; x<filter_width; ++x)
		{
			conPrintStr(toString(filter.elem(x, y)) + " ");
		}
		conPrintStr("\n");
	}*/


	/*for(int y=0; y<filter_width; ++y)
	{
		//const double pos_y = ((double)y + 0.5) / (double)factor; // y coordinate in big pixels
		//const double abs_dy = fabs(pos_y - center_pos_y);

		for(int x=0; x<filter_width; ++x)
		{
			const double pos_x = ((double)x + 0.5) / (double)factor; // y coordinate in big pixels
			const double abs_dx = fabs(pos_x - center_pos_x);

			filter.elem(x, y) = filter_function.eval(abs_dx) * filter_function.eval(abs_dy) * scale_factor;
		}
	}*/



	//const int neg_rad = factor * (filter_width / 2);
	//const int pos_rad = factor * (filter_width / 2 + 1);

	// For each pixel of result image
	int support_y = (int)floor((0.5 - filter_function.supportRadius()) * (double)factor) + border_width;

	for(int y=0; y<out.getHeight(); ++y)
	{
		/*const int src_y_center = y * factor;
		const int src_y_min = myMax(0, src_y_center - neg_rad);
		const int src_y_max = myMin(this->getHeight(), src_y_center + pos_rad);*/

		const int src_y_min = myMax(0, support_y);
		const int src_y_max = myMin((int)getHeight(), support_y + filter_width);

		//if(y % 50 == 0)
		//	printVar(y);


		int support_x = (int)floor((0.5 - filter_function.supportRadius()) * (double)factor) + border_width;

		for(int x=0; x<(int)out.getWidth(); ++x)
		{
			/*const int src_x_center = x * factor; 
			const int src_x_min = myMax(0, src_x_center - neg_rad);
			const int src_x_max = myMin(this->getWidth(), src_x_center + pos_rad);*/

			const int src_x_min = myMax(0, support_x);
			const int src_x_max = myMin((int)getWidth(), support_x + filter_width);

			Colour3f c(0.0f);

			// For each pixel in filter support of source image
			for(int sy=src_y_min; sy<src_y_max; ++sy)
			{
				const int filter_y = sy - support_y; //(sy - src_y_center) + neg_rad;
				assert(filter_y >= 0 && filter_y < filter_width);		

				for(int sx=src_x_min; sx<src_x_max; ++sx)
				{
					const int filter_x = sx - support_x; //(sx - src_x_center) + neg_rad;
					assert(filter_x >= 0 && filter_x < filter_width);	

					assert(this->getPixel(sx, sy).r >= 0.0 && this->getPixel(sx, sy).g >= 0.0 && this->getPixel(sx, sy).b >= 0.0);
					assert(isFinite(this->getPixel(sx, sy).r) && isFinite(this->getPixel(sx, sy).g) && isFinite(this->getPixel(sx, sy).b));

					c.addMult(this->getPixel(sx, sy), filter.elem(filter_x, filter_y));
				}
			}


			//assert(c.r >= 0.0 && c.g >= 0.0 && c.b >= 0.0);
			assert(isFinite(c.r) && isFinite(c.g) && isFinite(c.b));

			// Make sure components can't go below zero or above 1.0
			c.clampInPlace(0.0f, 1.0f);

			out.setPixel(x, y, c);

			support_x += factor;
		}

		support_y += factor;
	}

	*this = out;
}


/*
void Image::collapseImage(int factor, int border_width, DOWNSIZE_FILTER filter_type, double mn_B, double mn_C)
{
	if(filter_type == Image::DOWNSIZE_FILTER_BOX)
		collapseSizeBoxFilter(factor, border_width);
	else if(filter_type == Image::DOWNSIZE_FILTER_MN_CUBIC)
		collapseSizeMitchellNetravali(factor, border_width, mn_B, mn_C);
	else
	{
		assert(0);
	}
}*/


unsigned int Image::getByteSize() const
{
	return numPixels() * 3 * sizeof(float);
}

float Image::minLuminance() const
{
	float minlum = std::numeric_limits<float>::max();
	for(unsigned int i=0; i<numPixels(); ++i)
		minlum = myMin(minlum, getPixel(i).luminance());
	return minlum;
}
float Image::maxLuminance() const
{
	float maxlum = -std::numeric_limits<float>::max();
	for(unsigned int i=0; i<numPixels(); ++i)
		maxlum = myMax(maxlum, getPixel(i).luminance());
	return maxlum;
}


double Image::averageLuminance() const
{
	double sum = 0.0;
	for(unsigned int i=0; i<numPixels(); ++i)
		sum += (double)getPixel(i).luminance();
	return sum / (double)numPixels();
}


void Image::buildRGBFilter(const Image& original_filter, const Vec3d& filter_scales, Image& result_out)
{
	const double max_scale = myMax(filter_scales.x, myMax(filter_scales.y, filter_scales.z));

	const int new_filter_width = (int)((double)original_filter.getWidth() * max_scale);
	const int new_filter_height = (int)((double)original_filter.getHeight() * max_scale);
	Image filter(new_filter_width, new_filter_height);

	printVar(new_filter_width);

	const Vec3d scale_ratios(
		max_scale / filter_scales.x,
		max_scale / filter_scales.y,
		max_scale / filter_scales.z
		);

	const int SS_RES = 4;
	const double recip_ss_res = 1.0 / (double)SS_RES;
	//const double ss_weight = recip_ss_res * recip_ss_res;

	const Vec2d recip_filter_dimensions(1.0 / (double)filter.getWidth(), 1.0 / (double)filter.getHeight());

	for(int c=0; c<3; ++c)
	{
		for(int y=0; y<filter.getHeight(); ++y)
		{
			printVar(y);

			for(int x=0; x<filter.getWidth(); ++x)
			{
				float sum = 0.0f;
				for(int sy=0; sy<SS_RES; ++sy)
				{
					for(int sx=0; sx<SS_RES; ++sx)
					{
						const Vec2d normed_coords(
							((double)x + ((double)sx * recip_ss_res)) * recip_filter_dimensions.x,
							((double)y + ((double)sy * recip_ss_res)) * recip_filter_dimensions.y
							);

						assert(normed_coords.inHalfClosedInterval(0.0, 1.0));

						const Vec2d scaled_normed_coords = Vec2d(0.5, 0.5) + (normed_coords - Vec2d(0.5, 0.5)) * scale_ratios[c];

						if(scaled_normed_coords.inHalfClosedInterval(0.0, 1.0))
						{
							assert(scaled_normed_coords.inHalfClosedInterval(0.0, 1.0));
									
							//sum += original_filter.sample(
							//	scaled_normed_coords.x * (double)original_filter.getWidth(),
							//	scaled_normed_coords.y * (double)original_filter.getHeight()
							//	)[c];// * ss_weight;

							sum += (float)original_filter.vec3SampleTiled(
								scaled_normed_coords.x,
								scaled_normed_coords.y
								)[c];// * ss_weight;
						}
					}
				}
				filter.getPixel(x, y)[c] = sum;
			}
		}
	}

	// Re-normalise filter for each component
	Vec3d sum(0,0,0);
	for(unsigned int i=0; i<filter.numPixels(); ++i)
		sum += Vec3d(
			(double)filter.getPixel(i).r,
			(double)filter.getPixel(i).g,
			(double)filter.getPixel(i).b
			);

	const Vec3d scale_factors(1.0 / sum.x, 1.0 / sum.y, 1.0 / sum.z);
	for(unsigned int i=0; i<filter.numPixels(); ++i)
	{
		filter.getPixel(i).r *= (float)scale_factors.x;
		filter.getPixel(i).g *= (float)scale_factors.y;
		filter.getPixel(i).b *= (float)scale_factors.z;
	}

#ifdef DEBUG
	// Check normalised
	sum.set(0,0,0);
	for(unsigned int i=0; i<filter.numPixels(); ++i)
	{
		assert(filter.getPixel(i).r >= 0.0 && filter.getPixel(i).g >= 0.0 && filter.getPixel(i).b >= 0.0);
		assert(isFinite(filter.getPixel(i).r) && isFinite(filter.getPixel(i).g) && isFinite(filter.getPixel(i).b));

		sum += Vec3d(
			(double)filter.getPixel(i).r,
			(double)filter.getPixel(i).g,
			(double)filter.getPixel(i).b
			);
	}
	assert(epsEqual(sum.x, 1.0));
	assert(epsEqual(sum.y, 1.0));
	assert(epsEqual(sum.z, 1.0));
#endif


	/*{
	Image temp = filter;
	for(unsigned int i=0; i<temp.numPixels(); ++i)
		temp.getPixel(i).g = temp.getPixel(i).b = 0.0;

	IndigoImage igi;
	igi.write(temp, 1, 1, "resized_filter_r.igi");
	}
	{
	Image temp = filter;
	for(unsigned int i=0; i<temp.numPixels(); ++i)
		temp.getPixel(i).r = temp.getPixel(i).b = 0.0;

	IndigoImage igi;
	igi.write(temp, 1, 1, "resized_filter_g.igi");
	}
	{
	Image temp = filter;
	for(unsigned int i=0; i<temp.numPixels(); ++i)
		temp.getPixel(i).r = temp.getPixel(i).g = 0.0;

	IndigoImage igi;
	igi.write(temp, 1, 1, "resized_filter_b.igi");
	}

	{
	IndigoImage igi;
	igi.write(filter, 1, 1, "resized_filter.igi");
	}*/

	result_out = filter;
}

/*
void Image::convolve(const Image& filter, Image& result_out) const
{
	result_out.resize(getWidth(), getHeight());

	const int filter_w_2 = filter.getWidth() / 2;
	const int filter_h_2 = filter.getHeight() / 2;

	const int W = result_out.getWidth();
	const int H = result_out.getHeight();

	// For each pixel of result image
	for(int y=0; y<H; ++y)
	{
		const int src_y_min = myMax(0, y - filter_h_2);
		const int src_y_max = myMin(H, y + filter_h_2 - 1);

		printVar(y);

		for(int x=0; x<W; ++x)
		{
			const int src_x_min = myMax(0, x - filter_w_2);
			const int src_x_max = myMin(W, x + filter_w_2 - 1);

			Colour3f c(0.0f);

			// For each pixel in filter support of source image
			for(int sy=src_y_min; sy<src_y_max; ++sy)
			{
				const int filter_y = (sy - y) + filter_h_2;
				assert(filter_y >= 0 && filter_y < filter.getHeight());		

				for(int sx=src_x_min; sx<src_x_max; ++sx)
				{
					const int filter_x = (sx - x) + filter_w_2;
					assert(filter_x >= 0 && filter_x < filter.getWidth());	

					assert(this->getPixel(sx, sy).r >= 0.0 && this->getPixel(sx, sy).g >= 0.0 && this->getPixel(sx, sy).b >= 0.0);
					assert(isFinite(this->getPixel(sx, sy).r) && isFinite(this->getPixel(sx, sy).g) && isFinite(this->getPixel(sx, sy).b));

					c.addMult(this->getPixel(sx, sy), filter.getPixel(filter_x, filter_y));
				}
			}


			assert(c.r >= 0.0 && c.g >= 0.0 && c.b >= 0.0);
			assert(isFinite(c.r) && isFinite(c.g) && isFinite(c.b));

			result_out.setPixel(x, y, c);
		}
	}
}
*/

float Image::minPixelComponent() const
{
	float x = std::numeric_limits<float>::max();
	for(unsigned int i=0; i<numPixels(); ++i)
		x = myMin(x, myMin(getPixel(i).r, myMin(getPixel(i).g, getPixel(i).b)));
	return x;
}

float Image::maxPixelComponent() const
{
	float x = -std::numeric_limits<float>::max();
	for(unsigned int i=0; i<numPixels(); ++i)
		x = myMax(x, myMax(getPixel(i).r, myMax(getPixel(i).g, getPixel(i).b)));
	return x;
}



const Colour3d Image::vec3SampleTiled(double u, double v) const
{
	//return sampleTiled((float)x, (float)y).toColour3d();

	Colour3d colour_out;

	double intpart; // not used
	double u_frac_part = modf(u, &intpart);
	double v_frac_part = modf(1.0 - v, &intpart); // 1.0 - v because we want v=0 to be at top of image, and v=1 to be at bottom.

	if(u_frac_part < 0.0)
		u_frac_part = 1.0 + u_frac_part;
	if(v_frac_part < 0.0)
		v_frac_part = 1.0 + v_frac_part;

	assert(Maths::inHalfClosedInterval(u_frac_part, 0.0, 1.0));
	assert(Maths::inHalfClosedInterval(v_frac_part, 0.0, 1.0));

	// Convert from normalised image coords to pixel coordinates
	const double u_pixels = u_frac_part * (double)getWidth();
	const double v_pixels = v_frac_part * (double)getHeight();

	assert(Maths::inHalfClosedInterval(u_pixels, 0.0, (double)getWidth()));
	assert(Maths::inHalfClosedInterval(v_pixels, 0.0, (double)getHeight()));

	const unsigned int ut = (unsigned int)u_pixels;
	const unsigned int vt = (unsigned int)v_pixels;

	assert(ut >= 0 && ut < getWidth());
	assert(vt >= 0 && vt < getHeight());

	const unsigned int ut_1 = (ut + 1) % getWidth();
	const unsigned int vt_1 = (vt + 1) % getHeight();

	const double ufrac = u_pixels - (double)ut;
	const double vfrac = v_pixels - (double)vt;
	const double oneufrac = 1.0 - ufrac;
	const double onevfrac = 1.0 - vfrac;

	// Top left pixel
	{
		const float* pixel = getPixel(ut, vt).data();
		const double factor = oneufrac * onevfrac;
		colour_out.r = (double)pixel[0] * factor;
		colour_out.g = (double)pixel[1] * factor;
		colour_out.b = (double)pixel[2] * factor;
	}


	// Top right pixel
	{
		const float* pixel = getPixel(ut_1, vt).data();
		const double factor = ufrac * onevfrac;
		colour_out.r += (double)pixel[0] * factor;
		colour_out.g += (double)pixel[1] * factor;
		colour_out.b += (double)pixel[2] * factor;
	}


	// Bottom left pixel
	{
		const float* pixel = getPixel(ut, vt_1).data();
		const double factor = oneufrac * vfrac;
		colour_out.r += (double)pixel[0] * factor;
		colour_out.g += (double)pixel[1] * factor;
		colour_out.b += (double)pixel[2] * factor;
	}

	// Bottom right pixel
	{
		const float* pixel = getPixel(ut_1, vt_1).data();
		const double factor = ufrac * vfrac;
		colour_out.r += (double)pixel[0] * factor;
		colour_out.g += (double)pixel[1] * factor;
		colour_out.b += (double)pixel[2] * factor;
	}

	return colour_out;
}

/*
double Image::scalarSampleTiled(double x, double y) const
{
	const Colour3f col = sampleTiled((float)x, (float)y);
	return (double)(col.r + col.g + col.b) * (1.0 / 3.0);
}*/

double Image::scalarSampleTiled(double x, double y) const
{
	const Colour3d col = vec3SampleTiled(x, y);
	return (col.r + col.g + col.b) * (1.0 / 3.0);
}