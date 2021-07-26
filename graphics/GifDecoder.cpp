/*=====================================================================
GifDecoder.cpp
--------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "GifDecoder.h"


#include "imformatdecoder.h"
#include "ImageMap.h"
#include "ImageMapSequence.h"
#include "../utils/StringUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/PlatformUtils.h"
#include <stdio.h>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <gif_lib.h>


#ifndef S_IREAD
#define S_IREAD S_IRUSR
#endif

#ifndef S_IWRITE
#define S_IWRITE S_IWUSR
#endif


static const std::string errorString(int error_code)
{
	const char* str = GifErrorString(error_code);
	if(str == NULL)
		return "[Unknown]";
	else
		return std::string(str);
}


// throws ImFormatExcep on failure
Reference<Map2D> GIFDecoder::decode(const std::string& path)
{
	// Do this file descriptor malarkey so that we can handle Unicode paths.
	const int file_descriptor = FileUtils::openFileDescriptor(path, O_RDONLY);
	if(file_descriptor == -1)
		throw ImFormatExcep("failed to open gif file '" + path + "'");

	// NOTE: if DGifOpenFileHandle fails to open the gif file, it closes the handle itself.
	int error_code = 0;
	GifFileType* gif_file = DGifOpenFileHandle(file_descriptor, &error_code);
	if(gif_file == NULL)
		throw ImFormatExcep("failed to open gif file '" + path + "': " + GifErrorString(error_code));

	try
	{
		int res = DGifSlurp(gif_file);
		if(res != GIF_OK)
			throw ImFormatExcep("failed to process gif file: " + errorString(res));

		if(gif_file->ImageCount < 1)
			throw ImFormatExcep("invalid ImageCount (< 1)");

		const SavedImage* const image_0 = &gif_file->SavedImages[0];

		// Get the current palette:
		ColorMapObject* colour_map = image_0->ImageDesc.ColorMap ? image_0->ImageDesc.ColorMap : gif_file->SColorMap; // Use local colormap, if it exists.
		if(!colour_map)
			throw ImFormatExcep("Failed to get ColorMapObject (palette).");

		// Get image dimensions
		if(image_0->ImageDesc.Width <= 0 || image_0->ImageDesc.Width > 1000000)
			throw ImFormatExcep("Invalid image width.");
		if(image_0->ImageDesc.Height <= 0 || image_0->ImageDesc.Height > 1000000)
			throw ImFormatExcep("Invalid image height.");

		const size_t w = (size_t)image_0->ImageDesc.Width;
		const size_t h = (size_t)image_0->ImageDesc.Height;

		Reference<ImageMap<uint8, UInt8ComponentValueTraits> > image_map = new ImageMap<uint8, UInt8ComponentValueTraits>(w, h, 3);
		image_map->setGamma((float)2.2f);

		const GifColorType* const colours = colour_map->Colors;
		uint8* const dest = image_map->getData();
		
		const size_t num_pixels = w * h;
		for(size_t i=0; i<num_pixels; ++i)
		{
			const uint8 c = image_0->RasterBits[i];
			// Is this colour index guaranteed to be in-bounds for giflib?
			//if(c >= colour_map->ColorCount)
			//	throw ImFormatExcep("Colour index out of bounds.");
			dest[i*3 + 0] = colours[c].Red;
			dest[i*3 + 1] = colours[c].Green;
			dest[i*3 + 2] = colours[c].Blue;
		}

		error_code = 0;
		DGifCloseFile(gif_file, &error_code);

		return image_map;
	}
	catch(ImFormatExcep& e)
	{
		// Call DGifCloseFile to clean up any memory allocated by DGifOpenFileHandle and DGifSlurp, and to close the file descriptor.
		error_code = 0;
		DGifCloseFile(gif_file, &error_code);
		throw e;
	}
}


Reference<Map2D> GIFDecoder::decodeImageSequence(const std::string& path)
{
	// Do this file descriptor malarkey so that we can handle Unicode paths.
	const int file_descriptor = FileUtils::openFileDescriptor(path, O_RDONLY);
	if(file_descriptor == -1)
		throw ImFormatExcep("failed to open gif file '" + path + "'");

	// NOTE: if DGifOpenFileHandle fails to open the gif file, it closes the handle itself.
	int error_code = 0;
	GifFileType* gif_file = DGifOpenFileHandle(file_descriptor, &error_code);
	if(gif_file == NULL)
		throw ImFormatExcep("failed to open gif file '" + path + "': " + GifErrorString(error_code));

	try
	{
		int res = DGifSlurp(gif_file);
		if(res != GIF_OK)
			throw ImFormatExcep("failed to process gif file: " + errorString(res));

		Reference<ImageMapSequenceUInt8> sequence = new ImageMapSequenceUInt8();
		if(gif_file->ImageCount < 1)
			throw ImFormatExcep("invalid ImageCount (< 1)");
		if(gif_file->ImageCount > 1000000) // Sanity check ImageCount, avoid wraparound etc..
			throw ImFormatExcep("invalid ImageCount (> 1000000)");

		sequence->images           .resize(gif_file->ImageCount);
		sequence->frame_durations  .resize(gif_file->ImageCount);
		sequence->frame_start_times.resize(gif_file->ImageCount);

		size_t im_0_w = 0;
		size_t im_0_h = 0;
		int last_disposal_mode = 0;
		double cur_frame_start_time = 0;
		for(int im_i=0; im_i<gif_file->ImageCount; ++im_i)
		{
			const SavedImage* const image_i = &gif_file->SavedImages[im_i];

			GraphicsControlBlock control_block;
			res = DGifSavedExtensionToGCB(gif_file, im_i, &control_block);
			int disposal_mode = 0;
			int transparent_color = -1;
			int num_hundredths = 4;
			if(res != GIF_ERROR) // If there is a control block:
			{
				disposal_mode = control_block.DisposalMode;
				transparent_color = control_block.TransparentColor;
				num_hundredths = control_block.DelayTime;

				// Some gif files have delay time = 0, which seems invalid.  The standard for this case seems to be to use a delay time of 10 hundredths of a second. (From chrome and windows photo viewer)
				if(num_hundredths == 0)
					num_hundredths = 10;
			}

			sequence->frame_durations  [im_i] = 0.01 * num_hundredths;
			sequence->frame_start_times[im_i] = cur_frame_start_time;
			cur_frame_start_time += 0.01 * num_hundredths;


			// Get the current palette:
			ColorMapObject* colour_map = image_i->ImageDesc.ColorMap ? image_i->ImageDesc.ColorMap : gif_file->SColorMap; // Use local colormap, if it exists.

			if(!colour_map)
				throw ImFormatExcep("Failed to get ColorMapObject (palette).");

			// Get image dimensions
			if(image_i->ImageDesc.Width <= 0 || image_i->ImageDesc.Width > 1000000)
				throw ImFormatExcep("Invalid image width.");
			if(image_i->ImageDesc.Height <= 0 || image_i->ImageDesc.Height > 1000000)
				throw ImFormatExcep("Invalid image height.");

			const size_t w = (size_t)image_i->ImageDesc.Width;
			const size_t h = (size_t)image_i->ImageDesc.Height;

			if(im_i == 0)
			{
				im_0_w = w;
				im_0_h = h;
			}
			Reference<ImageMap<uint8, UInt8ComponentValueTraits> > image_map = new ImageMap<uint8, UInt8ComponentValueTraits>(im_0_w, im_0_h, 3);
			image_map->setGamma((float)2.2f);
			sequence->images[im_i] = image_map;

			if(im_i == 0)
			{
				// Initialise image 0 as blue.
				for(int q=0; q<im_0_w * im_0_h; ++q)
				{
					uint8* const p = image_map->getPixel(q);
					p[0] = 0;
					p[1] = 0;
					p[2] = 255;
				}
			}
			else
			{
				// Decide how to 'dispose' of the last frame.
				if(last_disposal_mode == DISPOSAL_UNSPECIFIED)
					std::memcpy(image_map->getData(), sequence->images[im_i - 1]->getData(), im_0_w * im_0_h * 3); // Unspecified seems to want the last image as well.
				else if(last_disposal_mode == DISPOSE_DO_NOT) // Leave last image in place
					std::memcpy(image_map->getData(), sequence->images[im_i - 1]->getData(), im_0_w * im_0_h * 3); // Do this by copying last image to this image
				else if(last_disposal_mode == DISPOSE_BACKGROUND) // Set area to background color
				{
					// This seems to be how gif files encode transparency.
					// For now just render as green.  TODO: handle this properly (Return a RGBA format etc..)
					for(int q=0; q<im_0_w * im_0_h; ++q)
					{
						uint8* const p = image_map->getPixel(q);
						p[0] = 0;
						p[1] = 255;
						p[2] = 0;
					}
				}
				else if(last_disposal_mode == DISPOSE_PREVIOUS) // Restore to previous content
				{
					// not supported, just copy image 0 for now.
					std::memcpy(image_map->getData(), sequence->images[0]->getData(), im_0_w * im_0_h * 3);
				}
			}

			const GifColorType* const colours = colour_map->Colors;

			const int start_x = myMax(0, image_i->ImageDesc.Left);
			const int start_y = myMax(0, image_i->ImageDesc.Top );
			const int end_x = myMin((int)im_0_w, image_i->ImageDesc.Left + (int)w);
			const int end_y = myMin((int)im_0_h, image_i->ImageDesc.Top  + (int)h);

			for(int y=start_y; y < end_y; ++y)
				for(int x=start_x; x < end_x; ++x)
				{
					uint8* const dest = image_map->getPixel(x, y);
					const int src_i = (y - image_i->ImageDesc.Top) * image_i->ImageDesc.Width + (x - image_i->ImageDesc.Left);
					const uint8 c = image_i->RasterBits[src_i];

					if(c != transparent_color)
					{
						dest[0] = colours[c].Red;
						dest[1] = colours[c].Green;
						dest[2] = colours[c].Blue;
					}
				}

			last_disposal_mode = disposal_mode;
		}

		error_code = 0;
		DGifCloseFile(gif_file, &error_code);

		return sequence;
	}
	catch(ImFormatExcep& e)
	{
		// Call DGifCloseFile to clean up any memory allocated by DGifOpenFileHandle and DGifSlurp, and to close the file descriptor.
		error_code = 0;
		DGifCloseFile(gif_file, &error_code);
		throw e;
	}
}


static void* tryMalloc(size_t size)
{
	void* p = malloc(size);
	if(!p)
		throw glare::Exception("malloc failed.");
	return p;
}


void GIFDecoder::resizeGIF(const std::string& path, const std::string& dest_path, int max_new_dim)
{
	// Do this file descriptor malarkey so that we can handle Unicode paths.
	const int file_descriptor = FileUtils::openFileDescriptor(path, O_RDONLY);
	if(file_descriptor == -1)
		throw ImFormatExcep("failed to open gif file '" + path + "'");

	// NOTE: if DGifOpenFileHandle fails to open the gif file, it closes the handle itself.
	int error_code = 0;
	GifFileType* gif_file = DGifOpenFileHandle(file_descriptor, &error_code);
	if(gif_file == NULL)
		throw ImFormatExcep("failed to open gif file '" + path + "': " + GifErrorString(error_code));




	const int write_file_descriptor = FileUtils::openFileDescriptor(dest_path, O_WRONLY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
	if(write_file_descriptor == -1)
		throw ImFormatExcep("failed to open gif file '" + dest_path + "': " + PlatformUtils::getLastErrorString());

	GifFileType* dest_gif_file = EGifOpenFileHandle(write_file_descriptor, &error_code);
	if(dest_gif_file == NULL)
		throw ImFormatExcep("failed to open gif file '" + path + "': " + GifErrorString(error_code));


	try
	{
		int res = DGifSlurp(gif_file);
		if(res != GIF_OK)
			throw ImFormatExcep("failed to process gif file: " + errorString(res));

		if(gif_file->ImageCount < 1)
			throw ImFormatExcep("invalid ImageCount (< 1)");
		if(gif_file->ImageCount > 1000000) // Sanity check ImageCount, avoid wraparound etc..
			throw ImFormatExcep("invalid ImageCount (> 1000000)");

		const int max_dim = myMax(gif_file->SWidth, gif_file->SHeight);
		const int scale_factor = myMax(1, (int)std::ceil((double)max_dim / max_new_dim)); // scale reduction factor

		dest_gif_file->SWidth = gif_file->SWidth / scale_factor;
		dest_gif_file->SHeight = gif_file->SHeight / scale_factor;
		dest_gif_file->SColorResolution = gif_file->SColorResolution;
		dest_gif_file->SBackGroundColor = gif_file->SBackGroundColor;
		dest_gif_file->AspectByte = gif_file->AspectByte;

		if(gif_file->SColorMap)
		{
			assert(dest_gif_file->SColorMap == NULL);
			dest_gif_file->SColorMap = (ColorMapObject*)tryMalloc(sizeof(ColorMapObject));
			*dest_gif_file->SColorMap = *gif_file->SColorMap; // Copy all fields
			dest_gif_file->SColorMap->Colors = (GifColorType*)tryMalloc(gif_file->SColorMap->ColorCount * sizeof(GifColorType));
			std::memcpy(dest_gif_file->SColorMap->Colors, gif_file->SColorMap->Colors, gif_file->SColorMap->ColorCount * sizeof(GifColorType));
		}

		dest_gif_file->ImageCount = gif_file->ImageCount;

		dest_gif_file->ExtensionBlockCount = gif_file->ExtensionBlockCount;
		assert(dest_gif_file->ExtensionBlocks == NULL);
		dest_gif_file->ExtensionBlocks = (ExtensionBlock*)tryMalloc(sizeof(ExtensionBlock) * dest_gif_file->ExtensionBlockCount);
		for(int i=0; i<dest_gif_file->ExtensionBlockCount; ++i)
		{
			dest_gif_file->ExtensionBlocks[i] = gif_file->ExtensionBlocks[i];
			dest_gif_file->ExtensionBlocks[i].Bytes = (GifByteType*)tryMalloc(gif_file->ExtensionBlocks[i].ByteCount);
			std::memcpy(dest_gif_file->ExtensionBlocks[i].Bytes, gif_file->ExtensionBlocks[i].Bytes, gif_file->ExtensionBlocks[i].ByteCount);
		}

		dest_gif_file->SavedImages = (SavedImage*)tryMalloc(sizeof(SavedImage) * gif_file->ImageCount);

		// Giflib currently leaks this memory, so keep a track of it and manually free it.  (See https://sourceforge.net/p/giflib/bugs/156/)
		int dest_ExtensionBlockCount = dest_gif_file->ExtensionBlockCount;
		ExtensionBlock* dest_ExtensionBlocks = dest_gif_file->ExtensionBlocks;
		SavedImage* dest_saved_images = dest_gif_file->SavedImages;

		for(int im_i=0; im_i<gif_file->ImageCount; ++im_i)
		{
			const SavedImage* const image_i = &gif_file->SavedImages[im_i];

			// Get image dimensions
			if(image_i->ImageDesc.Width <= 0 || image_i->ImageDesc.Width > 1000000)
				throw ImFormatExcep("Invalid image width.");
			if(image_i->ImageDesc.Height <= 0 || image_i->ImageDesc.Height > 1000000)
				throw ImFormatExcep("Invalid image height.");

			const size_t w = (size_t)image_i->ImageDesc.Width;
			const size_t h = (size_t)image_i->ImageDesc.Height;

			
			SavedImage* dest_im_i = &dest_gif_file->SavedImages[im_i];

			//--------------------------- Set ImageDesc ---------------------------
			dest_im_i->ImageDesc.Top = image_i->ImageDesc.Top / scale_factor;
			dest_im_i->ImageDesc.Left = image_i->ImageDesc.Left / scale_factor;
			dest_im_i->ImageDesc.Width = image_i->ImageDesc.Width / scale_factor;
			dest_im_i->ImageDesc.Height = image_i->ImageDesc.Height / scale_factor;
			dest_im_i->ImageDesc.Interlace = image_i->ImageDesc.Interlace;

			//--------------------------- Set RasterBits ---------------------------
			const size_t new_w = w / (size_t)scale_factor;
			const size_t new_h = h / (size_t)scale_factor;
			uint8* new_raster_bits = (uint8*)tryMalloc(new_w * new_h);

			for(size_t x=0; x<new_w; ++x)
				for(size_t y=0; y<new_h; ++y)
				{
					const size_t srcx = x * (size_t)scale_factor;
					const size_t srcy = y * (size_t)scale_factor;
					const size_t src_i = (srcy * w) + srcx;
					assert(src_i >= 0 && src_i < w * h);
					new_raster_bits[(y * new_w) + x] = image_i->RasterBits[src_i];
				}

			dest_im_i->RasterBits = new_raster_bits;

			//--------------------------- Set ExtensionBlockCount and ExtensionBlocks ---------------------------
			dest_im_i->ExtensionBlockCount = image_i->ExtensionBlockCount;
			dest_im_i->ExtensionBlocks = (ExtensionBlock*)tryMalloc(sizeof(ExtensionBlock) * image_i->ExtensionBlockCount);
			for(int i=0; i<dest_im_i->ExtensionBlockCount; ++i)
			{
				dest_im_i->ExtensionBlocks[i] = image_i->ExtensionBlocks[i];
				dest_im_i->ExtensionBlocks[i].Bytes = (GifByteType*)tryMalloc(image_i->ExtensionBlocks[i].ByteCount);
				std::memcpy(dest_im_i->ExtensionBlocks[i].Bytes, image_i->ExtensionBlocks[i].Bytes, image_i->ExtensionBlocks[i].ByteCount);
			}

			if(image_i->ImageDesc.ColorMap)
			{
				dest_im_i->ImageDesc.ColorMap = (ColorMapObject*)tryMalloc(sizeof(ColorMapObject));
				*dest_im_i->ImageDesc.ColorMap = *image_i->ImageDesc.ColorMap; // Copy all fields
				dest_im_i->ImageDesc.ColorMap->Colors = (GifColorType*)tryMalloc(sizeof(GifColorType) * image_i->ImageDesc.ColorMap->ColorCount);
				std::memcpy(dest_im_i->ImageDesc.ColorMap->Colors, image_i->ImageDesc.ColorMap->Colors, sizeof(GifColorType) * image_i->ImageDesc.ColorMap->ColorCount);
			}
			else
				dest_im_i->ImageDesc.ColorMap = NULL;
		}

		if(EGifSpew(dest_gif_file) == GIF_ERROR) // closes file also
			throw glare::Exception("EGifSpew failed: " + errorString(dest_gif_file->Error));

		// Manuall free extension blocks
		GifFreeExtensions(&dest_ExtensionBlockCount, &dest_ExtensionBlocks);

		// Manually free dest_saved_images (adapted from GifFreeSavedImages())
		for(int i=0; i<gif_file->ImageCount; ++i)
		{
			SavedImage* sp = &dest_saved_images[i];

			if (sp->ImageDesc.ColorMap != NULL) {
				GifFreeMapObject(sp->ImageDesc.ColorMap);
				sp->ImageDesc.ColorMap = NULL;
			}

			if (sp->RasterBits != NULL)
				free((char *)sp->RasterBits);

			GifFreeExtensions(&sp->ExtensionBlockCount, &sp->ExtensionBlocks);
		}
		free(dest_saved_images);

		// Free source GIF object
		error_code = 0;
		DGifCloseFile(gif_file, &error_code);
	}
	catch(ImFormatExcep& e)
	{
		// Call DGifCloseFile to clean up any memory allocated by DGifOpenFileHandle and DGifSlurp, and to close the file descriptor.
		error_code = 0;
		DGifCloseFile(gif_file, &error_code);
		throw e;
	}
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/ConPrint.h"


void GIFDecoder::test()
{
	conPrint("GIFDecoder::test()");

	// Test resizeGIF()
	try
	{
		{
			const std::string resized_gif_path = PlatformUtils::getTempDirPath() + "/cow_resized.gif";
			GIFDecoder::resizeGIF(TestUtils::getTestReposDir() + "/testfiles/gifs/https_58_47_47media.giphy.com_47media_47X93e1eC2J2hjy_47giphy.gif", resized_gif_path, 256);
		
			Reference<Map2D> im = GIFDecoder::decode(resized_gif_path);
			testAssert(im->getMapWidth() == 620 / 3);
			testAssert(im->getMapHeight() == 409 / 3);
			testAssert(im->getBytesPerPixel() == 3);
		}

		{
			const std::string resized_gif_path = PlatformUtils::getTempDirPath() + "/spiral_resized.gif";
			GIFDecoder::resizeGIF(TestUtils::getTestReposDir() + "/testfiles/gifs/https_58_47_47media.giphy.com_47media_47ppTMXv7gqwCDm_47giphy.gif", resized_gif_path, 256);

			Reference<Map2D> im = GIFDecoder::decode(resized_gif_path);
			testAssert(im->getMapWidth() == 479 / 2);
			testAssert(im->getMapHeight() == 479 / 2);
			testAssert(im->getBytesPerPixel() == 3);
		}
		
		// GIFDecoder::resizeGIF("C:\\Users\\nick\\AppData\\Roaming\\Cyberspace\\resources\\The_Third_Eye__420_gif_14700668946248627369.gif", TestUtils::getTestReposDir() + "/testfiles/gifs/The_Third_Eye_resized.gif", 256);
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	try
	{
		Reference<Map2D> im;

		// im = GIFDecoder::decodeImageSequence("C:\\Users\\nick\\Downloads\\rabbitplayhouse.gif"); // keyboard cat

		// im = GIFDecoder::decodeImageSequence("E:\\video\\2GU.gif"); // keyboard cat

		// im = GIFDecoder::decodeImageSequence("E:\\video\\E0m6.gif");

		// This files uses a local palette.
		im = GIFDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/gifs/https_58_47_47media.giphy.com_47media_47X93e1eC2J2hjy_47giphy.gif");
		testAssert(im->getMapWidth() == 620);
		testAssert(im->getMapHeight() == 409);
		testAssert(im->getBytesPerPixel() == 3);

		im = GIFDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/gifs/fire.gif");
		testAssert(im->getMapWidth() == 30);
		testAssert(im->getMapHeight() == 60);
		testAssert(im->getBytesPerPixel() == 3);

		// Test Unicode path
		const std::string euro = "\xE2\x82\xAC";
		Reference<Map2D> im2 = GIFDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/gifs/" + euro + ".gif");
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// This file has a 'virtual canvas' size of 480x480, but the actual image (image 0 at least) is 479x479.
	try
	{
		Reference<Map2D> im = GIFDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/gifs/https_58_47_47media.giphy.com_47media_47ppTMXv7gqwCDm_47giphy.gif");
		testAssert(im->getMapWidth() == 479);
		testAssert(im->getMapHeight() == 479);
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Test that failure to load an image is handled gracefully.

	// Try with an invalid path
	try
	{
		GIFDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/gifs/NO_SUCH_FILE.gif");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	// Try with a JPG file
	try
	{
		GIFDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/checker.jpg");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	conPrint("GIFDecoder::test() done.");
}


#endif // BUILD_TESTS
