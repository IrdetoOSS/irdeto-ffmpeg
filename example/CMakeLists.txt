
add_executable(irdeto-codec-util main.cpp ffmpeg_encoder.cpp)
target_link_libraries(irdeto-codec-util PRIVATE irffmpeg)
target_compile_options(irdeto-codec-util PRIVATE -Wall -Wextra -Werror)
target_include_directories(irdeto-codec-util PRIVATE ${IR_PROJECT_DIR}/source ${IR_PROJECT_DIR}/external/irdeto-xps-export/include)

set_target_properties(irdeto-codec-util PROPERTIES VERSION ${IR_PROJECT_VERSION})
set_target_properties(irdeto-codec-util PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
set_target_properties(irdeto-codec-util PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/${IR_LIBDIR})

install(TARGETS irdeto-codec-util RUNTIME DESTINATION ${IR_BINDIR})
