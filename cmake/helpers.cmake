include_guard(GLOBAL)

# Declares the cache option that selects the driver of a module, with its default and the drivers it allows, which the CMake interfaces offer, and fails on any other value, written in any case.
function(varn_driver_option var_name default_value description)
    set(allowed ${ARGN})
    if(NOT DEFINED CACHE{${var_name}})
        set(${var_name} "${default_value}" CACHE STRING "${description}")
    endif()
    set_property(CACHE ${var_name} PROPERTY STRINGS ${allowed})

    string(TOUPPER "${${var_name}}" value)
    if(NOT value IN_LIST allowed)
        message(FATAL_ERROR "The option \"${var_name}\" must be one of \"${allowed}\" (got \"${${var_name}}\").")
    endif()
    set_property(CACHE ${var_name} PROPERTY VALUE "${value}")
    set(${var_name} "${value}" PARENT_SCOPE)
endfunction()

# Adds the sources of a driver that the host supplies, `VARN_<MODULE>_DRIVER_SOURCES`, which implement the public header of the module that every driver implements, to the source list that `sources_var` names, `VARN_RUNTIME_SOURCES` for a driver of the runtime without Lua and `VARN_CORE_SOURCES` for a driver of a Lua module, with the libraries they link, `VARN_<MODULE>_DRIVER_LIBRARIES`.
function(varn_add_external_driver module_token sources_var)
    set(VARN_${module_token}_DRIVER_SOURCES "" CACHE STRING "Sources of the external ${module_token} driver")
    set(VARN_${module_token}_DRIVER_LIBRARIES "" CACHE STRING "Libraries of the external ${module_token} driver")
    if(VARN_${module_token}_DRIVER_SOURCES STREQUAL "")
        message(FATAL_ERROR "The driver \"EXTERNAL\" of \"VARN_${module_token}_DRIVER\" needs its sources in \"VARN_${module_token}_DRIVER_SOURCES\".")
    endif()
    list(APPEND ${sources_var} ${VARN_${module_token}_DRIVER_SOURCES})
    list(APPEND VARN_EXTERNAL_LIBRARIES ${VARN_${module_token}_DRIVER_LIBRARIES})
    set(${sources_var} "${${sources_var}}" PARENT_SCOPE)
    set(VARN_EXTERNAL_LIBRARIES "${VARN_EXTERNAL_LIBRARIES}" PARENT_SCOPE)
endfunction()

# Publishes the selected backend so sources can branch on it with `VARN_<MODULE>_DRIVER_<NAME>`.
function(varn_register_driver module_token driver_value)
    string(TOUPPER "${driver_value}" driver_upper)
    list(APPEND VARN_COMPILE_DEFS
        "VARN_${module_token}_DRIVER_${driver_upper}=1"
        "VARN_${module_token}_DRIVER_NAME=\"${driver_value}\"")
    set(VARN_COMPILE_DEFS "${VARN_COMPILE_DEFS}" PARENT_SCOPE)
endfunction()

