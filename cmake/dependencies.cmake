# Each module sets a VARN_NEEDS_<dep> flag in its own list file and the matching package is fetched here, so only the needed libraries download.
# Every package comes from an archive checked against its SHA-256, so a build always compiles the tree it was written against.

# Lua is the engine and is always vendored.
CPMAddPackage(
    SYSTEM YES
    NAME lua
    VERSION 5.5.0
    URL https://github.com/lua/lua/archive/refs/tags/v5.5.0.tar.gz
    URL_HASH SHA256=a33484f7ce4c14e12ea4d51cc5a7353bff2796a8074004b96ae2dc246f33f16e
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

    # Lua is built as C++, so a raised Lua error unwinds the embedding frames as an exception instead of a longjmp.
    # On MSVC a longjmp across the "/EHsc" frames at the boundary between C++ and Lua corrupts the unwind state and crashes.
    # An exception instead unwinds cleanly and stops at the protected call handler of Lua.
    set_source_files_properties(${_varn_lua_sources} PROPERTIES LANGUAGE CXX)

    # A yield jumps back to its resume with `_longjmp` from the header "varn-lua-unwind.h", since unwinding it as an exception costs about a hundred times more, while an error stays an exception.
    # MSVC keeps exceptions for both, because its `longjmp` unwinds every frame through the same machinery as an exception, so it runs the same destructors and saves only about half the cost.
    # The thread sanitizer cannot follow a `_longjmp` between the stacks of the threads it watches and stops the process, so its builds keep exceptions too.
    if(NOT MSVC AND NOT VARN_SANITIZE MATCHES "thread")
        set_source_files_properties("${lua_SOURCE_DIR}/ldo.c" PROPERTIES COMPILE_OPTIONS "-include;${CMAKE_CURRENT_LIST_DIR}/varn-lua-unwind.h")
    endif()

    add_library(varn_vendor_lua STATIC ${_varn_lua_sources})
    target_include_directories(varn_vendor_lua PUBLIC "${lua_SOURCE_DIR}")
    set_target_properties(varn_vendor_lua PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )

    # Mac Catalyst takes the SDK of iOS, where "system" is unavailable, so its Lua is the Lua of iOS.
    if(CMAKE_SYSTEM_NAME MATCHES "^(iOS|tvOS|watchOS|visionOS)$" OR VARN_MAC_CATALYST)
        target_compile_definitions(varn_vendor_lua PRIVATE LUA_USE_IOS)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        target_compile_definitions(varn_vendor_lua PRIVATE LUA_USE_MACOSX)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_definitions(varn_vendor_lua PRIVATE LUA_USE_LINUX)
        target_link_libraries(varn_vendor_lua PUBLIC dl m)
    elseif(ANDROID)
        target_compile_definitions(varn_vendor_lua PRIVATE LUA_USE_LINUX)

        # On a 64-bit ABI the seed buffer of `lauxlib.c` leaves no bytes to clear, which the fortify headers of the NDK report for its `memset`, so only that file drops the warnings those headers define.
        set_source_files_properties("${lua_SOURCE_DIR}/lauxlib.c" PROPERTIES COMPILE_OPTIONS "-Wno-user-defined-warnings")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
        target_compile_definitions(varn_vendor_lua PRIVATE LUA_USE_LINUX)
    elseif(WIN32)
        # The header "luaconf.h" enables LUA_USE_WINDOWS by itself when _WIN32 is defined.
    else()
        message(FATAL_ERROR "The Lua that Varn bundles does not support the platform \"${CMAKE_SYSTEM_NAME}\".")
    endif()
endif()

# The zlib recipe resolves before Poco, so Poco reuses its "ZLIB::ZLIB" target and libzip later picks it up through the "FindZLIB" the recipe overrides.
# The recipe checks the digest of the zlib archive it fetches itself.
if(VARN_NEEDS_ZIP OR VARN_NEEDS_ZLIB)
    CPMAddPackage(
        SYSTEM YES
        NAME zlib-cmake
        URL https://github.com/jimmy-park/zlib-cmake/archive/ace1a7672bdbdd3161cbb1d5284826e3fbe683b8.tar.gz
        URL_HASH SHA256=f1fb5e20d28d4e1451054ec1ced041bb114087aeb7f8672202ea146ee5c1c1fd
    )

    # The recipe publishes the checks of zlib to every target that links it, while libzip and others define "HAVE_UNISTD_H" themselves, so that check stays private to zlib.
    if(TARGET ZLIB)
        get_target_property(_varn_zlib_definitions ZLIB INTERFACE_COMPILE_DEFINITIONS)
        list(FILTER _varn_zlib_definitions EXCLUDE REGEX "HAVE_UNISTD_H")
        set_target_properties(ZLIB PROPERTIES INTERFACE_COMPILE_DEFINITIONS "${_varn_zlib_definitions}")
    endif()
endif()

# OpenSSL backs the crypto driver and TLS, and on Poco builds TLS reaches OpenSSL through Poco.
if(VARN_NEEDS_OPENSSL)
    include("${CMAKE_CURRENT_LIST_DIR}/openssl.cmake")
endif()

# Poco backs the HTTP server, the HTTP client and the TCP socket drivers.
if(VARN_NEEDS_POCO)
    # These Apple platforms mark "fork" and "exec" unavailable, so this disables only the process launch of Poco while its foundation, net, HTTP and socket parts still build.
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
        SYSTEM YES
        NAME Poco
        VERSION 1.15.3
        URL https://github.com/pocoproject/poco/archive/refs/tags/poco-1.15.3-release.tar.gz
        URL_HASH SHA256=4f112fea59e0c65f0fffe30a4957f8d66cf41528c21dd9903e6d7550022c794e
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

# The library libuv is the poller of the event loop on every platform, over epoll, kqueue and IOCP.
if(VARN_NEEDS_LIBUV)
    CPMAddPackage(
        SYSTEM YES
        NAME libuv
        VERSION 1.53.0
        URL https://github.com/libuv/libuv/archive/refs/tags/v1.53.0.tar.gz
        URL_HASH SHA256=279f3f67a24bb9921fe999ca6cd5e332fade8d515873ef9ba054b70e70a31d9e
        OPTIONS
            "LIBUV_BUILD_TESTS OFF"
            "LIBUV_BUILD_BENCH OFF"
            "LIBUV_BUILD_SHARED OFF"
    )

    # On tvOS and watchOS the process code of libuv still names "QUEUE_INIT", which it renamed "uv__queue_init", so the old name is given as a compile option, the one way CMake passes a macro with arguments.
    # Those systems leave out the branch that spawns processes, which leaves the variables of that branch unused.
    # On Android libuv reaches functions of newer API levels behind checks of its own, so their symbols are weak.
    if(TARGET uv_a)
        if(CMAKE_SYSTEM_NAME MATCHES "^(tvOS|watchOS)$")
            target_compile_options(uv_a PRIVATE "-DQUEUE_INIT(queue)=uv__queue_init(queue)" -Wno-unused-variable)
        elseif(ANDROID)
            target_compile_definitions(uv_a PRIVATE __ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__)
        endif()
    endif()
endif()

# The library llhttp parses the HTTP requests.
if(VARN_NEEDS_LLHTTP)
    CPMAddPackage(
        SYSTEM YES
        NAME llhttp
        VERSION 9.2.1
        URL https://github.com/nodejs/llhttp/archive/refs/tags/release/v9.2.1.tar.gz
        URL_HASH SHA256=3c163891446e529604b590f9ad097b2e98b5ef7e4d3ddcf1cf98b62ca668f23e
        OPTIONS
            "BUILD_SHARED_LIBS OFF"
            "BUILD_STATIC_LIBS ON"
    )
endif()

# The library spdlog backs the "SPDLOG" driver of the log.
if(VARN_NEEDS_SPDLOG)
    CPMAddPackage(
        SYSTEM YES
        NAME spdlog
        VERSION 1.17.0
        URL https://github.com/gabime/spdlog/archive/refs/tags/v1.17.0.tar.gz
        URL_HASH SHA256=d8862955c6d74e5846b3f580b1605d2428b11d97a410d86e2fb13e857cd3a744
        OPTIONS
            "SPDLOG_BUILD_EXAMPLE OFF"
            "SPDLOG_BUILD_TESTS OFF"
    )
endif()

# The JSON library of nlohmann backs the "NLOHMANN" driver of JSON, and a host that embeds Varn and already builds it shares its copy.
if(VARN_NEEDS_NLOHMANN AND NOT TARGET nlohmann_json::nlohmann_json)
    CPMAddPackage(
        SYSTEM YES
        NAME nlohmann_json
        VERSION 3.11.3
        URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz
        URL_HASH SHA256=0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406
    )
endif()

# The header "date.h" backs the datetime module with calendar arithmetic and the parsing and formatting of ISO 8601.
if(VARN_NEEDS_DATE)
    CPMAddPackage(
        SYSTEM YES
        NAME date
        VERSION 3.0.4
        URL https://github.com/HowardHinnant/date/archive/refs/tags/v3.0.4.tar.gz
        URL_HASH SHA256=56e05531ee8994124eeb498d0e6a5e1c3b9d4fccbecdf555fe266631368fb55f
        OPTIONS
            "BUILD_TZ_LIB OFF"
            "ENABLE_DATE_TESTING OFF"
    )
endif()

# The library pugixml backs the "PUGIXML" driver of XML.
if(VARN_NEEDS_PUGIXML)
    CPMAddPackage(
        SYSTEM YES
        NAME pugixml
        VERSION 1.14
        URL https://github.com/zeux/pugixml/archive/refs/tags/v1.14.tar.gz
        URL_HASH SHA256=610f98375424b5614754a6f34a491adbddaaec074e9044577d965160ec103d2e
        OPTIONS "BUILD_SHARED_LIBS OFF"
    )
endif()

# The library libzip backs the zip module and reuses the "ZLIB::ZLIB" target the zlib recipe resolved above.
if(VARN_NEEDS_ZIP)
    # Only Windows ships the functions of Annex K that end in "_s", so they are turned off elsewhere, where libzip can detect them wrongly when it cross compiles.
    if(NOT WIN32)
        foreach(_varn_no_annexk
            HAVE_MEMCPY_S HAVE_STRERROR_S HAVE_STRERRORLEN_S HAVE_STRNCPY_S
            HAVE_SNPRINTF_S HAVE_LOCALTIME_S HAVE__SNPRINTF_S HAVE__SNWPRINTF_S)
            set(${_varn_no_annexk} OFF CACHE INTERNAL "")
        endforeach()
    endif()

    CPMAddPackage(
        SYSTEM YES
        NAME libzip
        VERSION 1.10.1
        URL https://github.com/nih-at/libzip/archive/refs/tags/v1.10.1.tar.gz
        URL_HASH SHA256=d56d857d1c3ad4a7f3a4c01a51c6a6e5530e35ab93503f62276e8ba2b306186a
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
    # The strict C99 of glibc hides "strcasecmp" behind a feature macro, so _DEFAULT_SOURCE keeps "zip_name_locate.c" compiling.
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
