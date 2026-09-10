include_guard(GLOBAL)

# builds the vendored libffi plus the lua-ffi parser as the varn_ffi_vendor static library
# included from the top-level build only when VARN_FFI_DRIVER=LIBFFI after lua is available

if(EMSCRIPTEN OR CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    message(FATAL_ERROR "VARN_FFI_DRIVER=LIBFFI is not supported on Emscripten. Use VARN_FFI_DRIVER=DUMMY.")
endif()

set(VARN_LIBFFI_VERSION "3.5.2")
set(VARN_LIBFFI_VERSION_NUMBER 30502)

CPMAddPackage(
    NAME varn_libffi_upstream
    VERSION ${VARN_LIBFFI_VERSION}
    GITHUB_REPOSITORY libffi/libffi
    GIT_TAG v${VARN_LIBFFI_VERSION}
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

    # compile the ffi vendor as c++ so its references to the lua api mangle the same way varn_vendor_lua exports them, since lua itself is built as c++ to fix the msvc longjmp/seh corruption.
    # this also bypasses the C23 = {} initializer that the msvc c compiler rejects, because c++20 accepts the empty-aggregate initializer.
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
