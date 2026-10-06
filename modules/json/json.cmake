# The `json` module is a standalone `require("json")` module plus the serializer used by HTTP responses.

varn_driver_option(VARN_JSON_DRIVER NLOHMANN "JSON serializer backend" NLOHMANN DUMMY)

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
list(APPEND VARN_CORE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/JsonModule.cpp")

if(VARN_JSON_DRIVER STREQUAL "NLOHMANN")
    list(APPEND VARN_CORE_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/nlohmann/NlohmannJsonSerializer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/nlohmann/NlohmannJsonDecoder.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/nlohmann/NlohmannJsonEncoder.cpp"
    )
    set(VARN_NEEDS_NLOHMANN ON)
else()
    list(APPEND VARN_CORE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/dummy/DummyJsonSerializer.cpp")
endif()

varn_register_driver(JSON "${VARN_JSON_DRIVER}")
