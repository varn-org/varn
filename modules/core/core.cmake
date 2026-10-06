# The core module provides the scheduler, the shared log pipeline and TLS, which the runtime without Lua holds, and the Lua runtime, Lua engine and native module registry on top of them.

# The event loop waits on libuv (epoll/kqueue/IOCP) to host socket readiness inline with the driver `LIBUV`, while with the driver `PUMP` the host pumps it from its own run loop and it watches no socket, as in the browser and on a port to a system without libuv.
varn_driver_option(VARN_EVENT_LOOP_DRIVER LIBUV "Event loop" LIBUV PUMP)
if(VARN_EVENT_LOOP_DRIVER STREQUAL "LIBUV")
    set(VARN_NEEDS_POCO ON)
    set(VARN_NEEDS_LIBUV ON)
endif()
varn_register_driver(EVENT_LOOP "${VARN_EVENT_LOOP_DRIVER}")

# The worker pools run their jobs on threads of their own wherever the system has threads, whatever the event loop, and a build without threads, such as the browser, runs them inside the pump of a `PUMP` loop instead.
option(VARN_THREADS "Run the jobs of the worker pools on threads of their own" ON)
if(VARN_THREADS)
    list(APPEND VARN_COMPILE_DEFS "VARN_THREADS=1")
elseif(NOT VARN_EVENT_LOOP_DRIVER STREQUAL "PUMP")
    message(FATAL_ERROR "The event loop \"${VARN_EVENT_LOOP_DRIVER}\" runs on threads, which \"VARN_THREADS\" turns off. Take the event loop \"PUMP\", which runs the jobs of the pools inside its pump, or turn \"VARN_THREADS\" on.")
endif()

# The tvOS, watchOS and visionOS targets prohibit `fork`, a pumped loop runs no process of its own and a port knows no `fork`, so the multi-process worker path is compiled out on all of them.
if(CMAKE_SYSTEM_NAME MATCHES "^(tvOS|watchOS|visionOS)$" OR VARN_EVENT_LOOP_DRIVER STREQUAL "PUMP" OR VARN_TARGET STREQUAL "port")
    list(APPEND VARN_COMPILE_DEFS "VARN_NO_FORK=1")
endif()

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")

list(APPEND VARN_RUNTIME_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/runtime/Scheduler.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/runtime/EventLoop.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/runtime/TaskPool.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/log/Log.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/tls/CaBundle.cpp"
)

if(VARN_ENABLE_TLS)
    list(APPEND VARN_RUNTIME_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/tls/SystemTrust.cpp")
endif()

list(APPEND VARN_CORE_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/runtime/App.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/runtime/Runtime.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/lua/LuaEngine.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/lua/LuaHelpers.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/lua/NativeModules.cpp"
)
