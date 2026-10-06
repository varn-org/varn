# Makes a package of the OpenSSL that a build made, in the folder `PACKAGE`: the public headers of every folder of `INCLUDE_DIRS` in `include/openssl`, the libraries of `LIBRARIES` in `lib/`, the file `INPUTS` that lists what the package depends on as `inputs.txt`, and `varn-package.cmake`, which names its version and its libraries.
# The package appears whole or not at all, since it takes its place by one rename, which fails when another build put a package there first, and then that package stays.

if(EXISTS "${PACKAGE}/varn-package.cmake")
    return()
endif()

string(RANDOM LENGTH 12 suffix)
set(staging "${PACKAGE}.${suffix}")
file(REMOVE_RECURSE "${staging}")
file(MAKE_DIRECTORY "${staging}/include/openssl" "${staging}/lib")

foreach(include_dir IN LISTS INCLUDE_DIRS)
    file(GLOB headers "${include_dir}/openssl/*.h")
    file(COPY ${headers} DESTINATION "${staging}/include/openssl")
endforeach()
file(COPY ${LIBRARIES} DESTINATION "${staging}/lib")

list(GET LIBRARIES 0 crypto)
list(GET LIBRARIES 1 ssl)
cmake_path(GET crypto FILENAME crypto)
cmake_path(GET ssl FILENAME ssl)
file(STRINGS "${staging}/include/openssl/opensslv.h" version REGEX "^# *define +OPENSSL_FULL_VERSION_STR +\"")
string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1" version "${version}")
file(COPY_FILE "${INPUTS}" "${staging}/inputs.txt")
file(WRITE "${staging}/varn-package.cmake" "set(VARN_OPENSSL_VERSION \"${version}\")\nset(VARN_OPENSSL_CRYPTO_FILE \"${crypto}\")\nset(VARN_OPENSSL_SSL_FILE \"${ssl}\")\n")

file(RENAME "${staging}" "${PACKAGE}" RESULT renamed)
if(NOT renamed STREQUAL "0")
    file(REMOVE_RECURSE "${staging}")
    if(NOT EXISTS "${PACKAGE}/varn-package.cmake")
        message(FATAL_ERROR "The package of OpenSSL could not take its place in \"${PACKAGE}\": ${renamed}")
    endif()
endif()
