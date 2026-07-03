# 阶段4·第6章活教材:一个能产 .a + .so、带 install/export 的最小库工程。
cmake_minimum_required(VERSION 3.23)
project(mathlib VERSION 1.0.0 LANGUAGES C)

# 必须在用到 \${CMAKE_INSTALL_*} 之前 include
include(GNUInstallDirs)

# 同一份源、编成静态库和动态库两个 target(SOVERSION 给 .so 带 ABI 版本后缀)
add_library(mathlib STATIC src/mathlib.c)
add_library(mathlib_shared SHARED src/mathlib.c)
# 要 install/export 的 target,PUBLIC include 必须用 BUILD_INTERFACE/INSTALL_INTERFACE
# 生成器表达式:构建时指向源目录、安装后指向 ${CMAKE_INSTALL_INCLUDEDIR},
# 否则 CMake 拒绝生成(导出的 target 不能含源目录绝对路径)。
foreach(t IN ITEMS mathlib mathlib_shared)
    target_include_directories(${t}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_compile_features(${t} PUBLIC c_std_11)
    target_compile_options(${t} PRIVATE -Wall -Wextra)
endforeach()
set_target_properties(mathlib_shared PROPERTIES OUTPUT_NAME mathlib SOVERSION 1)

install(TARGETS mathlib mathlib_shared
        EXPORT MathlibTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
         LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES include/mathlib.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT MathlibTargets
        FILE MathlibTargets.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Mathlib)

# Config + 版本文件:让 find_package(Mathlib) 能装上、能查兼容版本
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    MathlibConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT)
install(FILES
        MathlibConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/MathlibConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Mathlib)
