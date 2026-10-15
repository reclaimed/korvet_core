/**
 * @file serial.cpp
 * @brief Реализация эмуляции последовательного порта RS232 ПК8020 "Корвет"
 *
 * korvet_core эмулирует только регистры и протокол UART-контроллера.
 * Куда физически уходят переданные байты (RS485, TCP, /dev/ttyUSB*) решает
 * хост через set_byte_out_callback() — см. serial.hpp.
 */

#include "serial.hpp"
#include "../pic/pic.hpp"

namespace korvet {

void Serial::init() {
    reset();
}

void Serial::reset() {
    // Инициализация RS-232
    rs232_queue_head_ = 0;
    rs232_queue_tail_ = 0;
    rs232_queue_len_ = 0;

    for (auto& byte : rs232_queue_) {
        byte = 0;
    }

    rs232_status_ = 0;
    control_ = 0;
    overrun_latch_ = false;
}

void Serial::write_data(uint8_t data) {
    rs232_write(data);
}

uint8_t Serial::read_data() {
    return rs232_read();
}

void Serial::rs232_write(uint8_t data) {
    // ФИКС: раньше это был буквально пустой метод — байты, переданные
    // программой на Корвете через RS232, никуда не уходили и терялись
    // молча. Теперь они уходят наружу через колбэк хоста.
    rs232_tx_count_++;
    if (byte_out_cb_) {
        byte_out_cb_(data);
    }
}

void Serial::write(uint8_t port, uint8_t data) {
    // Запись в порты ИРПС (0x10..0x17, реально используются 0x10/0x11):
    //   0x10 — ДАННЫЕ: байт уходит наружу (колбэк хоста);
    //   0x11 — УПРАВЛЕНИЕ (УСАПП): инициализация/сброс ошибок, наружу не
    //          уходит — иначе инициализация порта гостем «выстреливала» бы
    //          мусором в поток данных (v1.2.1: раньше писалось ВСЁ).
    // v1.2.3: управление УСАПП теперь МОДЕЛИРУЕТСЯ (см. write_control) —
    // программы (SHCOMP BIOS, ABRIS) сбрасывают им флаг ошибок OE.
    if ((port & 0xF0) == 0x10) {
        if ((port & 0x0F) == 0x00) {
            rs232_write(data);
        } else if ((port & 0x0F) == 0x01) {
            write_control(data);
        }
    }
    // Порты LAN (0x20-0x21) обрабатывает класс Lan
}

uint8_t Serial::read(uint8_t port) {
    // Чтение последовательных портов
    if ((port & 0xF0) == 0x10) {
        // Порт RS232
        if ((port & 0x0F) == 0x00) {
            return rs232_read();
        } else {
            return rs232_get_status();
        }
    }
    // Порты LAN (0x20-0x21) обрабатывает класс Lan
    return 0xFF;  // Default return value
}

uint8_t Serial::rs232_read() {
    uint8_t data = 0;
    if (rs232_queue_len_ > 0) {
        data = rs232_queue_[rs232_queue_head_];
        rs232_queue_head_ = (rs232_queue_head_ + 1) % RS232_BUFFER_SIZE;
        rs232_queue_len_--;
    }
    // После чтения поднимаем IRQ1 (как эталонный PIC_IntRequest(1))
    if (pic_) pic_->raise_irq(1);
    return data;
}

uint8_t Serial::rs232_get_status() {
    // v1.2.3: РАЗМЕТКА СТАТУСА ПРИВЕДЕНА К РЕАЛЬНОЙ БИС КР580ВВ51А (8251A).
    //
    // Раньше биты были сдвинуты (TxRDY=бит7, RxRDY=бит6) — ПОЛОВИНА гостевого
    // софта не видела данных: SHCOMP BIOS опрашивает RxRDY как `ANI 02H`
    // (бит 1, см. cpm.rom @0x261f), ABRIS ждёт готовность передатчика как
    // `ANI 81H` (биты 7+0: DSR|TxRDY, @ABRIS.ABP) и проверяет ошибки OE/FE
    // как `ANI 30H`. В следствие — «мышь не работает в ABRIS» и мёртвый
    // ввод с ИРПС в программах, читающих порт опросом.
    //
    // Реальная разметка статусного регистра 8251A (порт 0xFB11, чтение):
    //   бит 0 (0x01) — TxRDY: передатчик принял байт (готов к передаче);
    //   бит 1 (0x02) — RxRDY: в приёмном буфере есть байт;
    //   бит 2 (0x04) — TxEMPTY: передатчик пуст (всё выдано в линию);
    //   бит 3 (0x08) — PE: ошибка чётности (у нас чётности нет — всегда 0);
    //   бит 4 (0x10) — OE: переполнение приёмника (липкий, сброс командой ER);
    //   бит 5 (0x20) — FE: ошибка кадра (эмуляция побитово не ходит — 0);
    //   бит 6 (0x40) — SynDET (синхронный режим, не используется — 0);
    //   бит 7 (0x80) — DSR: модем готов. «Мышь» питается от DTR/RTS и
    //                  замыкает их на DSR (см. hw_docs/MICROSOFT_MOUSE_
    //                  SERIAL.txt: выводы 8 ↔ 2+7) — при подключённой мыши
    //                  DSR читается как 1. Держим 1 всегда (устройство есть).
    const bool tx_enabled = (control_ & 0x01) != 0;   // TXEN
    const bool rx_enabled = (control_ & 0x04) != 0;   // RXE

    uint8_t status = 0;
    if (tx_enabled || control_ == 0) {
        status |= 0x01;   // TxRDY (до первой инициализации порт «пустой»)
    }
    if (rs232_queue_len_ > 0 && (rx_enabled || control_ == 0)) {
        status |= 0x02;   // RxRDY
    }
    status |= 0x04;       // TxEMPTY: передача синхронная, всегда пусто
    if (overrun_latch_) status |= 0x10;   // OE
    status |= 0x80;       // DSR = 1 (модемный кабель мыши замкнут)

    return status;
}

void Serial::write_control(uint8_t data) {
    // Управляющее слово УСАПП. Бит 4 (ER, 0x10) — «reset error flags»:
    // гость (ABRIS: `MVI A,35H; STA 0FB11H`, см. hw_docs) сбрасывает им
    // липкие флаги ошибок после переполнения.
    control_ = data;
    if (data & 0x10) {
        overrun_latch_ = false;
    }
    // Бит 6 (IR, 0x40) — внутренний сброс микросхемы: чистим очередь,
    // флаги ошибок и управление (реальный 8251A сбрасывает всё).
    if (data & 0x40) {
        rs232_queue_head_ = 0;
        rs232_queue_tail_ = 0;
        rs232_queue_len_ = 0;
        overrun_latch_ = false;
        control_ = 0;
    }
}

void Serial::process_rs232_input(uint8_t data) {
    // Добавляем принятый байт в очередь, если есть место
    if (rs232_queue_len_ < RS232_BUFFER_SIZE) {
        rs232_queue_[rs232_queue_tail_] = data;
        rs232_queue_tail_ = (rs232_queue_tail_ + 1) % RS232_BUFFER_SIZE;
        rs232_queue_len_++;
        rs232_rx_count_++;

        // Ставим флаг RXRDY
        rs232_status_ |= 0x40;
        // v1.2.3: байт «с провода» поднимает IRQ1 приёмника — как на реальной
        // машине (RxRDY УСАППа → ПИК). Без этого прерывание-управляемые
        // драйверы (в т.ч. мышь в ABRIS) никогда не узнавали о данных,
        // а инжекция через serial_inject() была видна только поллерам.
        if (pic_) pic_->raise_irq(1);
    } else {
        // v1.2.3: переполнение приёмника — липкий флаг OE в статусе
        // (сбрасывается гостевой командой ER, см. write_control).
        overrun_latch_ = true;
        rs232_rx_errors_++;  // buffer overrun
    }
}

void Serial::add_to_queue(uint8_t byte) {
    if (rs232_queue_len_ >= RS232_BUFFER_SIZE) {
        overrun_latch_ = true;
        rs232_rx_errors_++;  // drop if full
        return;
    }
    rs232_queue_[rs232_queue_tail_] = byte;
    rs232_queue_tail_ = (rs232_queue_tail_ + 1) % RS232_BUFFER_SIZE;
    rs232_queue_len_++;
    rs232_rx_count_++;
    if (pic_) pic_->raise_irq(1);  // IRQ1 on every byte added
}

Serial::Rs232Status Serial::get_rs232_status() const {
    // Ранее была объявлена в .hpp, но нигде не определена — молчаливая
    // "бомба замедленного действия": заработала бы линковка только в
    // момент, когда кто-то реально вызовет этот метод.
    Rs232Status s{};
    s.idle = (rs232_queue_len_ == 0);
    s.rx_active = (rs232_queue_len_ > 0);
    s.tx_active = false;  // передача синхронная (см. rs232_write), не буферизуется
    s.rx_count = rs232_rx_count_;
    s.tx_count = rs232_tx_count_;
    s.rx_errors = rs232_rx_errors_;
    s.tx_errors = rs232_tx_errors_;
    return s;
}

}  // namespace korvet
