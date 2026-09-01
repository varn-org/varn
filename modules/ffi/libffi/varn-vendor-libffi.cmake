# builds upstream libffi as a static CMake target (no autoconf, no externalproject)

if(NOT DEFINED VARN_LIBFFI_ROOT OR VARN_LIBFFI_ROOT STREQUAL "")
    message(FATAL_ERROR "varn libffi: VARN_LIBFFI_ROOT is not set")
endif()
if(NOT DEFINED VARN_LIBFFI_VERSION OR VARN_LIBFFI_VERSION STREQUAL "")
    message(FATAL_ERROR "varn libffi: VARN_LIBFFI_VERSION is not set")
endif()
if(NOT DEFINED VARN_LIBFFI_VERSION_NUMBER)
    message(FATAL_ERROR "varn libffi: VARN_LIBFFI_VERSION_NUMBER is not set")
endif()
if(NOT EXISTS "${VARN_LIBFFI_ROOT}/include/ffi.h.in")
    message(FATAL_ERROR "varn libffi: incomplete tree at '${VARN_LIBFFI_ROOT}' (missing include/ffi.h.in)")
endif()

if(EMSCRIPTEN OR CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    message(FATAL_ERROR
        "varn libffi: vendored libffi is not supported on Emscripten. "
        "Configure with VARN_FFI_DRIVER=DUMMY for WebAssembly builds.")
endif()

macro(varn_libffi_add_assembly _varn_asm_file)
    get_filename_component(_varn_asm_abs "${_varn_asm_file}" ABSOLUTE)
    if(MSVC)
        if("${_varn_ffi_target}" STREQUAL "ARM_WIN64")
            set(_varn_libffi_masm_tool "armasm64")
        elseif("${_varn_ffi_target}" STREQUAL "ARM_WIN32")
            set(_varn_libffi_masm_tool "armasm")
        elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
            set(_varn_libffi_masm_tool "ml" "/safeseh" "/c" "/Zi")
        else()
            set(_varn_libffi_masm_tool "ml64" "/c" "/Zi")
        endif()

        get_filename_component(_varn_asm_base "${_varn_asm_abs}" NAME_WE)

        # the preprocessor include layout has cwd holding fficonfig.h, ./include holding ffi.h, and upstream include resolving the .S
        execute_process(
            COMMAND "${CMAKE_C_COMPILER}" /nologo /EP
                /I.
                /Iinclude
                "/I${VARN_LIBFFI_ROOT}/include"
                "${_varn_asm_abs}"
            WORKING_DIRECTORY "${VARN_FFI_GEN_DIR}"
            OUTPUT_FILE "${_varn_asm_base}.asm"
            RESULT_VARIABLE _varn_asm_pp_rc
        )
        if(NOT _varn_asm_pp_rc EQUAL 0)
            message(FATAL_ERROR "varn libffi: masm preprocess failed for '${_varn_asm_file}' (${_varn_asm_pp_rc})")
        endif()

        execute_process(
            COMMAND ${_varn_libffi_masm_tool} "${_varn_asm_base}.asm"
            WORKING_DIRECTORY "${VARN_FFI_GEN_DIR}"
            RESULT_VARIABLE _varn_asm_ml_rc
        )
        if(NOT _varn_asm_ml_rc EQUAL 0)
            message(FATAL_ERROR "varn libffi: masm failed for '${_varn_asm_file}' (${_varn_asm_ml_rc})")
        endif()

        list(APPEND _varn_libffi_sources "${VARN_FFI_GEN_DIR}/${_varn_asm_base}.obj")
    else()
        list(APPEND _varn_libffi_sources "${_varn_asm_file}")
    endif()
endmacro()

if(NOT CMAKE_SYSTEM_PROCESSOR)
    set(CMAKE_SYSTEM_PROCESSOR "${CMAKE_HOST_SYSTEM_PROCESSOR}")
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _varn_ffi_cpu_lc)

