/**
 * @file memory_dump.cpp
 * @brief Реализация функций дампа состояния памяти эмулятора
 * 
 * Содержит реализацию функций для создания дампов состояния
 * памяти эмулятора ПК8020 "Корвет" в файлы для отладки.
 * 
 * @author Korvet Team
 * @date 2024
 */

#include "memory_dump.hpp"
#include <cstdio>
#include <ctime>
#include <cstdarg>

namespace korvet {

bool MemoryDump::dump_ram_binary(const Memory& memory, const char* filename) {
    const uint8_t* ram = memory.get_ram();
    FILE* file = fopen(filename, "wb");
    if (!file) {
        return false;
    }
    
    size_t written = fwrite(ram, 1, Memory::RAM_SIZE, file);
    fclose(file);
    
    return written == Memory::RAM_SIZE;
}

bool MemoryDump::dump_ram_binary_unique(const Memory& memory, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_ram_%04d%02d%02d_%02d%02d%02d.bin",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_ram_binary(memory, unique_filename);
}

bool MemoryDump::dump_ram_hex(const Memory& memory, const char* filename) {
    const uint8_t* ram = memory.get_ram();
    FILE* file = fopen(filename, "w");
    if (!file) {
        return false;
    }
    
    fprintf(file, "RAM State Dump (HEX)\n");
    fprintf(file, "=====================\n");
    
    for (uint32_t i = 0; i < Memory::RAM_SIZE; i += 16) {
        fprintf(file, "%04X: ", i);
        
        // Печать шестнадцатеричных значений
        for (int j = 0; j < 16; j++) {
            if (i + j < Memory::RAM_SIZE) {
                fprintf(file, "%02X ", ram[i + j]);
            } else {
                fprintf(file, "   ");
            }
        }
        
        fprintf(file, "| ");
        
        // Печать ASCII-значений
        for (int j = 0; j < 16; j++) {
            if (i + j < Memory::RAM_SIZE) {
                uint8_t byte = ram[i + j];
                if (byte >= 32 && byte <= 126) {
                    fprintf(file, "%c", byte);
                } else {
                    fprintf(file, ".");
                }
            } else {
                fprintf(file, " ");
            }
        }
        
        fprintf(file, "\n");
    }
    
    fclose(file);
    return true;
}

bool MemoryDump::dump_ram_hex_unique(const Memory& memory, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_ram_%04d%02d%02d_%02d%02d%02d_hex.txt",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_ram_hex(memory, unique_filename);
}

bool MemoryDump::dump_ram_hex_with_addresses(const Memory& memory, const char* filename) {
    const uint8_t* ram = memory.get_ram();
    FILE* file = fopen(filename, "w");
    if (!file) {
        return false;
    }
    
    fprintf(file, "RAM State Dump with Addresses (HEX)\n");
    fprintf(file, "=====================================\n");
    fprintf(file, "Total RAM Size: %d bytes (0x%X)\n\n", Memory::RAM_SIZE, Memory::RAM_SIZE);
    
    for (uint32_t i = 0; i < Memory::RAM_SIZE; i += 16) {
        fprintf(file, "0x%04X: ", i);
        
        // Печать шестнадцатеричных значений
        for (int j = 0; j < 16; j++) {
            if (i + j < Memory::RAM_SIZE) {
                fprintf(file, "%02X ", ram[i + j]);
            } else {
                fprintf(file, "   ");
            }
        }
        
        fprintf(file, "| ");
        
        // Печать ASCII-значений
        for (int j = 0; j < 16; j++) {
            if (i + j < Memory::RAM_SIZE) {
                uint8_t byte = ram[i + j];
                if (byte >= 32 && byte <= 126) {
                    fprintf(file, "%c", byte);
                } else {
                    fprintf(file, ".");
                }
            } else {
                fprintf(file, " ");
            }
        }
        
        fprintf(file, "\n");
    }
    
    fclose(file);
    return true;
}

bool MemoryDump::dump_ram_hex_with_addresses_unique(const Memory& memory, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_ram_%04d%02d%02d_%02d%02d%02d_hex_addr.txt",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_ram_hex_with_addresses(memory, unique_filename);
}

bool MemoryDump::dump_rom_binary(const Memory& memory, const char* filename) {
    const uint8_t* rom = memory.get_rom();
    FILE* file = fopen(filename, "wb");
    if (!file) {
        return false;
    }
    
    size_t rom_size = memory.get_rom_size();
    size_t written = fwrite(rom, 1, rom_size, file);
    fclose(file);
    
    return written == rom_size;
}

bool MemoryDump::dump_rom_binary_unique(const Memory& memory, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_rom_%04d%02d%02d_%02d%02d%02d.bin",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_rom_binary(memory, unique_filename);
}

bool MemoryDump::dump_rom_hex(const Memory& memory, const char* filename) {
    const uint8_t* rom = memory.get_rom();
    size_t rom_size = memory.get_rom_size();
    FILE* file = fopen(filename, "w");
    if (!file) {
        return false;
    }
    
    fprintf(file, "ROM State Dump (HEX)\n");
    fprintf(file, "=====================\n");
    fprintf(file, "Total ROM Size: %zu bytes (0x%zX)\n\n", rom_size, rom_size);
    
    for (size_t i = 0; i < rom_size; i += 16) {
        fprintf(file, "%04zX: ", i);
        
        // Печать шестнадцатеричных значений
        for (size_t j = 0; j < 16 && (i + j) < rom_size; j++) {
            fprintf(file, "%02X ", rom[(i + j)]);
        }
        
        // Дополнение пробелами, если осталось <16 байт
        for (size_t j = (i + 15 < rom_size ? 15 : rom_size - i); j < 15; j++) {
            fprintf(file, "   ");
        }
        
        fprintf(file, "| ");
        
        // Печать ASCII-значений
        for (size_t j = 0; j < 16 && (i + j) < rom_size; j++) {
            uint8_t byte = rom[(i + j)];
            if (byte >= 32 && byte <= 126) {
                fprintf(file, "%c", byte);
            } else {
                fprintf(file, ".");
            }
        }
        
        fprintf(file, "\n");
    }
    
    fclose(file);
    return true;
}

bool MemoryDump::dump_rom_hex_unique(const Memory& memory, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_rom_%04d%02d%02d_%02d%02d%02d_hex.txt",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_rom_hex(memory, unique_filename);
}

bool MemoryDump::dump_rom_hex_with_addresses(const Memory& memory, const char* filename) {
    const uint8_t* rom = memory.get_rom();
    size_t rom_size = memory.get_rom_size();
    FILE* file = fopen(filename, "w");
    if (!file) {
        return false;
    }
    
    fprintf(file, "ROM State Dump with Addresses (HEX)\n");
    fprintf(file, "=====================================\n");
    fprintf(file, "Total ROM Size: %zu bytes (0x%zX)\n\n", rom_size, rom_size);
    
    for (size_t i = 0; i < rom_size; i += 16) {
        fprintf(file, "0x%04zX: ", i);
        
        // Печать шестнадцатеричных значений
        for (size_t j = 0; j < 16 && (i + j) < rom_size; j++) {
            fprintf(file, "%02X ", rom[(i + j)]);
        }
        
        // Дополнение пробелами, если осталось <16 байт
        for (size_t j = (i + 15 < rom_size ? 15 : rom_size - i); j < 15; j++) {
            fprintf(file, "   ");
        }
        
        fprintf(file, "| ");
        
        // Печать ASCII-значений
        for (size_t j = 0; j < 16 && (i + j) < rom_size; j++) {
            uint8_t byte = rom[(i + j)];
            if (byte >= 32 && byte <= 126) {
                fprintf(file, "%c", byte);
            } else {
                fprintf(file, ".");
            }
        }
        
        fprintf(file, "\n");
    }
    
    fclose(file);
    return true;
}

bool MemoryDump::dump_rom_hex_with_addresses_unique(const Memory& memory, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_rom_%04d%02d%02d_%02d%02d%02d_hex_addr.txt",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_rom_hex_with_addresses(memory, unique_filename);
}

bool MemoryDump::dump_gzu_binary(const Memory& memory, const char* filename) {
    // Получаем память GZU у объекта memory
    const uint8_t* gzu = memory.get_gzu();
    FILE* file = fopen(filename, "wb");
    if (!file) {
        return false;
    }
    
    size_t gzu_size = memory.get_gzu_size();
    size_t written = fwrite(gzu, 1, gzu_size, file);
    fclose(file);
    
    return written == gzu_size;
}

bool MemoryDump::dump_gzu_binary_unique(const Memory& memory, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_gzu_%04d%02d%02d_%02d%02d%02d.bin",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_gzu_binary(memory, unique_filename);
}

bool MemoryDump::dump_aczu_binary(const Memory& memory, const char* filename) {
    // Получаем память ACZU у объекта memory
    const uint8_t* aczu = memory.get_aczu();
    FILE* file = fopen(filename, "wb");
    if (!file) {
        return false;
    }
    
    size_t aczu_size = memory.get_aczu_size();
    size_t written = fwrite(aczu, 1, aczu_size, file);
    fclose(file);
    
    return written == aczu_size;
}

bool MemoryDump::dump_aczu_binary_unique(const Memory& memory, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_aczu_%04d%02d%02d_%02d%02d%02d.bin",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_aczu_binary(memory, unique_filename);
}

bool MemoryDump::dump_full(const Memory& memory, const char* base_filename) {
    bool success = true;
    
    // Отдельные имена файлов для каждого компонента
    char ram_filename[512];
    char rom_filename[512];
    char gzu_filename[512];
    char aczu_filename[512];
    
    // Уникальные имена файлов для каждого компонента
    snprintf(ram_filename, sizeof(ram_filename), "%s_ram", base_filename);
    snprintf(rom_filename, sizeof(rom_filename), "%s_rom", base_filename);
    snprintf(gzu_filename, sizeof(gzu_filename), "%s_gzu", base_filename);
    snprintf(aczu_filename, sizeof(aczu_filename), "%s_aczu", base_filename);
    
    // Дамп каждого компонента
    success &= dump_ram_binary_unique(memory, ram_filename);
    success &= dump_rom_binary_unique(memory, rom_filename);
    success &= dump_gzu_binary_unique(memory, gzu_filename);
    success &= dump_aczu_binary_unique(memory, aczu_filename);
    
    return success;
}

bool MemoryDump::dump_memory_slice_binary(const Memory& memory, uint16_t start_addr, uint16_t size, const char* filename) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        return false;
    }
    
    // Пишем срез памяти
    for (uint16_t i = 0; i < size; i++) {
        uint16_t addr = start_addr + i;
        uint8_t byte = memory.read_byte(addr);
        size_t written = fwrite(&byte, 1, 1, file);
        if (written != 1) {
            fclose(file);
            return false;
        }
    }
    
    fclose(file);
    return true;
}

