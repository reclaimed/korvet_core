#pragma once

#include <cstdint>
#include <array>
#include <cstddef>
#include <functional>

namespace korvet {

/// Событие «уровень выхода на кассету» (CASOUT): уровень 0/1/2 в такте N.
/// Уровни по описанию элементов: SETLOW=00B → 0, SETMID=01B → 1,
/// SETHIGH=11B → 2 (трёхуровневый выход, кодируемый битами 0-1 PPI2-C).
using TapeOutCallback = std::function<void(uint8_t level, uint64_t cycle)>;

/// Событие «реле кратковременного стопа» (PPI2-C бит 2, STOP).
/// stopped=true — виртуальная лента остановлена (позиция замирает).
using TapeRelayCallback = std::function<void(bool stopped)>;

/**
 * @brief Адаптер магнитофона ПК8020 «Корвет»
 *
 * Аппаратный контракт полностью описан в документации:
 *   CASIN  — вход с кассеты: PPI1-A бит 0 (CASIN=38H, CASMSK=01H);
 *   CASOUT — выход на кассету: PPI2-C биты 1,0 — трёхуровневый
 *            (SETLOW=00B, SETMID=01B, SETHIGH=11B);
 *   STOP   — реле «кратковременного стопа»: PPI2-C бит 2 (через
 *            усилитель, управляет лентопротяжкой).
 *
 * «Разряды регистров отражают только состояние входных и выходных
 * сигналов, а кодирование и декодирование информации должны
 * осуществляться программно» — адаптер однобитовый в обе стороны,
 * модуляцию/формат блоков/чётность целиком разбирает гостевое ПО.
 * Ядро не знает ни TAP, ни WAV: хост конвертирует их в поток
 * ПОЛУПЕРИОДОВ (в тактах CPU) и вставляет через set_input()/
 * input_append(); события записи собирает колбэком set_out_callback().
 *
 * Виртуальное время ленты привязано к тактам CPU: при остановленном
 * реле STOP лента не движется (позиция не прирастует).
 */
class Tape {
public:
    /// Ёмкость буфера полупериодов (поток line-in может опережать гостя).
    static constexpr size_t MAX_HALF_PERIODS = 8192;

    void init();
    void reset();

    // === Вход (кассета → Корвет). Полупериоды в тактах CPU. ===

    /// Вставить «кассету»: начальный уровень + список полупериодов
    /// (времена между сменами уровня), единица — такты CPU (CPU_CLK
    /// тактов/с). Хост переводит секунды в такты через константу ядра.
    void set_input(uint8_t initial_level, const uint32_t* half_period_cycles, size_t count);

    /// Потоковое добавление полупериодов (line-in в реальном времени).
    /// Если буфер полон — самые старые полупериоды вытесняются.
    void input_append(const uint32_t* half_period_cycles, size_t count);

    /// Перемотка входного потока в начало (уровень — начальный).
    void input_rewind();

    /// Есть ли входные данные (кассета «вставлена»).
    bool is_input_available() const { return count_ > 0; }

    // === Выход (Корвет → кассета). ===

    /// Подключить приёмник событий CASOUT: (уровень 0/1/2, такт CPU).
    void set_out_callback(TapeOutCallback cb) { out_cb_ = std::move(cb); }

    /// Запись битов 0-1 PPI2-C (вызывает Ppi). Код уровня:
    /// 0b00=SETLOW(0), 0b01=SETMID(1), 0b11=SETHIGH(2); 0b10 в
    /// документации не определён — трактуется как SETMID.
    void on_casout_write(uint8_t level_code, uint64_t cycle);

    /// Последний уровень выхода (для индикации/дампов).
    uint8_t get_out_level() const { return out_level_; }

    // === Реле «кратковременный стоп» (PPI2-C бит 2). ===

    /// Установить состояние реле (вызывает Ppi при записи бита 2).
    /// stopped=true — лента стоит: виртуальное время не прирастает
    /// (позиция входного потока замирает до снятия реле).
    void set_stop_relay(bool stopped, uint64_t cycle);
    bool is_stop_relay() const { return relay_engaged_; }

    /// Подключить приёмник событий реле (для иконки UI).
    void set_relay_callback(TapeRelayCallback cb) { relay_cb_ = std::move(cb); }

    // === Вход CASIN. ===

    /// Уровень входного сигнала в текущий момент (PPI1-A бит 0).
    /// Лениво продвигает виртуальное время ленты к такту cpu_cycle.
    uint8_t read_cas_level(uint64_t cpu_cycle);

    /// Виртуальная позиция ленты в тактах CPU (без учёта пауз реле).
    uint64_t get_tape_position() const { return tape_pos_; }
    /// Число полупериодов, доступных для чтения (не потреблено).
    size_t get_available_periods() const { return count_ - read_idx_; }

private:
    /// Синхронизация виртуального времени с тактом CPU (учёт реле).
    void sync(uint64_t cpu_cycle);

    /// Продвижение уровня через границы полупериодов (лениво).
    void advance_input_level();

    /// Полупериоды входного потока, такты CPU каждый.
    std::array<uint32_t, MAX_HALF_PERIODS> periods_{};
    size_t count_ = 0;     ///< всего записано полупериодов
    size_t read_idx_ = 0;  ///< индекс текущего (непотреблённого) полупериода

    uint8_t initial_level_ = 0;  ///< уровень в начале потока
    uint8_t input_level_ = 0;    ///< текущий уровень CASIN
    uint64_t period_start_ = 0;  ///< позиция ленты на начале текущего полупериода

    uint64_t tape_pos_ = 0;    ///< виртуальное время ленты (такты, минус паузы)
    uint64_t wall_cycle_ = 0;  ///< такт CPU последней синхронизации
    bool relay_engaged_ = false;

    uint8_t out_level_ = 0;  ///< текущий уровень CASOUT (0/1/2)
    TapeOutCallback out_cb_;
    TapeRelayCallback relay_cb_;
};

}  // namespace korvet
