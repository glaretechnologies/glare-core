/*=====================================================================
MP4FastStart.h
--------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "../utils/Platform.h"
#include <string>
#include <vector>


/*=====================================================================
MP4FastStart
------------
Moves the 'moov' box (the index: codec info, sample sizes and offsets) of an MP4 file
before the 'mdat' box (the sample data), so that playback can start before the whole
file has been downloaded.  Like ffmpeg's '-movflags +faststart'.

Moving moov shifts the mdat data later in the file by the size of moov, so every chunk
offset in the stco and co64 boxes that points into the shifted data is increased by that amount.

Fragmented MP4 files (with 'moof' boxes) aren't supported.

The input is treated as untrusted: all box sizes and offsets are bounds-checked, and
glare::Exception is thrown if the file is malformed.
=====================================================================*/
namespace MP4FastStart
{


// If moov is after the first mdat, writes the rearranged file to data_out and returns true.
// If moov is already before the first mdat, returns false and leaves data_out unchanged.
// Throws glare::Exception if the data isn't an MP4 file with the structure described above.
bool makeFastStart(const uint8* data, size_t data_size, std::vector<uint8>& data_out);

// Rewrites the file at path if moov is after the first mdat.  Returns true if the file was changed.
// Throws glare::Exception on failure.
bool makeFileFastStart(const std::string& path);


void test();


} // end namespace MP4FastStart
