# The `socket` module exposes TCP and UDP transports to Lua through a network driver.

varn_driver_option(VARN_SOCKET_DRIVER POCO "Socket backend" POCO DUMMY EXTERNAL)

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
list(APPEND VARN_CORE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/SocketModule.cpp")

if(VARN_SOCKET_DRIVER STREQUAL "POCO")
    list(APPEND VARN_CORE_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/poco/PocoSocketBlocked.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/poco/PocoSocketResolver.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/poco/PocoSocketStream.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/poco/PocoSocketTcp.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/poco/PocoSocketText.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/poco/PocoSocketUdp.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/poco/PocoSocketUnix.cpp"
    )
    if(VARN_ENABLE_TLS)
        list(APPEND VARN_CORE_SOURCES
            "${CMAKE_CURRENT_LIST_DIR}/src/drivers/poco/PocoSocketTls.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/drivers/poco/TlsSession.cpp"
        )
    else()
        list(APPEND VARN_CORE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/poco/PocoSocketTlsStub.cpp")
    endif()
    set(VARN_NEEDS_POCO ON)
elseif(VARN_SOCKET_DRIVER STREQUAL "EXTERNAL")
    varn_add_external_driver(SOCKET VARN_CORE_SOURCES)
else()
    list(APPEND VARN_CORE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/dummy/DummySocketModule.cpp")
endif()

varn_register_driver(SOCKET "${VARN_SOCKET_DRIVER}")
