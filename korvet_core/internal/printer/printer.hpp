/**
 * @file printer.hpp
 * @brief Эмуляция принтера ПК8020 "Корвет"
 *
 * Содержит определение класса Printer, который эмулирует работу
 * принтера, подключенного к ПК8020 "Корвет".
 *
 * @author Original authors
 * @date 2024
 */

#ifndef PRINTER_HPP
#define PRINTER_HPP

#include <cstdint>
#include <functional>

namespace korvet {

// Один готовый (уже "напечатанный") байт от эмулируемого принтера.
// Хост сам решает, что с ним делать: копить в TXT, рендерить в PDF,
// слать на физический принтер и т.д. — korvet_core файлов не открывает.
using PrinterByteCallback = std::function<void(uint8_t byte)>;

/**
 * @brief Класс эмуляции принтера
 *
 * Принтер ПК8020 подключается к портам 30h-33h и реализует интерфейс
 * для печати данных из эмулируемой системы. Само устройство вывода
 * (файл/PDF/физический принтер) целиком на стороне хоста — см.
 * set_output_callback().
 */
class Printer {
public:
    static constexpr uint8_t LSTDAT_PORT = 0x30;  ///< Порт данных принтера
    static constexpr uint8_t LSTSTS_PORT = 0x38;  ///< Порт статуса принтера
    static constexpr uint8_t LSTCTL_PORT = 0x32;  ///< Порт управления принтера
    
    Printer();
    ~Printer();
    
    void init();
    void reset();
    
    /**
     * @brief Запись данных в принтер (через PPI2_A)
     * @param data Данные для печати
     */
    void write_data(uint8_t data);
    
    /**
     * @brief Запись команды в контроллер принтера (через PPI2_C)
     * @param cmd Команда для контроллера
     */
    void write_cmd(uint8_t cmd);
    
    /**
     * @brief Чтение статуса принтера
     * @return Статус принтера
     */
    uint8_t read_status();
    
    /**
     * @brief Подключить приёмник байтов принтера. Вызывается синхронно,
     * из того же потока/момента, что и запись байта на строб — хост
     * решает сам, буферизовать это или писать сразу.
     */
    void set_output_callback(PrinterByteCallback cb) { output_cb_ = std::move(cb); }
    
    void set_enabled(bool enabled);
    bool is_enabled() const { return enabled_; }
    
    uint32_t get_printed_bytes_count() const { return printed_bytes_count_; }
    void reset_printed_bytes_count() { printed_bytes_count_ = 0; }
    
    void set_status_delay(int delay) { status_delay_ = delay; }
    int get_status_delay() const { return status_delay_; }
    
    /**
     * @brief Обработка строба принтера (имитация печати при активации строба)
     * @param strobe_state Состояние сигнала строба
     * @return Предыдущее состояние строба
     */
    bool process_strobe(bool strobe_state);

private:
    PrinterByteCallback output_cb_;  ///< Хост решает, куда девать байты
    bool enabled_;               ///< Состояние принтера
    bool ack_state_;             ///< Состояние сигнала ACK
    bool busy_state_;            ///< Состояние занятости
    bool paper_out_;             ///< Состояние бумаги
    bool selected_;              ///< Состояние выбора устройства
    bool error_state_;           ///< Состояние ошибки
    uint8_t last_data_;          ///< Последние данные
    uint8_t control_byte_;       ///< Байт управления
    int status_delay_;           ///< Задержка статуса
    int status_delay_counter_;   ///< Счетчик задержки статуса
    bool prev_strobe_state_;     ///< Предыдущее состояние строба
    uint32_t printed_bytes_count_; ///< Количество напечатанных байтов
    
    /**
     * @brief Отправить готовый байт наружу через output_cb_
     * @param byte Байт для отправки (до инверсии PPI2_A^0xFF)
     */
    void send_to_output(uint8_t byte);
    
    /**
     * @brief Обновить состояние принтера
     */
    void update_status();
};

}  // namespace korvet

#endif // PRINTER_HPP