if(ANDROID)
    if(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
        set(_varn_ffi_cpu_lc "aarch64")
    elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "armeabi-v7a")
        set(_varn_ffi_cpu_lc "arm")
    elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "x86_64")
        set(_varn_ffi_cpu_lc "x86_64")
    elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "x86")
        set(_varn_ffi_cpu_lc "i686")
    else()
        message(FATAL_ERROR "varn libffi: unsupported CMAKE_ANDROID_ARCH_ABI='${CMAKE_ANDROID_ARCH_ABI}'")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # host CMAKE_SYSTEM_PROCESSOR can be arm64 while CMAKE_OSX_ARCHITECTURES is x86_64 (cross slice / rosetta build)
    if(CMAKE_OSX_ARCHITECTURES)
        list(LENGTH CMAKE_OSX_ARCHITECTURES _varn_ffi_darwin_arch_len)
        if(_varn_ffi_darwin_arch_len GREATER 1)
            message(WARNING "varn libffi: multiple CMAKE_OSX_ARCHITECTURES; libffi uses the first entry only")
        endif()
        list(GET CMAKE_OSX_ARCHITECTURES 0 _varn_ffi_darwin_arch0)
        string(TOLOWER "${_varn_ffi_darwin_arch0}" _varn_ffi_cpu_lc)
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS"
        OR CMAKE_SYSTEM_NAME STREQUAL "tvOS"
        OR CMAKE_SYSTEM_NAME STREQUAL "watchOS"
        OR CMAKE_SYSTEM_NAME STREQUAL "visionOS")
    if(CMAKE_OSX_ARCHITECTURES)
        list(LENGTH CMAKE_OSX_ARCHITECTURES _varn_ffi_osx_arch_len)
        if(_varn_ffi_osx_arch_len GREATER 0)
            list(GET CMAKE_OSX_ARCHITECTURES 0 _varn_ffi_osx_arch0)
            string(TOLOWER "${_varn_ffi_osx_arch0}" _varn_ffi_cpu_lc)
        endif()
    endif()
endif()

set(_varn_ffi_known_cpu
    x86 x86_64 amd64 arm arm64 arm64_32 i386 i686 armv7l armv7-a armv7s armv7k aarch64)
if(NOT _varn_ffi_cpu_lc IN_LIST _varn_ffi_known_cpu)
    message(FATAL_ERROR "varn libffi: unsupported CMAKE_SYSTEM_PROCESSOR='${CMAKE_SYSTEM_PROCESSOR}'")
endif()

set(_varn_ffi_target "")
if(WIN32)
    if(_varn_ffi_cpu_lc MATCHES "^(arm64|aarch64)$")
        set(_varn_ffi_target "ARM_WIN64")
    elseif(_varn_ffi_cpu_lc MATCHES "^arm")
        set(_varn_ffi_target "ARM_WIN32")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(_varn_ffi_target "X86_WIN32")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_varn_ffi_target "X86_WIN64")
    else()
        message(FATAL_ERROR "varn libffi: unsupported windows pointer size")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(_varn_ffi_target "X86_DARWIN")
elseif(_varn_ffi_cpu_lc MATCHES "^(arm64|aarch64|arm64_32)$")
    # arm64_32 (apple watchos) uses the aarch64 port in ilp32 mode
    set(_varn_ffi_target "ARM64")
elseif(_varn_ffi_cpu_lc MATCHES "^arm")
    set(_varn_ffi_target "ARM")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_varn_ffi_target "X86_64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(_varn_ffi_target "X86")
else()
    message(FATAL_ERROR "varn libffi: cannot map cpu '${_varn_ffi_cpu_lc}' / void_p=${CMAKE_SIZEOF_VOID_P}")
endif()

set(VARN_FFI_SIZEOF_SIZE_T "${CMAKE_SIZEOF_VOID_P}")
set(VARN_FFI_MMAP_EXEC_WRIT "0")
set(VARN_FFI_EXEC_TRAMPOLINE_TABLE "0")
set(VARN_FFI_EXEC_STATIC_TRAMP "0")

if(CMAKE_SYSTEM_NAME MATCHES "^(Darwin|iOS|tvOS|watchOS|visionOS)$")
    set(VARN_FFI_MMAP_EXEC_WRIT "1")
    if(_varn_ffi_cpu_lc MATCHES "^(arm|armv7|armv7l|armv7-a|armv7s|aarch64|arm64)$")
        set(VARN_FFI_EXEC_TRAMPOLINE_TABLE "1")
    endif()
