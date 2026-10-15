/**
 * @file serial.hpp
 * @brief Эмуляция последовательного порта RS232 ПК8020 "Корвет"
 * 
 * Содержит определение класса Serial, который эмулирует работу
 * последовательного порта RS232 ПК8020 "Корвет".
 * 
 * @author Korvet Team
 * @date 2024
 */

#pragma once

#include <cstdint>
#include <array>
#include <functional>

namespace korvet {

class Pic;

// Один байт, который Корвет передал по RS232 наружу. Хост сам решает,
// куда его девать: RS485, TCP-сокет, /dev/ttyUSB* — korvet_core сокетов
// и портов не открывает.
using SerialByteOutCallback = std::function<void(uint8_t byte)>;

class Serial {
public:
    static constexpr uint16_t RS232_BUFFER_SIZE = 256;

    void init();
    void reset();
    
    // Интерфейс RS-232
    void write_data(uint8_t data);
    uint8_t read_data();
    uint8_t get_status();
    bool has_data() const { return rs232_queue_len_ > 0; }
    
    // Портовый интерфейс (используется маппером памяти)
    void write(uint8_t port, uint8_t data);
    uint8_t read(uint8_t port);

    // Байт, полученный ОТ хоста (с "провода"), кладём в приёмный буфер
    // Корвета — эмулируемая программа увидит его через порт данных.
    void process_rs232_input(uint8_t data);
    
    // Очередь для мыши и других устройств
    void set_pic(Pic* pic) { pic_ = pic; }
    void add_to_queue(uint8_t byte);

    // Подключить приёмник байтов, которые Корвет ПЕРЕДАЁТ наружу
    // (rs232_write() внутри вызывает этот колбэк вместо TCP/физического
    // порта, которых здесь больше нет — см. заголовок класса).
    void set_byte_out_callback(SerialByteOutCallback cb) { byte_out_cb_ = std::move(cb); }
    
    // Флаги состояния
    bool tx_ready() const { return true; }  // Placeholder
    bool rx_ready() const { return rs232_queue_len_ > 0; }

    // v1.2.3: регистр управления УСАПП (0xFB11, запись) — гость им:
    //   1) сбрасывает флаги ошибок (команда с битом ER = 0x10);
    //   2) выставляет TXEN/RXE/DTR/RTS. Состояние TXEN/RXE учитывается
    //      в статусе (как на реальной БИС КР580ВВ51А / Intel 8251A).
    void write_control(uint8_t data);
    uint8_t control_word() const { return control_; }


    // Интерфейс опроса для мониторинга
    struct Rs232Status {
        bool idle;              // Idle state
        bool rx_active;         // Receiving data
        bool tx_active;         // Transmitting data
        uint32_t rx_count;      // Received bytes counter
        uint32_t tx_count;      // Transmitted bytes counter
        uint32_t rx_errors;     // Receive errors counter
        uint32_t tx_errors;     // Transmit errors counter
    };
    
    Rs232Status get_rs232_status() const;

private:
    // RS-232
    std::array<uint8_t, RS232_BUFFER_SIZE> rs232_queue_;
    uint16_t rs232_queue_head_;
    uint16_t rs232_queue_tail_;
    uint16_t rs232_queue_len_;
    
    // Флаги состояния
    uint8_t rs232_status_;

    // v1.2.3: модель УСАПП (8251A/КР580ВВ51А): управляющее слово + липкий
    // флаг переполнения (OE). OE ставится при потере байта (буфер полон),
    // сбрасывается командой с битом ER (0x10) — как на реальной микросхеме.
    uint8_t control_ = 0;
    bool overrun_latch_ = false;

    
    // PIC для генерации прерываний
    Pic* pic_ = nullptr;

    SerialByteOutCallback byte_out_cb_;
    
    // Счётчики для мониторинга
    uint32_t rs232_rx_count_ = 0;
    uint32_t rs232_tx_count_ = 0;
    uint32_t rs232_rx_errors_ = 0;
    uint32_t rs232_tx_errors_ = 0;
    
    // Внутренние методы
    void rs232_write(uint8_t data);
    uint8_t rs232_read();
    uint8_t rs232_get_status();
    bool rs232_tx_ready() const { return true; }
    bool rs232_rx_ready() const { return rs232_queue_len_ > 0; }
};

}  // namespace korvet