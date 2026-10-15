/**
 * @file ppi_dump.cpp
 * @brief Реализация функций дампа состояния программируемого периферийного интерфейса эмулятора
 * 
 * Содержит реализацию функций для создания дампов состояния
 * PPI эмулятора ПК8020 "Корвет" в файлы для отладки.
 * 
 * @author Korvet Team
 * @date 2024
 */

#include "ppi_dump.hpp"
#include <cstdio>
#include <ctime>

namespace korvet {

bool PpiDump::dump_binary(const Ppi& ppi, const char* filename) {
    // Пока — заглушечная реализация
    // Полная реализация сериализовала бы состояние PPI
    uint8_t placeholder[256]; // Placeholder for PPI state
    memset(placeholder, 0, sizeof(placeholder));
    
    return DumpUtils::write_binary_file(filename, placeholder, sizeof(placeholder));
}

bool PpiDump::dump_binary_unique(const Ppi& ppi, const char* base_filename) {
    uint8_t placeholder[256]; // Placeholder for PPI state
    memset(placeholder, 0, sizeof(placeholder));
    
    return DumpUtils::write_binary_file_unique(base_filename, placeholder, sizeof(placeholder));
}

bool PpiDump::dump_text(const Ppi& ppi, const char* filename) {
    return DumpUtils::write_text_file(filename,
        "PPI DUMP\n"
        "========\n"
        "Timestamp: %ld\n"
        "\n"
        "Programmable Peripheral Interface State:\n"
        "  PPI1 (Video/Disk): Not implemented in dump yet\n"
        "  PPI2 (Tape/Printer): Not implemented in dump yet\n"
        "  PPI3 (Expansion): Not implemented in dump yet\n"
        "\n"
        "Note: Detailed PPI state dumping not yet implemented\n",
        std::time(nullptr));
}

bool PpiDump::dump_text_unique(const Ppi& ppi, const char* base_filename) {
    return DumpUtils::write_text_file_unique(base_filename,
        "PPI DUMP\n"
        "========\n"
        "Timestamp: %ld\n"
        "\n"
        "Programmable Peripheral Interface State:\n"
        "  PPI1 (Video/Disk): Not implemented in dump yet\n"
        "  PPI2 (Tape/Printer): Not implemented in dump yet\n"
        "  PPI3 (Expansion): Not implemented in dump yet\n"
        "\n"
        "Note: Detailed PPI state dumping not yet implemented\n",
        std::time(nullptr));
}

bool PpiDump::dump_full(const Ppi& ppi, const char* filename) {
    return DumpUtils::write_text_file(filename,
        "FULL PPI DUMP\n"
        "=============\n"
        "Timestamp: %ld\n"
        "\n"
        "PPI Configuration:\n"
        "\n"
        "PPI1 (Video/Disk Control - Addresses PBASE+38h-3Bh):\n"
        "  Port A (PIA1): Not implemented in dump yet\n"
        "  Port B (PIB1 - DRVREG): Not implemented in dump yet\n"
        "  Port C (PIC1 - VIREG): Not implemented in dump yet\n"
        "  Control Word (PIMC1): Not implemented in dump yet\n"
        "\n"
        "PPI2 (Tape/Printer - Addresses PBASE+30h-33h):\n"
        "  Port A (PIA2 - LSTDAT): Not implemented in dump yet\n"
        "  Port B (PIB2): Not implemented in dump yet\n"
        "  Port C (PIC2): Not implemented in dump yet\n"
        "  Control Word (PIMC2): Not implemented in dump yet\n"
        "\n"
        "PPI3 (Expansion Port - Addresses PBASE+08h-0Bh):\n"
        "  Port A (PIA3): Not implemented in dump yet\n"
        "  Port B (PIB3): Not implemented in dump yet\n"
        "  Port C (PIC3): Not implemented in dump yet\n"
        "  Control Word (PIMC3): Not implemented in dump yet\n"
        "\n"
        "Detailed Register States:\n"
        "  VIREG (Video Register): Not implemented in dump yet\n"
        "  NCREG (Color Register): Not implemented in dump yet\n"
        "  DRVREG (Drive Register): Not implemented in dump yet\n",
        std::time(nullptr));
}

bool PpiDump::dump_full_unique(const Ppi& ppi, const char* base_filename) {
    return DumpUtils::write_text_file_unique(base_filename,
        "FULL PPI DUMP\n"
        "=============\n"
        "Timestamp: %ld\n"
        "\n"
        "PPI Configuration:\n"
        "\n"
        "PPI1 (Video/Disk Control - Addresses PBASE+38h-3Bh):\n"
        "  Port A (PIA1): Not implemented in dump yet\n"
        "  Port B (PIB1 - DRVREG): Not implemented in dump yet\n"
        "  Port C (PIC1 - VIREG): Not implemented in dump yet\n"
        "  Control Word (PIMC1): Not implemented in dump yet\n"
        "\n"
        "PPI2 (Tape/Printer - Addresses PBASE+30h-33h):\n"
        "  Port A (PIA2 - LSTDAT): Not implemented in dump yet\n"
        "  Port B (PIB2): Not implemented in dump yet\n"
        "  Port C (PIC2): Not implemented in dump yet\n"
        "  Control Word (PIMC2): Not implemented in dump yet\n"
        "\n"
        "PPI3 (Expansion Port - Addresses PBASE+08h-0Bh):\n"
        "  Port A (PIA3): Not implemented in dump yet\n"
        "  Port B (PIB3): Not implemented in dump yet\n"
        "  Port C (PIC3): Not implemented in dump yet\n"
        "  Control Word (PIMC3): Not implemented in dump yet\n"
        "\n"
        "Detailed Register States:\n"
        "  VIREG (Video Register): Not implemented in dump yet\n"
        "  NCREG (Color Register): Not implemented in dump yet\n"
        "  DRVREG (Drive Register): Not implemented in dump yet\n",
        std::time(nullptr));
}

}  // namespace korvet