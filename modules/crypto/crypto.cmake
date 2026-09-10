# crypto provides digests, hmac, and random bytes from a primitives driver

if(NOT DEFINED CACHE{VARN_CRYPTO_DRIVER})
    if(VARN_ENABLE_TLS)
        set(VARN_CRYPTO_DRIVER "OPENSSL" CACHE STRING "crypto primitives backend: OPENSSL DUMMY")
    else()
        set(VARN_CRYPTO_DRIVER "DUMMY" CACHE STRING "crypto primitives backend: OPENSSL DUMMY")
    endif()
endif()
set_property(CACHE VARN_CRYPTO_DRIVER PROPERTY STRINGS OPENSSL PORTABLE DUMMY)
varn_validate_driver(VARN_CRYPTO_DRIVER OPENSSL PORTABLE DUMMY)

if(VARN_ENABLE_TLS AND NOT VARN_CRYPTO_DRIVER STREQUAL "OPENSSL")
    message(FATAL_ERROR "VARN_ENABLE_TLS=ON requires VARN_CRYPTO_DRIVER=OPENSSL.")
endif()

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
list(APPEND VARN_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/CryptoModule.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/CryptoCodecs.cpp"
)

if(VARN_CRYPTO_DRIVER STREQUAL "OPENSSL")
    list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/openssl/OpenSslCryptoPrimitives.cpp")
    set(VARN_NEEDS_OPENSSL ON)
elseif(VARN_CRYPTO_DRIVER STREQUAL "PORTABLE")
    list(APPEND VARN_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/portable/PortableCryptoPrimitives.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/portable/Sha.cpp"
    )
else()
    list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/dummy/DummyCryptoPrimitives.cpp")
endif()

varn_register_driver(CRYPTO "${VARN_CRYPTO_DRIVER}")
