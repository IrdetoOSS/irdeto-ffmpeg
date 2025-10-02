cmake_minimum_required(VERSION 3.0)

ENABLE_TESTING()

project(irdeto-x265)

find_package(irdeto-xps-export-devel CONFIG QUIET HINTS ../irdeto-xps-export/sourcetree)

FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION IR_VERSION)
FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/RELEASE IR_RELEASE)

SET(IR_PROJECT_VERSION  "${IR_VERSION}-${IR_RELEASE}")
SET(IR_PROJECT_DIR      "${CMAKE_CURRENT_SOURCE_DIR}")
SET(IR_PROJECT_PATCH    "${CMAKE_CURRENT_SOURCE_DIR}/irdeto-x265.patch")

include(GNUInstallDirs)
SET(IR_LIBDIR       ${CMAKE_INSTALL_LIBDIR}/irx265)
SET(IR_BINDIR       ${CMAKE_INSTALL_BINDIR}/irx265)
SET(IR_INCDIR       ${CMAKE_INSTALL_INCLUDEDIR}/irx265)
SET(IR_CMAKEDIR     ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

find_package(OpenMP)

include(ProcessorCount)
ProcessorCount(PROCN)

include(multilib_10bit.cmake)
include(multilib_12bit.cmake)

set(X265_INSTALL_DIR $<$<NOT:$<CONFIG:Debug>>:.>/)
ExternalProject_Add(
        X265_8BIT
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sources/source
        PATCH_COMMAND patch -p0 --forward -i "${IR_PROJECT_PATCH}" -d "${CMAKE_CURRENT_SOURCE_DIR}/sources/source" || true
        INSTALL_COMMAND make install DESTDIR=${X265_INSTALL_DIR}
        BUILD_COMMAND make -j${PROCN}
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -DEXPORT_C_API=ON
        "-DCMAKE_CXX_FLAGS=${OpenMP_CXX_FLAGS} -I${irdeto-xps-export-devel_DIR}/../${CMAKE_INSTALL_INCLUDEDIR}/"
        "-DCMAKE_C_FLAGS=${OpenMP_C_FLAGS} -I${irdeto-xps-export-devel_DIR}/../${CMAKE_INSTALL_INCLUDEDIR}/"
        $<$<CONFIG:Debug>:-DCMAKE_INSTALL_PREFIX=./${CMAKE_INSTALL_PREFIX}>
        $<$<NOT:$<CONFIG:Debug>>:-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}>
        -DLIB_INSTALL_DIR=${IR_LIBDIR}
        -DINCLUDE_INSTALL_DIR=${IR_INCDIR}
        -DBIN_INSTALL_DIR=${IR_BINDIR}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        #We need to escape semicolon twice with slashes: First unescaping happens in this line, second one happens in externalProject wrapper;
        #Also we have to disable treating semicolon as list entry separator there so use generator_expression
        -DEXTRA_LIB_10BIT=${CMAKE_CURRENT_BINARY_DIR}/10bit/lib/libx265.a
        -DEXTRA_LIB_12BIT=${CMAKE_CURRENT_BINARY_DIR}/12bit/lib/libx265.a
        -DEXTRA_LINK_FLAGS=-Wl,-rpath=${CMAKE_INSTALL_PREFIX}/${IR_LIBDIR}
        -DLINKED_10BIT=ON
        -DLINKED_12BIT=ON
        -DENABLE_CLI=ON
        -DENABLE_SHARED=ON
        -DENABLE_LIBNUMA=OFF ${CMAKE_CURRENT_LIST_DIR}/sources/source
        LOG_BUILD on
        LOG_CONFIGURE on
        LOG_INSTALL on
        BUILD_ALWAYS off
        CONFIGURE_ALWAYS off
        DEPENDS X265_10BIT
        DEPENDS X265_12BIT
)
ExternalProject_Get_Property(X265_8BIT BINARY_DIR)
ExternalProject_Add_Step(X265_8BIT forcebuild
       COMMAND ${CMAKE_COMMAND} -E echo "Force build of X265_8BIT"
       DEPENDEES update
       DEPENDERS configure build
       ALWAYS 1)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/project-config.cmake.in
        "
CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
@PACKAGE_INIT@

set(ENV{PKG_CONFIG_PATH} ${CMAKE_INSTALL_PREFIX}/${IR_LIBDIR}/pkgconfig/)

find_package(PkgConfig REQUIRED)
pkg_check_modules(X265 REQUIRED x265)

add_library(irx265 INTERFACE)
target_link_libraries(irx265 INTERFACE \"\${X265_LDFLAGS} -Wl,-rpath,${CMAKE_INSTALL_PREFIX}/${IR_LIBDIR}\" )
target_include_directories(irx265 INTERFACE \${X265_INCLUDE_DIRS} ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR})
target_compile_options(irx265 INTERFACE \${X265_CFLAGS} )

check_required_components(${PROJECT_NAME})
")

include ( CMakePackageConfigHelpers )
configure_package_config_file (
        ${CMAKE_CURRENT_BINARY_DIR}/project-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
        INSTALL_DESTINATION ${IR_LIBDIR}/cmake/${PROJECT_NAME}
        PATH_VARS
)

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
        VERSION ${IR_VERSION}
        COMPATIBILITY SameMajorVersion )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake DESTINATION ${IR_CMAKEDIR})
install(DIRECTORY ${BINARY_DIR}/${X265_INSTALL_DIR}/${CMAKE_INSTALL_PREFIX}/ DESTINATION ${CMAKE_INSTALL_PREFIX} USE_SOURCE_PERMISSIONS)

set(X265_LIB_DIR ${BINARY_DIR}/${X265_INSTALL_DIR}/${CMAKE_INSTALL_PREFIX}/${IR_LIBDIR})
set(X265_INCLUDE_DIRECTORY ${BINARY_DIR}/${X265_INSTALL_DIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR})

add_library(irx265 INTERFACE)
target_link_libraries(irx265 INTERFACE ${X265_LIB_DIR}/libx265.so)
target_include_directories(irx265 INTERFACE ${X265_INCLUDE_DIRECTORY})
add_dependencies(irx265 X265_8BIT)
