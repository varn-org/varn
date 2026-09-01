# process runs commands and reads the environment through a platform driver

if(NOT DEFINED CACHE{VARN_PROCESS_DRIVER})
    # windows runs through win32 createprocess, while posix forks and execs a shell.
    # among apple targets only macos allows that, so ios and the tv/watch/vision platforms fall back to the dummy along with emscripten.
    if(VARN_BUILDING_FOR_EMSCRIPTEN OR CMAKE_SYSTEM_NAME MATCHES "^(iOS|tvOS|watchOS|visionOS)$")
        set(VARN_PROCESS_DRIVER "DUMMY" CACHE STRING "process backend: POSIX WINDOWS DUMMY")
    elseif(WIN32)
        set(VARN_PROCESS_DRIVER "WINDOWS" CACHE STRING "process backend: POSIX WINDOWS DUMMY")
    else()
        set(VARN_PROCESS_DRIVER "POSIX" CACHE STRING "process backend: POSIX WINDOWS DUMMY")
    endif()
endif()
set_property(CACHE VARN_PROCESS_DRIVER PROPERTY STRINGS POSIX WINDOWS DUMMY)
varn_validate_driver(VARN_PROCESS_DRIVER POSIX WINDOWS DUMMY)

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/ProcessModule.cpp")

if(VARN_PROCESS_DRIVER STREQUAL "POSIX")
    list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/posix/PosixProcessRunner.cpp")
elseif(VARN_PROCESS_DRIVER STREQUAL "WINDOWS")
    list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/windows/WindowsProcessRunner.cpp")
else()
    list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/dummy/DummyProcessRunner.cpp")
endif()

varn_register_driver(PROCESS "${VARN_PROCESS_DRIVER}")
