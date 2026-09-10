# ffi is the native function interface where LIBFFI vendors libffi plus the lua-ffi parser and DUMMY stubs it

if(NOT DEFINED CACHE{VARN_FFI_DRIVER})
    if(VARN_BUILDING_FOR_EMSCRIPTEN)
        set(VARN_FFI_DRIVER "DUMMY" CACHE STRING "ffi backend: LIBFFI DUMMY")
    else()
        set(VARN_FFI_DRIVER "LIBFFI" CACHE STRING "ffi backend: LIBFFI DUMMY")
    endif()
endif()
set_property(CACHE VARN_FFI_DRIVER PROPERTY STRINGS LIBFFI DUMMY)
varn_validate_driver(VARN_FFI_DRIVER LIBFFI DUMMY)

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/FfiModule.cpp")

if(VARN_FFI_DRIVER STREQUAL "LIBFFI")
    set(VARN_NEEDS_LIBFFI ON)
else()
    list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/dummy/DummyFfiModule.cpp")
endif()

varn_register_driver(FFI "${VARN_FFI_DRIVER}")
