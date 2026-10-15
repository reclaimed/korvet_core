#include "ppi.hpp"
#include "../logger/logger.hpp"
#include "../video/video.hpp"
#include "../fdc/fdc.hpp"
#include "../timer/timer.hpp"
#include "../printer/printer.hpp"
#include "../mouse/mouse.hpp"
#include "../joystick/joystick.hpp"
#include "../serial/serial.hpp"
#include "../lan/lan.hpp"

namespace korvet
{

    // Форварды внешних компонентов (глобальные экземпляры Motherboard)
    extern Printer *printer_instance;   // This would be connected externally
    extern Mouse *mouse_instance;       // This would be connected externally
    extern Joystick *joystick_instance; // This would be connected externally
    extern Serial *serial_instance;     // This would be connected externally
    extern Lan *lan_instance;           // This would be connected externally

    void Ppi::init()
    {
        reset();
    }

    void Ppi::reset()
    {
        // Инициализация всех трёх PPI (КР580ВВ55)
        ppi1_.port_a.data = 0;
        ppi1_.port_a.direction = 0; // Assume output by default
        ppi1_.port_a.control = 0;

        ppi1_.port_b.data = 0;
        ppi1_.port_b.direction = 0;
        ppi1_.port_b.control = 0;

        ppi1_.port_c.data = 0;
        ppi1_.port_c.direction = 0;
        ppi1_.port_c.control = 0;

        ppi1_.control = 0;

        ppi2_.port_a.data = 0;
        ppi2_.port_a.direction = 0;
        ppi2_.port_a.control = 0;

        ppi2_.port_b.data = 0;
        ppi2_.port_b.direction = 0;
        ppi2_.port_b.control = 0;

        ppi2_.port_c.data = 0;
        ppi2_.port_c.direction = 0;
        ppi2_.port_c.control = 0;

        ppi2_.control = 0;

        ppi3_.port_a.data = 0;
        ppi3_.port_a.direction = 0;
        ppi3_.port_a.control = 0;

        ppi3_.port_b.data = 0;
        ppi3_.port_b.direction = 0;
        ppi3_.port_b.control = 0;

        ppi3_.port_c.data = 0;
        ppi3_.port_c.direction = 0;
        ppi3_.port_c.control = 0;

        ppi3_.control = 0;

        ppi1_ctrl_word_ = 0;
        ppi2_ctrl_word_ = 0;
        ppi3_ctrl_word_ = 0;

        sound_enable_ = 0;

        // Аппаратно-корректное распространение сброса: после reset гейт
        // звука (PPI2-C бит 3) закрыт. Раньше флаг обнулялся только внутри
        // PPI, а таймер сохранял прошлое состояние стробирования — после
        // power_on звук мог «истекать» из неоткрытого гейта.
        if (timer_)
        {
            timer_->set_sound_enable(false);
        }

        // Инициализация связей
    }

    uint8_t Ppi::read_port(uint8_t ppi_num, uint8_t port)
    {
        switch (ppi_num)
        {
        case 0: // PPI1
            switch (port & 0x03)
            {
            case 0:
                return read_ppi1_a_raw();
            case 1:
                return read_ppi1_b_raw();
            case 2:
                return read_vireg();
            case 3:
                return 0xff; // Control register always returns 0xff on read
            }
            break;
        case 1: // PPI2
            switch (port & 0x03)
            {
            case 0:
                return read_ppi2_a();
            case 1:
                return read_ppi2_b();
            case 2:
                return read_ppi2_c();
            case 3:
                return 0xff; // Control register always returns 0xff on read
            }
            break;
        case 2: // PPI3
            switch (port & 0x03)
            {
            case 0:
                return ppi3_.port_a.data;
            case 1:
                return read_ppi3_b();
            case 2:
                return read_ppi3_c();
            case 3:
                return 0xff; // Control register always returns 0xff on read
            }
            break;
        }

        return 0;
    }

