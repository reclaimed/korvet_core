/**
 * @file motherboard.hpp
 * @brief Основной класс материнской платы эмулятора Korvet
 * 
 * Содержит определение класса Motherboard, который представляет собой
 * фасад для взаимодействия с различными подсистемами эмулятора ПК8020 "Корвет".
 * 
 * @author Original authors
 * @date 2024
 */

#pragma once

#include "motherboard_types.hpp"
#include "internal/cpu/cpu.hpp"
#include "internal/memory/memory.hpp"
#include "internal/video/video.hpp"
#include "internal/audio/audio.hpp"
#include "internal/timer/timer.hpp"
#include "internal/pic/pic.hpp"
#include "internal/ppi/ppi.hpp"
#include "internal/fdc/fdc.hpp"
#include "internal/serial/serial.hpp"
#include "internal/lan/lan.hpp"  // Добавлен новый класс LAN
#include "internal/tape/tape.hpp"
#include "internal/keyboard/keyboard.hpp"
#include "internal/mouse/mouse.hpp"
#include "internal/joystick/joystick.hpp"
#include "internal/printer/printer.hpp"
#include "internal/cpu/disasm.hpp"
#include "internal/logger/logger.hpp"

#include <cstddef>  // For size_t
#include <memory>   // For std::unique_ptr
#include <chrono>   // (зарезервировано; v1.1.1: <thread> удалён — ядро не знает о потоках)

namespace korvet {

/**
 * @brief Класс материнской платы эмулятора ПК8020 "Корвет"
 * 
 * Основной класс, обеспечивающий координацию всех подсистем эмулятора.
 * Реализует паттерн Singleton для обеспечения единственного экземпляра.
 * 
 * Эмулируемый компьютер ПК8020 "Корвет" - это 8-битный персональный компьютер,
 * выпускавшийся в СССР в 1984-1990 годах. Содержит процессор КР580ВМ80А (аналог Intel 8080),
 * 64 КБ ОЗУ, 8-24 КБ ПЗУ, графическую систему GZU/ACZU и другие периферийные устройства.
 */
class Motherboard {
public:
    /**
     * @brief Получить экземпляр класса (Singleton)
     * @return Ссылка на единственный экземпляр Motherboard
     */
    static Motherboard& get_instance() {
        static Motherboard instance;
        return instance;
    }
    
    // Запрет копирования
    Motherboard(const Motherboard&) = delete;
    Motherboard& operator=(const Motherboard&) = delete;
    
    /**
     * @brief Инициализация эмулятора
     * @param config Конфигурация эмулятора
     * @return true в случае успеха, false при ошибке
     */
    bool init(const mb_config_t* config);
    
    /**
     * @brief Завершение работы эмулятора
     */
    void shutdown();
    
    /**
     * @brief Включить питание эмулятора
     */
    void power_on();
    
    /**
     * @brief Выключить питание эмулятора
     */
    void power_off();
    
    /**
     * @brief Приостановить эмуляцию
     */
    void pause();
    
    /**
     * @brief Возобновить эмуляцию
     */
    void resume();
    
    /**
     * @brief Холодный сброс системы
     */
    void cold_reset();
    
    /**
     * @brief Горячий сброс системы
     */
    void warm_reset();
    
    /**
     * @brief Проверить состояние питания
     * @return true если эмулятор включен, false если выключен
     */
    bool is_powered() const { return powered_; }
    
    /**
     * @brief Проверить состояние паузы
     * @return true если эмуляция приостановлена, false если работает
     */
    bool is_paused() const { return paused_; }
    
    /**
     * @brief Выполнить один шаг эмуляции (одну инструкцию CPU)
     */
    void step();
    
    /**
     * @brief Установить/снять точку останова
     * @param addr Адрес точки останова
     * @param enabled true для включения, false для отключения
     */
    void set_breakpoint(uint16_t addr, bool enabled);
    
    /**
     * @brief Проверить наличие точки останова
     * @param addr Адрес для проверки
     * @return true если точка останова установлена по данному адресу
     */
    bool has_breakpoint(uint16_t addr) const;
    
