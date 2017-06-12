// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#include <SDL.h>

// TODO: reference additional headers your program requires here
#pragma warning(disable:4251)
#include <glbinding/gl/gl.h>
using namespace gl;
#include <glbinding/Binding.h>
#include <glbinding/callbacks.h>
#include <glbinding/CallbackMask.h>

#include <sstream>

using std::vector;
using std::string;
using std::stringstream;

#include "lodepng.h"
#include <functional>
