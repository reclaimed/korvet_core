/**
 * @file logger.hpp
 * @brief Система логирования эмулятора Korvet
 *
 * Regular log — error/warning/info/debug, фильтр по уровню, выводится
 * через log_callback_ (формат mb_log_callback_t).
 *
 * Раньше здесь же был отдельный Trace-канал (подробная построчная
 * трассировка каждой инструкции/обращения к порту). Он полностью убран:
 * не нужен для отладки, а при включении заметно расшатывал стабильность
 * FPS (тысячи лишних вызовов в секунду на горячих путях — CPU/Memory/PIC
 * и т.д.). Если понадобится точечная диагностика — используйте LOG_DEBUG_SYS
 * или временный LOG_WARN/LOG_ERROR в конкретном месте, без постоянной
 * инфраструктуры per-instruction трассировки.
 */

#pragma once

#include <cstdio>
#include <cstdarg>
#include <string>
#include <cstdint>
#include "../../motherboard_types.hpp"

// v1.1.1 (platform-agnostic): на bare-metal (RP2040/STM32 без RTOS) нет
// потоков — сборка с -DKORVET_SINGLE_THREADED убирает std::mutex из логгера
// (ядро больше не тянет библиотеку потоков). По умолчанию — многопоточно.
#if defined(KORVET_SINGLE_THREADED)
#include <mutex>   // только для std::lock_guard — см. korvet::FakeLock
namespace korvet::detail {
struct FakeLock {
    template <typename T> explicit FakeLock(T&) {}
};
}
#else
#include <mutex>
#endif

namespace korvet {

class Config;

/**
 * @brief Система логов эмулятора (Singleton, потокобезопасно).
 */
class Logger {
public:
    static Logger& get_instance() {
        static Logger instance;
        return instance;
    }

    // ---------- Инициализация ----------
    void init(const Config* config);
    void init(mb_log_level_t level, const char* target = "console");

    // ---------- Regular log methods (4 уровня) ----------
    void error(const char* format, ...);
    void warn(const char* format, ...);
    void info(const char* format, ...);
    void debug(mb_log_subsystem_t subsystem, const char* format, ...);

    // ---------- Регистрация callback'а ----------
    void set_log_callback(mb_log_callback_t callback, void* user_data);

    // ---------- Управление уровнем ----------
    void set_level(mb_log_level_t level);
    mb_log_level_t get_level() const;

    // ---------- Включение/выключение ----------
    void set_enabled(bool enabled);
    bool is_enabled() const;

    // ---------- Конфигурация вывода ----------
    void set_target(const char* target);
    const char* get_target() const;

private:
    Logger() = default;
    ~Logger() = default;

    mb_log_level_t level_{mb_log_level_t::MB_LOG_INFO};
    std::string target_{"console"};
    bool enabled_{true};
#if !defined(KORVET_SINGLE_THREADED)
    std::mutex mutex_;
#endif

    mb_log_callback_t log_callback_{nullptr};
    void* log_user_data_{nullptr};

    // --- Внутреннее ---
    void log_internal(mb_log_level_t level, mb_log_subsystem_t subsystem,
                      const char* format, va_list args);
};

// ===================================================================
//  Макросы — единый API авторов кода.
// ===================================================================
#define LOG_ERROR(fmt, ...)    korvet::Logger::get_instance().error(fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)      korvet::Logger::get_instance().warn(fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)      korvet::Logger::get_instance().info(fmt, ##__VA_ARGS__)
#define LOG_DEBUG_SYS(sys, fmt, ...) \
    korvet::Logger::get_instance().debug(sys, fmt, ##__VA_ARGS__)

}  // namespace korvet
