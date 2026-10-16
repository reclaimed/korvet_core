/**
 * @file serial.cpp
 * @brief Реализация эмуляции последовательного порта ИРПС ПК8020 "Корвет"
 *
 * korvet_core эмулирует только регистры и протокол УСАПП (КР580ВВ51А).
 * Куда физически уходят переданные байты (RS485, TCP, /dev/ttyUSB*) решает
 * хост через set_byte_out_callback() — см. serial.hpp.
 */

#include "serial.hpp"
#include "../pic/pic.hpp"
#include "../logger/logger.hpp"

namespace korvet {

void Serial::init() {
    reset();
}

void Serial::reset() {
    // Инициализация ИРПС
    rs232_queue_head_ = 0;
    rs232_queue_tail_ = 0;
    rs232_queue_len_ = 0;

    for (auto& byte : rs232_queue_) {
        byte = 0;
    }

    control_ = 0;
    mode_word_ = 0;
    overrun_latch_ = false;
    // После аппаратного сброса УСАПП ждёт СЛОВО РЕЖИМА (двухфазный
    // автомат 8251A — см. write_control).
    awaiting_mode_word_ = true;
}

void Serial::write_data(uint8_t data) {
    rs232_write(data);
}

uint8_t Serial::read_data() {
    return rs232_read();
}

void Serial::rs232_write(uint8_t data) {
    // Байты, переданные программой на Корвете через ИРПС, уходят наружу
    // через колбэк хоста (передача синхронная — TxRDY сразу готов).
    rs232_tx_count_++;
    if (byte_out_cb_) {
        byte_out_cb_(data);
    }

    // SERINT2 (IRQ2) — прерывание передатчика: «сигнал высокого уровня
    // на выходе RATF... поступает на второй уровень прерываний» (схема).
    // Передача мгновенная — передатчик готов сразу после выдачи байта:
    // поднимаем IRQ2 на каждый переданный байт (при разрешённом TXEN).
    if (pic_ && (control_ & CMD_TXEN)) {
        pic_->raise_irq(2);
    }
}

void Serial::write(uint8_t port, uint8_t data) {
    // Запись в порты ИРПС (0x10..0x17, реально используются 0x10/0x11):
    //   0x10 — ДАННЫЕ: байт уходит наружу (колбэк хоста);
    //   0x11 — УПРАВЛЕНИЕ (УСАПП): слово режима/команды, наружу не
    //          уходит — иначе инициализация порта гостем «выстреливала» бы
    //          мусором в поток данных.
    if ((port & 0xF0) == 0x10) {
        if ((port & 0x0F) == 0x00) {
            rs232_write(data);
        } else if ((port & 0x0F) == 0x01) {
            write_control(data);
        }
    }
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
    return 0xFF;  // Default return value
}

uint8_t Serial::rs232_read() {
    uint8_t data = 0;
    if (rs232_queue_len_ > 0) {
        data = rs232_queue_[rs232_queue_head_];
        rs232_queue_head_ = (rs232_queue_head_ + 1) % RS232_BUFFER_SIZE;
        rs232_queue_len_--;

        // M-1: IRQ1 (SERINT1) поднимается ТОЛЬКО при реальном извлечении
        // байта из очереди. Раньше прерывание поднималось при каждом
        // чтении порта данных — в том числе пустого: обработчик IRQ1
        // перезапускал сам себя бесконечно (шторм прерываний нулевыми
        // байтами) — главная причина «не работает мышь». Повторный запрос
        // после извлечения нужен, чтобы обработчик дошёл до конца
        // много байтовой посылки (3 байта мыши) за последовательными
        // подтверждениями прерывания.
        if (pic_) pic_->raise_irq(1);
    }
    return data;
}

uint8_t Serial::rs232_get_status() {
    // Разрядка статусного регистра УСАПП по описанию элементов:
    //   бит 0 (0x01) — TRDY: передатчик принял байт (готов к передаче);
    //   бит 1 (0x02) — RRDY: в приёмном буфере есть байт;
    //   бит 2 (0x04) — TXBE: передатчик пуст (всё выдано в линию);
    //   бит 3 (0x08) — RPAR: ошибка чётности (не моделируется — 0);
    //   бит 4 (0x10) — ROV: переполнение приёмника (липкий, сброс CLERR);
    //   бит 5 (0x20) — RFR: ошибка кадра (побитовой передачи нет — 0);
    //   бит 6 (0x40) — SynDET: синхронный режим (не используется — 0);
    //   бит 7 (0x80) — DSR: модем готов. Мышь Microsoft питается от
    //                  сигнальных линий (контакт 8 ↔ выводы 2+7) и
    //                  замыкает их на DSR — при подключённой мыши DSR=1.
    const bool tx_enabled = (control_ & CMD_TXEN) != 0;
    const bool rx_enabled = (control_ & CMD_RXEN) != 0;
    // До первой инициализации порт «пустой» — статусы готовности
    // отдаются как установленные (совместимость с ПО, не дожидающимся
    // инициализации).
    const bool uninitialized = awaiting_mode_word_ && control_ == 0;

    uint8_t status = 0;
    if (tx_enabled || uninitialized) {
        status |= STS_TRDY;   // TxRDY (передача синхронная — всегда готов)
    }
    if (rs232_queue_len_ > 0 && (rx_enabled || uninitialized)) {
        status |= STS_RRDY;   // RxRDY
    }
    status |= STS_TXBE;       // TxEMPTY: передача синхронная, всегда пусто
    if (overrun_latch_) status |= STS_ROV;   // OE
    status |= STS_DSR;        // DSR = 1 (модемный кабель мыши замкнут)

    return status;
}

void Serial::write_control(uint8_t data) {
    // Двухфазный автомат УСАПП (M-3). Реальная КР580ВВ51А после
    // аппаратного/программного сброса принимает СЛОВО РЕЖИМА, и только
    // последующие записи — СЛОВО КОМАНДЫ. Слово режима ИРПС
    // (SERMD = R16X|CL8|ST1 = 0x4E) содержит бит 6 (один стоповый бит) —
    // раньше он трактовался как внутренний сброс и СТИРАЛ приёмную
    // очередь: любая реинициализация порта гостем уничтожала накопленные
    // байты мыши. Теперь слово режима сохраняется, очередь не трогается.
    if (awaiting_mode_word_) {
        mode_word_ = data;              // слово режима: скорость/биты/чётность
        awaiting_mode_word_ = false;
        return;
    }

    // Слово команды:
    control_ = data;
    if (data & CMD_CLERR) {
        // CLERR — сброс признаков ошибок (RPAR/ROV/RFR)
        overrun_latch_ = false;
    }
    if (data & CMD_USRST) {
        // USRST — внутренний сброс: автомат возвращается к ожиданию
        // слова режима. Приёмная очередь НЕ очищается (модель
        // ориентирована на поток мыши: байты продолжают приходить).
        awaiting_mode_word_ = true;
        control_ = 0;
        overrun_latch_ = false;
    }
}

void Serial::process_rs232_input(uint8_t data) {
    // Добавляем принятый байт в очередь, если есть место
    if (rs232_queue_len_ < RS232_BUFFER_SIZE) {
        rs232_queue_[rs232_queue_tail_] = data;
        rs232_queue_tail_ = (rs232_queue_tail_ + 1) % RS232_BUFFER_SIZE;
        rs232_queue_len_++;
        rs232_rx_count_++;

        // SERINT1 (IRQ1): байт «с провода» поднимает прерывание приёмника
        // (при разрешённом приёме RXE) — как на реальной машине.
        if (pic_ && (control_ & CMD_RXEN)) {
            pic_->raise_irq(1);
        }
    } else {
        // Переполнение приёмника — липкий флаг OE в статусе
        // (сбрасывается гостевой командой CLERR, см. write_control).
        overrun_latch_ = true;
        rs232_rx_errors_++;
    }
}

void Serial::add_to_queue(uint8_t byte) {
    // Путь виртуальной мыши: байт входит в очередь как принятый с линии.
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
