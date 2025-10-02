CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

ENABLE_TESTING()

PROJECT(irdeto-ffmpeg)

add_subdirectory(./external/irdeto-xps-export)

add_subdirectory(./external/irdeto-x264)
ExternalProject_Get_Property(X264 BINARY_DIR)
set(X264_INSTALL_PREFIX ${BINARY_DIR}/${CMAKE_INSTALL_PREFIX})

add_subdirectory(./external/irdeto-x265)
ExternalProject_Get_Property(X265_8BIT BINARY_DIR)
set(X265_INSTALL_PREFIX ${BINARY_DIR}/${CMAKE_INSTALL_PREFIX})

add_subdirectory(./external/irdeto-srt)
ExternalProject_Get_Property(LIBSRT BINARY_DIR)
set(LIBSRT_INSTALL_PREFIX ${BINARY_DIR}/${CMAKE_INSTALL_PREFIX})

add_subdirectory(./external/irdeto-aom)
ExternalProject_Get_Property(LIBAOM BINARY_DIR)
set(LIBAOM_INSTALL_PREFIX ${BINARY_DIR}/${CMAKE_INSTALL_PREFIX})

include(GNUInstallDirs)

FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION IR_VERSION)
FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/RELEASE IR_RELEASE)

SET(IR_PROJECT_VERSION     "${IR_VERSION}-${IR_RELEASE}")
SET(IR_PROJECT_DIR         "${CMAKE_CURRENT_SOURCE_DIR}")
SET(IR_PROJECT_EXTRA_LIBS  "-lirxps -lsrt -laom -ldl")

