#include "IncludeWindows.h"


// Including gl3w.h results in compile errors due to redefined symbols (or something) on OS X. 
// However on OS X the OpenGL headers are up-to-date for OpenGL 3 core profile, so we can just use them.
#if defined(OSX)
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#else
#include <GL/gl3w.h>
#endif
