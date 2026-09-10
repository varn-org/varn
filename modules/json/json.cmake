# json is a standalone require("json") module plus the serializer used by http responses

if(NOT DEFINED CACHE{VARN_JSON_DRIVER})
    set(VARN_JSON_DRIVER "NLOHMANN" CACHE STRING "json serializer backend: NLOHMANN DUMMY")
endif()
set_property(CACHE VARN_JSON_DRIVER PROPERTY STRINGS NLOHMANN DUMMY)
varn_validate_driver(VARN_JSON_DRIVER NLOHMANN DUMMY)

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/JsonModule.cpp")

if(VARN_JSON_DRIVER STREQUAL "NLOHMANN")
    list(APPEND VARN_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/nlohmann/NlohmannJsonSerializer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/nlohmann/NlohmannJsonConvert.cpp"
    )
    set(VARN_NEEDS_NLOHMANN ON)
else()
    list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/dummy/DummyJsonSerializer.cpp")
endif()

varn_register_driver(JSON "${VARN_JSON_DRIVER}")
