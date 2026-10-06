# The `ffi` module is the native function interface, where `LIBFFI` vendors libffi plus the lua-ffi parser and `DUMMY` stubs it.

if(VARN_BUILDING_FOR_EMSCRIPTEN)
    set(_varn_ffi_default DUMMY)
else()
    set(_varn_ffi_default LIBFFI)
endif()
varn_driver_option(VARN_FFI_DRIVER ${_varn_ffi_default} "FFI backend" LIBFFI DUMMY EXTERNAL)

# A callback of `LIBFFI` is a closure of libffi, which writes code into memory where the platform has no trampolines of its own, so a platform that forbids executable memory builds without them and `ffi.cast` of a Lua function fails with a sentence that says why.
option(VARN_FFI_CLOSURES "Let the ffi module make callbacks of Lua functions, whose closures write code into memory" ON)

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
list(APPEND VARN_CORE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/FfiModule.cpp")

if(VARN_FFI_DRIVER STREQUAL "LIBFFI")
    set(VARN_NEEDS_LIBFFI ON)
elseif(VARN_FFI_DRIVER STREQUAL "EXTERNAL")
    varn_add_external_driver(FFI VARN_CORE_SOURCES)
else()
    list(APPEND VARN_CORE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/dummy/DummyFfiModule.cpp")
endif()

varn_register_driver(FFI "${VARN_FFI_DRIVER}")
