/*=====================================================================
FileWatcherThread.cpp
---------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "FileWatcherThread.h"


#include <MemMappedFile.h>
#include <Exception.h>
#include <StringUtils.h>
#include <ConPrint.h>
#include <FileUtils.h>
#include <PlatformUtils.h>
#include <RuntimeCheck.h>
#include <Lock.h>
#include <ResponseUtils.h>
#ifdef __linux__
#include <unistd.h>
#include <sys/inotify.h>
#include <linux/limits.h>
#include <poll.h>
#include <errno.h>
#include <cstring>
#endif


FileWatcherThread::FileWatcherThread(FileWatcherThreadArgs args_)
:	args(args_)
{
#if defined(_WIN32)
	kill_event = CreateEvent(/*security attribs=*/NULL, /*manual reset=*/TRUE, /*initial state=*/FALSE, /*name=*/NULL);
	if(!kill_event)
		throw glare::Exception("CreateEvent failed: " + PlatformUtils::getLastErrorString());
#endif
	
}


FileWatcherThread::~FileWatcherThread()
{
#if defined(_WIN32)
	CloseHandle(kill_event);
#endif
}


#if defined(_WIN32)
static HANDLE makeWaitHandleForDir(const std::string& dir, bool watch_subtree)
{
	HANDLE wait_handle = FindFirstChangeNotification(StringUtils::UTF8ToPlatformUnicodeEncoding(dir).c_str(), /*watch subtree=*/watch_subtree,
		FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME // FILE_NOTIFY_CHANGE_FILE_NAME = file created, deleted or renamed.
	);
	if(wait_handle == INVALID_HANDLE_VALUE)
		throw glare::Exception("FindFirstChangeNotification failed: " + PlatformUtils::getLastErrorString());
	return wait_handle;
}
#elif defined(OSX)
 // TODO
#else // else on Linux:
static int makeWatchDescriptorForDir(int inotify_fd, const std::string& dir)
{
	const int watch_descriptor = inotify_add_watch(inotify_fd, dir.c_str(), IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_TO);
	if(watch_descriptor == -1)
		throw glare::Exception("inotify_add_watch failed: " + PlatformUtils::getLastErrorString());
	return watch_descriptor;
}
#endif


