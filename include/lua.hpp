#pragma once

// c++ convenience header for the lua api since upstream lua ships no lua.hpp
// lua is vendored and built as c++ (see cmake/dependencies.cmake), so its api has c++ linkage and needs no extern "c" wrapper
// building lua as c++ unwinds a raised lua error through the embedding frames as an exception instead of a longjmp that would corrupt them on msvc
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
