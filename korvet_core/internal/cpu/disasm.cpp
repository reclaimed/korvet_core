/**
 * @file disasm.cpp
 * @brief Реализация дизассемблера процессора i8080 для отладки
 * 
 * Содержит реализацию класса Disasm, который реализует
 * дизассемблирование инструкций процессора i8080 для отладки.
 * 
 * @author Korvet Team
 * @date 2024
 */

#include "disasm.hpp"
#include <cstdio>
#include <cstring>
#include <cctype>

namespace korvet {

// Статическая инициализация таблицы опкодов
const Disasm::opcode_entry_t Disasm::opcode_table_[256] = {
    // 0x00-0x0F
    {0x00, "NOP", 1, 4, 0, 0},     // No operation
    {0x01, "LXI B,", 3, 10, 2, 0}, // Load immediate to BC
    {0x02, "STAX B", 1, 7, 0, 0},  // Store accumulator at address in BC
    {0x03, "INX B", 1, 5, 0, 0},   // Increment BC
    {0x04, "INR B", 1, 5, 0, 0},   // Increment register B
    {0x05, "DCR B", 1, 5, 0, 0},   // Decrement register B
    {0x06, "MVI B,", 2, 7, 1, 0},  // Move immediate to register B
    {0x07, "RLC", 1, 4, 0, 0},     // Rotate left accumulator
    {0x08, "NOP", 1, 4, 0, 0},     // Undocumented NOP
    {0x09, "DAD B", 1, 10, 0, 0},  // Add BC to HL
    {0x0A, "LDAX B", 1, 7, 0, 0},  // Load accumulator from address in BC
    {0x0B, "DCX B", 1, 5, 0, 0},   // Decrement BC
    {0x0C, "INR C", 1, 5, 0, 0},   // Increment register C
    {0x0D, "DCR C", 1, 5, 0, 0},   // Decrement register C
    {0x0E, "MVI C,", 2, 7, 1, 0},  // Move immediate to register C
    {0x0F, "RRC", 1, 4, 0, 0},     // Rotate right accumulator

    // 0x10-0x1F
    {0x10, "NOP", 1, 4, 0, 0},     // Undocumented NOP
    {0x11, "LXI D,", 3, 10, 2, 0}, // Load immediate to DE
    {0x12, "STAX D", 1, 7, 0, 0},  // Store accumulator at address in DE
    {0x13, "INX D", 1, 5, 0, 0},   // Increment DE
    {0x14, "INR D", 1, 5, 0, 0},   // Increment register D
    {0x15, "DCR D", 1, 5, 0, 0},   // Decrement register D
    {0x16, "MVI D,", 2, 7, 1, 0},  // Move immediate to register D
    {0x17, "RAL", 1, 4, 0, 0},     // Rotate left through carry
    {0x18, "NOP", 1, 4, 0, 0},     // Undocumented NOP
    {0x19, "DAD D", 1, 10, 0, 0},  // Add DE to HL
    {0x1A, "LDAX D", 1, 7, 0, 0},  // Load accumulator from address in DE
    {0x1B, "DCX D", 1, 5, 0, 0},   // Decrement DE
    {0x1C, "INR E", 1, 5, 0, 0},   // Increment register E
    {0x1D, "DCR E", 1, 5, 0, 0},   // Decrement register E
    {0x1E, "MVI E,", 2, 7, 1, 0},  // Move immediate to register E
    {0x1F, "RAR", 1, 4, 0, 0},     // Rotate right through carry

    // 0x20-0x2F
    {0x20, "RIM", 1, 4, 0, 0},     // Read interrupt mask
    {0x21, "LXI H,", 3, 10, 2, 0}, // Load immediate to HL
    {0x22, "SHLD", 3, 16, 2, 0},   // Store HL direct
    {0x23, "INX H", 1, 5, 0, 0},   // Increment HL
    {0x24, "INR H", 1, 5, 0, 0},   // Increment register H
    {0x25, "DCR H", 1, 5, 0, 0},   // Decrement register H
    {0x26, "MVI H,", 2, 7, 1, 0},  // Move immediate to register H
    {0x27, "DAA", 1, 4, 0, 0},     // Decimal adjust accumulator
    {0x28, "NOP", 1, 4, 0, 0},     // Undocumented NOP
    {0x29, "DAD H", 1, 10, 0, 0},  // Add HL to HL
    {0x2A, "LHLD", 3, 16, 2, 0},   // Load HL direct
    {0x2B, "DCX H", 1, 5, 0, 0},   // Decrement HL
    {0x2C, "INR L", 1, 5, 0, 0},   // Increment register L
    {0x2D, "DCR L", 1, 5, 0, 0},   // Decrement register L
    {0x2E, "MVI L,", 2, 7, 1, 0},  // Move immediate to register L
    {0x2F, "CMA", 1, 4, 0, 0},     // Complement accumulator

    // 0x30-0x3F
    {0x30, "SIM", 1, 4, 0, 0},     // Serial interrupt mask
    {0x31, "LXI SP,", 3, 10, 2, 0}, // Load immediate to SP
    {0x32, "STA", 3, 13, 2, 0},    // Store accumulator direct
    {0x33, "INX SP", 1, 5, 0, 0},  // Increment SP
    {0x34, "INR M", 1, 10, 0, 0},  // Increment memory
    {0x35, "DCR M", 1, 10, 0, 0},  // Decrement memory
    {0x36, "MVI M,", 2, 10, 1, 0}, // Move immediate to memory
    {0x37, "STC", 1, 4, 0, 0},     // Set carry
    {0x38, "NOP", 1, 4, 0, 0},     // Undocumented NOP
    {0x39, "DAD SP", 1, 10, 0, 0}, // Add SP to HL
    {0x3A, "LDA", 3, 13, 2, 0},    // Load accumulator direct
    {0x3B, "DCX SP", 1, 5, 0, 0},  // Decrement SP
    {0x3C, "INR A", 1, 5, 0, 0},   // Increment register A
    {0x3D, "DCR A", 1, 5, 0, 0},   // Decrement register A
    {0x3E, "MVI A,", 2, 7, 1, 0},  // Move immediate to register A
    {0x3F, "CMC", 1, 4, 0, 0},     // Complement carry

    // 0x40-0x4F
    {0x40, "MOV B,B", 1, 5, 0, 0}, {0x41, "MOV B,C", 1, 5, 0, 0},
    {0x42, "MOV B,D", 1, 5, 0, 0}, {0x43, "MOV B,E", 1, 5, 0, 0},
    {0x44, "MOV B,H", 1, 5, 0, 0}, {0x45, "MOV B,L", 1, 5, 0, 0},
    {0x46, "MOV B,M", 1, 7, 0, 0}, {0x47, "MOV B,A", 1, 5, 0, 0},
    {0x48, "MOV C,B", 1, 5, 0, 0}, {0x49, "MOV C,C", 1, 5, 0, 0},
    {0x4A, "MOV C,D", 1, 5, 0, 0}, {0x4B, "MOV C,E", 1, 5, 0, 0},
    {0x4C, "MOV C,H", 1, 5, 0, 0}, {0x4D, "MOV C,L", 1, 5, 0, 0},
    {0x4E, "MOV C,M", 1, 7, 0, 0}, {0x4F, "MOV C,A", 1, 5, 0, 0},

    // 0x50-0x5F
    {0x50, "MOV D,B", 1, 5, 0, 0}, {0x51, "MOV D,C", 1, 5, 0, 0},
    {0x52, "MOV D,D", 1, 5, 0, 0}, {0x53, "MOV D,E", 1, 5, 0, 0},
    {0x54, "MOV D,H", 1, 5, 0, 0}, {0x55, "MOV D,L", 1, 5, 0, 0},
    {0x56, "MOV D,M", 1, 7, 0, 0}, {0x57, "MOV D,A", 1, 5, 0, 0},
    {0x58, "MOV E,B", 1, 5, 0, 0}, {0x59, "MOV E,C", 1, 5, 0, 0},
    {0x5A, "MOV E,D", 1, 5, 0, 0}, {0x5B, "MOV E,E", 1, 5, 0, 0},
    {0x5C, "MOV E,H", 1, 5, 0, 0}, {0x5D, "MOV E,L", 1, 5, 0, 0},
    {0x5E, "MOV E,M", 1, 7, 0, 0}, {0x5F, "MOV E,A", 1, 5, 0, 0},

    // 0x60-0x6F
    {0x60, "MOV H,B", 1, 5, 0, 0}, {0x61, "MOV H,C", 1, 5, 0, 0},
    {0x62, "MOV H,D", 1, 5, 0, 0}, {0x63, "MOV H,E", 1, 5, 0, 0},
    {0x64, "MOV H,H", 1, 5, 0, 0}, {0x65, "MOV H,L", 1, 5, 0, 0},
    {0x66, "MOV H,M", 1, 7, 0, 0}, {0x67, "MOV H,A", 1, 5, 0, 0},
    {0x68, "MOV L,B", 1, 5, 0, 0}, {0x69, "MOV L,C", 1, 5, 0, 0},
    {0x6A, "MOV L,D", 1, 5, 0, 0}, {0x6B, "MOV L,E", 1, 5, 0, 0},
    {0x6C, "MOV L,H", 1, 5, 0, 0}, {0x6D, "MOV L,L", 1, 5, 0, 0},
    {0x6E, "MOV L,M", 1, 7, 0, 0}, {0x6F, "MOV L,A", 1, 5, 0, 0},

    // 0x70-0x7F
    {0x70, "MOV M,B", 1, 7, 0, 0}, {0x71, "MOV M,C", 1, 7, 0, 0},
    {0x72, "MOV M,D", 1, 7, 0, 0}, {0x73, "MOV M,E", 1, 7, 0, 0},
    {0x74, "MOV M,H", 1, 7, 0, 0}, {0x75, "MOV M,L", 1, 7, 0, 0},
    {0x76, "HLT", 1, 7, 0, 0},     {0x77, "MOV M,A", 1, 7, 0, 0},
    {0x78, "MOV A,B", 1, 5, 0, 0}, {0x79, "MOV A,C", 1, 5, 0, 0},
    {0x7A, "MOV A,D", 1, 5, 0, 0}, {0x7B, "MOV A,E", 1, 5, 0, 0},
    {0x7C, "MOV A,H", 1, 5, 0, 0}, {0x7D, "MOV A,L", 1, 5, 0, 0},
    {0x7E, "MOV A,M", 1, 7, 0, 0}, {0x7F, "MOV A,A", 1, 5, 0, 0},

    // 0x80-0x8F
    {0x80, "ADD B", 1, 4, 0, 0}, {0x81, "ADD C", 1, 4, 0, 0},
    {0x82, "ADD D", 1, 4, 0, 0}, {0x83, "ADD E", 1, 4, 0, 0},
    {0x84, "ADD H", 1, 4, 0, 0}, {0x85, "ADD L", 1, 4, 0, 0},
    {0x86, "ADD M", 1, 7, 0, 0}, {0x87, "ADD A", 1, 4, 0, 0},
    {0x88, "ADC B", 1, 4, 0, 0}, {0x89, "ADC C", 1, 4, 0, 0},
    {0x8A, "ADC D", 1, 4, 0, 0}, {0x8B, "ADC E", 1, 4, 0, 0},
    {0x8C, "ADC H", 1, 4, 0, 0}, {0x8D, "ADC L", 1, 4, 0, 0},
    {0x8E, "ADC M", 1, 7, 0, 0}, {0x8F, "ADC A", 1, 4, 0, 0},

    // 0x90-0x9F
    {0x90, "SUB B", 1, 4, 0, 0}, {0x91, "SUB C", 1, 4, 0, 0},
    {0x92, "SUB D", 1, 4, 0, 0}, {0x93, "SUB E", 1, 4, 0, 0},
    {0x94, "SUB H", 1, 4, 0, 0}, {0x95, "SUB L", 1, 4, 0, 0},
    {0x96, "SUB M", 1, 7, 0, 0}, {0x97, "SUB A", 1, 4, 0, 0},
    {0x98, "SBB B", 1, 4, 0, 0}, {0x99, "SBB C", 1, 4, 0, 0},
    {0x9A, "SBB D", 1, 4, 0, 0}, {0x9B, "SBB E", 1, 4, 0, 0},
    {0x9C, "SBB H", 1, 4, 0, 0}, {0x9D, "SBB L", 1, 4, 0, 0},
    {0x9E, "SBB M", 1, 7, 0, 0}, {0x9F, "SBB A", 1, 4, 0, 0},

    // 0xA0-0xAF
    {0xA0, "ANA B", 1, 4, 0, 0}, {0xA1, "ANA C", 1, 4, 0, 0},
    {0xA2, "ANA D", 1, 4, 0, 0}, {0xA3, "ANA E", 1, 4, 0, 0},
    {0xA4, "ANA H", 1, 4, 0, 0}, {0xA5, "ANA L", 1, 4, 0, 0},
    {0xA6, "ANA M", 1, 7, 0, 0}, {0xA7, "ANA A", 1, 4, 0, 0},
    {0xA8, "XRA B", 1, 4, 0, 0}, {0xA9, "XRA C", 1, 4, 0, 0},
    {0xAA, "XRA D", 1, 4, 0, 0}, {0xAB, "XRA E", 1, 4, 0, 0},
    {0xAC, "XRA H", 1, 4, 0, 0}, {0xAD, "XRA L", 1, 4, 0, 0},
    {0xAE, "XRA M", 1, 7, 0, 0}, {0xAF, "XRA A", 1, 4, 0, 0},

    // 0xB0-0xBF
    {0xB0, "ORA B", 1, 4, 0, 0}, {0xB1, "ORA C", 1, 4, 0, 0},
    {0xB2, "ORA D", 1, 4, 0, 0}, {0xB3, "ORA E", 1, 4, 0, 0},
    {0xB4, "ORA H", 1, 4, 0, 0}, {0xB5, "ORA L", 1, 4, 0, 0},
    {0xB6, "ORA M", 1, 7, 0, 0}, {0xB7, "ORA A", 1, 4, 0, 0},
    {0xB8, "CMP B", 1, 4, 0, 0}, {0xB9, "CMP C", 1, 4, 0, 0},
    {0xBA, "CMP D", 1, 4, 0, 0}, {0xBB, "CMP E", 1, 4, 0, 0},
    {0xBC, "CMP H", 1, 4, 0, 0}, {0xBD, "CMP L", 1, 4, 0, 0},
    {0xBE, "CMP M", 1, 7, 0, 0}, {0xBF, "CMP A", 1, 4, 0, 0},

    // 0xC0-0xCF
    {0xC0, "RNZ", 1, 11, 0, 0}, {0xC1, "POP B", 1, 10, 0, 0},
    {0xC2, "JNZ", 3, 10, 2, 0}, {0xC3, "JMP", 3, 10, 2, 0},
    {0xC4, "CNZ", 3, 17, 2, 0}, {0xC5, "PUSH B", 1, 11, 0, 0},
    {0xC6, "ADI", 2, 7, 1, 0},  {0xC7, "RST 0", 1, 11, 0, 0},
    {0xC8, "RZ", 1, 11, 0, 0},  {0xC9, "RET", 1, 10, 0, 0},
    {0xCA, "JZ", 3, 10, 2, 0},  {0xCB, "JMP", 3, 10, 2, 0},  // Undocumented JMP
    {0xCC, "CZ", 3, 17, 2, 0},  {0xCD, "CALL", 3, 17, 2, 0},
    {0xCE, "ACI", 2, 7, 1, 0},  {0xCF, "RST 1", 1, 11, 0, 0},

    // 0xD0-0xDF
    {0xD0, "RNC", 1, 11, 0, 0}, {0xD1, "POP D", 1, 10, 0, 0},
    {0xD2, "JNC", 3, 10, 2, 0}, {0xD3, "OUT", 2, 10, 1, 0},
    {0xD4, "CNC", 3, 17, 2, 0}, {0xD5, "PUSH D", 1, 11, 0, 0},
    {0xD6, "SUI", 2, 7, 1, 0},  {0xD7, "RST 2", 1, 11, 0, 0},
    {0xD8, "RC", 1, 11, 0, 0},  {0xD9, "RET", 1, 10, 0, 0},  // Undocumented RET
    {0xDA, "JC", 3, 10, 2, 0},  {0xDB, "IN", 2, 10, 1, 0},
    {0xDC, "CC", 3, 17, 2, 0},  {0xDD, "CALL", 3, 17, 2, 0}, // Undocumented CALL
    {0xDE, "SBI", 2, 7, 1, 0},  {0xDF, "RST 3", 1, 11, 0, 0},

    // 0xE0-0xEF
    {0xE0, "RPO", 1, 11, 0, 0}, {0xE1, "POP H", 1, 10, 0, 0},
    {0xE2, "JPO", 3, 10, 2, 0}, {0xE3, "XTHL", 1, 18, 0, 0},
    {0xE4, "CPO", 3, 17, 2, 0}, {0xE5, "PUSH H", 1, 11, 0, 0},
    {0xE6, "ANI", 2, 7, 1, 0},  {0xE7, "RST 4", 1, 11, 0, 0},
    {0xE8, "RPE", 1, 11, 0, 0}, {0xE9, "PCHL", 1, 5, 0, 0},
    {0xEA, "JPE", 3, 10, 2, 0}, {0xEB, "XCHG", 1, 5, 0, 0},
    {0xEC, "CPE", 3, 17, 2, 0}, {0xED, "CALL", 3, 17, 2, 0}, // Undocumented CALL
    {0xEE, "XRI", 2, 7, 1, 0},  {0xEF, "RST 5", 1, 11, 0, 0},

    // 0xF0-0xFF
    {0xF0, "RP", 1, 11, 0, 0},  {0xF1, "POP PSW", 1, 10, 0, 0},
    {0xF2, "JP", 3, 10, 2, 0},  {0xF3, "DI", 1, 4, 0, 0},
    {0xF4, "CP", 3, 17, 2, 0},  {0xF5, "PUSH PSW", 1, 11, 0, 0},
    {0xF6, "ORI", 2, 7, 1, 0},  {0xF7, "RST 6", 1, 11, 0, 0},
    {0xF8, "RM", 1, 11, 0, 0},  {0xF9, "SPHL", 1, 5, 0, 0},
    {0xFA, "JM", 3, 10, 2, 0},  {0xFB, "EI", 1, 4, 0, 0},
    {0xFC, "CM", 3, 17, 2, 0},  {0xFD, "CALL", 3, 17, 2, 0}, // Undocumented CALL
    {0xFE, "CPI", 2, 7, 1, 0},  {0xFF, "RST 7", 1, 11, 0, 0}
};

void Disasm::init() {
    reset();
}

void Disasm::reset() {
    last_disasm_addr_ = 0;
    last_opcode_ = 0;
    last_instruction_size_ = 0;
    last_mnemonic_ = "";
}

void Disasm::disassemble(uint16_t addr, char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return;
    }
    
