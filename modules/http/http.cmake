# http exposes a server and outbound client, each selecting an independent transport driver

if(NOT DEFINED CACHE{VARN_HTTP_SERVER_DRIVER})
    set(VARN_HTTP_SERVER_DRIVER "POCO" CACHE STRING "http server transport: POCO DUMMY")
endif()
set_property(CACHE VARN_HTTP_SERVER_DRIVER PROPERTY STRINGS POCO DUMMY)
varn_validate_driver(VARN_HTTP_SERVER_DRIVER POCO DUMMY)

if(NOT DEFINED CACHE{VARN_HTTP_CLIENT_DRIVER})
    set(VARN_HTTP_CLIENT_DRIVER "POCO" CACHE STRING "http client transport: POCO EMSCRIPTEN_FETCH DUMMY")
endif()
set_property(CACHE VARN_HTTP_CLIENT_DRIVER PROPERTY STRINGS POCO EMSCRIPTEN_FETCH DUMMY)
varn_validate_driver(VARN_HTTP_CLIENT_DRIVER POCO EMSCRIPTEN_FETCH DUMMY)

if(VARN_HTTP_CLIENT_DRIVER STREQUAL "EMSCRIPTEN_FETCH" AND NOT VARN_BUILDING_FOR_EMSCRIPTEN)
    message(FATAL_ERROR "VARN_HTTP_CLIENT_DRIVER=EMSCRIPTEN_FETCH is only valid for Emscripten builds.")
endif()

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
list(APPEND VARN_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/HttpTypes.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/HttpUrl.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/HttpClientModule.cpp"
)

if(VARN_HTTP_SERVER_DRIVER STREQUAL "POCO")
    list(APPEND VARN_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/src/HttpServerModule.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/HttpAppModule.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/HttpText.cpp"
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
    list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/dummy/HttpServerModuleStub.cpp")
endif()

if(VARN_HTTP_CLIENT_DRIVER STREQUAL "POCO")
    list(APPEND VARN_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/poco/PocoHttpClient.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/poco/PocoClientExchange.cpp"
    )
    set(VARN_NEEDS_POCO ON)
elseif(VARN_HTTP_CLIENT_DRIVER STREQUAL "EMSCRIPTEN_FETCH")
    list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/emscripten_fetch/EmscriptenFetchHttpClient.cpp")
else()
    list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/dummy/DummyHttpClient.cpp")
endif()

varn_register_driver(HTTP_SERVER "${VARN_HTTP_SERVER_DRIVER}")
varn_register_driver(HTTP_CLIENT "${VARN_HTTP_CLIENT_DRIVER}")