    /**
     * @brief Проверить, было ли сработала точка останова при последнем execute()
     * @return true если точка останова сработала
     */
    bool was_breakpoint_hit() const { return cpu_.was_breakpoint_hit(); }
    
    /**
     * @brief Выполнить заданное количество циклов эмуляции
     * @param cycles Количество циклов для выполнения
     */
    void run(uint32_t cycles);
    
    /**
     * @brief Выполнить один кадр эмуляции (~50fps) с синхронизацией по реальному времени
     */
    void run_frame();

    // =================================================================
    //  Точное управление исполнением (все тайминги — в тиках CPU).
    //  Исполнение останавливается на границе инструкции, поэтому
    //  executed_ticks может превысить запрошенный бюджет не более чем
    //  на длину последней инструкции (4..17 тиков). Позиция внутри кадра
    //  сохраняется между вызовами: run_ticks(30000) + run_ticks(20000)
    //  эквивалентно run_ticks(50000) — граница кадра (VBLANK, IRQ4,
    //  звук) обрабатывается каждые TICKS_PER_FRAME фактических тиков.
    // =================================================================

    /// Выполнить не менее `ticks` тиков процессора (целыми инструкциями).
    mb_run_result_t run_ticks(uint64_t ticks);

    /// Выполнить ровно `count` инструкций (подтверждения прерываний
    /// считаются отдельным шагом и входят в счётчик).
    mb_run_result_t run_opcodes(uint64_t count);

    /// Выполнять, пока предикат не вернёт true (проверяется перед каждым
    /// шагом и до первого), либо пока не исчерпан timeout_ticks.
    /// timeout_ticks == 0 — без таймаута (до срабатывания условия).
    /// Предикат обязан быть чистым (без побочных эффектов).
    mb_run_result_t run_while(mb_exec_predicate_t pred, void* user_data,
                              uint64_t timeout_ticks);

    /// Полный счётчик тиков «прямо сейчас»: закрытые кадры + текущий
    /// открытый (возможно частичный) кадр. После power_on начинается с 0.
    uint64_t get_current_cycles() const;

    // =================================================================
    //  Трассировка исполнения. Колбэк вызывается из цикла исполнения
    //  перед КАЖДЫМ шагом CPU (инструкция или подтверждение прерывания).
    //  Ядро не хранит записи и ничего не знает о носителе — весь буфер/
    //  файл/UART — на стороне хоста. Вызов колбэка отключается передачей
    //  nullptr. При остановке на breakpoint последняя запись отражает
    //  инструкцию на точке останова (она НЕ исполнена).
    // =================================================================
    void set_trace_callback(mb_trace_callback_t callback, void* user_data);
    bool is_trace_enabled() const { return trace_callback_ != nullptr; }

    /// Программный запрос прерывания через PIC (IRQ 0..7) — как если бы
    /// устройство подняло линию IRQ. CPU примет его при IFF=1.
    void pic_trigger_irq(uint8_t irq);
    
    /**
     * @brief Готовый звук текущего кадра (~20мс, 16-бит моно PCM).
     *
     * Синхронно: заполняется заново в конце каждого run_frame(), полностью
     * перезаписывая предыдущий кадр. Никакой очереди — если внешний код
     * не забрал буфер до следующего run_frame(), этот кадр звука потерян.
     * Формат и модель 1:1 с эталонным MakeSound()/SOUNDBUF.
     */
    const int16_t* get_audio_frame_buffer() const;
    int get_audio_frame_buffer_size() const;

    void audio_set_mute(bool muted);
    void audio_set_volume(uint8_t volume_percent);
    void audio_set_enabled(bool enabled);
    
    /**
     * @brief Установить колбэк для видеокадров
     * @param callback Функция обратного вызова для получения видеокадра
     * @param user_data Пользовательские данные для передачи в колбэк
     */
    void set_video_callback(mb_video_frame_callback_t callback, void* user_data);
    
    /**
     * @brief Установить колбэк для прерываний
     * @param callback Функция обратного вызова для обработки прерываний
     * @param user_data Пользовательские данные для передачи в колбэк
     */
    void set_interrupt_callback(mb_interrupt_callback_t callback, void* user_data);
    
