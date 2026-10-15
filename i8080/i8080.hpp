/**
 * @file i8080.hpp
 * @brief Универсальный эмулятор процессора Intel 8080 / КР580ВМ80А
 *
 * Standalone-библиотека, не зависящая от проекта korvet-core.
 * Может быть использована в любом проекте, требующем эмуляции i8080.
 *
 * Зависимости:
 *   - IMemory, IPic (интерфейсы)
 *   - Стандартная библиотека C++ (cstdint, vector, unordered_set)
 *
 * @author Roo
 * @date 2026-09-01
 */
#pragma once

#include <cstdint>
#include <array>
#include <unordered_set>
#include <functional>
#include <string>
#include <cstdio>

namespace i8080 {

// =====================================================================
// Типы данных CPU
// =====================================================================

/**
 * @brief Состояние регистров CPU
 *
 * Хранит 8-битные регистры A, F, B, C, D, E, H, L, 16-битные SP, PC,
 * а также IFF (Interrupt Flip-Flop) и last_pc.
 */
struct cpu_state_t {
    uint8_t a, f;      // Accumulator and flags (f -- packed flags byte, F регистр)
    uint8_t b, c;      // BC register pair (b=high, c=low)
    uint8_t d, e;      // DE register pair
    uint8_t h, l;      // HL register pair
    uint16_t sp;       // Stack pointer
    uint16_t pc;       // Program counter
    uint16_t iff;      // Interrupt flip-flop
    uint16_t last_pc;  // Last program counter (для отладки)
};

/**
 * @brief Флаги CPU в декомпозированном виде
 *
 * Используется внутри Cpu для удобства. Упаковывается в F (через f_bits) при push.
 */
struct cpu_flags_t {
    bool carry;             // CY (бит 0)
    bool add_subtract;      // N (бит 1) -- для DAA
    bool parity_overflow;   // P/V (бит 2)
    // бит 3 не используется
    bool half_carry;        // H (бит 4) -- для DAA
    // бит 5 не используется
    bool zero;              // Z (бит 6)
    bool sign;              // S (бит 7)
};

// =====================================================================
// Интерфейсы для абстракции от korvet-core
// =====================================================================

/**
 * @brief Интерфейс памяти, который должен реализовать target
 *
 * CPU обращается к памяти только через этот интерфейс, поэтому
 * может быть подключен к любой системе памяти.
 */
class IMemory {
public:
    virtual ~IMemory() = default;

    /// Чтение 1 байта по адресу addr
    virtual uint8_t read(uint16_t addr) = 0;

    /// Запись 1 байта по адресу addr
    virtual void write(uint16_t addr, uint8_t data) = 0;

    /// Чтение 2 байт (little-endian) по адресу addr
    virtual uint16_t read_word(uint16_t addr) = 0;

    /// Запись 2 байт (little-endian) по адресу addr
    virtual void write_word(uint16_t addr, uint16_t data) = 0;
};

/**
 * @brief Интерфейс контроллера прерываний
 *
 * CPU опрашивает PIC перед каждой инструкцией. Если PIC nullptr --
 * прерывания запрещены.
 */
class IPic {
public:
    virtual ~IPic() = default;

    /// Есть ли ожидающее прерывание?
    virtual bool has_pending_interrupt() = 0;

    /// Подтвердить прерывание (вернуть вектор прерывания)
    /// Возвращает 0xFFFF если нет прерывания
    virtual uint16_t acknowledge_interrupt() = 0;
};

/**
 * @brief Опциональный интерфейс для трассировки/логирования
 *
 * Если передан nullptr, CPU работает без логирования.
 * Позволяет интегрировать CPU в любую систему логирования.
 */
class ILogger {
public:
    virtual ~ILogger() = default;

    /// Логирование выполнения инструкции
    virtual void log_instruction(uint16_t pc, uint8_t opcode,
                                 const cpu_state_t& state) = 0;

    /// Логирование чтения памяти
    virtual void log_memory_read(uint16_t addr, uint8_t value) = 0;

    /// Логирование записи в память
    virtual void log_memory_write(uint16_t addr, uint8_t value) = 0;
};

// =====================================================================
// Основной класс CPU
// =====================================================================

/**
 * @brief Эмулятор Intel 8080 / КР580ВМ80А
 *
 * Реализует полный набор из 256 опкодов (проверено построчно по эталону
 * examples/original/i8080inc.c).
 *
 * Не зависит от korvet-core: использует только IMemory, IPic, ILogger.
 *
 * Использование:
 * @code
 * class MyMemory : public IMemory { ... };
 * MyMemory mem;
 * Cpu cpu;
 * cpu.init(&mem, nullptr);  // без прерываний
 * cpu.reset();
 * while (running) {
 *     cpu.execute_one_instruction();
 * }
 * @endcode
 */
class Cpu {
public:
    Cpu();
    ~Cpu() = default;

    /// Инициализация с memory и pic (pic может быть nullptr)
    void init(IMemory* memory, IPic* pic = nullptr);

    /// Установить логгер (опционально, nullptr = без логирования)
    void set_logger(ILogger* logger) { logger_ = logger; }

    /// Сброс процессора в начальное состояние
    void reset();

    /// Выполнить одну инструкцию, вернуть количество T-циклов
    int execute_one_instruction();

    /// Выполнить N циклов
    void execute(uint32_t cycles);

