# desktop installers for the cli, each using the generator of its platform and leaving varn callable from a terminal without the user editing anything

set(CPACK_PACKAGE_NAME "varn")
set(CPACK_PACKAGE_VENDOR "Paulo Coutinho")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Lua everywhere: one codebase on desktop, mobile and the browser")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://varn.pages.dev")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "varn")
# the installer generators accept a plain text licence and refuse markdown, so the shipped one is copied under a name they take
configure_file("${VARN_ROOT}/LICENSE.md" "${CMAKE_BINARY_DIR}/LICENSE.txt" COPYONLY)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_BINARY_DIR}/LICENSE.txt")
set(CPACK_PACKAGE_CONTACT "paulocoutinhox@gmail.com")
set(CPACK_STRIP_FILES ON)

# only the executable belongs in an installer, so the embeddable library component stays out of it
set(CPACK_COMPONENTS_ALL cli)
set(CPACK_COMPONENT_CLI_DISPLAY_NAME "Varn")
set(CPACK_COMPONENT_CLI_DESCRIPTION "The varn executable")

if(APPLE)
    # a disk image only copies a bundle across and has no install step, so it cannot put anything on the PATH; a product archive installs under a prefix that already is on it
    set(CPACK_GENERATOR productbuild)
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local")
    set(CPACK_PRODUCTBUILD_IDENTIFIER "com.varn.cli")
    set(CPACK_PRODUCTBUILD_DOMAINS ON)
    set(CPACK_PRODUCTBUILD_DOMAINS_ANYWHERE OFF)
    set(CPACK_PRODUCTBUILD_DOMAINS_USER OFF)
    set(CPACK_PRODUCTBUILD_DOMAINS_ROOT ON)
elseif(WIN32)
    # the installer writes the machine PATH itself, since windows has no directory that is on it by default
    set(CPACK_GENERATOR NSIS)
    set(CPACK_NSIS_PACKAGE_NAME "Varn")
    set(CPACK_NSIS_DISPLAY_NAME "Varn ${PROJECT_VERSION}")
    set(CPACK_NSIS_URL_INFO_ABOUT "${CPACK_PACKAGE_HOMEPAGE_URL}")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY "bin")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "")
else()
    # a debian package installs under /usr, whose bin directory every shell already searches
    set(CPACK_GENERATOR DEB)
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_VENDOR} <${CPACK_PACKAGE_CONTACT}>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
endif()

include(CPack)

cpack_add_component(cli
    DISPLAY_NAME "${CPACK_COMPONENT_CLI_DISPLAY_NAME}"
    DESCRIPTION "${CPACK_COMPONENT_CLI_DESCRIPTION}"
    REQUIRED
)
