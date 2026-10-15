/**
 * @file memory_dump.hpp
 * @brief Функции дампа состояния памяти эмулятора
 * 
 * Содержит определения функций для создания дампов состояния
 * памяти эмулятора ПК8020 "Корвет" в файлы для отладки.
 * 
 * @author Korvet Team
 * @date 2024
 */

#pragma once

#include "memory.hpp"
#include <cstdio>

namespace korvet {

/**
 * @brief Класс для дампа состояния памяти
 * 
 * Предоставляет методы для создания дампов различных участков
 * памяти эмулятора в различные форматы (бинарный, текстовый) для отладки.
 */
class MemoryDump {
public:
    /**
     * @brief Создать бинарный дамп всей RAM
     * @param memory Ссылка на объект памяти
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_ram_binary(const Memory& memory, const char* filename);

    /**
     * @brief Создать бинарный дамп всей RAM с уникальным именем
     * @param memory Ссылка на объект памяти
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_ram_binary_unique(const Memory& memory, const char* base_filename);

    /**
     * @brief Создать текстовый дамп RAM в HEX-формате
     * @param memory Ссылка на объект памяти
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_ram_hex(const Memory& memory, const char* filename);

    /**
     * @brief Создать текстовый дамп RAM в HEX-формате с уникальным именем
     * @param memory Ссылка на объект памяти
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_ram_hex_unique(const Memory& memory, const char* base_filename);

    /**
     * @brief Создать текстовый дамп RAM в HEX-формате с адресами
     * @param memory Ссылка на объект памяти
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_ram_hex_with_addresses(const Memory& memory, const char* filename);

    /**
     * @brief Создать текстовый дамп RAM в HEX-формате с адресами и уникальным именем
     * @param memory Ссылка на объект памяти
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_ram_hex_with_addresses_unique(const Memory& memory, const char* base_filename);

    /**
     * @brief Создать бинарный дамп ROM
     * @param memory Ссылка на объект памяти
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_rom_binary(const Memory& memory, const char* filename);

    /**
     * @brief Создать бинарный дамп ROM с уникальным именем
     * @param memory Ссылка на объект памяти
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_rom_binary_unique(const Memory& memory, const char* base_filename);

    /**
     * @brief Создать текстовый дамп ROM в HEX-формате
     * @param memory Ссылка на объект памяти
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_rom_hex(const Memory& memory, const char* filename);

    /**
     * @brief Создать текстовый дамп ROM в HEX-формате с уникальным именем
     * @param memory Ссылка на объект памяти
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_rom_hex_unique(const Memory& memory, const char* base_filename);

    /**
     * @brief Создать текстовый дамп ROM в HEX-формате с адресами
     * @param memory Ссылка на объект памяти
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_rom_hex_with_addresses(const Memory& memory, const char* filename);

    /**
     * @brief Создать текстовый дамп ROM в HEX-формате с адресами и уникальным именем
     * @param memory Ссылка на объект памяти
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_rom_hex_with_addresses_unique(const Memory& memory, const char* base_filename);

    /**
     * @brief Создать бинарный дамп GZU (видеопамяти)
     * @param memory Ссылка на объект памяти
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_gzu_binary(const Memory& memory, const char* filename);

    /**
     * @brief Создать бинарный дамп GZU (видеопамяти) с уникальным именем
     * @param memory Ссылка на объект памяти
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_gzu_binary_unique(const Memory& memory, const char* base_filename);

    /**
     * @brief Создать бинарный дамп ACZU (текстовой видеопамяти)
     * @param memory Ссылка на объект памяти
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_aczu_binary(const Memory& memory, const char* filename);

    /**
     * @brief Создать бинарный дамп ACZU (текстовой видеопамяти) с уникальным именем
     * @param memory Ссылка на объект памяти
     * @param base_filename Базовое имя файла
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_aczu_binary_unique(const Memory& memory, const char* base_filename);

    /**
     * @brief Создать полный дамп всех участков памяти
     * @param memory Ссылка на объект памяти
     * @param base_filename Базовое имя файла (будет добавлен суффикс для каждого типа)
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_full(const Memory& memory, const char* base_filename);

    /**
     * @brief Создать дамп среза памяти по заданному адресу и размеру
     * @param memory Ссылка на объект памяти
     * @param start_addr Начальный адрес
     * @param size Размер среза
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_memory_slice_binary(const Memory& memory, uint16_t start_addr, uint16_t size, const char* filename);

    /**
     * @brief Создать текстовый дамп среза памяти по заданному адресу и размеру
     * @param memory Ссылка на объект памяти
     * @param start_addr Начальный адрес
     * @param size Размер среза
     * @param filename Имя файла для записи дампа
     * @return true в случае успеха, false при ошибке
     */
    static bool dump_memory_slice_hex(const Memory& memory, uint16_t start_addr, uint16_t size, const char* filename);
};

}  // namespace korvet