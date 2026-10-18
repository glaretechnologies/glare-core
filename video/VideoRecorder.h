/*=====================================================================
VideoRecorder.h
---------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/ThreadSafeQueue.h"
#include "../utils/AtomicInt.h"
#include "../utils/Reference.h"
#include "../utils/Vector.h"
#include "../utils/Platform.h"
#include <string>
#include <vector>


/*=====================================================================
VideoEncodeJob
--------------
Encodes the frames of a recording made by VideoRecorder (see below) to a video file, while the recording is in progress.
VideoRecorder queues frames on the recording thread, and run() encodes them on another thread.
=====================================================================*/
class VideoEncodeJob : public ThreadSafeRefCounted
{
public:
	struct Frame : public ThreadSafeRefCounted
	{
		Frame() : slot(0), end_of_recording(false) {}

		js::Vector<uint8, 16> data; // 8-bit RGB, top row first, tightly packed.
		int64 slot; // Output frame slot index.  For the end of recording marker, the number of output slots in the recording.
		bool end_of_recording; // If true, this is not a frame but marks the end of the recording, and has no data.
	};
	typedef Reference<Frame> FrameRef;

	VideoEncodeJob(const std::string& path, int width, int height, double fps, uint32 bitrate);

	// Encodes queued frames until the recording is stopped, then finalises the file.  Blocks until then.
	// Encodes at a constant frame rate of fps.  Slots with no frame repeat the previous frame, up to the end of the recording.  For MP4 files, the moov box is moved to the start of the file.
	// Throws glare::Exception on failure, in which case any file this job created is deleted.
	void run();

	bool hasFailed() const { return failed != 0; } // True if run() has failed.  Can be called from any thread.

	// A frame near the middle of the video, for use as a poster image.  Set when run() returns successfully.
	FrameRef getPosterFrame() const { return poster_frame; }

	const std::string path;
	const int width, height; // Frame size
	const double fps;
	const uint32 bitrate; // Bits per second

private:
	friend class VideoRecorder;
	GLARE_DISABLE_COPY(VideoEncodeJob)

	void addPosterCandidate(const FrameRef& frame);

	ThreadSafeQueue<FrameRef> queue; // Frames to encode, in slot order, followed by an end of recording marker.
	glare::AtomicInt failed;

	// Frames sampled at least poster_candidate_interval slots apart, spanning the video so far.  Only accessed by run().
	std::vector<FrameRef> poster_candidates;
	int64 poster_candidate_interval;

	FrameRef poster_frame;
};

typedef Reference<VideoEncodeJob> VideoEncodeJobRef;


/*=====================================================================
VideoRecorder
-------------
Records captured frames to a video file.  Frames are encoded while recording, by a VideoEncodeJob running on another thread,
so memory use doesn't grow with the length of the recording while the encoder keeps up.

The output has a constant frame rate, while the application's frame rate may vary.  Output time is divided into
slots of 1/fps seconds, and the first rendered frame in each slot is used.  shouldCaptureFrame() makes this
decision before the frame is read back from the GPU, so frames that won't be used are never read back.
Slots with no frame (when rendering slower than fps) repeat the previous frame.

If the encoder falls behind, frames wait in memory until it catches up.  If the waiting frames reach MAX_QUEUED_BYTES,
further frames are dropped, and their slots repeat the previous frame.
=====================================================================*/
class VideoRecorder
{
public:
	VideoRecorder();
	~VideoRecorder(); // Stops any recording in progress.

	static const size_t MAX_QUEUED_BYTES = 2048ull * 1024 * 1024; // About 26 seconds of 1280x720 frames at 30 fps, or 13 seconds at 60 fps.

	// Starts recording to a video file at path (e.g. "something.mp4").  bitrate is in bits per second.
	// Returns the encode job for the recording.  The caller should call its run() method on another thread.
	VideoEncodeJobRef startRecording(const std::string& path, int width, int height, double cur_time, double fps, double max_duration_s, uint32 bitrate);

	// Ends the recording at stop_time.  The encode job then encodes the remaining queued frames, repeats the last frame up to stop_time
	// (or the max duration if earlier), and finalises the file.
	// Frames from grabs still in flight should be added with addFrame() before calling this.
	void stopRecording(double stop_time);

	bool isRecording() const { return job.nonNull(); }
	double getElapsedTime(double cur_time) const { return cur_time - start_time; }
	bool reachedMaxDuration(double cur_time) const { return getElapsedTime(cur_time) >= max_duration; }
	double getMaxDuration() const { return max_duration; } // Max duration of the current or last recording, in seconds.
	bool encodingFailed() const { return job.nonNull() && job->hasFailed(); }
	size_t getNumDroppedFrames() const { return num_dropped_frames; } // Frames dropped in the current or last recording because the encoder fell behind.

	// Returns true if a frame rendered at frame_time is the first in a new output slot, and so should be captured.  Claims the slot if so.
	bool shouldCaptureFrame(double frame_time);

	// Queues a captured frame for encoding, taking the data from frame_data (by swapping).  Data is 8-bit RGB, top row first, tightly packed.
	// Frames must be added in order of frame_time.  Frames are ignored if not recording, or if their size differs from the recording size.
	void addFrame(js::Vector<uint8, 16>& frame_data, int width, int height, double frame_time);

private:
	GLARE_DISABLE_COPY(VideoRecorder)

	int64 slotForTime(double t) const;
	void endRecording(int64 num_slots); // Queues the end of recording marker for the encode job.

	VideoEncodeJobRef job; // Encode job for the recording in progress, or null if not recording.

	double start_time;
	double fps;
	double max_duration;
	int64 last_claimed_slot;
	size_t num_dropped_frames;
};
