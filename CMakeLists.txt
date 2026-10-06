cmake_minimum_required(VERSION 3.26)

# Dependencies that declare an older CMake minimum version are configured with the policies of the oldest version CMake keeps without a deprecation warning.
set(CMAKE_POLICY_VERSION_MINIMUM 3.10 CACHE STRING "")

project(varn VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

cmake_policy(SET CMP0054 NEW)
cmake_policy(SET CMP0076 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)

set(VARN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
include("${VARN_ROOT}/cmake/helpers.cmake")

# The generated version header carries the project version.
set(VARN_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/varn_generated")
configure_file(
    "${VARN_ROOT}/cmake/VarnVersion.h.in"
    "${VARN_GENERATED_INCLUDE_DIR}/VarnVersion.h"
    @ONLY
)

# Toolchains set `EMSCRIPTEN`, and `CMAKE_SYSTEM_NAME` also names an Emscripten build, so the build for the browser still stubs its drivers.
if(EMSCRIPTEN OR CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(VARN_BUILDING_FOR_EMSCRIPTEN ON)
else()
    set(VARN_BUILDING_FOR_EMSCRIPTEN OFF)
endif()

# Mac Catalyst builds for the Mac with the SDK of iOS, which a target triple ending in `-macabi` names, so Lua and OpenSSL are configured as for iOS.
if(APPLE AND CMAKE_C_COMPILER_TARGET MATCHES "-macabi$")
    set(VARN_MAC_CATALYST ON)
else()
    set(VARN_MAC_CATALYST OFF)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

option(VARN_ENABLE_TLS "Enable TLS (HTTPS) support" ON)
option(VARN_ENABLE_STATIC_FILES "Enable public static file serving" ON)
option(VARN_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(VARN_ENABLE_ZIP "Build the zip module (libzip and zlib)" ON)
option(VARN_NO_SENDFILE "Disable the zero-copy sendfile fast path and serve files portably" OFF)
option(VARN_BUILD_TESTS "Build the native C++ test target (googletest)" OFF)
set(VARN_SANITIZE "" CACHE STRING "Comma-separated sanitizers to build with, for example \"address,undefined\" (Clang and GCC only)")

# The targets `cli` and `wasm` produce an executable on the static core, while `apple`, `android` and `lib` produce the shared library.
# The target is resolved before any driver is chosen, whether a host names it on the command line or in its own list file.
if(NOT DEFINED CACHE{VARN_TARGET})
    if(VARN_BUILDING_FOR_EMSCRIPTEN)
        set(VARN_TARGET "wasm" CACHE STRING "Build target: cli, wasm, apple, android or lib")
    else()
        set(VARN_TARGET "cli" CACHE STRING "Build target: cli, wasm, apple, android or lib")
    endif()
endif()
set_property(CACHE VARN_TARGET PROPERTY STRINGS cli wasm apple android lib)
set(_varn_target_ok cli wasm apple android lib)
if(NOT VARN_TARGET IN_LIST _varn_target_ok)
    message(FATAL_ERROR "The option \"VARN_TARGET\" must be one of \"${_varn_target_ok}\" (got \"${VARN_TARGET}\").")
endif()
message(STATUS "Varn build target: ${VARN_TARGET}")

# An embedded mobile build talks to the network through the platform, so the app steers it from its own manifest.
# App Transport Security on Apple and the network security config on Android are honoured by the stack that reads them, and the trust store, the system proxy and HTTP/2 come from the OS rather than from a bundle the engine ships.
# The driver follows the target from the first configure, so no host names the driver itself.
# A host that links the core on an Apple system other than macOS, Mac Catalyst included, gets the driver of the platform by default and may still name another one.
if(VARN_TARGET STREQUAL "apple")
    set(VARN_HTTP_CLIENT_DRIVER "APPLE" CACHE STRING "" FORCE)
elseif(VARN_TARGET STREQUAL "android")
    set(VARN_HTTP_CLIENT_DRIVER "ANDROID" CACHE STRING "" FORCE)
elseif((CMAKE_SYSTEM_NAME MATCHES "^(iOS|tvOS|watchOS|visionOS)$" OR VARN_MAC_CATALYST) AND NOT DEFINED CACHE{VARN_HTTP_CLIENT_DRIVER})
    set(VARN_HTTP_CLIENT_DRIVER "APPLE" CACHE STRING "HTTP client transport: POCO APPLE ANDROID EMSCRIPTEN_FETCH DUMMY")
endif()

# The browser cannot host raw TCP or an in-page server, so the wasm-capable driver set is forced.
if(VARN_BUILDING_FOR_EMSCRIPTEN)
    set(VARN_ENABLE_TLS OFF CACHE BOOL "TLS (disabled for Emscripten)" FORCE)
    set(VARN_HTTP_SERVER_DRIVER "DUMMY" CACHE STRING "" FORCE)
    set(VARN_HTTP_CLIENT_DRIVER "EMSCRIPTEN_FETCH" CACHE STRING "" FORCE)
    set(VARN_SOCKET_DRIVER "DUMMY" CACHE STRING "" FORCE)
    set(VARN_JSON_DRIVER "NLOHMANN" CACHE STRING "" FORCE)
    set(VARN_XML_DRIVER "PUGIXML" CACHE STRING "" FORCE)
    set(VARN_CRYPTO_DRIVER "PORTABLE" CACHE STRING "" FORCE)
    set(VARN_LOG_DRIVER "SPDLOG" CACHE STRING "" FORCE)
    set(VARN_FS_DRIVER "STD" CACHE STRING "" FORCE)
    set(VARN_FFI_DRIVER "DUMMY" CACHE STRING "" FORCE)

    # Lua compiled as C++ needs exception support so a raised Lua error can be caught by `pcall`, and a yield jumps back to its resume with `_longjmp`.
    # Both are the native exceptions and the native `longjmp` of WebAssembly, which every current browser runs, rather than an emulation that calls out to JavaScript.
    add_compile_options(-fwasm-exceptions "SHELL:-s SUPPORT_LONGJMP=wasm")
    add_link_options(-fwasm-exceptions "SHELL:-s SUPPORT_LONGJMP=wasm")
endif()

# The Apple platform deployment targets match the dependency toolchains when cross-compiling.
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "iOS deployment target")
elseif(CMAKE_SYSTEM_NAME STREQUAL "watchOS")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "8.0" CACHE STRING "watchOS deployment target")
elseif(CMAKE_SYSTEM_NAME STREQUAL "tvOS")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "tvOS deployment target")
elseif(CMAKE_SYSTEM_NAME STREQUAL "visionOS")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "1.0" CACHE STRING "visionOS deployment target")
endif()

# Every artifact lands under the output directories of the build tree.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# These accumulators are populated by the list file of each module.
set(VARN_SOURCES "")
set(VARN_INCLUDE_DIRS "")
set(VARN_COMPILE_DEFS "")

# TLS always needs OpenSSL even when crypto reaches it through Poco.
if(VARN_ENABLE_TLS)
    set(VARN_NEEDS_OPENSSL ON)
endif()

# The module `core` is included first so the public API and feature modules can declare their sources, drivers and dependencies on top of it.
foreach(_varn_module
    core api platform log async fs crypto json xml zip socket http ffi process datetime)
    include("${VARN_ROOT}/modules/${_varn_module}/${_varn_module}.cmake")
endforeach()

# The dependencies are driven by the `VARN_NEEDS_*` flags the modules set above.
include("${VARN_ROOT}/cmake/cpm/cpm-downloader.cmake")
include("${VARN_ROOT}/cmake/dependencies.cmake")

if(NOT VARN_BUILDING_FOR_EMSCRIPTEN)
    find_package(Threads REQUIRED)
endif()

if(VARN_NEEDS_LIBFFI)
    include("${VARN_ROOT}/modules/ffi/libffi.cmake")
endif()

message(STATUS "Varn HTTP server driver: ${VARN_HTTP_SERVER_DRIVER}")
message(STATUS "Varn HTTP client driver: ${VARN_HTTP_CLIENT_DRIVER}")
message(STATUS "Varn socket driver: ${VARN_SOCKET_DRIVER}")
message(STATUS "Varn JSON driver: ${VARN_JSON_DRIVER}")
message(STATUS "Varn XML driver: ${VARN_XML_DRIVER}")
message(STATUS "Varn crypto driver: ${VARN_CRYPTO_DRIVER}")
message(STATUS "Varn log driver: ${VARN_LOG_DRIVER}")
message(STATUS "Varn FS driver: ${VARN_FS_DRIVER}")
message(STATUS "Varn FFI driver: ${VARN_FFI_DRIVER}")
message(STATUS "Varn TLS enabled: ${VARN_ENABLE_TLS}")
message(STATUS "Varn zip module: ${VARN_ENABLE_ZIP}")

if(VARN_TARGET STREQUAL "apple")
    # A self-contained dynamic framework compiles every module into one shared image.
    # The module map is what lets Swift write `import varn` instead of reaching the header through a bridging header, and Clang only reads it under that exact file name, so it is copied to it rather than bundled as it is named here.
    set(VARN_MODULEMAP "${CMAKE_BINARY_DIR}/module.modulemap")
    configure_file("${VARN_ROOT}/cmake/varn.modulemap" "${VARN_MODULEMAP}" COPYONLY)
    set_source_files_properties("${VARN_MODULEMAP}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Modules"
        HEADER_FILE_ONLY TRUE
    )

    # The framework is named in lower case like the library and the package everywhere else, so an import reads `varn/varn.h`.
    add_library(varn SHARED ${VARN_SOURCES} "${VARN_ROOT}/modules/api/include/varn/varn.h" "${VARN_MODULEMAP}")
    varn_apply_usage(varn PRIVATE)
    varn_apply_warnings(varn)
    set_target_properties(varn PROPERTIES
        OUTPUT_NAME "varn"
        FRAMEWORK TRUE
        FRAMEWORK_VERSION A
        MACOSX_FRAMEWORK_IDENTIFIER "com.varn.lib"
        MACOSX_FRAMEWORK_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_FRAMEWORK_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        PUBLIC_HEADER "${VARN_ROOT}/modules/api/include/varn/varn.h"
    )

    # Several libraries inside the core each name the zlib they share, and the linker of Apple reports every repeated name although it links zlib once.
    target_link_options(varn PRIVATE "LINKER:-no_warn_duplicate_libraries")
elseif(VARN_TARGET STREQUAL "android")
    # The Android Gradle module links this static core with the JNI bindings into `libvarn.so`.
    add_library(varn_core STATIC ${VARN_SOURCES})
    set_target_properties(varn_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
    varn_apply_usage(varn_core PUBLIC)
    varn_apply_warnings(varn_core)
elseif(VARN_TARGET STREQUAL "lib")
    # An embeddable shared library exports only the C API with every dependency linked in privately, installed as a `find_package(varn)` package.
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    add_library(varn SHARED ${VARN_SOURCES} "${VARN_ROOT}/modules/api/include/varn/varn.h")
    add_library(varn::varn ALIAS varn)
    varn_apply_usage(varn PRIVATE)
    varn_apply_warnings(varn)
    set_target_properties(varn PROPERTIES
        OUTPUT_NAME varn
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        PUBLIC_HEADER "${VARN_ROOT}/modules/api/include/varn/varn.h"
    )
    target_include_directories(varn PUBLIC
        "$<BUILD_INTERFACE:${VARN_ROOT}/modules/api/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )

    # The definition `VARN_SHARED` marks the C API `dllexport` on Windows, and the linker script hides every private dependency symbol on ELF and Mach-O where the visibility preset cannot reach.
    target_compile_definitions(varn PRIVATE VARN_SHARED)
    if(APPLE)
        # Several libraries inside the core each name the zlib they share, and the linker of Apple reports every repeated name although it links zlib once.
        target_link_options(varn PRIVATE "LINKER:-no_warn_duplicate_libraries")
        target_link_options(varn PRIVATE "LINKER:-exported_symbols_list,${VARN_ROOT}/cmake/varn-exports.symbols")
        set_target_properties(varn PROPERTIES LINK_DEPENDS "${VARN_ROOT}/cmake/varn-exports.symbols")
    elseif(UNIX)
        target_link_options(varn PRIVATE "LINKER:--version-script=${VARN_ROOT}/cmake/varn-exports.map")
        set_target_properties(varn PROPERTIES LINK_DEPENDS "${VARN_ROOT}/cmake/varn-exports.map")
    endif()

    # Tagging every install rule with the `varn` component keeps a `--component varn` install to the library, header and CMake package.
    install(TARGETS varn EXPORT varnTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT varn
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT varn
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT varn
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/varn COMPONENT varn
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(EXPORT varnTargets
        NAMESPACE varn::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/varn
        FILE varnTargets.cmake
        COMPONENT varn
    )
    configure_package_config_file(
        "${VARN_ROOT}/cmake/varnConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/varnConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/varn
    )
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/varnConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/varnConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/varnConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/varn
        COMPONENT varn
    )
else()
    # The static core is built once, then the host executable links against it.
    add_library(varn_core STATIC ${VARN_SOURCES})
    set_target_properties(varn_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
    varn_apply_usage(varn_core PUBLIC)
    varn_apply_warnings(varn_core)
    varn_apply_sanitize(varn_core PUBLIC)

    if(VARN_TARGET STREQUAL "wasm")
        add_executable(varn_wasm "${VARN_ROOT}/src/wasm/VarnWasm.cpp")
        target_link_libraries(varn_wasm PRIVATE varn_core)
        set_property(TARGET varn_wasm APPEND PROPERTY LINK_DEPENDS "${VARN_ROOT}/src/wasm/varn_wasm_post.js")
        target_link_options(varn_wasm PRIVATE
            "SHELL:-s WASM=1"
            "SHELL:-s EXPORT_ES6=1"
            "SHELL:-s MODULARIZE=1"
            "SHELL:-s EXPORT_NAME=createVarnWasm"
            "SHELL:-s ALLOW_MEMORY_GROWTH=1"
            "SHELL:-s STACK_SIZE=8388608"
            "SHELL:-s INITIAL_MEMORY=67108864"
            "SHELL:--bind"
            "SHELL:--post-js ${VARN_ROOT}/src/wasm/varn_wasm_post.js"
            "SHELL:-s ENVIRONMENT=web,worker"
            "SHELL:-s FILESYSTEM=1"
        )
    elseif(NOT CMAKE_SYSTEM_NAME MATCHES "^(iOS|tvOS|watchOS|visionOS)$")
        # A mobile Apple platform cannot run a plain executable, so a `cli` build there stops at the static core.
        add_executable(varn "${VARN_ROOT}/src/main.cpp")
        target_link_libraries(varn PRIVATE varn_core)

        if(MSVC)
            # Windows gives the main thread a 1 MB stack against the 8 MB Unix gives, so it is matched for deeply recursive work like JSON parsing.
            target_link_options(varn PRIVATE /STACK:8388608)

            # The crash handler resolves a backtrace through `dbghelp`, and program-database symbols name the frames with file and line even in release.
            target_link_libraries(varn PRIVATE dbghelp)
            target_compile_options(varn_core PRIVATE /Zi)
            target_compile_options(varn PRIVATE /Zi)
            target_link_options(varn PRIVATE /DEBUG)
        endif()

        install(TARGETS varn RUNTIME DESTINATION bin COMPONENT cli)

        # A desktop `cli` build also produces the platform installer, which is what puts `varn` on the `PATH`.
        if(NOT VARN_BUILDING_FOR_EMSCRIPTEN AND NOT ANDROID)
            include("${VARN_ROOT}/cmake/packaging.cmake")
        endif()
    endif()
endif()

# The native C++ unit tests are kept off the shippable build and out of the wasm and mobile targets.
if(VARN_BUILD_TESTS AND TARGET varn_core AND NOT VARN_BUILDING_FOR_EMSCRIPTEN)
    enable_testing()
    add_subdirectory("${VARN_ROOT}/tests/cpp")
endif()
