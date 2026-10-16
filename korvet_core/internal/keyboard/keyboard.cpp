#include "keyboard.hpp"
#include "../logger/logger.hpp"
#include <cstring>

namespace korvet {

void Keyboard::init() {
    reset();
}

void Keyboard::reset() {
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            key_state_[row][col] = false;
        }
    }

    total_keys_pressed_ = 0;
    current_keys_pressed_ = 0;
}

// key_code = (row << 8) | col, где row 0..7, col 0..11
// col — индекс колонки (0..11), а не битовая маска, чтобы избежать
// перекрытия бит колонок >=8 с битами row.
// Как в original: KeyAlias[KBD[row][col]] = key[scancode]
void Keyboard::key_press(uint16_t key_code) {
    uint8_t row = (key_code >> 8) & 0xFF;
    uint8_t col = key_code & 0xFF;

    if (row < ROWS && col < COLS) {
        if (!key_state_[row][col]) {
            key_state_[row][col] = true;
            current_keys_pressed_++;
            total_keys_pressed_++;
            // а LOG_DEBUG идёт через log_callback_ в korvet.log.
            // Сюда (trace.log) более логично — клавиатура это событие, не ошибка.
        }
    }
}

void Keyboard::key_release(uint16_t key_code) {
    uint8_t row = (key_code >> 8) & 0xFF;
    uint8_t col = key_code & 0xFF;

    if (row < ROWS && col < COLS) {
        if (key_state_[row][col]) {
            key_state_[row][col] = false;
            if (current_keys_pressed_ > 0) {
                current_keys_pressed_--;
            }
        }
    }
}

void Keyboard::clear_all() {
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            key_state_[row][col] = false;
        }
    }
}

// Опрос матрицы (адресная линия → маска строк):
//   if (Addr & 0x100) Line = 8;
//   for each bit 0..7 of addr: Value |= ChkMattrixLine(Line); Line++;
// ChkMattrixLine(N) проверяет столбец N, возвращает маску строк 0..7
uint8_t Keyboard::read(uint16_t addr) const {
    uint8_t value = 0;
    int line = 0;

    if (addr & 0x100) line = 8;

    if (addr & 0x0001) value |= chk_matrix_line(line);
    line++;
    if (addr & 0x0002) value |= chk_matrix_line(line);
    line++;
    if (addr & 0x0004) value |= chk_matrix_line(line);
    line++;
    if (addr & 0x0008) value |= chk_matrix_line(line);
    line++;
    if (addr & 0x0010) value |= chk_matrix_line(line);
    line++;
    if (addr & 0x0020) value |= chk_matrix_line(line);
    line++;
    if (addr & 0x0040) value |= chk_matrix_line(line);
    line++;
    if (addr & 0x0080) value |= chk_matrix_line(line);
    line++;

    return value;
}

// chk_matrix_line(N):
//   Value = 0;
//   Value |= KeyAlias[KBD[0][N]] ? 0x01 : 0;
//   Value |= KeyAlias[KBD[1][N]] ? 0x02 : 0;
//   ...
//   Value |= KeyAlias[KBD[7][N]] ? 0x80 : 0;
//   return Value;
// Проверяет столбец N (0..11), возвращает маску строк (0..7)
int Keyboard::chk_matrix_line(int n) const {
    if (n >= COLS) {
        return 0;
    }

    int value = 0;
    for (int row = 0; row < ROWS; row++) {
        if (key_state_[row][n]) {
            value |= (1 << row);
        }
    }

    return value;
}

void Keyboard::set_key_state(uint16_t row, uint16_t col, bool pressed) {
    if (row < ROWS && col < COLS) {
        key_state_[row][col] = pressed;
    }
}

bool Keyboard::get_key_state(uint16_t row, uint16_t col) const {
    if (row < ROWS && col < COLS) {
        return key_state_[row][col];
    }
    return false;
}

Keyboard::KeyboardStatus Keyboard::get_status() const {
    KeyboardStatus status;
    status.connected = true;

    uint32_t pressed_count = 0;
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            if (key_state_[row][col]) {
                pressed_count++;
            }
        }
    }

    status.keys_pressed_count = pressed_count;
    status.total_keys_pressed = total_keys_pressed_;

    return status;
}

}  // namespace korvet
