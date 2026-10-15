#pragma once

#include <cstdint>
#include <string>
#include "../i8080/i8080.hpp"

namespace korvet {

// Структура конфигурации
struct mb_config_t {
    // Системная конфигурация
    bool fdc_enabled;
    uint8_t gzu_pages;
    uint8_t lan_addr;

    // Настройки эмуляции
    int emulation_speed;  // 1-100 percentage, -1 for max speed
    bool sound_enabled;
    int sound_volume;     // Volume level 0-100
    int sound_sample_rate; // Audio sample rate (e.g., 22050, 44100)
    int sound_frequency;  // Audio frequency
    int sound_bit_depth;  // Audio bit depth (typically 8, 16)
    int sound_channels;   // Number of audio channels (1=mono, 2=stereo)
    bool sound_high_quality; // High quality audio processing
    int keyboard_layout;  // 0=QWERTY, 1=JCUKEN, 2=AUTO
    int kbd_led_initial_state;  // Initial keyboard LED state for AUTO layout mode

    // Конфигурация последовательных интерфейсов (только режимы, без хостовых путей)
    int rs232_mode;    // 0=disabled, 1=TCP listener, 2=TCP initiator, 3=physical port
    int lan_mode;      // 0=disabled, 1=master (initiator), 2=slave (listener)

    // Настройки отображения (OSD)
    bool osd_lut;
    bool osd_fps;
    bool osd_fdd;
    bool osd_cas;

    // Масштабирование яркости LUT (6-битный ЦАП VGA 0-63 → 8-битный RGB 0-255)
    float lut_r_scale = 255.0f; // Red channel scale factor
    float lut_g_scale = 255.0f; // Green channel scale factor
    float lut_b_scale = 255.0f; // Blue channel scale factor

    // Настройки окна/отладки
    int windowed;              // Windowed mode (1=on, 0=fullscreen)
    int debugger_on_start;     // Start debugger on startup (1=on, 0=off)

    // Настройки логирования
    int log_level;             // 0=ERROR, 1=WARN, 2=INFO, 3=DEBUG
    int log_enabled;           // 0=disabled, 1=enabled
    int peripherals_polling_interval; // Interval for polling peripherals in milliseconds

    // Пути к файлам (для тестов и загрузчика GUI)
    std::string rom_path = "data/rom.rom";
    std::string font_path = "data/korvet.fnt";
    std::string mapper_path = "data/mapper.mem";
    std::string disk_paths[4] = {"data/disk1.kdi", "data/disk2.kdi", "data/disk3.kdi", "data/disk4.kdi"};
    std::string tape_path = "data/cassette.kas";
    std::string log_target = "console";
    std::string rs232_port = "8021";
    std::string lan_port = "8020";
    std::string dump_dir = "dumps";
    std::string screenshot_dir = "screenshots";
    std::string printer_dir = "printer";
    std::string kdi_dir = "data";
    std::string kas_dir = "data";

    // ФИКС A4: защита LUT от записи (для отладки). По умолчанию false
    // (оригинальное поведение — палитра программируется BIOS).
    bool lut_write_protect = false;
};

// Состояние видео для отладки
struct mb_video_state_t {
    uint8_t gzu_read_page;     // Page for CPU access
    uint8_t gzu_show_page;     // Page for display
    uint8_t gzu_color_mode;    // COLORMD
    uint8_t gzu_read_mask;     // RDMSK
    uint8_t gzu_write_mask;    // WRMSK
    uint8_t gzu_write_bit;     // WBIT

