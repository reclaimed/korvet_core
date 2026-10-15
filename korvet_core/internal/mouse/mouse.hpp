#pragma once

#include <cstdint>

namespace korvet {

class Serial;

class Mouse {
public:
    enum class InterfaceMode {
        STANDARD,      // Standard mouse interface
        MOUSE_SYSTEM,  // Mouse Systems interface
        KORVET_MOUSE   // Korvet-specific mouse interface
    };

    void init();
    void reset();
    
    void move(int16_t dx, int16_t dy);
    void set_position(int16_t x, int16_t y);
    void set_buttons(uint8_t buttons);
    
    int16_t get_delta_x() const { return delta_x_; }
    int16_t get_delta_y() const { return delta_y_; }
    uint8_t get_buttons() const { return buttons_; }
    
    // Чтение из порта PPI (совместимость с PPI3)
    uint8_t read_from_port(uint8_t port);
    
    void accumulate_deltas(bool enable) { accumulate_ = enable; }
    void set_max_delta(int16_t max) { max_delta_ = max; }
    
    // Выбор режима интерфейса мыши
    void set_interface_mode(InterfaceMode mode) { interface_mode_ = mode; }
    InterfaceMode get_interface_mode() const { return interface_mode_; }
    
    // Совместимость с оригинальным интерфейсом
    void set_mouse_system(bool enabled) {
        mouse_system_enabled_ = enabled;
        interface_mode_ = enabled ? InterfaceMode::MOUSE_SYSTEM : InterfaceMode::STANDARD;
    }
    bool is_mouse_system_enabled() const { return mouse_system_enabled_; }
    
    // Состояние мыши для чтения порта B PPI3
    uint8_t get_ppi3_port_b_value() const;
    
    // Последовательный интерфейс для пакетов мыши
    void set_serial(Serial* serial) { serial_ = serial; }
    void check_mouse();

private:
    int16_t delta_x_;
    int16_t delta_y_;
    uint8_t buttons_;
    
    int16_t accumulated_x_;
    int16_t accumulated_y_;
    bool accumulate_;
    
    int16_t max_delta_;
    
    bool mouse_system_enabled_;
    InterfaceMode interface_mode_;
    
    // Для интерфейса Mouse Systems
    mutable int current_packet_byte_;
    
    // Последовательный интерфейс для пакетов мыши
    Serial* serial_ = nullptr;
    int16_t prev_delta_x_ = 0;
    int16_t prev_delta_y_ = 0;
    uint8_t prev_buttons_ = 0;
    
    static constexpr uint8_t MOUSE_PACKET_SIZE = 3;
    
    // Вспомогательные методы
    void clamp_delta(int16_t& delta);
    
    // Чтение по конкретному интерфейсу
    uint8_t read_standard() const;
    uint8_t read_mouse_system() const;
    uint8_t read_korvet_mouse() const;
};

}  // namespace korvet