    uint8_t Ppi::read_ppi2_b() const
    {
        // PPI2 порт B — в оригинале не использовался (зарезервирован)
        return ppi2_.port_b.data;
    }

    void Ppi::write_port(uint8_t ppi_num, uint8_t port, uint8_t data)
    {
        switch (ppi_num)
        {
        case 0: // PPI1
            switch (port & 0x03)
            {
            case 0:
                // ФИКС C3: логируем запись в PPI1-A (важные системные флаги).
                ppi1_.port_a.data = data;
                break;
            case 1:
                // ФИКС C3: логируем запись в PPI1-B (DRVREG). Увидим ВСЕ записи в FDC.
                ppi1_.port_b.data = data;
                write_drive_reg(data);
                break;
            case 2:
                write_vireg(data);
                ppi1_.port_c.data = data;
                break;
            case 3:
                if (data & 0x80)
                {
                    ppi1_ctrl_word_ = data;
                }
                else
                {
                    int bit = 1 << ((data & 0x0e) >> 1);
                    int oldc = ppi1_.port_c.data;
                    if (data & 1)
                    {
                        oldc |= bit;
                    }
                    else
                    {
                        oldc &= bit ^ 0xff;
                    }
                    disassemble_vireg_ppi1_c(oldc);
                    ppi1_.port_c.data = oldc;
                }
                break;
            }
            break;
        case 1: // PPI2
            switch (port & 0x03)
            {
            case 0:
                ppi2_.port_a.data = data;
                // Запись в PPI2_A — это данные принтера (PPI2 порт A)
                if (printer_instance)
                {
                    printer_instance->write_data(data);
                }
                break;
            case 1:
                ppi2_.port_b.data = data;
                break;
            case 2:
                ppi2_.port_c.data = data;
                disassemble_ppi2_c(ppi2_.port_c.data);
                break;
            case 3:
                if (data & 0x80)
                {
                    ppi2_ctrl_word_ = data;
                }
                else
                {
                    int bit = 1 << ((data & 0x0e) >> 1);
                    if (data & 1)
                    {
                        ppi2_.port_c.data |= bit;
                    }
                    else
                    {
                        ppi2_.port_c.data &= bit ^ 0xff;
                    }
                }
                disassemble_ppi2_c(ppi2_.port_c.data);
                break;
            }
            break;
        case 2: // PPI3
            switch (port & 0x03)
            {
            case 0:
                ppi3_.port_a.data = data;
                break;
            case 1:
                ppi3_.port_b.data = data;
                // PPI3_B — ввод джойстика/мыши
                break;
            case 2:
                ppi3_.port_c.data = data;
                // PPI3_C — выбор устройства чтения (мышь или джойстик)
                break;
            case 3:
                if (data & 0x80)
                {
                    ppi3_ctrl_word_ = data;
                }
                else
                {
                    int bit = 1 << ((data & 0x0e) >> 1);
                    if (data & 1)
                    {
                        ppi3_.port_c.data |= bit;
                    }
                    else
                    {
                        ppi3_.port_c.data &= bit ^ 0xff;
                    }
                }
                break;
            }
            break;
        }
    }

    void Ppi::write_ctrl(uint8_t ppi_num, uint8_t data)
    {
        switch (ppi_num)
        {
        case 0:
            ppi1_.control = data;
            break;
        case 1:
            ppi2_.control = data;
            break;
        case 2:
            ppi3_.control = data;
            break;
        }
    }

    // Специфичные функции PPI1
    uint8_t Ppi::read_ppi1_a_raw() const
    {
        return assemble_ppi1_a();
    }

    uint8_t Ppi::read_ppi1_b_raw() const
    {
        return fdc_ ? fdc_->read_drvreg() : 0;
    }

    void Ppi::write_drive_reg(uint8_t value)
    {
        if (fdc_)
        {
            fdc_->write_drvreg(value);
        }
    }

