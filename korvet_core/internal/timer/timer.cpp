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
        channels_[channel].out = true;  // после сброса выход высокий (спецификация i8253)
        channels_[channel].counting_active = false;
        channels_[channel].load_pending = false;
        channels_[channel].irq_pending = false;
        channels_[channel].waiting_for_load = false;
    }
}

uint64_t Timer::total_cpu_cycles = 0;  // определение статического члена

void Timer::feed_audio_out(int channel, int value) {
    // Аудио интересует только канал 0 (сигнал динамика Корвета) —
    // каналы 1/2 сюда не идут, у них нет отношения к звуку.
    if (channel != 0 || !audio_) return;

    // Стробирование звука: если sound_enable_ выключен (PPI2-C бит 3=0),
    // то принудительно передаём тишину (value=0), а не реальный выход канала.
    // Это соответствует оригинальному emu80/Korvet.cpp, где звук отключается
    // битом 3 порта PPI2-C (SoundEnable).
    bool high = sound_enable_ && (value != 0);
    audio_->accumulate_tick(high);
}

void Timer::tick(uint32_t cycles) {

    // Монотонный счётчик тактов на всю сессию — НЕ заворачивается.
    // Используется исключительно в advance_channel0_by_elapsed_cycles() (см. timer.hpp::end_of_frame())
    // для разницы total_cpu_cycles-last_processed_cycles_. До ФИКСА здесь был заворот
    // (-=TICKS_PER_FRAME), синхронизированный по смыслу с эталонным Takt/PrevTakt,
    // но в реальности заворот tick() (каждая инструкция) и вызов
    // end_of_frame() (раз в кадр) не гарантированно синхронны — это
    // приводило к дрейфу last_processed_cycles_ и переполнению uint32_t при вычитании
    // (см. HANDOFF/находка с gdb: takt=33329551 и takt=103347425).
    // Простой и надёжный фикс — счётчик, который просто никогда не
    // оборачивается (uint64_t хватит на многие часы непрерывной работы).
    total_cpu_cycles += cycles;

    // Накопление с дробным остатком — эквивалент пакетной формулы эталона
    // DoTMR(0, (Takt-PrevTakt)*20/25), вызываемой раз в кадр: при
    // по-инструкционном (cycles*20)/25 усечение теряло бы тики, поэтому
    // остаток сохраняется между вызовами.
    uint32_t total = cycles * 20 + tick_remainder_;
    uint32_t timer_ticks = total / 25;
    tick_remainder_ = total % 25;

    if (timer_ticks > 0) {
        // ФИКС A2: тикаем ВСЕ каналы, не только 0 (звук).
        // check_interrupts() ждёт channels_[2].irq_pending для IRQ5,
        // но канал 2 никогда не тикает → IRQ5 heartbeat не приходит → BIOS зависает в EI;HLT.
        switch (channels_[0].mode) {
            case 0: mode0_do(0, timer_ticks); break;
            case 1: mode1_do(0, timer_ticks); break;
            case 2: mode2_do(0, timer_ticks); break;
            case 3: mode3_do(0, timer_ticks); break;
            case 4: mode4_do(0, timer_ticks); break;
            case 5: mode5_do(0, timer_ticks); break;
        }

        // ФИКС A2: канал 2 (TMRINT/IRQ5) — системный heartbeat.
        // Если BIOS его программирует (обычно mode 2/3, rate generator),
        // он должен тикать и периодически вызывать trigger_interrupt(2).
        tick_channel(2, timer_ticks);

        // ФИКС (v1.2): КАНАЛ 1 обязан тикать. Комментарий «channel 1 for
        // baud rate» был неверен: игры (CHESS.COM) программируют канал 1
        // (управляющее слово 0x64 = канал 1, mode 2) под собственные
        // таймерные прерывания. Канал 1 тоже поднимает TMRINT (IRQ5).
        tick_channel(1, timer_ticks);
    }

    // Проверка ожидающих прерываний (TMRINT/IRQ5)
    check_interrupts();

}

