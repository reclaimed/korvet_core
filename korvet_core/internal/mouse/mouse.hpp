#pragma once

#include <cstdint>

namespace korvet {

class Serial;

/// Режим подключения мыши (фасад Motherboard::mouse_set_mode()).
/// Мышь Корвета существует в двух аппаратных вариантах:
///   MOUSE_SERIAL   — Microsoft-совместимая мышь на ИРПС (последовательная);
///   MOUSE_PARALLEL — ММ8031 через ЭУ8040 (порт расширения, PPI3);
///   MOUSE_OFF      — мышь не подключена (ввод игнорируется).
enum class mb_mouse_mode_t : uint8_t {
    MOUSE_OFF = 0,
    MOUSE_SERIAL = 1,
    MOUSE_PARALLEL = 2
};

class Mouse {
public:
    void init();
    void reset();

    // === Ввод хоста (фасад mouse_move/mouse_set_buttons) ===
    // dx/dy — в экранных координатах («вниз = положительный dy»);
    // режим определяет, куда уйдут приращения (см. mb_mouse_mode_t).
    void move(int16_t dx, int16_t dy);
    void set_buttons(uint8_t buttons);

    // === Режим подключения ===
    void set_mode(mb_mouse_mode_t mode) { mode_ = mode; }
    mb_mouse_mode_t get_mode() const { return mode_; }

    // === Последовательная мышь (ИРПС) ===
    void set_serial(Serial* serial) { serial_ = serial; }
    /// Раз в кадр из Motherboard::end_frame(): сформировать 3-байтовые
    /// посылки Microsoft-формата по событию движения/кнопок.
    void check_mouse();

    // === Параллельная мышь ММ8031 (через ЭУ8040, слот ХР1) ===
    /// Чтение порта B PPI3: смещённый код счётчика выбранной оси
    /// (Синхр=0 → СЧХ, Синхр=1 → СЧУ) + кнопки в битах 4-5 (активный
    /// низкий). После каждого считывания счётчик устанавливается
    /// в исходное состояние 1000b (формирователь Ф).
    uint8_t read_mm8031_port_b(bool sync);

    /// Сигнал «инициализация» с ПК (ХР3:04 = Порт 2.3 = PPI3-C бит 3):
    /// счётчики ММ8031 в исходное состояние (1000b).
    void mm8031_init_pulse();

    /// Состояние счётчиков ММ8031 (для отладки/тестов).
    uint8_t mm8031_get_count_x() const { return mm_cnt_x_; }
    uint8_t mm8031_get_count_y() const { return mm_cnt_y_; }

    // === Общее состояние (отладка) ===
    uint8_t get_buttons() const { return buttons_; }

private:
    int16_t accumulated_x_ = 0;   ///< накопитель X между кадрами (serial)
    int16_t accumulated_y_ = 0;   ///< накопитель Y между кадрами (serial)
    uint8_t buttons_ = 0;         ///< бит 0 — левая, бит 1 — правая

    int16_t max_delta_ = 127;     ///< клэмп посылки (±127, 1 байт со знаком)
    mb_mouse_mode_t mode_ = mb_mouse_mode_t::MOUSE_SERIAL;

    // === ММ8031: реверсивные 4-разрядные счётчики СЧХ/СЧУ ===
    // Смещённый код: 1000b = 0, 1111b = +7, 0000b = −8 (паспорт ММ8031 §2.6).
    uint8_t mm_cnt_x_ = 8;
    uint8_t mm_cnt_y_ = 8;

    // === Последовательный путь ===
    Serial* serial_ = nullptr;
    uint8_t prev_buttons_ = 0;    ///< кнопки предыдущей посылки

    /// Сброс счётчика ММ8031 с клэмпом приращения (−8..+7).
    static uint8_t mm_clamp(int value) {
        if (value < 0) return 0;
        if (value > 15) return 15;
        return static_cast<uint8_t>(value);
    }
};

}  // namespace korvet