# Applies what the runtime without Lua needs, the include dirs, driver defines and feature flags every Varn artifact shares and the libraries of the scheduler, the log, data views, the HTTP client and TLS, and never the Lua interpreter, so no source of the runtime reaches a header of Lua.
function(varn_apply_runtime_usage target scope)
    target_include_directories(${target} ${scope}
        ${VARN_INCLUDE_DIRS}
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

    target_link_libraries(${target} ${scope} ${VARN_EXTERNAL_LIBRARIES})
    if(NOT VARN_BUILDING_FOR_EMSCRIPTEN)
        target_link_libraries(${target} ${scope} Threads::Threads)
    endif()

    # An Apple app has no trust store on disk, so the CA bundle is looked up in the application bundle through Core Foundation.
    if(APPLE)
        target_link_libraries(${target} ${scope} "-framework CoreFoundation")
    endif()

    # The Apple HTTP client and the Objective-C class of data views are built on Foundation.
    if(VARN_NEEDS_FOUNDATION)
        target_link_libraries(${target} ${scope} "-framework Foundation")
    endif()

    if(VARN_NEEDS_LIBUV)
        target_link_libraries(${target} ${scope} uv_a)
    endif()
    if(VARN_NEEDS_SPDLOG)
        target_link_libraries(${target} ${scope} spdlog::spdlog_header_only)
    endif()
    if(VARN_NEEDS_NLOHMANN)
        target_link_libraries(${target} ${scope} nlohmann_json::nlohmann_json)
    endif()

    # TLS runs OpenSSL on every platform, and on Windows it reads the trusted roots from the system store.
    if(VARN_ENABLE_TLS)
        target_link_libraries(${target} ${scope} OpenSSL::SSL OpenSSL::Crypto)
        if(WIN32)
            target_link_libraries(${target} ${scope} crypt32)
        endif()
    endif()

    if(VARN_NEEDS_POCO)
        if(VARN_ENABLE_TLS)
            target_link_libraries(${target} ${scope} Poco::NetSSL)
        else()
            target_link_libraries(${target} ${scope} Poco::Net Poco::Util)
        endif()
    endif()
endfunction()

# Applies what the Lua runtime and its modules need on top of the runtime without Lua: the Lua interpreter with the header "lua.hpp" and the libraries that only the modules link.
function(varn_apply_core_usage target scope)
    target_include_directories(${target} ${scope} "${VARN_ROOT}/include")
    target_link_libraries(${target} ${scope} varn-vendor-lua)

    if(VARN_NEEDS_LLHTTP)
        target_link_libraries(${target} ${scope} llhttp_static)
    endif()
    if(VARN_NEEDS_PUGIXML)
        target_link_libraries(${target} ${scope} pugixml::pugixml)
    endif()
    if(VARN_NEEDS_DATE)
        target_link_libraries(${target} ${scope} date::date)
    endif()
    if(VARN_NEEDS_LIBFFI)
        target_link_libraries(${target} ${scope} varn-ffi-vendor)
    endif()

    if(VARN_NEEDS_ZIP)
        target_compile_definitions(${target} ${scope} VARN_HAVE_LIBZIP=1)
        if(TARGET libzip::zip)
            target_link_libraries(${target} ${scope} libzip::zip ZLIB::ZLIB)
        elseif(TARGET zip)
            target_link_libraries(${target} ${scope} zip ZLIB::ZLIB)
        else()
            message(FATAL_ERROR "The libzip target was not found after CPM (expected \"libzip::zip\" or \"zip\").")
        endif()
    endif()

    # The zlib library backs HTTP response compression independently of the zip module.
    if(VARN_NEEDS_ZLIB)
        target_compile_definitions(${target} ${scope} VARN_HAVE_ZLIB=1)
        target_link_libraries(${target} ${scope} ZLIB::ZLIB)
    endif()

    # The crypto driver of OpenSSL links its library even where TLS is off.
    if(NOT VARN_ENABLE_TLS AND VARN_CRYPTO_DRIVER STREQUAL "OPENSSL")
        target_link_libraries(${target} ${scope} OpenSSL::Crypto)
    endif()

    # The portable crypto driver draws randomness from `bcrypt` on Windows.
    if(WIN32 AND VARN_CRYPTO_DRIVER STREQUAL "PORTABLE")
        target_link_libraries(${target} ${scope} bcrypt)
    endif()
endfunction()

# Adds the two static libraries of the core: `varn-runtime`, the scheduler, the log, data views, the HTTP client and TLS without Lua, and `varn-core`, the Lua runtime and every module on top of it, so a host that links only `varn-runtime` links no Lua.
function(varn_add_static_libraries)
    add_library(varn-runtime STATIC ${VARN_RUNTIME_SOURCES})
    set_target_properties(varn-runtime PROPERTIES POSITION_INDEPENDENT_CODE ON)
    varn_apply_runtime_usage(varn-runtime PUBLIC)
    varn_apply_warnings(varn-runtime)

    add_library(varn-core STATIC ${VARN_CORE_SOURCES})
    set_target_properties(varn-core PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(varn-core PUBLIC varn-runtime)
    varn_apply_core_usage(varn-core PUBLIC)
    varn_apply_warnings(varn-core)
endfunction()

# Builds a target with the requested sanitizers so the test suite can detect memory and undefined-behavior bugs.
function(varn_apply_sanitize target scope)
    if(VARN_SANITIZE STREQUAL "" OR MSVC)
        return()
    endif()
    target_compile_options(${target} ${scope} -fsanitize=${VARN_SANITIZE} -fno-omit-frame-pointer -g)
    target_link_options(${target} ${scope} -fsanitize=${VARN_SANITIZE})
endfunction()

# Applies the project warning flags to a target that compiles the own sources of Varn.
function(varn_apply_warnings target)
    if(MSVC)
        # The CRT marks standard functions such as `std::getenv` deprecated in favour of variants only Microsoft ships, which portable code does not use.
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
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

# Takes OpenSSL from a package that `cmake/openssl-package.cmake` made, with the imported targets and the variables of `FindOpenSSL` that the recipe of OpenSSL defines when it builds one, so every target links the package as it links a build.
function(varn_import_openssl_package package)
    include("${package}/varn-package.cmake")
    set(include_dir "${package}/include")
    set(crypto "${package}/lib/${VARN_OPENSSL_CRYPTO_FILE}")
    set(ssl "${package}/lib/${VARN_OPENSSL_SSL_FILE}")

    set(dependencies "")
    set(link_dependencies "")
    if(WIN32)
        set(link_dependencies ws2_32 crypt32)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(Threads)
        if(Threads_FOUND)
            list(APPEND link_dependencies Threads::Threads)
            list(APPEND dependencies ${CMAKE_THREAD_LIBS_INIT})
        endif()
        list(APPEND link_dependencies ${CMAKE_DL_LIBS})
    endif()

    set(OPENSSL_FOUND ON CACHE BOOL "Override FindOpenSSL variables" FORCE)
    set(OPENSSL_INCLUDE_DIR "${include_dir}" CACHE STRING "Override FindOpenSSL variables" FORCE)
    set(OPENSSL_CRYPTO_LIBRARY "${crypto}" CACHE STRING "Override FindOpenSSL variables" FORCE)
    set(OPENSSL_CRYPTO_LIBRARIES "${crypto};${dependencies}" CACHE STRING "Override FindOpenSSL variables" FORCE)
    set(OPENSSL_SSL_LIBRARY "${ssl}" CACHE STRING "Override FindOpenSSL variables" FORCE)
    set(OPENSSL_SSL_LIBRARIES "${ssl};${dependencies}" CACHE STRING "Override FindOpenSSL variables" FORCE)
    set(OPENSSL_LIBRARIES "${crypto};${ssl};${dependencies}" CACHE STRING "Override FindOpenSSL variables" FORCE)
    set(OPENSSL_VERSION "${VARN_OPENSSL_VERSION}" CACHE STRING "Override FindOpenSSL variables" FORCE)

    add_library(OpenSSL::Crypto STATIC IMPORTED GLOBAL)
    set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES C
        IMPORTED_LOCATION "${crypto}"
        INTERFACE_INCLUDE_DIRECTORIES "${include_dir}"
        INTERFACE_LINK_LIBRARIES "${link_dependencies}"
    )
    add_library(OpenSSL::SSL STATIC IMPORTED GLOBAL)
    set_target_properties(OpenSSL::SSL PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES C
        IMPORTED_LOCATION "${ssl}"
        INTERFACE_INCLUDE_DIRECTORIES "${include_dir}"
        INTERFACE_LINK_LIBRARIES "${link_dependencies};OpenSSL::Crypto"
    )

    # A build of OpenSSL comes through FetchContent with `OVERRIDE_FIND_PACKAGE`, so `find_package(OpenSSL)` resolves to it, and the package resolves the same way. Without it the module `FindOpenSSL` runs, which under MSVC ignores the variables above and searches the system for an OpenSSL of its own.
    file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/openssl-config.cmake" "")
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file("${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/openssl-config-version.cmake" VERSION "${VARN_OPENSSL_VERSION}" COMPATIBILITY AnyNewerVersion)
endfunction()
