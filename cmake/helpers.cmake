include_guard(GLOBAL)

# normalizes a driver cache value to upper case and fails when it is outside the allowed set
function(varn_validate_driver var_name)
    set(allowed ${ARGN})
    string(TOUPPER "${${var_name}}" value)
    set(${var_name} "${value}" CACHE STRING "" FORCE)

    if(NOT value IN_LIST allowed)
        message(FATAL_ERROR "${var_name} must be one of: ${allowed} (got '${value}')")
    endif()
endfunction()

# publishes the selected backend so sources can branch on it with VARN_<MODULE>_DRIVER_<NAME>
function(varn_register_driver module_token driver_value)
    string(TOUPPER "${driver_value}" driver_upper)
    list(APPEND VARN_COMPILE_DEFS
        "VARN_${module_token}_DRIVER_${driver_upper}=1"
        "VARN_${module_token}_DRIVER_NAME=\"${driver_value}\"")
    set(VARN_COMPILE_DEFS "${VARN_COMPILE_DEFS}" PARENT_SCOPE)
endfunction()

# applies the include dirs, driver defines, feature flags and dependency links every varn artifact shares
function(varn_apply_usage target scope)
    target_include_directories(${target} ${scope}
        ${VARN_INCLUDE_DIRS}
        "${VARN_ROOT}/include"
        "${VARN_GENERATED_INCLUDE_DIR}"
    )

    target_compile_definitions(${target} ${scope} ${VARN_COMPILE_DEFS})
    if(VARN_ENABLE_STATIC_FILES)
        target_compile_definitions(${target} ${scope} VARN_ENABLE_STATIC_FILES=1)
    endif()
    if(VARN_ENABLE_TLS)
        target_compile_definitions(${target} ${scope} VARN_ENABLE_TLS=1)
    endif()

    if(VARN_NO_SENDFILE)
        target_compile_definitions(${target} ${scope} VARN_NO_SENDFILE=1)
    endif()

    target_link_libraries(${target} ${scope} varn_vendor_lua)
    if(NOT VARN_BUILDING_FOR_EMSCRIPTEN)
        target_link_libraries(${target} ${scope} Threads::Threads)
    endif()

    # an apple app has no trust store on disk, so the ca bundle is looked up in the application bundle through core foundation
    if(APPLE)
        target_link_libraries(${target} ${scope} "-framework CoreFoundation")
    endif()

    if(VARN_NEEDS_LIBUV)
        target_link_libraries(${target} ${scope} uv_a)
    endif()
    if(VARN_NEEDS_LLHTTP)
        target_link_libraries(${target} ${scope} llhttp_static)
    endif()
    if(VARN_NEEDS_SPDLOG)
        target_link_libraries(${target} ${scope} spdlog::spdlog_header_only)
    endif()
    if(VARN_NEEDS_NLOHMANN)
        target_link_libraries(${target} ${scope} nlohmann_json::nlohmann_json)
    endif()
    if(VARN_NEEDS_PUGIXML)
        target_link_libraries(${target} ${scope} pugixml::pugixml)
    endif()
    if(VARN_NEEDS_DATE)
        target_link_libraries(${target} ${scope} date::date)
    endif()
    if(VARN_NEEDS_LIBFFI)
        target_link_libraries(${target} ${scope} varn_ffi_vendor)
    endif()

    if(VARN_NEEDS_ZIP)
        target_compile_definitions(${target} ${scope} VARN_HAVE_LIBZIP=1)
        if(TARGET libzip::zip)
            target_link_libraries(${target} ${scope} libzip::zip ZLIB::ZLIB)
        elseif(TARGET zip)
            target_link_libraries(${target} ${scope} zip ZLIB::ZLIB)
        else()
            message(FATAL_ERROR "libzip target not found after CPM (expected libzip::zip or zip)")
        endif()
    endif()

    # zlib backs http response compression independently of the zip module
    if(VARN_NEEDS_ZLIB)
        target_compile_definitions(${target} ${scope} VARN_HAVE_ZLIB=1)
        target_link_libraries(${target} ${scope} ZLIB::ZLIB)
    endif()

    # openssl is linked directly only when poco does not already bring it in for tls
    if(VARN_ENABLE_TLS)
        if(NOT VARN_NEEDS_POCO)
            target_link_libraries(${target} ${scope} OpenSSL::SSL OpenSSL::Crypto)
        endif()
    elseif(VARN_CRYPTO_DRIVER STREQUAL "OPENSSL")
        target_link_libraries(${target} ${scope} OpenSSL::Crypto)
    endif()

    # the portable crypto driver draws randomness from bcrypt on windows
    if(WIN32 AND VARN_CRYPTO_DRIVER STREQUAL "PORTABLE")
        target_link_libraries(${target} ${scope} bcrypt)
    endif()

    if(VARN_NEEDS_POCO)
        if(VARN_ENABLE_TLS)
            if(WIN32)
                target_link_libraries(${target} ${scope} Poco::NetSSLWin Poco::Crypto)
            else()
                target_link_libraries(${target} ${scope} Poco::NetSSL)
            endif()
        else()
            target_link_libraries(${target} ${scope} Poco::Net Poco::Util)
        endif()
    endif()
endfunction()

# builds a target with the requested sanitizers so the test suite can detect memory and ub bugs
function(varn_apply_sanitize target scope)
    if(VARN_SANITIZE STREQUAL "" OR MSVC)
        return()
    endif()
    target_compile_options(${target} ${scope} -fsanitize=${VARN_SANITIZE} -fno-omit-frame-pointer -g)
    target_link_options(${target} ${scope} -fsanitize=${VARN_SANITIZE})
endfunction()

# applies the project warning flags to a target that compiles varn's own sources
function(varn_apply_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(VARN_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(VARN_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
