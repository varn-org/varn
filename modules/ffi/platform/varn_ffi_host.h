#pragma once

struct lua_State;

struct varn_ffi_host
{
    bool (*on_lua_thread)(const struct varn_ffi_host* host);
    bool (*fail)(struct lua_State* L);
    void (*post_failure)(const struct varn_ffi_host* host, const char* message);
    int (*capture)(struct lua_State* L);
};

int varn_ffi_open(struct lua_State* L, const struct varn_ffi_host* host);
