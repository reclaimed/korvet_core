# ---------------------------------------------------------------------------
# minigui — консольный инструмент проверки ядра korvet_core (без GUI-фреймворков).
#
# Цели:
#   minigui     — CLI-харнесс: прогон гостевого ПО, PNG-скриншоты, WAV-звук,
#                 ввод клавиш/мыши/джойстика по сценарию, selftest подсистем
#   guest_test  — прогон одного гостевого сценария (загрузка → команда → дамп)
#   minigui-sdl — ОПЦИОНАЛЬНО: SDL2-окно 512×256 (собирается только при
#                 наличии SDL2 в системе; для CI/headless не требуется)
#
# Утилиты (PNG/WAV-кодеки, карта клавиатуры, адаптер дискет) — header-only.
# ---------------------------------------------------------------------------

find_package(ZLIB REQUIRED)

add_executable(minigui main.cpp)
target_link_libraries(minigui PRIVATE korvet_core ZLIB::ZLIB)
target_include_directories(minigui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..            # korvet_core/...
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../i8080   # i8080/...
)

# Прогон одного сценария гостевого ПО
add_executable(guest_test guest_test.cpp)
target_link_libraries(guest_test PRIVATE korvet_core ZLIB::ZLIB)
target_include_directories(guest_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../i8080
)

# Верификация ИРПС/мыши/джойстика/LAN гостевым кодом 8080 (periph_prog.hpp)
add_executable(periph_guest_test periph_guest_test.cpp)
target_link_libraries(periph_guest_test PRIVATE korvet_core ZLIB::ZLIB)
target_include_directories(periph_guest_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../i8080
)

# --- Опциональное SDL2-окно (интерактивный прогон) ---
# Собирается ТОЛЬКО если SDL2 найден в системе (например,
# apt install libsdl2-dev). Headless-проверка (CI) в SDL2 не нуждается:
# всё ядро тестируется через minigui/guest_test.
find_package(SDL2 QUIET)
if(SDL2_FOUND AND NOT KORVET_MINIGUI_NO_SDL)
    message(STATUS "minigui: SDL2 найден — собираю интерактивное окно minigui-sdl")
    add_executable(minigui-sdl sdl_window.cpp)
    target_link_libraries(minigui-sdl PRIVATE korvet_core SDL2::SDL2)
    target_include_directories(minigui-sdl PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../i8080
    )
else()
    message(STATUS "minigui: SDL2 не найден — пропускаю minigui-sdl (headless-режим)")
endif()

# Тестовые сценарии (ctest): самопроверки подсистем + загрузка ПЗУ.
# Для запуска нужны данные (ПЗУ/маппер/шрифт/диски) — каталог передаётся
# через -DKORVET_DATA_DIR=... (по умолчанию ожидается рядом с исходниками).
set(KORVET_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../korvet_data/data"
    CACHE PATH "Каталог данных (ПЗУ, шрифты, KDI-образы)")

add_test(NAME minigui_selftest
         COMMAND minigui --selftest)
set_tests_properties(minigui_selftest PROPERTIES
    PASS_REGULAR_EXPRESSION "ВСЕ ПРОВЕРКИ ПРОЙДЕНЫ")

if(EXISTS "${KORVET_DATA_DIR}/rom/cpm22_shkomp.rom")
    add_test(NAME minigui_boot_shkcomp
             COMMAND minigui --rom ${KORVET_DATA_DIR}/rom/cpm22_shkomp.rom
                             --mapper ${KORVET_DATA_DIR}/rom/mapper.mem
                             --font ${KORVET_DATA_DIR}/rom/korvet.fnt
                             --disk 0=${KORVET_DATA_DIR}/kdi/disk__system.kdi
                             --frames 800 --quiet)
    set_tests_properties(minigui_boot_shkcomp PROPERTIES
        PASS_REGULAR_EXPRESSION "готово")

    # Гостевой код 8080 на шине: ИРПС+мышь (serial) и ММ8031+джойстик (parallel)
    add_test(NAME minigui_periph_serial
             COMMAND periph_guest_test ${KORVET_DATA_DIR}/rom/cpm22_shkomp.rom
                                       ${KORVET_DATA_DIR}/kdi/disk__system.kdi serial)
    set_tests_properties(minigui_periph_serial PROPERTIES
        PASS_REGULAR_EXPRESSION "ВСЕ ГостЕВЫЕ ПРОВЕРКИ ПРОЙДЕНЫ")
    add_test(NAME minigui_periph_parallel
             COMMAND periph_guest_test ${KORVET_DATA_DIR}/rom/cpm22_shkomp.rom
                                       ${KORVET_DATA_DIR}/kdi/disk__system.kdi parallel)
    set_tests_properties(minigui_periph_parallel PROPERTIES
        PASS_REGULAR_EXPRESSION "ВСЕ ГостЕВЫЕ ПРОВЕРКИ ПРОЙДЕНЫ")
endif()