    /**
     * @brief Установить колбэк для логирования (regular: error/warning/info/debug)
     * @param callback Функция для обработки сообщений лога
     * @param user_data Пользовательские данные для передачи в колбэк
     */
    void set_log_callback(mb_log_callback_t callback, void* user_data);
    
    /**
     * @brief Имитация нажатия клавиши на клавиатуре
     * @param key_code Код клавиши
     */
    void keyboard_key_press(uint16_t key_code);
    
    /**
     * @brief Имитация отпускания клавиши на клавиатуре
     * @param key_code Код клавиши
     */
    void keyboard_key_release(uint16_t key_code);

    /**
     * @brief Какая раскладка сейчас активна в госте (read-only).
     *
     * Ядро не хранит и не выбирает раскладку — трансляция host-клавиш
     * целиком на стороне внешнего кода (см. Keyboard::key_press). Этот
     * метод лишь честно читает то же самое место, что и BIOS/CP-M сами
     * используют для индикации языка (байт в RAM + бит порта клавиатуры),
     * тем же способом, что и эталонный эмулятор.
     *
     * @return true — QWERTY/латиница активна, false — ЙЦУКЕН/кириллица.
     */
    bool get_keyboard_language_layout() const;
    
    /**
     * @brief Переместить мышь
     * @param dx Смещение по оси X
     * @param dy Смещение по оси Y
     */
    void mouse_move(int16_t dx, int16_t dy);
    
    /**
     * @brief Установить состояние кнопок мыши
     * @param buttons Битовая маска состояния кнопок
     */
    void mouse_set_buttons(uint8_t buttons);
    
    /**
     * @brief Установить значение оси джойстика
     * @param axis Номер оси
     * @param value Значение оси
     */
    void joystick_set_axis(uint8_t axis, uint8_t value);
    
    /**
     * @brief Установить состояние кнопки джойстика
     * @param button Номер кнопки
     * @param pressed true если кнопка нажата, false если отпущена
     */
    void joystick_set_button(uint8_t button, bool pressed);
    
    /**
     * @brief Загрузить данные в компоненты (ROM, mapper, шрифт)
     */
    void load_data(const uint8_t* rom, size_t rom_size,
                   const uint8_t* mapper, size_t mapper_size,
                   const uint8_t* font, size_t font_size);
    
    /**
     * @brief Подключить адаптер доступа к образам дискет.
     *
     * Motherboard/Fdc НЕ хранят и не загружают образы дисков сами — они
     * запрашивают конкретные секторы через IFddImageAdapter. "Вставка" и
     * "извлечение" дискеты — целиком на стороне хоста (gui_qt5 открывает/
     * закрывает файл образа; на RP2040 адаптер читает сектора прямо с
     * SD-карты). См. internal/fdc/fdc_image_adapter.hpp.
     */
    void fdd_set_image_adapter(IFddImageAdapter* adapter);

    /**
     * @brief Вставлен ли диск в дисковод (спрашивает подключённый адаптер).
     */
    bool fdd_is_disk_inserted(uint8_t drive) const;

    /**
     * @brief Пометить вставленный в drive диск как доступный только для
     * чтения (или снять эту пометку). Сообщается FDC — дальше он сам не
     * даёт писать на этот дисковод, независимо от прав самого файла на
     * хосте (см. Fdc::is_write_protected()).
     */
    void fdd_set_write_protect(uint8_t drive, bool read_only);

    /**
     * @brief Статус всех 4 дисководов разом — для визуализации в GUI
     * (шторка/мотор/трекинг/чтение/запись/защита записи/выбранный привод).
     */
    Fdc::FdcStatus fdd_get_status() const;

