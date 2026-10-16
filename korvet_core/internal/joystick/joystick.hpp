#pragma once

#include <cstdint>

namespace korvet {

/// Джойстик МД8030 — пассивное устройство порта расширения (через ЭУ8040,
/// слот ХР2): четыре направления («Вверх/Вниз/Влево/Вправо») и две кнопки
/// «Пуск1»/«Пуск2», замыкающие линии на корпус (активный низкий уровень).
/// Направления попадают в PPI3-B биты 0-3, кнопки — в PPI3-A биты 4-5
/// (разводка ЭУ8040, приложение 2 паспорта).
class Joystick {
public:
    static constexpr uint8_t MAX_BUTTONS = 2;  ///< «Пуск1»/«Пуск2»

    void init();
    void reset();

    // === Цифровой джойстик — дискретные направления ===
    void set_direction_left(bool active);
    void set_direction_right(bool active);
    void set_direction_up(bool active);
    void set_direction_down(bool active);
    void set_button(uint8_t button, bool pressed);
    void set_connected(bool connected) { connected_ = connected; }

    // Геттеры цифрового состояния
    bool is_direction_left() const { return direction_left_; }
    bool is_direction_right() const { return direction_right_; }
    bool is_direction_up() const { return direction_up_; }
    bool is_direction_down() const { return direction_down_; }
    uint8_t get_buttons() const { return buttons_; }
    bool is_connected() const { return connected_; }

    /// Чтение порта B PPI3: направления в битах 0-3 (активный низкий),
    /// кнопки — бит 4 («Пуск1», зеркало любой кнопки — совместимость
    /// с ПО, отлаженным на однобайтовой модели) и бит 5 («Пуск2»).
    /// Не подключён → 0xFF.
    uint8_t read_ppi3_port_b() const;

    /// Чтение порта A PPI3: кнопки «Пуск1»/«Пуск2» в битах 4-5
    /// (активный низкий; разводка ХР3:18-19 = Порт 0.4/0.5).
    /// Не подключён → 0xFF (подтяжки входов).
    uint8_t read_ppi3_port_a() const;

private:
    bool connected_ = false;
    uint8_t buttons_ = 0;         ///< бит 0 — «Пуск1», бит 1 — «Пуск2»
    bool direction_left_ = false;
    bool direction_right_ = false;
    bool direction_up_ = false;
    bool direction_down_ = false;

public:
    // Интерфейс опроса для мониторинга
    struct JoystickStatus {
        bool connected;              // Is joystick connected
        bool left_active;            // Is left direction active
        bool right_active;           // Is right direction active
        bool up_active;              // Is up direction active
        bool down_active;            // Is down direction active
        uint8_t button_state;        // Button state bitmask
    };

    JoystickStatus get_status() const;
};

}  // namespace korvet
