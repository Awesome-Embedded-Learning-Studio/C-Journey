cmake_minimum_required(VERSION 3.23.0)
project(CCSTD_UtilsLib
        VERSION 1.0.0
        DESCRIPTION "A CCSTD_UtilsLib Builder")

message("Start Building")
set(CCSTD_BasicUtils_INC_DIR "./Basic_Utils/Includes/")
set(CCSTD_BasicDataStructure_INC_DIR "./BasicDataStructure./Includes/")
set(CCSTD_SystemRelated_INC_DIR "./SystemRelated/Includes/")
set(CCSTD_Test_DIR  "./test/")

set(CCSTD_BasicUtils_SRC_DIR "./Basic_Utils/Sources")
set(CCSTD_BasicDataStructure_SRC_DIR "./BasicDataStructure/Sources")
set(CCSTD_SystemRelated_SRC_DIR "./SystemRelated/Sources")

# Collect the source files
file(GLOB_RECURSE CCSTD_BasicUtils_SRC_FILE ${CCSTD_BasicUtils_SRC_DIR}/*.c)
file(GLOB_RECURSE CCSTD_BasicDataStructure_SRC_FILE ${CCSTD_BasicDataStructure_SRC_DIR}/*.c)
file(GLOB_RECURSE CCSTD_SystemRelated_SRC_FILE ${CCSTD_SystemRelated_SRC_DIR}/*.c)
file(GLOB_RECURSE CCSTD_Test_SRC_FILE ${CCSTD_Test_DIR}/*.c)

set(CCSTD_COMPILE_ALL_SRC   ${CCSTD_BasicUtils_SRC_FILE}
                            ${CCSTD_BasicDataStructure_SRC_FILE} 
                            ${CCSTD_SystemRelated_SRC_FILE}
                            ${CCSTD_Test_SRC_FILE})



set(CCSTD_COMPILE_HEADER_PATH   ${CCSTD_Test_DIR}
                                ${CCSTD_BasicUtils_INC_DIR}
                                ${CCSTD_SystemRelated_INC_DIR}
                                ${CCSTD_BasicDataStructure_INC_DIR})

message("Displaying the configure:> ")                        
message("CCSTD_BasicUtils_SRC_FILE = \n${CCSTD_BasicUtils_SRC_FILE}\n")
message("CCSTD_BasicDataStructure_SRC_FILE = \n${CCSTD_BasicDataStructure_SRC_FILE}\n")
message("CCSTD_SystemRelated_SRC_FILE = \n${CCSTD_SystemRelated_SRC_FILE}\n")
message("CCSTD_Test_SRC_FILE = \n${CCSTD_Test_SRC_FILE}\n")
message("CCSTD_COMPILE_ALL_SRC = \n${CCSTD_COMPILE_ALL_SRC}\n")

add_executable(TEST_RES ${CCSTD_COMPILE_ALL_SRC} main.c)

target_include_directories(TEST_RES PUBLIC ${CCSTD_COMPILE_HEADER_PATH})





