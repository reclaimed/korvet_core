#include "tape.hpp"
#include "../logger/logger.hpp"

#include <cstring>

namespace korvet {

void Tape::init() {
    reset();
}

void Tape::reset() {
    motor_on_ = false;
    read_mode_ = true;
    tape_data_.clear();
    
    sample_rate_ = SAMPLE_RATE;
    bit_position_ = 0;
    current_byte_ = 0;
    current_bit_ = 0;
    
    playing_ = false;
    recording_ = false;
    current_position_ = 0;
    total_size_ = 0;
}

bool Tape::set_data(const uint8_t* data, size_t size) {
    if (!data || size == 0) return false;
    tape_data_.assign(data, data + size);
    total_size_ = tape_data_.size();
    current_position_ = 0;
    return true;
}

void Tape::eject() {
    tape_data_.clear();
    current_position_ = 0;
    total_size_ = 0;
}

void Tape::set_motor(bool on) {
    motor_on_ = on;
    if (on) {
        // Мотор включили — начинаем воспроизведение, если данные загружены
        if (!tape_data_.empty()) {
            play();
        }
    } else {
        // Мотор выключили — останавливаем воспроизведение
        stop();
    }
}

uint8_t Tape::read() {
    if (tape_data_.empty() || !playing_) {
        return 0;  // Default value when not playing
    }
    
    // Чтение байта с ленты
    if (current_position_ < total_size_) {
        uint8_t data = tape_data_[current_position_];
        current_position_++;
        return data;
    }
    
    return 0;  // End of tape reached
}

void Tape::write(uint8_t data) {
    if (!recording_) {
        return;  // Cannot write if not recording
    }
    
    // Расширяем данные ленты при необходимости
    if (current_position_ >= tape_data_.size()) {
        tape_data_.push_back(data);
    } else {
        tape_data_[current_position_] = data;
    }
    
    current_position_++;
    if (current_position_ > total_size_) {
        total_size_ = current_position_;
    }
}

bool Tape::read_block(uint8_t* buffer, uint16_t* size) {
    if (tape_data_.empty() || !playing_ || !buffer || !size) {
        return false;
    }
    
    // Чтение блока данных с ленты
    // Полная реализация читала бы по формату кассеты
    
    // Пока — фиксированный объём
    uint16_t bytes_to_read = *size;
    if (current_position_ + bytes_to_read > total_size_) {
        bytes_to_read = static_cast<uint16_t>(total_size_ - current_position_);
    }
    
    // Копируем данные из буфера
    for (uint16_t i = 0; i < bytes_to_read; i++) {
        buffer[i] = tape_data_[current_position_ + i];
    }
    
    current_position_ += bytes_to_read;
    *size = bytes_to_read;
    
    return bytes_to_read > 0;
}

bool Tape::write_block(const uint8_t* buffer, uint16_t size) {
    if (!recording_ || !buffer) {
        return false;
    }
    
    // Расширяем данные ленты при необходимости
    if (current_position_ + size > tape_data_.size()) {
        tape_data_.resize(current_position_ + size);
    }
    
    // Копируем данные в буфер
    for (uint16_t i = 0; i < size; i++) {
        tape_data_[current_position_ + i] = buffer[i];
    }
    
    current_position_ += size;
    if (current_position_ > total_size_) {
        total_size_ = current_position_;
    }
    
    return true;
}

void Tape::rewind() {
    current_position_ = 0;
}

void Tape::fast_forward() {
    current_position_ = total_size_;
}

void Tape::seek(double position) {
    if (total_size_ > 0) {
        uint32_t byte_pos = static_cast<uint32_t>(position * sample_rate_ / 1000); // Rough conversion
        if (byte_pos > total_size_) {
            byte_pos = total_size_;
        }
        current_position_ = byte_pos;
    }
}

double Tape::get_position() const {
    if (total_size_ > 0) {
        return static_cast<double>(current_position_) / sample_rate_ * 1000; // Convert to milliseconds
    }
    return 0.0;
}

double Tape::get_total_time() const {
    if (total_size_ > 0) {
        return static_cast<double>(total_size_) / sample_rate_ * 1000; // Convert to milliseconds
    }
    return 0.0;
}

void Tape::play() {
    playing_ = true;
    recording_ = false;
}

void Tape::stop() {
    playing_ = false;
    recording_ = false;
}

void Tape::record() {
    recording_ = true;
    playing_ = false;
}

}  // namespace korvet