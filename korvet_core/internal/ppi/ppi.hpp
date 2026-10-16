#pragma once

#include <cstdint>
#include <functional>

namespace korvet {

class Video;
class Fdc;
class Timer;
class Printer;
class Mouse;
class Joystick;
class Serial;
class Tape;
class Lan;

struct ppi_port_t {
    uint8_t data;
    uint8_t direction;  // 0=output, 1=input
    uint8_t control;
};

class Ppi {
public:
    void init();
    void reset();

    uint8_t read_port(uint8_t ppi_num, uint8_t port);
    void write_port(uint8_t ppi_num, uint8_t port, uint8_t data);
    void write_ctrl(uint8_t ppi_num, uint8_t data);

    // Функции для интерфейса памяти
    uint8_t read_ppi1_a() const { return read_ppi1_a_raw(); }
    uint8_t read_ppi1_b() const { return read_ppi1_b_raw(); }
    uint8_t read_ppi1_c() const { return read_vireg(); }
    void write_ppi1_a(uint8_t value);
    void write_ppi1_b(uint8_t value);
    void write_ppi1_c(uint8_t value) { write_vireg(value); }

    uint8_t read_ppi2_a() const { return read_ppi2_a_raw(); }
    uint8_t read_ppi2_b() const;
    uint8_t read_ppi2_c() const { return read_ppi2_c_raw(); }
    void write_ppi2_a(uint8_t value);
    void write_ppi2_b(uint8_t value);
    void write_ppi2_c(uint8_t value);

    uint8_t read_ppi3_a() const;
    uint8_t read_ppi3_b() const;
    uint8_t read_ppi3_c() const;
    void write_ppi3_a(uint8_t value);
    void write_ppi3_b(uint8_t value);
    void write_ppi3_c(uint8_t value);

    // Специфичные функции PPI1 (видео, управление дисководом)
    uint8_t read_ppi1_a_raw() const;
    uint8_t read_ppi1_b_raw() const;
    void write_drive_reg(uint8_t value);
    uint8_t read_drive_reg() const;
    void write_vireg(uint8_t value);
    uint8_t read_vireg() const;

    // Специфичные функции PPI2 (звук, кассета, принтер)
    void write_sound(uint8_t value);
    uint8_t read_ppi2_a_raw() const;
    uint8_t read_ppi2_c_raw() const;

    // Специфичные функции PPI3 (порт расширения: ЭУ8040, мышь, джойстик)
    uint8_t read_ppi3_b_raw() const;

    // Подключение компонентов
    void set_video(Video* video) { video_ = video; }
    void set_fdc(Fdc* fdc) { fdc_ = fdc; }
    void set_timer(Timer* timer) { timer_ = timer; }
    void set_printer(Printer* printer) { printer_ = printer; }
    void set_mouse(Mouse* mouse) { mouse_ = mouse; }
    void set_joystick(Joystick* joystick) { joystick_ = joystick; }
    void set_serial(Serial* serial) { serial_ = serial; }
    void set_tape(Tape* tape) { tape_ = tape; }
    void set_lan(Lan* lan) { lan_ = lan; }
    void set_frame_cycles_getter(std::function<uint32_t()> getter) { frame_cycles_getter_ = getter; }
    /// Геттер абсолютного времени (такты CPU с power_on) — для событий
    /// кассеты (CASOUT-уровень в такте N) и CASIN.
    void set_cycle_getter(std::function<uint64_t()> getter) { cycle_getter_ = getter; }

    // Методы для тестирования
    int get_sound_enable_state() const { return assemble_ppi2_c(); }
    int get_sound_enable() const { return sound_enable_; }

private:
    struct ppi_chip_t {
        ppi_port_t port_a;
        ppi_port_t port_b;
        ppi_port_t port_c;
        uint8_t control;
    };

    ppi_chip_t ppi1_;  // Видео, управление дисководом (VIREG, DRVREG)
    ppi_chip_t ppi2_;  // Звук, кассета, принтер
    ppi_chip_t ppi3_;  // Порт расширения (ЭУ8040: мышь/джойстик)

    Video* video_ = nullptr;
    Fdc* fdc_ = nullptr;
    Timer* timer_ = nullptr;
    Printer* printer_ = nullptr;
    Mouse* mouse_ = nullptr;
    Joystick* joystick_ = nullptr;
    Serial* serial_ = nullptr;
    Tape* tape_ = nullptr;
    Lan* lan_ = nullptr;

    // Внутреннее состояние
    int ppi1_ctrl_word_;
    int ppi2_ctrl_word_;
    int ppi3_ctrl_word_;

    // Флаг разрешения звука (бит 3 PPI2-C, SNDEN)
    int sound_enable_;

    // Предыдущие значения битов кассеты (детекторы фронтов):
    // биты 0-1 PPI2-C — уровень выхода CASOUT; бит 2 — реле STOP;
    // бит 3 PPI3-C — сигнал «инициализация» мыши ММ8031.
    uint8_t prev_casout_ = 0;
    bool prev_stop_relay_ = false;
    bool prev_mouse_init_ = false;

    // Геттер кадровых тиков CPU (для сигнала VBLANK)
    std::function<uint32_t()> frame_cycles_getter_;
    // Геттер абсолютных тактов CPU (для событий кассеты)
    std::function<uint64_t()> cycle_getter_;

    // Вспомогательные функции PPI1
    uint8_t assemble_ppi1_a() const;
    void disassemble_vireg_ppi1_c(uint8_t value);
    uint8_t assemble_vireg_ppi1_c() const;

    // Вспомогательные функции PPI2
    void disassemble_ppi2_c(int value);
    int assemble_ppi2_c() const;

    // Вспомогательные функции PPI3
    uint8_t read_joystick() const;
};

}  // namespace korvet
