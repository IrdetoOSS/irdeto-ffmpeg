CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

PROJECT(irdeto-aom)

include(GNUInstallDirs)

FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION IR_VERSION)
FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/RELEASE IR_RELEASE)

SET(IR_PROJECT_VERSION "${IR_VERSION}")
SET(IR_PROJECT_DIR     "${CMAKE_CURRENT_SOURCE_DIR}")
SET(IR_PROJECT_PATCH   "${CMAKE_CURRENT_SOURCE_DIR}/irdeto-aom.patch")

SET(IR_LIBDIR   ${CMAKE_INSTALL_LIBDIR}/iraom)
SET(IR_BINDIR   ${CMAKE_INSTALL_BINDIR}/iraom)
SET(IR_INCDIR   ${CMAKE_INSTALL_INCLUDEDIR}/iraom)
SET(IR_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

set(LIBAOM_DESTDIR ./)
include(ExternalProject)

ExternalProject_Add(
        LIBAOM
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sources
        CMAKE_ARGS -DBUILD_SHARED_LIBS=0
                   -DCONFIG_AV1_DECODER=1
                   -DCONFIG_AV1_ENCODER=1
                   -DCONFIG_RUNTIME_CPU_DETECT=1
                   -DENABLE_EXAMPLES=0
                   -DENABLE_TESTS=0
                   -DENABLE_TOOLS=0
                   -DENABLE_DOCS=0
                   -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
        PATCH_COMMAND   patch -p0 --forward -i "${IR_PROJECT_PATCH}" -d "${CMAKE_CURRENT_SOURCE_DIR}/sources" || true
        INSTALL_COMMAND make install DESTDIR=${LIBAOM_DESTDIR}
        LOG_CONFIGURE   on
        LOG_BUILD       on
        LOG_INSTALL     on
        BUILD_ALWAYS    off
        BUILD_IN_SOURCE off
)

ExternalProject_Get_Property(LIBAOM BINARY_DIR)
set(LIBAOM_CMAKE_LIB_DIR ${BINARY_DIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})
set(LIBAOM_CMAKE_INC_DIR ${BINARY_DIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR})

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
install(FILES     ${LIBAOM_CMAKE_LIB_DIR}/libaom.a                                 DESTINATION ${IR_LIBDIR})
install(FILES     ${LIBAOM_CMAKE_LIB_DIR}/pkgconfig/aom.pc                         DESTINATION ${IR_LIBDIR}/pkgconfig)
install(DIRECTORY ${LIBAOM_CMAKE_INC_DIR}/aom/                                     DESTINATION ${IR_INCDIR} USE_SOURCE_PERMISSIONS)
