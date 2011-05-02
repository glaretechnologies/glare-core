#include "image.h"


#include "MitchellNetravali.h"
#include "BoxFilterFunction.h"
#include "../utils/stringutils.h"
#include "../utils/fileutils.h"
#include "../utils/FileHandle.h"
#include "../utils/Exception.h"
#include "../maths/vec2.h"
#include <fstream>
#include <limits>
#include <cmath>
#include <cassert>
#include <stdio.h>


#ifndef BASIC_IMAGE

#ifdef OPENEXR_SUPPORT
#include <ImfRgbaFile.h>
#include <ImathBox.h>
#include <ImfChannelList.h>
#include <ImfOutputFile.h>
#include <ImfStdIO.h>
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
	//width = 0;
	//height = 0;
}

Image::Image(size_t width_, size_t height_)
:	pixels(width_, height_)
{
	//width = width_;
	//height = height_;
}



Image::~Image()
{}



Image& Image::operator = (const Image& other)
{
	if(&other == this)
		return *this;

	if(getWidth() != other.getWidth() || getHeight() != other.getHeight())
	{
		//width = other.width;
		//height = other.height;

		pixels.resize(other.getWidth(), other.getHeight());
	}

	/*for(unsigned int x=0; x<width; ++x)
		for(unsigned int y=0; y<height; ++y)
		{
			setPixel(x, y, other.getPixel(x, y));
		}*/
	//for(unsigned int i=0; i<other.numPixels(); ++i)
	//	this->getPixel(i) = other.getPixel(i);
	this->pixels = other.pixels;

	return *this;
}


