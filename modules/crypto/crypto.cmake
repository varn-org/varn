# The `crypto` module provides digests, HMAC, and random bytes from a primitives driver.

if(VARN_ENABLE_TLS)
    set(_varn_crypto_default OPENSSL)
else()
    set(_varn_crypto_default DUMMY)
endif()
varn_driver_option(VARN_CRYPTO_DRIVER ${_varn_crypto_default} "Crypto primitives backend" OPENSSL PORTABLE DUMMY EXTERNAL)

if(VARN_ENABLE_TLS AND NOT VARN_CRYPTO_DRIVER STREQUAL "OPENSSL")
    message(FATAL_ERROR "The option \"VARN_ENABLE_TLS=ON\" requires \"VARN_CRYPTO_DRIVER=OPENSSL\".")
endif()

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
list(APPEND VARN_CORE_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/CryptoModule.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/CryptoCodecs.cpp"
)

if(VARN_CRYPTO_DRIVER STREQUAL "OPENSSL")
    list(APPEND VARN_CORE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/openssl/OpenSslCryptoPrimitives.cpp")
    set(VARN_NEEDS_OPENSSL ON)
elseif(VARN_CRYPTO_DRIVER STREQUAL "PORTABLE")
    list(APPEND VARN_CORE_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/portable/PortableCryptoPrimitives.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/drivers/portable/Sha.cpp"
    )
elseif(VARN_CRYPTO_DRIVER STREQUAL "EXTERNAL")
    varn_add_external_driver(CRYPTO VARN_CORE_SOURCES)
else()
    list(APPEND VARN_CORE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/drivers/dummy/DummyCryptoPrimitives.cpp")
endif()

varn_register_driver(CRYPTO "${VARN_CRYPTO_DRIVER}")
