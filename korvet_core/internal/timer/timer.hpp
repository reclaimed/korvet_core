#pragma once

#include <cstdint>
#include <array>

namespace korvet {

class Pic;
class Audio;

/// Источник тактирования канала 2 (TMRINT/IRQ5). Источники противоречат
/// друг другу: описание программно-доступных элементов (НИИСЧЕТМАШ,
/// 10.12.1986) указывает на вход 50 Гц; комментарии к принципиальной
/// схеме (pk8020.ru) — на сигнал HBL с периодом 65.6 мкс (15244 Гц).
/// По умолчанию принимается вариант по схеме (HBL), остальные — опции.
enum class mb_timer_ch2_clock_t : uint8_t {
    MB_TIMER_CH2_HBL = 0,   ///< HBL, период 65.6 мкс = 164 такта CPU (по умолчанию)
    MB_TIMER_CH2_50HZ = 1,  ///< кадровая частота 50 Гц (по описанию элементов)
    MB_TIMER_CH2_2MHZ = 2,  ///< 2 МГц, как у каналов 0/1 (режим совместимости)
    MB_TIMER_CH2_OFF = 3    ///< канал не тикает (совместимость с мгновенной моделью)
};

struct timer_channel_t {
    // Термины i8253 (КР580ИК53): counting element (CE), count register (CR),
    // output latch (OL) — официальные названия из документации БИС.
    uint16_t counting_element;   ///< CE — текущее значение счётчика
    uint16_t count_register;     ///< CR — записанное начальное значение
    uint16_t output_latch;       ///< OL — защёлка для чтения CE
    uint8_t mode;                ///< режим канала 0..5
    bool bcd_mode;               ///< десятичный (BCD) счёт
    uint8_t rw_mode;             ///< 0=latch, 1=lsb, 2=msb, 3=lsb+msb
    uint8_t write_phase;         ///< фаза побайтной записи в CR (зеркало rw_mode)
    uint8_t read_phase;          ///< фаза побайтного чтения из OL (зеркало rw_mode)
    bool out;                    ///< текущий уровень выхода OUT
    bool counting_active;        ///< идёт счёт (после первой загрузки CR)
    bool load_pending;           ///< CE будет загружен из CR на ближайшем шаге
    bool waiting_for_load;       ///< канал ждёт первую (полную) запись CR
};

class Timer {
public:
    static constexpr uint32_t TICKS_PER_FRAME = 50000;  ///< Тактов CPU на кадр (50 Гц)
    static constexpr uint32_t CPU_CLK = 2500000;        ///< Частота CPU
    /// Период HBL по схеме: 65.6 мкс × 2.5 МГц = 164 такта CPU.
    static constexpr uint32_t HBL_CYCLES = 164;

    void init();
    void reset();

    /// Продвижение таймера на `cycles` тактов CPU (вызывается после каждой
    /// инструкции). Каналы 0/1 тикают от 2 МГц (такт CPU × 20/25), канал 2 —
    /// от источника, выбранного set_channel2_clock().
    void tick(uint32_t cycles);

    /// Тактирование одного канала заданным числом тиков.
    void tick_channel(int channel, int ticks);

    uint8_t read(uint8_t port);
    void write(uint8_t port, uint8_t data);
    void write_ctrl(uint8_t data);

    /// Источник тактов канала 2 (TMRINT). По умолчанию HBL (65.6 мкс).
    void set_channel2_clock(mb_timer_ch2_clock_t source) { ch2_clock_ = source; }
    mb_timer_ch2_clock_t get_channel2_clock() const { return ch2_clock_; }

    // Каналы
    timer_channel_t get_channel(uint8_t channel) const { return channels_[channel]; }
    bool get_output(uint8_t channel) const { return channels_[channel].out; }

    // Sound enable — гейт динамика (PPI2-C бит 3, SNDEN). Управляется ТОЛЬКО
    // записью гостя в PPI2-C; конфигурация хоста влияет лишь на микшер Audio.
    void set_sound_enable(bool enable) { sound_enable_ = enable; }

    // Аудио: канал 0 пишет свои тики напрямую в Audio (внутренний вызов,
    // не колбэк наружу — см. audio.hpp).
    void set_audio(Audio* audio) { audio_ = audio; }

    // Обработка прерываний (TMRINT/IRQ5)
    void set_pic(Pic* pic) { pic_ = pic; }
    void check_interrupts();

    // Методы для тестирования
    void write_control_word(uint8_t value);

 private:
    std::array<timer_channel_t, 3> channels_;
    Pic* pic_ = nullptr;

    uint8_t ctrl_latch_ = 0;
    uint8_t selected_channel_ = 0;

    // Флаг гейта динамика: при false тики звука не передаются (тишина)
    bool sound_enable_ = false;

    // Аудио-приёмник тиков канала 0 (не владеем, время жизни у Motherboard)
    Audio* audio_ = nullptr;

    // Дробные остатки тактирования: каналы 0/1 (2 МГц от 2.5 МГц CPU)
    // и канал 2 (HBL — 1 тик на 164 такта CPU)
    uint32_t tick_remainder_ = 0;
    uint32_t ch2_remainder_ = 0;

    // Источник тактов канала 2 (по умолчанию HBL — см. enum выше)
    mb_timer_ch2_clock_t ch2_clock_ = mb_timer_ch2_clock_t::MB_TIMER_CH2_HBL;

    // Предыдущий уровень OUT канала 2 — детектор фронта для TMRINT
    bool ch2_out_prev_ = false;

    void write_channel_data(uint8_t channel, uint8_t data);

    // Функции отдельных режимов i8253
    void mode0_setmode(int channel);
    void mode0_write_new_value(int channel, int rw_mode);
    void mode0_do(int channel, int ticks);

    void mode1_setmode(int channel);
    void mode1_write_new_value(int channel, int rw_mode);
    void mode1_do(int channel, int ticks);

    void mode2_setmode(int channel);
    void mode2_write_new_value(int channel, int rw_mode);
    void mode2_do(int channel, int ticks);

    void mode3_setmode(int channel);
    void mode3_write_new_value(int channel, int rw_mode);
    void mode3_do(int channel, int ticks);

    void mode4_setmode(int channel);
    void mode4_write_new_value(int channel, int rw_mode);
    void mode4_do(int channel, int ticks);

    void mode5_setmode(int channel);
    void mode5_write_new_value(int channel, int rw_mode);
    void mode5_do(int channel, int ticks);

    void init_timer_counter(int channel);

    // Вспомогательный метод — передаёт тик в аудио на каждом шаге
    void feed_audio_out(int channel, int value);

    // Декремент CE с учётом BCD-режима канала (двоично-десятичный счёт,
    // бит 0 управляющего слова; описание элементов: BCDC=01H)
    static uint16_t decrement_ce(uint16_t value, bool bcd);
};

}  // namespace korvet
