/**
 * @file fdc_dump.hpp
 * @brief Функции дампа состояния контроллера дисковода эмулятора
 * 
 * Содержит определения функций для создания дампов состояния
 * FDC эмулятора ПК8020 "Корвет" в файлы для отладки.
 * 
 * @author Korvet Team
 * @date 2024
 */

#pragma once

#include "fdc.hpp"
#include "../dump/dump_utils.hpp"

namespace korvet {

/**
 * @brief Класс для дампа состояния контроллера дисковода
 * 
 * Предоставляет методы для создания дампов состояния FDC
 * в различные форматы (бинарный, текстовый) для отладки.
 */
class FdcDump {
public:
    /**
     * @brief Создать бинарный дамп состояния FDC
     * @param fdc Ссылка на объект FDC
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_binary(const Fdc& fdc, const char* filename);

    /**
     * @brief Создать бинарный дамп состояния FDC с уникальным именем
     * @param fdc Ссылка на объект FDC
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_binary_unique(const Fdc& fdc, const char* base_filename);

    /**
     * @brief Создать текстовый дамп состояния FDC
     * @param fdc Ссылка на объект FDC
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_text(const Fdc& fdc, const char* filename);

    /**
     * @brief Создать текстовый дамп состояния FDC с уникальным именем
     * @param fdc Ссылка на объект FDC
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_text_unique(const Fdc& fdc, const char* base_filename);

    /**
     * @brief Создать полный дамп FDC (регистры, состояние дисков и т.д.)
     * @param fdc Ссылка на объект FDC
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_full(const Fdc& fdc, const char* filename);

    /**
     * @brief Создать полный дамп FDC с уникальным именем
     * @param fdc Ссылка на объект FDC
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_full_unique(const Fdc& fdc, const char* base_filename);
};

}  // namespace korvet