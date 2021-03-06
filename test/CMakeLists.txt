get_filename_component(AVC_NON_ANNEXB_EXTRDADA_BIN_FILE cenc_samples/extradata_nonannexb.bin ABSOLUTE)
get_filename_component(AVC_NON_ANNEXB_SAMPLE1_BIN_FILE cenc_samples/sample1_nonannexb.bin ABSOLUTE)
get_filename_component(AVC_NON_ANNEXB_SAMPLE2_BIN_FILE cenc_samples/sample2_nonannexb.bin ABSOLUTE)

get_filename_component(AVC_CBCS_SAMPLE1_BIN_FILE cenc_samples/sample1_cbcs.bin ABSOLUTE)
get_filename_component(AVC_CBCS_SAMPLE2_BIN_FILE cenc_samples/sample2_cbcs.bin ABSOLUTE)

get_filename_component(AVC_CENC_SAMPLE1_BIN_FILE cenc_samples/sample1_cenc.bin ABSOLUTE)



get_filename_component(AVC_ANNEXB_EXTRDADA_BIN_FILE cenc_samples/extradata_annexb.bin ABSOLUTE)
get_filename_component(AVC_ANNEXB_SAMPLE1_BIN_FILE cenc_samples/sample1_annexb.bin ABSOLUTE)
get_filename_component(AVC_ANNEXB_SAMPLE2_BIN_FILE cenc_samples/sample2_annexb.bin ABSOLUTE)

get_filename_component(AVC_CBCS_SAMPLE1_FROM_ANNEXB_BIN_FILE cenc_samples/sample1_cbcs_fromannexb.bin ABSOLUTE)
get_filename_component(AVC_CBCS_SAMPLE2_FROM_ANNEXB_BIN_FILE cenc_samples/sample2_cbcs_fromannexb.bin ABSOLUTE)

get_filename_component(AVC_CENC_V3_SAMPLE1_FROM_ANNEXB_BIN_FILE cenc_samples/sample1_cenc_v3_fromannexb.bin ABSOLUTE)


#----------------------------------------------------------
add_executable(test_cbcs test_cbcs.c main.c test_mock.c
                               ${IR_PROJECT_DIR}/source/libavformat/movenccbcs.c
                               ${IR_PROJECT_DIR}/source/libavutil/aes.c
                               ${IR_PROJECT_DIR}/source/libavformat/avc.c
                               ${IR_PROJECT_DIR}/source/libavcodec/golomb.c
                               ${IR_PROJECT_DIR}/source/libavutil/intmath.c
                               ${IR_PROJECT_DIR}/source/libavutil/log2_tab.c
              )

target_include_directories(test_cbcs PRIVATE ${IR_PROJECT_DIR}/source
                                             ${IR_PROJECT_DIR}/source/compat/avisynth/avs
                          )
target_compile_options(test_cbcs PRIVATE -Wall -Wextra -std=c99 -D SUINT=int
                                         -DAVC_NON_ANNEXB_EXTRDADA_BIN_FILE="${AVC_NON_ANNEXB_EXTRDADA_BIN_FILE}"
                                         -DAVC_NON_ANNEXB_SAMPLE1_BIN_FILE="${AVC_NON_ANNEXB_SAMPLE1_BIN_FILE}"
                                         -DAVC_NON_ANNEXB_SAMPLE2_BIN_FILE="${AVC_NON_ANNEXB_SAMPLE2_BIN_FILE}"
                                         -DAVC_CBCS_SAMPLE1_BIN_FILE="${AVC_CBCS_SAMPLE1_BIN_FILE}"
                                         -DAVC_CBCS_SAMPLE2_BIN_FILE="${AVC_CBCS_SAMPLE2_BIN_FILE}"

                                         -DAVC_ANNEXB_EXTRDADA_BIN_FILE="${AVC_ANNEXB_EXTRDADA_BIN_FILE}"
                                         -DAVC_ANNEXB_SAMPLE1_BIN_FILE="${AVC_ANNEXB_SAMPLE1_BIN_FILE}"
                                         -DAVC_ANNEXB_SAMPLE2_BIN_FILE="${AVC_ANNEXB_SAMPLE2_BIN_FILE}"
                                         -DAVC_CBCS_SAMPLE1_FROM_ANNEXB_BIN_FILE="${AVC_CBCS_SAMPLE1_FROM_ANNEXB_BIN_FILE}"
                                         -DAVC_CBCS_SAMPLE2_FROM_ANNEXB_BIN_FILE="${AVC_CBCS_SAMPLE2_FROM_ANNEXB_BIN_FILE}"
                                     )
