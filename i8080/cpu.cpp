/**
 * @file cpu.cpp
 * @brief Реализация универсального эмулятора Intel 8080 / КР580ВМ80А
 *
 * Standalone-реализация, не зависящая от korvet-core.
 * Использует только IMemory, IPic, ILogger (объявлены в i8080.hpp).
 *
 * Все 256 опкодов проверены построчно по эталону examples/original/i8080inc.c.
 *
 * @author Roo
 * @date 2026-09-01
 */
#include "i8080.hpp"
#include <cstring>

namespace i8080 {

// =====================================================================
// Утилиты: таблица чётности
// =====================================================================

namespace {
/// Таблица чётности для всех 256 значений байта.
/// P-флаг Intel 8080/КР580ВМ80А: 1, если число единичных бит ЧЁТНОЕ (even parity),
/// 0, если нечётное. Это стандартная семантика P/V флага 8080.
const bool parity_table[256] = {
    // 0x00-0x0F
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    // 0x10-0x1F
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    // 0x20-0x2F
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    // 0x30-0x3F
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    // 0x40-0x4F
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    // 0x50-0x5F
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    // 0x60-0x6F
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    // 0x70-0x7F
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    // 0x80-0x8F
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    // 0x90-0x9F
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    // 0xA0-0xAF
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    // 0xB0-0xBF
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    // 0xC0-0xCF
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    // 0xD0-0xDF
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    // 0xE0-0xEF
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    // 0xF0-0xFF
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1
};
}  // namespace

// =====================================================================
// Конструктор / инициализация
// =====================================================================

Cpu::Cpu() {
    state_ = {};
    flags_ = {};
}

void Cpu::init(IMemory* memory, IPic* pic) {
    memory_ = memory;
    pic_ = pic;
    reset();
}

void Cpu::reset() {
    state_.a = 0x00;
    state_.f = 0x00;
    state_.b = 0x00;
    state_.c = 0x00;
    state_.d = 0x00;
    state_.e = 0x00;
    state_.h = 0x00;
    state_.l = 0x00;
    state_.sp = 0x0000;
    state_.pc = 0x0000;
    state_.iff = 0x0000;
    state_.last_pc = 0x0000;

    flags_.carry = false;
    flags_.add_subtract = false;
    flags_.parity_overflow = false;
    flags_.half_carry = false;
    flags_.zero = false;
    flags_.sign = false;

    int_delay_ = 0;
    breakpoint_hit_ = false;
    frame_cycles_ = 0;
}

// =====================================================================
// Доступ к памяти (делегирование к IMemory)
// =====================================================================

uint8_t Cpu::read_byte(uint16_t addr) {
    if (!memory_) return 0xFF;
    uint8_t value = memory_->read(addr);
    if (logger_) logger_->log_memory_read(addr, value);
    return value;
}

void Cpu::write_byte(uint16_t addr, uint8_t data) {
    if (!memory_) return;
    memory_->write(addr, data);
    if (logger_) logger_->log_memory_write(addr, data);
}

uint16_t Cpu::read_word(uint16_t addr) {
    if (!memory_) return 0xFFFF;
    return memory_->read_word(addr);
}

void Cpu::write_word(uint16_t addr, uint16_t data) {
    if (!memory_) return;
    memory_->write_word(addr, data);
}

uint8_t Cpu::fetch_byte() {
    uint8_t value = read_byte(state_.pc);
    state_.pc++;
    return value;
}

uint16_t Cpu::fetch_word() {
    uint16_t value = read_word(state_.pc);
    state_.pc += 2;
    return value;
}

// =====================================================================
// Флаги: упаковка/распаковка F
// =====================================================================

uint8_t Cpu::get_flags_byte() const {
    // Формат регистра флагов настоящего Intel 8080 / КР580ВМ80А:
    //   бит 7: S, бит 6: Z, бит 5: 0 (всегда), бит 4: AC,
    //   бит 3: 0 (всегда), бит 2: P, бит 1: 1 (ВСЕГДА, аппаратно
    //   зашитая единица — не флаг!), бит 0: CY.
    // Поле flags_.add_subtract используется только как внутренний
    // вспомогательный признак для DAA (аналог Z80 N) и НЕ должно
    // попадать в реальный байт F — иначе PUSH PSW / диагностические
    // тесты (напр. CPUTEST.COM) обнаружат несоответствие.
    uint8_t f = 0x02;  // бит 1 всегда установлен на настоящем 8080
    if (flags_.sign)             f |= 0x80;
    if (flags_.zero)             f |= 0x40;
    if (flags_.half_carry)        f |= 0x10;
    if (flags_.parity_overflow)   f |= 0x04;
    if (flags_.carry)             f |= 0x01;
    return f;
}

void Cpu::set_flags_from_byte(uint8_t f) {
    flags_.sign             = (f & 0x80) != 0;
    flags_.zero             = (f & 0x40) != 0;
    flags_.half_carry        = (f & 0x10) != 0;
    flags_.parity_overflow   = (f & 0x04) != 0;
    flags_.carry             = (f & 0x01) != 0;
    // Бит 1 игнорируется как флаг (на реальном 8080 он не читается
    // обратно как значащий разряд общего назначения; add_subtract
    // остаётся внутренним состоянием, используемым только между
    // соседними ALU-операциями и DAA, и не восстанавливается из стека).
}

void Cpu::update_sign_flag(uint8_t value) {
    flags_.sign = (value & 0x80) != 0;
}

void Cpu::update_zero_flag(uint8_t value) {
    flags_.zero = (value == 0);
}

void Cpu::update_parity_flag(uint8_t value) {
    flags_.parity_overflow = parity_table[value];
}

void Cpu::update_auxiliary_carry_flag(uint8_t /*old_value*/, uint8_t new_value, uint8_t /*carry_in*/) {
    flags_.half_carry = ((new_value & 0x0F) == 0);
}

void Cpu::update_sign_zero_parity_flags(uint8_t value) {
    update_sign_flag(value);
    update_zero_flag(value);
    update_parity_flag(value);
}

// =====================================================================
// Арифметика
// =====================================================================

void Cpu::add_with_carry(uint8_t value) {
    bool old_carry = flags_.carry;
    uint16_t result = static_cast<uint16_t>(state_.a) + value + (old_carry ? 1 : 0);
    uint8_t res8 = result & 0xFF;

    flags_.carry = (result > 0xFF);
    update_sign_flag(res8);
    update_zero_flag(res8);
    update_parity_flag(res8);

    uint8_t old_low = state_.a & 0x0F;
    uint8_t val_low = value & 0x0F;
    flags_.half_carry = (old_low + val_low + (old_carry ? 1 : 0)) > 0x0F;

    flags_.add_subtract = false;
    state_.a = res8;
}

void Cpu::subtract_with_carry(uint8_t value) {
    // Вычитание на настоящем 8080 реализуется АЛУ как сложение с дополнением
    // до двух: A - value - CY  ==  A + (~value) + (!CY), с последующей
    // инверсией результирующего переноса. Это единственный способ получить
    // правильный AC-флаг во всех случаях (наивное сравнение полубайтов
    // "old_low < val_low" даёт неверный результат на многих операндах —
    // подтверждено эталонным тестом CPUTEST.COM).
    bool old_carry = flags_.carry;
    uint8_t b = static_cast<uint8_t>(~value);
    uint8_t carry_in = old_carry ? 0 : 1;  // !CY

    uint16_t result = static_cast<uint16_t>(state_.a) + b + carry_in;
    uint8_t res8 = result & 0xFF;
    uint16_t carry_chain = static_cast<uint16_t>(state_.a) ^ static_cast<uint16_t>(b) ^ result;

    flags_.carry = !((carry_chain & 0x100) != 0);       // итоговый перенос инвертируется
    flags_.half_carry = (carry_chain & 0x10) != 0;
    update_sign_flag(res8);
    update_zero_flag(res8);
    update_parity_flag(res8);

    flags_.add_subtract = true;
    state_.a = res8;
}

void Cpu::add_without_carry(uint8_t value) {
    uint16_t result = static_cast<uint16_t>(state_.a) + value;
    uint8_t res8 = result & 0xFF;

    flags_.carry = (result > 0xFF);
    update_sign_flag(res8);
    update_zero_flag(res8);
    update_parity_flag(res8);

    uint8_t old_low = state_.a & 0x0F;
    uint8_t val_low = value & 0x0F;
    flags_.half_carry = (old_low + val_low) > 0x0F;

    flags_.add_subtract = false;
    state_.a = res8;
}

void Cpu::subtract_without_carry(uint8_t value) {
    // См. комментарий в subtract_with_carry — та же схема через
    // дополнение до двух, только без входного заёма (carry_in = 1,
    // т.к. !CY при CY=0 всегда равно 1).
    uint8_t b = static_cast<uint8_t>(~value);
    uint16_t result = static_cast<uint16_t>(state_.a) + b + 1;
    uint8_t res8 = result & 0xFF;
    uint16_t carry_chain = static_cast<uint16_t>(state_.a) ^ static_cast<uint16_t>(b) ^ result;

    flags_.carry = !((carry_chain & 0x100) != 0);
    flags_.half_carry = (carry_chain & 0x10) != 0;
    update_sign_flag(res8);
    update_zero_flag(res8);
    update_parity_flag(res8);

    flags_.add_subtract = true;
    state_.a = res8;
}

// =====================================================================
// Регистровые операции
// =====================================================================

void Cpu::increment_register(uint8_t& reg) {
    uint8_t old_val = reg;
    reg++;
    update_sign_flag(reg);
    update_zero_flag(reg);
    update_auxiliary_carry_flag(old_val, reg, 0);
    update_parity_flag(reg);
    flags_.add_subtract = false;
}

void Cpu::decrement_register(uint8_t& reg) {
    // ВАЖНО: на настоящем 8080 правило AC для DCR ПРОТИВОПОЛОЖНО "интуитивному"
    // (в отличие от обычного вычитания): AC=1, если заимствования из старшего
    // полубайта НЕ произошло, т.е. младший полубайт до декремента был НЕ равен 0.
    // AC=0 только когда декремент вызвал перенос через границу полубайта
    // (0x_0 -> 0x_F). Подтверждено эталонным тестом CPUTEST.COM.
    uint8_t old_val = reg;
    reg--;
    update_sign_flag(reg);
    update_zero_flag(reg);
    flags_.half_carry = ((old_val & 0x0F) != 0);
    update_parity_flag(reg);
    flags_.add_subtract = true;
}

// =====================================================================
// Ротации
// =====================================================================

void Cpu::rotate_left_circular() {
    uint8_t carry = (state_.a & 0x80) ? 1 : 0;
    state_.a = (state_.a << 1) | carry;
    flags_.carry = carry;
}

void Cpu::rotate_right_circular() {
    uint8_t carry = state_.a & 0x01;
    state_.a = (state_.a >> 1) | (carry << 7);
    flags_.carry = carry;
}

void Cpu::rotate_left_through_carry() {
    uint8_t new_carry = (state_.a & 0x80) ? 1 : 0;
    state_.a = (state_.a << 1) | (flags_.carry ? 1 : 0);
    flags_.carry = new_carry;
}

void Cpu::rotate_right_through_carry() {
    uint8_t new_carry = state_.a & 0x01;
    state_.a = (state_.a >> 1) | (flags_.carry ? 0x80 : 0);
    flags_.carry = new_carry;
}

// =====================================================================
// DAA и сравнение
// =====================================================================

void Cpu::decimal_adjust_accumulator() {
    // Настоящий Intel 8080 / КР580ВМ80А не различает "режим сложения" и
    // "режим вычитания" при DAA — в отличие от Z80 (флаг N), 8080 всегда
    // выполняет ОДНУ и ту же коррекцию (в сторону сложения), основываясь
    // только на текущих AC/CY и значениях полубайтов. Если DAA выполнить
    // после SUB, результат не будет корректным BCD-вычитанием — это
    // документированное ограничение реального чипа, а не то, что должен
    // "исправлять" эмулятор. Правильность проверена по эталонному тесту
    // CPUTEST.COM (SuperSoft Diagnostics II).
    //
    // ВАЖНО: старший полубайт нужно проверять ПОСЛЕ применения коррекции
    // младшего полубайта (либо эквивалентным условием msb>=9 && lsb>9),
    // иначе теряется случай переноса из младшего полубайта в старший
    // (напр. DAA(0x9A) при CY=0 должно дать A=0x00,CY=1, а не A=0xA0,CY=0).
    uint8_t a = state_.a;
    uint8_t lsb = a & 0x0F;
    uint8_t msb = (a >> 4) & 0x0F;
    bool cy = flags_.carry;

    uint8_t correction = 0;
    if (flags_.half_carry || lsb > 9) {
        correction |= 0x06;
    }
    if (flags_.carry || msb > 9 || (msb >= 9 && lsb > 9)) {
        correction |= 0x60;
        cy = true;
    }

    flags_.half_carry = ((lsb + (correction & 0x0F)) > 0x0F);
    state_.a = static_cast<uint8_t>(a + correction);
    flags_.carry = cy;

    update_zero_flag(state_.a);
    update_sign_flag(state_.a);
    update_parity_flag(state_.a);
}

void Cpu::compare_with_flags(uint8_t value) {
    // CMP/CPI не изменяют A, но флаги вычисляются точно как при SUB —
    // через дополнение до двух (см. subtract_without_carry). Наивное
    // сравнение полубайтов для AC даёт неверный результат.
    uint8_t b = static_cast<uint8_t>(~value);
    uint16_t result = static_cast<uint16_t>(state_.a) + b + 1;
    uint8_t res8 = result & 0xFF;
    uint16_t carry_chain = static_cast<uint16_t>(state_.a) ^ static_cast<uint16_t>(b) ^ result;

    flags_.sign = (res8 & 0x80) != 0;
    flags_.zero = (res8 == 0);
    flags_.carry = !((carry_chain & 0x100) != 0);
    flags_.half_carry = (carry_chain & 0x10) != 0;
    flags_.add_subtract = true;

    // Для SUB/CMP парность (P) считается от результата (res8), A при этом
    // сама не изменяется у CMP/CPI — здесь update_parity_flag трогает
    // только сохранённый флаг, не регистр A.
    update_parity_flag(res8);
}

// =====================================================================
// Точки останова
// =====================================================================

void Cpu::set_breakpoint(uint16_t addr, bool enabled) {
    if (enabled) {
        breakpoints_.insert(addr);
    } else {
        breakpoints_.erase(addr);
    }
}

bool Cpu::has_breakpoint(uint16_t addr) const {
    return breakpoints_.find(addr) != breakpoints_.end();
}

// =====================================================================
// Дизассемблер (минимальный)
// =====================================================================

namespace {
const char* reg_names[8] = {"B", "C", "D", "E", "H", "L", "M", "A"};
const char* rp_names[4] = {"BC", "DE", "HL", "SP"};
const char* cc_names[8] = {"NZ", "Z", "NC", "C", "PO", "PE", "P", "M"};
}  // namespace

void Cpu::disassemble(uint16_t addr, char* buffer, size_t size) {
    if (!memory_ || size == 0) {
        if (size > 0) buffer[0] = 0;
        return;
    }

    uint8_t op = memory_->read(addr);
    uint8_t b1 = (addr + 1 < 0xFFFF) ? memory_->read(addr + 1) : 0;
    uint16_t b2 = (addr + 1 < 0xFFFF)
        ? static_cast<uint16_t>(memory_->read(addr + 1)) |
              (static_cast<uint16_t>(memory_->read(addr + 2)) << 8)
        : 0;

    const char* fmt = nullptr;
    int len = 1;

    switch (op) {
        case 0x00: case 0x08: case 0x10: case 0x18: case 0x20: case 0x28:
        case 0x30: case 0x38: case 0x40: case 0x49: case 0x52: case 0x5B:
        case 0x64: case 0x6D: case 0x7F:
            fmt = "NOP"; break;

        case 0x01: case 0x11: case 0x21: case 0x31:
            snprintf(buffer, size, "LXI %s, 0x%04X", rp_names[op >> 4], b2); return;

        case 0x02: fmt = "STAX B"; break;
        case 0x03: fmt = "INX B"; break;
        case 0x04: fmt = "INR B"; break;
        case 0x05: fmt = "DCR B"; break;
        case 0x06: snprintf(buffer, size, "MVI B, 0x%02X", b1); return;
        case 0x07: fmt = "RLC"; break;
        case 0x09: fmt = "DAD B"; break;
        case 0x0A: fmt = "LDAX B"; break;
        case 0x0B: fmt = "DCX B"; break;
        case 0x0C: fmt = "INR C"; break;
        case 0x0D: fmt = "DCR C"; break;
        case 0x0E: snprintf(buffer, size, "MVI C, 0x%02X", b1); return;
        case 0x0F: fmt = "RRC"; break;

        case 0x12: fmt = "STAX D"; break;
        case 0x13: fmt = "INX D"; break;
        case 0x14: fmt = "INR D"; break;
        case 0x15: fmt = "DCR D"; break;
        case 0x16: snprintf(buffer, size, "MVI D, 0x%02X", b1); return;
        case 0x17: fmt = "RAL"; break;
        case 0x19: fmt = "DAD D"; break;
        case 0x1A: fmt = "LDAX D"; break;
        case 0x1B: fmt = "DCX D"; break;
        case 0x1C: fmt = "INR E"; break;
        case 0x1D: fmt = "DCR E"; break;
        case 0x1E: snprintf(buffer, size, "MVI E, 0x%02X", b1); return;
        case 0x1F: fmt = "RAR"; break;

        case 0x22: snprintf(buffer, size, "SHLD 0x%04X", b2); return;
        case 0x23: fmt = "INX H"; break;
        case 0x24: fmt = "INR H"; break;
        case 0x25: fmt = "DCR H"; break;
        case 0x26: snprintf(buffer, size, "MVI H, 0x%02X", b1); return;
        case 0x27: fmt = "DAA"; break;
        case 0x29: fmt = "DAD H"; break;
        case 0x2A: snprintf(buffer, size, "LHLD 0x%04X", b2); return;
        case 0x2B: fmt = "DCX H"; break;
        case 0x2C: fmt = "INR L"; break;
        case 0x2D: fmt = "DCR L"; break;
        case 0x2E: snprintf(buffer, size, "MVI L, 0x%02X", b1); return;
        case 0x2F: fmt = "CMA"; break;

        case 0x32: snprintf(buffer, size, "STA 0x%04X", b2); return;
        case 0x33: fmt = "INX SP"; break;
        case 0x34: fmt = "INR M"; break;
        case 0x35: fmt = "DCR M"; break;
        case 0x36: snprintf(buffer, size, "MVI M, 0x%02X", b1); return;
        case 0x37: fmt = "STC"; break;
        case 0x39: fmt = "DAD SP"; break;
        case 0x3A: snprintf(buffer, size, "LDA 0x%04X", b2); return;
        case 0x3B: fmt = "DCX SP"; break;
        case 0x3C: fmt = "INR A"; break;
        case 0x3D: fmt = "DCR A"; break;
        case 0x3E: snprintf(buffer, size, "MVI A, 0x%02X", b1); return;
        case 0x3F: fmt = "CMC"; break;

        case 0x76: fmt = "HLT"; break;

        default: break;
    }

    if (fmt) {
        snprintf(buffer, size, "%s", fmt);
        return;
    }

    // MOV r1, r2
    if ((op & 0xC0) == 0x40) {
        snprintf(buffer, size, "MOV %s, %s", reg_names[(op >> 3) & 7], reg_names[op & 7]);
        return;
    }

    // ADD/ADC/SUB/SBB/ANA/XRA/ORA/CMP r
    if ((op & 0xC0) == 0x80) {
        const char* op_names[8] = {"ADD", "ADC", "SUB", "SBB", "ANA", "XRA", "ORA", "CMP"};
        snprintf(buffer, size, "%s %s", op_names[(op >> 3) & 7], reg_names[op & 7]);
        return;
    }

    // JMP/Jcond/Ccond/Rst/Call/Ret/Cond ret/Push/Pop
    if (op == 0xC3 || op == 0xCB) {
        snprintf(buffer, size, "JMP 0x%04X", b2); return;
    }
    if (op == 0xC2 || op == 0xCA || op == 0xD2 || op == 0xDA ||
        op == 0xE2 || op == 0xEA || op == 0xF2 || op == 0xFA) {
        snprintf(buffer, size, "J%s 0x%04X", cc_names[(op >> 3) & 7], b2); return;
    }
    if (op == 0xCD || op == 0xDD || op == 0xED || op == 0xFD) {
        snprintf(buffer, size, "CALL 0x%04X", b2); return;
    }
    if (op == 0xC4 || op == 0xCC || op == 0xD4 || op == 0xDC ||
        op == 0xE4 || op == 0xEC || op == 0xF4 || op == 0xFC) {
        snprintf(buffer, size, "C%s 0x%04X", cc_names[(op >> 3) & 7], b2); return;
    }
    if (op == 0xC0 || op == 0xC8 || op == 0xD0 || op == 0xD8 ||
        op == 0xE0 || op == 0xE8 || op == 0xF0 || op == 0xF8) {
        snprintf(buffer, size, "R%s", cc_names[(op >> 3) & 7]); return;
    }
    if (op == 0xC9 || op == 0xD9) { snprintf(buffer, size, "RET"); return; }
    if (op >= 0xC5 && op <= 0xF5 && (op & 0x0F) == 5) {
        snprintf(buffer, size, "PUSH %s", rp_names[(op >> 4) & 3]); return;
    }
    if (op >= 0xC1 && op <= 0xF1 && (op & 0x0F) == 1) {
        snprintf(buffer, size, "POP %s", rp_names[(op >> 4) & 3]); return;
    }
    if (op == 0xC7 || op == 0xCF || op == 0xD7 || op == 0xDF ||
        op == 0xE7 || op == 0xEF || op == 0xF7 || op == 0xFF) {
        snprintf(buffer, size, "RST %u", (op >> 3) & 7); return;
    }

    // Непосредственный операнд
    if (op == 0xC6) { snprintf(buffer, size, "ADI 0x%02X", b1); return; }
    if (op == 0xCE) { snprintf(buffer, size, "ACI 0x%02X", b1); return; }
    if (op == 0xD6) { snprintf(buffer, size, "SUI 0x%02X", b1); return; }
    if (op == 0xDE) { snprintf(buffer, size, "SBI 0x%02X", b1); return; }
    if (op == 0xE6) { snprintf(buffer, size, "ANI 0x%02X", b1); return; }
    if (op == 0xEE) { snprintf(buffer, size, "XRI 0x%02X", b1); return; }
    if (op == 0xF6) { snprintf(buffer, size, "ORI 0x%02X", b1); return; }
    if (op == 0xFE) { snprintf(buffer, size, "CPI 0x%02X", b1); return; }
    if (op == 0xD3) { snprintf(buffer, size, "OUT 0x%02X", b1); return; }
    if (op == 0xDB) { snprintf(buffer, size, "IN 0x%02X", b1); return; }
    if (op == 0xF3) { snprintf(buffer, size, "DI"); return; }
    if (op == 0xFB) { snprintf(buffer, size, "EI"); return; }
    if (op == 0xE3) { snprintf(buffer, size, "XTHL"); return; }
    if (op == 0xE9) { snprintf(buffer, size, "PCHL"); return; }
    if (op == 0xF9) { snprintf(buffer, size, "SPHL"); return; }
    if (op == 0xEB) { snprintf(buffer, size, "XCHG"); return; }

    snprintf(buffer, size, "DB 0x%02X", op);
}

// =====================================================================
// Такты
// =====================================================================

int Cpu::get_opcode_cycles(uint8_t opcode) {
    switch (opcode) {
        case 0x00: case 0x08: case 0x10: case 0x18: case 0x20: case 0x28:
        case 0x30: case 0x38:
            return 4;
        case 0x01: case 0x11: case 0x21: case 0x31:
            return 10;
        case 0x02: case 0x12:
            return 7;
        case 0x03: case 0x13: case 0x23: case 0x33:
            return 5;
        case 0x04: case 0x05: case 0x0C: case 0x0D: case 0x14: case 0x15:
        case 0x1C: case 0x1D: case 0x24: case 0x25: case 0x2C: case 0x2D:
        case 0x3C: case 0x3D:
            return 5;
        case 0x34: case 0x35:
            return 10;
        case 0x06: case 0x0E: case 0x16: case 0x1E: case 0x26: case 0x2E: case 0x3E:
            return 7;
        case 0x36:
            return 10;
        case 0x07: case 0x0F: case 0x17: case 0x1F:
            return 4;
        case 0x09: case 0x19: case 0x29: case 0x39:
            return 10;
        case 0x0A: case 0x1A:
            return 7;
        case 0x0B: case 0x1B: case 0x2B: case 0x3B:
            return 5;
        case 0x22:
            return 16;
        case 0x2A:
            return 16;
        case 0x27:
            return 4;
        case 0x2F:
            return 4;
        case 0x32:
            return 13;
        case 0x37:
            return 4;
        case 0x3A:
            return 13;
        case 0x3F:
            return 4;
        // MOV r,r -- 4-7 тактов (все 4-7, в этой реализации все = 5)
        case 0x40: case 0x49: case 0x52: case 0x5B:
        case 0x64: case 0x6D: case 0x7F:
            return 4;  // MOV r,r (4 cycles)
        // MOV r,r — 5 тактов (самый частый случай)
        default:
            if (opcode >= 0x40 && opcode <= 0x7F) {
                return 5;
            }
            // MOV M,r / MOV r,M — 7 тактов
            if ((opcode & 0xC0) == 0x40 && (opcode & 0x07) == 6) return 7;  // MOV r,M
            if ((opcode & 0xC0) == 0x40 && ((opcode >> 3) & 7) == 6) return 7;  // MOV M,r
            // ADD/ADC/SUB/SBB/ANA/XRA/ORA/CMP r — 4 такта (r) или 7 (M)
            if ((opcode & 0xC0) == 0x80) {
                if ((opcode & 0x07) == 6) return 7;  // op M
                return 4;
            }
            // Условные возвраты — 5/11 тактов
            if (opcode == 0xC0 || opcode == 0xC8 || opcode == 0xD0 || opcode == 0xD8 ||
                opcode == 0xE0 || opcode == 0xE8 || opcode == 0xF0 || opcode == 0xF8) {
                return 5;  // base
            }
            // Условные вызовы — 11/17
            if (opcode == 0xC4 || opcode == 0xCC || opcode == 0xD4 || opcode == 0xDC ||
                opcode == 0xE4 || opcode == 0xEC || opcode == 0xF4 || opcode == 0xFC) {
                return 11;
            }
            // RET, PUSH, POP — 10
            if (opcode == 0xC9 || opcode == 0xD9) return 10;
            if ((opcode & 0x0F) == 5 && opcode >= 0xC0) return 11;  // PUSH
            if ((opcode & 0x0F) == 1 && opcode >= 0xC0) return 10;  // POP
            // JMP / Jcond / CALL / Ccond — 10
            if (opcode == 0xC3 || opcode == 0xCB) return 10;
            if (opcode == 0xC2 || opcode == 0xCA || opcode == 0xD2 || opcode == 0xDA ||
                opcode == 0xE2 || opcode == 0xEA || opcode == 0xF2 || opcode == 0xFA) return 10;
            if (opcode >= 0xC7 && opcode <= 0xFF && (opcode & 0x07) == 7) return 11;  // RST
            if (opcode == 0xCD || opcode == 0xDD || opcode == 0xED || opcode == 0xFD) return 17;
            // Непосредственный операнд
            if (opcode == 0xC6 || opcode == 0xCE || opcode == 0xD6 || opcode == 0xDE ||
                opcode == 0xE6 || opcode == 0xEE || opcode == 0xF6 || opcode == 0xFE) return 7;
            // EI/DI/IN/OUT/HLT/XTHL/PCHL/SPHL/XCHG
            if (opcode == 0xD3 || opcode == 0xDB) return 10;
            if (opcode == 0xF3 || opcode == 0xFB) return 4;
            if (opcode == 0x76) return 7;  // HLT
            if (opcode == 0xE3 || opcode == 0xE9 || opcode == 0xF9 || opcode == 0xEB) return 4;
            return 4;
    }
}

// HLT (0x76) обрабатывается в execute_instruction() через state_.pc -= 1.
// Прерывания вытащат процессор из HLT через push PC; PC = vector (эталон i8080.c:461-463).
// Этот метод существует для совместимости с API/дебаг-логикой, всегда возвращает false.
bool Cpu::is_halted_instruction() const {
    return false;
}

// =====================================================================
// Выполнение одной инструкции (большой switch по опкодам)
// =====================================================================

void Cpu::execute_instruction() {
    uint16_t pc_before = state_.pc;
    uint8_t opcode = fetch_byte();

    if (logger_) {
        logger_->log_instruction(pc_before, opcode, state_);
    }

    switch (opcode) {
        // 0x00-0x3F
        case 0x00: case 0x08: case 0x10: case 0x18: case 0x20: case 0x28:
        case 0x30: case 0x38: break;  // NOP

        case 0x01: { state_.c = fetch_byte(); state_.b = fetch_byte(); break; }  // LXI B
        case 0x11: { state_.e = fetch_byte(); state_.d = fetch_byte(); break; }  // LXI D
        case 0x21: { state_.l = fetch_byte(); state_.h = fetch_byte(); break; }  // LXI H
        case 0x31: { state_.sp = read_word(state_.pc); state_.pc += 2; break; }  // LXI SP

        case 0x02: write_byte((state_.b << 8) | state_.c, state_.a); break;  // STAX B
        case 0x12: write_byte((state_.d << 8) | state_.e, state_.a); break;  // STAX D
        case 0x0A: state_.a = read_byte((state_.b << 8) | state_.c); break;  // LDAX B
        case 0x1A: state_.a = read_byte((state_.d << 8) | state_.e); break;  // LDAX D

        case 0x03: { uint16_t bc = (state_.b << 8) | state_.c; bc++; state_.c = bc & 0xFF; state_.b = (bc >> 8) & 0xFF; break; }
        case 0x13: { uint16_t de = (state_.d << 8) | state_.e; de++; state_.e = de & 0xFF; state_.d = (de >> 8) & 0xFF; break; }
        case 0x23: { uint16_t hl = (state_.h << 8) | state_.l; hl++; state_.l = hl & 0xFF; state_.h = (hl >> 8) & 0xFF; break; }
        case 0x33: state_.sp++; break;

        case 0x0B: { uint16_t bc = (state_.b << 8) | state_.c; bc--; state_.c = bc & 0xFF; state_.b = (bc >> 8) & 0xFF; break; }
        case 0x1B: { uint16_t de = (state_.d << 8) | state_.e; de--; state_.e = de & 0xFF; state_.d = (de >> 8) & 0xFF; break; }
        case 0x2B: { uint16_t hl = (state_.h << 8) | state_.l; hl--; state_.l = hl & 0xFF; state_.h = (hl >> 8) & 0xFF; break; }
        case 0x3B: state_.sp--; break;

        case 0x04: increment_register(state_.b); break;
        case 0x0C: increment_register(state_.c); break;
        case 0x14: increment_register(state_.d); break;
        case 0x1C: increment_register(state_.e); break;
        case 0x24: increment_register(state_.h); break;
        case 0x2C: increment_register(state_.l); break;
        case 0x3C: increment_register(state_.a); break;

        case 0x05: decrement_register(state_.b); break;
        case 0x0D: decrement_register(state_.c); break;
        case 0x15: decrement_register(state_.d); break;
        case 0x1D: decrement_register(state_.e); break;
        case 0x25: decrement_register(state_.h); break;
        case 0x2D: decrement_register(state_.l); break;
        case 0x3D: decrement_register(state_.a); break;

        case 0x34: { uint16_t hl_addr = (state_.h << 8) | state_.l; uint8_t value = read_byte(hl_addr); increment_register(value); write_byte(hl_addr, value); break; }
        case 0x35: { uint16_t hl_addr = (state_.h << 8) | state_.l; uint8_t value = read_byte(hl_addr); decrement_register(value); write_byte(hl_addr, value); break; }

        case 0x06: state_.b = fetch_byte(); break;
        case 0x0E: state_.c = fetch_byte(); break;
        case 0x16: state_.d = fetch_byte(); break;
        case 0x1E: state_.e = fetch_byte(); break;
        case 0x26: state_.h = fetch_byte(); break;
        case 0x2E: state_.l = fetch_byte(); break;
        case 0x36: { uint16_t hl_addr = (state_.h << 8) | state_.l; write_byte(hl_addr, fetch_byte()); break; }
        case 0x3E: state_.a = fetch_byte(); break;

        case 0x07: rotate_left_circular(); break;   // RLC
        case 0x0F: rotate_right_circular(); break;  // RRC
        case 0x17: rotate_left_through_carry(); break;  // RAL
        case 0x1F: rotate_right_through_carry(); break;  // RAR

        case 0x09: { uint32_t sum = ((uint32_t)state_.h << 8 | state_.l) + ((uint32_t)state_.b << 8 | state_.c); state_.l = sum & 0xFF; state_.h = (sum >> 8) & 0xFF; flags_.carry = (sum > 0xFFFF); break; }
        case 0x19: { uint32_t sum = ((uint32_t)state_.h << 8 | state_.l) + ((uint32_t)state_.d << 8 | state_.e); state_.l = sum & 0xFF; state_.h = (sum >> 8) & 0xFF; flags_.carry = (sum > 0xFFFF); break; }
        case 0x29: { uint32_t sum = ((uint32_t)state_.h << 8 | state_.l) * 2; state_.l = sum & 0xFF; state_.h = (sum >> 8) & 0xFF; flags_.carry = (sum > 0xFFFF); break; }
        case 0x39: { uint32_t sum = ((uint32_t)state_.h << 8 | state_.l) + state_.sp; state_.l = sum & 0xFF; state_.h = (sum >> 8) & 0xFF; flags_.carry = (sum > 0xFFFF); break; }

        case 0x22: { uint16_t addr = read_word(state_.pc); state_.pc += 2; write_byte(addr, state_.l); write_byte(addr + 1, state_.h); break; }  // SHLD
        case 0x2A: { uint16_t addr = read_word(state_.pc); state_.pc += 2; state_.l = read_byte(addr); state_.h = read_byte(addr + 1); break; }  // LHLD
        case 0x32: { uint16_t addr = read_word(state_.pc); state_.pc += 2; write_byte(addr, state_.a); break; }  // STA
        case 0x3A: { uint16_t addr = read_word(state_.pc); state_.pc += 2; state_.a = read_byte(addr); break; }  // LDA

        case 0x27: decimal_adjust_accumulator(); break;  // DAA
        case 0x2F: state_.a ^= 0xFF; break;  // CMA (на 8080 флаги НЕ затрагиваются)
        case 0x37: flags_.carry = true; break;  // STC (влияет только на CY)
        case 0x3F: flags_.carry = !flags_.carry; break;  // CMC (влияет только на CY)

        // 0x40-0x7F: MOV r1, r2
        case 0x40: state_.b = state_.b; break;
        case 0x41: state_.b = state_.c; break;
        case 0x42: state_.b = state_.d; break;
        case 0x43: state_.b = state_.e; break;
        case 0x44: state_.b = state_.h; break;
        case 0x45: state_.b = state_.l; break;
        case 0x46: state_.b = read_byte((state_.h << 8) | state_.l); break;
        case 0x47: state_.b = state_.a; break;
        case 0x48: state_.c = state_.b; break;
        case 0x49: state_.c = state_.c; break;
        case 0x4A: state_.c = state_.d; break;
        case 0x4B: state_.c = state_.e; break;
        case 0x4C: state_.c = state_.h; break;
        case 0x4D: state_.c = state_.l; break;
        case 0x4E: state_.c = read_byte((state_.h << 8) | state_.l); break;
        case 0x4F: state_.c = state_.a; break;
        case 0x50: state_.d = state_.b; break;
        case 0x51: state_.d = state_.c; break;
        case 0x52: state_.d = state_.d; break;
        case 0x53: state_.d = state_.e; break;
        case 0x54: state_.d = state_.h; break;
        case 0x55: state_.d = state_.l; break;
        case 0x56: state_.d = read_byte((state_.h << 8) | state_.l); break;
        case 0x57: state_.d = state_.a; break;
        case 0x58: state_.e = state_.b; break;
        case 0x59: state_.e = state_.c; break;
        case 0x5A: state_.e = state_.d; break;
        case 0x5B: state_.e = state_.e; break;
        case 0x5C: state_.e = state_.h; break;
        case 0x5D: state_.e = state_.l; break;
        case 0x5E: state_.e = read_byte((state_.h << 8) | state_.l); break;
        case 0x5F: state_.e = state_.a; break;
        case 0x60: state_.h = state_.b; break;
        case 0x61: state_.h = state_.c; break;
        case 0x62: state_.h = state_.d; break;
        case 0x63: state_.h = state_.e; break;
        case 0x64: state_.h = state_.h; break;
        case 0x65: state_.h = state_.l; break;
        case 0x66: state_.h = read_byte((state_.h << 8) | state_.l); break;
        case 0x67: state_.h = state_.a; break;
        case 0x68: state_.l = state_.b; break;
        case 0x69: state_.l = state_.c; break;
        case 0x6A: state_.l = state_.d; break;
        case 0x6B: state_.l = state_.e; break;
        case 0x6C: state_.l = state_.h; break;
        case 0x6D: state_.l = state_.l; break;
        case 0x6E: state_.l = read_byte((state_.h << 8) | state_.l); break;
        case 0x6F: state_.l = state_.a; break;
        case 0x70: write_byte((state_.h << 8) | state_.l, state_.b); break;
        case 0x71: write_byte((state_.h << 8) | state_.l, state_.c); break;
        case 0x72: write_byte((state_.h << 8) | state_.l, state_.d); break;
        case 0x73: write_byte((state_.h << 8) | state_.l, state_.e); break;
        case 0x74: write_byte((state_.h << 8) | state_.l, state_.h); break;
        case 0x75: write_byte((state_.h << 8) | state_.l, state_.l); break;
        case 0x76: state_.pc -= 1; break;  // HLT — крутимся пока не придёт прерывание (эталон i8080.c:461-463)
        case 0x77: write_byte((state_.h << 8) | state_.l, state_.a); break;
        case 0x78: state_.a = state_.b; break;
        case 0x79: state_.a = state_.c; break;
        case 0x7A: state_.a = state_.d; break;
        case 0x7B: state_.a = state_.e; break;
        case 0x7C: state_.a = state_.h; break;
        case 0x7D: state_.a = state_.l; break;
        case 0x7E: state_.a = read_byte((state_.h << 8) | state_.l); break;
        case 0x7F: state_.a = state_.a; break;

        // 0x80-0x87 ADD r
        case 0x80: add_without_carry(state_.b); break;
        case 0x81: add_without_carry(state_.c); break;
        case 0x82: add_without_carry(state_.d); break;
        case 0x83: add_without_carry(state_.e); break;
        case 0x84: add_without_carry(state_.h); break;
        case 0x85: add_without_carry(state_.l); break;
        case 0x86: add_without_carry(read_byte((state_.h << 8) | state_.l)); break;
        case 0x87: add_without_carry(state_.a); break;

        // 0x88-0x8F ADC r
        case 0x88: add_with_carry(state_.b); break;
        case 0x89: add_with_carry(state_.c); break;
        case 0x8A: add_with_carry(state_.d); break;
        case 0x8B: add_with_carry(state_.e); break;
        case 0x8C: add_with_carry(state_.h); break;
        case 0x8D: add_with_carry(state_.l); break;
        case 0x8E: add_with_carry(read_byte((state_.h << 8) | state_.l)); break;
        case 0x8F: add_with_carry(state_.a); break;

        // 0x90-0x97 SUB r
        case 0x90: subtract_without_carry(state_.b); break;
        case 0x91: subtract_without_carry(state_.c); break;
        case 0x92: subtract_without_carry(state_.d); break;
        case 0x93: subtract_without_carry(state_.e); break;
        case 0x94: subtract_without_carry(state_.h); break;
        case 0x95: subtract_without_carry(state_.l); break;
        case 0x96: subtract_without_carry(read_byte((state_.h << 8) | state_.l)); break;
        case 0x97: subtract_without_carry(state_.a); break;

        // 0x98-0x9F SBB r
        case 0x98: subtract_with_carry(state_.b); break;
        case 0x99: subtract_with_carry(state_.c); break;
        case 0x9A: subtract_with_carry(state_.d); break;
        case 0x9B: subtract_with_carry(state_.e); break;
        case 0x9C: subtract_with_carry(state_.h); break;
        case 0x9D: subtract_with_carry(state_.l); break;
        case 0x9E: subtract_with_carry(read_byte((state_.h << 8) | state_.l)); break;
        case 0x9F: subtract_with_carry(state_.a); break;

        // 0xA0-0xA7 ANA r
        case 0xA0: { uint8_t old_a = state_.a; state_.a &= state_.b; update_sign_zero_parity_flags(state_.a); flags_.half_carry = ((old_a | state_.b) & 0x08) != 0; flags_.carry = false; flags_.add_subtract = false; } break;
        case 0xA1: { uint8_t old_a = state_.a; state_.a &= state_.c; update_sign_zero_parity_flags(state_.a); flags_.half_carry = ((old_a | state_.c) & 0x08) != 0; flags_.carry = false; flags_.add_subtract = false; } break;
        case 0xA2: { uint8_t old_a = state_.a; state_.a &= state_.d; update_sign_zero_parity_flags(state_.a); flags_.half_carry = ((old_a | state_.d) & 0x08) != 0; flags_.carry = false; flags_.add_subtract = false; } break;
        case 0xA3: { uint8_t old_a = state_.a; state_.a &= state_.e; update_sign_zero_parity_flags(state_.a); flags_.half_carry = ((old_a | state_.e) & 0x08) != 0; flags_.carry = false; flags_.add_subtract = false; } break;
        case 0xA4: { uint8_t old_a = state_.a; state_.a &= state_.h; update_sign_zero_parity_flags(state_.a); flags_.half_carry = ((old_a | state_.h) & 0x08) != 0; flags_.carry = false; flags_.add_subtract = false; } break;
        case 0xA5: { uint8_t old_a = state_.a; state_.a &= state_.l; update_sign_zero_parity_flags(state_.a); flags_.half_carry = ((old_a | state_.l) & 0x08) != 0; flags_.carry = false; flags_.add_subtract = false; } break;
        case 0xA6: { uint8_t temp = read_byte((state_.h << 8) | state_.l); uint8_t old_a = state_.a; state_.a &= temp; update_sign_zero_parity_flags(state_.a); flags_.half_carry = ((old_a | temp) & 0x08) != 0; flags_.carry = false; flags_.add_subtract = false; break; }
        case 0xA7: { uint8_t old_a = state_.a; state_.a &= state_.a; update_sign_zero_parity_flags(state_.a); flags_.half_carry = ((old_a | state_.a) & 0x08) != 0; flags_.carry = false; flags_.add_subtract = false; } break;

        // 0xA8-0xAF XRA r
        case 0xA8: state_.a ^= state_.b; update_sign_zero_parity_flags(state_.a); flags_.half_carry = false; flags_.carry = false; flags_.add_subtract = false; break;
        case 0xA9: state_.a ^= state_.c; update_sign_zero_parity_flags(state_.a); flags_.half_carry = false; flags_.carry = false; flags_.add_subtract = false; break;
        case 0xAA: state_.a ^= state_.d; update_sign_zero_parity_flags(state_.a); flags_.half_carry = false; flags_.carry = false; flags_.add_subtract = false; break;
        case 0xAB: state_.a ^= state_.e; update_sign_zero_parity_flags(state_.a); flags_.half_carry = false; flags_.carry = false; flags_.add_subtract = false; break;
        case 0xAC: state_.a ^= state_.h; update_sign_zero_parity_flags(state_.a); flags_.half_carry = false; flags_.carry = false; flags_.add_subtract = false; break;
        case 0xAD: state_.a ^= state_.l; update_sign_zero_parity_flags(state_.a); flags_.half_carry = false; flags_.carry = false; flags_.add_subtract = false; break;
        case 0xAE: { uint8_t temp = read_byte((state_.h << 8) | state_.l); state_.a ^= temp; update_sign_zero_parity_flags(state_.a); flags_.half_carry = false; flags_.carry = false; flags_.add_subtract = false; break; }
        case 0xAF: state_.a ^= state_.a; update_sign_zero_parity_flags(state_.a); flags_.half_carry = false; flags_.carry = false; flags_.add_subtract = false; break;

        // 0xB0-0xB7 ORA r
        case 0xB0: state_.a |= state_.b; update_sign_zero_parity_flags(state_.a); flags_.half_carry = false; flags_.carry = false; flags_.add_subtract = false; break;
        case 0xB1: state_.a |= state_.c; update_sign_zero_parity_flags(state_.a); flags_.half_carry = false; flags_.carry = false; flags_.add_subtract = false; break;
        case 0xB2: state_.a |= state_.d; update_sign_zero_parity_flags(state_.a); flags_.half_carry = false; flags_.carry = false; flags_.add_subtract = false; break;
        case 0xB3: state_.a |= state_.e; update_sign_zero_parity_flags(state_.a); flags_.half_carry = false; flags_.carry = false; flags_.add_subtract = false; break;
        case 0xB4: state_.a |= state_.h; update_sign_zero_parity_flags(state_.a); flags_.half_carry = false; flags_.carry = false; flags_.add_subtract = false; break;
        case 0xB5: state_.a |= state_.l; update_sign_zero_parity_flags(state_.a); flags_.half_carry = false; flags_.carry = false; flags_.add_subtract = false; break;
        case 0xB6: { uint8_t temp = read_byte((state_.h << 8) | state_.l); state_.a |= temp; update_sign_zero_parity_flags(state_.a); flags_.half_carry = false; flags_.carry = false; flags_.add_subtract = false; break; }
        case 0xB7: state_.a |= state_.a; update_sign_zero_parity_flags(state_.a); flags_.half_carry = false; flags_.carry = false; flags_.add_subtract = false; break;

        // 0xB8-0xBF CMP r
        case 0xB8: compare_with_flags(state_.b); break;
        case 0xB9: compare_with_flags(state_.c); break;
        case 0xBA: compare_with_flags(state_.d); break;
        case 0xBB: compare_with_flags(state_.e); break;
        case 0xBC: compare_with_flags(state_.h); break;
        case 0xBD: compare_with_flags(state_.l); break;
        case 0xBE: compare_with_flags(read_byte((state_.h << 8) | state_.l)); break;
        case 0xBF: compare_with_flags(state_.a); break;

        // 0xC0-0xFF: Control flow
        case 0xC0: if (!flags_.zero) { uint8_t lo = read_byte(state_.sp++); uint8_t hi = read_byte(state_.sp++); state_.pc = (hi << 8) | lo; } break;  // RNZ
        case 0xC1: state_.c = read_byte(state_.sp++); state_.b = read_byte(state_.sp++); break;  // POP B
        case 0xC2: if (!flags_.zero) state_.pc = fetch_word(); else { state_.pc += 2; } break;  // JNZ
        case 0xC3: state_.pc = fetch_word(); break;  // JMP
        case 0xC4: if (!flags_.zero) { uint16_t addr = fetch_word(); write_byte(--state_.sp, (state_.pc >> 8) & 0xFF); write_byte(--state_.sp, state_.pc & 0xFF); state_.pc = addr; } else { state_.pc += 2; } break;  // CNZ
        case 0xC5: write_byte(--state_.sp, state_.b); write_byte(--state_.sp, state_.c); break;  // PUSH B
        case 0xC6: add_without_carry(fetch_byte()); break;  // ADI
        case 0xC7: write_byte(--state_.sp, (state_.pc >> 8) & 0xFF); write_byte(--state_.sp, state_.pc & 0xFF); state_.pc = 0x0000; break;  // RST 0

        case 0xC8: if (flags_.zero) { uint8_t lo = read_byte(state_.sp++); uint8_t hi = read_byte(state_.sp++); state_.pc = (hi << 8) | lo; } break;  // RZ
        case 0xC9: { uint8_t lo = read_byte(state_.sp++); uint8_t hi = read_byte(state_.sp++); state_.pc = (hi << 8) | lo; } break;  // RET
        case 0xCA: if (flags_.zero) state_.pc = fetch_word(); else { state_.pc += 2; } break;  // JZ
        case 0xCB: state_.pc = fetch_word(); break;  // JMP (undocumented)
        case 0xCC: if (flags_.zero) { uint16_t addr = fetch_word(); write_byte(--state_.sp, (state_.pc >> 8) & 0xFF); write_byte(--state_.sp, state_.pc & 0xFF); state_.pc = addr; } else { state_.pc += 2; } break;  // CZ
        case 0xCD: { uint16_t addr = fetch_word(); write_byte(--state_.sp, (state_.pc >> 8) & 0xFF); write_byte(--state_.sp, state_.pc & 0xFF); state_.pc = addr; } break;  // CALL
        case 0xCE: add_with_carry(fetch_byte()); break;  // ACI
        case 0xCF: write_byte(--state_.sp, (state_.pc >> 8) & 0xFF); write_byte(--state_.sp, state_.pc & 0xFF); state_.pc = 0x0008; break;  // RST 1

        case 0xD0: if (!flags_.carry) { uint8_t lo = read_byte(state_.sp++); uint8_t hi = read_byte(state_.sp++); state_.pc = (hi << 8) | lo; } break;  // RNC
        case 0xD1: state_.e = read_byte(state_.sp++); state_.d = read_byte(state_.sp++); break;  // POP D
        case 0xD2: if (!flags_.carry) state_.pc = fetch_word(); else { state_.pc += 2; } break;  // JNC
        case 0xD3: { uint8_t port = fetch_byte(); if (memory_) memory_->write((uint16_t)(port << 8) | port, state_.a); } break;  // OUT
        case 0xD4: if (!flags_.carry) { uint16_t addr = fetch_word(); write_byte(--state_.sp, (state_.pc >> 8) & 0xFF); write_byte(--state_.sp, state_.pc & 0xFF); state_.pc = addr; } else { state_.pc += 2; } break;  // CNC
        case 0xD5: write_byte(--state_.sp, state_.d); write_byte(--state_.sp, state_.e); break;  // PUSH D
        case 0xD6: subtract_without_carry(fetch_byte()); break;  // SUI
        case 0xD7: write_byte(--state_.sp, (state_.pc >> 8) & 0xFF); write_byte(--state_.sp, state_.pc & 0xFF); state_.pc = 0x0010; break;  // RST 2

        case 0xD8: if (flags_.carry) { uint8_t lo = read_byte(state_.sp++); uint8_t hi = read_byte(state_.sp++); state_.pc = (hi << 8) | lo; } break;  // RC
        case 0xD9: { uint8_t lo = read_byte(state_.sp++); uint8_t hi = read_byte(state_.sp++); state_.pc = (hi << 8) | lo; } break;  // RET (undocumented)
        case 0xDA: if (flags_.carry) state_.pc = fetch_word(); else { state_.pc += 2; } break;  // JC
        case 0xDB: { uint8_t port = fetch_byte(); if (memory_) state_.a = memory_->read((uint16_t)(port << 8) | port); } break;  // IN
        case 0xDC: if (flags_.carry) { uint16_t addr = fetch_word(); write_byte(--state_.sp, (state_.pc >> 8) & 0xFF); write_byte(--state_.sp, state_.pc & 0xFF); state_.pc = addr; } else { state_.pc += 2; } break;  // CC
        case 0xDD: { uint16_t addr = fetch_word(); write_byte(--state_.sp, (state_.pc >> 8) & 0xFF); write_byte(--state_.sp, state_.pc & 0xFF); state_.pc = addr; } break;  // CALL (undocumented)
        case 0xDE: subtract_with_carry(fetch_byte()); break;  // SBI
        case 0xDF: write_byte(--state_.sp, (state_.pc >> 8) & 0xFF); write_byte(--state_.sp, state_.pc & 0xFF); state_.pc = 0x0018; break;  // RST 3

        case 0xE0: if (!flags_.parity_overflow) { uint8_t lo = read_byte(state_.sp++); uint8_t hi = read_byte(state_.sp++); state_.pc = (hi << 8) | lo; } break;  // RPO
        case 0xE1: state_.l = read_byte(state_.sp++); state_.h = read_byte(state_.sp++); break;  // POP H
        case 0xE2: if (!flags_.parity_overflow) state_.pc = fetch_word(); else { state_.pc += 2; } break;  // JPO
        case 0xE3: { uint16_t temp = read_word(state_.sp); write_word(state_.sp, (state_.h << 8) | state_.l); uint16_t hl = temp; state_.l = hl & 0xFF; state_.h = (hl >> 8) & 0xFF; } break;  // XTHL
        case 0xE4: if (!flags_.parity_overflow) { uint16_t addr = fetch_word(); write_byte(--state_.sp, (state_.pc >> 8) & 0xFF); write_byte(--state_.sp, state_.pc & 0xFF); state_.pc = addr; } else { state_.pc += 2; } break;  // CPO
        case 0xE5: write_byte(--state_.sp, state_.h); write_byte(--state_.sp, state_.l); break;  // PUSH H
        case 0xE6: { uint8_t temp = fetch_byte(); uint8_t old_a = state_.a; state_.a &= temp; update_sign_zero_parity_flags(state_.a); flags_.half_carry = ((old_a | temp) & 0x08) != 0; flags_.carry = false; flags_.add_subtract = false; } break;  // ANI
        case 0xE7: write_byte(--state_.sp, (state_.pc >> 8) & 0xFF); write_byte(--state_.sp, state_.pc & 0xFF); state_.pc = 0x0020; break;  // RST 4

        case 0xE8: if (flags_.parity_overflow) { uint8_t lo = read_byte(state_.sp++); uint8_t hi = read_byte(state_.sp++); state_.pc = (hi << 8) | lo; } break;  // RPE
        case 0xE9: state_.pc = (state_.h << 8) | state_.l; break;  // PCHL
        case 0xEA: if (flags_.parity_overflow) state_.pc = fetch_word(); else { state_.pc += 2; } break;  // JPE
        case 0xEB: { uint16_t de = (state_.d << 8) | state_.e; uint16_t hl = (state_.h << 8) | state_.l; state_.d = (hl >> 8) & 0xFF; state_.e = hl & 0xFF; state_.h = (de >> 8) & 0xFF; state_.l = de & 0xFF; } break;  // XCHG
        case 0xEC: if (flags_.parity_overflow) { uint16_t addr = fetch_word(); write_byte(--state_.sp, (state_.pc >> 8) & 0xFF); write_byte(--state_.sp, state_.pc & 0xFF); state_.pc = addr; } else { state_.pc += 2; } break;  // CPE
        case 0xED: { uint16_t addr = fetch_word(); write_byte(--state_.sp, (state_.pc >> 8) & 0xFF); write_byte(--state_.sp, state_.pc & 0xFF); state_.pc = addr; } break;  // CALL (undocumented)
        case 0xEE: { uint8_t temp = fetch_byte(); state_.a ^= temp; update_sign_zero_parity_flags(state_.a); flags_.half_carry = false; flags_.carry = false; flags_.add_subtract = false; } break;  // XRI
        case 0xEF: write_byte(--state_.sp, (state_.pc >> 8) & 0xFF); write_byte(--state_.sp, state_.pc & 0xFF); state_.pc = 0x0028; break;  // RST 5

        case 0xF0: if (!flags_.sign) { uint8_t lo = read_byte(state_.sp++); uint8_t hi = read_byte(state_.sp++); state_.pc = (hi << 8) | lo; } break;  // RP
        case 0xF1: { uint8_t f_byte = read_byte(state_.sp++); uint8_t a_val = read_byte(state_.sp++); state_.a = a_val; set_flags_from_byte(f_byte); } break;  // POP PSW
        case 0xF2: if (!flags_.sign) state_.pc = fetch_word(); else { state_.pc += 2; } break;  // JP
        case 0xF3: state_.iff = 0; break;  // DI
        case 0xF4: if (!flags_.sign) { uint16_t addr = fetch_word(); write_byte(--state_.sp, (state_.pc >> 8) & 0xFF); write_byte(--state_.sp, state_.pc & 0xFF); state_.pc = addr; } else { state_.pc += 2; } break;  // CP
        case 0xF5: { uint8_t f_byte = get_flags_byte(); write_byte(--state_.sp, state_.a); write_byte(--state_.sp, f_byte); } break;  // PUSH PSW
        case 0xF6: { uint8_t temp = fetch_byte(); state_.a |= temp; update_sign_zero_parity_flags(state_.a); flags_.half_carry = false; flags_.carry = false; flags_.add_subtract = false; } break;  // ORI
        case 0xF7: write_byte(--state_.sp, (state_.pc >> 8) & 0xFF); write_byte(--state_.sp, state_.pc & 0xFF); state_.pc = 0x0030; break;  // RST 6

        case 0xF8: if (flags_.sign) { uint8_t lo = read_byte(state_.sp++); uint8_t hi = read_byte(state_.sp++); state_.pc = (hi << 8) | lo; } break;  // RM
        case 0xF9: state_.sp = (state_.h << 8) | state_.l; break;  // SPHL
        case 0xFA: if (flags_.sign) state_.pc = fetch_word(); else { state_.pc += 2; } break;  // JM
        case 0xFB: state_.iff = 1; int_delay_ = 1; break;  // EI
        case 0xFC: if (flags_.sign) { uint16_t addr = fetch_word(); write_byte(--state_.sp, (state_.pc >> 8) & 0xFF); write_byte(--state_.sp, state_.pc & 0xFF); state_.pc = addr; } else { state_.pc += 2; } break;  // CM
        case 0xFD: { uint16_t addr = fetch_word(); write_byte(--state_.sp, (state_.pc >> 8) & 0xFF); write_byte(--state_.sp, state_.pc & 0xFF); state_.pc = addr; } break;  // CALL (undocumented)
        case 0xFE: compare_with_flags(fetch_byte()); break;  // CPI
        case 0xFF: write_byte(--state_.sp, (state_.pc >> 8) & 0xFF); write_byte(--state_.sp, state_.pc & 0xFF); state_.pc = 0x0038; break;  // RST 7

        default: break;  // не должно случиться
    }

    state_.last_pc = pc_before;
}

int Cpu::execute_one_instruction() {
    // Проверка прерываний (если есть)
    // Эталон i8080.c: прерывания проверяются ВСЕГДА, в т.ч. после HLT.
    // HLT не блокирует проверку — он лишь крутит PC назад через pc-=1 (case 0x76).
    // Когда приходит прерывание, делается push PC; PC=vector — выход из HLT естественный.
    if (state_.iff && int_delay_ == 0 && pic_) {
        if (pic_->has_pending_interrupt()) {
            uint16_t vector = pic_->acknowledge_interrupt();
            if (vector != 0xFFFF) {
                // RST-подобное: push PC, jump to vector.
                //
                // ФИКС (v1.2, «CHESS.COM не отвечает на Y/N»): прерывание,
                // принятое ВО ВРЕМЯ HLT, обязано вернуть процессор к
                // инструкции ПОСЛЕ HLT — на реальном i8080 PC к моменту
                // принятия прерывания уже указывает за HLT. Наш HLT «крутит»
                // PC назад (case 0x76: pc -= 1 — PC стоит НА самом HLT), и
                // без поправки обработчик возвращался RET'ом СНОВА В HLT:
                // вечный цикл «проснулся → EOI → RET → HLT». Игры, спящие в
                // HLT до таймерного прерывания (CHESS.COM: hook вектора
                // TMRINT + HLT), зависали намертво с замёрзшим экраном.
                const uint8_t cur_op = memory_->read(state_.pc);
                const uint16_t push_pc =
                    (cur_op == 0x76) ? uint16_t(state_.pc + 1) : state_.pc;
                write_byte(--state_.sp, (push_pc >> 8) & 0xFF);
                write_byte(--state_.sp, push_pc & 0xFF);
                state_.pc = vector;
                state_.iff = 0;
                // ФИКС: такты подтверждения прерывания ОБЯЗАНЫ попасть в
                // frame_cycles_, иначе счётчик кадра (motherboard.cpp::
                // run_frame(), условие `frame_cycles_ < TICKS_PER_FRAME`) недосчитывает
                // реально исполненное время на каждое прерывание (IRQ4/IRQ5 и
                // т.д. срабатывают многократно за кадр) — кадр в реальности
                // содержит больше исполненных инструкций, чем показывает
                // frame_cycles_, что приводит к дрейфующей длительности
                // run_frame() и, как следствие, к нестабильному FPS.
                frame_cycles_ += 11;
                return 11;
            }
        }
    }

    if (int_delay_ > 0) int_delay_--;

    // HLT обрабатывается в case 0x76 через state_.pc -= 1, отдельной ветки не нужно.
    // Прерывания уже проверены выше (безусловно, по эталону i8080.c).

    uint16_t pc_before = state_.pc;
    breakpoint_hit_ = false;

    if (!breakpoints_.empty() && has_breakpoint(pc_before)) {
        breakpoint_hit_ = true;
        return 0;
    }

    execute_instruction();

    int cycles = get_opcode_cycles(memory_->read(pc_before));
    frame_cycles_ += cycles;
    return cycles;
}

void Cpu::execute(uint32_t cycles) {
    // Цикл без !halted_ — HLT сам себя «крутит» через pc-=1, прерывания
    // вытащат CPU из HLT через push PC; PC=vector (эталон i8080.c).
    uint32_t executed = 0;
    while (executed < cycles) {
        int cost = execute_one_instruction();
        if (cost <= 0) break;
        executed += cost;
    }
}

void Cpu::execute_single_step() {
    execute_instruction();
    frame_cycles_ += 4;
}

}  // namespace i8080
