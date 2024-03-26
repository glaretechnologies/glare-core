/*=====================================================================
AsyncTextureLoader.cpp
----------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "AsyncTextureLoader.h"


#include "opengl/OpenGLEngine.h"
#if defined(EMSCRIPTEN)
#include <emscripten/emscripten.h>
#endif


#if EMSCRIPTEN

static void onTextureDataLoad(unsigned int firstarg, void* userdata_arg, const char* path)
{
	// conPrint("AsyncTextureLoader: onTextureDataLoad: '" + std::string(path) + "', firstarg: " + toString(firstarg));

	AsyncTextureLoader* loader = (AsyncTextureLoader*)userdata_arg;

	try
	{
		OpenGLTextureRef texture = loader->opengl_engine->getTexture(path); // Load the texture from disk into OpenGL

		auto res = loader->tex_info.find(std::string(path));
		assert(res != loader->tex_info.end());
		if(res != loader->tex_info.end())
		{
			AsyncTextureLoader::LoadingTexInfo* loading_tex_info = res->second.ptr();

			loading_tex_info->handler->textureLoaded(texture, std::string(path));

			loader->tex_info.erase(res);
		}
	}
	catch(glare::Exception& e)
	{
		conPrint("AsyncTextureLoader: onTextureDataLoad: error while loading texture '" + std::string(path) + "': " + e.what());
	}
}

static void onTextureDataError(unsigned int, void* userdata_arg, int http_status_code)
{
	conPrint("AsyncTextureLoader::onTextureDataError: " + toString(http_status_code));
}

static void onTextureDataProgress(unsigned int, void* userdata_arg, int percent_complete)
{
	// conPrint("onAnimDataProgress: " + toString(percent_complete));
}

#endif // EMSCRIPTEN


AsyncTextureLoader::AsyncTextureLoader(const std::string& server_hostname_, const std::string& url_path_prefix_, OpenGLEngine* opengl_engine_)
:	server_hostname(server_hostname_), url_path_prefix(url_path_prefix_), opengl_engine(opengl_engine_)
{
}


AsyncTextureLoader::AsyncTextureLoader(const std::string& local_path_prefix_, OpenGLEngine* opengl_engine_)
:	local_path_prefix(local_path_prefix_), opengl_engine(opengl_engine_)
{
}

AsyncTextureLoader::~AsyncTextureLoader()
{
#if EMSCRIPTEN
	// Cancel all pending downloads
	for(auto it = tex_info.begin(); it != tex_info.end(); ++it)
	{
		AsyncTextureLoader::LoadingTexInfo* loading_tex_info = it->second.ptr();

		emscripten_async_wget2_abort(loading_tex_info->emscripten_handle);
	}
	tex_info.clear();
#endif
}


// local_path should be a path relative to the 'data' directory, for example "resources/foam_windowed.ktx2"
// Returns request handle
void AsyncTextureLoader::startLoadingTexture(const std::string& local_path, AsyncTextureLoadedHandler* handler)
{
#if EMSCRIPTEN
	const bool use_TLS = server_hostname != "localhost"; // Don't use TLS on localhost for now, for testing.
	const std::string protocol = use_TLS ? "https" : "http";

	const int handle = emscripten_async_wget2(
		(protocol + "://" + server_hostname + url_path_prefix + local_path).c_str(), local_path.c_str(),  
		/*requesttype =*/"GET", /*POST params=*/"", /*userdata arg=*/this, 
		onTextureDataLoad, onTextureDataError, onTextureDataProgress
	);

	Reference<LoadingTexInfo> loading_tex_info = new LoadingTexInfo();
	loading_tex_info->emscripten_handle = handle;
	loading_tex_info->handler = handler;
	tex_info[local_path] = loading_tex_info;
#else
	// Load it immediately
	const std::string full_local_path = local_path_prefix + local_path;
	try
	{
		OpenGLTextureRef texture = opengl_engine->getTexture(full_local_path); // Load the texture from disk into OpenGL

		handler->textureLoaded(texture, local_path);
	}
	catch(glare::Exception& e)
	{
		conPrint("AsyncTextureLoader: onTextureDataLoad: error while loading texture '" + std::string(full_local_path) + "': " + e.what());
	}
#endif
}


void AsyncTextureLoader::cancelLoadingTexture(const std::string& local_path)
{
#if EMSCRIPTEN
	auto res = tex_info.find(local_path);
	if(res != tex_info.end())
	{
		AsyncTextureLoader::LoadingTexInfo* loading_tex_info = res->second.ptr();

		emscripten_async_wget2_abort(loading_tex_info->emscripten_handle);

		tex_info.erase(res);
	}
#endif
}
