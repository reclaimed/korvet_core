/**
 * @file lan.cpp
 * @brief Реализация эмуляции адаптера локальной сети ПК8020 "Корвет"
 *
 * Модель адаптера ЛВС (КР580ВВ51А, D11) симметрична модели ИРПС
 * (internal/serial): приёмная очередь + статус-регистр + двухфазный
 * автомат «слово режима / слово команды» + прерывание приёма (IRQ3).
 * Отличия от ИРПС: адреса портов (0x20/0x21), фиксированная скорость
 * ~19500 бит/с (канал таймера не нужен), отсутствие DSR-механики
 * (общая линия) и LANADR в PPI1-A.
 *
 * Ядро эмулирует только байтовый уровень. Куда физически уходят байты
 * (RS485 между несколькими Корветами, TCP/IP-туннель и т.д.) решает
 * хост через set_byte_out_callback().
 */

#include "lan.hpp"
#include "../pic/pic.hpp"

namespace korvet {

void Lan::init() {
    reset();
}

void Lan::reset() {
    // Приёмная очередь пуста; автомат УСАПП ждёт слово режима
    rx_head_ = 0;
    rx_tail_ = 0;
    rx_len_ = 0;

    for (auto& byte : rx_buffer_) {
        byte = 0;
    }

    mode_word_ = 0;
    command_ = 0;
    overrun_latch_ = false;
    parity_error_latch_ = false;
    awaiting_mode_word_ = true;

    // Адрес РМУ НЕ сбрасывается: он задан перемычками платы (конфиг
    // хоста), а не состоянием УСАПП.
}

void Lan::write(uint8_t port, uint8_t data) {
    // Декодирование бита A0 (L-1): 0x20 — регистр данных,
    // 0x21 — регистр управления (слово режима/команды).
    if ((port & 0x01) == 0) {
        write_data(data);
    } else {
        // Управление УСАПП — наружу (в линию) НЕ уходит: иначе слово
        // режима LANC (0x5E) и команды попадали бы в поток данных.
        if (awaiting_mode_word_) {
            mode_word_ = data;  // слово режима: LANMD = R16X|CL8|PEHB|ST1
            awaiting_mode_word_ = false;
            return;
        }
        command_ = data;
        if (data & CMD_CLERR) {
            overrun_latch_ = false;
            parity_error_latch_ = false;
        }
        if (data & CMD_USRST) {
            awaiting_mode_word_ = true;
            command_ = 0;
            overrun_latch_ = false;
            parity_error_latch_ = false;
        }
    }
}

uint8_t Lan::read(uint8_t port) {
    if ((port & 0x01) == 0) {
        return read_data();
    }
    return get_status();
}

void Lan::write_data(uint8_t data) {
    // Регистр LAND: байт уходит в линию через колбэк хоста. До
    // инициализации УСАПП (ни слова режима, ни команды) канал
    // прозрачный — байты не теряются (совместимость с ПО, не
    // дожидающимся полной инициализации). После инициализации
    // передача требует TXEN.
    const bool uninitialized = awaiting_mode_word_ && command_ == 0;
    const bool tx_allowed = uninitialized || (command_ & CMD_TXEN);

    lan_tx_count_++;
    if (byte_out_cb_ && tx_allowed) {
        byte_out_cb_(data);
    }
}

uint8_t Lan::read_data() {
    if (rx_len_ > 0) {
        uint8_t data = rx_buffer_[rx_head_];
        rx_head_ = (rx_head_ + 1) % LAN_BUFFER_SIZE;
        rx_len_--;

        // Чтение данных гасит RRDY автоматически (очередь пустеет) —
        // стандартная пара «статус/данные».
        return data;
    }
    return 0;  // данных нет
}

uint8_t Lan::get_status() {
    // Разрядка статуса LANS по описанию элементов:
    //   TRDY=01H — передатчик готов (передача мгновенная);
    //   RRDY=02H — в приёмном буфере есть байт;
    //   TXBE=04H — передатчик пуст;
    //   RPAR=08H — ошибка чётности (см. set_parity_check);
    //   ROV=10H — переполнение приёмника (липкий, сброс CLERR);
    //   RFR=20H — ошибка кадра (побитовой передачи нет — 0).
    // DSR (бит 7) в разрядке ЛВС отсутствует — общая линия без модема.
    const bool uninitialized = awaiting_mode_word_ && command_ == 0;
    const bool rx_enabled = uninitialized || (command_ & CMD_RXEN);

    uint8_t status = STS_TRDY | STS_TXBE;  // передатчик всегда готов/пуст
    if (rx_len_ > 0 && rx_enabled) {
        status |= STS_RRDY;
    }
    if (parity_error_latch_) status |= STS_RPAR;
    if (overrun_latch_) status |= STS_ROV;
    return status;
}

void Lan::process_input(uint8_t data) {
    // Приём байта из линии. До инициализации УСАПП байты принимаются
    // (канал прозрачный); после инициализации приём требует RXE —
    // выключенный приёмник байты теряет (как на реальной микросхеме).
    const bool uninitialized = awaiting_mode_word_ && command_ == 0;
    const bool rx_enabled = uninitialized || (command_ & CMD_RXEN);
    if (!rx_enabled) {
        lan_rx_errors_++;  // приёмник выключен — байт потерян
        return;
    }

    if (rx_len_ < LAN_BUFFER_SIZE) {
        rx_buffer_[rx_tail_] = data;
        rx_tail_ = (rx_tail_ + 1) % LAN_BUFFER_SIZE;
        rx_len_++;
        lan_rx_count_++;

        // LANINT (IRQ3): «адаптер способен генерировать запрос
        // прерывания при приёме байта данных из канала; используется
        // уровень прерывания 3» (описание элементов). Единственный
        // штатный способ узнать о байте — без IRQ3 ни одно сетевое ПО
        // не может принять данные.
        if (pic_) pic_->raise_irq(3);
    } else {
        // Переполнение приёмника — липкий ROV
        overrun_latch_ = true;
        lan_rx_errors_++;
    }
}

Lan::LanStatus Lan::get_lan_status() const {
    LanStatus status{};
    status.idle = (rx_len_ == 0);
    status.rx_active = (rx_len_ > 0);
    status.tx_active = false;
    status.rx_count = lan_rx_count_;
    status.tx_count = lan_tx_count_;
    status.rx_errors = lan_rx_errors_;
    status.tx_errors = lan_tx_errors_;
    return status;
}

}  // namespace korvet