    disasm_result_t result;
    disassemble(addr, nullptr, &result);  // Assuming memory is accessible through class
    
    // Форматируем результат в буфер
    snprintf(buffer, size, "0x%04X: %s", addr, result.mnemonic.c_str());
    
    // Обновляем переменные отслеживания
    last_disasm_addr_ = addr;
    last_opcode_ = result.opcode;
    last_instruction_size_ = result.size;
    last_mnemonic_ = result.mnemonic;
}

void Disasm::disassemble(uint16_t addr, disasm_result_t* result) {
    // Эта версия считает, что память доступна внутри
    // (на практике память передаётся параметром)
    disassemble(addr, nullptr, result);
}

void Disasm::disassemble(uint16_t addr, const uint8_t* memory, disasm_result_t* result) {
    if (!result) {
        return;
    }
    
    // Инициализация структуры результата
    memset(result, 0, sizeof(disasm_result_t));
    
    if (!memory) {
        // Без памяти корректно дизассемблировать нельзя
        result->mnemonic = "INVALID";
        result->size = 1;
        result->opcode = 0x00;
        result->bytes[0] = 0x00;
        return;
    }
    
    // Опкод по указанному адресу
    uint8_t opcode = memory[addr];
    result->opcode = opcode;
    result->size = get_instruction_size(opcode);
    
    // Сохраняем фактические байты инструкции
    for (int i = 0; i < result->size && i < 3; i++) {
        result->bytes[i] = memory[addr + i];
    }
    
    // Мнемоника опкода
    const opcode_entry_t& entry = get_opcode_entry(opcode);
    std::string mnemonic = std::string(entry.mnemonic);
    
    // Добавляем операнды при необходимости
    if (entry.size == 2) {  // Immediate 8-bit operand
        uint8_t imm8 = memory[addr + 1];
        char operand[16];
        snprintf(operand, sizeof(operand), "0x%02X", imm8);
        mnemonic += operand;
    } else if (entry.size == 3) {  // Immediate 16-bit operand
        uint16_t imm16 = memory[addr + 1] | (memory[addr + 2] << 8);
        char operand[16];
        snprintf(operand, sizeof(operand), "0x%04X", imm16);
        mnemonic += operand;
    }
    
    result->mnemonic = mnemonic;
    
    // Обновляем переменные отслеживания
    last_disasm_addr_ = addr;
    last_opcode_ = opcode;
    last_instruction_size_ = result->size;
    last_mnemonic_ = result->mnemonic;
}

