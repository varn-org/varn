# archive create, extract and list backed by libzip, toggled by VARN_ENABLE_ZIP

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
list(APPEND VARN_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/ZipModule.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/ZipPath.cpp"
)

if(VARN_ENABLE_ZIP)
    set(VARN_NEEDS_ZIP ON)
endif()
