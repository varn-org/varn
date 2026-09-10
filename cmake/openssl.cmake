# openssl built from source via jimmy-park/openssl-cmake
# resolves the configure target and toolchain options per platform and per slice
# tvos and watchos have no upstream openssl configure targets, so they are emitted inline here

# pick the active architecture from the apple slice request, otherwise the host processor
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
    if(CMAKE_OSX_DEPLOYMENT_TARGET)
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

# tvos and watchos lack upstream openssl configure targets, so emit them inline
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

    # write only when the content changes so the file mtime stays stable across reconfigures
    # openssl tracks the --config file as a dependency and would otherwise rebuild mid-make
    set(_ossl_conf_current "")
    if(EXISTS "${_ossl_conf}")
        file(READ "${_ossl_conf}" _ossl_conf_current)
    endif()

    if(NOT _ossl_conf_current STREQUAL _ossl_conf_content)
        file(WRITE "${_ossl_conf}" "${_ossl_conf_content}")
    endif()

    list(APPEND _ossl_options "--config=${_ossl_conf}")
endif()

# openssl-cmake compiles with a plain cc and drops the sdk sysroot from the target triple
# without an explicit -isysroot the compiler picks the macos sdk while targeting another apple platform
# on the x86_64 simulator that produces wrong-arch objects and later trips an ld arch-mismatch warning
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

# varn never ships the openssl cli, and tvos/watchos/visionos sdks block the fork/exec its apps need
list(PREPEND _ossl_options no-apps)

set(OPENSSL_CONFIGURE_OPTIONS ${_ossl_options} CACHE INTERNAL "" FORCE)

set(_ossl_cpm_options "OPENSSL_TARGET_VERSION 3.6.2")
if(_ossl_target)
    list(APPEND _ossl_cpm_options "OPENSSL_TARGET_PLATFORM ${_ossl_target}")
endif()

CPMAddPackage(
    NAME openssl-cmake
    URL https://github.com/jimmy-park/openssl-cmake/archive/main.tar.gz
    OPTIONS ${_ossl_cpm_options}
)