endif()
if(ANDROID)
    set(VARN_FFI_MMAP_EXEC_WRIT "1")
endif()
# closures.c only redefines FFI_MMAP_EXEC_WRIT when it is 0, so windows needs it preset to 1 for VirtualAlloc and DEP
if(WIN32)
    set(VARN_FFI_MMAP_EXEC_WRIT "1")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID)
    if(_varn_ffi_cpu_lc MATCHES "^(x86|i386|i686|x86_64|amd64|aarch64|arm|armv7|armv7l|armv7-a)$")
        set(VARN_FFI_EXEC_STATIC_TRAMP "1")
    else()
        message(FATAL_ERROR "varn libffi: unsupported linux cpu '${_varn_ffi_cpu_lc}'")
    endif()
endif()

include(CheckTypeSize)
check_type_size("long double" VARN_FFI_CSIZE_LONG_DOUBLE)
check_type_size("double" VARN_FFI_CSIZE_DOUBLE)
if(NOT VARN_FFI_CSIZE_LONG_DOUBLE OR NOT VARN_FFI_CSIZE_DOUBLE)
    message(FATAL_ERROR "varn libffi: CheckTypeSize failed (long double or double)")
endif()
if(VARN_FFI_CSIZE_LONG_DOUBLE GREATER VARN_FFI_CSIZE_DOUBLE)
    set(VARN_FFI_HAVE_LONG_DOUBLE "1")
else()
    set(VARN_FFI_HAVE_LONG_DOUBLE "0")
endif()

set(VARN_FFI_GEN_DIR "${CMAKE_BINARY_DIR}/generated/varn_vendor_libffi")
set(VARN_FFI_GEN_INCLUDE "${VARN_FFI_GEN_DIR}/include")
file(MAKE_DIRECTORY "${VARN_FFI_GEN_DIR}")
file(MAKE_DIRECTORY "${VARN_FFI_GEN_INCLUDE}")