size_t Disasm::get_instruction_size(uint8_t opcode) const {
    const opcode_entry_t& entry = get_opcode_entry(opcode);
    return entry.size;
}

size_t Disasm::get_instruction_size(uint16_t addr, const uint8_t* memory) const {
    if (!memory) {
        return 1;  // Default to 1 byte if no memory provided
    }
    
    uint8_t opcode = memory[addr];
    return get_instruction_size(opcode);
}

bool Disasm::is_jump_or_return(uint8_t opcode) const {
    // Переходы: 0xC3, 0xCB, 0xCA, 0xC2, 0xCD, 0xC4, 0xDA, 0xD2, 0xEA, 0xE2, 0xFA, 0xF2
    // Вызовы: 0xCD, 0xC4, 0xCC, 0xD4, 0xDC, 0xE4, 0xEC, 0xF4, 0xFC
    // Возвраты: 0xC9, 0xC0, 0xC8, 0xD0, 0xD8, 0xE0, 0xE8, 0xF0, 0xF8
    return (opcode == 0xC3 || opcode == 0xCB ||  // JMP
            (opcode >= 0xC2 && opcode <= 0xCA && opcode != 0xC7 && opcode != 0xCF && opcode != 0xD7 && opcode != 0xDF && opcode != 0xE7 && opcode != 0xEF && opcode != 0xF7) ||  // Conditional jumps
            opcode == 0xC9 ||  // RET
            (opcode >= 0xC0 && opcode <= 0xC8 && opcode != 0xC7 && opcode != 0xC3 && opcode != 0xCB) ||  // Conditional returns
            (opcode >= 0xD0 && opcode <= 0xD8 && opcode != 0xD7 && opcode != 0xD3) ||  // More conditional returns
            (opcode >= 0xE0 && opcode <= 0xE8 && opcode != 0xE7) ||  // More conditional returns
            (opcode >= 0xF0 && opcode <= 0xF8 && opcode != 0xF7));   // More conditional returns
}

