cmake_minimum_required(VERSION 3.16)
project(korvet_core VERSION 0.3.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# i8080 — standalone CPU library, lives next to korvet_core.
# i8080 — standalone-библиотека CPU, поставляется вместе с korvet_core.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/i8080 ${CMAKE_BINARY_DIR}/i8080_build)

# Collect all source files
# v1.1.1 (platform-agnostic / host-naive аудит):
#   - internal/config/config.cpp читает/пишет файлы (std::ifstream/ofstream)
#     и никем из ядра не используется — вынесен в опцию KORVET_CORE_HOST_TOOLS
#     (по умолчанию OFF). Хост сам загружает ПЗУ/шрифты/mapper через фасад.
#   - *_dump.cpp (cpu/memory/timer/video/fdc/ppi/pic/logger) — отладочные
#     утилиты с fopen() — В СБОРКУ ЯДРА НЕ ВХОДЯТ и не должны; они для
#     host-инструментов (подключайте выборочно в GUI/CLI, не в ядре).
#   - main.cpp (демо с файловыми путями) перенесён в examples/ и не собирается.
set(KORVET_CORE_SOURCES
    korvet_core/motherboard.cpp
    korvet_core/internal/cpu/cpu.cpp
    korvet_core/internal/cpu/disasm.cpp
    korvet_core/internal/memory/memory.cpp
    korvet_core/internal/video/video.cpp
    korvet_core/internal/audio/audio.cpp
    korvet_core/internal/timer/timer.cpp
    korvet_core/internal/pic/pic.cpp
    korvet_core/internal/ppi/ppi.cpp
    korvet_core/internal/fdc/fdc.cpp
    korvet_core/internal/serial/serial.cpp
    korvet_core/internal/lan/lan.cpp
    korvet_core/internal/tape/tape.cpp
    korvet_core/internal/keyboard/keyboard.cpp
    korvet_core/internal/mouse/mouse.cpp
    korvet_core/internal/joystick/joystick.cpp
    korvet_core/internal/printer/printer.cpp
    korvet_core/internal/logger/logger.cpp
)

option(KORVET_CORE_HOST_TOOLS "Собрать host-утилиты ядра (config.cpp с файловым I/O)" OFF)
if(KORVET_CORE_HOST_TOOLS)
    list(APPEND KORVET_CORE_SOURCES korvet_core/internal/config/config.cpp)
endif()

set(KORVET_CORE_HEADERS
    korvet_core/motherboard.hpp
    korvet_core/motherboard_types.hpp
    korvet_core/internal/cpu/cpu.hpp
    korvet_core/internal/cpu/disasm.hpp
    korvet_core/internal/memory/memory.hpp
    korvet_core/internal/video/video.hpp
    korvet_core/internal/audio/audio.hpp
    korvet_core/internal/timer/timer.hpp
    korvet_core/internal/pic/pic.hpp
    korvet_core/internal/ppi/ppi.hpp
    korvet_core/internal/fdc/fdc.hpp
    korvet_core/internal/serial/serial.hpp
    korvet_core/internal/lan/lan.hpp
    korvet_core/internal/tape/tape.hpp
    korvet_core/internal/keyboard/keyboard.hpp
    korvet_core/internal/mouse/mouse.hpp
    korvet_core/internal/joystick/joystick.hpp
    korvet_core/internal/printer/printer.hpp
    korvet_core/internal/logger/logger.hpp
    korvet_core/internal/config/config.hpp
)

# Create the korvet_core library
add_library(korvet_core STATIC ${KORVET_CORE_SOURCES} ${KORVET_CORE_HEADERS})
add_dependencies(korvet_core i8080)
target_link_libraries(korvet_core PUBLIC i8080)
target_include_directories(korvet_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/i8080)

# Set include directories — expose `korvet_core/` so includes look like
#   #include "korvet_core/motherboard.hpp"
target_include_directories(korvet_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

# Add compile options — keep warnings but don't treat them as errors
# (some legacy code in korvet_core trips -Wextra).
target_compile_options(korvet_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wno-unused-parameter -Wno-unused-variable>
)

# Define preprocessor macros
target_compile_definitions(korvet_core
    PRIVATE
        KORVET_CORE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        KORVET_CORE_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        KORVET_CORE_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# --- Пример: минимальная инициализация ядра (host читает файлы сам) ---
option(KORVET_CORE_BUILD_EXAMPLES "Собрать examples/core_demo" ON)
if(KORVET_CORE_BUILD_EXAMPLES)
    add_executable(core_demo korvet_core/examples/core_demo.cpp)
    target_link_libraries(core_demo PRIVATE korvet_core)
    target_include_directories(core_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/i8080)
endif()

# --- Тесты (ctest): внутриядровые selftest'ы через minigui ---
include(CTest)

# --- minigui: консольный инструмент проверки ядра (headless + опция SDL2) ---
# Скриншоты PNG/WAV-дампы/ввод по сценарию/selftest подсистем.
# Полноценная проверка — запуск гостевого ПО (см. korvet_core/minigui/README.md).
option(KORVET_CORE_BUILD_MINIGUI "Собрать minigui (CLI-проверка ядра)" ON)
if(KORVET_CORE_BUILD_MINIGUI)
    add_subdirectory(korvet_core/minigui)
endif()
