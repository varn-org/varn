# The `platform` module exposes operating system, architecture, and shared-library naming to Lua with no external deps.

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")

list(APPEND VARN_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/PlatformInfo.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/PlatformModule.cpp"
)