    uint8_t aczu_attr_write;   // Attribute write mode
    bool aczu_wide_mode;       // 32 chars per line
    bool aczu_second_font;     // Alternative font
};

// Состояние и флаги CPU — в [i8080/i8080.hpp](../i8080/i8080.hpp).
// Псевдонимы для обратной совместимости:
using cpu_state_t = i8080::cpu_state_t;
using cpu_flags_t = i8080::cpu_flags_t;

// Log levels (T2.1: ровно 4 уровня Log-канала; Trace — отдельный канал)
enum class mb_log_level_t {
    MB_LOG_ERROR = 0,
    MB_LOG_WARNING = 1,
    MB_LOG_INFO = 2,
    MB_LOG_DEBUG = 3
};

// =====================================================================
//  Подсистемы трассировки. X-макрос MB_LOG_SUBSYSTEM_LIST —
//  единственный источник истины (T4.4). Из него генерируются:
//    1) enum class mb_log_subsystem_t
//    2) constexpr-массив коротких имён в namespace korvet::detail
//    3) inline-функция subsystem_short_name()
//
//  Добавление/удаление/переименование подсистемы — ТОЛЬКО здесь.
// =====================================================================
#define MB_LOG_SUBSYSTEM_LIST(X)                              \
    X(SYS_GENERAL,  "GEN") /* init, config, general events */ \
    X(SYS_CPU,      "CPU") /* CPU execution                */ \
    X(SYS_MEMORY,   "MEM") /* Memory read/write            */ \
    X(SYS_VIDEO,    "VID") /* Video subsystem (GZU, ACZU)  */ \
    X(SYS_TIMER,    "TIM") /* Timer                        */ \
    X(SYS_AUDIO,    "SND") /* Audio generation             */ \
    X(SYS_FDC,      "FDC") /* Floppy disk controller       */ \
    X(SYS_TAPE,     "TAP") /* Cassette tape                */ \
    X(SYS_KEYBOARD, "KB")  /* Keyboard matrix              */ \
    X(SYS_JOYSTICK, "JS")  /* Joystick                     */ \
    X(SYS_MOUSE,    "MS")  /* Mouse                        */ \
    X(SYS_SERIAL,   "SER") /* RS-232 serial                */ \
    X(SYS_LAN,      "LAN") /* Local network                */ \
    X(SYS_PRINTER,  "PRT") /* Printer                      */ \
    X(SYS_PPI,      "PPI") /* Parallel port interface      */ \
    X(SYS_PIC,      "PIC") /* Programmable interrupt ctl   */

enum class mb_log_subsystem_t {
#define X(name, str) name,
    MB_LOG_SUBSYSTEM_LIST(X)
#undef X
    SYS_COUNT  // sentinel — keep last
};

namespace detail {

// Короткие имена подсистем (T4.4: один источник истины).
// Сгенерировано из MB_LOG_SUBSYSTEM_LIST.
inline constexpr const char* subsystem_short_names[] = {
#define X(name, str) str,
    MB_LOG_SUBSYSTEM_LIST(X)
#undef X
};

inline constexpr int kSubsystemCount = static_cast<int>(mb_log_subsystem_t::SYS_COUNT);

}  // namespace detail

/**
 * @brief Короткое имя подсистемы для Trace-формата (T7.4).
 * @param sys Подсистема
 * @return Строка из 3 символов или "???" если значение вне диапазона.
 */
inline const char* subsystem_short_name(mb_log_subsystem_t sys) {
    int idx = static_cast<int>(sys);
    if (idx >= 0 && idx < detail::kSubsystemCount) {
        return detail::subsystem_short_names[idx];
    }
    return "???";
}

// Callback types — единый источник истины (T4.4, критерий приёмки №3).
// Сигнатуры проверены компилятором: и korvet_core, и gui_qt5
// используют одни и те же using-объявления из этого заголовка.
using mb_video_frame_callback_t = void(*)(const uint8_t* framebuffer, int width, int height, void* user_data);
using mb_interrupt_callback_t = void(*)(uint8_t irq_vector, void* user_data);
using mb_log_callback_t = void(*)(mb_log_level_t level, mb_log_subsystem_t subsystem, const char* message, void* user_data);

// =====================================================================
//  Трассировка исполнения (instruction trace).
//
//  Платформо-агностичный канал: ядро перед вызовом каждой инструкции
//  (и перед каждым подтверждением прерывания) формирует снимок состояния
//  и отдаёт его наружу через колбэк. Ядро НИЧЕГО не знает о том, куда
//  хост пишет записи (файл, RAM-буфер, UART на RP2040 — не его дело).
//  Формат двоичной записи фиксирован (34 байта, little-endian) и
//  документирован в TRACE_FORMAT.md; структура ниже — его точное
//  отображение, static_assert контролирует размер.
// =====================================================================
#pragma pack(push, 1)
struct mb_trace_record_t {
    uint64_t cycles;           ///< Тики процессора с момента power_on (ДО этой инструкции)
    uint16_t pc;               ///< PC исполняемой инструкции
    uint8_t  opcode_bytes[3];  ///< Байты по адресам PC, PC+1, PC+2 (без побочных эффектов)
    uint8_t  a, f;             ///< PSW до исполнения
    uint8_t  b, c, d, e, h, l; ///< Регистровые пары до исполнения
    uint16_t sp;               ///< Указатель стека
    uint8_t  iff;              ///< Флаг разрешения прерываний
    uint8_t  pic_irr;          ///< PIC: регистр запросов (IRR)
    uint8_t  pic_isr;          ///< PIC: регистр обслуживания (ISR)
    uint8_t  pic_imr;          ///< PIC: регистр масок (IMR)
    uint16_t timer_ch[3];      ///< Таймер: counting element каналов 0..2
    uint8_t  timer_out;        ///< Биты 0..2 = выходы каналов 0..2
};
#pragma pack(pop)
static_assert(sizeof(mb_trace_record_t) == 34, "mb_trace_record_t must be exactly 34 bytes (KVTR v1)");

using mb_trace_callback_t = void(*)(const mb_trace_record_t* record, void* user_data);

// =====================================================================
//  Управление исполнением с точностью до тика.
//
//  ВСЕ тайминги — в тиках процессора (2.5 МГц). Реального времени нет
//  ни на одном уровне ядра. Одна инструкция = 4..17 тиков; исполнение
//  всегда останавливается на границе инструкции, поэтому фактическое
//  число исполненных тиков может превышать запрошенное не более чем
//  на длину последней инструкции (см. mb_run_result_t::executed_ticks).
// =====================================================================

/// Причина остановки цикла исполнения.
enum class mb_run_stop_t {
    MB_RUN_COMPLETED  = 0, ///< Бюджет исчерпан / условие выполнено / предел инструкций
    MB_RUN_TIMEOUT    = 1, ///< run_while: таймаут (в тиках) исчерпан, условие не наступило
    MB_RUN_BREAKPOINT = 2, ///< Сработала точка останова CPU
    MB_RUN_HLT        = 3, ///< CPU не может продолжать (HLT-спин/внешняя остановка)
    MB_RUN_PAUSED     = 4  ///< Питание выключено либо эмуляция на паузе — ничего не исполнено
};

/// Результат выполнения цикла исполнения.
struct mb_run_result_t {
    uint64_t executed_ticks;        ///< Фактически исполнено тиков
    uint64_t executed_instructions; ///< Фактически исполнено инструкций (включая подтверждения прерываний)
    mb_run_stop_t stop_reason;
};

/// Предикат для run_while(): вернуть true — остановиться. Должен быть
/// чистым (без побочных эффектов): вызывается многократно, в т.ч. один
/// раз финально для различения «условие» vs «таймаут».
using mb_exec_predicate_t = bool(*)(void* user_data);

// Коды результатов диагностики
enum class mb_diag_result_code_t {
    MB_DIAG_PASS = 0,      // Test passed
    MB_DIAG_FAIL = 1,      // Test failed
    MB_DIAG_ERROR = 2,     // Error occurred during test
    MB_DIAG_TIMEOUT = 3,   // Test timed out
    MB_DIAG_SKIP = 4       // Test skipped
};

// Категории диагностических тестов
enum class mb_diag_category_t {
    MB_DIAG_CPU = 0,       // CPU operations
    MB_DIAG_MEMORY = 1,    // Memory operations
    MB_DIAG_VIDEO = 2,     // Video subsystem
    MB_DIAG_AUDIO = 3,     // Audio subsystem
    MB_DIAG_TIMER = 4,     // Timer operations
    MB_DIAG_PIC = 5,       // Interrupt controller
    MB_DIAG_PPI = 6,       // Peripheral interface
    MB_DIAG_FDC = 7,       // Floppy disk controller
    MB_DIAG_SERIAL = 8,    // Serial interfaces
    MB_DIAG_TAPE = 9,      // Tape subsystem
    MB_DIAG_KEYBOARD = 10, // Keyboard
    MB_DIAG_MOUSE = 11,    // Mouse
    MB_DIAG_JOYSTICK = 12, // Joystick
    MB_DIAG_PRINTER = 13,  // Printer
    MB_DIAG_SYSTEM = 14    // Overall system
};

// Структура результата диагностики
struct mb_diag_result_t {
    mb_diag_category_t category;      // Category of the test
    mb_diag_result_code_t result;     // Result of test
    int exit_code;                   // Exit code from test (if applicable)
    const char* message;             // Additional message from test
    uint64_t duration_us;            // Duration of test in microseconds
};

}  // namespace korvet
