/**
 * @file cpu_dump.hpp
 * @brief Функции дампа состояния процессора КР580ВМ80А
 * 
 * Содержит определения функций для создания дампов состояния
 * процессора КР580ВМ80А в файлы для отладки.
 * 
 * @author Korvet Team
 * @date 2024
 */

#pragma once

#include "cpu.hpp"
#include <cstdio>

namespace korvet {

/**
 * @brief Класс для дампа состояния процессора
 * 
 * Предоставляет методы для создания дампов состояния процессора
 * в различные форматы (бинарный, текстовый) для отладки.
 */
class CpuDump {
public:
    /**
     * @brief Создать бинарный дамп состояния процессора
     * @param cpu Ссылка на объект процессора
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_binary(const Cpu& cpu, const char* filename);

    /**
     * @brief Создать бинарный дамп состояния процессора с уникальным именем
     * @param cpu Ссылка на объект процессора
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_binary_unique(const Cpu& cpu, const char* base_filename);

    /**
     * @brief Создать текстовый дамп состояния процессора
     * @param cpu Ссылка на объект процессора
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_text(const Cpu& cpu, const char* filename);

    /**
     * @brief Создать текстовый дамп состояния процессора с уникальным именем
     * @param cpu Ссылка на объект процессора
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_text_unique(const Cpu& cpu, const char* base_filename);

    /**
     * @brief Создать полный дамп процессора (регистры, флаги и т.д.)
     * @param cpu Ссылка на объект процессора
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_full(const Cpu& cpu, const char* filename);

    /**
     * @brief Создать полный дамп процессора с уникальным именем
     * @param cpu Ссылка на объект процессора
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_full_unique(const Cpu& cpu, const char* base_filename);
};

}  // namespace korvet