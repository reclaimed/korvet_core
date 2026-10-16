/**
 * @file lan.hpp
 * @brief Эмуляция адаптера локальной сети ПК8020 "Корвет"
 *
 * Адаптер ЛВС — вторая микросхема КР580ВВ51А (D11, лист 12 схемы):
 * асинхронный побайтовый канал на общую линию класса. Регистры:
 *   LAND  = 20H — регистр данных (A0=0);
 *   LANC  = 21H — регистр управления (запись, A0=1);
 *   LANS  = 21H — регистр состояния (чтение, A0=1).
 * Прерывание приёма — LANINT, уровень 3 (IRQ3). Скорость фиксирована
 * (тактирование 312 кГц, ~19500 бит/с) — канал таймера не используется.
 *
 * Формат кадров/протокол опроса (КОРНЕТ, NET-CP/M) реализует ГОСТЕВОЕ
 * ПО — адаптер лишь «глупая» побайтовая труба. Транспорт между
 * экземплярами эмулятора (TCP/RS485) строит хост через
 * set_byte_out_callback()/process_input().
 */

#pragma once

#include <cstdint>
#include <array>
#include <functional>

namespace korvet {

class Pic;

// Один байт LAN-порта Корвета, переданный наружу. Семантика общей линии
// (party line, полудуплекс): каждый переданный байт доставляется ВСЕМ
// станциям сети — транспорт с многоадресной доставкой строит хост.
using LanByteOutCallback = std::function<void(uint8_t byte)>;

class Lan {
public:
    static constexpr uint16_t LAN_BUFFER_SIZE = 256;

    // Регистры адаптера ЛВС (описание элементов):
    static constexpr uint8_t LAND_PORT = 0x20;  ///< регистр данных (A0=0)
    static constexpr uint8_t LANC_PORT = 0x21;  ///< управление/статус (A0=1)

    // Разрядка статуса LANS (описание элементов):
    static constexpr uint8_t STS_TRDY  = 0x01;  ///< готовность передатчика
    static constexpr uint8_t STS_RRDY  = 0x02;  ///< готовность приёмника
    static constexpr uint8_t STS_TXBE  = 0x04;  ///< передатчик пуст
    static constexpr uint8_t STS_RPAR  = 0x08;  ///< ошибка чётности
    static constexpr uint8_t STS_ROV   = 0x10;  ///< переполнение приёмника
    static constexpr uint8_t STS_RFR   = 0x20;  ///< ошибка кадра

    // Биты слова команды (аналогично ИРПС):
    static constexpr uint8_t CMD_TXEN  = 0x01;  ///< разрешение передатчика
    static constexpr uint8_t CMD_RXEN  = 0x04;  ///< разрешение приёмника
    static constexpr uint8_t CMD_SBCH  = 0x08;  ///< разрыв линии
    static constexpr uint8_t CMD_CLERR = 0x10;  ///< сброс признаков ошибок
    static constexpr uint8_t CMD_USRST = 0x40;  ///< внутренний сброс

    void init();
    void reset();

    /// Портовый интерфейс (вызывается маппером памяти). Декодируется бит
    /// A0 адреса: 0x20 — данные, 0x21 — управление (запись) / статус
    /// (чтение). Раньше A0 не декодировался вовсе: слово режима LANC
    /// уходило в поток данных, а чтение статуса извлекало байт из
    /// приёмной очереди (L-1).
    void write(uint8_t port, uint8_t data);
    uint8_t read(uint8_t port);

    /// Регистр данных: байт уходит в линию (колбэк хоста).
    void write_data(uint8_t data);
    /// Регистр данных: извлечь байт из приёмной очереди (0, если пусто).
    uint8_t read_data();
    /// Регистр состояния LANS.
    uint8_t get_status();

    /// Адрес рабочего места (1-15; 0 — групповой). Задаётся перемычками,
    /// читается программно через LANADR (PPI1-A биты 4-7).
    void set_address(uint8_t addr) { address_ = addr & 0x0F; }
    uint8_t get_address() const { return address_; }

    /// Байт, пришедший по линии ЛВС (хост получает его от транспорта).
    /// При включённом приёме (RXE) поднимает LANINT (IRQ3) — единственный
    /// штатный способ узнать о байте (описание элементов).
    void process_input(uint8_t data);

    /// Подключить приёмник байтов, которые Корвет передаёт в линию.
    void set_byte_out_callback(LanByteOutCallback cb) { byte_out_cb_ = std::move(cb); }

    /// Подключить PIC (для LANINT/IRQ3).
    void set_pic(Pic* pic) { pic_ = pic; }

    /// Режим «с контролем байтов на чётность» (схема; LANMD=0x5E с
    /// разрешением контроля). Побайтовый транспорт между экземплярами
    /// эмулятора доставки не искажает, поэтому контроль чётности —
    /// осознанная опция: при false (по умолчанию) бит RPAR не
    /// выставляется. См. docs/OVERVIEW.md («ЛВС»).
    void set_parity_check(bool enable) { parity_check_ = enable; }
    bool is_parity_check() const { return parity_check_; }

    /// true, если УСАПП ждёт слово режима (после сброса/USRST).
    bool awaiting_mode_word() const { return awaiting_mode_word_; }

    // Флаги состояния
    bool tx_ready() const { return true; }
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
    // Приёмная очередь (кольцо)
    std::array<uint8_t, LAN_BUFFER_SIZE> rx_buffer_{};
    uint16_t rx_head_ = 0;
    uint16_t rx_tail_ = 0;
    uint16_t rx_len_ = 0;

    // Автомат УСАПП: слово режима → слово команды (как у ИРПС)
    bool awaiting_mode_word_ = true;
    uint8_t mode_word_ = 0;
    uint8_t command_ = 0;
    bool overrun_latch_ = false;
    bool parity_error_latch_ = false;

    uint8_t address_ = 1;  // адрес РМУ по умолчанию (1-15)

    Pic* pic_ = nullptr;
    bool parity_check_ = false;

    LanByteOutCallback byte_out_cb_;

    // Счётчики для мониторинга
    uint32_t lan_rx_count_ = 0;
    uint32_t lan_tx_count_ = 0;
    uint32_t lan_rx_errors_ = 0;
    uint32_t lan_tx_errors_ = 0;
};

}  // namespace korvet