bool Disasm::is_call(uint8_t opcode) const {
    // Вызовы: 0xCD, 0xC4, 0xCC, 0xD4, 0xDC, 0xE4, 0xEC, 0xF4, 0xFC
    return (opcode == 0xCD || 
            (opcode >= 0xC4 && opcode <= 0xCC && opcode != 0xC7) ||
            (opcode >= 0xD4 && opcode <= 0xDC && opcode != 0xD7) ||
            (opcode >= 0xE4 && opcode <= 0xEC && opcode != 0xE7) ||
            (opcode >= 0xF4 && opcode <= 0xFC && opcode != 0xF7));
}

bool Disasm::is_conditional(uint8_t opcode) const {
    // Условный переход, вызов или возврат?
    // У таких инструкций битовые шаблоны кодируют условие
    // В i8080 условные инструкции: Jcc, Ccc, Rcc, где cc — условие
    
    // Переходы: JNZ(0xC2), JZ(0xCA), JNC(0xD2), JC(0xDA), JPO(0xE2), JPE(0xEA), JP(0xF2), JM(0xFA)
    // Вызовы: CNZ(0xC4), CZ(0xCC), CNC(0xD4), CC(0xDC), CPO(0xE4), CPE(0xEC), CP(0xF4), CM(0xFC)
    // Возвраты: RNZ(0xC0), RZ(0xC8), RNC(0xD0), RC(0xD8), RPO(0xE0), RPE(0xE8), RP(0xF0), RM(0xF8)
    
    switch (opcode) {
        case 0xC2: case 0xCA:  // JNZ, JZ
        case 0xD2: case 0xDA:  // JNC, JC
        case 0xE2: case 0xEA:  // JPO, JPE
        case 0xF2: case 0xFA:  // JP, JM
        case 0xC4: case 0xCC:  // CNZ, CZ
        case 0xD4: case 0xDC:  // CNC, CC
        case 0xE4: case 0xEC:  // CPO, CPE
        case 0xF4: case 0xFC:  // CP, CM
        case 0xC0: case 0xC8:  // RNZ, RZ
        case 0xD0: case 0xD8:  // RNC, RC
        case 0xE0: case 0xE8:  // RPO, RPE
        case 0xF0: case 0xF8:  // RP, RM
            return true;
        default:
            return false;
    }
}

