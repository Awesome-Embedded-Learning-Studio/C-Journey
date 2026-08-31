# pack1:动态数组。库 + 测试 + 演示,自包含
file(GLOB_RECURSE JYP_VECTOR_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_library(jyp_vector STATIC ${JYP_VECTOR_SOURCES})
add_library(jyp::vector ALIAS jyp_vector)
target_include_directories(jyp_vector PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# 分配器地基只在自己 .c 里用,PRIVATE:不漏给下游
target_link_libraries(jyp_vector PRIVATE jyp::common)

# 测试直接用 common 的 raw API 换分配器,所以这里显式链它
add_executable(jyp_vector_tests test/test_jyp_vector.c)
target_link_libraries(jyp_vector_tests PRIVATE jyp::vector jyp::common)
add_test(NAME vector COMMAND jyp_vector_tests)

add_executable(jyp_vector_example example/example_jyp_vector.c)
target_link_libraries(jyp_vector_example PRIVATE jyp::vector)
