/**
 * @file lan.hpp
 * @brief Эмуляция локальной сети (LAN) ПК8020 "Корвет"
 * 
 * Содержит определение класса Lan, который эмулирует работу
 * локальной сети (LAN) ПК8020 "Корвет", включая протоколы
 * и интерфейсы для связи между несколькими машинами.
 * 
 * @author Korvet Team
 * @date 2024
 */

#pragma once

#include <cstdint>
#include <array>
#include <functional>

namespace korvet {

// Один байт LAN-порта Корвета, переданный наружу. На уровне железа это
// обычный побайтовый порт (как RS232) — упаковка в кадры RS485 или пакеты
// TCP/IP делается хостом поверх этого потока байт, а не здесь.
using LanByteOutCallback = std::function<void(uint8_t byte)>;

class Lan {
public:
    static constexpr uint16_t LAN_BUFFER_SIZE = 256;
    
    void init();
    void reset();
    
    // Интерфейс LAN
    void write_data(uint8_t data);
    uint8_t read_data();
    uint8_t get_status();
    void set_address(uint8_t addr) { address_ = addr; }
    uint8_t get_address() const { return address_; }
    
    // Портовый интерфейс (используется маппером памяти)
    void write(uint8_t data);
    uint8_t read();

    // Байт, полученный ОТ хоста (пришедший по RS485/TCP), кладём в
    // приёмный буфер — эмулируемая программа увидит его через порт данных.
    void process_input(uint8_t data);

    // Подключить приёмник байтов, которые Корвет передаёт через LAN-порт.
    void set_byte_out_callback(LanByteOutCallback cb) { byte_out_cb_ = std::move(cb); }
    
    // Флаги состояния
    bool tx_ready() const { return true; }  // Placeholder
    bool rx_ready() const { return rx_len_ > 0; }
    
    // Интерфейс опроса для мониторинга
    struct LanStatus {
        bool idle;              // Idle state
        bool rx_active;         // Receiving data
        bool tx_active;         // Transmitting data
        uint32_t rx_count;      // Received bytes counter
        uint32_t tx_count;      // Transmitted bytes counter
        uint32_t rx_errors;     // Receive errors counter
        uint32_t tx_errors;     // Transmit errors counter
    };
    
    LanStatus get_lan_status() const;

private:
    uint8_t address_;
    uint8_t status_;
    std::array<uint8_t, LAN_BUFFER_SIZE> rx_buffer_;
    std::array<uint8_t, LAN_BUFFER_SIZE> tx_buffer_;
    uint16_t rx_pos_;
    uint16_t tx_pos_;
    uint16_t rx_len_;
    uint16_t tx_len_;

    LanByteOutCallback byte_out_cb_;
    
    // Флаги состояния
    uint8_t status_internal_;
    
    // Счётчики для мониторинга
    uint32_t lan_rx_count_ = 0;
    uint32_t lan_tx_count_ = 0;
    uint32_t lan_rx_errors_ = 0;
    uint32_t lan_tx_errors_ = 0;
};

}  // namespace korvet