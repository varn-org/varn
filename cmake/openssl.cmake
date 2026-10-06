# OpenSSL is built from source through `jimmy-park/openssl-cmake`.
# This file resolves the configure target and toolchain options per platform and per slice.

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
        # MSVC 14.51 for ARM64 can emit the stack probe of `/Gs0` before a prologue saves its return address, so a function such as `tls_parse_all_extensions` returns into itself.
        # The flag `-Gs` lands after `/Gs0` and restores the default probe threshold.
        list(APPEND _ossl_options -Gs)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_ossl_target "VC-WIN64A")
    else()
        set(_ossl_target "VC-WIN32")
    endif()
endif()

# The compiler launcher that CMake is given, whatever tool it names, such as a compiler cache, goes in front of the compiler that Configure takes, so the makefile it writes compiles through it.
list(JOIN CMAKE_C_COMPILER_LAUNCHER " " _ossl_launcher)

# The platforms tvOS and watchOS lack upstream OpenSSL configure targets, so they are emitted inline.
# MSVC builds through a target of its own that inherits the upstream one and keeps the debug information inside every object, since the shared PDB that the upstream target writes is one no compiler cache restores.
# The configuration of Android in OpenSSL derives the compiler wrapper of the NDK itself, over any compiler it is given, so with a launcher Android builds through a target of its own that puts the launcher in front of the compiler that configuration derived.
set(_ossl_conf_content "")
if(_ossl_embedded)
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
elseif(MSVC AND _ossl_target)
    set(_ossl_conf_content "my \$varn_embedded_debug_info = sub { join(' ', map { s{/Zi /Fd\\S+}{/Z7}gr } grep { defined(\$_) && \$_ ne '' } @_) };
my %targets = (
    'varn-${_ossl_target}' => {
        inherit_from => [ '${_ossl_target}' ],
        lib_cflags   => \$varn_embedded_debug_info,
        dso_cflags   => \$varn_embedded_debug_info,
        bin_cflags   => \$varn_embedded_debug_info,
    },
);
")
    set(_ossl_target "varn-${_ossl_target}")
elseif(ANDROID AND _ossl_launcher)
    string(REPLACE "\\" "\\\\" _ossl_quoted_launcher "${_ossl_launcher}")
    string(REPLACE "'" "\\'" _ossl_quoted_launcher "${_ossl_quoted_launcher}")
    set(_ossl_conf_content "my %targets = (
    'varn-${_ossl_target}' => {
        inherit_from => [ '${_ossl_target}' ],
        cflags       => sub { android_ndk(); \$user{CC} = '${_ossl_quoted_launcher} ' . \$user{CC}; add()->(@_) },
    },
);
")
    set(_ossl_target "varn-${_ossl_target}")
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

# The dependency scan of MSVC compiles every source of OpenSSL a second time to list its headers, which only a rebuild after a change of a header needs, and the sources come from a release archive that never changes in place.
if(MSVC)
    list(APPEND _ossl_options no-makedepend)
endif()

# OpenSSL builds with its own build system, so it takes the hidden visibility of a project that includes Varn with that preset, as the dependencies that CMake builds do, and the programs of that project export none of its symbols.
if(CMAKE_C_VISIBILITY_PRESET STREQUAL "hidden" AND NOT MSVC)
    list(APPEND _ossl_options -fvisibility=hidden)
endif()

# The key of a package of OpenSSL names its options without the launcher, which changes how a build compiles and never what it makes.
set(_ossl_package_options ${_ossl_options})

# Every platform but MSVC and Android takes the launcher with the compiler as the option CC, so a change of launcher configures OpenSSL again.
if(_ossl_launcher AND NOT ANDROID AND NOT MSVC)
    list(APPEND _ossl_options "CC=${_ossl_launcher} ${CMAKE_C_COMPILER}")
endif()

# The source of OpenSSL comes from its release archive checked against its SHA-256, and the recipe builds that local source.
CPMAddPackage(
    NAME varn_openssl_source
    VERSION 3.6.2
    URL https://github.com/openssl/openssl/releases/download/openssl-3.6.2/openssl-3.6.2.tar.gz
    URL_HASH SHA256=aaf51a1fe064384f811daeaeb4ec4dce7340ec8bd893027eee676af31e83a04f
    DOWNLOAD_ONLY YES
)

# The file is named after its content, which Configure takes as an option, so a change of the content, such as another launcher, configures OpenSSL again, and an unchanged file keeps its time, since OpenSSL tracks it as a dependency and would otherwise configure itself again in the middle of a build.
# It lives in the build folder that CPM gives the source of OpenSSL, next to the configured build of OpenSSL, so whatever keeps that build, such as a cache of the dependencies of a CI, keeps the file with it.
if(_ossl_conf_content)
    string(SHA256 _ossl_conf_hash "${_ossl_conf_content}")
    string(SUBSTRING "${_ossl_conf_hash}" 0 16 _ossl_conf_hash)
    set(_ossl_conf "${varn_openssl_source_BINARY_DIR}/openssl-varn-${_ossl_conf_hash}.conf")
    if(NOT EXISTS "${_ossl_conf}")
        file(WRITE "${_ossl_conf}" "${_ossl_conf_content}")
    endif()

    list(APPEND _ossl_options "--config=${_ossl_conf}")
endif()

set(OPENSSL_CONFIGURE_OPTIONS ${_ossl_options} CACHE INTERNAL "" FORCE)

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

# The makefile of Windows quotes the compiler as one program, so MSVC takes the launcher in the compiler macro of the command line of `nmake`, which every rule expands in front of its arguments, while Configure keeps its plain `cl`.
# The target `build_libs` builds the libraries in another `nmake`, which receives the macros of the command line only as variables of its environment and so compiles with the `cl` of the makefile, so one `nmake` builds the generated files and then the libraries itself.
if(MSVC)
    if(_ossl_launcher)
        list(APPEND OPENSSL_BUILD_OPTIONS "CC=${_ossl_launcher} cl")
    endif()
    set(OPENSSL_BUILD_TARGET build_generated _build_libs)
endif()

set(_ossl_jobs "$ENV{CMAKE_BUILD_PARALLEL_LEVEL}")
if(NOT MSVC AND _ossl_jobs MATCHES "^[0-9]+$" AND _ossl_jobs GREATER 1)
    list(APPEND OPENSSL_BUILD_OPTIONS -j ${_ossl_jobs})
endif()

# The recipe would look for one compiler cache by name, so Varn turns that off and hands Configure the launcher that CMake is given instead.
list(APPEND _ossl_cpm_options "OPENSSL_USE_CCACHE OFF")

# A folder that `VARN_PACKAGE_CACHE` names keeps every OpenSSL that a build made as a package of its headers and libraries, named after the hash of everything the build depends on: the release and the recipe, the target, the options and the configuration file of Configure, the compiler, the system and the variables of the environment that Configure reads. A build whose hash names a package of the folder takes it instead of building OpenSSL, so the package is what the build would make.
file(TO_CMAKE_PATH "$ENV{VARN_PACKAGE_CACHE}" _ossl_package_cache)
set(VARN_PACKAGE_CACHE "${_ossl_package_cache}" CACHE PATH "The folder of the prebuilt packages of the dependencies that Varn builds with their own build systems, or empty to build them in every tree")
if(VARN_PACKAGE_CACHE)
    set(_ossl_package_build_options ${OPENSSL_BUILD_OPTIONS})
    list(FILTER _ossl_package_build_options EXCLUDE REGEX "^(CC=|-j$|[0-9]+$)")
    set(_ossl_package_inputs
        "release 3.6.2 aaf51a1fe064384f811daeaeb4ec4dce7340ec8bd893027eee676af31e83a04f"
        "recipe d5a26d22bb7d17b6ed7a1298651b6ecb2c07c51d1e3a7bce6095def08e3f1f33"
        "target ${_ossl_target}"
        "options ${_ossl_package_options}"
        "configuration ${_ossl_conf_content}"
        "build ${_ossl_package_build_options} ${OPENSSL_BUILD_TARGET} shared ${BUILD_SHARED_LIBS}"
        "compiler ${CMAKE_C_COMPILER} ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION} ${CMAKE_C_COMPILER_TARGET} ${CMAKE_AR}"
        "system ${CMAKE_HOST_SYSTEM_NAME} ${CMAKE_HOST_SYSTEM_PROCESSOR} ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR} ${CMAKE_OSX_ARCHITECTURES} ${CMAKE_ANDROID_ARCH_ABI} ${ANDROID_NATIVE_API_LEVEL} ${CMAKE_ANDROID_NDK}"
        "environment CC=$ENV{CC} CFLAGS=$ENV{CFLAGS} CPPFLAGS=$ENV{CPPFLAGS} LDFLAGS=$ENV{LDFLAGS} LDLIBS=$ENV{LDLIBS} AR=$ENV{AR} RANLIB=$ENV{RANLIB} CROSS_COMPILE=$ENV{CROSS_COMPILE}"
    )
    list(JOIN _ossl_package_inputs "\n" _ossl_package_inputs)
    set(_ossl_package_inputs_file "${CMAKE_CURRENT_BINARY_DIR}/openssl-package-inputs.txt")
    file(WRITE "${_ossl_package_inputs_file}" "${_ossl_package_inputs}\n")
    string(SHA256 _ossl_package_hash "${_ossl_package_inputs}")
    string(SUBSTRING "${_ossl_package_hash}" 0 24 _ossl_package_hash)
    set(_ossl_package "${VARN_PACKAGE_CACHE}/openssl-3.6.2-${_ossl_package_hash}")

    if(EXISTS "${_ossl_package}/varn-package.cmake")
        message(STATUS "OpenSSL comes from the package ${_ossl_package}")
        varn_import_openssl_package("${_ossl_package}")
        return()
    endif()
endif()

CPMAddPackage(
    NAME openssl-cmake
    URL https://github.com/jimmy-park/openssl-cmake/archive/48c3f910074784adab7fe422cb955d71eda8fc4b.tar.gz
    URL_HASH SHA256=d5a26d22bb7d17b6ed7a1298651b6ecb2c07c51d1e3a7bce6095def08e3f1f33
    OPTIONS ${_ossl_cpm_options}
)

# A build that names a package folder keeps what it made there once OpenSSL built, so the next build of the same hash takes it. Every target that links OpenSSL waits for the package, whose step returns at once when the package is there.
if(VARN_PACKAGE_CACHE)
    add_custom_target(varn-openssl-package
        COMMAND "${CMAKE_COMMAND}" "-DPACKAGE=${_ossl_package}" "-DINCLUDE_DIRS=${OPENSSL_INCLUDE_DIR}" "-DLIBRARIES=${OPENSSL_CRYPTO_LIBRARY};${OPENSSL_SSL_LIBRARY}" "-DINPUTS=${_ossl_package_inputs_file}" -P "${VARN_ROOT}/cmake/openssl-package.cmake"
        VERBATIM
    )
    add_dependencies(varn-openssl-package openssl-build)
    add_dependencies(OpenSSL::Crypto varn-openssl-package)
endif()
