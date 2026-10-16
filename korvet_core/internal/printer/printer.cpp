/**
 * @file printer.cpp
 * @brief Реализация эмуляции принтера ПК8020 "Корвет"
 *
 * Логика протокола (строб, статус, инверсия PPI2_A^0xFF) остаётся здесь —
 * это часть эмулируемого железа. Куда деваются готовые байты (файл, PDF,
 * физический принтер) решает хост через set_output_callback(). reset()
 * НЕ трогает output_cb_ — подключение к хосту не часть состояния
 * эмулируемого железа, которое обнуляется по сбросу/INIT.
 */

#include "printer.hpp"
#include "../pic/pic.hpp"
#include "../logger/logger.hpp"

namespace korvet {

Printer::Printer()
    : enabled_(false)
    , ack_state_(true)
    , busy_state_(false)
    , paper_out_(false)
    , selected_(true)
    , error_state_(false)
    , last_data_(0)
    , control_byte_(0)
    , status_delay_(0)
    , status_delay_counter_(0)
    , prev_strobe_state_(false)
    , printed_bytes_count_(0)
    , disabled_warned_(false)
{
}

Printer::~Printer() = default;

void Printer::init()
{
    reset();
}

void Printer::reset()
{
    enabled_ = false;
    ack_state_ = true;
    busy_state_ = false;
    paper_out_ = false;
    selected_ = true;
    error_state_ = false;
    last_data_ = 0;
    control_byte_ = 0;
    status_delay_ = 0;
    status_delay_counter_ = 0;
    prev_strobe_state_ = false;
    printed_bytes_count_ = 0;
    disabled_warned_ = false;
}

void Printer::write_data(uint8_t data)
{
    if (!enabled_) return;
    
    // Сохраняем данные для последующей печати по стробу
    last_data_ = data;
    // Не печатаем сразу — ждём сигнал строба
}

void Printer::write_cmd(uint8_t cmd)
{
    if (!enabled_) return;
    
    // Обработка команды принтера
    control_byte_ = cmd;
    
    // Бит 7: резерв
    // Бит 6: резерв
    // Бит 5: строб (переход 0->1 запускает печать)
    // Бит 4: инициализация принтера
    // Бит 3: резерв
    // Бит 2: резерв
    // Бит 1: резерв
    // Бит 0: резерв
    
    // Проверяем бит строба (бит 5) на переход 0 -> 1
    bool current_strobe = (cmd & 0x20) != 0;
    bool strobe_transition = current_strobe && !prev_strobe_state_;
    
    if (strobe_transition) {
        // Переход строба 0 -> 1 — печатаем сохранённый байт
        send_to_output(last_data_);
    }
    
    prev_strobe_state_ = current_strobe;
    
    if (cmd & 0x10) {  // Initialize printer
        // Поведение 1:1 со старым кодом: reset() сбрасывает enabled_ в
        // false, output_cb_ при этом не трогается (не входит в reset()).
        reset();
        selected_ = true;
    }
    
    update_status();
}

bool Printer::process_strobe(bool strobe_state)
{
    bool old_strobe = prev_strobe_state_;
    prev_strobe_state_ = strobe_state;
    
    // Проверяем переход строба 0 -> 1
    if (strobe_state && !old_strobe) {
        // Печать сохранённого байта при поднятии строба
        send_to_output(last_data_);
    }
    
    return old_strobe;
}

uint8_t Printer::read_status()
{
    // Байт статуса принтера (LSTRDY): =1 при готовности, =0 при занятости.
    // Задержка неготовности — 5 чтений статуса после печати (модель
    // занятости механики).
    
    if (status_delay_counter_ > 0) {
        status_delay_counter_--;
        if (status_delay_counter_ == 0) {
            // X-2: LSTINT (IRQ6) — конец занятости (BUSYP → низкий):
            // «интерфейс принтера — уровень 6 прерываний». ПО, печатающее
            // по прерыванию готовности, получает запрос здесь.
            if (pic_) {
                pic_->raise_irq(6);
            }
        }
        return 0; // Not ready during delay
    }
    
    // =1 — готов
    return 1;
}

void Printer::set_enabled(bool enabled)
{
    enabled_ = enabled;
}

void Printer::send_to_output(uint8_t byte)
{
    if (!enabled_) {
        // X-2: без printer_set_enabled() байты терялись бы МОЛЧА —
        // предупреждаем один раз, чтобы интегратор видел проблему.
        if (!disabled_warned_) {
            disabled_warned_ = true;
            LOG_WARN("Printer: байт потерян — принтер не включён (printer_set_enabled(true))");
        }
        return;
    }
    
    // Apply inverse mapping (original code does PPI2_A^0xff) — часть
    // эмулируемого железа, остаётся здесь независимо от того, куда
    // байт уйдёт дальше. LSTINV: «в драйверы... необходимо вводить
    // инвертирование передаваемого байта» (описание элементов).
    uint8_t processed_byte = byte ^ 0xFF;
    
    if (output_cb_) {
        output_cb_(processed_byte);
    }
    
    // Инкремент счётчика напечатанных байтов
    printed_bytes_count_++;
    
    // Задержка статуса (на 5 чтений порта)
    status_delay_counter_ = 5;
}

void Printer::update_status()
{
    // Обновляем внутренние флаги состояния
    ack_state_ = true;  // Always acknowledge for now
    // Состояние BUSY управляется счётчиком задержки статуса
    busy_state_ = (status_delay_counter_ > 0);
}

}  // namespace korvet