SET(IR_LIBDIR       ${CMAKE_INSTALL_LIBDIR}/irffmpeg)
SET(IR_BINDIR       ${CMAKE_INSTALL_BINDIR}/irffmpeg)
SET(IR_INCDIR       ${CMAKE_INSTALL_INCLUDEDIR}/irffmpeg)
SET(IR_CMAKEDIR     ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

include(ProcessorCount)
ProcessorCount(PROCN)

include(ExternalProject)
set(FFMPEG_INSTALL_DIR ./)
ExternalProject_Add(
        FFMPEG
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/source
        CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/source/configure
            $<$<CONFIG:Debug>:--disable-optimizations>
            --env=\\"PKG_CONFIG_PATH=${X264_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/irx264/pkgconfig/:${X265_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/irx265/pkgconfig/\\"
            --enable-libx264
            --enable-libx265
            --enable-libaom
            --enable-libopenjpeg
            --enable-libsrt
            --disable-sdl2
            --disable-gpl
            --disable-static
            --disable-doc
            --enable-pic
            --enable-openssl
            --enable-shared
            --disable-stripping
            --disable-encoder=prores_aw
            --disable-encoder=prores
            --enable-encoder=prores_ks
            --enable-demuxer=dash
            --enable-libxml2
            --enable-rpath
            --disable-avisynth
            --extra-cflags=-isystem${X265_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/irx265/
            --extra-cflags=-isystem${X264_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/irx264/
            --extra-cflags=-isystem${irdeto-xps-export-devel_DIR}/../${CMAKE_INSTALL_INCLUDEDIR}/
            --extra-cflags=-isystem${LIBSRT_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/
            --extra-cflags=-isystem${LIBAOM_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/
            --extra-cflags=-Wl,--no-undefined
            --extra-ldflags=-Wl,-rpath,${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/irx264/
            --extra-ldflags=-Wl,-rpath,${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/irx265/
            --extra-ldflags=-Wl,-rpath,${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/irxps/
            --extra-ldflags=-Wl,-rpath,${X264_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/irx264/
            --extra-ldflags=-Wl,-rpath,${X265_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/irx265/
            --extra-ldflags=-Wl,-rpath,$<TARGET_LINKER_FILE_DIR:irxps>
            --extra-ldflags=-L${X264_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/irx264/
            --extra-ldflags=-L${X265_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/irx265/
            --extra-ldflags=-L$<TARGET_LINKER_FILE_DIR:irxps>
            --extra-ldflags=-L${LIBSRT_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/
            --extra-ldflags=-L${LIBAOM_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/
            --extra-ldexeflags=-Wl,-rpath,${CMAKE_INSTALL_PREFIX}/${IR_LIBDIR}/
            --extra-ldexeflags=-Wl,-rpath,${CMAKE_CURRENT_BINARY_DIR}/FFMPEG-prefix/src/FFMPEG-build/${FFMPEG_INSTALL_DIR}/${CMAKE_INSTALL_PREFIX}/${IR_LIBDIR}/
            --extra-libs=${IR_PROJECT_EXTRA_LIBS}
            --prefix=${CMAKE_INSTALL_PREFIX}
            --libdir=${CMAKE_INSTALL_PREFIX}/${IR_LIBDIR}/
            --bindir=${CMAKE_INSTALL_PREFIX}/${IR_BINDIR}/
            --incdir=${CMAKE_INSTALL_PREFIX}/${IR_INCDIR}/
            --datadir=${CMAKE_INSTALL_PREFIX}/share/irffmpeg
        DEPENDS X264
        DEPENDS X265_8BIT
        DEPENDS irxps
        DEPENDS LIBSRT
        DEPENDS LIBAOM
        BUILD_COMMAND make -j${PROCN}
        INSTALL_COMMAND make install DESTDIR=${FFMPEG_INSTALL_DIR}
        LOG_CONFIGURE on
        LOG_BUILD on
        LOG_INSTALL on
        BUILD_ALWAYS off
        BUILD_IN_SOURCE off
)
ExternalProject_Get_Property(FFMPEG BINARY_DIR)

add_subdirectory(example)
add_subdirectory(test)

########################################################################################################################
install(DIRECTORY ${BINARY_DIR}/${FFMPEG_INSTALL_DIR}/${CMAKE_INSTALL_PREFIX}/ DESTINATION ${CMAKE_INSTALL_PREFIX} USE_SOURCE_PERMISSIONS)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/project-config.cmake.in
        "
CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
@PACKAGE_INIT@

set(ENV{PKG_CONFIG_PATH} ${CMAKE_INSTALL_PREFIX}/${IR_LIBDIR}/pkgconfig/)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavcodec libavdevice libavfilter libavformat libavutil libswresample libswscale)

add_library(irffmpeg INTERFACE)
target_link_libraries(irffmpeg INTERFACE \${FFMPEG_LDFLAGS} )
target_include_directories(irffmpeg INTERFACE ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR} \${FFMPEG_INCLUDE_DIRS})
target_compile_options(irffmpeg INTERFACE \${FFMPEG_CFLAGS} )

check_required_components(${PROJECT_NAME})

find_package(irdeto-x264 REQUIRED CONFIG)
find_package(irdeto-x265 REQUIRED CONFIG)
find_package(irdeto-xps-export REQUIRED CONFIG)
find_package(irdeto-srt REQUIRED CONFIG)
find_package(irdeto-aom REQUIRED CONFIG)
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

########################################################################################################################

set(FFMPEG_LIB_DIR ${BINARY_DIR}/${FFMPEG_INSTALL_DIR}/${CMAKE_INSTALL_PREFIX}/${IR_LIBDIR})
set(FFMPEG_INCLUDE_DIRECTORY ${BINARY_DIR}/${FFMPEG_INSTALL_DIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR})

add_library(avutil INTERFACE)
target_link_libraries(avutil INTERFACE ${FFMPEG_LIB_DIR}/libavutil.so)
target_include_directories(avutil INTERFACE ${FFMPEG_INCLUDE_DIRECTORY} ${FFMPEG_INCLUDE_DIRECTORY}/irffmpeg ${BINARY_DIR})

add_library(avformat INTERFACE)
target_link_libraries(avformat INTERFACE ${FFMPEG_LIB_DIR}/libavformat.so)
target_include_directories(avformat INTERFACE ${FFMPEG_INCLUDE_DIRECTORY} ${FFMPEG_INCLUDE_DIRECTORY}/irffmpeg ${BINARY_DIR})

add_library(avcodec INTERFACE)
target_link_libraries(avcodec INTERFACE ${FFMPEG_LIB_DIR}/libavcodec.so)
target_include_directories(avcodec INTERFACE ${FFMPEG_INCLUDE_DIRECTORY} ${FFMPEG_INCLUDE_DIRECTORY}/irffmpeg ${BINARY_DIR})

add_library(swscale INTERFACE)
target_link_libraries(swscale INTERFACE ${FFMPEG_LIB_DIR}/libswscale.so)
target_include_directories(swscale INTERFACE ${FFMPEG_INCLUDE_DIRECTORY} ${FFMPEG_INCLUDE_DIRECTORY}/irffmpeg ${BINARY_DIR})

add_library(avdevice INTERFACE)
target_link_libraries(avdevice INTERFACE ${FFMPEG_LIB_DIR}/libavdevice.so)
target_include_directories(avdevice INTERFACE ${FFMPEG_INCLUDE_DIRECTORY} ${FFMPEG_INCLUDE_DIRECTORY}/irffmpeg ${BINARY_DIR})

add_library(avfilter INTERFACE)
target_link_libraries(avfilter INTERFACE ${FFMPEG_LIB_DIR}/libavfilter.so)
target_include_directories(avfilter INTERFACE ${FFMPEG_INCLUDE_DIRECTORY} ${FFMPEG_INCLUDE_DIRECTORY}/irffmpeg ${BINARY_DIR})

add_library(swresample INTERFACE)
target_link_libraries(swresample INTERFACE ${FFMPEG_LIB_DIR}/libswresample.so)
target_include_directories(swresample INTERFACE ${FFMPEG_INCLUDE_DIRECTORY} ${FFMPEG_INCLUDE_DIRECTORY}/irffmpeg ${BINARY_DIR})

add_library(irffmpeg INTERFACE)
target_link_libraries(irffmpeg INTERFACE avcodec avutil avformat swscale avdevice avfilter swresample)
target_include_directories(irffmpeg INTERFACE ${FFMPEG_INCLUDE_DIRECTORY})
add_dependencies(irffmpeg FFMPEG)

INSTALL(FILES source/RELEASE_NOTES RELEASE source/LICENSE.md source/COPYING.LGPLv2.1 source/CREDITS DESTINATION share/irffmpeg)

INSTALL(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_INSTALL_PREFIX}/${IR_BINDIR}/ffmpeg ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/irdeto-ffmpeg)")
INSTALL(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_INSTALL_PREFIX}/${IR_BINDIR}/ffprobe ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/irdeto-ffprobe)")
