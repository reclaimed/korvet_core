#include "joystick.hpp"

namespace korvet {

void Joystick::init() {
    reset();
}

void Joystick::reset() {
    connected_ = false;
    buttons_ = 0;
    direction_left_ = false;
    direction_right_ = false;
    direction_up_ = false;
    direction_down_ = false;
    current_axis_ = 0;
    axis_active_ = false;
    type_ = JoystickType::DIGITAL_DEFLECTOR;  // Default type
}

void Joystick::set_direction_left(bool active) {
    direction_left_ = active;
}

void Joystick::set_direction_right(bool active) {
    direction_right_ = active;
}

void Joystick::set_direction_up(bool active) {
    direction_up_ = active;
}

void Joystick::set_direction_down(bool active) {
    direction_down_ = active;
}

void Joystick::set_button(uint8_t button, bool pressed) {
    if (button < MAX_BUTTONS) {
        if (pressed) {
            buttons_ |= (1 << button);
        } else {
            buttons_ &= ~(1 << button);
        }
    }
}

// метод get_axis больше не нужен для цифрового джойстика

uint8_t Joystick::read() {
    // Состояние джойстика в формате, совместимом с оригинальной системой
    // (значение строится из текущего состояния)
    
    if (!connected_) {
        return 0xFF;  // No joystick connected
    }
    
    // Формат зависит от типа джойстика
    switch (type_) {
        case JoystickType::DIGITAL_DEFLECTOR:
            return format_for_deflector();
        case JoystickType::ANALOG_KORVET:
            return format_for_analog();
        case JoystickType::GAMEPAD:
            // Переходим на формат по умолчанию
        default:
            // Цифровой джойстик: упаковываем направления и кнопки
            uint8_t result = 0;
            
            // Направления: бит 0 = ВВЕРХ, бит 1 = ВНИЗ, бит 2 = ВЛЕВО, бит 3 = ВПРАВО
            if (direction_up_) result |= 0x01;
            if (direction_down_) result |= 0x02;
            if (direction_left_) result |= 0x04;
            if (direction_right_) result |= 0x08;
            
            // Кнопки — в старшем нибле
            result |= (buttons_ & 0x0F) << 4;
            
            return result;
    }
}

uint8_t Joystick::read_for_ppi3() const {
    // Специальный формат для PPI3 (как в оригинальной системе)
    return read_ppi3_port_b();
}

uint8_t Joystick::read_ppi3_port_b() const {
    // Состояние джойстика в формате порта B PPI3
    // Соответствует оригинальной реализации (joystick.c)
    
    if (!connected_) {
        return 0xFF;  // No joystick connected
    }
    
    // По оригинальной функции Read_Joystick()
    uint8_t port = 0;
    
    // Кнопки (поддерживаются до 4)
    for (int c = 0; c < 4 && c < MAX_BUTTONS; c++) {
        if (buttons_ & (1 << c)) {
            port |= 0x10;  // Button pressed
            break;  // Only first button affects this bit
        }
    }
    
    // Цифровые направления вместо аналоговых осей
    // Влево: бит 2
    if (direction_left_) {
        port |= 0x04;  // Left
    }
    
    // Вправо: бит 3
    if (direction_right_) {
        port |= 0x08;  // Right
    }
    
    // Вверх: бит 0
    if (direction_up_) {
        port |= 0x01;  // Up
    }
    
    // Вниз: бит 1
    if (direction_down_) {
        port |= 0x02;  // Down
    }
    
    // Инверсия — как в оригинальной реализации
    return port ^ 0xFF;
}

uint8_t Joystick::format_for_deflector() const {
    // Формат «Дефлектор» (цифровой джойстик) — как в оригинале
    uint8_t port = 0;
    
    // Проверяем нажатие кнопок
    for (int c = 0; c < 4 && c < MAX_BUTTONS; c++) {
        if (buttons_ & (1 << c)) {
            port |= 0x10;  // Button pressed
            break;
        }
    }
    
    // Цифровые направления вместо аналоговых осей
    // Влево: бит 2
    if (direction_left_) {
        port |= 0x04;
    }
    
    // Вправо: бит 3
    if (direction_right_) {
        port |= 0x08;
    }
    
    // Вверх: бит 0
    if (direction_up_) {
        port |= 0x01;
    }
    
    // Вниз: бит 1
    if (direction_down_) {
        port |= 0x02;
    }
    
    // Инверсия — как в оригинальной реализации
    return port ^ 0xFF;
}

uint8_t Joystick::format_for_analog() const {
    // Формат аналогового джойстика
    // Для цифрового — цифровые состояния как квантованные «аналоговые» значения
    uint8_t x_val = 0x80;  // Center value
    uint8_t y_val = 0x80;  // Center value
    
    // Маппинг направлений в приближённые аналоговые значения
    if (direction_left_) {
        x_val = 0x20;  // Far left
    } else if (direction_right_) {
        x_val = 0xE0;  // Far right
    }
    
    if (direction_up_) {
        y_val = 0x20;  // Far up
    } else if (direction_down_) {
        y_val = 0xE0;  // Far down
    }
    
    return (x_val >> 4) | ((y_val >> 4) << 4);
}

Joystick::JoystickStatus Joystick::get_status() const {
    JoystickStatus status;
    status.connected = connected_;
    status.left_active = direction_left_;
    status.right_active = direction_right_;
    status.up_active = direction_up_;
    status.down_active = direction_down_;
    status.button_state = buttons_;
    status.type = static_cast<uint8_t>(type_);
    
    return status;
}

}  // namespace korvet