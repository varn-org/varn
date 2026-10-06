# The `fs` module provides async read and write plus a sync `exists`, backed by a storage driver.

varn_driver_option(VARN_FS_DRIVER STD "Filesystem storage backend" STD DUMMY EXTERNAL)

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
list(APPEND VARN_CORE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/FsModule.cpp")

if(VARN_FS_DRIVER STREQUAL "STD")
    list(APPEND VARN_CORE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/std/StdFsStorage.cpp")
elseif(VARN_FS_DRIVER STREQUAL "EXTERNAL")
    varn_add_external_driver(FS VARN_CORE_SOURCES)
else()
    list(APPEND VARN_CORE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/dummy/DummyFsStorage.cpp")
endif()

varn_register_driver(FS "${VARN_FS_DRIVER}")
