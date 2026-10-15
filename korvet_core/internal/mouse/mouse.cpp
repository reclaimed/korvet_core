#include "mouse.hpp"
#include "../logger/logger.hpp"
#include "../serial/serial.hpp"

namespace korvet {

void Mouse::init() {
    reset();
}

void Mouse::reset() {
    delta_x_ = 0;
    delta_y_ = 0;
    buttons_ = 0;
    
    accumulated_x_ = 0;
    accumulated_y_ = 0;
    accumulate_ = true;
    
    max_delta_ = 127;  // Default maximum delta value
    
    mouse_system_enabled_ = false;
    interface_mode_ = InterfaceMode::STANDARD;
    current_packet_byte_ = 0;
    prev_delta_x_ = 0;
    prev_delta_y_ = 0;
    prev_buttons_ = 0;
}

void Mouse::move(int16_t dx, int16_t dy) {
    if (accumulate_) {
        accumulated_x_ += dx;
        accumulated_y_ += dy;
    } else {
        delta_x_ = dx;
        delta_y_ = dy;
        clamp_delta(delta_x_);
        clamp_delta(delta_y_);
    }
}

void Mouse::set_position(int16_t x, int16_t y) {
    // Для абсолютного позиционирования нужно хранить абсолютные координаты
    // Пока — просто сброс дельт
    delta_x_ = 0;
    delta_y_ = 0;
    accumulated_x_ = 0;
    accumulated_y_ = 0;
}

void Mouse::set_buttons(uint8_t buttons) {
    buttons_ = buttons & 0x07;  // Support up to 3 buttons (bits 0-2)
}

uint8_t Mouse::read_from_port(uint8_t port) {
    // Поведение чтения зависит от порта и режима интерфейса
    switch (interface_mode_) {
        case InterfaceMode::STANDARD:
            return read_standard();
        case InterfaceMode::MOUSE_SYSTEM:
            return read_mouse_system();
        case InterfaceMode::KORVET_MOUSE:
            return read_korvet_mouse();
    }
    return 0xFF;  // Default value
}

uint8_t Mouse::get_ppi3_port_b_value() const {
    // Значение, которое возвращается при чтении порта B PPI3
    // В оригинальной реализации здесь были бы данные мыши
    // Стандартный формат данных мыши для порта B PPI3
    
    // Стандартный интерфейс: комбинированные дельты + кнопки
    int16_t dx = delta_x_;
    int16_t dy = delta_y_;
    
    // Клэмп в 6-битный диапазон (-32..+31)
    if (dx > 31) dx = 31;
    if (dx < -32) dx = -32;
    if (dy > 31) dy = 31;
    if (dy < -32) dy = -32;
    
    // Формат по оригинальному протоколу мыши
    uint8_t result = 0x40;  // Always set bit 6 (sign bit for both X and Y)
    result |= (buttons_ & 0x03) << 4;  // Buttons in bits 5-4
    result |= ((dy & 0xC0) >> 2) & 0x30;  // Top 2 bits of Y in bits 5-4
    result |= ((dx & 0xC0) >> 4) & 0x0C;  // Top 2 bits of X in bits 3-2
    result |= 0x03;  // Set lower 2 bits to indicate movement
    
    return result;
}

uint8_t Mouse::read_standard() const {
    // Стандартный интерфейс мыши
    if (accumulate_) {
        // Дельты из накопленного движения
        int16_t dx = accumulated_x_;
        int16_t dy = accumulated_y_;
        
        // Клэмп без изменения членов класса
        if (dx > max_delta_) {
            dx = max_delta_;
        } else if (dx < -max_delta_) {
            dx = -max_delta_;
        }
        
        if (dy > max_delta_) {
            dy = max_delta_;
        } else if (dy < -max_delta_) {
            dy = -max_delta_;
        }
        
        // Дельта (зависит от того, какой байт читается)
        return static_cast<uint8_t>(dx & 0xFF);
    }
    
    return static_cast<uint8_t>(delta_x_ & 0xFF);
}

uint8_t Mouse::read_mouse_system() const {
    // Интерфейс Mouse Systems
    // 5-байтовый пакет по оригинальной спецификации
    
    switch (current_packet_byte_) {
        case 0: {
            // Байт 1: синхронизация и кнопки
            uint8_t byte = 0x80;  // Always set bit 7
            byte |= (buttons_ & 0x01) ? 0x00 : 0x01;  // Left button (active low)
            byte |= (buttons_ & 0x02) ? 0x00 : 0x04;  // Middle button (active low)
            byte |= (buttons_ & 0x04) ? 0x00 : 0x02;  // Right button (active low)
            current_packet_byte_++;
            return byte;
        }
        case 1: {
            // Байт 2: смещение X
            current_packet_byte_++;
            return static_cast<uint8_t>(delta_x_ & 0xFF);
        }
        case 2: {
            // Байт 3: смещение Y
            current_packet_byte_++;
            return static_cast<uint8_t>((256 - delta_y_) & 0xFF);
        }
        case 3: {
            // Байт 4: смещение X' (с прошлого пакета)
            current_packet_byte_++;
            return 0x00;  // Placeholder
        }
        case 4: {
            // Байт 5: смещение Y' (с прошлого пакета)
            current_packet_byte_ = 0;  // Reset for next packet
            return 0x00;  // Placeholder
        }
        default:
            current_packet_byte_ = 0;
            return 0x00;
    }
}

uint8_t Mouse::read_korvet_mouse() const {
    // Специфичный для Корвета интерфейс мыши
    // По оригинальному коду значения должны отдаваться в специальном
    // формате в последовательную очередь
    
    // Пока — стандартный формат
    return read_standard();
}

void Mouse::clamp_delta(int16_t& delta) {
    if (delta > max_delta_) {
        delta = max_delta_;
    } else if (delta < -max_delta_) {
        delta = -max_delta_;
    }
}

void Mouse::check_mouse() {
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

    // Биты первого байта по спецификации «Корвета»:
    //   bit 5 (0x20) — нажата клавиша 1 (левая кнопка),
    //   bit 4 (0x10) — нажата клавиша 2 (правая кнопка).
    // (Именно так читает тестовая программа из описания:
    //  Button_1 = (b1 & 0x20) != 0; Button_2 = (b1 & 0x10) != 0.)
    uint8_t btn = 0;
    btn |= (buttons_ & 0x01) ? 0x20 : 0;  // левая  → бит 5
    btn |= (buttons_ & 0x02) ? 0x10 : 0;  // правая → бит 4

    // Посылка шлётся при движении ИЛИ изменении кнопок (реальная мышь
    // «молчит» только когда ничего не происходит). Движение фиксируем
    // по ненулевому накопителю ДО клампинга.
    const bool moved = (accumulated_x_ != 0 || accumulated_y_ != 0);
    const bool buttons_changed = (btn != prev_buttons_);
    if (!moved && !buttons_changed) {
        return;
    }

    // 3-байтовый пакет по оригинальному ChkMouse():
    // Байт 1: 0x40 | кнопки | (Y>>6)<<2 | (X>>6)
    // Байт 2: X & 0x3F
    // Байт 3: Y & 0x3F
    uint8_t b1 = 0x40 | btn | ((dy & 0xC0) >> 4) | ((dx & 0xC0) >> 6);
    uint8_t b2 = dx & 0x3F;
    uint8_t b3 = dy & 0x3F;

    serial_->add_to_queue(b1);
    serial_->add_to_queue(b2);
    serial_->add_to_queue(b3);

    prev_delta_x_ = dx;
    prev_delta_y_ = dy;
    prev_buttons_ = btn;

    // После отправки накопленные дельты сбрасываются
    accumulated_x_ = 0;
    accumulated_y_ = 0;
}

}  // namespace korvet