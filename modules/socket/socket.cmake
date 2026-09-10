# socket exposes tcp and udp transports to lua through a network driver

if(NOT DEFINED CACHE{VARN_SOCKET_DRIVER})
    set(VARN_SOCKET_DRIVER "POCO" CACHE STRING "socket backend: POCO DUMMY")
endif()
set_property(CACHE VARN_SOCKET_DRIVER PROPERTY STRINGS POCO DUMMY)
varn_validate_driver(VARN_SOCKET_DRIVER POCO DUMMY)

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/SocketModule.cpp")

if(VARN_SOCKET_DRIVER STREQUAL "POCO")
    list(APPEND VARN_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/poco/PocoSocketTcp.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/poco/PocoSocketUdp.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/poco/PocoSocketUnix.cpp"
    )
    if(VARN_ENABLE_TLS)
        list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/poco/PocoSocketTls.cpp")
    else()
        list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/poco/PocoSocketTlsStub.cpp")
    endif()
    set(VARN_NEEDS_POCO ON)
else()
    list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/dummy/DummySocketModule.cpp")
endif()

varn_register_driver(SOCKET "${VARN_SOCKET_DRIVER}")