    uint8_t Ppi::read_drive_reg() const
    {
        return fdc_ ? fdc_->read_drvreg() : 0;
    }

    void Ppi::write_vireg(uint8_t value)
    {
        disassemble_vireg_ppi1_c(value);
        ppi1_.port_c.data = value;
    }

    uint8_t Ppi::read_vireg() const
    {
        return assemble_vireg_ppi1_c();
    }

    // Специфичные функции PPI2
    void Ppi::write_sound(uint8_t value)
    {
        sound_enable_ = value;
    }

    uint8_t Ppi::read_ppi2_a_raw() const
    {
        return ppi2_.port_a.data;
    }

    uint8_t Ppi::read_ppi2_c_raw() const
    {
        return assemble_ppi2_c();
    }

    // Специфичные функции PPI3
    uint8_t Ppi::read_ppi3_a() const
    {
        return ppi3_.port_a.data;
    }

    uint8_t Ppi::read_ppi3_b() const
    {
        // Джойстик: берём экземпляр, если подключён
        if (joystick_instance)
        {
            return joystick_instance->read_ppi3_port_b();
        }
        // Фолбэк на оригинальную реализацию
        return read_ppi3_b_raw();
    }

    uint8_t Ppi::read_ppi3_b_raw() const
    {
        // Мышь: берём экземпляр, если подключён
        if (mouse_instance)
        {
            return mouse_instance->get_ppi3_port_b_value();
        }
        // Нет мыши — фолбэк на джойстик
        if (joystick_instance)
        {
            return joystick_instance->read_ppi3_port_b();
        }
        // Фолбэк по умолчанию
        return 0xFF;
    }

    uint8_t Ppi::read_ppi3_c() const
    {
        return ppi3_.port_c.data;
    }

    void Ppi::write_ppi3_a(uint8_t value)
    {
        ppi3_.port_a.data = value;
    }

    void Ppi::write_ppi3_b(uint8_t value)
    {
        ppi3_.port_b.data = value;
    }

    void Ppi::write_ppi3_c(uint8_t value)
    {
        ppi3_.port_c.data = value;
    }

    // Вспомогательные функции (история реализаций assemble_ppi1_a)
    // uint8_t Ppi::assemble_ppi1_a() const {
    //     uint8_t value = 0;

    //     // LAN address bits (4-7) - from LAN component
    //     if (lan_instance) {
    //         value |= (lan_instance->get_address() & 0x0F) << 4;
    //     } else {
    //         value |= 0x00;  // Default LAN address
    //     }

    //     // ACZU attribute read bit (3) - from video component
    //     if (video_) {
    //         value |= (video_->get_aczu_attr_read() & 0x01) << 3;
    //     }

    //     // Printer status bit (2) - from printer component
    //     if (printer_instance) {
    //         // Get printer status - 1 if ready, 0 if busy
    //         value |= (printer_instance->read_status() & 0x01) << 2;
    //     } else {
    //         value |= 0x04;  // Default to ready state
    //     }

    //     // VBLANK bit (1) - from video component (Takt < VBLANK_TICKS)
    //     if (video_ && frame_cycles_getter_) {
    //         value |= (video_->is_in_vblank(frame_cycles_getter_()) ? 1 : 0) << 1;
    //     }

    //     // Cassette input bit (0) - from tape component
    //     // Placeholder - would read actual cassette input state
    //     value |= 0x00;

    //     return value;
    // }

    // uint8_t Ppi::assemble_ppi1_a() const
    // {
    //     uint8_t value = 0;

    //     // LAN address bits (4-7) - from LAN component
    //     if (lan_instance)
    //     {
    //         value |= (lan_instance->get_address() & 0x0F) << 4;
    //     }
    //     else
    //     {
    //         value |= 0x00; // Default LAN address
    //     }

    //     // ACZU attribute read bit (3) - always 1 (FDC ready signal)
    //     value |= 0x08;

