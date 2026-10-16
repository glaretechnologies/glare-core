/*=====================================================================
VideoRecorder.cpp
-----------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "VideoRecorder.h"


#include "VideoWriter.h"
#include "MP4FastStart.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/Timer.h"
#include "../maths/mathstypes.h"
#include <cmath>


static const size_t MAX_POSTER_CANDIDATES = 16;


VideoEncodeJob::VideoEncodeJob(const std::string& path_, int width_, int height_, double fps_, uint32 bitrate_)
:	path(path_),
	width(width_),
	height(height_),
	fps(fps_),
	bitrate(bitrate_),
	failed(0),
	poster_candidate_interval(1)
{}


void VideoEncodeJob::run()
{
	// If a file already exists at path, it isn't deleted on failure, since it wasn't created by this job.
	const bool file_existed = FileUtils::fileExists(path);
	try
	{
		Timer timer;

		size_t num_frames = 0;
		int64 next_slot = 0; // Slot of the next output frame to write.
		{
			VidParams params;
			params.bitrate = bitrate;
			params.width = (uint32)width;
			params.height = (uint32)height;
			params.fps = fps;
			params.standard = VidParams::CompressionStandard_H264; // HEVC isn't playable in many browsers.

			VideoWriterRef writer = VideoWriter::create(path, params);

			const size_t row_stride_B = (size_t)width * 3;
			FrameRef prev_frame;
			while(1)
			{
				FrameRef frame = queue.dequeue();
				if(frame->end_of_recording)
				{
					// Repeat the last frame up to the end of the recording, so the video isn't shortened if no frames were captured near the end.
					if(prev_frame)
						for(; next_slot < frame->slot; ++next_slot)
							writer->writeFrame(prev_frame->data.data(), row_stride_B);
					break;
				}

				if(frame->slot < next_slot)
					continue;

				// Fill any empty slots before this frame with the previous frame, or with this frame if it's the first.
				const Frame* fill_frame = prev_frame ? prev_frame.ptr() : frame.ptr();
				for(; next_slot < frame->slot; ++next_slot)
					writer->writeFrame(fill_frame->data.data(), row_stride_B);

				writer->writeFrame(frame->data.data(), row_stride_B);
				next_slot++;

				addPosterCandidate(frame);
				prev_frame = frame;
				num_frames++;
			}

			if(num_frames == 0)
				throw glare::Exception("No frames were captured.");

			writer->finalise();
		} // Close the file.

		// Move the MP4 index (moov box) to the start of the file, so playback in browsers can start before the whole file is downloaded.
		if(hasExtension(path, "mp4"))
			MP4FastStart::makeFileFastStart(path);

		// Use the candidate nearest the middle of the video as the poster frame.
		const int64 mid_slot = next_slot / 2;
		poster_frame = poster_candidates[0];
		for(size_t i=1; i<poster_candidates.size(); ++i)
			if(std::abs(poster_candidates[i]->slot - mid_slot) < std::abs(poster_frame->slot - mid_slot))
				poster_frame = poster_candidates[i];
		poster_candidates.clear();

		conPrint("VideoEncodeJob: encoded " + toString(num_frames) + " unique frames to " + toString(next_slot) + " output frames (" +
			doubleToStringNDecimalPlaces(next_slot / fps, 1) + " s) at " + toString(width) + "x" + toString(height) + ", job ran for " + timer.elapsedStringNSigFigs(3));
	}
	catch(glare::Exception&)
	{
		failed = 1;
		poster_candidates.clear();
		queue.clear();

		try
		{
			if(!file_existed && FileUtils::fileExists(path))
				FileUtils::deleteFile(path);
		}
		catch(glare::Exception&)
		{}

		throw;
	}
}


void VideoEncodeJob::addPosterCandidate(const FrameRef& frame)
{
	if(!poster_candidates.empty() && (frame->slot - poster_candidates.back()->slot < poster_candidate_interval))
		return;

	poster_candidates.push_back(frame);

	if(poster_candidates.size() > MAX_POSTER_CANDIDATES)
	{
		// Keep every second candidate, and double the sampling interval.
		size_t num_kept = 0;
		for(size_t i=0; i<poster_candidates.size(); i += 2)
			poster_candidates[num_kept++] = poster_candidates[i];
		poster_candidates.resize(num_kept);

		poster_candidate_interval *= 2;
	}
}


VideoRecorder::VideoRecorder()
:	start_time(0),
	fps(30),
	max_duration(0),
	last_claimed_slot(-1),
	num_dropped_frames(0)
{}


VideoRecorder::~VideoRecorder()
{
	endRecording(/*num slots=*/0);
}


VideoEncodeJobRef VideoRecorder::startRecording(const std::string& path, int width, int height, double cur_time, double fps_, double max_duration_s, uint32 bitrate)
{
	stopRecording(cur_time);

	job = new VideoEncodeJob(path, width, height, fps_, bitrate);

	start_time = cur_time;
	fps = fps_;
	max_duration = max_duration_s;
	last_claimed_slot = -1;
	num_dropped_frames = 0;

	return job;
}


void VideoRecorder::stopRecording(double stop_time)
{
	const int64 max_num_slots = (int64)std::ceil(max_duration * fps);
	endRecording(/*num slots=*/myClamp<int64>(slotForTime(stop_time), 0, max_num_slots));
}


void VideoRecorder::endRecording(int64 num_slots)
{
	if(job)
	{
		VideoEncodeJob::FrameRef end_marker = new VideoEncodeJob::Frame();
		end_marker->end_of_recording = true;
		end_marker->slot = num_slots;
		job->queue.enqueue(end_marker);

		job = nullptr;
	}
}


int64 VideoRecorder::slotForTime(double t) const
{
	return (int64)std::floor((t - start_time) * fps);
}


bool VideoRecorder::shouldCaptureFrame(double frame_time)
{
	if(!isRecording() || reachedMaxDuration(frame_time))
		return false;

	const int64 slot = slotForTime(frame_time);
	if(slot > last_claimed_slot)
	{
		last_claimed_slot = slot;
		return true;
	}
	return false;
}


void VideoRecorder::addFrame(js::Vector<uint8, 16>& frame_data, int frame_width, int frame_height, double frame_time)
{
	if(job.isNull() || frame_width != job->width || frame_height != job->height)
		return;

	const size_t frame_size_B = (size_t)frame_width * frame_height * 3;
	if((job->queue.size() + 1) * frame_size_B > MAX_QUEUED_BYTES)
	{
		num_dropped_frames++;
		return;
	}

	VideoEncodeJob::FrameRef frame = new VideoEncodeJob::Frame();
	frame->data.swapWith(frame_data);
	frame->slot = myMax<int64>(0, slotForTime(frame_time));
	job->queue.enqueue(frame);
}
