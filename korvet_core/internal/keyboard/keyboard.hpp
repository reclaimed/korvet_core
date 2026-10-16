#pragma once

#include <cstdint>
#include <array>

namespace korvet {

class Keyboard {
public:
    // Матрица Корвета: 8 строк × 13 столбцов. Основное поле — столбцы
    // 0..7 (линии KB00..KB07), дополнительное поле — столбцы 8..12
    // (KB08..KB11 + 13-я колонка с клавишей ПЧТ «печать экрана»,
    // адрес опроса 110H, бит 5 — по документации клавиатуры).
    // Чтение: адрес опроса выбирает ЛИНИЮ (столбец), возвращаемое
    // значение — маска строк (биты 0..7).
    static constexpr uint8_t ROWS = 8;
    static constexpr uint8_t COLS = 13;

    void init();
    void reset();

    // Единственный вход: key_code = (row<<8)|col. Перевод host-клавиши в
    // матрицу Корвета (раскладка, LED хоста и т.п.) — целиком забота
    // внешнего кода, ядро знает только про физическую матрицу.
    void key_press(uint16_t key_code);
    void key_release(uint16_t key_code);
    void clear_all();

    uint8_t read(uint16_t addr) const;

    void set_key_state(uint16_t row, uint16_t col, bool pressed);
    bool get_key_state(uint16_t row, uint16_t col) const;

    struct KeyboardStatus {
        bool connected;
        uint32_t keys_pressed_count;
        uint32_t total_keys_pressed;
    };

    KeyboardStatus get_status() const;

private:
    // key_state_[row][col] — как в original: KBD[row][col]
    std::array<std::array<bool, COLS>, ROWS> key_state_;

    uint32_t total_keys_pressed_;
    uint32_t current_keys_pressed_;

    // chk_matrix_line(n) — проверяет столбец n (0..11), возвращает маску строк (0..7)
    // Как в original: ChkMattrixLine(int N)
    int chk_matrix_line(int n) const;
};

}  // namespace korvet

