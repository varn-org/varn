# The native unit tests are plain googletest, linked against the static core, from an archive checked against its SHA-256.

CPMAddPackage(
    NAME googletest
    VERSION 1.15.2
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926
    OPTIONS
        "INSTALL_GTEST OFF"
        "gtest_force_shared_crt ON"
)

# The framework is built with the sanitizers of the tests, since the standard library of Apple marks its containers for the address sanitizer and a container that code without it grows breaks those marks.
varn_apply_sanitize(gtest PRIVATE)
varn_apply_sanitize(gtest_main PRIVATE)

file(GLOB VARN_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

add_executable(varn_tests ${VARN_TEST_SOURCES})

# The `varn_core` target publishes its include dirs and defines.
# The module src dirs let the tests reach internal helpers (HTTP token/form/range, zip path safety) without widening any public API.
target_link_libraries(varn_tests PRIVATE varn_core GTest::gtest_main)
target_include_directories(varn_tests PRIVATE
    "${VARN_ROOT}/modules/http/src"
    "${VARN_ROOT}/modules/zip/src"
)

if(MSVC)
    # Match the 8 MB main-thread stack the CLI uses, so recursive code under test has the same headroom.
    target_link_options(varn_tests PRIVATE /STACK:8388608)
endif()

add_test(NAME varn_tests COMMAND varn_tests)
