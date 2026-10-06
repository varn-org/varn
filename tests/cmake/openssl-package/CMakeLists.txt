# Checks that a prebuilt package of OpenSSL answers `find_package(OpenSSL)` itself, as a build of OpenSSL does, and that no OpenSSL of the system answers in its place, such as the one that the Windows runners keep under Program Files.
#
#   cmake -S tests/cmake/openssl-package -B build/openssl-package -G Ninja -DVARN_OPENSSL_PACKAGE=<folder of the package>
cmake_minimum_required(VERSION 3.26)
project(varn-openssl-package-test C)

file(TO_CMAKE_PATH "${VARN_OPENSSL_PACKAGE}" package)
if(NOT EXISTS "${package}/varn-package.cmake")
    message(FATAL_ERROR "Name the folder of a package of OpenSSL with \"VARN_OPENSSL_PACKAGE\", such as one in the folder that \"VARN_PACKAGE_CACHE\" names.")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/helpers.cmake")
varn_import_openssl_package("${package}")
include("${package}/varn-package.cmake")

find_package(OpenSSL 1.1.1 REQUIRED)
if(NOT OpenSSL_DIR STREQUAL CMAKE_FIND_PACKAGE_REDIRECTS_DIR)
    message(FATAL_ERROR "The package \"${package}\" did not answer \"find_package(OpenSSL)\", which the module \"FindOpenSSL\" answered instead.")
endif()
if(NOT OPENSSL_CRYPTO_LIBRARY STREQUAL "${package}/lib/${VARN_OPENSSL_CRYPTO_FILE}" OR NOT OPENSSL_SSL_LIBRARY STREQUAL "${package}/lib/${VARN_OPENSSL_SSL_FILE}")
    message(FATAL_ERROR "The libraries of OpenSSL are \"${OPENSSL_CRYPTO_LIBRARY}\" and \"${OPENSSL_SSL_LIBRARY}\" instead of those of the package \"${package}\".")
endif()
message(STATUS "The package \"${package}\" answers \"find_package(OpenSSL)\" with OpenSSL ${VARN_OPENSSL_VERSION}.")
