#pragma once

// Overrides how the vendored Lua unwinds, and is forced into `ldo.c` on every compiler but MSVC (see `cmake/dependencies.cmake`).
// A yield jumps straight back to the `lua_resume` that waits for it with `_longjmp`, which costs a fraction of the C++ unwinder.
// An error stays a C++ exception, so it unwinds the embedding frames and runs their destructors.
// A yield runs no destructor, so no C++ frame may keep a destructible local alive across `lua_callk`, `lua_pcallk` or `lua_yieldk`.
#include <setjmp.h>

#define LUAI_THROW(L, c) ((c)->status == LUA_YIELD ? _longjmp((c)->b, 1) : throw(c))

#define LUAI_TRY(L, c, f, ud) \
    if (_setjmp((c)->b) == 0) \
    { \
        try \
        { \
            (f)(L, ud); \
        } \
        catch (lua_longjmp * thrown) \
        { \
            if (thrown != (c)) \
            { \
                throw; \
            } \
        } \
        catch (...) \
        { \
            (c)->status = -1; \
        } \
    }
