#pragma once

// A C++ convenience header for the Lua API, since upstream Lua ships no `lua.hpp`.
// Lua is vendored and built as C++ (see `cmake/dependencies.cmake`), so its API has C++ linkage and needs no `extern "C"` wrapper.
// Building Lua as C++ unwinds a raised Lua error through the embedding frames as an exception instead of a `longjmp` that would corrupt them on MSVC.
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
