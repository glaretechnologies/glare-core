/*=====================================================================
AVFVideoWriter.h
----------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "VideoWriter.h"
#include "ComObHandle.h"
#include "IncludeWindows.h"
#include <string>


#ifdef OSX


/*=====================================================================
AVFVideoWriter
--------------
Apple AVFoundation video writer.
=====================================================================*/
class AVFVideoWriter : public VideoWriter
{
public:
	AVFVideoWriter(const std::string& URL, const VidParams& vid_params); // Throws Indigo::Exception
	~AVFVideoWriter();

	static void test();

	// RGB data
	virtual void writeFrame(const uint8* data, size_t source_row_stride_B);

	virtual void finalise();

	const VidParams& getVidParams() const { return vid_params; }
private:
	uint64 frame_index;
	VidParams vid_params;
	
	void* m_video_writer;
	void* m_writer_input;
	void* m_adaptor;
};


#endif // OSX
