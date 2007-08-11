#include "image.h"

#pragma warning(disable : 4290)//disable exception specification warning in VS2003


#include <stdio.h>
#include <fstream>
#include "../indigo/globals.h"
#include "../utils/stringutils.h"
#include <assert.h>
#include <limits>

#ifndef BASIC_IMAGE

#if !defined(WIN64)
#define OPENEXR_SUPPORT 1
#endif

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

Image::Image(int width_, int height_)
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

	for(int x=0; x<width; ++x)
		for(int y=0; y<height; ++y)
		{
			setPixel(x, y, other.getPixel(x, y));
		}

	return *this;
}










typedef unsigned char BYTE;


inline float byteToFloat(BYTE c)
{
	return (float)c / 255.0f;
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

			ColourType pixelcolour(r, g, b);

			this->setPixel(x, y, pixelcolour);

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

void Image::resize(int newwidth, int newheight)
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


void Image::blitToImage(Image& dest, int destx, int desty)
{
	for(int y=0; y<getHeight(); ++y)
		for(int x=0; x<getWidth(); ++x)		
		{
			int dx = x + destx;
			int dy = y + desty;
			if(dx >= 0 && dx < dest.getWidth() && dy >= 0 && dy < dest.getHeight())
				dest.setPixel(dx, dy, getPixel(x, y));
		}
}

void Image::addImage(const Image& img, int destx, int desty)
{	
	for(int y=0; y<img.getHeight(); ++y)
		for(int x=0; x<img.getWidth(); ++x)		
		{
			int dx = x + destx;
			int dy = y + desty;
			if(dx >= 0 && dx < getWidth() && dy >= 0 && dy < getHeight())
				getPixel(dx, dy) += img.getPixel(x, y);
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

	return colour_out;
}

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
}

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
void error_func(png_structp png, const char* msg)
{
	throw ImageExcep("LibPNG error: " + std::string(msg));

}

void warning_func(png_structp png, const char* msg)
{
	throw ImageExcep("LibPNG warning: " + std::string(msg));
}

void Image::saveToPng(const std::string& pathname) const
{

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
	png_set_IHDR(png, info, getWidth(), getHeight(),
       8, //bit depth of each channel
	   PNG_COLOR_TYPE_RGB, //colour type
	   PNG_INTERLACE_NONE, //interlace type
	   PNG_COMPRESSION_TYPE_BASE,//PNG_COMPRESSION_TYPE_DEFAULT, //compression type
	   PNG_FILTER_TYPE_BASE);//PNG_FILTER_TYPE_DEFAULT);//filter method


	//png_set_gAMA(png_ptr, info_ptr, 2.3);

	//write info
	png_write_info(png, info);


	//------------------------------------------------------------------------
	//write image rows
	//------------------------------------------------------------------------
	std::vector<unsigned char> rowdata(getWidth() * 3);

	for(int y=0; y<getHeight(); ++y)
	{
		//------------------------------------------------------------------------
		//copy floating point data to 8bpp format
		//------------------------------------------------------------------------
		for(int x=0; x<getWidth(); ++x)
			for(int i=0; i<3; ++i)
				rowdata[x*3 + i] = (unsigned char)(getPixel(x, y)[i] * 255.0f);

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
}

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



void Image::collapseSize(int factor)
{
	Image out(width/factor, height/factor);

	for(int y=0; y<out.getHeight(); ++y)
		for(int x=0; x<out.getWidth(); ++x)
		{
			out.getPixel(x, y).set(0.f, 0.f, 0.f);
			for(int s=0; s<factor; ++s)
				for(int t=0; t<factor; ++t)
					out.getPixel(x, y) += getPixel(x*factor+s, y*factor+t);
			out.getPixel(x, y) /= (float)(factor * factor);
			//out.setPixel(x, y, (getPixel(x*factor, y*factor) + getPixel(x*factor+1, y*factor) + getPixel(x*factor, y*factor+1) + getPixel(x*factor+1, y*2+1)) * 0.25f);
		}

	*this = out;
}



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
	float maxlum = std::numeric_limits<float>::min();
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

























