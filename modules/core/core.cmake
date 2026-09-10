# The core module provides the runtime, Lua engine, native module registry, and shared log pipeline.

# The event loop waits on libuv (epoll/kqueue/IOCP) to host socket readiness inline, while wasm pumps the loop manually.
if(NOT VARN_BUILDING_FOR_EMSCRIPTEN)
    set(VARN_NEEDS_POCO ON)
    set(VARN_NEEDS_LIBUV ON)
endif()

# The tvOS, watchOS and visionOS targets prohibit `fork`, and wasm has no process model, so the multi-process worker path is compiled out on all of them.
if(CMAKE_SYSTEM_NAME MATCHES "^(tvOS|watchOS|visionOS)$" OR VARN_BUILDING_FOR_EMSCRIPTEN)
    list(APPEND VARN_COMPILE_DEFS "VARN_NO_FORK=1")
endif()

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")

list(APPEND VARN_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/runtime/App.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/runtime/Runtime.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/runtime/EventLoop.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/runtime/TaskPool.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/lua/LuaEngine.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/lua/LuaHelpers.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/lua/NativeModules.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/log/Log.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/tls/CaBundle.cpp"
)