bool MemoryDump::dump_memory_slice_hex(const Memory& memory, uint16_t start_addr, uint16_t size, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        return false;
    }
    
    fprintf(file, "Memory Slice Dump (HEX)\n");
    fprintf(file, "========================\n");
    fprintf(file, "Start Address: 0x%04X\n", start_addr);
    fprintf(file, "Size: %d bytes (0x%X)\n\n", size, size);
    
    for (uint16_t i = 0; i < size; i += 16) {
        fprintf(file, "0x%04X: ", start_addr + i);
        
        // Печать шестнадцатеричных значений
        for (int j = 0; j < 16 && (i + j) < size; j++) {
            uint16_t addr = start_addr + i + j;
            uint8_t byte = memory.read_byte(addr);
            fprintf(file, "%02X ", byte);
        }
        
        // Дополнение пробелами, если осталось <16 байт
        for (int j = ((i + 15 < size) ? 15 : size - i); j < 15; j++) {
            fprintf(file, "   ");
        }
        
        fprintf(file, "| ");
        
        // Печать ASCII-значений
        for (int j = 0; j < 16 && (i + j) < size; j++) {
            uint16_t addr = start_addr + i + j;
            uint8_t byte = memory.read_byte(addr);
            if (byte >= 32 && byte <= 126) {
                fprintf(file, "%c", byte);
            } else {
                fprintf(file, ".");
            }
        }
        
        fprintf(file, "\n");
    }
    
    fclose(file);
    return true;
}

}  // namespace korvet