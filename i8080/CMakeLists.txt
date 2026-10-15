# CMakeLists.txt для standalone-библиотеки i8080
#
# Универсальный эмулятор Intel 8080 / КР580ВМ80А.
# Не зависит от korvet-core — может быть использован в любом проекте.

add_library(i8080 STATIC
    cpu.cpp
)
target_include_directories(i8080 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(i8080 PUBLIC)
target_compile_options(i8080 PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wno-unused-parameter -Wno-unused-variable>
)
