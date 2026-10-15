#pragma once

#include <cstdint>

namespace korvet {

class Joystick {
public:
    static constexpr uint8_t MAX_AXES = 2;
    static constexpr uint8_t MAX_BUTTONS = 4;  // Support up to 4 buttons
    
    enum class JoystickType {
        DIGITAL_DEFLECTOR,  // Deflector Digital Joystick
        ANALOG_KORVET,      // Korvet analog joystick
        GAMEPAD             // Standard gamepad
    };
    
    void init();
    void reset();
    
    // Цифровой джойстик — дискретные направления
    void set_direction_left(bool active);
    void set_direction_right(bool active);
    void set_direction_up(bool active);
    void set_direction_down(bool active);
    void set_button(uint8_t button, bool pressed);
    void set_connected(bool connected) { connected_ = connected; }
    void set_type(JoystickType type) { type_ = type; }
    
    // Геттеры цифрового состояния
    bool is_direction_left() const { return direction_left_; }
    bool is_direction_right() const { return direction_right_; }
    bool is_direction_up() const { return direction_up_; }
    bool is_direction_down() const { return direction_down_; }
    uint8_t get_buttons() const { return buttons_; }
    
    uint8_t read();
    
    bool is_connected() const { return connected_; }
    
    // Совместимость с PPI3
    uint8_t read_for_ppi3() const;
    
    // Чтение порта B PPI3 (как в оригинальной реализации)
    uint8_t read_ppi3_port_b() const;

private:
    bool connected_;
    uint8_t buttons_;
    bool direction_left_;
    bool direction_right_;
    bool direction_up_;
    bool direction_down_;
    JoystickType type_;
    
    // Внутреннее состояние
    uint8_t current_axis_;
    bool axis_active_;
    
    // Преобразование формата джойстика
    uint8_t format_for_deflector() const;
    uint8_t format_for_analog() const;
    
public:
    // Интерфейс опроса для мониторинга
    struct JoystickStatus {
        bool connected;              // Is joystick connected
        bool left_active;            // Is left direction active
        bool right_active;           // Is right direction active
        bool up_active;              // Is up direction active
        bool down_active;            // Is down direction active
        uint8_t button_state;        // Button state bitmask
        uint8_t type;                // Joystick type
    };
    
    JoystickStatus get_status() const;
};

}  // namespace korvet