void Timer::write(uint8_t port, uint8_t data) {
    // ФИКС ДРЕБЕЗГА ЗВУКА: advance_channel0_by_elapsed_cycles() раньше вызывался здесь безусловно —
    // но tick() (каждая инструкция, см. выше) УЖЕ инкрементально гонит
    // mode_do(0, ...) для канала 0 в реальном времени. advance_channel0_by_elapsed_cycles() делает
    // РОВНО ТО ЖЕ САМОЕ ещё раз, задним числом — то есть каждая запись в
    // порт таймера (а после фикса переполнения ещё и каждый кадр через
    // end_of_frame()) ДВАЖДЫ прогоняла один и тот же промежуток времени
    // через генератор канала 0. Для устойчивого тона это давало
    // наложение двух неполностью совпадающих копий сигнала — отсюда и
    // "дребезг"/DTMF-подобное звучание на звуках дольше ~0.3с (на
    // коротких щелчках дублирование слишком короткое, чтобы услышать).
    
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
    
    switch (channels_[channel].mode) {
        case 0: mode0_write_new_value(channel, channels_[channel].write_phase); break;
        case 1: mode1_write_new_value(channel, channels_[channel].write_phase); break;
        case 2: mode2_write_new_value(channel, channels_[channel].write_phase); break;
        case 3: mode3_write_new_value(channel, channels_[channel].write_phase); break;
        case 4: mode4_write_new_value(channel, channels_[channel].write_phase); break;
        case 5: mode5_write_new_value(channel, channels_[channel].write_phase); break;
    }
    
    // Побайтная запись данных в CR по фазе RW-режима
    if (channels_[channel].write_phase & 0x01) {  // 01, 11 — младший байт
        channels_[channel].count_register = (channels_[channel].count_register & 0xff00) | data;
        channels_[channel].write_phase ^= 0x01;
    } else if (channels_[channel].write_phase & 0x02) {  // 10, 11 — старший байт
        channels_[channel].count_register = (channels_[channel].count_register & 0x00ff) | (data << 8);
        channels_[channel].write_phase ^= 0x02;
    }
    
    if (channels_[channel].write_phase == 0) {
        channels_[channel].write_phase = channels_[channel].rw_mode;
    }
    
    // При двухбайтовой записи CR загружаем CE сразу после второго байта
    if (channels_[channel].rw_mode == 3 && channels_[channel].write_phase == 0) {
        channels_[channel].counting_element = channels_[channel].count_register;
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
        channels_[channel].bcd_mode = bcd_flag;
        channels_[channel].write_phase = rw_mode;  // подготовка фазы записи
        
        switch (channels_[channel].mode) {
            case 0: mode0_setmode(channel); break;
            case 1: mode1_setmode(channel); break;
            case 2: mode2_setmode(channel); break;
            case 3: mode3_setmode(channel); break;
            case 4: mode4_setmode(channel); break;
            case 5: mode5_setmode(channel); break;
        }
        channels_[channel].waiting_for_load = true;  // ждём первую запись CR
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

void Timer::advance_channel0_by_elapsed_cycles() {
    // Продвигаем только канал 0 (как DoTMR(0, N) в эталоне). Метод —
    // страховка: tick() уже гонит канал 0 инкрементально, вызов актуален
    // только снаружи (end_of_frame()).
    uint32_t timer_ticks = ((total_cpu_cycles - last_processed_cycles_) * 20) / 25;
    
    switch (channels_[0].mode) {
        case 0: mode0_do(0, timer_ticks); break;
        case 1: mode1_do(0, timer_ticks); break;
        case 2: mode2_do(0, timer_ticks); break;
        case 3: mode3_do(0, timer_ticks); break;
        case 4: mode4_do(0, timer_ticks); break;
        case 5: mode5_do(0, timer_ticks); break;
    }
    
    last_processed_cycles_ = total_cpu_cycles;
}

// Режим 0: прерывание по терминальному счёту (i8253 Mode 0)
void Timer::mode0_setmode(int channel) {
    channels_[channel].out = 0;  // выход стартует низким
    channels_[channel].load_pending = true;
    channels_[channel].counting_active = true;
    channels_[channel].irq_pending = false;
}

void Timer::mode0_write_new_value(int channel, int rw_mode) {
    if (rw_mode == 3) {  // двухбайтовая загрузка
        // В режиме 0 запись в счётчик не меняет выход;
        // CE загрузится на ближайшем такте
        channels_[channel].load_pending = true;
        channels_[channel].counting_active = true;
    } else {
        channels_[channel].load_pending = true;
        channels_[channel].counting_active = true;
    }
}

void Timer::mode0_do(int channel, int ticks) {
    if (channels_[channel].load_pending) {
        channels_[channel].counting_element = channels_[channel].count_register;
        channels_[channel].load_pending = false;
        ticks--;
        channels_[channel].waiting_for_load = false;
        
        // В режиме 0 загрузка счёта сразу запускает счёт
        channels_[channel].counting_active = true;
        channels_[channel].out = 0;  // выход низкий, пока идёт счёт
    }
    
    while (ticks--) {
        if (channels_[channel].counting_active) {
            // Декремент счётчика
            if (channels_[channel].counting_element > 0) {
                channels_[channel].counting_element--;
                
                // Выход уходит в аудиобуфер, пока идёт счёт
                feed_audio_out(channel, channels_[channel].out);
            } else {
                // Терминальный счёт (досчитали до нуля)
                channels_[channel].counting_active = false;
                channels_[channel].out = 1;  // выход поднимается
            }
        } else {
            // После терминального счёта выход держится высоким
            feed_audio_out(channel, channels_[channel].out);
        }
    }
}

// Режим 1: перезапускаемый аппаратный одновибратор (i8253 Mode 1)
void Timer::mode1_setmode(int channel) {
    channels_[channel].out = 1;  // выход стартует высоким
    channels_[channel].counting_active = false;
    channels_[channel].irq_pending = false;
    channels_[channel].load_pending = false;  // загрузки пока не ждём
}

void Timer::mode1_write_new_value(int channel, int rw_mode) {
    // Запись нового значения готовит счётчик, но отсчёт не начинается
    // до аппаратного триггера (gate). В эталоне — лишь подготовка
    // к следующему триггеру
    channels_[channel].load_pending = true;
}

void Timer::mode1_do(int channel, int ticks) {
    // Режим 1 аппаратно-триггерный; для эмуляции считаем, что триггер
    // происходит в момент загрузки счёта
    
    if (channels_[channel].load_pending) {
        // Загружаем CE в момент триггера
        channels_[channel].counting_element = channels_[channel].count_register;
        channels_[channel].load_pending = false;
        channels_[channel].counting_active = true;  // счёт пошёл
        channels_[channel].out = 0;  // выход низкий, пока идёт счёт
    }
    
    while (ticks--) {
        if (channels_[channel].counting_active) {
            // Декремент счётчика
            if (channels_[channel].counting_element > 0) {
                channels_[channel].counting_element--;
                
                // В аудиобуфер, пока идёт счёт (выход низкий)
                feed_audio_out(channel, channels_[channel].out);
            } else {
                // Терминальный счёт (досчитали до нуля)
                channels_[channel].counting_active = false;
                channels_[channel].out = 1;  // выход поднимается
                
                // В режиме 1 терминальный счёт взводит прерывание
                if (!channels_[channel].irq_pending) {
                    channels_[channel].irq_pending = true;
                }
                break; // после терминального счёта однократный импульс завершён
            }
        } else {
            // После терминального счёта выход держится высоким
            feed_audio_out(channel, channels_[channel].out);
        }
    }
}

// Режим 2: генератор частоты (i8253 Mode 2, rate generator)
void Timer::mode2_setmode(int channel) {
    channels_[channel].out = 1;  // выход стартует высоким
    channels_[channel].load_pending = true;
    channels_[channel].counting_active = true;
    channels_[channel].irq_pending = false;
}

void Timer::mode2_write_new_value(int channel, int rw_mode) {
    if (rw_mode == 3) {  // двухбайтовая загрузка
        channels_[channel].load_pending = true;
        channels_[channel].counting_active = true;
    } else {
        channels_[channel].load_pending = true;
        channels_[channel].counting_active = true;
    }
}

void Timer::mode2_do(int channel, int ticks) {
    if (channels_[channel].load_pending) {
        channels_[channel].counting_element = channels_[channel].count_register;
        channels_[channel].load_pending = false;
        channels_[channel].waiting_for_load = false;
        channels_[channel].counting_active = true;
    }
    
    while (ticks--) {
        if (channels_[channel].counting_active) {
            // Эталон: if (1 == CountingElement--) { ADD_OUT(Counter,0); reload; }
            // feed_audio_out(…,0) один раз за период (в терминальном счёте)
            int prev = channels_[channel].counting_element;
            channels_[channel].counting_element--;
            if (prev == 1) {
                feed_audio_out(channel, 0);
                channels_[channel].counting_element = channels_[channel].count_register;
            }
        } else {
            feed_audio_out(channel, 1);
        }
    }
}

// Режим 3: меандр (i8253 Mode 3, square wave)
void Timer::mode3_setmode(int channel) {
    channels_[channel].out = 1;  // выход стартует высоким
    channels_[channel].load_pending = true;
    channels_[channel].counting_active = true;
    channels_[channel].irq_pending = false;
}

void Timer::mode3_write_new_value(int channel, int rw_mode) {
    if (rw_mode == 3) {  // двухбайтовая загрузка
        channels_[channel].load_pending = true;
        channels_[channel].counting_active = true;
    } else {
        channels_[channel].load_pending = true;
        channels_[channel].counting_active = true;
    }
}

void Timer::mode3_do(int channel, int ticks) {
    if (channels_[channel].load_pending) {
        channels_[channel].counting_element = channels_[channel].count_register & 0xFFFE;
        channels_[channel].load_pending = false;
        channels_[channel].waiting_for_load = false;
        channels_[channel].counting_active = true;
    }
    
    while (ticks--) {
        if (channels_[channel].counting_active) {
            if (channels_[channel].count_register & 1) {
                // НЕЧЁТНЫЙ CR — по эталону timer.c, строки 188-204
                channels_[channel].counting_element -= 2;
                
                if (channels_[channel].counting_element == 1) {
                    channels_[channel].counting_element = channels_[channel].count_register & 0xFFFE;
                    channels_[channel].out = 0;
                }
                
                if (channels_[channel].counting_element == 0) {
                    if (channels_[channel].out == 1) {
                        channels_[channel].counting_element = 3;
                    } else {
                        channels_[channel].counting_element = channels_[channel].count_register & 0xFFFE;
                        channels_[channel].out = 1;
                    }
                }
            } else {
                // ЧЁТНЫЙ CR — по эталону timer.c, строки 205-211
                channels_[channel].counting_element -= 2;
                if (channels_[channel].counting_element == 0) {
                    channels_[channel].counting_element = channels_[channel].count_register;
                    channels_[channel].out ^= 1;
                }
            }
            feed_audio_out(channel, channels_[channel].out);
        } else {
            feed_audio_out(channel, 1);
        }
    }
}

// Режим 4: программный строб (i8253 Mode 4)
void Timer::mode4_setmode(int channel) {
    channels_[channel].out = 1;  // выход стартует высоким
    channels_[channel].load_pending = true;
    channels_[channel].counting_active = true;
    channels_[channel].irq_pending = false;
}

void Timer::mode4_write_new_value(int channel, int rw_mode) {
    if (rw_mode == 3) {  // двухбайтовая загрузка
        channels_[channel].load_pending = true;
        channels_[channel].counting_active = true;
    } else {
        channels_[channel].load_pending = true;
        channels_[channel].counting_active = true;
    }
}

void Timer::mode4_do(int channel, int ticks) {
    if (channels_[channel].load_pending) {
        channels_[channel].counting_element = channels_[channel].count_register;
        channels_[channel].load_pending = false;
        channels_[channel].waiting_for_load = false;
        channels_[channel].counting_active = true;
    }
    
    while (ticks--) {
        if (channels_[channel].counting_active) {
            if (channels_[channel].counting_element > 0) {
                // Пока идёт счёт, выход высокий
                feed_audio_out(channel, channels_[channel].out);
                
                channels_[channel].counting_element--;
                
                if (channels_[channel].counting_element == 0) {
                    // В нуле — одиночный низкий импульс на один такт
                    channels_[channel].out = 0;
                    
                    // Низкий импульс — в аудиобуфер
                    feed_audio_out(channel, 0);
                    
                    // После строба счёт останавливается, выход снова высокий
                    channels_[channel].counting_active = false;
                    channels_[channel].out = 1;
                }
            }
        } else {
            // После строба выход держится высоким
            feed_audio_out(channel, 1);
        }
    }
}

// Режим 5: аппаратный перезапускаемый строб (i8253 Mode 5)
void Timer::mode5_setmode(int channel) {
    channels_[channel].out = 1;  // выход стартует высоким
    channels_[channel].counting_active = false;  // без триггера счёта нет
    channels_[channel].load_pending = false;  // загрузки не ждём
    channels_[channel].irq_pending = false;
}

void Timer::mode5_write_new_value(int channel, int rw_mode) {
    // Пустая функция — originals/timer.c:254
    // В оригинале mode5_WriteNewValue() не делает ничего.
    // Запись нового значения подготавливает счётчик, но не начинает отсчёт
    // до аппаратного триггера (gate).
    (void)channel;
    (void)rw_mode;
}

void Timer::mode5_do(int channel, int ticks) {
    if (channels_[channel].load_pending) {
        // В режиме 5 счёт загружается по сигналу gate. В эмуляции считаем,
        // что загрузка происходит при записи значения, а счёт стартует
        // по внешнему триггеру (см. ниже)
        channels_[channel].counting_element = channels_[channel].count_register;
        channels_[channel].load_pending = false;
        // На реальном железе счёт стартует триггером gate
    }
    
    // Режим 5 требует внешнего триггера; в эмуляции считаем триггером
    // наличие загруженного значения CR
    if (channels_[channel].count_register != 0 && !channels_[channel].counting_active) {
        channels_[channel].counting_active = true;
        channels_[channel].counting_element = channels_[channel].count_register;
    }
    
    if (channels_[channel].counting_active) {
        while (ticks--) {
            if (channels_[channel].counting_element > 0) {
                // Пока идёт счёт, выход высокий
                feed_audio_out(channel, channels_[channel].out);
                
                channels_[channel].counting_element--;
                
                if (channels_[channel].counting_element == 0) {
                    // В нуле — одиночный низкий импульс на один такт
                    channels_[channel].out = 0;
                    
                    // Низкий импульс — в аудиобуфер
                    feed_audio_out(channel, 0);
                    
                    // После строба счёт останавливается, выход снова высокий
                    channels_[channel].counting_active = false;
                    channels_[channel].out = 1;
                    break; // режим 5 завершается одним циклом
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
    // Каналы 1 и 2 генерируют IRQ5 (TMRINT) — системный таймер.
    // Канал 0 используется для звука (IRQ не поднимает).
    // v1.2: канал 1 добавлен — см. tick() (игры программируют его для
    // собственных таймерных прерываний: CHESS.COM, mode 2 + HLT).
    if (channels_[1].irq_pending) {
        trigger_interrupt(1);
        channels_[1].irq_pending = false;
    }
    if (channels_[2].irq_pending) {
        trigger_interrupt(2);
        channels_[2].irq_pending = false;
    }
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

bool Timer::check_and_clear_channel2_irq() {
    // true, если выход канала 2 высокий (активен IRQ5 / TMRINT)
    return channels_[2].out;
}

void Timer::trigger_interrupt(uint8_t channel) {
    // По схеме «Корвета»:
    //   канал 2 подключён к IRQ5 (TMRINT) — прерывание системного таймера;
    //   канал 0 — звук (в режимах 0/4 тоже может поднять IRQ);
    //   канал 1 исторически считался генератором baud rate, но игры
    //   программируют его под собственные прерывания (см. tick()).
    if (pic_ != nullptr) {
        uint8_t irq = 5;  // прерывание таймера (IRQ5)
        
        switch (channel) {
            case 0:
                // Терминальный счёт в режимах 0/4 — тоже TMRINT
                irq = 5;
                break;
            case 1:
                // Канал 1 поднимает тот же TMRINT (совместимость)
                irq = 5;
                break;
            case 2:
                // Основной источник системного прерывания (IRQ5, TMRINT)
                irq = 5;
                break;
            default:
                irq = 5;  // значение по умолчанию
                break;
        }
        
        pic_->raise_irq(irq);
    }
}

}  // namespace korvet