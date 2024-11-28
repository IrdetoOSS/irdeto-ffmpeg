include(ProcessorCount)
ProcessorCount(PROCN)

include(ExternalProject)
ExternalProject_Add(
        X265_12BIT
        SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/sources/source
        PATCH_COMMAND patch -p0 --forward -i "${IR_PROJECT_PATCH}" -d "${CMAKE_CURRENT_LIST_DIR}/sources/source" || true
        INSTALL_COMMAND make install DESTDIR=${CMAKE_CURRENT_BINARY_DIR}
        BUILD_COMMAND make -j${PROCN}
        CONFIGURE_COMMAND cmake -DHIGH_BIT_DEPTH=ON -DMAIN12=ON
        "-DCMAKE_CXX_FLAGS=${OpenMP_CXX_FLAGS} -I${irdeto-xps-export-devel_DIR}/../${CMAKE_INSTALL_INCLUDEDIR}/"
        "-DCMAKE_C_FLAGS=${OpenMP_C_FLAGS} -I${irdeto-xps-export-devel_DIR}/../${CMAKE_INSTALL_INCLUDEDIR}/"
        -DEXPORT_C_API=OFF
        -DENABLE_SHARED=OFF
        -DENABLE_CLI=OFF
        -DCMAKE_INSTALL_PREFIX=/12bit
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DENABLE_LIBNUMA=OFF ${CMAKE_CURRENT_LIST_DIR}/sources/source
        LOG_BUILD on
        LOG_CONFIGURE on
        LOG_INSTALL on
        BUILD_ALWAYS on
)
ExternalProject_Add_Step(X265_12BIT forcebuild
        COMMAND ${CMAKE_COMMAND} -E echo "Force build of X265_12BIT"
        DEPENDEES configure
        DEPENDERS build
        ALWAYS 1)
