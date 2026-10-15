/**
 * @file lan.cpp
 * @brief Реализация эмуляции локальной сети (LAN) ПК8020 "Корвет"
 *
 * korvet_core эмулирует только регистры порта LAN. Куда физически уходят
 * переданные байты (RS485 между несколькими Корветами, TCP/IP-туннель
 * и т.д.) решает хост через set_byte_out_callback() — см. lan.hpp.
 */

#include "lan.hpp"

namespace korvet {

void Lan::init() {
    reset();
}

void Lan::reset() {
    // Инициализация LAN
    address_ = 0x01;  // Default address
    status_ = 0;
    status_internal_ = 0;
    
    rx_pos_ = 0;
    tx_pos_ = 0;
    rx_len_ = 0;
    tx_len_ = 0;
    
    for (auto& byte : rx_buffer_) {
        byte = 0;
    }
    
    for (auto& byte : tx_buffer_) {
        byte = 0;
    }
}

void Lan::write(uint8_t data) {
    // Store in transmit buffer (для мониторинга/статуса — см. get_lan_status())
    if (tx_pos_ < LAN_BUFFER_SIZE) {
        tx_buffer_[tx_pos_] = data;
        tx_pos_++;
        tx_len_++;
    }
    
    // Обновляем статус: передатчик занят
    status_internal_ &= ~0x80;  // Clear TXRDY

    // ФИКС: раньше send_packet(data) был пустым телом — байт молча
    // терялся, ни в какую "сеть" не уходил. Теперь он реально уходит
    // наружу через колбэк хоста.
    lan_tx_count_++;
    if (byte_out_cb_) {
        byte_out_cb_(data);
    }
}

uint8_t Lan::read() {
    if (rx_len_ > 0) {
        uint8_t data = rx_buffer_[rx_pos_];
        rx_pos_++;
        rx_len_--;
        
        if (rx_len_ == 0) {
            // Сброс позиции при пустом буфере
            rx_pos_ = 0;
        }
        
        // Обновляем статус, если буфер опустел
        if (rx_len_ == 0) {
            status_internal_ &= ~0x40;  // Clear RXRDY
        }
        
        return data;
    }
    
    return 0;  // No data available
}

uint8_t Lan::get_status() {
    // Регистр статуса LAN
    // По оригинальной реализации возвращает 0x4
    // (serial.c: return 0x4;)
    return 0x04;
}

void Lan::process_input(uint8_t data) {
    // Добавляем принятый байт LAN в буфер, если есть место
    if (rx_len_ < LAN_BUFFER_SIZE) {
        rx_buffer_[rx_pos_ + rx_len_] = data;
        rx_len_++;
        lan_rx_count_++;
        
        // Ставим флаг RXRDY
        status_internal_ |= 0x40;
    } else {
        lan_rx_errors_++;  // buffer overrun
    }
}

Lan::LanStatus Lan::get_lan_status() const {
    LanStatus status{};
    status.idle = (tx_len_ == 0 && rx_len_ == 0);
    status.rx_active = (rx_len_ > 0);
    status.tx_active = (tx_len_ > 0);
    status.rx_count = lan_rx_count_;
    status.tx_count = lan_tx_count_;
    status.rx_errors = lan_rx_errors_;
    status.tx_errors = lan_tx_errors_;
    return status;
}

}  // namespace korvet
