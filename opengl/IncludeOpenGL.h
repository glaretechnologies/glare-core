#include "IncludeWindows.h"


// Including gl3w.h results in compile errors due to redefined symbols (or something) on OS X. 
// However on OS X the OpenGL headers are up-to-date for OpenGL 3 core profile, so we can just use them.
#if defined(OSX)
//#include <OpenGL/gl.h>
//#include <OpenGL/glext.h>
#include <GL/gl3w.h>
#else

#undef GL_VERSION_1_1 // This seems to be needed to get the compile to work for some reason.

#include <GL/gl3w.h>
#endif
