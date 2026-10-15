/**
 * @file timer_dump.hpp
 * @brief Функции дампа состояния таймера эмулятора
 * 
 * Содержит определения функций для создания дампов состояния
 * таймера эмулятора ПК8020 "Корвет" в файлы для отладки.
 * 
 * @author Korvet Team
 * @date 2024
 */

#pragma once

#include "timer.hpp"
#include <cstdio>

namespace korvet {

/**
 * @brief Класс для дампа состояния таймера
 * 
 * Предоставляет методы для создания дампов состояния
 * таймера i8253 и связанных с ним компонентов.
 */
class TimerDump {
public:
    /**
     * @brief Создать бинарный дамп состояния таймера
     * @param timer Ссылка на объект таймера
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_binary(const Timer& timer, const char* filename);

    /**
     * @brief Создать бинарный дамп состояния таймера с уникальным именем
     * @param timer Ссылка на объект таймера
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_binary_unique(const Timer& timer, const char* base_filename);

    /**
     * @brief Создать текстовый дамп состояния таймера
     * @param timer Ссылка на объект таймера
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_text(const Timer& timer, const char* filename);

    /**
     * @brief Создать текстовый дамп состояния таймера с уникальным именем
     * @param timer Ссылка на объект таймера
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_text_unique(const Timer& timer, const char* base_filename);

    /**
     * @brief Создать полный дамп таймера (регистры, каналы, состояние и т.д.)
     * @param timer Ссылка на объект таймера
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_full(const Timer& timer, const char* filename);

    /**
     * @brief Создать полный дамп таймера с уникальным именем
     * @param timer Ссылка на объект таймера
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_full_unique(const Timer& timer, const char* base_filename);

    /**
     * @brief Создать дамп каналов таймера (состояние каждого канала отдельно)
     * @param timer Ссылка на объект таймера
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_channels(const Timer& timer, const char* filename);

    /**
     * @brief Создать дамп каналов таймера с уникальным именем
     * @param timer Ссылка на объект таймера
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_channels_unique(const Timer& timer, const char* base_filename);

    /**
     * @brief Создать дамп состояния аудио-генератора таймера
     * @param timer Ссылка на объект таймера
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_audio_generator(const Timer& timer, const char* filename);

    /**
     * @brief Создать дамп состояния аудио-генератора таймера с уникальным именем
     * @param timer Ссылка на объект таймера
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_audio_generator_unique(const Timer& timer, const char* base_filename);

    /**
     * @brief Создать дамп состояния прерываний таймера
     * @param timer Ссылка на объект таймера
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_interrupts(const Timer& timer, const char* filename);

    /**
     * @brief Создать дамп состояния прерываний таймера с уникальным именем
     * @param timer Ссылка на объект таймера
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_interrupts_unique(const Timer& timer, const char* base_filename);
};

}  // namespace korvet