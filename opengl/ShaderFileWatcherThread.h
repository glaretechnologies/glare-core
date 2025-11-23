/*=====================================================================
ShaderFileWatcherThread.h
-------------------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include <utils/MessageableThread.h>
#include <utils/Reference.h>
#include <string>
#include <vector>


class OpenGLEngine;


/*=====================================================================
ShaderFileWatcherThread
-----------------------
Watches for changes to shaders and other gl data files,
calls gl_engine->shaderFileChanged() when a file changes.
=====================================================================*/
class ShaderFileWatcherThread : public MessageableThread
{
public:
	ShaderFileWatcherThread(const std::vector<std::string>& watch_dirs, OpenGLEngine* gl_engine);
	virtual ~ShaderFileWatcherThread();

	virtual void doRun();

	virtual void kill();

private:
	std::vector<std::string> watch_dirs;
	OpenGLEngine* gl_engine;

#if defined(_WIN32)
	HANDLE kill_event;
#endif
};
