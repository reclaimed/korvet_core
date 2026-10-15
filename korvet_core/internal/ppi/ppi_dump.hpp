/**
 * @file ppi_dump.hpp
 * @brief Функции дампа состояния программируемого периферийного интерфейса эмулятора
 * 
 * Содержит определения функций для создания дампов состояния
 * PPI эмулятора ПК8020 "Корвет" в файлы для отладки.
 * 
 * @author Korvet Team
 * @date 2024
 */

#pragma once

#include "ppi.hpp"
#include "../dump/dump_utils.hpp"

namespace korvet {

/**
 * @brief Класс для дампа состояния программируемого периферийного интерфейса
 * 
 * Предоставляет методы для создания дампов состояния PPI
 * в различные форматы (бинарный, текстовый) для отладки.
 */
class PpiDump {
public:
    /**
     * @brief Создать бинарный дамп состояния PPI
     * @param ppi Ссылка на объект PPI
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_binary(const Ppi& ppi, const char* filename);

    /**
     * @brief Создать бинарный дамп состояния PPI с уникальным именем
     * @param ppi Ссылка на объект PPI
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_binary_unique(const Ppi& ppi, const char* base_filename);

    /**
     * @brief Создать текстовый дамп состояния PPI
     * @param ppi Ссылка на объект PPI
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_text(const Ppi& ppi, const char* filename);

    /**
     * @brief Создать текстовый дамп состояния PPI с уникальным именем
     * @param ppi Ссылка на объект PPI
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_text_unique(const Ppi& ppi, const char* base_filename);

    /**
     * @brief Создать полный дамп PPI (все регистры, состояния портов и т.д.)
     * @param ppi Ссылка на объект PPI
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_full(const Ppi& ppi, const char* filename);

    /**
     * @brief Создать полный дамп PPI с уникальным именем
     * @param ppi Ссылка на объект PPI
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_full_unique(const Ppi& ppi, const char* base_filename);
};

}  // namespace korvet