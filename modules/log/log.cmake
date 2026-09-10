# log is lua-facing logging bound to the shared core pipeline through a backend driver

if(NOT DEFINED CACHE{VARN_LOG_DRIVER})
    set(VARN_LOG_DRIVER "SPDLOG" CACHE STRING "log backend: SPDLOG STDOUT DUMMY")
endif()
set_property(CACHE VARN_LOG_DRIVER PROPERTY STRINGS SPDLOG STDOUT DUMMY)
varn_validate_driver(VARN_LOG_DRIVER SPDLOG STDOUT DUMMY)

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/LogModule.cpp")

if(VARN_LOG_DRIVER STREQUAL "SPDLOG")
    list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/spdlog/SpdlogLogBackend.cpp")
    set(VARN_NEEDS_SPDLOG ON)
elseif(VARN_LOG_DRIVER STREQUAL "STDOUT")
    list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/stdout/StdoutLogBackend.cpp")
else()
    list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/dummy/DummyLogBackend.cpp")
endif()

varn_register_driver(LOG "${VARN_LOG_DRIVER}")
