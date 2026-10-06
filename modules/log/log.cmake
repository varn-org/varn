# The `log` module is Lua-facing logging bound to the shared log pipeline, whose backend driver the runtime without Lua holds.

varn_driver_option(VARN_LOG_DRIVER SPDLOG "Log backend" SPDLOG STDOUT DUMMY)

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
list(APPEND VARN_CORE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/LogModule.cpp")

if(VARN_LOG_DRIVER STREQUAL "SPDLOG")
    list(APPEND VARN_RUNTIME_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/spdlog/SpdlogLogBackend.cpp")
    set(VARN_NEEDS_SPDLOG ON)
elseif(VARN_LOG_DRIVER STREQUAL "STDOUT")
    list(APPEND VARN_RUNTIME_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/stdout/StdoutLogBackend.cpp")
else()
    list(APPEND VARN_RUNTIME_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/dummy/DummyLogBackend.cpp")
endif()

varn_register_driver(LOG "${VARN_LOG_DRIVER}")
