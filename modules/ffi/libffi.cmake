include_guard(GLOBAL)

# Builds the vendored libffi plus the lua-ffi parser as the `varn_ffi_vendor` static library.
# The top-level build includes this file only when `VARN_FFI_DRIVER=LIBFFI`, after Lua is available.

if(EMSCRIPTEN OR CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    message(FATAL_ERROR "The driver \"VARN_FFI_DRIVER=LIBFFI\" is not supported on Emscripten. Use \"VARN_FFI_DRIVER=DUMMY\".")
endif()

set(VARN_LIBFFI_VERSION "3.5.2")
set(VARN_LIBFFI_VERSION_NUMBER 30502)

CPMAddPackage(
    NAME varn_libffi_upstream
    VERSION ${VARN_LIBFFI_VERSION}
    URL https://github.com/libffi/libffi/archive/refs/tags/v${VARN_LIBFFI_VERSION}.tar.gz
    URL_HASH SHA256=dd19253d3007f366319a51d248a40c9e5fcace4498cbea990b566291844e4e30
    DOWNLOAD_ONLY YES
)

set(VARN_LIBFFI_ROOT "${varn_libffi_upstream_SOURCE_DIR}")
include("${CMAKE_CURRENT_LIST_DIR}/libffi/varn-vendor-libffi.cmake")

if(NOT TARGET varn_ffi_vendor)
    set(_varn_ffi_gen_dir "${CMAKE_BINARY_DIR}/generated/varn_ffi")
    file(MAKE_DIRECTORY "${_varn_ffi_gen_dir}")
    set(LUA_FFI_VERSION_MAJOR 1)
    set(LUA_FFI_VERSION_MINOR 1)
    set(LUA_FFI_VERSION_PATCH 0)
    configure_file(
        "${CMAKE_CURRENT_LIST_DIR}/platform/lua_ffi_config.h.in"
        "${_varn_ffi_gen_dir}/config.h"
        @ONLY
    )

    set(_varn_ffi_vendor_sources
        "${CMAKE_CURRENT_LIST_DIR}/vendor/ffi.c"
        "${CMAKE_CURRENT_LIST_DIR}/vendor/lex.c"
        "${CMAKE_CURRENT_LIST_DIR}/platform/varn_ffi_dl.c"
    )

    # Compile the FFI vendor as C++ so its references to the Lua API mangle the same way `varn_vendor_lua` exports them, since Lua itself is built as C++ to fix the MSVC `longjmp`/SEH corruption.
    # This also bypasses the C23 `= {}` initializer that the MSVC C compiler rejects, because C++20 accepts the empty-aggregate initializer.
    set_source_files_properties(${_varn_ffi_vendor_sources} PROPERTIES LANGUAGE CXX)

    add_library(varn_ffi_vendor STATIC ${_varn_ffi_vendor_sources})
    set_target_properties(varn_ffi_vendor PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )
    if(MSVC)
        target_compile_options(varn_ffi_vendor PRIVATE /wd4244 /wd4267)
        target_include_directories(varn_ffi_vendor BEFORE PRIVATE
            "${_varn_ffi_gen_dir}"
            "${CMAKE_CURRENT_LIST_DIR}/platform/msvc_compat"
        )
    else()
        target_compile_options(varn_ffi_vendor PRIVATE -fno-strict-aliasing -w)
        target_include_directories(varn_ffi_vendor BEFORE PRIVATE "${_varn_ffi_gen_dir}")
        target_compile_definitions(varn_ffi_vendor PRIVATE _GNU_SOURCE)
    endif()
    target_include_directories(varn_ffi_vendor PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/vendor"
        "${CMAKE_CURRENT_LIST_DIR}/platform"
    )
    target_include_directories(varn_ffi_vendor PRIVATE $<TARGET_PROPERTY:varn_vendor_lua,INTERFACE_INCLUDE_DIRECTORIES>)

    target_link_libraries(varn_ffi_vendor PUBLIC varn_vendor_libffi)
endif()