    //     // Printer status bit (2) - from printer component
    //     if (printer_instance)
    //     {
    //         // Get printer status - 1 if ready, 0 if busy
    //         value |= (printer_instance->read_status() & 0x01) << 2;
    //     }
    //     else
    //     {
    //         value |= 0x04; // Default to ready state
    //     }

    //     // VBLANK bit (1) - from video component (Takt < VBLANK_TICKS)
    //     if (video_ && frame_cycles_getter_)
    //     {
    //         value |= (video_->is_in_vblank(frame_cycles_getter_()) ? 1 : 0) << 1;
    //     }

    //     // Cassette input bit (0) - from tape component
    //     // Placeholder - would read actual cassette input state
    //     value |= 0x00;

    //     return value;
    // }

    uint8_t Ppi::assemble_ppi1_a() const
    {
        uint8_t value = 0;

        // Биты адреса LAN (4-7) — из компонента LAN
        if (lan_instance)
        {
            value |= (lan_instance->get_address() & 0x0F) << 4;
        }
        else
        {
            value |= 0x00; // Default LAN address
        }

    // ACZU attribute bit (3) — VISTS: бит 0 переменной scr_Attr_Read (visst_),
    // а НЕ флаг "был ли доступ к АЦЗУ". Эталон (ppi.c::Assemble_PPI1A):
    //   Value|=(scr_Attr_Read & 0x01)<<3;
    // scr_Attr_Read/visst_ обновляется в aczu_read() при scr_Attr_Write==3
    // (режим ATRFRE) значением АТРИБУТНОГО байта, считанного из памяти —
    // это НЕ булев "был ли доступ", а конкретное значение (обычно 0x00/0xFF).
    // Раньше здесь стоял aczu_attr_written_ (bool "была запись/чтение"),
    // что давало неверный бит и ломало self-test ПЗУ, ожидающий именно
    // (scr_Attr_Read & 1).
    if (video_)
        value |= (video_->get_aczu_attr_read() & 0x01) << 3;

        // Бит статуса принтера (2) — из компонента принтера
        if (printer_instance)
        {
            value |= (printer_instance->read_status() & 0x01) << 2;
        }
        else
        {
            value |= 0x04; // Default to ready state
        }

        // VBLANK bit (1) — как в оригинале: Value|=((Takt<VBLANK_TICKS)?1:0)<<1
        // Бит равен 1 только первые ~8170 тактов КАЖДОГО кадра (обратный ход луча),
        // а не весь кадр целиком. БИОС опрашивает этот бит в цикле, ожидая его
        // сброса в 0 (выход из обратного хода) — если бит держать в 1 весь кадр,
        // ожидание никогда не завершается и эмулятор виснет в цикле опроса.
        // Считаем динамически от текущего числа тактов в кадре, а не от статического
        // флага, выставляемого в начале/конце run_frame().
        if (video_ && frame_cycles_getter_ && video_->is_in_vblank(frame_cycles_getter_()))
            value |= (1 << 1);

        // Бит входа кассеты (0) — из компонента магнитофона
        // Заглушка (реальный вход кассеты не подключён)
        value |= 0x00;

        return value;
    }

    void Ppi::disassemble_vireg_ppi1_c(uint8_t value)
    {
        if (video_)
        {
            video_->write_vireg(value);
        }
    }

    uint8_t Ppi::assemble_vireg_ppi1_c() const
    {
        uint8_t value = 0;

        if (video_)
        {
            // Восстанавливаем регистр из состояния видео
            value |= (video_->get_gzu_read_page() & 0x03) << 6;           // scr_page_access
            value |= (video_->get_aczu_attr_write() & 0x03) << 4;         // scr_attr_write
            value |= (video_->get_aczu_wide_mode() ? 0x01 : 0x00) << 3;   // scr_wide_mode
            value |= (video_->get_aczu_second_font() ? 0x01 : 0x00) << 2; // scr_second_font
            value |= (video_->get_gzu_show_page() & 0x03) << 0;           // scr_page_show
        }

        return value;
    }

