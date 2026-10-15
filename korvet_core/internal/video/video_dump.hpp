/**
 * @file video_dump.hpp
 * @brief Функции дампа состояния видеоподсистемы эмулятора
 * 
 * Содержит определения функций для создания дампов состояния
 * видеоподсистемы эмулятора ПК8020 "Корвет" в файлы для отладки.
 * 
 * @author Korvet Team
 * @date 2024
 */

#pragma once

#include "video.hpp"
#include <cstdio>

namespace korvet {

/**
 * @brief Класс для дампа состояния видеоподсистемы
 * 
 * Предоставляет методы для создания дампов видеопамяти,
 * состояния видеорегистров и других компонентов видеоподсистемы.
 */
class VideoDump {
public:
    /**
     * @brief Создать бинарный дамп видеопамяти GZU
     * @param video Ссылка на объект видео
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_gzu_binary(const Video& video, const char* filename);

    /**
     * @brief Создать бинарный дамп видеопамяти GZU с уникальным именем
     * @param video Ссылка на объект видео
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_gzu_binary_unique(const Video& video, const char* base_filename);

    /**
     * @brief Создать бинарный дамп текстовой видеопамяти ACZU
     * @param video Ссылка на объект видео
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_aczu_binary(const Video& video, const char* filename);

    /**
     * @brief Создать бинарный дамп текстовой видеопамяти ACZU с уникальным именем
     * @param video Ссылка на объект видео
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_aczu_binary_unique(const Video& video, const char* base_filename);

    /**
     * @brief Создать бинарный дамп видеокадра (рендеринг в битмап 512x256)
     * @param video Ссылка на объект видео
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_framebuffer_binary(const Video& video, const char* filename);

    /**
     * @brief Создать бинарный дамп видеокадра с уникальным именем
     * @param video Ссылка на объект видео
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_framebuffer_binary_unique(const Video& video, const char* base_filename);

    /**
     * @brief Создать текстовый дамп состояния видеорегистров
     * @param video Ссылка на объект видео
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_video_registers_text(const Video& video, const char* filename);

    /**
     * @brief Создать текстовый дамп состояния видеорегистров с уникальным именем
     * @param video Ссылка на объект видео
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_video_registers_text_unique(const Video& video, const char* base_filename);

    /**
     * @brief Создать дамп палитры (LUT)
     * @param video Ссылка на объект видео
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_palette_binary(const Video& video, const char* filename);

    /**
     * @brief Создать дамп палитры (LUT) с уникальным именем
     * @param video Ссылка на объект видео
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_palette_binary_unique(const Video& video, const char* base_filename);

    /**
     * @brief Создать текстовый дамп палитры (LUT)
     * @param video Ссылка на объект видео
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_palette_text(const Video& video, const char* filename);

    /**
     * @brief Создать текстовый дамп палитры (LUT) с уникальным именем
     * @param video Ссылка на объект видео
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_palette_text_unique(const Video& video, const char* base_filename);

    /**
     * @brief Создать текстовый дамп текущего состояния видео-подсистемы
     * @param video Ссылка на объект видео
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_video_state_text(const Video& video, const char* filename);

    /**
     * @brief Создать текстовый дамп текущего состояния видео-подсистемы с уникальным именем
     * @param video Ссылка на объект видео
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_video_state_text_unique(const Video& video, const char* base_filename);

    /**
     * @brief Создать полный дамп видео-подсистемы
     * @param video Ссылка на объект видео
     * @param base_filename Базовое имя файла (будет добавлен суффикс для каждого компонента)
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_full(const Video& video, const char* base_filename);

    /**
     * @brief Создать PNG скриншот текущего кадра
     * @param video Ссылка на объект видео
     * @param filename Имя файла для записи PNG
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_screenshot_png(const Video& video, const char* filename);

    /**
     * @brief Создать PNG скриншот текущего кадра с уникальным именем
     * @param video Ссылка на объект видео
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_screenshot_png_unique(const Video& video, const char* base_filename);
};

}  // namespace korvet