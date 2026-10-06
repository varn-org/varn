# The package manager CPM comes from its release, checked against its SHA-256, and a copy already in place that does not match is downloaded again.
# Reference: https://github.com/cpm-cmake/CPM.cmake

set(CPM_DOWNLOAD_VERSION 0.42.1)
set(CPM_DOWNLOAD_HASH f3a6dcc6a04ce9e7f51a127307fa4f699fb2bade357a8eb4c5b45df76e1dc6a5)

if(CPM_SOURCE_CACHE)
    set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif(DEFINED ENV{CPM_SOURCE_CACHE})
    set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else()
    set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif()

# A path given with a tilde is expanded before it is read.
get_filename_component(CPM_DOWNLOAD_LOCATION ${CPM_DOWNLOAD_LOCATION} ABSOLUTE)

set(_varn_cpm_digest "")
if(EXISTS ${CPM_DOWNLOAD_LOCATION})
    file(SHA256 ${CPM_DOWNLOAD_LOCATION} _varn_cpm_digest)
endif()

if(NOT _varn_cpm_digest STREQUAL CPM_DOWNLOAD_HASH)
    message(STATUS "Downloading \"CPM.cmake\" to \"${CPM_DOWNLOAD_LOCATION}\".")
    file(DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
        ${CPM_DOWNLOAD_LOCATION}
        EXPECTED_HASH SHA256=${CPM_DOWNLOAD_HASH}
    )
endif()

include(${CPM_DOWNLOAD_LOCATION})
