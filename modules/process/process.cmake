# The `process` module runs commands and reads the environment through a platform driver.

# Windows runs through the Win32 `CreateProcess`, while POSIX forks and execs a shell.
# Among Apple targets only macOS allows that, so iOS and the tv, watch and vision platforms read the environment through POSIX and start no process, and the browser, which has neither, takes the dummy.
if(VARN_BUILDING_FOR_EMSCRIPTEN)
    set(_varn_process_default DUMMY)
elseif(CMAKE_SYSTEM_NAME MATCHES "^(iOS|tvOS|watchOS|visionOS)$")
    set(_varn_process_default APPLE_MOBILE)
elseif(WIN32)
    set(_varn_process_default WINDOWS)
else()
    set(_varn_process_default POSIX)
endif()
varn_driver_option(VARN_PROCESS_DRIVER ${_varn_process_default} "Process backend" POSIX APPLE_MOBILE WINDOWS DUMMY EXTERNAL)

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
list(APPEND VARN_CORE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/ProcessModule.cpp")

if(VARN_PROCESS_DRIVER STREQUAL "POSIX")
    list(APPEND VARN_CORE_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/posix/PosixProcessRunner.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/posix/PosixProcessEnvironment.cpp"
    )
elseif(VARN_PROCESS_DRIVER STREQUAL "APPLE_MOBILE")
    list(APPEND VARN_CORE_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/apple-mobile/AppleMobileProcessRunner.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/posix/PosixProcessEnvironment.cpp"
    )
elseif(VARN_PROCESS_DRIVER STREQUAL "WINDOWS")
    list(APPEND VARN_CORE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/windows/WindowsProcessRunner.cpp")
elseif(VARN_PROCESS_DRIVER STREQUAL "EXTERNAL")
    varn_add_external_driver(PROCESS VARN_CORE_SOURCES)
else()
    list(APPEND VARN_CORE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/dummy/DummyProcessRunner.cpp")
endif()

varn_register_driver(PROCESS "${VARN_PROCESS_DRIVER}")
