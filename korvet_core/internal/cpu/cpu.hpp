/**
 * @file cpu.hpp
 * @brief Обёртка над универсальным i8080::Cpu для korvet-core
 *
 * Этот файл предоставляет совместимый API korvet::Cpu, но внутри
 * использует i8080::Cpu (универсальный standalone-эмулятор).
 *
 * Все методы korvet::Cpu делегируют к i8080::Cpu.
 */
#pragma once

#include <cstdint>
#include "../../../i8080/i8080.hpp"

namespace korvet {

class Memory;
class Pic;

// =====================================================================
// Адаптеры для korvet-core типов
// =====================================================================

/**
 * @brief Адаптер korvet::Memory -> i8080::IMemory
 */
class MemoryToIMemory : public i8080::IMemory {
public:
    explicit MemoryToIMemory(Memory* m) : mem_(m) {}
    void set_memory(Memory* m) { mem_ = m; }

    uint8_t read(uint16_t addr) override;
    void write(uint16_t addr, uint8_t data) override;
    uint16_t read_word(uint16_t addr) override;
    void write_word(uint16_t addr, uint16_t data) override;

private:
    Memory* mem_;
};

/**
 * @brief Адаптер korvet::Pic -> i8080::IPic
 */
class PicToIPic : public i8080::IPic {
public:
    explicit PicToIPic(Pic* p) : pic_(p) {}
    void set_pic(Pic* p) { pic_ = p; }

    bool has_pending_interrupt() override;
    uint16_t acknowledge_interrupt() override;

private:
    Pic* pic_;
};

/**
 * @brief Обёртка над i8080::Cpu для korvet-core
 *
 * Внутри содержит i8080::Cpu + адаптеры для Memory и Pic.
 * Все методы korvet::Cpu делегируют к i8080::Cpu.
 */
class Cpu {
public:
    Cpu();
    ~Cpu() = default;

    /// Инициализация с memory и pic
    void init(Memory* memory, Pic* pic);

    /// Сброс процессора
    void reset();

    /// Выполнить одну инструкцию, вернуть T-циклы
    int execute_one_instruction();

    /// Выполнить N циклов
    void execute(uint32_t cycles) { cpu_.execute(cycles); }

    /// Одиночный шаг (для отладчика)
    void execute_single_step() { cpu_.execute_single_step(); }

    /// Сброс счётчика кадра
    void reset_frame_cycles() { cpu_.reset_frame_cycles(); }

    /// Получить счётчик кадра
    uint64_t get_frame_cycles() const { return cpu_.get_frame_cycles(); }

    // ===== Регистры (делегаты) =====
    uint16_t get_pc() const { return cpu_.get_pc(); }
    void set_pc(uint16_t pc) { cpu_.set_pc(pc); }
    uint16_t get_sp() const { return cpu_.get_sp(); }
    void set_sp(uint16_t sp) { cpu_.set_sp(sp); }
    uint8_t get_a() const { return cpu_.get_a(); }
    void set_a(uint8_t a) { cpu_.set_a(a); }
    uint8_t get_b() const { return cpu_.get_b(); }
    void set_b(uint8_t b) { cpu_.set_b(b); }
    uint8_t get_c() const { return cpu_.get_c(); }
    void set_c(uint8_t c) { cpu_.set_c(c); }
    uint8_t get_d() const { return cpu_.get_d(); }
    void set_d(uint8_t d) { cpu_.set_d(d); }
    uint8_t get_e() const { return cpu_.get_e(); }
    void set_e(uint8_t e) { cpu_.set_e(e); }
    uint8_t get_h() const { return cpu_.get_h(); }
    void set_h(uint8_t h) { cpu_.set_h(h); }
    uint8_t get_l() const { return cpu_.get_l(); }
    void set_l(uint8_t l) { cpu_.set_l(l); }
    uint8_t get_f() const { return cpu_.get_f(); }
    void set_f(uint8_t f) { cpu_.set_f(f); }

    // ===== Флаги (делегаты) =====
    bool get_flag_carry() const { return cpu_.get_flag_carry(); }
    void set_flag_carry(bool v) { cpu_.set_flag_carry(v); }
    bool get_flag_zero() const { return cpu_.get_flag_zero(); }
    void set_flag_zero(bool v) { cpu_.set_flag_zero(v); }
    bool get_flag_sign() const { return cpu_.get_flag_sign(); }
    void set_flag_sign(bool v) { cpu_.set_flag_sign(v); }
    bool get_flag_parity_overflow() const { return cpu_.get_flag_parity_overflow(); }
    void set_flag_parity_overflow(bool v) { cpu_.set_flag_parity_overflow(v); }
    bool get_flag_half_carry() const { return cpu_.get_flag_half_carry(); }
    void set_flag_half_carry(bool v) { cpu_.set_flag_half_carry(v); }
    bool get_flag_add_subtract() const { return cpu_.get_flag_add_subtract(); }
    void set_flag_add_subtract(bool v) { cpu_.set_flag_add_subtract(v); }

    // ===== Прерывания =====
    void enable_interrupts() { cpu_.enable_interrupts(); }
    void disable_interrupts() { cpu_.disable_interrupts(); }

    /// Установить PIC (для прерываний) — оборачивает в адаптер
    void set_pic(Pic* pic);

    // ===== Точки останова =====
    void set_breakpoint(uint16_t addr, bool enabled) { cpu_.set_breakpoint(addr, enabled); }
    bool has_breakpoint(uint16_t addr) const { return cpu_.has_breakpoint(addr); }
    bool was_breakpoint_hit() const { return cpu_.was_breakpoint_hit(); }

    // ===== Прочее =====
    bool is_halted() const { return cpu_.is_halted(); }
    void set_halted(bool h) { cpu_.set_halted(h); }
    void set_iff(uint16_t iff) { cpu_.set_iff(iff); }
    uint16_t get_iff() const { return cpu_.get_iff(); }
    uint16_t get_last_pc() const { return cpu_.get_state().last_pc; }
    i8080::cpu_state_t get_state() const { return cpu_.get_state(); }
    void set_state(const i8080::cpu_state_t& s) { cpu_.set_state(s); }

    /// Дизассемблирование инструкции
    void disassemble(uint16_t addr, char* buffer, size_t size);

private:
    i8080::Cpu cpu_;
    MemoryToIMemory mem_adapter_;
    PicToIPic pic_adapter_;
    bool pic_set_ = false;
};

}  // namespace korvet
