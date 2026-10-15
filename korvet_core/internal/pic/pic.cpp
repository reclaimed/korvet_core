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
    // Как эталонный FindMaxBit_Ini() с picLowINT
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
            // ICW1 — команда инициализации
            icw1_ = data;
            icw_load_active_ = true;

            // Сброс состояния (как в эталоне)
            low_priority_ = 7;
            poll_command_ = false;
            special_mask_mode_ = false;
            imr_ = 0;
            isr_ = 0;

            // ФИКС A3: оригинал (pic.c:143) использует каскад для защиты ICW4.
            // icw1_ & 0x01 = ICW4 нужна — устанавливаем icw_load_step_=1 (один доп. байт).
            // В нашей single-mode без этого байт ICW4 пишется в IMR и маскирует IRQ.
            if (!(data & 0x02)) icw_load_step_ = 2;  // SNGL=0: multi-controller mode
            if (data & 0x01) {
                // нужен ICW4
                if (icw_load_step_ != 2) {
                    icw_load_step_ = 1;  // ожидаем один байт ICW4 после ICW2
                }
            }

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
        // A0 = 1 — как case 1 эталонного PIC_Write
        if (icw_load_active_) {
            if (icw_load_step_ == 2) {
                icw2_ = data;           // ICW2 — vector base high byte
            } else if (icw_load_step_ == 1) {
                // ФИКС A3: может быть ICW3 (cascade) ИЛИ ICW4 (single + ICW4 needed).
                // Смотрим icw1_ & 0x02 (SNGL) и icw1_ & 0x01 (ICW4 нужна).
                if (icw1_ & 0x02) {
                    // Single controller mode: ICW4 ожидается
                    if (icw1_ & 0x01) {
                        icw4_ = data;     // ICW4 — environment
                        icw_load_active_ = false;
                        icw_load_step_ = 0;
                    } else {
                        icw_load_active_ = false; // ICW3 не ожидается
                        icw_load_step_ = 0;
                    }
                } else {
                    icw_load_active_ = false;     // ICW3 — slave configuration (consumed)
                    icw_load_step_ = 0;
                }
            } else {
                // ФИКС A3: ICW2 в single mode. Если ICW4 нужна, остаёмся в icw_load_active_.
                icw2_ = data;
                if (icw1_ & 0x01) {
                    // ожидается ICW4: остаёмся в режиме программирования
                    icw_load_step_ = 1;
                } else {
                    icw_load_active_ = false;
                    icw_load_step_ = 0;
                }
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
    
    // Как эталонный DoPIC/CheckPIC: ищем старший в (IRR & ~IMR) | ISR
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
    // Как эталонный DoPIC():
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
