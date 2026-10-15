# CMakeLists.txt для тестирования интеграции таймера и аудио

cmake_minimum_required(VERSION 3.16)
project(timer_audio_test)

set(CMAKE_CXX_STANDARD 17)

# Включаем автоматическую обработку MOC, UIC и RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Добавляем директорию с заголовочными файлами
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# Определяем исходные файлы
set(SOURCES
    test_timer_audio.cpp
    test_timer_comprehensive.cpp
    test_timer_audio_sync.cpp
    timer.cpp
    ../audio/audio.cpp
    ../pic/pic.cpp
)

# Создаем исполняемый файл для теста
add_executable(timer_audio_test ${SOURCES})

# Линкуем с необходимыми библиотеками (если есть)
# target_link_libraries(timer_audio_test ...)