    /// Одиночный шаг (для отладчика)
    void execute_single_step();

    // ===== Доступ к регистрам =====
    uint16_t get_pc() const { return state_.pc; }
    void set_pc(uint16_t pc) { state_.pc = pc; }

    uint16_t get_sp() const { return state_.sp; }
    void set_sp(uint16_t sp) { state_.sp = sp; }

    uint8_t get_a() const { return state_.a; }
    void set_a(uint8_t a) { state_.a = a; }

    uint8_t get_f() const { return state_.f; }
    void set_f(uint8_t f) { state_.f = f; }

    uint8_t get_b() const { return state_.b; }
    void set_b(uint8_t b) { state_.b = b; }

    uint8_t get_c() const { return state_.c; }
    void set_c(uint8_t c) { state_.c = c; }

    uint8_t get_d() const { return state_.d; }
    void set_d(uint8_t d) { state_.d = d; }

    uint8_t get_e() const { return state_.e; }
    void set_e(uint8_t e) { state_.e = e; }

    uint8_t get_h() const { return state_.h; }
    void set_h(uint8_t h) { state_.h = h; }

    uint8_t get_l() const { return state_.l; }
    void set_l(uint8_t l) { state_.l = l; }

    /// Полное состояние (для дампов и сохранения)
    cpu_state_t get_state() const { return state_; }
    void set_state(const cpu_state_t& state) { state_ = state; }

    // ===== Флаги =====
    bool get_flag_carry() const { return flags_.carry; }
    void set_flag_carry(bool v) { flags_.carry = v; }

    bool get_flag_zero() const { return flags_.zero; }
    void set_flag_zero(bool v) { flags_.zero = v; }

    bool get_flag_sign() const { return flags_.sign; }
    void set_flag_sign(bool v) { flags_.sign = v; }

    bool get_flag_parity_overflow() const { return flags_.parity_overflow; }
    void set_flag_parity_overflow(bool v) { flags_.parity_overflow = v; }

    bool get_flag_half_carry() const { return flags_.half_carry; }
    void set_flag_half_carry(bool v) { flags_.half_carry = v; }

    bool get_flag_add_subtract() const { return flags_.add_subtract; }
    void set_flag_add_subtract(bool v) { flags_.add_subtract = v; }

    /// Получить упакованный F (как в стеке при PUSH PSW)
    uint8_t get_flags_byte() const;

    /// Распаковать F в flags_
    void set_flags_from_byte(uint8_t f);

    // ===== Прерывания =====
    void enable_interrupts() { state_.iff = 1; int_delay_ = 1; }
    void disable_interrupts() { state_.iff = 0; }
    uint16_t get_iff() const { return state_.iff; }
    void set_iff(uint16_t iff) { state_.iff = iff; }

    // ===== Прочее =====
    /// HLT в нашей реализации крутится через pc-=1 без флага halted (эталон i8080.c:461-463).
    /// Метод оставлен для совместимости API, всегда возвращает false.
    bool is_halted() const { return false; }
    void set_halted(bool /*h*/) { /* no-op: HLT — это цикл в execute_instruction() */ }

    uint64_t get_frame_cycles() const { return frame_cycles_; }
    void reset_frame_cycles() { frame_cycles_ = 0; }

    // ===== Точки останова =====
    void set_breakpoint(uint16_t addr, bool enabled);
    bool has_breakpoint(uint16_t addr) const;
    bool was_breakpoint_hit() const { return breakpoint_hit_; }

    // ===== Дизассемблер =====
    /// Дизассемблировать одну инструкцию по адресу
    void disassemble(uint16_t addr, char* buffer, size_t size);

private:
    cpu_state_t state_;
    cpu_flags_t flags_;
    IMemory* memory_ = nullptr;
    IPic* pic_ = nullptr;
    ILogger* logger_ = nullptr;

    int int_delay_ = 0;
    bool breakpoint_hit_ = false;
    uint64_t frame_cycles_ = 0;
    std::unordered_set<uint16_t> breakpoints_;

    // Внутренние методы (реализация в cpu.cpp)
    uint8_t fetch_byte();
    uint16_t fetch_word();
    uint8_t read_byte(uint16_t addr);
    void write_byte(uint16_t addr, uint8_t data);
    uint16_t read_word(uint16_t addr);
    void write_word(uint16_t addr, uint16_t data);
    void execute_instruction();

    // Флаги
    void update_sign_flag(uint8_t value);
    void update_zero_flag(uint8_t value);
    void update_parity_flag(uint8_t value);
    void update_auxiliary_carry_flag(uint8_t old_value, uint8_t new_value, uint8_t carry_in);

    // Арифметика
    void add_without_carry(uint8_t value);
    void add_with_carry(uint8_t value);
    void subtract_without_carry(uint8_t value);
    void subtract_with_carry(uint8_t value);

    // Регистры
    void increment_register(uint8_t& reg);
    void decrement_register(uint8_t& reg);

    // Ротации
    void rotate_left_circular();
    void rotate_right_circular();
    void rotate_left_through_carry();
    void rotate_right_through_carry();

    // Специальные
    void decimal_adjust_accumulator();
    void compare_with_flags(uint8_t value);

    // Вспомогательные
    void update_sign_zero_parity_flags(uint8_t value);
    int get_opcode_cycles(uint8_t opcode);
    bool is_halted_instruction() const;
};

}  // namespace i8080
