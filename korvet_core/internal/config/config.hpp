/**
 * @file config.hpp
 * @brief Класс конфигурации эмулятора Korvet
 * 
 * Содержит определение класса Config, который хранит
 * изменяемую конфигурацию эмулятора ПК8020 "Корвет".
 * 
 * @author Korvet Team
 * @date 2024
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace korvet {

/**
 * @brief Уровни логирования
 */
enum class LogLevel {
    ERROR = 0,
    WARNING = 1,
    INFO = 2,
    DEBUG = 3,
    TRACE = 4
};

/**
 * @brief Класс хранения изменяемой конфигурации эмулятора
 * 
 * Предоставляет интерфейс для хранения и управления конфигурацией
 * эмулятора ПК8020 "Корвет". Конфигурация содержит только параметры,
 * относящиеся к эмуляции, без хост-специфичных настроек.
 */
class Config {
public:
    // Конструкторы и деструктор
    Config();
    ~Config() = default;
    
    // Копирование и перемещение
    Config(const Config& other) = default;
    Config& operator=(const Config& other) = default;
    Config(Config&& other) noexcept = default;
    Config& operator=(Config&& other) noexcept = default;
    
    // Методы доступа к конфигурации
    
    // Параметры эмуляции
    void set_emulation_speed(int speed_percent) { emulation_speed_ = speed_percent; }
    int get_emulation_speed() const { return emulation_speed_; }
    
    void set_fdc_enabled(bool enabled) { fdc_enabled_ = enabled; }
    bool get_fdc_enabled() const { return fdc_enabled_; }
    
    void set_gzu_pages(uint8_t pages) { gzu_pages_ = pages; }
    uint8_t get_gzu_pages() const { return gzu_pages_; }
    
    void set_lan_addr(uint8_t addr) { lan_addr_ = addr; }
    uint8_t get_lan_addr() const { return lan_addr_; }
    
    // Параметры звука
    void set_sound_enabled(bool enabled) { sound_enabled_ = enabled; }
    bool get_sound_enabled() const { return sound_enabled_; }
    
    void set_sound_volume(uint8_t volume) { sound_volume_ = volume; }
    uint8_t get_sound_volume() const { return sound_volume_; }
    
    void set_sound_sample_rate(uint32_t sample_rate) { sound_sample_rate_ = sample_rate; }
    uint32_t get_sound_sample_rate() const { return sound_sample_rate_; }
    
    void set_sound_frequency(double frequency) { sound_frequency_ = frequency; }
    double get_sound_frequency() const { return sound_frequency_; }
    
    void set_sound_bit_depth(uint8_t bit_depth) { sound_bit_depth_ = bit_depth; }
    uint8_t get_sound_bit_depth() const { return sound_bit_depth_; }
    
    void set_sound_channels(uint8_t channels) { sound_channels_ = channels; }
    uint8_t get_sound_channels() const { return sound_channels_; }
    
    void set_sound_high_quality(bool high_quality) { sound_high_quality_ = high_quality; }
    bool get_sound_high_quality() const { return sound_high_quality_; }
    
    // Параметры клавиатуры
    void set_keyboard_layout(int layout) { keyboard_layout_ = layout; }
    int get_keyboard_layout() const { return keyboard_layout_; }
    
    void set_kbd_led_initial_state(uint8_t state) { kbd_led_initial_state_ = state; }
    uint8_t get_kbd_led_initial_state() const { return kbd_led_initial_state_; }
    
    // Параметры отображения
    void set_windowed(bool windowed) { windowed_ = windowed; }
    bool get_windowed() const { return windowed_; }
    
    void set_osd_lut(bool enabled) { osd_lut_ = enabled; }
    bool get_osd_lut() const { return osd_lut_; }
    
    void set_osd_fps(bool enabled) { osd_fps_ = enabled; }
    bool get_osd_fps() const { return osd_fps_; }
    
    void set_osd_fdd(bool enabled) { osd_fdd_ = enabled; }
    bool get_osd_fdd() const { return osd_fdd_; }
    
    void set_osd_cas(bool enabled) { osd_cas_ = enabled; }
    bool get_osd_cas() const { return osd_cas_; }
    
    // Параметры отладки
    void set_debugger_on_start(bool enabled) { debugger_on_start_ = enabled; }
    bool get_debugger_on_start() const { return debugger_on_start_; }
    