add_dependencies(test_cbcs irffmpeg)
target_link_libraries(test_cbcs check)
add_test(test_cbcs test_cbcs)


#----------------------------------------------------------
add_executable(test_cencv3 test_cencv3.c main.c test_mock.c
                               ${IR_PROJECT_DIR}/source/libavformat/movenccencv3.c
                               ${IR_PROJECT_DIR}/source/libavutil/aes.c
                               ${IR_PROJECT_DIR}/source/libavutil/aes_ctr.c
                               ${IR_PROJECT_DIR}/source/libavformat/avc.c
                               ${IR_PROJECT_DIR}/source/libavcodec/golomb.c
                               ${IR_PROJECT_DIR}/source/libavutil/intmath.c
                               ${IR_PROJECT_DIR}/source/libavutil/log2_tab.c
              )

target_include_directories(test_cencv3 PRIVATE ${IR_PROJECT_DIR}/source
                                             ${IR_PROJECT_DIR}/source/compat/avisynth/avs
                          )
target_compile_options(test_cencv3 PRIVATE -Wall -Wextra -std=c99 -D SUINT=int
                                         -DAVC_NON_ANNEXB_EXTRDADA_BIN_FILE="${AVC_NON_ANNEXB_EXTRDADA_BIN_FILE}"
                                         -DAVC_NON_ANNEXB_SAMPLE1_BIN_FILE="${AVC_NON_ANNEXB_SAMPLE1_BIN_FILE}"
                                         -DAVC_CENC_SAMPLE1_BIN_FILE="${AVC_CENC_SAMPLE1_BIN_FILE}"

                                         -DAVC_ANNEXB_EXTRDADA_BIN_FILE="${AVC_ANNEXB_EXTRDADA_BIN_FILE}"
                                         -DAVC_ANNEXB_SAMPLE1_BIN_FILE="${AVC_ANNEXB_SAMPLE1_BIN_FILE}"
                                         -DAVC_CENC_V3_SAMPLE1_FROM_ANNEXB_BIN_FILE="${AVC_CENC_V3_SAMPLE1_FROM_ANNEXB_BIN_FILE}"
                                     )
add_dependencies(test_cencv3 irffmpeg)
target_link_libraries(test_cencv3 check)
add_test(test_cencv3 test_cencv3)

#----------------------------------------------------------
add_executable(test_avc_parser test_avc_parser.c main.c test_mock.c
                               ${IR_PROJECT_DIR}/source/libavformat/avc.c
                               ${IR_PROJECT_DIR}/source/libavcodec/golomb.c
                               ${IR_PROJECT_DIR}/source/libavutil/intmath.c
                               ${IR_PROJECT_DIR}/source/libavutil/log2_tab.c
                )
target_include_directories(test_avc_parser PRIVATE ${IR_PROJECT_DIR}/source
                                                   ${IR_PROJECT_DIR}/source/compat/avisynth/avs
                                                   ${IR_PROJECT_DIR}/source/libavutil
                          )
target_compile_options(test_avc_parser PRIVATE -Wall -Wextra -std=c99 -D SUINT=int -DIRDETO_UNIT_TEST)
add_dependencies(test_avc_parser irffmpeg)
target_link_libraries(test_avc_parser check)
add_test(test_avc_parser test_avc_parser)
