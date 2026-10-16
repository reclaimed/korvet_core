#include "tape.hpp"
#include "../logger/logger.hpp"

#include <cstring>

namespace korvet {

void Tape::init() {
    reset();
}

void Tape::reset() {
    // Позиция ленты — в начало; сам поток полупериодов (вставленная
    // «кассета») сохраняется: аппаратный сброс не вынимает кассету
    // из деки. Реле снимается (лента может двигаться).
    read_idx_ = 0;
    input_level_ = initial_level_;
    period_start_ = tape_pos_;

    tape_pos_ = 0;
    wall_cycle_ = 0;
    relay_engaged_ = false;
    out_level_ = 0;
}

void Tape::set_input(uint8_t initial_level, const uint32_t* half_period_cycles, size_t count) {
    if (!half_period_cycles && count > 0) {
        return;
    }
    count_ = (count > MAX_HALF_PERIODS) ? MAX_HALF_PERIODS : count;
    if (count_ > 0) {
        std::memcpy(periods_.data(), half_period_cycles, count_ * sizeof(uint32_t));
    }
    initial_level_ = initial_level & 0x01;
    // Новый поток стартует с текущей виртуальной позиции
    read_idx_ = 0;
    input_level_ = initial_level_;
    period_start_ = tape_pos_;
}

void Tape::input_append(const uint32_t* half_period_cycles, size_t count) {
    if (!half_period_cycles || count == 0) {
        return;
    }
    // Освобождаем потреблённые полупериоды (сдвиг к началу)
    if (read_idx_ > 0 && count_ > 0) {
        size_t consumed = read_idx_;
        std::memmove(periods_.data(), periods_.data() + consumed,
                     (count_ - consumed) * sizeof(uint32_t));
        count_ -= consumed;
        read_idx_ = 0;
    }
    // Добавляем новые; при переполнении вытесняем самые старые
    size_t to_add = count;
    if (count_ + to_add > MAX_HALF_PERIODS) {
        size_t overflow = count_ + to_add - MAX_HALF_PERIODS;
        if (overflow >= MAX_HALF_PERIODS) {
            // Полностью замещаем буфер последними MAX_HALF_PERIODS значениями
            count_ = 0;
            read_idx_ = 0;
            to_add = MAX_HALF_PERIODS;
            half_period_cycles += (count - to_add);
        } else {
            std::memmove(periods_.data(), periods_.data() + overflow,
                         (count_ - overflow) * sizeof(uint32_t));
            count_ -= overflow;
            LOG_WARN("Tape: буфер полупериодов переполнен, вытеснено %zu старых",
                     (size_t)overflow);
        }
    }
    size_t n = (to_add > MAX_HALF_PERIODS - count_) ? (MAX_HALF_PERIODS - count_) : to_add;
    std::memcpy(periods_.data() + count_, half_period_cycles, n * sizeof(uint32_t));
    count_ += n;
}

void Tape::input_rewind() {
    read_idx_ = 0;
    input_level_ = initial_level_;
    period_start_ = tape_pos_;
}

void Tape::sync(uint64_t cpu_cycle) {
    // Виртуальное время ленты прирастает только при отпущенном реле:
    // «кратковременный стоп» останавливает лентопротяжку.
    if (!relay_engaged_) {
        uint64_t delta = cpu_cycle - wall_cycle_;
        tape_pos_ += delta;
    }
    wall_cycle_ = cpu_cycle;
}

void Tape::advance_input_level() {
    // Проходим границы полупериодов, пока хватает виртуального времени
    while (read_idx_ < count_) {
        uint32_t dur = periods_[read_idx_];
        if (tape_pos_ - period_start_ < dur) {
            break;  // текущий полупериод ещё не закончился
        }
        period_start_ += dur;
        read_idx_++;
        input_level_ ^= 1;  // смена уровня
    }
}

uint8_t Tape::read_cas_level(uint64_t cpu_cycle) {
    sync(cpu_cycle);
    advance_input_level();
    return input_level_ & 0x01;
}

void Tape::on_casout_write(uint8_t level_code, uint64_t cycle) {
    // Кодирование выхода по описанию элементов:
    // SETLOW=00B → 0, SETMID=01B → 1, SETHIGH=11B → 2.
    // Код 0b10 не определён — трактуем как SETMID (уровень 1).
    uint8_t level;
    switch (level_code & 0x03) {
        case 0x00: level = 0; break;  // SETLOW
        case 0x03: level = 2; break;  // SETHIGH
        default:   level = 1; break;  // SETMID (в т.ч. неопределённый 0b10)
    }
    if (level != out_level_) {
        out_level_ = level;
        if (out_cb_) {
            out_cb_(level, cycle);
        }
    }
}

void Tape::set_stop_relay(bool stopped, uint64_t cycle) {
    if (stopped == relay_engaged_) {
        return;
    }
    // Сначала доводим виртуальное время до текущего такта со старым
    // состоянием реле, затем переключаем — пауза отсекается корректно.
    sync(cycle);
    relay_engaged_ = stopped;
    if (relay_cb_) {
        relay_cb_(stopped);
    }
}

}  // namespace korvet
