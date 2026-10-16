#include "pic.hpp"
#include "../logger/logger.hpp"

namespace korvet {

void Pic::init() {
    reset();
}

void Pic::reset() {
    irr_ = 0;
    isr_ = 0;
    imr_ = 0xFF;  // All interrupts masked at startup (matches original)
    
    icw1_ = 0;
    icw2_ = 0;
    icw3_ = 0;
    icw4_ = 0;

    icw_load_active_ = false;
    icw_load_step_ = 0;
    icw_cascade_ = false;
    
    ocw3_ = 0;
    
    special_mask_mode_ = false;
    poll_command_ = false;
    
    low_priority_ = 7;  // IRQ7 is lowest priority at init (matches original)
}

void Pic::raise_irq(uint8_t irq) {
    if (irq < 8) {
        irr_ |= (1 << irq);
        update_interrupt_line();
    }
}

void Pic::lower_irq(uint8_t irq) {
    if (irq < 8) {
        irr_ &= ~(1 << irq);
    }
}

uint8_t Pic::find_highest_priority_irq(uint8_t mask) const {
    // Циклический приоритет: начинаем с (low_priority_ + 1) & 7
    // Циклический приоритет от младшего (эквивалент FindMaxBit с picLowINT)
    uint8_t irq = (low_priority_ + 1) & 0x07;
    for (int i = 0; i < 8; i++) {
        if (mask & (1 << irq)) return irq;
        irq = (irq + 1) & 0x07;
    }
    return 8;  // Not found
}

void Pic::write(uint8_t port, uint8_t data) {
    // Бит 0 порта = линия A0
    // A0=0: ICW1/OCW2/OCW3
    // A0=1: ICW2/ICW3/ICW4/OCW1

    if ((port & 1) == 0) {
        // A0 = 0
        if (data & 0x10) {
            // ICW1 — команда инициализации. Автомат последовательности ICW:
            //   следующий байт A0=1 — ВСЕГДА ICW2 (база векторов);
            //   при SNGL=0 (бит 1 сброшен) за ним ожидается ещё ICW3
            //   (конфигурация каскада — поглощается).
            // После завершения последовательности все дальнейшие записи
            // A0=1 идут в OCW1 (IMR) — в том числе байт ICW4:
            // зафиксированное поведение, воспроизводим его сознательно.
            // I-2: раньше SNGL=0 зависал в шаге ICW2 — cascade-инициализация
            // ломала маскирование прерываний; SNGL=1 наоборот терял ICW2
            // (база векторов оставалась 0000H — прерывание уводило
            // процессор в мусорный вектор).
            icw1_ = data;
            icw_load_active_ = true;
            icw_cascade_ = (data & 0x02) == 0;  // SNGL=0 — ждём ICW3 после ICW2
            icw_load_step_ = 1;                 // следующий байт — ICW2

            // Сброс состояния при ICW1
            low_priority_ = 7;
            poll_command_ = false;
            special_mask_mode_ = false;
            imr_ = 0;
            isr_ = 0;
        } else if (data & 0x08) {
            // OCW3
            if (data & 0x40) {
                special_mask_mode_ = (data & 0x20) != 0;
            }
            if (data & 0x02) {
                // бит RR/RIS: выбор регистра для чтения
                ocw3_ = data;
            }
            if (data & 0x04) {
                poll_command_ = true;
            }

        } else {
            // OCW2 — управление прерываниями (7 подкоманд по битам 7-5)
            uint8_t subcmd = (data >> 5) & 0x07;
            switch (subcmd) {
                case 0:  // NOP
                case 2:
                case 4:
                    break;

                case 1: {  // Non-specific EOI (ENDINT)
                    uint8_t irq = find_highest_priority_irq(isr_);
                    if (irq < 8) {
                        isr_ &= ~(1 << irq);
                    }
                    low_priority_ = 7;
                    break;
                }

                case 3: {  // Specific EOI (SPECENDINT)
                    uint8_t irq = data & 0x07;
                    isr_ &= ~(1 << irq);
                    low_priority_ = 7;
                    break;
                }

                case 5: {  // Rotate on non-specific EOI (ENDINT + rotate)
                    uint8_t irq = find_highest_priority_irq(isr_);
                    if (irq < 8) {
                        isr_ &= ~(1 << irq);
                        low_priority_ = irq;
                    }
                    break;
                }

                case 6: {  // Rotate in service (set priority)
                    low_priority_ = data & 0x07;
                    break;
                }

                case 7: {  // Rotate on specific EOI
                    uint8_t irq = data & 0x07;
                    isr_ &= ~(1 << irq);
                    low_priority_ = irq;
                    break;
                }
            }
        }

    } else {
        // A0 = 1 — последовательность ICW (пока активна) либо OCW1 (IMR)
        if (icw_load_active_) {
            // I-2: первый байт последовательности — ВСЕГДА ICW2 (база
            // векторов прерываний); второй (только при SNGL=0) — ICW3,
            // который поглощается. Автомат не зависает ни в single-,
            // ни в cascade-режиме и не теряет ICW2.
            if (icw_load_step_ == 1) {
                icw2_ = data;                     // ICW2 — база векторов
                if (icw_cascade_) {
                    icw_load_step_ = 2;           // ждём ICW3
                } else {
                    icw_load_active_ = false;     // single — готово
                    icw_load_step_ = 0;
                }
            } else {
                icw3_ = data;                     // ICW3 — конфигурация каскада (поглощается)
                icw_load_active_ = false;
                icw_load_step_ = 0;
            }
        } else {
            // OCW1 — регистр масок прерываний (IMR)
            imr_ = data;
        }
    }
    
    update_interrupt_line();
}

uint8_t Pic::read(uint8_t port) {
    if ((port & 1) == 0) {
        // A0 = 0
        if (poll_command_) {
            // Команда Poll: вернуть IRQ наивысшего приоритета с битом 7
            poll_command_ = false;
            uint8_t irq = find_highest_priority_irq(irr_);
            if (irq < 8) return irq | 0x80;
            return (low_priority_ + 1) & 0x07;
        }
        
        // Читаем IRR или ISR по биту RIS из OCW3
        if (ocw3_ & 0x02) {
            return (ocw3_ & 0x01) ? isr_ : irr_;
        }
        return irr_;  // Default
        
    } else {
        // A0 = 1 — чтение IMR
        return imr_;
    }
}

bool Pic::has_pending_interrupt(uint8_t* vector) {
    if (vector == nullptr) return false;
    
    // Поиск старшего запроса в (IRR & ~IMR) | ISR
    uint8_t combined = (irr_ & ~imr_) | isr_;
    uint8_t irq = find_highest_priority_irq(combined);
    if (irq >= 8) return false;
    
    // Отказ, если уровень совпадает с обслуживающимся — без вытеснения
    uint8_t highest_isr = find_highest_priority_irq(isr_);
    if (irq == highest_isr) return false;
    
    *vector = irq;
    return true;
}

uint16_t Pic::acknowledge_interrupt() {
    // Подтверждение прерывания (модель DoPIC):
    //   i = FindMaxBit(IRR & ~IMR | ISR);
    //   if ((i == 8) || (i == FindMaxBit(ISR))) return -1;
    //   IRR &= ~(1<<i); ISR |= 1<<i;
    //   return (picHIGH << 8) | ((picRUS & ((picRUS & 0x04)?0xe0:0xc0)) | (i << ((picRUS & 0x04)?2:3)));
    uint8_t combined = (irr_ & ~imr_) | isr_;
    uint8_t irq = find_highest_priority_irq(combined);
    if (irq >= 8) {
        // CPU запросил прерывание, но ничего pending (IMR=0xFF после init).
        // Нормально если BIOS ещё не размаскировал нужные IRQ.
        return 0xFFFF;
    }

    // Отказ, если уровень совпадает с обслуживающимся — без вытеснения
    uint8_t highest_isr = find_highest_priority_irq(isr_);
    if (irq == highest_isr) {
        return 0xFFFF;
    }

    // Снимаем бит IRR, ставим бит ISR
    irr_ &= ~(1 << irq);
    isr_ |= (1 << irq);

    // Адрес вектора: старшие биты из icw1_, сдвиг irq
    uint8_t low;
    if (icw1_ & 0x04) {
        // Шаг 4: младшие биты = (icw1_ & 0xE0) | (irq << 2)
        low = (icw1_ & 0xE0) | (irq << 2);
    } else {
        // Шаг 8: младшие биты = (icw1_ & 0xC0) | (irq << 3)
        low = (icw1_ & 0xC0) | (irq << 3);
    }

    uint16_t vector = (static_cast<uint16_t>(icw2_) << 8) | low;

    // Лог: прерывание успешно подтверждено CPU.

    return vector;
}

void Pic::update_interrupt_line() {
    // Обновляем признак: есть ли незамаскированное необслуженное прерывание
}

}  // namespace korvet
