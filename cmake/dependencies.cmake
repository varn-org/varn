# each module sets a VARN_NEEDS_<dep> flag in its own .cmake and the matching package is fetched here so only the needed libraries download

# lua is the engine and is always vendored
CPMAddPackage(
    NAME lua
    VERSION 5.5.0
    GITHUB_REPOSITORY lua/lua
    GIT_TAG v5.5.0
    DOWNLOAD_ONLY YES
)

if(NOT TARGET varn_vendor_lua)
    set(_varn_lua_sources
        lapi.c lauxlib.c lbaselib.c lcode.c lcorolib.c lctype.c ldblib.c ldebug.c
        ldo.c ldump.c lfunc.c lgc.c linit.c liolib.c llex.c lmathlib.c lmem.c
        loadlib.c lobject.c lopcodes.c loslib.c lparser.c lstate.c lstring.c
        lstrlib.c ltable.c ltablib.c ltm.c lundump.c lutf8lib.c lvm.c lzio.c
    )
    list(TRANSFORM _varn_lua_sources PREPEND "${lua_SOURCE_DIR}/")

    # lua is built as c++ so a raised lua error unwinds the embedding frames as an exception instead of a longjmp.
    # on msvc a longjmp across the /ehsc frames at the c++/lua boundary corrupts the unwind state and crashes.
    # an exception instead unwinds cleanly and stops at lua's own protected-call handler.
    set_source_files_properties(${_varn_lua_sources} PROPERTIES LANGUAGE CXX)

    add_library(varn_vendor_lua STATIC ${_varn_lua_sources})
    target_include_directories(varn_vendor_lua PUBLIC "${lua_SOURCE_DIR}")
    set_target_properties(varn_vendor_lua PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )

    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        target_compile_definitions(varn_vendor_lua PRIVATE LUA_USE_MACOSX)
    elseif(CMAKE_SYSTEM_NAME MATCHES "^(iOS|tvOS|watchOS|visionOS)$")
        target_compile_definitions(varn_vendor_lua PRIVATE LUA_USE_IOS)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_definitions(varn_vendor_lua PRIVATE LUA_USE_LINUX)
        target_link_libraries(varn_vendor_lua PUBLIC dl m)
    elseif(ANDROID)
        target_compile_definitions(varn_vendor_lua PRIVATE LUA_USE_LINUX)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
        target_compile_definitions(varn_vendor_lua PRIVATE LUA_USE_LINUX)
    elseif(WIN32)
        # luaconf.h enables lua_use_windows automatically when _win32 is defined
    else()
        message(FATAL_ERROR "varn bundled lua: unsupported platform (${CMAKE_SYSTEM_NAME})")
    endif()
endif()

# zlib resolves before poco so poco reuses this ZLIB::ZLIB target and libzip later picks it up through zlib-cmake's FindZLIB override
if(VARN_NEEDS_ZIP OR VARN_NEEDS_ZLIB)
    CPMAddPackage(
        NAME zlib-cmake
        URL https://github.com/jimmy-park/zlib-cmake/archive/main.tar.gz
    )
endif()

# openssl backs the crypto driver and tls, and on poco builds tls reaches openssl through poco
if(VARN_NEEDS_OPENSSL)
    include("${CMAKE_CURRENT_LIST_DIR}/openssl.cmake")
endif()

# poco backs the http server, http client and tcp socket drivers
if(VARN_NEEDS_POCO)
    # these apple platforms mark fork/exec unavailable, so this disables only poco's process launch path while foundation, net, http and socket still build
    if(CMAKE_SYSTEM_NAME MATCHES "^(tvOS|watchOS|visionOS)$")
        add_compile_definitions(POCO_NO_FORK_EXEC)
    endif()

    if(VARN_ENABLE_TLS)
        if(WIN32)
            set(_varn_poco_netssl_options "ENABLE_NETSSL OFF" "ENABLE_NETSSL_WIN ON")
        else()
            set(_varn_poco_netssl_options "ENABLE_NETSSL ON" "ENABLE_NETSSL_WIN OFF")
        endif()
        set(_varn_poco_crypto_option "ENABLE_CRYPTO ON")
    else()
        set(_varn_poco_netssl_options "ENABLE_NETSSL OFF" "ENABLE_NETSSL_WIN OFF")
        set(_varn_poco_crypto_option "ENABLE_CRYPTO OFF")
    endif()

    CPMAddPackage(
        NAME Poco
        VERSION 1.15.3
        GITHUB_REPOSITORY pocoproject/poco
        GIT_TAG poco-1.15.3-release
        OPTIONS
            "BUILD_SHARED_LIBS OFF"
            "ENABLE_FOUNDATION ON"
            "ENABLE_NET ON"
            ${_varn_poco_netssl_options}
            ${_varn_poco_crypto_option}
            "ENABLE_UTIL ON"
            "ENABLE_JSON OFF"
            "ENABLE_XML OFF"
            "ENABLE_MONGODB OFF"
            "ENABLE_DATA OFF"
            "ENABLE_DATA_SQLITE OFF"
            "ENABLE_DATA_MYSQL OFF"
            "ENABLE_DATA_POSTGRESQL OFF"
            "ENABLE_DATA_ODBC OFF"
            "POCO_ENABLE_SQL OFF"
            "ENABLE_REDIS OFF"
            "ENABLE_PROMETHEUS OFF"
            "ENABLE_ENCODINGS OFF"
            "ENABLE_ENCODINGS_COMPILER OFF"
            "ENABLE_PAGECOMPILER OFF"
            "ENABLE_PAGECOMPILER_FILE2PAGE OFF"
            "ENABLE_ACTIVERECORD OFF"
            "ENABLE_ACTIVERECORD_COMPILER OFF"
            "ENABLE_ZIP OFF"
            "ENABLE_JWT OFF"
            "ENABLE_APACHECONNECTOR OFF"
            "ENABLE_TESTS OFF"
            "ENABLE_SAMPLES OFF"
    )
