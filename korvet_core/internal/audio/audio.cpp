/**
 * @file audio.cpp
 * @brief Синхронная аудио-подсистема эмулятора Korvet (см. audio.hpp)
 */

#include "audio.hpp"

namespace korvet {

void Audio::init() {
    reset();
}

void Audio::reset() {
    raw_ticks_.fill(0);
    raw_tick_count_ = 0;
    raw_overflow_ = false;
    sound_buf_.fill(0);
    frac_acc_ = 0;
    mute_ = false;
    enabled_ = true;
    volume_ = 100;
}

void Audio::accumulate_tick(bool high) {
    if (raw_tick_count_ >= static_cast<int>(raw_ticks_.size())) {
        // За кадр пришло больше тиков, чем физически укладывается в
        // 40000 при 2МГц/50Гц — сигнал, что где-то в другом месте
        // сбита частота/деление таймера. Не роняем эмуляцию, просто
        // отбрасываем лишнее и помечаем факт для диагностики.
        raw_overflow_ = true;
        return;
    }
    raw_ticks_[raw_tick_count_++] = high ? 1 : 0;
}

void Audio::generate_frame_buffer() {
    if (!enabled_ || mute_) {
        sound_buf_.fill(0);
        raw_tick_count_ = 0;
        raw_overflow_ = false;
        return;
    }

    int outptr = 0;
    int sample_count = 0;

    while (sample_count < FRAME_SAMPLES) {
        // Брезенхем: сколько сырых тиков читать на этот сэмпл — 90 или 91,
        // без накопления ошибки округления на длинных сессиях.
        int ticks_this_sample = TICKS_PER_SAMPLE_BASE;
        frac_acc_ += TICKS_FRAC_PER_SAMPLE;
        if (frac_acc_ >= TICKS_DEN) {
            frac_acc_ -= TICKS_DEN;
            ticks_this_sample++;
        }

        int high_count = 0;
        int ticks_read = 0;
        for (int j = 0; j < ticks_this_sample && outptr < raw_tick_count_; j++) {
            if (raw_ticks_[outptr++]) high_count++;
            ticks_read++;
        }

        if (ticks_read == 0) break;  // тики этого кадра кончились раньше времени

        // Пороговое решение (0/255) — не линейное
        // усреднение, чтобы сохранить аутентичное звучание однобитного
        // динамика Корвета.
        uint8_t pwm_byte = (high_count > ticks_read / 2) ? 255 : 0;
        int16_t sample = static_cast<int16_t>((static_cast<int>(pwm_byte) - 128) * 256
                                               * volume_ / 100);
        sound_buf_[sample_count++] = sample;
    }

    // Если сырых тиков не хватило на весь кадр (тишина/HLT) —
    // дозаполняем тишиной, а не оставляем мусор с прошлого кадра.
    for (; sample_count < FRAME_SAMPLES; sample_count++) {
        sound_buf_[sample_count] = 0;
    }

    raw_tick_count_ = 0;
    raw_overflow_ = false;
}

}  // namespace korvet
