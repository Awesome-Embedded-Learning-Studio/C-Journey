cmake_minimum_required(VERSION 3.31.0)

project(journey_your_pack LANGUAGES C VERSION 0.1.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 23)

# gcc/clang 下默认全家桶:库、测试、example 全部 ASan+UBSan 伺候
option(JYP_ENABLE_SANITIZERS "ASan+UBSan(gcc/clang)" ON)
if(JYP_ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

enable_testing()

# 共用地基先来,各 pack 挂在后面(它们链 common)
add_subdirectory(common)
add_subdirectory(pack1_vector)
