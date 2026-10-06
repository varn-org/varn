# The `dataview` module shares memory between the host and Lua by reference, so a large buffer such as a camera frame crosses the bridge as an address and a size instead of a copy.

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
list(APPEND VARN_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/DataView.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/DataViewModule.cpp"
)

# The tokens of the bridge are JSON, so their reader exists only in a build with a JSON serializer.
if(VARN_JSON_DRIVER STREQUAL "NLOHMANN")
    list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/DataViewJson.cpp")
endif()

# Objective-C and Swift reach views through `VarnDataView`, which Apple builds compile into the core with automatic reference counting.
if(APPLE)
    enable_language(OBJCXX)
    set(VARN_DATAVIEW_APPLE_SOURCE "${CMAKE_CURRENT_LIST_DIR}/platform/apple/VarnDataView.mm")
    list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/platform/apple")
    list(APPEND VARN_SOURCES "${VARN_DATAVIEW_APPLE_SOURCE}")
    set_source_files_properties("${VARN_DATAVIEW_APPLE_SOURCE}" PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    set(VARN_NEEDS_FOUNDATION ON)
endif()

# Java and Kotlin reach views through `com.varn.VarnDataView`, whose natives the host registers from its `JNI_OnLoad` with `DataViewJni::publish`.
if(ANDROID)
    list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/platform/android")
    list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/platform/android/DataViewJni.cpp")
endif()
