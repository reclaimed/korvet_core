/**
 * @file audio.hpp
 * @brief Аудио-подсистема эмулятора Korvet — синхронная, по кадрам.
 *
 * Модель 1:1 с эталоном (timer.c::MakeSound()): за время кадра сюда
 * копятся сырые тики канала 0 таймера (accumulate_tick), а в конце
 * кадра generate_frame_buffer() одним проходом даунсэмплит их в
 * ~20мс готового звука. Буфер синхронный — каждый вызов
 * generate_frame_buffer() полностью перезаписывает предыдущий кадр.
 * Никакой очереди, никакой асинхронности: не забрал звук внешний код
 * до следующего кадра — значит не забрал, ядру всё равно.
 *
 * Даунсэмплинг — точная дробная схема (алгоритм Брезенхема) вместо
 * целочисленного усечения TICKS_NUM/TICKS_DEN, чтобы ошибка не
 * накапливалась на длинных сессиях.
 */
#pragma once

#include <cstdint>
#include <array>

namespace korvet {

class Audio {
public:
    static constexpr int SAMPLE_RATE = 22050;
    static constexpr int TICKS_NUM = 2000000;             // тиков/сек (2 МГц)
    static constexpr int TICKS_DEN = SAMPLE_RATE;
    static constexpr int TICKS_PER_SAMPLE_BASE = TICKS_NUM / TICKS_DEN;  // 90
    static constexpr int TICKS_FRAC_PER_SAMPLE = TICKS_NUM % TICKS_DEN;  // 15500
    static constexpr int FRAME_SAMPLES = SAMPLE_RATE / 50;               // 441 (~20мс)
    static constexpr int MAX_RAW_TICKS = TICKS_NUM / 50 + 256;           // 40000 + запас

    void init();
    void reset();

    // Вызывается из Timer::feed_audio_out() на каждый тик канала 0 — просто
    // копит сырые 0/1 в буфер кадра. Никакого колбэка наружу.
    void accumulate_tick(bool high);

    // Вызывается ОДИН раз в конце Motherboard::run_frame() (как
    // MakeSound() в эталоне). Даунсэмплит накопленные тики в
    // sound_buf_ (полностью перезаписывает), затем сбрасывает счётчик
    // сырых тиков для следующего кадра.
    void generate_frame_buffer();

    // Синхронный плоский доступ к готовому кадру. Формат — 16-бит
    // signed PCM, моно, SAMPLE_RATE Гц — то, что обычно напрямую
    // проглатывает Qt/host audio API без пересэмплинга.
    const int16_t* get_frame_buffer() const { return sound_buf_.data(); }
    int get_frame_buffer_size() const { return FRAME_SAMPLES; }

    void set_mute(bool muted) { mute_ = muted; }
    bool is_muted() const { return mute_; }

    void set_volume(uint8_t volume_percent) { volume_ = volume_percent; }
    uint8_t get_volume() const { return volume_; }

    void set_sound_enabled(bool enabled) { enabled_ = enabled; }
    bool is_sound_enabled() const { return enabled_; }

private:
    // Сырые тики текущего кадра (0/1), пишутся accumulate_tick(),
    // читаются и обнуляются generate_frame_buffer().
    std::array<uint8_t, MAX_RAW_TICKS> raw_ticks_{};
    int raw_tick_count_ = 0;
    bool raw_overflow_ = false;  // true если за кадр тиков пришло больше MAX_RAW_TICKS

    std::array<int16_t, FRAME_SAMPLES> sound_buf_{};

    // Дробный аккумулятор даунсэмплинга (Брезенхем) — состояние
    // переносится между кадрами, чтобы не копить дрейф на границах.
    int frac_acc_ = 0;

    bool mute_ = false;
    bool enabled_ = true;
    uint8_t volume_ = 100;
};

}  // namespace korvet
