// Including gl3w.h results in compile errors due to redefined symbols (or something) on OS X. 
// However on OS X the OpenGL headers are up-to-date for OpenGL 3 core profile, so we can just use them.
#if defined(OSX)
// QtGui/qopengl.h also defines this before including gl3.h and gl3ext.h.
#define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>

#elif defined(EMSCRIPTEN)

#include <GLES3/gl3.h>

#else

// Stop windows.h from defining the min() and max() macros
#ifndef NOMINMAX
#define NOMINMAX
#endif

#undef GL_VERSION_1_1 // This seems to be needed to get the compile to work for some reason.

#include <GL/gl3w.h>

#endif
