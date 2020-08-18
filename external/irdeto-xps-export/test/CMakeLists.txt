SET(TEST_RUNNER_NAME            "${PROJECT_NAME}-test-runner")
SET(VALGRIND_RUNNER_NAME        "${PROJECT_NAME}-valgrind-runner")

#
# Introduce values for test results
#
SET(TEST_PASS_VALUE             0)
SET(TEST_FAIL_VALUE             1)

#
# List of test and related to that list values
#
SET(TEST_CASE_ID                0)
SET(TEST_CASE_SIGNATURES_LIST   "")
SET(TEST_CASE_FUNCTIONS         "")

SET(TEST_CASE_LIST              ir_test_xps_common_flow
                                ir_test_xps_corrupted_ctx
                                ir_test_xps_import_x265_null
                                ir_test_xps_import_x265_unready
                                ir_test_xps_import_x265_ready
                                ir_test_xps_import_x265_slices
                                ir_test_xps_import_x265_tiles
)

################################################################################
#
################################################################################

INCLUDE_DIRECTORIES(${IR_PROJECT_DIR}/include)

################################################################################
# ADD FILE TO LINK HERE
################################################################################
SET(UNIT_SRC
                ir_xps_export_test.cpp
                test_runner.cpp)

SET(VALG_SRC
                ir_xps_export_test.cpp
                valgrind_runner.cpp)

#
# Add tests to the list
#
FOREACH(TEST_NAME ${TEST_CASE_LIST})
    ADD_TEST(${TEST_NAME}
                ${TEST_RUNNER_NAME}
                "${TEST_CASE_ID}")

    SET_TESTS_PROPERTIES(${TEST_NAME} PROPERTIES
                                    PASS_REGULAR_EXPRESSION
                                    "${TEST_PASS_VALUE}")

    MATH(EXPR TEST_CASE_ID "${TEST_CASE_ID}+1")

    SET(TEST_CASE_SIGNATURES_LIST
    "${TEST_CASE_SIGNATURES_LIST} extern TR_TYPE ${TEST_NAME}(void);\n")

    SET(TEST_CASE_FUNCTIONS "${TEST_CASE_FUNCTIONS} ${TEST_NAME},\n\t")

ENDFOREACH(TEST_NAME)

#
# test/test_types.h file generation
#
CONFIGURE_FILE(${IR_PROJECT_DIR}/config/test_types.h.in
    ${IR_PROJECT_DIR}/test/test_types.h)

#
# test/test_runner.c file generation
#
CONFIGURE_FILE(${IR_PROJECT_DIR}/config/test_runner.c.in
    ${IR_PROJECT_DIR}/test/test_runner.cpp)

#
# test/valgrind_runner.c file generation
#
CONFIGURE_FILE(${IR_PROJECT_DIR}/config/valgrind_runner.c.in
    ${IR_PROJECT_DIR}/test/valgrind_runner.cpp)

#
# Create a irdeto-scd-test binary for unit-testing
#
ADD_EXECUTABLE(${TEST_RUNNER_NAME}   ${UNIT_SRC})
target_compile_features(${TEST_RUNNER_NAME} PUBLIC cxx_nullptr)

ADD_EXECUTABLE(${VALGRIND_RUNNER_NAME}  ${VALG_SRC})
target_compile_features(${VALGRIND_RUNNER_NAME} PUBLIC cxx_nullptr)

TARGET_LINK_LIBRARIES(${TEST_RUNNER_NAME}   irxps_gcov irx265 m)
TARGET_LINK_LIBRARIES(${VALGRIND_RUNNER_NAME}   irxps_gcov irx265 m)
