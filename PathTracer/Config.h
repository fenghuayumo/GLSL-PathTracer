#pragma once


#ifdef __EMSCRIPTEN__
#include <GLES3/gl32.h>
#else
#include <GL/gl3w.h>
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#undef min
#undef max
#endif