endif()

# libuv is the cross-platform event loop poller (epoll/kqueue/iocp) the runtime waits on natively
if(VARN_NEEDS_LIBUV)
    CPMAddPackage(
        NAME libuv
        VERSION 1.49.2
        GITHUB_REPOSITORY libuv/libuv
        GIT_TAG v1.49.2
        OPTIONS
            "LIBUV_BUILD_TESTS OFF"
            "LIBUV_BUILD_BENCH OFF"
            "LIBUV_BUILD_SHARED OFF"
    )
endif()

# llhttp is the http request parser
if(VARN_NEEDS_LLHTTP)
    CPMAddPackage(
        NAME llhttp
        VERSION 9.2.1
        GITHUB_REPOSITORY nodejs/llhttp
        GIT_TAG release/v9.2.1
        OPTIONS
            "BUILD_SHARED_LIBS OFF"
            "BUILD_STATIC_LIBS ON"
    )
endif()

# spdlog backs the log SPDLOG driver
if(VARN_NEEDS_SPDLOG)
    CPMAddPackage(
        NAME spdlog
        VERSION 1.17.0
        GITHUB_REPOSITORY gabime/spdlog
        GIT_TAG v1.17.0
        OPTIONS
            "SPDLOG_BUILD_EXAMPLE OFF"
            "SPDLOG_BUILD_TESTS OFF"
    )
endif()

# nlohmann_json backs the json NLOHMANN driver
if(VARN_NEEDS_NLOHMANN)
    CPMAddPackage(
        NAME nlohmann_json
        VERSION 3.11.3
        GITHUB_REPOSITORY nlohmann/json
        GIT_TAG v3.11.3
    )
endif()

# date.h backs the datetime module with calendar arithmetic and iso-8601 parsing and formatting
if(VARN_NEEDS_DATE)
    CPMAddPackage(
        NAME date
        VERSION 3.0.4
        GITHUB_REPOSITORY HowardHinnant/date
        GIT_TAG v3.0.4
        OPTIONS
            "BUILD_TZ_LIB OFF"
            "ENABLE_DATE_TESTING OFF"
    )
endif()

# pugixml backs the xml PUGIXML driver
if(VARN_NEEDS_PUGIXML)
    CPMAddPackage(
        NAME pugixml
        VERSION 1.14
        GITHUB_REPOSITORY zeux/pugixml
        GIT_TAG v1.14
        OPTIONS "BUILD_SHARED_LIBS OFF"
    )
endif()

# libzip backs the zip module and reuses the ZLIB::ZLIB resolved above via zlib-cmake
if(VARN_NEEDS_ZIP)
    # only windows ships the annex k (_s) functions, so pin them off elsewhere where libzip can misdetect them when cross-compiling
    if(NOT WIN32)
        foreach(_varn_no_annexk
            HAVE_MEMCPY_S HAVE_STRERROR_S HAVE_STRERRORLEN_S HAVE_STRNCPY_S
            HAVE_SNPRINTF_S HAVE_LOCALTIME_S HAVE__SNPRINTF_S HAVE__SNWPRINTF_S)
            set(${_varn_no_annexk} OFF CACHE INTERNAL "")
        endforeach()
    endif()

    CPMAddPackage(
        NAME libzip
        VERSION 1.10.1
        GITHUB_REPOSITORY nih-at/libzip
        GIT_TAG v1.10.1
        OPTIONS
            "BUILD_SHARED_LIBS OFF"
            "BUILD_TOOLS OFF"
            "BUILD_EXAMPLES OFF"
            "BUILD_DOC OFF"
            "BUILD_REGRESS OFF"
            "LIBZIP_DO_INSTALL OFF"
            "ENABLE_OPENSSL OFF"
            "ENABLE_MBEDTLS OFF"
            "ENABLE_GNUTLS OFF"
            "ENABLE_COMMONCRYPTO OFF"
            "ENABLE_WINDOWS_CRYPTO OFF"
            "ENABLE_BZIP2 OFF"
            "ENABLE_LZMA OFF"
            "ENABLE_ZSTD OFF"
    )
    # glibc strict C99 hides strcasecmp behind a feature macro, so define _DEFAULT_SOURCE to keep zip_name_locate.c compiling
    if(TARGET zip)
        set_target_properties(zip PROPERTIES
            C_STANDARD 99
            C_STANDARD_REQUIRED ON
            C_EXTENSIONS OFF
        )
        if(NOT WIN32)
            target_compile_definitions(zip PRIVATE $<$<COMPILE_LANGUAGE:C>:_DEFAULT_SOURCE>)
        endif()
    endif()
endif()
