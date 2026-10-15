/**
 * @file logger_dump.hpp
 * @brief Заглушка модуля дампа логгера
 *
 * Ранние версии содержали обширный API dump'ов
 * (timestamps_enabled, get_messages_logged и т.п.), который
 * ссылался на нереализованные методы Logger. После приведения
 * системы логов/трейсов в соответствие с docs/LOGGING.md эти
 * дампы не нужны — состояние Logger'а читается через get_*-методы,
 * а Trace уже сериализуется в trace.log автоматически (T7).
 *
 * Заголовок оставлен для совместимости с потенциальными вызывающими
 * (в CMake korvet_core этот файл НЕ подключён). Все функции —
 * no-op, возвращают false/0 — вызывающий код обязан их игнорировать.
 */

#pragma once

#include "logger.hpp"
#include <cstdio>

namespace korvet {

class LoggerDump {
public:
    static bool dump_config_text(const Logger&, const char*)              { return false; }
    static bool dump_config_text_unique(const Logger&, const char*)       { return false; }
    static bool dump_state_binary(const Logger&, const char*)             { return false; }
    static bool dump_state_binary_unique(const Logger&, const char*)      { return false; }
    static bool dump_log_buffer(const Logger&, const char*)              { return false; }
    static bool dump_log_buffer_unique(const Logger&, const char*)       { return false; }
    static bool dump_full(const Logger&, const char*)                     { return false; }
    static bool dump_full_unique(const Logger&, const char*)              { return false; }
    static bool dump_settings(const Logger&, const char*)                 { return false; }
    static bool dump_settings_unique(const Logger&, const char*)          { return false; }
    static bool dump_message_history(const Logger&, const char*)          { return false; }
    static bool dump_message_history_unique(const Logger&, const char*)   { return false; }
    static bool dump_filters(const Logger&, const char*)                  { return false; }
    static bool dump_filters_unique(const Logger&, const char*)           { return false; }
    static bool dump_statistics(const Logger&, const char*)               { return false; }
    static bool dump_statistics_unique(const Logger&, const char*)        { return false; }
};

}  // namespace korvet
