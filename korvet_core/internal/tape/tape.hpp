#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace korvet {

struct tape_block_header_t {
    uint8_t sync;
    uint8_t type;
    uint16_t length;
    // Явный учёт размера вместо flexible array member
};

class Tape {
public:
    static constexpr uint32_t SAMPLE_RATE = 22050;
    
    void init();
    void reset();
    
    bool set_data(const uint8_t* data, size_t size);
    void eject();
    
    bool is_inserted() const { return !tape_data_.empty(); }
    
    void set_motor(bool on);
    bool is_motor_on() const { return motor_on_; }
    
    uint8_t read();
    void write(uint8_t data);
    
    // Чтение/запись блоков
    bool read_block(uint8_t* buffer, uint16_t* size);
    bool write_block(const uint8_t* buffer, uint16_t size);
    
    // Управление позицией на ленте
    void rewind();
    void fast_forward();
    void seek(double position);  // Position in seconds
    
    // Состояние
    double get_position() const;  // Current position in seconds
    double get_total_time() const;  // Total tape time in seconds
    bool is_playing() const { return playing_; }
    bool is_recording() const { return recording_; }
    void play();
    void stop();
    void record();

private:
    bool motor_on_;
    bool read_mode_;
    std::vector<uint8_t> tape_data_;
    
    uint32_t sample_rate_;
    uint32_t bit_position_;
    uint8_t current_byte_;
    uint8_t current_bit_;
    
    // Состояние воспроизведения/записи
    bool playing_;
    bool recording_;
    size_t current_position_;  // in bytes (replaces the earlier declaration)
    uint32_t total_size_;        // in bytes
    
    // Внутренние методы
    void process_byte();
};

}  // namespace korvet