if(VARN_FFI_EXEC_STATIC_TRAMP STREQUAL "1")
    # tramp.c tests FFI_EXEC_STATIC_TRAMP with #ifdef, so the macro must be absent when disabled
    set(VARN_FFI_EXEC_STATIC_TRAMP_CPP [[#define FFI_EXEC_STATIC_TRAMP 1]])
else()
    set(VARN_FFI_EXEC_STATIC_TRAMP_CPP "")
endif()

# libffi tests have_as_* with #ifdef, so defining it to 0 still enables the branch and breaks msvc ml64
set(VARN_FFI_HAVE_AS_CFI_CPP "")
if(NOT WIN32)
    if(_varn_ffi_target MATCHES "^(X86|X86_64|X86_DARWIN)$")
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang|IntelLLVM|Intel")
            set(VARN_FFI_HAVE_AS_CFI_CPP [[#define HAVE_AS_CFI_PSEUDO_OP 1]])
        endif()
    endif()
endif()

# without this, unix64.S and sysv.S expand PCREL() to GNU "sym@rel" which Apple Clang rejects ("invalid variant 'rel'")
set(VARN_FFI_HAVE_AS_X86_PCREL_CPP "")
if(NOT WIN32)
    if(_varn_ffi_target MATCHES "^(X86|X86_64|X86_DARWIN)$")
        set(VARN_FFI_HAVE_AS_X86_PCREL_CPP [[#define HAVE_AS_X86_PCREL 1]])
    endif()
endif()

set(VARN_FFI_HAVE_AS_X86_64_UNWIND_CPP "")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID)
    if(_varn_ffi_target STREQUAL "X86_64")
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang|IntelLLVM|Intel")
            set(VARN_FFI_HAVE_AS_X86_64_UNWIND_CPP [[#define HAVE_AS_X86_64_UNWIND_SECTION_TYPE 1]])
        endif()
    endif()
endif()

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/fficonfig.h.in"
    "${VARN_FFI_GEN_DIR}/fficonfig.h"
    @ONLY
)

set(VERSION "${VARN_LIBFFI_VERSION}")
set(FFI_VERSION_STRING "${VARN_LIBFFI_VERSION}")
set(FFI_VERSION_NUMBER "${VARN_LIBFFI_VERSION_NUMBER}")
set(FFI_EXEC_TRAMPOLINE_TABLE "${VARN_FFI_EXEC_TRAMPOLINE_TABLE}")
set(HAVE_LONG_DOUBLE "${VARN_FFI_HAVE_LONG_DOUBLE}")
if(DEFINED TARGET)
    set(_varn_ffi_saved_target "${TARGET}")
endif()
set(TARGET "${_varn_ffi_target}")
configure_file(
    "${VARN_LIBFFI_ROOT}/include/ffi.h.in"
    "${VARN_FFI_GEN_INCLUDE}/ffi.h"
    @ONLY
)
if(DEFINED _varn_ffi_saved_target)
    set(TARGET "${_varn_ffi_saved_target}")
    unset(_varn_ffi_saved_target)
else()
    unset(TARGET)
endif()

if("${_varn_ffi_target}" STREQUAL "ARM_WIN64" OR "${_varn_ffi_target}" STREQUAL "ARM64")
    file(COPY "${VARN_LIBFFI_ROOT}/src/aarch64/ffitarget.h" DESTINATION "${VARN_FFI_GEN_INCLUDE}")
elseif("${_varn_ffi_target}" STREQUAL "ARM_WIN32" OR "${_varn_ffi_target}" STREQUAL "ARM")
    file(COPY "${VARN_LIBFFI_ROOT}/src/arm/ffitarget.h" DESTINATION "${VARN_FFI_GEN_INCLUDE}")
else()
    file(COPY "${VARN_LIBFFI_ROOT}/src/x86/ffitarget.h" DESTINATION "${VARN_FFI_GEN_INCLUDE}")
endif()

if(NOT TARGET varn_vendor_libffi)
    set(_varn_libffi_sources
        "${VARN_LIBFFI_ROOT}/src/closures.c"
        "${VARN_LIBFFI_ROOT}/src/prep_cif.c"
        "${VARN_LIBFFI_ROOT}/src/types.c"
        "${VARN_LIBFFI_ROOT}/src/tramp.c"
    )

    if("${_varn_ffi_target}" STREQUAL "ARM_WIN64" OR "${_varn_ffi_target}" STREQUAL "ARM64")
        list(APPEND _varn_libffi_sources "${VARN_LIBFFI_ROOT}/src/aarch64/ffi.c")
    elseif("${_varn_ffi_target}" STREQUAL "ARM_WIN32" OR "${_varn_ffi_target}" STREQUAL "ARM")
        list(APPEND _varn_libffi_sources "${VARN_LIBFFI_ROOT}/src/arm/ffi.c")
    else()
        list(APPEND _varn_libffi_sources
            "${VARN_LIBFFI_ROOT}/src/java_raw_api.c"
            "${VARN_LIBFFI_ROOT}/src/raw_api.c"
        )
        if("${_varn_ffi_target}" STREQUAL "X86_WIN32"
                OR "${_varn_ffi_target}" STREQUAL "X86_DARWIN"
                OR "${_varn_ffi_target}" STREQUAL "X86")
            list(APPEND _varn_libffi_sources "${VARN_LIBFFI_ROOT}/src/x86/ffi.c")
        elseif("${_varn_ffi_target}" STREQUAL "X86_WIN64")
            list(APPEND _varn_libffi_sources "${VARN_LIBFFI_ROOT}/src/x86/ffiw64.c")
        elseif("${_varn_ffi_target}" STREQUAL "X86_64")
            list(APPEND _varn_libffi_sources
                "${VARN_LIBFFI_ROOT}/src/x86/ffi64.c"
                "${VARN_LIBFFI_ROOT}/src/x86/ffiw64.c"
            )
        endif()
    endif()

    if("${_varn_ffi_target}" STREQUAL "X86")
        enable_language(ASM)
        set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -m32")
    elseif(NOT MSVC)
        enable_language(ASM)
    endif()

    if("${_varn_ffi_target}" STREQUAL "X86" OR "${_varn_ffi_target}" STREQUAL "X86_DARWIN")
        varn_libffi_add_assembly("${VARN_LIBFFI_ROOT}/src/x86/sysv.S")
    elseif("${_varn_ffi_target}" STREQUAL "X86_64")
        varn_libffi_add_assembly("${VARN_LIBFFI_ROOT}/src/x86/unix64.S")
        varn_libffi_add_assembly("${VARN_LIBFFI_ROOT}/src/x86/win64.S")
    elseif("${_varn_ffi_target}" STREQUAL "X86_WIN32")
        if(MSVC)
            varn_libffi_add_assembly("${VARN_LIBFFI_ROOT}/src/x86/sysv_intel.S")
        else()
            varn_libffi_add_assembly("${VARN_LIBFFI_ROOT}/src/x86/sysv.S")
        endif()
    elseif("${_varn_ffi_target}" STREQUAL "X86_WIN64")
        if(MSVC)
            varn_libffi_add_assembly("${VARN_LIBFFI_ROOT}/src/x86/win64_intel.S")
        else()
            varn_libffi_add_assembly("${VARN_LIBFFI_ROOT}/src/x86/win64.S")
        endif()
    elseif("${_varn_ffi_target}" STREQUAL "ARM_WIN32")
        if(MSVC)
            varn_libffi_add_assembly("${VARN_LIBFFI_ROOT}/src/arm/sysv_msvc_arm32.S")
        else()
            varn_libffi_add_assembly("${VARN_LIBFFI_ROOT}/src/arm/sysv.S")
        endif()
    elseif("${_varn_ffi_target}" STREQUAL "ARM")
        varn_libffi_add_assembly("${VARN_LIBFFI_ROOT}/src/arm/sysv.S")
    elseif("${_varn_ffi_target}" STREQUAL "ARM_WIN64")
        if(MSVC)
            varn_libffi_add_assembly("${VARN_LIBFFI_ROOT}/src/aarch64/win64_armasm.S")
        else()
            varn_libffi_add_assembly("${VARN_LIBFFI_ROOT}/src/aarch64/sysv.S")
        endif()
    elseif("${_varn_ffi_target}" STREQUAL "ARM64")
        varn_libffi_add_assembly("${VARN_LIBFFI_ROOT}/src/aarch64/sysv.S")
    else()
        message(FATAL_ERROR "varn libffi: no assembly path for target '${_varn_ffi_target}'")
    endif()

    add_library(varn_vendor_libffi STATIC ${_varn_libffi_sources})

    target_sources(varn_vendor_libffi PRIVATE
        "$<$<CONFIG:Debug>:${VARN_LIBFFI_ROOT}/src/debug.c>"
    )
    target_compile_definitions(varn_vendor_libffi PRIVATE
        "$<$<CONFIG:Debug>:FFI_DEBUG>"
    )

    set_target_properties(varn_vendor_libffi PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS ON
        POSITION_INDEPENDENT_CODE ON
    )

    target_compile_definitions(varn_vendor_libffi
        PRIVATE
            FFI_BUILDING
        PUBLIC
            FFI_STATIC_BUILD
    )

    if(MSVC)
        target_compile_options(varn_vendor_libffi PRIVATE /wd4101 /wd4244 /wd4267)
    elseif(WIN32 AND CMAKE_C_COMPILER_ID MATCHES "GNU")
        target_compile_options(varn_vendor_libffi PRIVATE
            -Wno-deprecated-declarations
            -Wno-pointer-to-int-cast
        )
    endif()

    if(NOT MSVC)
        target_compile_options(varn_vendor_libffi PRIVATE -fno-strict-aliasing)
    endif()

    target_include_directories(varn_vendor_libffi
        PUBLIC
            "${VARN_FFI_GEN_INCLUDE}"
            "${VARN_FFI_GEN_DIR}"
            "${VARN_LIBFFI_ROOT}/include"
            "${VARN_LIBFFI_ROOT}"
        PRIVATE
            "${VARN_LIBFFI_ROOT}/src"
    )

    if(NOT MSVC)
        target_compile_definitions(varn_vendor_libffi PRIVATE _GNU_SOURCE)
    endif()

    if(NOT WIN32)
        target_link_libraries(varn_vendor_libffi PRIVATE Threads::Threads)
    endif()

    if(APPLE AND CMAKE_OSX_ARCHITECTURES)
        set_target_properties(varn_vendor_libffi PROPERTIES OSX_ARCHITECTURES "${CMAKE_OSX_ARCHITECTURES}")
    endif()
endif()