    void Ppi::disassemble_ppi2_c(int value)
    {
        // Гейт звука: бит 3 управляет выводом звука
        int new_sound_enable = (value & 0x08) >> 3;

        // Обновляем только при смене состояния гейта
        if (sound_enable_ != new_sound_enable)
        {
            sound_enable_ = new_sound_enable;

            // Передаём SoundEnable в таймер, чтобы стробировать звук
            if (timer_) {
                timer_->set_sound_enable(sound_enable_ != 0);
            }
        }

        // Строб принтера (бит 5; в оригинале — SetPrinterStrobe)
        // Эталон: SetPrinterStrobe((value & 0x20) >> 4);
        // У нас: бит 5 = строб принтера
        bool strobe_state = (value & 0x20) != 0;
        if (printer_instance)
        {
            printer_instance->process_strobe(strobe_state);
        }

        // Остальные биты — управление кассетой
        // Бит 2: мотор кассеты ВКЛ
        // Биты 1,0: уровень выхода кассеты
    }

    // int Ppi::assemble_ppi2_c() const {
    //     int value = 0;
    //     value |= (sound_enable_ & 0x01) << 3;
    //     return value;
    // }

    // int Ppi::assemble_ppi2_c() const {
    //     int value = 0;
    //     // Бит 3: при чтении отражает готовность FDC (INTRQ),
    //     // при записи управляет звуком (SOUND_ON).
    //     if (fdc_ && fdc_->get_intrq())   // FDC сигнализирует о завершении операции
    //         value |= 0x08;               // бит 3 = 1
    //     else
    //         value |= (sound_enable_ & 0x01) << 3;  // или просто состояние звука, если INTRQ=0
    //     return value;
    // }

    // int Ppi::assemble_ppi2_c() const
    // {
    //     int value = 0;
    //     // Бит 3: INTRQ от FDC (активный низкий? В original IntrqDrq=0xBF когда INTRQ=0,
    //     // и 0x3F когда INTRQ=1. Значит, INTRQ=1 — это низкий уровень бита 6? Запутанно.
    //     // Проще: всегда 1, так как FDC в покое.
    //     value |= 0x08; // бит 3 всегда 1
    //     return value;
    // }

    // int Ppi::assemble_ppi2_c() const
    // {
    //     // Бит 3 всегда 1 (INTRQ FDC), биты 2-0 — состояние звука/мотора
    //     return 0x08; // всегда возвращаем готовность FDC
    // }

    // int Ppi::assemble_ppi2_c() const
    // {
    //     int value = 0;
    //     // Бит 3: выход звукового канала таймера (меандр)
    //     if (timer_)
    //     {
    //         value |= (timer_->get_output(0) ? 1 : 0) << 3;
    //     }
    //     return value;
    // }

    int Ppi::assemble_ppi2_c() const
    {
        int value = 0;
        // Бит 3: SoundEnable (как в original/ppi.c: assemble_ppi2_c возвращает SoundEnable << 3)
        // При чтении PPI2-C бит 3 отражает состояние звука, установленное при последней записи
        if (sound_enable_)
            value |= 0x08;
        // Бит 0: выход звукового канала таймера (меандр)
        if (timer_ && timer_->get_output(0))
            value |= 0x01;
        return value;
    }

    uint8_t Ppi::read_joystick() const
    {
        // Состояние джойстика: берём экземпляр, если подключён
        if (joystick_instance)
        {
            return joystick_instance->read_ppi3_port_b();
        }
        // По умолчанию: кнопки не нажаты
        return 0xFF;
    }

    void Ppi::write_ppi2_c(uint8_t value)
    {
        ppi2_.port_c.data = value;
        disassemble_ppi2_c(ppi2_.port_c.data);
    }

} // namespace korvet