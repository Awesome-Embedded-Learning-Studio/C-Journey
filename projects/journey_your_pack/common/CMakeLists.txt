# 各 pack 共用的地基:分配器层
file(GLOB_RECURSE JYP_COMMON_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_library(jyp_common STATIC ${JYP_COMMON_SOURCES})
add_library(jyp::common ALIAS jyp_common)
target_include_directories(jyp_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(jyp_allocation_tests test/test_jyp_allocation.c)
target_link_libraries(jyp_allocation_tests PRIVATE jyp::common)
add_test(NAME allocation COMMAND jyp_allocation_tests)
