/*=====================================================================
FileWatcherThread.h
-------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include <MessageableThread.h>
#include <EventFD.h>
#include <AtomicInt.h>
#include <string>
#include <vector>
#include <functional>


/*=====================================================================
FileWatcherThread
-----------------
Watches for changes to files, 
and calls a function when the files change.
=====================================================================*/
class FileWatcherThread : public MessageableThread
{
public:
	struct WatchDir
	{
		WatchDir() : watch_subtree(false) {}
		WatchDir(const std::string& dir_path_, bool watch_subtree_ = false) : dir_path(dir_path_), watch_subtree(watch_subtree_) {}

		std::string dir_path;
		bool watch_subtree;

		size_t wait_handle_index; // internal (Windows)
		int watch_descriptor; // internal (Linux)
	};

	// A set of directories and a function to call when a file in them changes.
	struct WatchGroup
	{
		std::vector<WatchDir> dirs;
		std::function<void()> on_change_func;
	};

	struct FileWatcherThreadArgs
	{
		std::vector<WatchGroup> watch_groups;
	};

	FileWatcherThread(FileWatcherThreadArgs args);
	virtual ~FileWatcherThread();

	virtual void doRun() override;

	virtual void kill() override;

private:
	glare::AtomicInt should_quit;

	FileWatcherThreadArgs args;

#if defined(_WIN32)
	/*HANDLE*/void* kill_event;
#else
	EventFD kill_event_fd; // Written to by kill() to wake up the poll() in doRun().
#endif
};
