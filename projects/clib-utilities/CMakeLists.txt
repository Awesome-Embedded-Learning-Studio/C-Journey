cmake_minimum_required(VERSION 3.23)
project(clib_utilities VERSION 1.0.0 LANGUAGES C)

# 显式源表(不用 GLOB_RECURSE):CMake 官方不推荐 GLOB 收源——
# 加/删文件不会自动触发重新 configure,列出来更稳、更可读、可审查。
set(CLUIB_SRC
    BasicDataStructure/Sources/CCBasicString.c
    BasicDataStructure/Sources/CCDynamicArray.c
    BasicDataStructure/Sources/CCLinkList.c
    BasicDataStructure/Sources/CCRange.c
    Basic_Utils/Sources/CCPair.c
    Basic_Utils/Sources/CCPlainObject.c
    Basic_Utils/Sources/CCSTD_CmpBasic.c
    Basic_Utils/Sources/CCSTDLib_Index_Handle.c
    SystemRelated/Sources/CCDynamicLoader.c
    SystemRelated/Sources/CCMutex.c
    SystemRelated/Sources/CCSTDLib_FetchError.c
    SystemRelated/Sources/CCSTDLib_MultiByte_WCharAdapter.c
    SystemRelated/Sources/CCThread.c)

add_library(clib_utils STATIC ${CLUIB_SRC})

target_include_directories(clib_utils
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/BasicDataStructure/Includes
        ${CMAKE_CURRENT_SOURCE_DIR}/Basic_Utils/Includes
        ${CMAKE_CURRENT_SOURCE_DIR}/SystemRelated/Includes)

target_compile_features(clib_utils PUBLIC c_std_11)
target_compile_options(clib_utils PRIVATE -Wall -Wextra)

# POSIX 需要 pthread(线程/互斥锁)与 dl(动态加载);Windows 走自己的。
if(NOT WIN32)
    target_link_libraries(clib_utils PUBLIC pthread dl)
endif()

# ---- 测试 ----
# 基建阶段:smoke test(能编能跑、退出 0 即过)。
# 第 7 章 会把现有手写 test 函数迁到 Unity + 逐例 add_test。
enable_testing()
add_executable(test_runner
    main.c
    test/testDynamicArray.c
    test/testDynamicPlugin.c
    test/textMutex.c)
target_link_libraries(test_runner PRIVATE clib_utils)
target_include_directories(test_runner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test)
add_test(NAME clib_smoke COMMAND test_runner)

# Unity-based DynamicArray 测试(用 test/unity/ 的极简 Unity shim)
add_executable(test_dynamic_array_unity
    test/test_DynamicArray_unity.c
    test/unity/unity.c)
target_link_libraries(test_dynamic_array_unity PRIVATE clib_utils)
target_include_directories(test_dynamic_array_unity PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test/unity)
add_test(NAME dynamic_array_unity COMMAND test_dynamic_array_unity)
