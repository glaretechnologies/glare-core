/*=====================================================================
AVFVideoWriter.h
----------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "VideoWriter.h"
#include <string>


#ifdef OSX


/*=====================================================================
AVFVideoWriter
--------------
Apple AVFoundation video writer.
writeFrame() blocks until the encoder is ready for more data, so should be called on a thread other than the main thread.
=====================================================================*/
class AVFVideoWriter : public VideoWriter
{
public:
	AVFVideoWriter(const std::string& path, const VidParams& vid_params); // Throws glare::Exception
	~AVFVideoWriter();

	static void test();

	// data is 8-bit RGB, top row first.
	virtual void writeFrame(const uint8* data, size_t source_row_stride_B) override;

	virtual void finalise() override;

	const VidParams& getVidParams() const { return vid_params; }
private:
	void releaseObjects();

	uint64 frame_index;
	VidParams vid_params;

	void* m_video_writer; // AVAssetWriter*
	void* m_writer_input; // AVAssetWriterInput*
	void* m_adaptor; // AVAssetWriterInputPixelBufferAdaptor*
};


#endif // OSX