void FileWatcherThread::doRun()
{
	try
	{
#if defined(_WIN32)
		std::vector<HANDLE> wait_handles;
		for(size_t g=0; g < args.watch_groups.size(); ++g)
		{
			WatchGroup& group = args.watch_groups[g];
			for(size_t i=0; i<group.dirs.size(); ++i)
			{
				group.dirs[i].wait_handle_index = wait_handles.size();
				wait_handles.push_back(makeWaitHandleForDir(group.dirs[i].dir_path, group.dirs[i].watch_subtree));
			}
		}
		wait_handles.push_back(kill_event); // Add a 'kill' event as the last wait handle

		while(!should_quit)
		{
			const DWORD res = WaitForMultipleObjects(/*count=*/(DWORD)wait_handles.size(), wait_handles.data(), /*wait all=*/FALSE, /*wait time (ms)=*/INFINITE);
			if(res >= WAIT_OBJECT_0 && res < WAIT_OBJECT_0 + (DWORD)wait_handles.size())
			{
				const size_t wait_handle_i = res - WAIT_OBJECT_0;

				runtimeCheck(wait_handle_i < wait_handles.size());
				if(wait_handle_i == wait_handles.size() - 1) // This is the kill event, so break loop.
					break; 

				for(size_t g=0; g < args.watch_groups.size(); ++g)
				{
					WatchGroup& group = args.watch_groups[g];
					for(size_t di=0; di<group.dirs.size(); ++di)
						if(group.dirs[di].wait_handle_index == wait_handle_i)
						{
							// Found the dir that changed
							group.on_change_func(); // Call the on-change function
						}
				}

				if(FindNextChangeNotification(wait_handles[wait_handle_i]) == 0)
					throw glare::Exception("FindNextChangeNotification failed: " + PlatformUtils::getLastErrorString());
			}
			else if(res == WAIT_TIMEOUT)
			{}
			else
				throw glare::Exception("Unhandled WaitForSingleObject result");
		}

		// Close the change notification handles.  (The last handle is kill_event, which is closed in the destructor.)
		for(size_t i=0; i + 1 < wait_handles.size(); ++i)
			FindCloseChangeNotification(wait_handles[i]);
#elif defined(OSX)
		
		// TODO

#else // else on Linux:

		const int inotify_fd = inotify_init();
		if(inotify_fd == -1)
			throw glare::Exception("inotify_init failed: " + PlatformUtils::getLastErrorString());

		// NOTE: inotify watches only the directory itself, not subdirectories, so watch_subtree is ignored on Linux.
		for(size_t g=0; g < args.watch_groups.size(); ++g)
		{
			WatchGroup& group = args.watch_groups[g];
			for(size_t i=0; i<group.dirs.size(); ++i)
				group.dirs[i].watch_descriptor = makeWatchDescriptorForDir(inotify_fd, group.dirs[i].dir_path);
		}

		// read() on an inotify fd returns one or more whole events, packed one after another.  Each event is a struct inotify_event
		// followed by event.len bytes of filename (event.len includes padding).  The buffer must hold at least one event with
		// the longest possible name, or read() fails with EINVAL.  See https://man7.org/linux/man-pages/man7/inotify.7.html
		std::vector<uint8> buf(16 * (sizeof(struct inotify_event) + NAME_MAX + 1));
		std::vector<bool> group_changed(args.watch_groups.size());

		while(!should_quit)
		{
			// Block until a watched dir changes, or kill() writes to kill_event_fd.
			struct pollfd poll_fds[2];
			poll_fds[0].fd = inotify_fd;
			poll_fds[0].events = POLLIN;
			poll_fds[0].revents = 0;
			poll_fds[1].fd = kill_event_fd.efd;
			poll_fds[1].events = POLLIN;
			poll_fds[1].revents = 0;

			const int num = poll(poll_fds, /*num fds=*/2, /*timeout (ms)=*/-1);
			if(num == -1)
			{
				if(errno == EINTR) // Interrupted by a signal, just poll again.
					continue;
				throw glare::Exception("FileWatcherThread: poll failed: " + PlatformUtils::getLastErrorString());
			}

			if(poll_fds[1].revents != 0) // This is the kill event, so break loop.
				break;

			if(poll_fds[0].revents & POLLIN)
			{
				const ssize_t length = read(inotify_fd, buf.data(), buf.size());
				if(length == -1)
					throw glare::Exception("read failed: " + PlatformUtils::getLastErrorString());

				// Work out which groups had a dir change, so each group's on-change function is called at most once per read.
				std::fill(group_changed.begin(), group_changed.end(), false);
				size_t offset = 0;
				while(offset < (size_t)length)
				{
					// Copy the fixed-size part of the event out of the buffer.  (We don't need the filename that follows it.)
					struct inotify_event event;
					runtimeCheck(offset + sizeof(event) <= (size_t)length);
					std::memcpy(&event, &buf[offset], sizeof(event));
					offset += sizeof(event) + event.len;
					runtimeCheck(offset <= (size_t)length);

					if(event.mask & IN_Q_OVERFLOW) // Events were dropped (event.wd is -1), so we don't know what changed: treat all groups as changed.
						std::fill(group_changed.begin(), group_changed.end(), true);

					for(size_t g=0; g < args.watch_groups.size(); ++g)
					{
						const WatchGroup& group = args.watch_groups[g];
						for(size_t di=0; di<group.dirs.size(); ++di)
							if(group.dirs[di].watch_descriptor == event.wd)
								group_changed[g] = true; // Found the dir that changed
					}
				}

				for(size_t g=0; g < args.watch_groups.size(); ++g)
					if(group_changed[g])
						args.watch_groups[g].on_change_func(); // Call the on-change function
			}
		}

		for(size_t g=0; g < args.watch_groups.size(); ++g)
		{
			const WatchGroup& group = args.watch_groups[g];
			for(size_t i=0; i<group.dirs.size(); ++i)
				/*int res =*/ inotify_rm_watch(inotify_fd, group.dirs[i].watch_descriptor);
		}

		/*int res =*/ close(inotify_fd);

#endif
	}
	catch(glare::Exception& e)
	{
		// TODO: cleanup watch events/handles. Do with RAII guards.

		conPrint("FileWatcherThread error: " + e.what());
	}
}


void FileWatcherThread::kill()
{
	should_quit = 1;

	// Signal events to wake up the thread
#if defined(_WIN32)
	SetEvent(kill_event);
#else
	kill_event_fd.notify();
#endif
}
