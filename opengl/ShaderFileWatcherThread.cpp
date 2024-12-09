/*=====================================================================
ShaderFileWatcherThread.cpp
---------------------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#include "ShaderFileWatcherThread.h"


#include "OpenGLEngine.h"
#include <utils/MemMappedFile.h>
#include <utils/Exception.h>
#include <utils/StringUtils.h>
#include <utils/ConPrint.h>
#include <utils/FileUtils.h>
#include <utils/PlatformUtils.h>
#include <utils/Lock.h>


ShaderFileWatcherThread::ShaderFileWatcherThread(const std::string& watch_dir_, OpenGLEngine* gl_engine_)
:	watch_dir(watch_dir_),
	gl_engine(gl_engine_)
#if defined(_WIN32)
	, kill_event(NULL)
#endif
{}


ShaderFileWatcherThread::~ShaderFileWatcherThread()
{
#if defined(_WIN32)
	if(kill_event)
		CloseHandle(kill_event);
#endif
}


void ShaderFileWatcherThread::kill()
{
#if defined(_WIN32)
	if(kill_event)
		SetEvent(kill_event);
#endif
}


#if defined(_WIN32)
static HANDLE makeWaitHandleForDir(const std::string& dir, bool watch_subtree)
{
	HANDLE wait_handle = FindFirstChangeNotification(StringUtils::UTF8ToPlatformUnicodeEncoding(dir).c_str(), /*watch subtree=*/watch_subtree, FILE_NOTIFY_CHANGE_LAST_WRITE);
	if(wait_handle == INVALID_HANDLE_VALUE)
		throw glare::Exception("FindFirstChangeNotification failed: " + PlatformUtils::getLastErrorString());
	return wait_handle;
}
#endif


void ShaderFileWatcherThread::doRun()
{
	try
	{
#if defined(_WIN32)
		conPrint("ShaderFileWatcherThread: Watching for shader changes in '" + watch_dir + "'...");

		kill_event = CreateEvent( 
			NULL,   // lpEventAttributes: default security attributes
			FALSE,  // bManualReset: auto-reset event object
			FALSE,  // bInitialState: nonsignaled
			NULL    // lpName: unnamed object
		);
		if(!kill_event)
			throw glare::Exception("CreateEvent failed: " + PlatformUtils::getLastErrorString());

		std::vector<HANDLE> wait_handles;
		wait_handles.push_back(makeWaitHandleForDir(watch_dir, /*watch subtree=*/true));
		wait_handles.push_back(kill_event);
		
		while(1)
		{
			const DWORD res = WaitForMultipleObjects(/*count=*/(DWORD)wait_handles.size(), wait_handles.data(), /*wait all=*/FALSE, /*wait time=*/INFINITE);
			if(res >= WAIT_OBJECT_0 && res < WAIT_OBJECT_0 + (DWORD)wait_handles.size() - 1)
			{
				const size_t i = res - WAIT_OBJECT_0;
				conPrint("opengl data dir file changed.");

				gl_engine->shaderFileChanged();

				if(FindNextChangeNotification(wait_handles[i]) == 0)
					throw glare::Exception("FindNextChangeNotification failed: " + PlatformUtils::getLastErrorString());
			}
			else if(res == WAIT_OBJECT_0 + (DWORD)wait_handles.size() - 1) // If the kill event was signalled:
				return;
			else
				throw glare::Exception("Unhandled WaitForSingleObject result");
		}
#endif
	}
	catch(glare::Exception& e)
	{
		conPrint("WebDataFileWatcherThread error: " + e.what());
	}
}
