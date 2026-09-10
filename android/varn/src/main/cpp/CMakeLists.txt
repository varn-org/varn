cmake_minimum_required(VERSION 3.26)

project(varn_android LANGUAGES C CXX)

# build the varn static core from the repository root, then link it with the jni bindings.
set(VARN_TARGET "android" CACHE STRING "" FORCE)
get_filename_component(VARN_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)
add_subdirectory("${VARN_REPO_ROOT}" "${CMAKE_BINARY_DIR}/varn_root")

add_library(varn SHARED jni.cpp)
target_link_libraries(varn PRIVATE varn_core log)
