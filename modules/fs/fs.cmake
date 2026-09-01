# fs provides async read/write and sync exists backed by a storage driver

if(NOT DEFINED CACHE{VARN_FS_DRIVER})
    set(VARN_FS_DRIVER "STD" CACHE STRING "fs storage backend: STD DUMMY")
endif()
set_property(CACHE VARN_FS_DRIVER PROPERTY STRINGS STD DUMMY)
varn_validate_driver(VARN_FS_DRIVER STD DUMMY)

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/FsModule.cpp")

if(VARN_FS_DRIVER STREQUAL "STD")
    list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/std/StdFsStorage.cpp")
else()
    list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/dummy/DummyFsStorage.cpp")
endif()

varn_register_driver(FS "${VARN_FS_DRIVER}")
