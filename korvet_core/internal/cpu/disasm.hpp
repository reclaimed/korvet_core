/**
 * @file disasm.hpp
 * @brief Дизассемблер процессора i8080 для отладки
 * 
 * Содержит определение класса Disasm, который реализует
 * дизассемблирование инструкций процессора i8080 для отладки.
 * 
 * @author Korvet Team
 * @date 2024
 */

#pragma once

#include <cstdint>
#include <string>

namespace korvet {

struct disasm_result_t {
    uint8_t opcode;
    uint8_t size;
    std::string mnemonic;
    uint8_t bytes[3];
};

/**
 * @brief Класс дизассемблера процессора i8080
 * 
 * Реализует функции дизассемблирования инструкций процессора
 * i8080 для целей отладки эмуляции.
 */
class Disasm {
public:
    void init();
    void reset();
    
    void disassemble(uint16_t addr, char* buffer, size_t size);
    void disassemble(uint16_t addr, disasm_result_t* result);
    void disassemble(uint16_t addr, const uint8_t* memory, disasm_result_t* result);
    
    size_t get_instruction_size(uint8_t opcode) const;
    size_t get_instruction_size(uint16_t addr, const uint8_t* memory) const;
    
    // Типы инструкций
    bool is_jump_or_return(uint8_t opcode) const;
    bool is_call(uint8_t opcode) const;
    bool is_conditional(uint8_t opcode) const;
    bool is_io_instruction(uint8_t opcode) const;
    bool is_interrupt_instruction(uint8_t opcode) const;
    
    // Утилиты
    static constexpr size_t MAX_MNEMONIC_SIZE = 16;
    static constexpr size_t MAX_OPERAND_SIZE = 12;
    
    // Имена регистров
    std::string get_register_name(uint8_t reg_code) const;
    std::string get_condition_name(uint8_t cond_code) const;
    
    // Интерфейс отладки
    struct DisasmStatus {
        bool enabled;              // Is disassembler enabled
        uint16_t last_disasm_addr; // Last address disassembled
        uint8_t last_opcode;       // Last opcode disassembled
        uint8_t last_instruction_size; // Size of last instruction
        std::string last_mnemonic; // Mnemonic of last instruction
    };
    
    DisasmStatus get_status() const;

private:
    // Таблицы инструкций
    struct opcode_entry_t {
        uint8_t opcode;
        const char* mnemonic;
        uint8_t size;
        uint8_t cycles;
        uint8_t type;      // 0=no operands, 1=imm8, 2=imm16, 3=reg
        uint8_t param_mask;
    };
    
    static const opcode_entry_t opcode_table_[256];
    
    // Вспомогательные методы
    std::string format_operand(uint8_t type, const uint8_t* memory, uint16_t addr) const;
    const opcode_entry_t& get_opcode_entry(uint8_t opcode) const;
    
    // Отслеживание состояния
    uint16_t last_disasm_addr_;
    uint8_t last_opcode_;
    uint8_t last_instruction_size_;
    std::string last_mnemonic_;
};

}  // namespace korvet