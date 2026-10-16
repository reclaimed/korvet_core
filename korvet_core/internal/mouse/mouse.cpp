#include "mouse.hpp"
#include "../logger/logger.hpp"
#include "../serial/serial.hpp"

namespace korvet {

void Mouse::init() {
    reset();
}

void Mouse::reset() {
    accumulated_x_ = 0;
    accumulated_y_ = 0;
    buttons_ = 0;

    max_delta_ = 127;  // клэмп посылки (±127, 1 байт со знаком)

    // Режим подключения НЕ сбрасывается: это конфигурация «шнура»
    // (что подключил пользователь), а не состояние гостя. По умолчанию —
    // последовательная мышь на ИРПС (Microsoft-совместимая).

    // ММ8031: счётчики приращений в исходное состояние 1000b
    mm_cnt_x_ = 8;
    mm_cnt_y_ = 8;

    prev_buttons_ = 0;
}

void Mouse::move(int16_t dx, int16_t dy) {
    switch (mode_) {
        case mb_mouse_mode_t::MOUSE_SERIAL:
            // Накопление между кадрами — посылка уходит раз в кадр
            // (check_mouse), как кадровая модель ввода.
            accumulated_x_ += dx;
            accumulated_y_ += dy;
            break;

        case mb_mouse_mode_t::MOUSE_PARALLEL: {
            // ММ8031: реверсивные 4-разрядные счётчики. Ось Y
            // инвертирована («вверх» = положительный код) — при экранных
            // координатах «вниз = положительный dy» вычитаем.
            mm_cnt_x_ = mm_clamp(static_cast<int>(mm_cnt_x_) + dx);
            mm_cnt_y_ = mm_clamp(static_cast<int>(mm_cnt_y_) - dy);
            break;
        }

        case mb_mouse_mode_t::MOUSE_OFF:
        default:
            // Мышь не подключена — ввод игнорируется
            break;
    }
}

void Mouse::set_buttons(uint8_t buttons) {
    // Бит 0 — левая («клавиша 1»), бит 1 — правая («клавиша 2»)
    buttons_ = buttons & 0x03;
}

void Mouse::check_mouse() {
    if (mode_ != mb_mouse_mode_t::MOUSE_SERIAL) {
        return;  // последовательный путь активен только в MOUSE_SERIAL
    }
    if (!serial_) return;

    // Клампинг накопленных смещений: относительное смещение передаётся
    // одним байтом со знаком, диапазон ±127. Без клампинга быстрое
    // движение хоста «заворачивается» и летит в обратную сторону.
    int16_t dx = accumulated_x_;
    int16_t dy = accumulated_y_;
    if (dx > max_delta_) dx = max_delta_;
    else if (dx < -max_delta_) dx = -max_delta_;
    if (dy > max_delta_) dy = max_delta_;
    else if (dy < -max_delta_) dy = -max_delta_;

    // Биты первого байта по документации (приложение А к руководству):
    //   бит 7 = 0, бит 6 = 1 (маркер первого байта посылки);
    //   бит 5 (0x20) — нажата клавиша 1 (левая кнопка);
    //   бит 4 (0x10) — нажата клавиша 2 (правая кнопка);
    //   биты 3-2 — старшие биты Y, биты 1-0 — старшие биты X.
    // Кнопки — строго по документации (левая → бит 5): ПО, писавшееся
    // под реальную мышь, читает именно эту разрядку (тест-программа
    // приложения А: Button_1 = (b1 & 0x20), Button_2 = (b1 & 0x10)).
    uint8_t btn = 0;
    btn |= (buttons_ & 0x01) ? 0x20 : 0;  // левая  → бит 5
    btn |= (buttons_ & 0x02) ? 0x10 : 0;  // правая → бит 4

    // Посылка шлётся при движении ИЛИ изменении кнопок («если не
    // происходит изменения состояния клавишей или движения,
    // манипулятор молчит»).
    const bool moved = (accumulated_x_ != 0 || accumulated_y_ != 0);
    const bool buttons_changed = (btn != prev_buttons_);
    if (!moved && !buttons_changed) {
        return;
    }

    // 3-байтовый пакет:
    // Байт 1: 0x40 | кнопки | (Y>>6)<<2 | (X>>6)
    // Байт 2: X & 0x3F
    // Байт 3: Y & 0x3F
    uint8_t b1 = 0x40 | btn | ((dy & 0xC0) >> 4) | ((dx & 0xC0) >> 6);
    uint8_t b2 = dx & 0x3F;
    uint8_t b3 = dy & 0x3F;

    serial_->add_to_queue(b1);
    serial_->add_to_queue(b2);
    serial_->add_to_queue(b3);

    prev_buttons_ = btn;

    // После отправки накопленные дельты сбрасываются
    accumulated_x_ = 0;
    accumulated_y_ = 0;
}

uint8_t Mouse::read_mm8031_port_b(bool sync) {
    // ММ8031 (паспорт §2.6): считывание мультиплексором МХУ по сигналу
    // «Синхр» — при нулевом уровне выдаётся СЧХ, при единичном — СЧУ;
    // после каждого считывания формирователь Ф устанавливает счётчики
    // в исходное состояние 1000b. Кнопки SB1/SB2 («Упр1»/«Упр2») —
    // «в виде потенциала земли», активный низкий, биты 4-5.
    uint8_t cnt;
    if (sync) {
        cnt = mm_cnt_y_ & 0x0F;   // Синхр=1 → счётчик Y
        mm_cnt_y_ = 8;            // сброс после считывания
    } else {
        cnt = mm_cnt_x_ & 0x0F;   // Синхр=0 → счётчик X
        mm_cnt_x_ = 8;            // сброс после считывания
    }
    // Кнопки: бит 4 = Упр1 (левая), бит 5 = Упр2 (правая), активный низкий
    uint8_t btn_bits = static_cast<uint8_t>(((buttons_ & 0x03) ^ 0x03) << 4);
    return cnt | btn_bits;
}

void Mouse::mm8031_init_pulse() {
    // Сигнал инициализации с ПК (Порт 2.3): счётчики в исходное состояние
    mm_cnt_x_ = 8;
    mm_cnt_y_ = 8;
}

}  // namespace korvet
