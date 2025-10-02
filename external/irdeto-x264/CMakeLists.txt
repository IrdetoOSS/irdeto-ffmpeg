CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

ENABLE_TESTING()

PROJECT(irdeto-x264)

find_package(irdeto-xps-export-devel CONFIG QUIET HINTS ../irdeto-xps-export/sourcetree)

include(GNUInstallDirs)

FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION IR_VERSION)
FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/RELEASE IR_RELEASE)

SET(IR_PROJECT_VERSION 	"${IR_VERSION}-${IR_RELEASE}")
SET(IR_PROJECT_DIR      "${CMAKE_CURRENT_SOURCE_DIR}")
SET(IR_PROJECT_PATCH    "${CMAKE_CURRENT_SOURCE_DIR}/irdeto-x264.patch")

SET(IR_LIBDIR       ${CMAKE_INSTALL_LIBDIR}/irx264)
SET(IR_BINDIR       ${CMAKE_INSTALL_BINDIR}/irx264)
SET(IR_INCDIR       ${CMAKE_INSTALL_INCLUDEDIR}/irx264)
SET(IR_CMAKEDIR     ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

find_program(YASM_EXECUTABLE NAMES yasm)

include(ProcessorCount)
ProcessorCount(PROCN)

include(ExternalProject)
set(X264_INSTALL_DIR $<$<NOT:$<CONFIG:Debug>>:.>/)
ExternalProject_Add(
        X264
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/source
        PATCH_COMMAND patch -p0 --forward -i "${IR_PROJECT_PATCH}" -d "${CMAKE_CURRENT_SOURCE_DIR}/source" || true
        CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/source/configure
        --enable-shared
        --disable-opencl
        --disable-gpl
        --disable-lavf
        --disable-avs
        $<$<CONFIG:Debug>:--prefix=${CMAKE_CURRENT_BINARY_DIR}/X264-prefix/src/X264-build/${CMAKE_INSTALL_PREFIX}>
        $<$<NOT:$<CONFIG:Debug>>:--prefix=${CMAKE_INSTALL_PREFIX}>
        --libdir=\\\${prefix}/${IR_LIBDIR}/
        --includedir=\\\${prefix}/${IR_INCDIR}/
        --bindir=\\\${prefix}/${IR_BINDIR}/
        --extra-cflags=-Wl,--no-undefined
        --extra-cflags=-isystem${irdeto-xps-export-devel_DIR}/../${CMAKE_INSTALL_INCLUDEDIR}/
        --enable-debug
        BUILD_COMMAND make -j${PROCN} VPATH=${CMAKE_CURRENT_SOURCE_DIR}/source --debug
        INSTALL_COMMAND make install VPATH=${CMAKE_CURRENT_SOURCE_DIR}/source DESTDIR=${X264_INSTALL_DIR}
        LOG_CONFIGURE on
        LOG_BUILD on
        LOG_INSTALL on
        BUILD_ALWAYS on
        BUILD_IN_SOURCE off
)
ExternalProject_Get_Property(X264 BINARY_DIR)
# # GNU make v.3.82 has an issue. http://savannah.gnu.org/bugs/?39934
# # The following is a workaround for this
# find_program(PATCH_EXECUTABLE NAMES patch)
#ExternalProject_Add_Step(
#        X264 PatchMakefile
#        COMMAND rm ${BINARY_DIR}/Makefile
#        COMMAND cp -pft ${BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/source/Makefile
#        COMMAND patch -i ${IR_PROJECT_PATCH} -o ${BINARY_DIR}/Makefile
#        DEPENDEES configure
#        DEPENDERS build
#        ALWAYS 1
#        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/source
#        LOG on
#)

########################################################################################################################
install(DIRECTORY ${BINARY_DIR}/${X264_INSTALL_DIR}/${CMAKE_INSTALL_PREFIX}/ DESTINATION ${CMAKE_INSTALL_PREFIX} USE_SOURCE_PERMISSIONS)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/project-config.cmake.in
        "
CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
@PACKAGE_INIT@

set(ENV{PKG_CONFIG_PATH} ${CMAKE_INSTALL_PREFIX}/${IR_LIBDIR}/pkgconfig/)

find_package(PkgConfig REQUIRED)
pkg_check_modules(X264 REQUIRED x264)

add_library(irx264 INTERFACE)
target_link_libraries(irx264 INTERFACE \"\${X264_LDFLAGS} -Wl,-rpath,${CMAKE_INSTALL_PREFIX}/${IR_LIBDIR}\" )
target_include_directories(irx264 INTERFACE \${X264_INCLUDE_DIRS} ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR})
target_compile_options(irx264 INTERFACE \${X264_CFLAGS} )

check_required_components(${PROJECT_NAME})
")

include ( CMakePackageConfigHelpers )
configure_package_config_file (
        ${CMAKE_CURRENT_BINARY_DIR}/project-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
        INSTALL_DESTINATION ${IR_CMAKEDIR}
        PATH_VARS
)

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
        VERSION ${IR_VERSION}
        COMPATIBILITY SameMajorVersion )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake DESTINATION ${IR_CMAKEDIR})

set(X264_LIB_DIR ${BINARY_DIR}/${X264_INSTALL_DIR}/${CMAKE_INSTALL_PREFIX}/${IR_LIBDIR})
set(X264_INCLUDE_DIRECTORY ${BINARY_DIR}/${X264_INSTALL_DIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR})

add_library(irx264 INTERFACE)
target_link_libraries(irx264 INTERFACE ${X264_LIB_DIR}/libx264.so)
target_include_directories(irx264 INTERFACE ${X264_INCLUDE_DIRECTORY})
add_dependencies(irx264 X264)