    // =================================================================
    //  Отложенные операции ВГ93 (FDC PendingOp) — для GUI, которому
    //  нужны реалистичные тайминги дискеты (звуки, светодиод, пауза
    //  BUSY на время позиционирования). mcp_server/bridge этим НЕ
    //  пользуется и работает в мгновенном (эталонном) режиме.
    //  См. internal/fdc/fdc.hpp.
    // =================================================================
    void fdc_set_deferred_ops(bool enable) { fdc_.set_deferred_ops(enable); }
    bool fdc_is_deferred_ops() const { return fdc_.is_deferred_ops(); }
    bool fdc_has_pending_op() const { return fdc_.has_pending_op(); }
    fdc_pending_op_t fdc_get_pending_op() const { return fdc_.get_pending_op(); }
    fdc_op_result_t fdc_complete_pending_op() { return fdc_.complete_pending(); }
    void fdc_set_pending_op_callback(FdcPendingOpCallback cb) {
        fdc_.set_pending_op_callback(std::move(cb));
    }

    /**
     * @brief Подключить приёмник байтов принтера. Хост (gui_qt5) решает,
     * писать ли в TXT/PDF или на физический принтер — korvet_core сам
     * файлов не открывает. См. internal/printer/printer.hpp.
     */
    void printer_set_output_callback(PrinterByteCallback cb);

    /**
     * @brief Включить/выключить эмуляцию принтера. Пока выключен,
     * write_data()/send_to_output() игнорируют байты (гость видит
     * «принтер не подключён»). Вызывает хост при смене режима порта.
     */
    void printer_set_enabled(bool enabled) { printer_.set_enabled(enabled); }
    bool printer_is_enabled() const { return printer_.is_enabled(); }

    /**
     * @brief Подключить приёмник байтов, которые Корвет передаёт по RS232.
     * Хост сам решает транспорт: TCP, RS485, /dev/ttyUSB* и т.д.
     */
    void serial_set_byte_out_callback(SerialByteOutCallback cb);

    // Счётчики активности ИРПС (мышь/инъекции хоста = rx, передача гостя = tx).
    // Для индикатора активности в статус-баре GUI.
    uint32_t serial_rx_count() const { return serial_.get_rs232_status().rx_count; }
    uint32_t serial_tx_count() const { return serial_.get_rs232_status().tx_count; }

    /**
     * @brief Байт, пришедший ОТ хоста ("с провода") — кладётся в приёмный
     * буфер RS232, эмулируемая программа увидит его через порт данных.
     */
    void serial_inject_byte(uint8_t byte);

    /**
     * @brief Статусный регистр УСАПП (то, что гость читает из 0xFB11).
     * Разметка 8251A: 0x01 TxRDY | 0x02 RxRDY | 0x04 TxEMPTY | 0x10 OE
     * | 0x80 DSR (v1.2.3 — см. internal/serial/serial.cpp).
     */
    uint8_t serial_read_status();

    /**
     * @brief Подключить приёмник байтов LAN-порта — так же, как RS232,
     * это просто побайтовый поток, упаковку в кадры/пакеты делает хост.
     */
    void lan_set_byte_out_callback(LanByteOutCallback cb);

    /**
     * @brief Байт, пришедший ОТ хоста по LAN — кладётся в приёмный буфер.
     */
    void lan_inject_byte(uint8_t byte);
    
    /**
     * @brief Вставить кассету в магнитофон (из буфера)
     */
    bool tape_insert_buffer(const uint8_t* data, size_t size);
    
    /**
     * @brief Извлечь кассету из магнитофона
     */
    bool tape_eject();
    
    /**
     * @brief Получить текущее состояние CPU
     * @return Состояние CPU
     */
    cpu_state_t cpu_get_state() const { return cpu_.get_state(); }
    
    /**
     * @brief Установить состояние CPU
     * @param state Новое состояние CPU
     */
    void cpu_set_state(const cpu_state_t& state) { cpu_.set_state(state); }
    
    /**
     * @brief Установить/снять точку останова
     * @param addr Адрес точки останова
     * @param enabled true чтобы включить точку останова, false чтобы выключить
     */
    void cpu_set_breakpoint(uint16_t addr, bool enabled);
    
    /**
     * @brief Дизассемблировать инструкцию по указанному адресу
     */
    void disassemble(uint16_t addr, char* buffer, int size);
    
