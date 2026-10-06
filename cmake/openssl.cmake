# OpenSSL is built from source through `jimmy-park/openssl-cmake`.
# This file resolves the configure target and toolchain options per platform and per slice.
# The platforms tvOS and watchOS have no upstream OpenSSL configure targets, so they are emitted inline here.

# The active architecture comes from the Apple slice request, otherwise from the host processor.
if(CMAKE_OSX_ARCHITECTURES)
    list(GET CMAKE_OSX_ARCHITECTURES 0 _ossl_arch)
else()
    set(_ossl_arch "${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(_ossl_simulator OFF)
if(CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
    set(_ossl_simulator ON)
endif()

set(_ossl_target "")
set(_ossl_options "")
set(_ossl_embedded OFF)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    if(_ossl_simulator)
        if(_ossl_arch STREQUAL "x86_64")
            set(_ossl_target "iossimulator-x86_64-xcrun")
        else()
            set(_ossl_target "iossimulator-arm64-xcrun")
        endif()
        list(APPEND _ossl_options "-mios-simulator-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    else()
        set(_ossl_target "ios64-xcrun")
        list(APPEND _ossl_options "-miphoneos-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "tvOS")
    set(_ossl_embedded ON)
    if(_ossl_simulator)
        if(_ossl_arch STREQUAL "x86_64")
            set(_ossl_target "tvossimulator-x86_64-xcrun")
        else()
            set(_ossl_target "tvossimulator-arm64-xcrun")
        endif()
        list(APPEND _ossl_options "-mtvos-simulator-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    else()
        set(_ossl_target "tvos64-xcrun")
        list(APPEND _ossl_options "-mtvos-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "watchOS")
    set(_ossl_embedded ON)
    if(_ossl_simulator)
        if(_ossl_arch STREQUAL "x86_64")
            set(_ossl_target "watchossimulator-x86_64-xcrun")
        else()
            set(_ossl_target "watchossimulator-arm64-xcrun")
        endif()
        list(APPEND _ossl_options "-mwatchos-simulator-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    else()
        set(_ossl_target "watchos-arm64_32-xcrun")
        list(APPEND _ossl_options "-mwatchos-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "visionOS")
    if(_ossl_simulator)
        list(APPEND _ossl_options "-mtargetos=xros${CMAKE_OSX_DEPLOYMENT_TARGET}-simulator")
    else()
        list(APPEND _ossl_options "-mtargetos=xros${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif()
elseif(ANDROID)
    if(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
        set(_ossl_target "android-arm64")
    elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "armeabi-v7a")
        set(_ossl_target "android-arm")
    elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "x86_64")
        set(_ossl_target "android-x86_64")
    elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "x86")
        set(_ossl_target "android-x86")
    endif()
    set(ENV{ANDROID_API} ${ANDROID_NATIVE_API_LEVEL})
    set(ENV{ANDROID_NDK_ROOT} ${CMAKE_ANDROID_NDK})
    list(APPEND _ossl_options no-ui-console no-engine)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(_ossl_arch MATCHES "arm64|aarch64")
        set(_ossl_target "darwin64-arm64-cc")
    else()
        set(_ossl_target "darwin64-x86_64-cc")
    endif()
    # Mac Catalyst compiles for the target triple the toolchain names, since the flags of the environment would build for macOS.
    if(VARN_MAC_CATALYST)
        list(APPEND _ossl_options "-target ${CMAKE_C_COMPILER_TARGET}")
    elseif(CMAKE_OSX_DEPLOYMENT_TARGET)
        list(APPEND _ossl_options "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(_ossl_arch MATCHES "arm64|aarch64")
        set(_ossl_target "linux-aarch64")
    else()
        set(_ossl_target "linux-x86_64")
    endif()
elseif(WIN32)
    if(_ossl_arch MATCHES "ARM64|aarch64|arm64")
        set(_ossl_target "VC-WIN64-ARM")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_ossl_target "VC-WIN64A")
    else()
        set(_ossl_target "VC-WIN32")
    endif()
endif()

# The platforms tvOS and watchOS lack upstream OpenSSL configure targets, so they are emitted inline.
if(_ossl_embedded)
    set(_ossl_conf "${CMAKE_CURRENT_BINARY_DIR}/openssl-apple-embedded.conf")

    set(_ossl_conf_content [=[
my %targets = (
    "tvos-common" => {
        template       => 1,
        inherit_from   => [ "darwin-common" ],
        sys_id         => "tvOS",
        disable        => [ "async" ],
    },
    "tvos64-xcrun" => {
        inherit_from   => [ "tvos-common" ],
        CC             => "xcrun -sdk appletvos cc",
        cflags         => add("-arch arm64 -fno-common"),
        bn_ops         => "SIXTY_FOUR_BIT_LONG RC4_CHAR",
        asm_arch       => 'aarch64',
        perlasm_scheme => "ios64",
    },
    "tvossimulator-arm64-xcrun" => {
        inherit_from   => [ "tvos-common" ],
        CC             => "xcrun -sdk appletvsimulator cc",
        cflags         => add("-arch arm64 -fno-common"),
        bn_ops         => "SIXTY_FOUR_BIT_LONG",
        asm_arch       => 'aarch64',
        perlasm_scheme => "ios64",
    },
    "tvossimulator-x86_64-xcrun" => {
        inherit_from   => [ "tvos-common" ],
        CC             => "xcrun -sdk appletvsimulator cc",
        cflags         => add("-arch x86_64 -fno-common"),
        bn_ops         => "SIXTY_FOUR_BIT_LONG",
        asm_arch       => 'x86_64',
        perlasm_scheme => "macosx",
    },
    "watchos-common" => {
        template       => 1,
        inherit_from   => [ "darwin-common" ],
        sys_id         => "watchOS",
        disable        => [ "async", "asm" ],
    },
    "watchos-arm64_32-xcrun" => {
        inherit_from   => [ "watchos-common" ],
        CC             => "xcrun -sdk watchos cc",
        cflags         => add("-arch arm64_32 -fno-common"),
        bn_ops         => "SIXTY_FOUR_BIT",
    },
    "watchossimulator-arm64-xcrun" => {
        inherit_from   => [ "watchos-common" ],
        CC             => "xcrun -sdk watchsimulator cc",
        cflags         => add("-arch arm64 -fno-common"),
        bn_ops         => "SIXTY_FOUR_BIT_LONG",
    },
    "watchossimulator-x86_64-xcrun" => {
        inherit_from   => [ "watchos-common" ],
        CC             => "xcrun -sdk watchsimulator cc",
        cflags         => add("-arch x86_64 -fno-common"),
        bn_ops         => "SIXTY_FOUR_BIT_LONG",
    },
);
]=])

    # The file is written only when the content changes so its mtime stays stable across reconfigures.
    # OpenSSL tracks the `--config` file as a dependency and would otherwise rebuild mid-make.
    set(_ossl_conf_current "")
    if(EXISTS "${_ossl_conf}")
        file(READ "${_ossl_conf}" _ossl_conf_current)
    endif()

    if(NOT _ossl_conf_current STREQUAL _ossl_conf_content)
        file(WRITE "${_ossl_conf}" "${_ossl_conf_content}")
    endif()

    list(APPEND _ossl_options "--config=${_ossl_conf}")
endif()

# The recipe `openssl-cmake` compiles with a plain `cc` and drops the SDK sysroot from the target triple.
# Without an explicit `-isysroot` the compiler picks the macOS SDK while targeting another Apple platform.
# On the x86_64 simulator that produces wrong-arch objects and later trips an `ld` arch-mismatch warning.
if(CMAKE_OSX_SYSROOT)
    if(IS_DIRECTORY "${CMAKE_OSX_SYSROOT}")
        set(_ossl_sysroot "${CMAKE_OSX_SYSROOT}")
    else()
        execute_process(
            COMMAND xcrun --sdk ${CMAKE_OSX_SYSROOT} --show-sdk-path
            OUTPUT_VARIABLE _ossl_sysroot
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()

    if(_ossl_sysroot)
        list(APPEND _ossl_options "-isysroot ${_ossl_sysroot}")
    endif()
endif()

# Varn never ships the OpenSSL CLI, and the tvOS, watchOS and visionOS SDKs block the fork and exec its apps need.
list(PREPEND _ossl_options no-apps)

set(OPENSSL_CONFIGURE_OPTIONS ${_ossl_options} CACHE INTERNAL "" FORCE)

# The source of OpenSSL comes from its release archive checked against its SHA-256, and the recipe builds that local source.
CPMAddPackage(
    NAME varn_openssl_source
    VERSION 3.6.2
    URL https://github.com/openssl/openssl/releases/download/openssl-3.6.2/openssl-3.6.2.tar.gz
    URL_HASH SHA256=aaf51a1fe064384f811daeaeb4ec4dce7340ec8bd893027eee676af31e83a04f
    DOWNLOAD_ONLY YES
)

set(_ossl_cpm_options "OPENSSL_TARGET_VERSION 3.6.2" "OPENSSL_SOURCE ${varn_openssl_source_SOURCE_DIR}" "OPENSSL_ENABLE_PARALLEL OFF")
if(_ossl_target)
    list(APPEND _ossl_cpm_options "OPENSSL_TARGET_PLATFORM ${_ossl_target}")
endif()

# The build of OpenSSL keeps to the job count given in `CMAKE_BUILD_PARALLEL_LEVEL`, and runs one job when none is given.
# Its archiver creates each library quietly, which only the command line of `make` can ask for, since the Android targets of OpenSSL replace the archive flags a configure is given.
set(OPENSSL_BUILD_OPTIONS "")
if(NOT MSVC)
    list(APPEND OPENSSL_BUILD_OPTIONS ARFLAGS=rcs)
endif()

set(_ossl_jobs "$ENV{CMAKE_BUILD_PARALLEL_LEVEL}")
if(NOT MSVC AND _ossl_jobs MATCHES "^[0-9]+$" AND _ossl_jobs GREATER 1)
    list(APPEND OPENSSL_BUILD_OPTIONS -j ${_ossl_jobs})
endif()

CPMAddPackage(
    NAME openssl-cmake
    URL https://github.com/jimmy-park/openssl-cmake/archive/48c3f910074784adab7fe422cb955d71eda8fc4b.tar.gz
    URL_HASH SHA256=d5a26d22bb7d17b6ed7a1298651b6ecb2c07c51d1e3a7bce6095def08e3f1f33
    OPTIONS ${_ossl_cpm_options}
)
