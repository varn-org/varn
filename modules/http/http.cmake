# The `http` module exposes a server and an outbound client, each selecting an independent transport driver.
# The C++ client and its transports belong to the runtime without Lua, and the server and the Lua functions of the client belong to the Lua runtime.

varn_driver_option(VARN_HTTP_SERVER_DRIVER POCO "HTTP server transport" POCO DUMMY)

varn_driver_option(VARN_HTTP_CLIENT_DRIVER POCO "HTTP client transport" POCO APPLE ANDROID EMSCRIPTEN_FETCH DUMMY EXTERNAL)

if(VARN_HTTP_CLIENT_DRIVER STREQUAL "EMSCRIPTEN_FETCH" AND NOT VARN_BUILDING_FOR_EMSCRIPTEN)
    message(FATAL_ERROR "VARN_HTTP_CLIENT_DRIVER=EMSCRIPTEN_FETCH is only valid for Emscripten builds.")
endif()

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
list(APPEND VARN_RUNTIME_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/HttpTypes.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/HttpText.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/HttpUrl.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/HttpClient.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/HttpClientDownload.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/HttpClientFailureNote.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/HttpClientHandle.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/HttpClientTransfer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/HttpClientTypes.cpp"
)
list(APPEND VARN_CORE_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/HttpClientControllerLua.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/HttpClientLuaFunction.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/HttpClientModule.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/HttpClientReaderLua.cpp"
)

# A browser follows a redirect itself and hands over only where it ended up, so the following every other target does above its driver has nothing to do there and no blocking perform to be written against.
if(NOT VARN_HTTP_CLIENT_DRIVER STREQUAL "EMSCRIPTEN_FETCH")
    list(APPEND VARN_RUNTIME_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/HttpRedirects.cpp")
endif()

if(VARN_HTTP_SERVER_DRIVER STREQUAL "POCO")
    list(APPEND VARN_CORE_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/src/HttpServerModule.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/HttpAppModule.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/HttpUrlForm.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/HttpMultipart.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/HttpToken.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/Router.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/HttpRange.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/StaticFileHandler.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/StaticContent.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/MimeTypes.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/poco/TlsServerContext.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/reactor/ReactorHttpServer.cpp"
    )
    set(VARN_NEEDS_POCO ON)
    set(VARN_NEEDS_LLHTTP ON)
    set(VARN_NEEDS_ZLIB ON)
else()
    list(APPEND VARN_CORE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/dummy/HttpServerModuleStub.cpp")
endif()

if(VARN_HTTP_CLIENT_DRIVER STREQUAL "POCO")
    list(APPEND VARN_RUNTIME_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/poco/PocoHttpClient.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/poco/PocoClientExchange.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/poco/HttpClientConnections.cpp"
    )
    set(VARN_NEEDS_POCO ON)
elseif(VARN_HTTP_CLIENT_DRIVER STREQUAL "APPLE")
    # The URL Loading System is Objective-C, so this one file compiles as Objective-C++ and links Foundation.
    enable_language(OBJCXX)
    set(VARN_APPLE_HTTP_SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/drivers/apple/AppleHttpClient.mm")
    list(APPEND VARN_RUNTIME_SOURCES "${VARN_APPLE_HTTP_SOURCE}")
    # ARC owns the session, its delegate and every string handed to Foundation, which manual retain counting would leak.
    set_source_files_properties("${VARN_APPLE_HTTP_SOURCE}" PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    set(VARN_NEEDS_FOUNDATION ON)
elseif(VARN_HTTP_CLIENT_DRIVER STREQUAL "ANDROID")
    list(APPEND VARN_RUNTIME_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/android/AndroidHttpClient.cpp")
elseif(VARN_HTTP_CLIENT_DRIVER STREQUAL "EMSCRIPTEN_FETCH")
    list(APPEND VARN_RUNTIME_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/emscripten-fetch/EmscriptenFetchHttpClient.cpp")
elseif(VARN_HTTP_CLIENT_DRIVER STREQUAL "EXTERNAL")
    varn_add_external_driver(HTTP_CLIENT VARN_RUNTIME_SOURCES)
else()
    list(APPEND VARN_RUNTIME_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/dummy/DummyHttpClient.cpp")
endif()

varn_register_driver(HTTP_SERVER "${VARN_HTTP_SERVER_DRIVER}")
varn_register_driver(HTTP_CLIENT "${VARN_HTTP_CLIENT_DRIVER}")
