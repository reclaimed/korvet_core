/**
 * @file logger.cpp
 * @brief Реализация системы логирования эмулятора Korvet
 *
 * Два независимых канала:
 *   1. log_internal() — regular log (error/warning/info/debug),
 *      фильтр по level_, вывод через log_callback_.
 *   2. trace()        — trace log, фильтр по trace_subsystem_enabled_[],
 *      вывод через trace_callback_ с передачей mb_trace_entry_t.
 *
 * Соответствие docs/LOGGING.md:
 *   - T2.2: каждая запись Log содержит timestamp+level+[MODULE]+msg
 *   - T3.3/T5.1: trace() выполняет минимум работы при выключенной
 *     трассировке (одна-две проверки флагов до vsnprintf).
 *   - T8.1: потокобезопасно (mutex_).
 */

#include "logger.hpp"

// v1.1.1: тип блокировки логгера. В однопоточной сборке (bare-metal) —
// пустой объект вместо std::lock_guard<std::mutex>.
#if defined(KORVET_SINGLE_THREADED)
using LoggerLock = korvet::detail::FakeLock;
#else
using LoggerLock = std::lock_guard<std::mutex>;
#endif
#include "../config/config.hpp"
#include <cstdarg>
#include <ctime>
#include <cstring>
#include <cstdio>

namespace korvet {

// ---------- Инициализация ----------

void Logger::init(const Config* config) {
    if (config) {
        enabled_ = config->get_log_enabled();
        level_ = static_cast<mb_log_level_t>(config->get_log_level());
        target_ = config->get_log_target();
    } else {
        enabled_ = true;
        level_ = mb_log_level_t::MB_LOG_INFO;
        target_ = "console";
    }
}

void Logger::init(mb_log_level_t level, const char* target) {
    set_level(level);
    set_target(target ? target : "console");
    enabled_ = true;
}

// ---------- Методы лога (4 уровня) ----------

void Logger::error(const char* format, ...) {
    if (!enabled_ || level_ < mb_log_level_t::MB_LOG_ERROR) return;
    va_list args; va_start(args, format);
    log_internal(mb_log_level_t::MB_LOG_ERROR, mb_log_subsystem_t::SYS_GENERAL, format, args);
    va_end(args);
}

void Logger::warn(const char* format, ...) {
    if (!enabled_ || level_ < mb_log_level_t::MB_LOG_WARNING) return;
    va_list args; va_start(args, format);
    log_internal(mb_log_level_t::MB_LOG_WARNING, mb_log_subsystem_t::SYS_GENERAL, format, args);
    va_end(args);
}

void Logger::info(const char* format, ...) {
    if (!enabled_ || level_ < mb_log_level_t::MB_LOG_INFO) return;
    va_list args; va_start(args, format);
    log_internal(mb_log_level_t::MB_LOG_INFO, mb_log_subsystem_t::SYS_GENERAL, format, args);
    va_end(args);
}

void Logger::debug(mb_log_subsystem_t subsystem, const char* format, ...) {
    if (!enabled_ || level_ < mb_log_level_t::MB_LOG_DEBUG) return;
    va_list args; va_start(args, format);
    log_internal(mb_log_level_t::MB_LOG_DEBUG, subsystem, format, args);
    va_end(args);
}

// ---------- Внутренняя реализация лога ----------
//
// Формат строки Log: "[YYYY-MM-DD HH:MM:SS LEVEL][MODULE] message"
// — модуль (подсистема) печатается, чтобы соответствовать T2.2
// ("источник-модуль (например GUI:CoreManager, CORE:Motherboard)").
void Logger::log_internal(mb_log_level_t level, mb_log_subsystem_t subsystem,
                          const char* format, va_list args) {
    if (!enabled_ || !log_callback_) return;

    char formatted_msg[1024];
    char timestamp[32];

    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    const char* level_str = "LOG";
    switch (level) {
        case mb_log_level_t::MB_LOG_ERROR:   level_str = "ERROR"; break;
        case mb_log_level_t::MB_LOG_WARNING: level_str = "WARN";  break;
        case mb_log_level_t::MB_LOG_INFO:    level_str = "INFO";  break;
        case mb_log_level_t::MB_LOG_DEBUG:   level_str = "DEBUG"; break;
    }

    // Короткое имя подсистемы из единого источника истины (T4.4).
    const char* mod_str = subsystem_short_name(subsystem);

    int len = snprintf(formatted_msg, sizeof(formatted_msg),
                       "[%s %s][%s] ", timestamp, level_str, mod_str);
    if (len > 0 && len < static_cast<int>(sizeof(formatted_msg))) {
        vsnprintf(formatted_msg + len, sizeof(formatted_msg) - len, format, args);
    }

    LoggerLock lock(mutex_);
    log_callback_(level, subsystem, formatted_msg, log_user_data_);
}

// ---------- Регистрация колбэка ----------

void Logger::set_log_callback(mb_log_callback_t callback, void* user_data) {
    LoggerLock lock(mutex_);
    log_callback_ = callback;
    log_user_data_ = user_data;
}

// ---------- Конфигурация ----------

void Logger::set_level(mb_log_level_t level) { level_ = level; }
mb_log_level_t Logger::get_level() const { return level_; }

void Logger::set_enabled(bool enabled) { enabled_ = enabled; }
bool Logger::is_enabled() const { return enabled_; }

void Logger::set_target(const char* target) {
    if (!target) return;
    LoggerLock lock(mutex_);
    target_ = target;
}

const char* Logger::get_target() const { return target_.c_str(); }

}  // namespace korvet
