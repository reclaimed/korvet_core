#pragma once

#include <cstdint>
#include <array>

namespace korvet {

class Pic;
class Audio;

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
    bool counting_active;        ///< идёт счёт (после загрузки/гейта)
    bool load_pending;           ///< CE будет загружен из CR на ближайшем шаге
    bool irq_pending;            ///< терминальный счёт — поднять TMRINT (IRQ5)
    bool waiting_for_load;       ///< канал ждёт первую запись CR
};

class Timer {
public:
    static constexpr uint32_t BASE_FREQUENCY = 1250000;  // 1.25 МГц
    static constexpr uint32_t TICKS_PER_FRAME = 50000;  // Number of CPU cycles per video frame (VBLANK)
    static uint64_t total_cpu_cycles;  // Монотонный счётчик тактов на всю сессию (для advance_channel0_by_elapsed_cycles()), никогда не заворачивается
    
    void init();
    void reset();
    void tick(uint32_t cycles);
    void tick_channel(int channel, int ticks);
    bool check_and_clear_channel2_irq();
    
    uint8_t read(uint8_t port);
    void write(uint8_t port, uint8_t data);
    void write_ctrl(uint8_t data);
    
    // Каналы
    timer_channel_t get_channel(uint8_t channel) const { return channels_[channel]; }
    bool get_output(uint8_t channel) const { return channels_[channel].out; }

    // Основные функции таймера
    void advance_channel0_by_elapsed_cycles();

    // Вызывается ОДИН раз в конце Motherboard::run_frame() — гарантирует,
    // что advance_channel0_by_elapsed_cycles() не простаивает дольше одного кадра, даже если гость
    // ни разу не тронул порты таймера (типично для долгого тона/музыки:
    // канал 0 настроен один раз и дальше свободно тикает сам). Без этого
    // «разница» total_cpu_cycles-last_processed_cycles_ внутри advance_channel0_by_elapsed_cycles() накапливается
    // на десятки/сотни кадров и потом обрабатывается одним гигантским
    // синхронным циклом — реальная причина «музыка тормозит FPS».
    //
    // ВАЖНО: total_cpu_cycles монотонный (никогда не заворачивается —
    // единственное его назначение — эта разница, ни на что другое внутри
    // tick() он не влияет), поэтому никакой ручной корректировки last_processed_cycles_
    // здесь не нужно — advance_channel0_by_elapsed_cycles() сам корректно выставит
    // last_processed_cycles_=total_cpu_cycles в конце. Более ранняя версия этого
    // метода ошибочно повторяла 32-битный трюк эталона (Takt/PrevTakt
    // заворачиваются вручную раз за кадр) — но тот трюк держится на том,
    // что заворот происходит РОВНО в одном месте синхронно для обеих
    // переменных; здесь заворот tick()'а и вызов end_of_frame() были не
    // гарантированно синхронны, что и приводило к дрейфу и переполнению.
    
    // Страховочный прогон канала 0 по накопленным тактам CPU: гарантирует,
    // что звук не «застаивается» дольше кадра, если хост гонит кадры
    // только через run_frame(). Основной путь канала 0 — инкрементальный
    // tick() на каждой инструкции.
    void end_of_frame() { advance_channel0_by_elapsed_cycles(); }

    // Sound enable — стробирует AUD_OUT для отключения звука (управляется PPI2-C бит 3)
    void set_sound_enable(bool enable) { sound_enable_ = enable; }

    // Аудио: канал 0 пишет свои тики напрямую в Audio (обычный вызов
    // метода внутри korvet_core, не колбэк наружу — см. audio.hpp).
    void set_audio(Audio* audio) { audio_ = audio; }

    // Обработка прерываний
    void set_pic(Pic* pic) { pic_ = pic; }
    void check_interrupts();
    void trigger_interrupt(uint8_t channel);
    
    // Методы для тестирования
    void write_control_word(uint8_t value);

 private:
    std::array<timer_channel_t, 3> channels_;
    Pic* pic_;  // Reference to PIC for interrupt handling
    
    uint8_t ctrl_latch_;
    uint8_t selected_channel_;
    
    // Флаг разрешения звука: при false тики звука не передаются (тишина)
    bool sound_enable_ = true;

    // Аудио-приёмник тиков канала 0 (не владеем, время жизни у Motherboard)
    Audio* audio_ = nullptr;
    uint64_t last_processed_cycles_ = 0;
    uint32_t tick_remainder_;  // дробный остаток для точного накопления тиков таймера

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
};

}  // namespace korvet