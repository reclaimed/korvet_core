#include "timer.hpp"
#include "../pic/pic.hpp"
#include "../audio/audio.hpp"
#include "../logger/logger.hpp"

namespace korvet {

void Timer::init() {
    pic_ = nullptr;  // PIC подключается позже через set_pic()
    reset();
}

void Timer::reset() {
    // Сброс всех трёх каналов таймера
    for (int i = 0; i < 3; i++) {
        init_timer_counter(i);
    }

    ctrl_latch_ = 0;
    selected_channel_ = 0;

    tick_remainder_ = 0;
    ch2_remainder_ = 0;
    ch2_out_prev_ = false;
}

void Timer::init_timer_counter(int channel) {
    if (channel >= 0 && channel < 3) {
        channels_[channel].counting_element = 0;
        channels_[channel].count_register = 0;
        channels_[channel].output_latch = 0;
        channels_[channel].mode = 0;
        channels_[channel].bcd_mode = false;
        channels_[channel].rw_mode = 0;
        channels_[channel].write_phase = 0;
        channels_[channel].read_phase = 0;
        // После сброса все каналы КР580ВИ53 находятся в режиме 0 —
        // выход OUT низкий до терминального счёта (S-2: раньше ставился
        // высокий уровень — паразитное DC-смещение при открытом гейте).
        channels_[channel].out = false;
        channels_[channel].counting_active = false;
        channels_[channel].load_pending = false;
        channels_[channel].waiting_for_load = false;
    }
}

void Timer::feed_audio_out(int channel, int value) {
    // Аудио интересует только канал 0 (сигнал динамика Корвета) —
    // каналы 1/2 сюда не идут, у них нет отношения к звуку.
    if (channel != 0 || !audio_) return;

    // Стробирование звука: гейт динамика — бит 3 порта C ППР№2 (SNDEN).
    // Управляется ТОЛЬКО записью гостя; конфигурация хоста на гейт
    // не влияет (хост управляет громкостью/мьютом через Audio).
    bool high = sound_enable_ && (value != 0);
    audio_->accumulate_tick(high);
}

void Timer::tick(uint32_t cycles) {
    // Каналы 0 и 1: вход 2 МГц = такт CPU × 20/25. Накопление с дробным
    // остатком — эквивалент пакетной формулы (Takt-PrevTakt)*20/25.
    uint32_t total = cycles * 20 + tick_remainder_;
    uint32_t timer_ticks = total / 25;
    tick_remainder_ = total % 25;

    if (timer_ticks > 0) {
        tick_channel(0, timer_ticks);
        // Канал 1 — задатчик скорости ИРПС (baud): тикает от 2 МГц
        // (описание элементов: «на вход счётчика — частота 2000 кГц»).
        tick_channel(1, timer_ticks);
    }

    // Канал 2 — системный таймер (TMRINT/IRQ5). Источник тактирования
    // конфигурируется (по умолчанию HBL — см. mb_timer_ch2_clock_t):
    // источники противоречат друг другу (схема: HBL 65.6 мкс; описание
    // элементов: 50 Гц), поэтому выбор вынесен в конфигурацию хоста.
    switch (ch2_clock_) {
        case mb_timer_ch2_clock_t::MB_TIMER_CH2_HBL: {
            // 1 тик канала на HBL_CYCLES (164) тактов CPU
            uint32_t t2 = cycles + ch2_remainder_;
            uint32_t n = t2 / HBL_CYCLES;
            ch2_remainder_ = t2 % HBL_CYCLES;
            if (n > 0) tick_channel(2, static_cast<int>(n));
            break;
        }
        case mb_timer_ch2_clock_t::MB_TIMER_CH2_50HZ: {
            // 1 тик канала на кадр (TICKS_PER_FRAME тактов CPU)
            uint32_t t2 = cycles + ch2_remainder_;
            uint32_t n = t2 / TICKS_PER_FRAME;
            ch2_remainder_ = t2 % TICKS_PER_FRAME;
            if (n > 0) tick_channel(2, static_cast<int>(n));
            break;
        }
        case mb_timer_ch2_clock_t::MB_TIMER_CH2_2MHZ: {
            // Режим совместимости: канал 2 тикает от 2 МГц, как каналы 0/1
            if (timer_ticks > 0) tick_channel(2, static_cast<int>(timer_ticks));
            break;
        }
        case mb_timer_ch2_clock_t::MB_TIMER_CH2_OFF:
            // Канал заморожен (мгновенная модель совместимости)
            break;
    }

    // TMRINT (IRQ5) — по фронту выхода OUT канала 2 (см. check_interrupts)
    check_interrupts();
}

void Timer::write(uint8_t port, uint8_t data) {
    switch (port & 3) {
        case 0:  // канал 0
        case 1:  // канал 1
        case 2:  // канал 2
            write_channel_data(port & 3, data);
            break;
        case 3:  // регистр управляющего слова
            write_control_word(data);
            break;
    }
}

void Timer::write_channel_data(uint8_t channel, uint8_t data) {
    if (channel >= 3) return;

    // Побайтная запись данных в CR по фазе RW-режима.
    // ВАЖНО (S-1): при двухбайтовой загрузке (RW=3) счёт начинается только
    // после записи СТАРШЕГО байта; при однобайтовой (RW=1/2) — после
    // единственного байта. Пока первая загрузка CR не завершена, канал
    // не считает (waiting_for_load) — семантика КР580ВИ53.
    bool cr_load_complete = false;
    if (channels_[channel].rw_mode == 3) {
        // Двухбайтовая загрузка: LSB, затем MSB
        if (channels_[channel].write_phase & 0x01) {  // ждём младший байт
            channels_[channel].count_register =
                (channels_[channel].count_register & 0xff00) | data;
            channels_[channel].write_phase ^= 0x01;
        } else if (channels_[channel].write_phase & 0x02) {  // ждём старший байт
            channels_[channel].count_register =
                (channels_[channel].count_register & 0x00ff) | (data << 8);
            channels_[channel].write_phase ^= 0x02;
            cr_load_complete = true;  // старший байт записан — загрузка завершена
        }
    } else if (channels_[channel].rw_mode == 1) {
        // Только младший байт: каждая запись — полная загрузка CR
        channels_[channel].count_register =
            (channels_[channel].count_register & 0xff00) | data;
        cr_load_complete = true;
    } else if (channels_[channel].rw_mode == 2) {
        // Только старший байт: каждая запись — полная загрузка CR
        channels_[channel].count_register =
            (channels_[channel].count_register & 0x00ff) | (data << 8);
        cr_load_complete = true;
    }

    if (cr_load_complete) {
        // Загрузка CR завершена: CE перезагрузится на ближайшем тике,
        // счёт разрешается. Действует и на первую загрузку (снятие
        // waiting_for_load), и на смену коэффициента «на ходу».
        channels_[channel].load_pending = true;
        channels_[channel].counting_active = true;
        channels_[channel].waiting_for_load = false;
    }
}

void Timer::write_control_word(uint8_t value) {
    int channel = (value & 0xc0) >> 6;
    int rw_mode = (value & 0x30) >> 4;
    int mode = (value & 0x0e) >> 1;
    int bcd_flag = value & 0x01;

    if (rw_mode == 0) {  // команда защёлкивания счётчика
        if (channel < 3 && channels_[channel].read_phase == 0) {  // повторная команда игнорируется
            channels_[channel].output_latch = channels_[channel].counting_element;
            // фаза чтения = текущий rw_mode (для корректного обратного чтения)
            channels_[channel].read_phase = channels_[channel].rw_mode;
        }
    } else {  // установка режима канала
        if (mode & 0x02) mode &= 0x03;  // коды 6,7 — алиасы режимов 2,3

        channels_[channel].mode = mode;
        channels_[channel].rw_mode = rw_mode;
        channels_[channel].bcd_mode = bcd_flag != 0;
        channels_[channel].write_phase = rw_mode;  // подготовка фазы записи

        // Запись управляющего слова переводит канал в исходное состояние,
        // но счёт НЕ начинается до загрузки CR (S-1). Уровень OUT после
        // управляющего слова: режим 0 — низкий, остальные — высокий
        // (семантика i8253).
        channels_[channel].counting_active = false;
        channels_[channel].load_pending = false;
        channels_[channel].waiting_for_load = true;

        switch (channels_[channel].mode) {
            case 0: mode0_setmode(channel); break;
            case 1: mode1_setmode(channel); break;
            case 2: mode2_setmode(channel); break;
            case 3: mode3_setmode(channel); break;
            case 4: mode4_setmode(channel); break;
            case 5: mode5_setmode(channel); break;
        }
    }
}

uint8_t Timer::read(uint8_t port) {
    // Чтение значения счётчика указанного канала
    int channel = port & 3;
    if (channel < 3) {
        // Если была команда защёлкивания — отдаём защёлкнутое значение
        if (channels_[channel].read_phase > 0) {
            uint8_t result;
            if (channels_[channel].read_phase & 0x01) {  // сначала младший байт
                result = channels_[channel].output_latch & 0xFF;
                channels_[channel].read_phase ^= 0x01;
            } else if (channels_[channel].read_phase & 0x02) {  // затем старший
                result = (channels_[channel].output_latch >> 8) & 0xFF;
                channels_[channel].read_phase ^= 0x02;
            } else {
                result = 0;
            }

            // Оба байта прочитаны — восстанавливаем фазу
            if (channels_[channel].read_phase == 0) {
                channels_[channel].read_phase = channels_[channel].rw_mode;
            }

            return result;
        } else {
            // Без защёлкивания — текущее значение CE
            return static_cast<uint8_t>(channels_[channel].counting_element & 0xFF);
        }
    }
    return 0;
}

// Декремент CE: двоичный или двоично-десятичный (BCD) счёт.
// BCD: разряды-нибблы считают с заёмом 10, перенос 0000 -> 9999.
uint16_t Timer::decrement_ce(uint16_t value, bool bcd) {
    if (!bcd) {
        return static_cast<uint16_t>(value - 1);
    }
    if ((value & 0x000F) != 0) return static_cast<uint16_t>(value - 1);
    if ((value & 0x00F0) != 0) return static_cast<uint16_t>(value - 0x10 + 0x09);
    if ((value & 0x0F00) != 0) return static_cast<uint16_t>(value - 0x100 + 0x99);
    if ((value & 0xF000) != 0) return static_cast<uint16_t>(value - 0x1000 + 0x999);
    return 0x9999;  // 0000 заворачивается в 9999 (полный BCD-цикл 10000)
}

// Режим 0: прерывание по терминальному счёту (i8253 Mode 0)
void Timer::mode0_setmode(int channel) {
    channels_[channel].out = false;  // выход стартует низким
}

void Timer::mode0_write_new_value(int channel, int rw_mode) {
    (void)channel;
    (void)rw_mode;
    // Запись CR обрабатывается в write_channel_data (load_pending);
    // здесь дополнительной работы нет.
}

void Timer::mode0_do(int channel, int ticks) {
    if (channels_[channel].waiting_for_load) {
        // До первой загрузки CR канал не считает: выход держит уровень
        // после управляющего слова (низкий), тики идут в аудио канала 0.
        while (ticks--) feed_audio_out(channel, channels_[channel].out);
        return;
    }
    if (channels_[channel].load_pending) {
        channels_[channel].counting_element = channels_[channel].count_register;
        channels_[channel].load_pending = false;
        ticks--;
        channels_[channel].counting_active = true;
        channels_[channel].out = false;  // выход низкий, пока идёт счёт
    }

    while (ticks--) {
        if (channels_[channel].counting_active) {
            // Декремент счётчика (двоичный или BCD)
            if (channels_[channel].counting_element > 0) {
                channels_[channel].counting_element =
                    decrement_ce(channels_[channel].counting_element,
                                 channels_[channel].bcd_mode);

                // Выход уходит в аудиобуфер, пока идёт счёт
                feed_audio_out(channel, channels_[channel].out);
            } else {
                // Терминальный счёт (досчитали до нуля)
                channels_[channel].counting_active = false;
                channels_[channel].out = true;  // выход поднимается и держится
            }
        } else {
            // После терминального счёта выход держится высоким
            feed_audio_out(channel, channels_[channel].out);
        }
    }
}

// Режим 1: перезапускаемый аппаратный одновибратор (i8253 Mode 1)
void Timer::mode1_setmode(int channel) {
    channels_[channel].out = true;  // выход стартует высоким
}

void Timer::mode1_write_new_value(int channel, int rw_mode) {
    // Запись нового значения готовит счётчик, но отсчёт не начинается
    // до аппаратного триггера (gate). В эмуляции считаем триггером
    // завершение загрузки CR (см. write_channel_data).
    (void)channel;
    (void)rw_mode;
}

void Timer::mode1_do(int channel, int ticks) {
    if (channels_[channel].waiting_for_load) {
        while (ticks--) feed_audio_out(channel, channels_[channel].out);
        return;
    }
    if (channels_[channel].load_pending) {
        // Загружаем CE в момент триггера
        channels_[channel].counting_element = channels_[channel].count_register;
        channels_[channel].load_pending = false;
        channels_[channel].counting_active = true;  // счёт пошёл
        channels_[channel].out = false;  // выход низкий, пока идёт счёт
    }

    while (ticks--) {
        if (channels_[channel].counting_active) {
            // Декремент счётчика
            if (channels_[channel].counting_element > 0) {
                channels_[channel].counting_element =
                    decrement_ce(channels_[channel].counting_element,
                                 channels_[channel].bcd_mode);

                // В аудиобуфер, пока идёт счёт (выход низкий)
                feed_audio_out(channel, channels_[channel].out);
            } else {
                // Терминальный счёт (досчитали до нуля)
                channels_[channel].counting_active = false;
                channels_[channel].out = true;  // выход поднимается

                // Положительный фронт OUT зафиксирует check_interrupts()
                // (TMRINT по каналу 2); для канала 0 фронт не подключён.
                break;  // после терминального счёта однократный импульс завершён
            }
        } else {
            // После терминального счёта выход держится высоким
            feed_audio_out(channel, channels_[channel].out);
        }
    }
}

// Режим 2: генератор частоты (i8253 Mode 2, rate generator)
void Timer::mode2_setmode(int channel) {
    channels_[channel].out = true;  // выход стартует высоким
}

void Timer::mode2_write_new_value(int channel, int rw_mode) {
    (void)channel;
    (void)rw_mode;
}

void Timer::mode2_do(int channel, int ticks) {
    if (channels_[channel].waiting_for_load) {
        while (ticks--) feed_audio_out(channel, channels_[channel].out);
        return;
    }
    if (channels_[channel].load_pending) {
        channels_[channel].counting_element = channels_[channel].count_register;
        channels_[channel].load_pending = false;
        channels_[channel].counting_active = true;
    }

    while (ticks--) {
        if (channels_[channel].counting_active) {
            // OUT высокий весь период, низкий — один такт в терминальном
            // счёте (S-4: раньше в аудио шёл только терминальный такт —
            // наследственная неточность; теперь каждый тик отдаёт
            // реальный уровень OUT).
            uint16_t prev = channels_[channel].counting_element;
            channels_[channel].counting_element =
                decrement_ce(channels_[channel].counting_element,
                             channels_[channel].bcd_mode);
            if (prev <= 1) {
                channels_[channel].out = false;  // одиночный низкий такт
                feed_audio_out(channel, 0);
                channels_[channel].out = true;
                // Перезагрузка CE=CR (генератор частоты)
                channels_[channel].counting_element =
                    channels_[channel].count_register;
            } else {
                feed_audio_out(channel, channels_[channel].out);
            }
        } else {
            feed_audio_out(channel, channels_[channel].out);
        }
    }
}

// Режим 3: меандр (i8253 Mode 3, square wave)
void Timer::mode3_setmode(int channel) {
    channels_[channel].out = true;  // выход стартует высоким
}

void Timer::mode3_write_new_value(int channel, int rw_mode) {
    (void)channel;
    (void)rw_mode;
}

void Timer::mode3_do(int channel, int ticks) {
    if (channels_[channel].waiting_for_load) {
        while (ticks--) feed_audio_out(channel, channels_[channel].out);
        return;
    }
    if (channels_[channel].load_pending) {
        channels_[channel].counting_element = channels_[channel].count_register & 0xFFFE;
        channels_[channel].load_pending = false;
        channels_[channel].counting_active = true;
    }

    while (ticks--) {
        if (channels_[channel].counting_active) {
            if (channels_[channel].count_register & 1) {
                // НЕЧЁТНЫЙ CR — модель с полуторным чередованием:
                // чётная/нечётная половины сдвинуты на 1 (побитово
                // воспроизводит меандр ВИ53 при нечётном делителе)
                channels_[channel].counting_element = static_cast<uint16_t>(
                    channels_[channel].counting_element - 2);

                if (channels_[channel].counting_element == 1) {
                    channels_[channel].counting_element =
                        channels_[channel].count_register & 0xFFFE;
                    channels_[channel].out = false;
                }

                if (channels_[channel].counting_element == 0) {
                    if (channels_[channel].out) {
                        channels_[channel].counting_element = 3;
                    } else {
                        channels_[channel].counting_element =
                            channels_[channel].count_register & 0xFFFE;
                        channels_[channel].out = true;
                    }
                }
            } else {
                // ЧЁТНЫЙ CR — симметричный меандр
                channels_[channel].counting_element = static_cast<uint16_t>(
                    channels_[channel].counting_element - 2);
                if (channels_[channel].counting_element == 0) {
                    channels_[channel].counting_element =
                        channels_[channel].count_register;
                    channels_[channel].out = !channels_[channel].out;
                }
            }
            feed_audio_out(channel, channels_[channel].out);
        } else {
            feed_audio_out(channel, channels_[channel].out);
        }
    }
}

// Режим 4: программный строб (i8253 Mode 4)
void Timer::mode4_setmode(int channel) {
    channels_[channel].out = true;  // выход стартует высоким
}

void Timer::mode4_write_new_value(int channel, int rw_mode) {
    (void)channel;
    (void)rw_mode;
}

void Timer::mode4_do(int channel, int ticks) {
    if (channels_[channel].waiting_for_load) {
        while (ticks--) feed_audio_out(channel, channels_[channel].out);
        return;
    }
    if (channels_[channel].load_pending) {
        channels_[channel].counting_element = channels_[channel].count_register;
        channels_[channel].load_pending = false;
        channels_[channel].counting_active = true;
    }

    while (ticks--) {
        if (channels_[channel].counting_active) {
            if (channels_[channel].counting_element > 0) {
                // Пока идёт счёт, выход высокий
                feed_audio_out(channel, channels_[channel].out);

                channels_[channel].counting_element =
                    decrement_ce(channels_[channel].counting_element,
                                 channels_[channel].bcd_mode);

                if (channels_[channel].counting_element == 0) {
                    // В нуле — одиночный низкий импульс на один такт
                    channels_[channel].out = false;

                    // Низкий импульс — в аудиобуфер
                    feed_audio_out(channel, 0);

                    // После строба счёт останавливается, выход снова высокий
                    channels_[channel].counting_active = false;
                    channels_[channel].out = true;
                }
            }
        } else {
            // После строба выход держится высоким
            feed_audio_out(channel, channels_[channel].out);
        }
    }
}

// Режим 5: аппаратный перезапускаемый строб (i8253 Mode 5)
void Timer::mode5_setmode(int channel) {
    channels_[channel].out = true;  // выход стартует высоким
}

void Timer::mode5_write_new_value(int channel, int rw_mode) {
    (void)channel;
    (void)rw_mode;
}

void Timer::mode5_do(int channel, int ticks) {
    if (channels_[channel].waiting_for_load) {
        while (ticks--) feed_audio_out(channel, channels_[channel].out);
        return;
    }
    if (channels_[channel].load_pending) {
        // В режиме 5 счёт загружается по сигналу gate. В эмуляции считаем,
        // что загрузка происходит при записи значения, а счёт стартует
        // сразу после завершения загрузки CR.
        channels_[channel].counting_element = channels_[channel].count_register;
        channels_[channel].load_pending = false;
        channels_[channel].counting_active = true;
    }

    if (channels_[channel].counting_active) {
        while (ticks--) {
            if (channels_[channel].counting_element > 0) {
                // Пока идёт счёт, выход высокий
                feed_audio_out(channel, channels_[channel].out);

                channels_[channel].counting_element =
                    decrement_ce(channels_[channel].counting_element,
                                 channels_[channel].bcd_mode);

                if (channels_[channel].counting_element == 0) {
                    // В нуле — одиночный низкий импульс на один такт
                    channels_[channel].out = false;

                    // Низкий импульс — в аудиобуфер
                    feed_audio_out(channel, 0);

                    // После строба счёт останавливается, выход снова высокий
                    channels_[channel].counting_active = false;
                    channels_[channel].out = true;
                    break;  // режим 5 завершается одним циклом
                }
            }
        }
    } else {
        // Без счёта выход держится высоким
        while (ticks--) {
            feed_audio_out(channel, channels_[channel].out);
        }
    }
}

void Timer::check_interrupts() {
    // TMRINT (IRQ5): запрос прерывания формируется ПОЛОЖИТЕЛЬНЫМ ФРОНТОМ
    // выхода OUT канала 2 — контроллер прерываний защёлкивает запрос по
    // фронту. Такой модели соответствует и «однократный» запрос режима 0
    // (OUT поднимается в терминальном счёте), и периодические запросы
    // режимов 2/3 (фронт один раз за период деления). Каналы 0 (звук) и
    // 1 (задатчик скорости ИРПС) к PIC не подключены — по описанию
    // элементов TMRINT связан только с каналом 2.
    if (channels_[2].out && !ch2_out_prev_) {
        if (pic_) pic_->raise_irq(5);
    }
    ch2_out_prev_ = channels_[2].out;
}

void Timer::tick_channel(int channel, int ticks) {
    if (channel < 0 || channel >= 3 || ticks <= 0) return;
    switch (channels_[channel].mode) {
        case 0: mode0_do(channel, ticks); break;
        case 1: mode1_do(channel, ticks); break;
        case 2: mode2_do(channel, ticks); break;
        case 3: mode3_do(channel, ticks); break;
        case 4: mode4_do(channel, ticks); break;
        case 5: mode5_do(channel, ticks); break;
    }
}

}  // namespace korvet
