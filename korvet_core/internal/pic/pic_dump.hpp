/**
 * @file pic_dump.hpp
 * @brief Функции дампа состояния контроллера прерываний эмулятора
 * 
 * Содержит определения функций для создания дампов состояния
 * контроллера прерываний эмулятора ПК8020 "Корвет" в файлы для отладки.
 * 
 * @author Korvet Team
 * @date 2024
 */

#pragma once

#include "pic.hpp"
#include "../dump/dump_utils.hpp"

namespace korvet {

/**
 * @brief Класс для дампа состояния контроллера прерываний
 * 
 * Предоставляет методы для создания дампов состояния PIC
 * в различные форматы (бинарный, текстовый) для отладки.
 */
class PicDump {
public:
    /**
     * @brief Создать бинарный дамп состояния PIC
     * @param pic Ссылка на объект PIC
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_binary(const Pic& pic, const char* filename);

    /**
     * @brief Создать бинарный дамп состояния PIC с уникальным именем
     * @param pic Ссылка на объект PIC
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_binary_unique(const Pic& pic, const char* base_filename);

    /**
     * @brief Создать текстовый дамп состояния PIC
     * @param pic Ссылка на объект PIC
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_text(const Pic& pic, const char* filename);

    /**
     * @brief Создать текстовый дамп состояния PIC с уникальным именем
     * @param pic Ссылка на объект PIC
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_text_unique(const Pic& pic, const char* base_filename);

    /**
     * @brief Создать полный дамп PIC (регистры, маски и т.д.)
     * @param pic Ссылка на объект PIC
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_full(const Pic& pic, const char* filename);

    /**
     * @brief Создать полный дамп PIC с уникальным именем
     * @param pic Ссылка на объект PIC
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_full_unique(const Pic& pic, const char* base_filename);
};

}  // namespace korvet