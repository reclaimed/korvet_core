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
            buttons_ |= static_cast<uint8_t>(1 << button);
        } else {
            buttons_ = static_cast<uint8_t>(buttons_ & ~(1 << button));
        }
    }
}

uint8_t Joystick::read_ppi3_port_b() const {
    // Пассивные замыкатели МД8030 (через мультиплексор ЭУ8040, слот ХР2):
    //   биты 0-3 — «Вверх/Вниз/Влево/Вправо», активный низкий;
    //   бит 4 — «Пуск1» (при нажатии ЛЮБОЙ кнопки — совместимость с ПО,
    //           отлаженным на модели, где все кнопки схлопнуты в один бит);
    //   бит 5 — «Пуск2».
    // Возврат — с инверсией (потенциал земли при нажатии).
    if (!connected_) {
        return 0xFF;  // джойстик не подключен — подтяжки линий
    }

    uint8_t port = 0;

    // Направления
    if (direction_up_)    port |= 0x01;  // Вверх
    if (direction_down_)  port |= 0x02;  // Вниз
    if (direction_left_)  port |= 0x04;  // Влево
    if (direction_right_) port |= 0x08;  // Вправо

    // Кнопки: «Пуск1» — бит 4, «Пуск2» — бит 5;
    // любая нажатая кнопка дополнительно отражается в бите 4.
    if (buttons_ & 0x01) port |= 0x10;
    if (buttons_ & 0x02) port |= 0x20;
    if (buttons_ & 0x02) port |= 0x10;  // зеркало для однобайтовой модели

    // Инверсия — активный низкий уровень
    return static_cast<uint8_t>(port ^ 0xFF);
}

uint8_t Joystick::read_ppi3_port_a() const {
    // Кнопки МД8030 через ЭУ8040 идут на контакты 18-19 вилки ХР3 =
    // Порт 0.4/0.5 (PPI3-A биты 4-5), активный низкий. Остальные биты
    // порта A не задействованы — подтяжки (1).
    if (!connected_) {
        return 0xFF;
    }
    uint8_t value = 0xFF;
    value = static_cast<uint8_t>(value & ~((buttons_ & 0x03) << 4));
    return value;
}

Joystick::JoystickStatus Joystick::get_status() const {
    JoystickStatus status{};
    status.connected = connected_;
    status.left_active = direction_left_;
    status.right_active = direction_right_;
    status.up_active = direction_up_;
    status.down_active = direction_down_;
    status.button_state = buttons_;
    return status;
}

}  // namespace korvet
