CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

ENABLE_TESTING()

PROJECT(irdeto-xps-export)

include(GNUInstallDirs)

FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION IR_VERSION)

SET(IR_PROJECT_VERSION  "${IR_VERSION}")
SET(IR_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

SET(IR_LIBDIR       ${CMAKE_INSTALL_LIBDIR})
SET(IR_BINDIR       ${CMAKE_INSTALL_BINDIR})
SET(IR_INCDIR       ${CMAKE_INSTALL_INCLUDEDIR})
SET(IR_CMAKEDIR     ${IR_LIBDIR}/cmake/${PROJECT_NAME})

ADD_SUBDIRECTORY(config)
ADD_SUBDIRECTORY(source)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/project-config.cmake.in
"
@PACKAGE_INIT@
include ( \"\${CMAKE_CURRENT_LIST_DIR}/${PROJECT_NAME}-targets.cmake\" )
check_required_components(${PROJECT_NAME})
")

include ( CMakePackageConfigHelpers )
configure_package_config_file (
        ${CMAKE_CURRENT_BINARY_DIR}/project-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
        INSTALL_DESTINATION ${IR_LIBDIR}/${PROJECT_NAME}/cmake
        PATH_VARS
)

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
        VERSION ${IR_VERSION}
        COMPATIBILITY SameMajorVersion )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake DESTINATION ${IR_CMAKEDIR})