    // Доступ к состоянию только для чтения
    uint8_t* readout_ram() { return memory_.get_ram(); }
    
    /**
     * @brief Получить доступ к видео-буферу для чтения
     * @return Указатель на буфер кадра
     */
    uint8_t* readout_video() { return video_.get_framebuffer(); }
    
    /**
     * @brief Получить LUT RGB данные для GUI
     * @return Указатель на массив из 16 RgbColor структур
     */
    const RgbColor* readout_lut_rgb() { return video_.get_lut_rgb(); }
    
    /**
     * @brief Получить состояние видео-подсистемы
     * @return Состояние видео-подсистемы
     */
    mb_video_state_t readout_video_state() const { return video_.get_state(); }
    
    /**
     * @brief Получить значение системного регистра
     * @return Значение системного регистра
     */
    uint8_t readout_sysreg() const { return memory_.get_sysreg(); }

    /**
     * @brief Записать системный регистр (сырой байт, как гость пишет в
     * порт): sysreg_ = (value >> 2) & 0x1F — выбор банка памяти. Отладчик.
     */
    void debug_write_sysreg(uint8_t value) { memory_.set_sysreg(value); }

    /**
     * @brief Записать VIREG (страница/режим ГЗУ) — отладчик.
     */
    void debug_write_vireg(uint8_t value) { video_.write_vireg(value); }

    /**
     * @brief Запись в PIC/Timer по номеру порта — отладчик.
     */
    void debug_pic_write(uint8_t port, uint8_t value) { pic_.write(port, value); }
    void debug_timer_write(uint8_t port, uint8_t value) { timer_.write(port, value); }
    
    /**
     * @brief Получить значение регистра цвета
     * @return Значение регистра цвета (NCREG)
     */
    uint8_t readout_ncreg() const { return video_.get_ncreg(); }
    
    /**
     * @brief Получить значение регистра видео
     * @return Значение регистра видео (VIREG)
     */
    uint8_t readout_vireg() const { return video_.get_vireg(); }
    
    /**
     * @brief Получить страницу GZU для доступа CPU
     * @return Номер страницы GZU для CPU
     */
    uint8_t readout_gzu_page_access() const { return video_.get_gzu_read_page(); }
    
    /**
     * @brief Получить страницу GZU для отображения
     * @return Номер страницы GZU для отображения
     */
    uint8_t readout_gzu_page_show() const { return video_.get_gzu_show_page(); }
    
    /**
     * @brief Получить режим записи атрибутов ACZU
     * @return Режим записи атрибутов ACZU
     */
    uint8_t readout_aczu_attr_write() const { return video_.get_aczu_attr_write(); }
    
    /**
     * @brief Получить режим широкого шрифта ACZU
     * @return true если включен широкий режим, false если обычный
     */
    bool readout_aczu_wide_mode() const { return video_.get_aczu_wide_mode(); }
    
    /**
     * @brief Получить использование второго шрифта ACZU
     * @return true если используется второй шрифт, false если первый
     */
    bool readout_aczu_second_font() const { return video_.get_aczu_second_font(); }
    
    /**
     * @brief Получить текущую видео-линию
     * @return Номер текущей видео-линии (0-255)
     */
    uint8_t readout_video_line() const { return video_.get_current_line(); }

    /**
     * @brief Получить текущий адрес PC (программного счетчика)
     * @return Значение регистра PC
     */
    uint16_t readout_pc() const { return cpu_.get_pc(); }
    uint64_t get_total_executed_cycles() const { return total_executed_cycles_; }

    /// Получить счётчик тактов текущего кадра (сбрасывается в начале run_frame).
    /// Используется EmulationWorker для определения "полный кадр" vs "HLT-кадр".
    uint64_t get_frame_cycles() const { return cpu_.get_frame_cycles(); }

