CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

PROJECT(irdeto-srt)

include(GNUInstallDirs)

FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION IR_VERSION)
FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/RELEASE IR_RELEASE)

SET(IR_PROJECT_VERSION "${IR_VERSION}")
SET(IR_PROJECT_DIR     "${CMAKE_CURRENT_SOURCE_DIR}")
SET(IR_PROJECT_PATCH   "${CMAKE_CURRENT_SOURCE_DIR}/irdeto-srt.patch")

SET(IR_LIBDIR   ${CMAKE_INSTALL_LIBDIR}/irsrt)
SET(IR_BINDIR   ${CMAKE_INSTALL_BINDIR}/irsrt)
SET(IR_INCDIR   ${CMAKE_INSTALL_INCLUDEDIR}/irsrt)
SET(IR_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

set(LIBSRT_DESTDIR ./)
include(ExternalProject)

ExternalProject_Add(
        LIBSRT
        SOURCE_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/sources
        PATCH_COMMAND     patch -p0 --forward -i "${IR_PROJECT_PATCH}" -d "${CMAKE_CURRENT_SOURCE_DIR}/sources" || true
        CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/sources/configure
                          --disable-apps
                          --disable-c++11
                          --enable-debug=0
                          --disable-shared
                          --enable-static
                          --prefix=${CMAKE_INSTALL_PREFIX}
        BUILD_COMMAND     make VPATH=${CMAKE_CURRENT_SOURCE_DIR}/sources
        INSTALL_COMMAND   make install VPATH=${CMAKE_CURRENT_SOURCE_DIR}/sources DESTDIR=${LIBSRT_DESTDIR}
        LOG_CONFIGURE     on
        LOG_BUILD         on
        LOG_INSTALL       on
        BUILD_ALWAYS      off
        BUILD_IN_SOURCE   off
)

ExternalProject_Get_Property(LIBSRT BINARY_DIR)
set(LIBSRT_CMAKE_LIB_DIR ${BINARY_DIR}/${LIBSRT_DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})
set(LIBSRT_CMAKE_INC_DIR ${BINARY_DIR}/${LIBSRT_DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR})

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/project-config.cmake.in
"
CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
@PACKAGE_INIT@

set(ENV{PKG_CONFIG_PATH} ${CMAKE_INSTALL_PREFIX}/${IR_LIBDIR}/pkgconfig/)

check_required_components(${PROJECT_NAME})
")

include(CMakePackageConfigHelpers)

configure_package_config_file(
        ${CMAKE_CURRENT_BINARY_DIR}/project-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
        INSTALL_DESTINATION ${IR_LIBDIR}/cmake/${PROJECT_NAME}
        PATH_VARS
)

write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
        VERSION ${IR_VERSION}
        COMPATIBILITY SameMajorVersion
)

install(FILES     ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake         DESTINATION ${IR_CMAKEDIR})
install(FILES     ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake DESTINATION ${IR_CMAKEDIR})
install(FILES     ${LIBSRT_CMAKE_LIB_DIR}/libsrt.a                                 DESTINATION ${IR_LIBDIR})
install(FILES     ${LIBSRT_CMAKE_LIB_DIR}/pkgconfig/srt.pc                         DESTINATION ${IR_LIBDIR}/pkgconfig)
install(DIRECTORY ${LIBSRT_CMAKE_INC_DIR}/srt/                                     DESTINATION ${IR_INCDIR} USE_SOURCE_PERMISSIONS)
