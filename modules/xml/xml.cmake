# xml is a standalone require("xml") module plus the serializer used by http responses

if(NOT DEFINED CACHE{VARN_XML_DRIVER})
    set(VARN_XML_DRIVER "PUGIXML" CACHE STRING "xml serializer backend: PUGIXML DUMMY")
endif()
set_property(CACHE VARN_XML_DRIVER PROPERTY STRINGS PUGIXML DUMMY)
varn_validate_driver(VARN_XML_DRIVER PUGIXML DUMMY)

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/XmlModule.cpp")

if(VARN_XML_DRIVER STREQUAL "PUGIXML")
    list(APPEND VARN_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/pugixml/PugixmlXmlSerializer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/pugixml/PugixmlXmlConvert.cpp"
    )
    set(VARN_NEEDS_PUGIXML ON)
else()
    list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/dummy/DummyXmlSerializer.cpp")
endif()

varn_register_driver(XML "${VARN_XML_DRIVER}")