// will throw ImageExcep if bytespp != 3
void Image::setFromBitmap(const Bitmap& bmp, float image_gamma)
{
	if(bmp.getBytesPP() != 1 && bmp.getBytesPP() != 3)
		throw ImageExcep("Image bytes per pixel must be 1 or 3.");

	resize(bmp.getWidth(), bmp.getHeight());

	if(bmp.getBytesPP() == 1)
	{
		const float factor = 1.0f / 255.0f;
		for(size_t y = 0; y < bmp.getHeight(); ++y)
		for(size_t x = 0; x < bmp.getWidth();  ++x)
		{
			setPixel(x, y, Colour3f(
				std::pow((float)*bmp.getPixel(x, y) * factor, image_gamma)
			));
		}
	}
	else
	{
		assert(bmp.getBytesPP() == 3);

		const float factor = 1.0f / 255.0f;
		for(size_t y = 0; y < bmp.getHeight(); ++y)
		for(size_t x = 0; x < bmp.getWidth();  ++x)
		{
			setPixel(x, y,
				Colour3f(
					std::pow((float)bmp.getPixel(x, y)[0] * factor, image_gamma),
					std::pow((float)bmp.getPixel(x, y)[1] * factor, image_gamma),
					std::pow((float)bmp.getPixel(x, y)[2] * factor, image_gamma)
					)
				);
		}
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

	for(int y = y1; y < y2; ++y)
	for(int x = x1; x < x2; ++x)
	{
		unsigned char* pixel = bmp_out.getPixelNonConst(x - x1, y - y1);
		pixel[0] = (unsigned char)(getPixel(x, y).r * 255.0f);
		pixel[1] = (unsigned char)(getPixel(x, y).g * 255.0f);
		pixel[2] = (unsigned char)(getPixel(x, y).b * 255.0f);
	}
}


void Image::copyToBitmap(Bitmap& bmp_out) const
{
	bmp_out.resize(getWidth(), getHeight(), 3);

	for(size_t y = 0; y < getHeight(); ++y)
	for(size_t x = 0; x < getWidth();  ++x)
	{
		const ColourType& p = getPixel(x, y);

		unsigned char* pixel = bmp_out.getPixelNonConst(x, y);
		pixel[0] = (unsigned char)(p.r * 255.0f);
		pixel[1] = (unsigned char)(p.g * 255.0f);
		pixel[2] = (unsigned char)(p.b * 255.0f);
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
	try
	{
		//-----------------------------------------------------------------
		//open file
		//-----------------------------------------------------------------
		FileHandle f(pathname, "rb");

		//-----------------------------------------------------------------
		//read bitmap header
		//-----------------------------------------------------------------
		BITMAP_HEADER bitmap_header;
		const int header_size = sizeof(BITMAP_HEADER);
		assert(header_size == 14);

		fread(&bitmap_header, 14, 1, f.getFile());

		//debugPrint("file size: %i\n", (int)bitmap_header.size);
		//debugPrint("offset to image data in bytes: %i\n", (int)bitmap_header.offset);


		//-----------------------------------------------------------------
		//read bitmap info-header
		//-----------------------------------------------------------------
		assert(sizeof(BITMAP_INFOHEADER) == 40);

		BITMAP_INFOHEADER infoheader;

		fread(&infoheader, sizeof(BITMAP_INFOHEADER), 1, f.getFile());

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


		const size_t width = infoheader.width;
		const size_t height = infoheader.height;

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

		fread(data, DATASIZE, 1, f.getFile());

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
		//fclose (f);
	}
	catch(Indigo::Exception& )
	{
		throw ImageExcep("could not open file '" + pathname + "'.");
	}
}


void Image::saveToBitmap(const std::string& pathname)
{
	try
	{
		FileHandle f(pathname, "wb");

		BITMAP_HEADER bitmap_header;
		bitmap_header.offset = sizeof(BITMAP_HEADER) + sizeof(BITMAP_INFOHEADER);
		bitmap_header.type = BITMAP_ID;
		bitmap_header.size = sizeof(BITMAP_HEADER) + sizeof(BITMAP_INFOHEADER) + getWidth() * getHeight() * 3;

		fwrite(&bitmap_header, sizeof(BITMAP_HEADER), 1, f.getFile());

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

		fwrite(&bitmap_infoheader, sizeof(BITMAP_INFOHEADER), 1, f.getFile());


		int rowpaddingbytes = 4 - ((getWidth() * 3) % 4);
		if(rowpaddingbytes == 4)
			rowpaddingbytes = 0;


		for(int y = getHeight() - 1; y >= 0; --y)
		{
			for(int x = 0; x < getWidth(); ++x)
			{
				ColourType pixelcol = getPixel(x, y);
				pixelcol.positiveClipComponents();

				const BYTE b = (BYTE)(pixelcol.b * 255.0f);
				fwrite(&b, 1, 1, f.getFile());

				const BYTE g = (BYTE)(pixelcol.g * 255.0f);
				fwrite(&g, 1, 1, f.getFile());

				const BYTE r = (BYTE)(pixelcol.r * 255.0f);
				fwrite(&r, 1, 1, f.getFile());

				/*const BYTE r = (BYTE)(getPixel(x, y).r * 255.0f);
				fwrite(&r, 1, 1, f);
				const BYTE g = (BYTE)(getPixel(x, y).g * 255.0f);
				fwrite(&g, 1, 1, f);
				const BYTE b = (BYTE)(getPixel(x, y).b * 255.0f);
				fwrite(&b, 1, 1, f);*/
			}

			const BYTE zerobyte = 0;
			for(int i = 0; i < rowpaddingbytes; ++i)
				fwrite(&zerobyte, 1, 1, f.getFile());
		}

		//fclose(f);
	}
	catch(Indigo::Exception& )
	{
		throw ImageExcep("could not open file '" + pathname + "'.");
	}
}


//TODO: error handling
/*void Image::loadFromRAW(const std::string& pathname, int width_, int height_,
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
}*/


void Image::zero()
{
	set(0);
}


void Image::set(const float s)
{
	const ColourType s_colour(s, s, s);

	const size_t num = numPixels();
	for(size_t i = 0; i < num; ++i)
		getPixel(i) = s_colour;
}


void Image::resize(size_t newwidth, size_t newheight)
{
	if(getWidth() == newwidth && getHeight() == newheight)
		return;

	//width = newwidth;
	//height = newheight;

	pixels.resize(newwidth, newheight);
}


void Image::posClamp()
{
	const size_t num = numPixels();
	for(size_t i = 0; i < num; ++i)
		getPixel(i).positiveClipComponents();

	//for(int x=0; x<width; ++x)
	//	for(int y=0; y<height; ++y)
	//		getPixel(x, y).positiveClipComponents();
}


void Image::clampInPlace(float min, float max)
{
	const size_t num = numPixels();
	for(size_t i = 0; i < num; ++i)
		getPixel(i).clampInPlace(min, max);
}


void Image::gammaCorrect(float exponent)
{
	/*for(int x=0; x<width; ++x)
		for(int y=0; y<height; ++y)
		{
			ColourType& colour = getPixel(x, y);

			colour.r = pow(colour.r, exponent);
			colour.g = pow(colour.g, exponent);
			colour.b = pow(colour.b, exponent);
		}*/
	const size_t num = numPixels();
	for(size_t i = 0; i < num; ++i)
	{
		const ColourType colour = getPixel(i);
		ColourType newcolour(
			std::pow(colour.r, exponent),
			std::pow(colour.g, exponent),
			std::pow(colour.b, exponent)
		);

		getPixel(i) = newcolour;
	}
}


void Image::scale(float factor)
{
	/*for(int x=0; x<width; ++x)
		for(int y=0; y<height; ++y)
		{
			getPixel(x, y) *= factor;
		}*/
	const size_t num = numPixels();
	for(size_t i = 0; i < num; ++i)
		getPixel(i) *= factor;
}


void Image::blitToImage(Image& dest, int destx, int desty) const
{
	for(int y = 0; y < getHeight(); ++y)
	for(int x = 0; x < getWidth();  ++x)
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

	for(int y = src_start_y; y < src_end_y; ++y)
	for(int x = src_start_x; x < src_end_x; ++x)
	{
		const int dx = (x - src_start_x) + destx;
		const int dy = (y - src_start_y) + desty;

		if(dx >= 0 && dx < dest.getWidth() && dy >= 0 && dy < dest.getHeight())
			dest.setPixel(dx, dy, getPixel(x, y));
	}
}


void Image::addImage(const Image& img, const int destx, const int desty, const float alpha/* = 1*/)
{
	for(int y = 0; y < img.getHeight(); ++y)
	for(int x = 0; x < img.getWidth();  ++x)
	{
		const int dx = x + destx;
		const int dy = y + desty;

		if(dx >= 0 && dx < getWidth() && dy >= 0 && dy < getHeight())
			getPixel(dx, dy) += img.getPixel(x, y) * alpha;
	}
}


void Image::blendImage(const Image& img, const int destx, const int desty, const Colour3f& solid_colour, const float alpha/* = 1*/)
{
	for(int y = 0; y < img.getHeight(); ++y)
	for(int x = 0; x < img.getWidth();  ++x)
	{
		const int dx = x + destx;
		const int dy = y + desty;

		if(dx >= 0 && dx < getWidth() && dy >= 0 && dy < getHeight())
			setPixel(dx, dy, solid_colour * img.getPixel(x, y).r * alpha + getPixel(dx, dy) * (1 - img.getPixel(x, y).r * alpha));
	}
}


void Image::mulImage(const Image& img, const int destx, const int desty, const float alpha/* = 1*/, bool invert/* = false*/)
{
	for(int y = 0; y < img.getHeight(); ++y)
	for(int x = 0; x < img.getWidth();  ++x)
	{
		const int dx = x + destx;
		const int dy = y + desty;

		if(dx >= 0 && dx < getWidth() && dy >= 0 && dy < getHeight())
		{
			const float inv_alpha = ((invert) ? 1 - img.getPixel(x, y).r : img.getPixel(x, y).r) * alpha;
			setPixel(dx, dy, getPixel(dx, dy) * (1 - alpha) + getPixel(dx, dy) * inv_alpha);
		}
	}
}


void Image::subImage(const Image& img, int destx, int desty)
{
	for(int y = 0; y < img.getHeight(); ++y)
	for(int x = 0; x < img.getWidth();  ++x)
	{
		int dx = x + destx;
		int dy = y + desty;

		if(dx >= 0 && dx < getWidth() && dy >= 0 && dy < getHeight())
			getPixel(dx, dy).addMult(img.getPixel(x, y), -1.0f); //NOTE: slow :)
	}
}


void Image::overwriteImage(const Image& img, int destx, int desty)
{
	for(int y = 0; y < img.getHeight(); ++y)
	for(int x = 0; x < img.getWidth();  ++x)
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
/*void Image::saveTo16BitExr(const std::string& pathname) const
{
	try
	{
		std::ofstream outfile_stream(StringUtils::UTF8ToPlatformUnicodeEncoding(pathname).c_str(), std::ios::binary);

		Imf::StdOFStream exr_ofstream(outfile_stream, pathname.c_str());

		Imf::RgbaOutputFile file(exr_ofstream, getWidth(), getHeight(), Imf::WRITE_RGBA);

		std::vector<Imf::Rgba> rgba(getWidth() * getHeight());

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

		file.writePixels(getHeight());
	}
	catch(const std::exception& e)
	{
		throw ImageExcep("Error writing EXR file: " + std::string(e.what()));
	}
}*/


void Image::saveTo32BitExr(const std::string& pathname) const
{
	// See 'Reading and writing image files.pdf', section 3.1: Writing an Image File
	try
	{
		//NOTE: I'm assuming that the pixel data is densely packed, so that y-stride is sizeof(ColourType) * getWidth())

		std::ofstream outfile_stream(FileUtils::convertUTF8ToFStreamPath(pathname).c_str(), std::ios::binary);

		Imf::StdOFStream exr_ofstream(outfile_stream, pathname.c_str());

		Imf::Header header(getWidth(), getHeight());
		header.channels().insert("R", Imf::Channel(Imf::FLOAT));
		header.channels().insert("G", Imf::Channel(Imf::FLOAT));
		header.channels().insert("B", Imf::Channel(Imf::FLOAT));
		Imf::OutputFile file(exr_ofstream, header);                               
		Imf::FrameBuffer frameBuffer;

		frameBuffer.insert("R",				// name
			Imf::Slice(Imf::FLOAT,			// type
			(char*)&getPixel(0).r,			// base
			sizeof(ColourType),				// xStride
			sizeof(ColourType) * getWidth())// yStride
		);
		frameBuffer.insert("G",				// name
			Imf::Slice(Imf::FLOAT,			// type
			(char*)&getPixel(0).g,			// base
			sizeof(ColourType),				// xStride
			sizeof(ColourType) * getWidth())// yStride
			);
		frameBuffer.insert("B",				// name
			Imf::Slice(Imf::FLOAT,			// type
			(char*)&getPixel(0).b,			// base
			sizeof(ColourType),				// xStride
			sizeof(ColourType) * getWidth())// yStride
			);
		file.setFrameBuffer(frameBuffer);
		file.writePixels(getHeight());
	}
	catch(const std::exception& e)
	{
		throw ImageExcep("Error writing EXR file: " + std::string(e.what()));
	}
}


#else //OPENEXR_SUPPORT


void Image::saveTo32BitExr(const std::string& pathname) const
{
	throw ImageExcep("Image::saveTo32BitExr: OPENEXR_SUPPORT disabled.");
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
	if(getHeight() == 0 || getWidth() == 0)
		return;

	double av_lum = 0.0;
	for(size_t i=0; i<numPixels(); ++i)
		av_lum += (double)getPixel(i).luminance();
	av_lum /= (double)(numPixels());

	const float factor = (float)(1.0 / av_lum);
	for(size_t i=0; i<numPixels(); ++i)
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

	Image out;
	BoxFilterFunction ff;
	collapseImage(
		factor,
		0, // border
		ff,
		1.0f, // max component value
		*this,
		out
		);

	*this = out;
}


void Image::collapseImage(int factor, int border_width, const FilterFunction& filter_function, float max_component_value, const Image& in, Image& out)
{
	assert(border_width >= 0);
	assert(in.getWidth()  > border_width * 2);
	assert(in.getHeight() > border_width * 2);
	assert((in.getWidth() - (border_width * 2)) % factor == 0);

	//Image out((width - (border_width * 2)) / factor, (height - (border_width * 2)) / factor);
	out.resize((in.getWidth() - (border_width * 2)) / factor, (in.getHeight() - (border_width * 2)) / factor);

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
	//int support_y = (int)floor((0.5 - filter_function.supportRadius()) * (double)factor) + border_width;

	#ifndef OSX
	#pragma omp parallel for
	#endif
	for(int y=0; y<(int)out.getHeight(); ++y)
	{

		// Get the y-range of pixels in the source image that lie in the filter support for the destination pixel.
		const int support_y = (y * factor) + (int)floor((0.5 - filter_function.supportRadius()) * (double)factor) + border_width;
		/*const int src_y_center = y * factor;
		const int src_y_min = myMax(0, src_y_center - neg_rad);
		const int src_y_max = myMin(this->getHeight(), src_y_center + pos_rad);*/

		const int src_y_min = myMax(0, support_y);
		const int src_y_max = myMin((int)in.getHeight(), support_y + filter_width);

		//if(y % 50 == 0)
		//	printVar(y);


		int support_x = (int)floor((0.5 - filter_function.supportRadius()) * (double)factor) + border_width;

		for(int x=0; x<(int)out.getWidth(); ++x)
		{
			/*const int src_x_center = x * factor;
			const int src_x_min = myMax(0, src_x_center - neg_rad);
			const int src_x_max = myMin(this->getWidth(), src_x_center + pos_rad);*/

			// Get the x-range of pixels in the source image that lie in the filter support for the destination pixel.
			const int src_x_min = myMax(0, support_x);
			const int src_x_max = myMin((int)in.getWidth(), support_x + filter_width);

			Colour3f c(0.0f); // Running sum of source pixel value * filter value.

			// For each pixel in filter support of source image
			for(int sy=src_y_min; sy<src_y_max; ++sy)
			{
				const int filter_y = sy - support_y; //(sy - src_y_center) + neg_rad;
				assert(filter_y >= 0 && filter_y < filter_width);

				for(int sx=src_x_min; sx<src_x_max; ++sx)
				{
					const int filter_x = sx - support_x; //(sx - src_x_center) + neg_rad;
					assert(filter_x >= 0 && filter_x < filter_width);

					assert(in.getPixel(sx, sy).r >= 0.0 && in.getPixel(sx, sy).g >= 0.0 && in.getPixel(sx, sy).b >= 0.0);
					assert(isFinite(in.getPixel(sx, sy).r) && isFinite(in.getPixel(sx, sy).g) && isFinite(in.getPixel(sx, sy).b));

					c.addMult(in.getPixel(sx, sy), filter.elem(filter_x, filter_y));
				}
			}


			//assert(c.r >= 0.0 && c.g >= 0.0 && c.b >= 0.0);
			assert(isFinite(c.r) && isFinite(c.g) && isFinite(c.b));

			// Make sure components can't go below zero or above max_component_value
			c.clampInPlace(0.0f, max_component_value);
			//c.lowerClampInPlace(0.f);

			out.setPixel(x, y, c);

			support_x += factor;
		}

		//support_y += factor;
	}

	//*this = out;
}


void Image::collapseImageNew(const int factor, const int border_width, const int filter_span, const float* const resize_filter, const float max_component_value, const Image& img_in, Image& img_out)
{
	//assert((int64_t)img_in.getWidth() * (int64_t)img_in.getHeight() < 2147483648);	// no 32bit integer overflow
	assert(border_width >= 0);												// have padding pixels
	assert(img_in.getWidth()  > border_width * 2);								// have at least one interior pixel in x
	assert(img_in.getHeight() > border_width * 2);								// have at least one interior pixel in y
	assert((img_in.getWidth() - (border_width * 2)) % factor == 0);				// padded image is multiple of supersampling factor

	assert(filter_span > 0);
	assert(resize_filter != 0);

	img_out.resize( (img_in.getWidth()  - (border_width * 2)) / factor,
					(img_in.getHeight() - (border_width * 2)) / factor);

	const int in_xres  = (int)img_in.getWidth();
	const int in_yres  = (int)img_in.getHeight();
	const int out_xres = (int)img_out.getWidth();
	const int out_yres = (int)img_out.getHeight();
	const int filter_bound = filter_span / 2 - 1;

	ColourType const * const in_buffer  = &img_in.getPixel(0, 0);
	ColourType		 * const out_buffer = &img_out.getPixel(0, 0);

	#ifndef OSX
	#pragma omp parallel for
	#endif
	for(int y = 0; y < out_yres; ++y)
	for(int x = 0; x < out_xres; ++x)
	{
		ColourType weighted_sum(0);
		uint32 filter_addr = 0;

		for(int v = -filter_bound; v <= filter_bound; ++v)
		for(int u = -filter_bound; u <= filter_bound; ++u)
		{
			const int addr = (y * factor + factor / 2 + v + border_width) * in_xres +
							  x * factor + factor / 2 + u + border_width;

			weighted_sum.addMult(in_buffer[addr], resize_filter[filter_addr++]);
		}

		assert(isFinite(weighted_sum.r) && isFinite(weighted_sum.g) && isFinite(weighted_sum.b));

		// Make sure components can't go below zero or above max_component_value
		weighted_sum.clampInPlace(0.0f, max_component_value);
		//weighted_sum.lowerClampInPlace(0.f);

		out_buffer[y * out_xres + x] = weighted_sum;
	}

	//*this = out;
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


size_t Image::getByteSize() const
{
	return numPixels() * 3 * sizeof(float);
}

float Image::minLuminance() const
{
	float minlum = std::numeric_limits<float>::max();
	for(size_t i=0; i<numPixels(); ++i)
		minlum = myMin(minlum, getPixel(i).luminance());
	return minlum;
}
float Image::maxLuminance() const
{
	float maxlum = -std::numeric_limits<float>::max();
	for(size_t i=0; i<numPixels(); ++i)
		maxlum = myMax(maxlum, getPixel(i).luminance());
	return maxlum;
}


double Image::averageLuminance() const
{
	double sum = 0.0;
	for(size_t i=0; i<numPixels(); ++i)
		sum += (double)getPixel(i).luminance();
	return sum / (double)numPixels();
}


void Image::buildRGBFilter(const Image& original_filter, const Vec3d& filter_scales, Image& result_out)
{
	const double max_scale = myMax(filter_scales.x, myMax(filter_scales.y, filter_scales.z));

	const int new_filter_width = (int)((double)original_filter.getWidth() * max_scale);
	const int new_filter_height = (int)((double)original_filter.getHeight() * max_scale);
	Image filter(new_filter_width, new_filter_height);

	//printVar(new_filter_width);

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
			//printVar(y);

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
	for(size_t i=0; i<filter.numPixels(); ++i)
		sum += Vec3d(
			(double)filter.getPixel(i).r,
			(double)filter.getPixel(i).g,
			(double)filter.getPixel(i).b
			);

	const Vec3d scale_factors(1.0 / sum.x, 1.0 / sum.y, 1.0 / sum.z);
	for(size_t i=0; i<filter.numPixels(); ++i)
	{
		filter.getPixel(i).r *= (float)scale_factors.x;
		filter.getPixel(i).g *= (float)scale_factors.y;
		filter.getPixel(i).b *= (float)scale_factors.z;
	}

#ifdef DEBUG
	// Check normalised
	sum.set(0,0,0);
	for(size_t i=0; i<filter.numPixels(); ++i)
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
	for(size_t i=0; i<numPixels(); ++i)
		x = myMin(x, myMin(getPixel(i).r, myMin(getPixel(i).g, getPixel(i).b)));
	return x;
}

float Image::maxPixelComponent() const
{
	float x = -std::numeric_limits<float>::max();
	for(size_t i=0; i<numPixels(); ++i)
		x = myMax(x, myMax(getPixel(i).r, myMax(getPixel(i).g, getPixel(i).b)));
	return x;
}



const Colour3<Image::Value> Image::vec3SampleTiled(Coord u, Coord v) const
{
	//return sampleTiled((float)x, (float)y).toColour3d();

	Colour3<Value> colour_out;

	Coord intpart; // not used
	Coord u_frac_part = std::modf(u, &intpart);
	Coord v_frac_part = std::modf(1.0 - v, &intpart); // 1.0 - v because we want v=0 to be at top of image, and v=1 to be at bottom.

	if(u_frac_part < 0.0)
		u_frac_part = 1.0 + u_frac_part;
	if(v_frac_part < 0.0)
		v_frac_part = 1.0 + v_frac_part;

	assert(Maths::inHalfClosedInterval<Coord>(u_frac_part, 0.0, 1.0));
	assert(Maths::inHalfClosedInterval<Coord>(v_frac_part, 0.0, 1.0));

	// Convert from normalised image coords to pixel coordinates
	const Coord u_pixels = u_frac_part * (Coord)getWidth();
	const Coord v_pixels = v_frac_part * (Coord)getHeight();

	assert(Maths::inHalfClosedInterval<Coord>(u_pixels, 0.0, (Coord)getWidth()));
	assert(Maths::inHalfClosedInterval<Coord>(v_pixels, 0.0, (Coord)getHeight()));

	const unsigned int ut = (unsigned int)u_pixels;
	const unsigned int vt = (unsigned int)v_pixels;

	assert(ut >= 0 && ut < getWidth());
	assert(vt >= 0 && vt < getHeight());

	const unsigned int ut_1 = (ut + 1) % getWidth();
	const unsigned int vt_1 = (vt + 1) % getHeight();

	const Coord ufrac = u_pixels - (Coord)ut;
	const Coord vfrac = v_pixels - (Coord)vt;
	const Coord oneufrac = 1.0 - ufrac;
	const Coord onevfrac = 1.0 - vfrac;

	// Top left pixel
	{
		const float* pixel = getPixel(ut, vt).data();
		const Value factor = oneufrac * onevfrac;
		colour_out.r = pixel[0] * factor;
		colour_out.g = pixel[1] * factor;
		colour_out.b = pixel[2] * factor;
	}


	// Top right pixel
	{
		const float* pixel = getPixel(ut_1, vt).data();
		const Value factor = ufrac * onevfrac;
		colour_out.r += pixel[0] * factor;
		colour_out.g += pixel[1] * factor;
		colour_out.b += pixel[2] * factor;
	}


	// Bottom left pixel
	{
		const float* pixel = getPixel(ut, vt_1).data();
		const Value factor = oneufrac * vfrac;
		colour_out.r += pixel[0] * factor;
		colour_out.g += pixel[1] * factor;
		colour_out.b += pixel[2] * factor;
	}

	// Bottom right pixel
	{
		const float* pixel = getPixel(ut_1, vt_1).data();
		const Value factor = ufrac * vfrac;
		colour_out.r += pixel[0] * factor;
		colour_out.g += pixel[1] * factor;
		colour_out.b += pixel[2] * factor;
	}

	return colour_out;
}

/*
double Image::scalarSampleTiled(double x, double y) const
{
	const Colour3f col = sampleTiled((float)x, (float)y);
	return (double)(col.r + col.g + col.b) * (1.0 / 3.0);
}*/

Image::Value Image::scalarSampleTiled(Coord x, Coord y) const
{
	const Colour3<Value> col = vec3SampleTiled(x, y);
	return (col.r + col.g + col.b) * static_cast<Image::Value>(1.0 / 3.0);
}


Reference<Image> Image::convertToImage() const
{
	// Return copy of this image.
	return Reference<Image>(new Image(*this));
}


#if (BUILD_TESTS)

#include "../indigo/TestUtils.h"
#include "../graphics/MitchellNetravaliFilterFunction.h"

void Image::test()
{
	const int mitchell_b_steps = 6;

	for(int mitchell_b_int = 0; mitchell_b_int < mitchell_b_steps; ++mitchell_b_int)
	for(int supersample_factor = 2; supersample_factor < 4; ++supersample_factor)
	for(int sqrt_image_size = 1; sqrt_image_size < 3; ++sqrt_image_size)
	{
		// Compute Mitchell-Netravali B, C values from the relation 2C + B = 1
		const double mitchell_b = (double)mitchell_b_int / (double)(mitchell_b_steps - 1);
		const double mitchell_c = (1 - mitchell_b) / 2.0;

		MitchellNetravaliFilterFunction filter(mitchell_b, mitchell_c);

		const int filter_pixel_span  = (int)ceil(filter.supportRadius() * 2) * supersample_factor;
		const int filter_pixel_bound = filter_pixel_span / 2 - 1;

		const int src_xres = 2 * filter_pixel_bound + supersample_factor * sqrt_image_size;
		const int src_yres = 2 * filter_pixel_bound + supersample_factor * sqrt_image_size;
		const int dst_xres = sqrt_image_size;
		const int dst_yres = sqrt_image_size;

		Image src(src_xres, src_yres), dst(dst_xres, dst_yres);

		src.set(1);

		Image::collapseImageNew(
			supersample_factor,	// factor
			filter_pixel_bound,	// border width
			filter.getFilterSpan(supersample_factor),
			filter.getFilterData(supersample_factor),
			1000000.0f, // max component value
			src, // in
			dst // out
			);

		testAssert(::epsEqual(dst.maxPixelComponent(), 1.0f));
		//std::cout << "supersample = " << supersample_factor << ", size = " << sqrt_image_size << "x" << sqrt_image_size << ", B = " << mitchell_b << ", C = " << mitchell_c << ", max pixel component = " << dst.maxPixelComponent() << std::endl;
	}
}

#endif
