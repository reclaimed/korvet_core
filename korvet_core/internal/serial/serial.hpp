/**
 * @file serial.hpp
 * @brief Эмуляция последовательного порта ИРПС (КР580ВВ51А) ПК8020 "Корвет"
 *
 * Содержит определение класса Serial, который эмулирует работу
 * последовательного порта ИРПС ПК8020 "Корвет": приёмная очередь байтов,
 * регистр состояния и прерывание приёмника SERINT1 (IRQ1).
 *
 * Ядро эмулирует только регистры и протокол УСАПП. Куда физически
 * уходят переданные байты (RS485, TCP, /dev/ttyUSB*) решает хост через
 * set_byte_out_callback().
 */

#pragma once

#include <cstdint>
#include <array>
#include <functional>

namespace korvet {

class Pic;

// Один байт, который Корвет передал по ИРПС наружу. Хост сам решает,
// куда его девать: RS485, TCP-сокет, /dev/ttyUSB* — korvet_core сокетов
// и портов не открывает.
using SerialByteOutCallback = std::function<void(uint8_t byte)>;

class Serial {
public:
    static constexpr uint16_t RS232_BUFFER_SIZE = 256;

    // Регистры ИРПС (описание элементов):
    //   SERD = 10H — регистр данных;
    //   SERC = 11H — регистр управления (запись);
    //   SERS = 11H — регистр состояния (чтение).
    static constexpr uint8_t SERD_PORT = 0x10;
    static constexpr uint8_t SERC_PORT = 0x11;

    // Разрядка статуса SERS (описание элементов):
    static constexpr uint8_t STS_TRDY  = 0x01;  ///< готовность передатчика
    static constexpr uint8_t STS_RRDY  = 0x02;  ///< готовность приёмника
    static constexpr uint8_t STS_TXBE  = 0x04;  ///< передатчик пуст
    static constexpr uint8_t STS_RPAR  = 0x08;  ///< ошибка чётности
    static constexpr uint8_t STS_ROV   = 0x10;  ///< переполнение приёмника
    static constexpr uint8_t STS_RFR   = 0x20;  ///< ошибка кадра
    static constexpr uint8_t STS_DSR   = 0x80;  ///< модем готов

    // Биты слова команды:
    static constexpr uint8_t CMD_TXEN  = 0x01;  ///< разрешение передатчика
    static constexpr uint8_t CMD_RXEN  = 0x04;  ///< разрешение приёмника
    static constexpr uint8_t CMD_SBCH  = 0x08;  ///< разрыв линии
    static constexpr uint8_t CMD_CLERR = 0x10;  ///< сброс признаков ошибок
    static constexpr uint8_t CMD_USRST = 0x40;  ///< внутренний сброс

    void init();
    void reset();

    // Интерфейс RS-232
    void write_data(uint8_t data);
    uint8_t read_data();
    bool has_data() const { return rs232_queue_len_ > 0; }

    // Портовый интерфейс (используется маппером памяти):
    //   запись 0x10 — данные, 0x11 — слово режима/команды;
    //   чтение 0x10 — данные, 0x11 — статус.
    void write(uint8_t port, uint8_t data);
    uint8_t read(uint8_t port);

    // Байт, полученный ОТ хоста (с "провода"), кладём в приёмный буфер
    // Корвета — эмулируемая программа увидит его через порт данных.
    void process_rs232_input(uint8_t data);

    // Очередь для мыши и других устройств
    void set_pic(Pic* pic) { pic_ = pic; }
    void add_to_queue(uint8_t byte);

    // Подключить приёмник байтов, которые Корвет ПЕРЕДАЁТ наружу
    // (передача синхронная, мгновенная).
    void set_byte_out_callback(SerialByteOutCallback cb) { byte_out_cb_ = std::move(cb); }

    // Флаги состояния
    bool tx_ready() const { return true; }
    bool rx_ready() const { return rs232_queue_len_ > 0; }

    // Регистр управления УСАПП (0xFB11, запись). Двухфазный автомат
    // КР580ВВ51А (M-3): после сброса первая запись — СЛОВО РЕЖИМА
    // (скорость/длина/чётность/стоп-биты; у ИРПС SERMD=0x4E), последующие —
    // СЛОВО КОМАНДЫ (TXEN/RXE/SBCH/CLERR/USRST). Слово режима
    // интерпретации команды не подвергается — иначе бит 6 (стоп-биты)
    // стирал бы приёмную очередь при каждой инициализации порта.
    void write_control(uint8_t data);
    uint8_t control_word() const { return control_; }

    /// true, если УСАПП ждёт слово режима (после сброса/USRST).
    bool awaiting_mode_word() const { return awaiting_mode_word_; }

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

    // Модель УСАПП (КР580ВВ51А/8251A):
    //   awaiting_mode_word_ — фаза автомата «ждём слово режима»;
    //   mode_word_ — сохранённое слово режима (SERMD и т.п.);
    //   control_ — слово команды (TXEN/RXE/ER/USRST);
    //   overrun_latch_ — липкий флаг переполнения OE (сброс командой CLERR).
    bool awaiting_mode_word_ = true;
    uint8_t mode_word_ = 0;
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
};

}  // namespace korvet