    // Параметры логирования
    void set_log_level(LogLevel level) { log_level_ = level; }
    LogLevel get_log_level() const { return log_level_; }
    
    void set_log_enabled(bool enabled) { log_enabled_ = enabled; }
    bool get_log_enabled() const { return log_enabled_; }
    
    void set_log_target(const std::string& target) { log_target_ = target; }
    const std::string& get_log_target() const { return log_target_; }
    
    // Параметры джойстика
    void set_joystick_enabled(bool enabled) { joystick_enabled_ = enabled; }
    bool get_joystick_enabled() const { return joystick_enabled_; }
    
    void set_joystick_type(uint8_t type) { joystick_type_ = type; }
    uint8_t get_joystick_type() const { return joystick_type_; }
    
    // Параметры мыши
    void set_mouse_enabled(bool enabled) { mouse_enabled_ = enabled; }
    bool get_mouse_enabled() const { return mouse_enabled_; }
    
    void set_mouse_sensitivity(double sensitivity) { mouse_sensitivity_ = sensitivity; }
    double get_mouse_sensitivity() const { return mouse_sensitivity_; }
    
    // Параметры поллинга периферии
    void set_peripherals_polling_interval(uint32_t interval_ms) { peripherals_polling_interval_ = interval_ms; }
    uint32_t get_peripherals_polling_interval() const { return peripherals_polling_interval_; }
    
    // Методы для сохранения и загрузки конфигурации
    bool load_from_file(const char* filename);
    bool save_to_file(const char* filename) const;
    
    // Метод для сброса к значениям по умолчанию
    void reset_to_defaults();
    
    // Метод для проверки корректности значений
    bool validate() const;

private:
    // Параметры эмуляции
    int emulation_speed_{100};           ///< Скорость эмуляции в процентах (1-100) или -1 для максимальной
    bool fdc_enabled_{true};             ///< Включена ли эмуляция FDC
    uint8_t gzu_pages_{4};              ///< Количество страниц GZU (1-8)
    uint8_t lan_addr_{1};               ///< Адрес в сети LAN (1-16, где 1=РМП, 2-16=РМУ)
    
    // Параметры звука
    bool sound_enabled_{true};           ///< Включен ли звук
    uint8_t sound_volume_{50};           ///< Громкость звука (0-100)
    uint32_t sound_sample_rate_{44100};  ///< Частота дискретизации аудио
    double sound_frequency_{440.0};      ///< Базовая частота аудио
    uint8_t sound_bit_depth_{8};         ///< Битность аудио (8, 16)
    uint8_t sound_channels_{1};          ///< Количество каналов (1=моно, 2=стерео)
    bool sound_high_quality_{false};     ///< Использовать ли высококачественную обработку аудио
    
    // Параметры клавиатуры
    int keyboard_layout_{2};             ///< Раскладка клавиатуры (0=QWERTY, 1=JCUKEN, 2=AUTO)
    uint8_t kbd_led_initial_state_{2};   ///< Начальное состояние светодиода KBD_LED (для AUTO режима)
    
    // Параметры отображения
    bool windowed_{true};                ///< Оконный режим (true) или полноэкранный (false)
    bool osd_lut_{true};                 ///< Показывать OSD для LUT
    bool osd_fps_{true};                 ///< Показывать OSD для FPS
    bool osd_fdd_{true};                 ///< Показывать OSD для FDD
    bool osd_cas_{true};                 ///< Показывать OSD для магнитофона
    
    // Параметры отладки
    bool debugger_on_start_{false};      ///< Запускать ли отладчик при старте
    
    // Параметры логирования
    LogLevel log_level_{LogLevel::INFO}; ///< Уровень логирования
    bool log_enabled_{true};             ///< Включено ли логирование
    std::string log_target_{"console"};  ///< Цель логирования ("console", "file", или имя файла)
    
    // Параметры поллинга периферии
    uint32_t peripherals_polling_interval_{100};  ///< Интервал опроса периферии в миллисекундах
    
    // Параметры джойстика
    bool joystick_enabled_{true};        ///< Включен ли джойстик
    uint8_t joystick_type_{0};           ///< Тип джойстика (0=digital deflector, 1=analog, 2=gamepad)
    
    // Параметры мыши
    bool mouse_enabled_{true};           ///< Включена ли мышь
    double mouse_sensitivity_{1.0};      ///< Чувствительность мыши
};

}  // namespace korvet