bool Disasm::is_io_instruction(uint8_t opcode) const {
    // Инструкции ввода-вывода: IN (0xDB), OUT (0xD3)
    return (opcode == 0xDB || opcode == 0xD3);
}

bool Disasm::is_interrupt_instruction(uint8_t opcode) const {
    // Инструкции прерываний: EI (0xFB), DI (0xF3), RIM (0x20), SIM (0x30)
    // А также RST (0xC7, 0xCF, 0xD7, 0xDF, 0xE7, 0xEF, 0xF7)
    return (opcode == 0xFB || opcode == 0xF3 || opcode == 0x20 || opcode == 0x30 ||
            (opcode >= 0xC7 && opcode <= 0xFF && (opcode & 0x07) == 0x07));  // RST instructions
}

std::string Disasm::get_register_name(uint8_t reg_code) const {
    switch (reg_code) {
        case 0: return "B";
        case 1: return "C";
        case 2: return "D";
        case 3: return "E";
        case 4: return "H";
        case 5: return "L";
        case 6: return "M";  // Memory (HL indirect)
        case 7: return "A";  // Accumulator
        default: return "?";
    }
}

std::string Disasm::get_condition_name(uint8_t cond_code) const {
    switch (cond_code) {
        case 0: return "NZ";  // Not Zero
        case 1: return "Z";   // Zero
        case 2: return "NC";  // No Carry
        case 3: return "C";   // Carry
        case 4: return "PO";  // Parity Odd
        case 5: return "PE";  // Parity Even
        case 6: return "P";   // Plus
        case 7: return "M";   // Minus
        default: return "?";
    }
}

const Disasm::opcode_entry_t& Disasm::get_opcode_entry(uint8_t opcode) const {
    return opcode_table_[opcode];
}

std::string Disasm::format_operand(uint8_t type, const uint8_t* memory, uint16_t addr) const {
    if (!memory) {
        return "??";
    }
    
    switch (type) {
        case 1:  // 8-bit immediate
            return "$" + std::to_string(memory[addr + 1]);
        case 2:  // 16-bit immediate
            {
                uint16_t value = memory[addr + 1] | (memory[addr + 2] << 8);
                return "$" + std::to_string(value);
            }
        case 3:  // Register
            return get_register_name(memory[addr + 1]);
        default:
            return "";
    }
}

Disasm::DisasmStatus Disasm::get_status() const {
    DisasmStatus status;
    status.enabled = true;  // Disassembler is always available
    status.last_disasm_addr = last_disasm_addr_;
    status.last_opcode = last_opcode_;
    status.last_instruction_size = last_instruction_size_;
    status.last_mnemonic = last_mnemonic_;
    
    return status;
}

}  // namespace korvet