    // Сырые аппаратные данные для дампов
    const uint8_t* readout_aczu() const { return video_.get_aczu_data(); }
    size_t readout_aczu_size() const { return video_.get_aczu_size(); }
    const uint8_t* readout_gzu() const { return reinterpret_cast<const uint8_t*>(video_.get_gzu_planes().data()); }
    size_t readout_gzu_size() const { return video_.get_gzu_planes_size(); }
    const uint32_t* readout_lut() const { return video_.get_lut_data(); }
    size_t readout_lut_size() const { return video_.get_lut_size(); }
    const uint8_t* readout_rom() const { return memory_.get_rom(); }
    size_t readout_rom_size() const { return memory_.get_rom_size(); }
    const uint8_t* readout_mapper() const { return reinterpret_cast<const uint8_t*>(memory_.get_mapper().data()); }
    size_t readout_mapper_size() const { return memory_.get_mapper_size(); }

    Disasm& get_disasm() { return disasm_; }
    const Disasm& get_disasm() const { return disasm_; }

    // Диагностический интерфейс
    mb_diag_result_t run_diagnostic(mb_diag_category_t category);
    int run_all_diagnostics(mb_diag_result_t* results, int max_results);
    int run_category_diagnostics(mb_diag_category_t category, mb_diag_result_t* results, int max_results);
    void print_diagnostic_results(const mb_diag_result_t* results, int count);
    int get_diagnostic_test_count(mb_diag_category_t category);
    int get_total_diagnostic_test_count();

public:
    Motherboard() = default;
    ~Motherboard() = default;
    
    // Компоненты
    Cpu cpu_;
    Memory memory_;
    Video video_;
    Timer timer_;
    Audio audio_;
    Pic pic_;
    Ppi ppi_;
    Fdc fdc_;
    Serial serial_;
    Lan lan_;
    Tape tape_;
    Keyboard keyboard_;
    Mouse mouse_;
    Joystick joystick_;
    Printer printer_;
    Disasm disasm_;
    
    // Состояние
    bool initialized_ = false;
    bool powered_ = false;

    // Сохранённый указатель на конфигурацию (используется в connect_components()).
    const mb_config_t* saved_config_ = nullptr;
    bool paused_ = false;
    uint64_t total_executed_cycles_ = 0;  ///< Накопительный счётчик исполненных тактов

    // Тайминги кадра
    static constexpr uint32_t TICKS_PER_FRAME = 50000;            ///< T-states per frame (50fps @ 2.5MHz)
    static constexpr uint32_t CPU_CLK = 2500000;            ///< CPU clock frequency
    
    // Колбэки
    
    mb_video_frame_callback_t video_callback_ = nullptr;
    void* video_callback_data_ = nullptr;
    
    mb_interrupt_callback_t interrupt_callback_ = nullptr;
    void* interrupt_callback_data_ = nullptr;
    
    mb_log_callback_t log_callback_ = nullptr;
    void* log_callback_data_ = nullptr;
    
    // Инициализация компонентов
    void init_components();
    void connect_components();

    // -------------------------------------------------------------
    // Внутренний общий цикл исполнения (используется run_frame,
    // run_ticks, run_opcodes, run_while). max_ticks == 0 — без лимита
    // по тикам; max_instructions == 0 — без лимита по инструкциям.
    // -------------------------------------------------------------
    mb_run_result_t execute_loop(mb_exec_predicate_t pred, void* user_data,
                                 uint64_t max_ticks, uint64_t max_instructions);
    void begin_frame();   ///< VBLANK-начало кадра (сброс счётчика кадра)
    void end_frame();     ///< конец кадра: VBLANK-конец, IRQ4, звук, мышь
    void emit_trace_record();          ///< снимок состояния и вызов колбэка
    void track_dead_loop();            ///< детектор зависшего PC (как в run_frame)

    // Кадр «открыт», если исполнены не все TICKS_PER_FRAME тиков текущего кадра
    // (позиция внутри кадра сохраняется между вызовами run_*).
    bool frame_open_ = false;

    mb_trace_callback_t trace_callback_ = nullptr;
    void* trace_callback_data_ = nullptr;

    // Состояние детектора зависаний (перенесено из static в члены,
    // чтобы сохранялось между вызовами и сбрасывалось при power_on).
    uint16_t deadloop_last_pc_ = 0xFFFF;
    uint64_t deadloop_same_pc_count_ = 0;
};

}  // namespace korvet