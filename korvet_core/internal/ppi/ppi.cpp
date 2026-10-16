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
#include "../tape/tape.hpp"

namespace korvet
{

    void Ppi::init()
    {
        reset();
    }

    void Ppi::reset()
    {
        // Инициализация всех трёх PPI (КР580ВВ55)
        ppi1_ = ppi_chip_t{};
        ppi2_ = ppi_chip_t{};
        ppi3_ = ppi_chip_t{};

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

        // Детекторы фронтов кассеты/мыши — в исходное состояние
        prev_casout_ = 0;
        prev_stop_relay_ = false;
        prev_mouse_init_ = false;
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
                return read_ppi3_a();
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
                ppi1_.port_a.data = data;
                break;
            case 1:
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
                    disassemble_vireg_ppi1_c(static_cast<uint8_t>(oldc));
                    ppi1_.port_c.data = static_cast<uint8_t>(oldc);
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
                if (printer_)
                {
                    printer_->write_data(data);
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
                        ppi2_.port_c.data |= static_cast<uint8_t>(bit);
                    }
                    else
                    {
                        ppi2_.port_c.data = static_cast<uint8_t>(
                            ppi2_.port_c.data & (bit ^ 0xff));
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
                // PPI3_B — вход манипуляторов (чтение через read_ppi3_b)
                break;
            case 2:
                ppi3_.port_c.data = data;
                // PPI3_C — управление ЭУ8040: бит 6 — выбор слота
                // (0=ХР1/мышь, 1=ХР2/джойстик), бит 7 — «Разрешение»;
                // бит 3 — сигнал «инициализация» мыши ММ8031 (фронт);
                // бит 4 — линия «Синхр» (уровень, читается при опросе B).
                if (mouse_)
                {
                    bool init_bit = (data & 0x08) != 0;
                    if (init_bit && !prev_mouse_init_)
                    {
                        mouse_->mm8031_init_pulse();
                    }
                    prev_mouse_init_ = init_bit;
                }
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
                        ppi3_.port_c.data |= static_cast<uint8_t>(bit);
                    }
                    else
                    {
                        ppi3_.port_c.data = static_cast<uint8_t>(
                            ppi3_.port_c.data & (bit ^ 0xff));
                    }
                    // BSR-запись отдельных битов C затрагивает и биты
                    // управления ЭУ8040 — прогоняем через ту же логику
                    if (mouse_)
                    {
                        bool init_bit = (ppi3_.port_c.data & 0x08) != 0;
                        if (init_bit && !prev_mouse_init_)
                        {
                            mouse_->mm8031_init_pulse();
                        }
                        prev_mouse_init_ = init_bit;
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
        return static_cast<uint8_t>(assemble_ppi2_c());
    }

    // Специфичные функции PPI3
    uint8_t Ppi::read_ppi3_a() const
    {
        // Порт A ППР№3: чтение зависит от направления, заданного словом
        // режима (бит 4: 1 = ввод, 0 = вывод). В режиме ВЫВОДА чтение
        // возвращает выходную защёлку (семантика КР580ВВ55) — именно так
        // работает тест ПЗУ при включении (запись/считывание порта
        // расширения как «проверка шины»). В режиме ВВОДА порт читает
        // линии кнопок МД8030 (контакты 18-19 вилки ХР3 = Порт 0.4/0.5,
        // биты 4-5, активный низкий) через мультиплексор ЭУ8040.
        if ((ppi3_ctrl_word_ & 0x10) == 0) {
            return ppi3_.port_a.data;  // вывод: защёлка
        }
        uint8_t pc = ppi3_.port_c.data;
        if ((pc & 0x80) || !(pc & 0x40)) {
            return 0xFF;  // «Разрешение»=1 или слот ХР1 — линии свободны
        }
        if (joystick_)
        {
            return joystick_->read_ppi3_port_a();
        }
        return 0xFF;
    }

    uint8_t Ppi::read_ppi3_b() const
    {
        // Порт B ППР№3: аналогично — в режиме ВЫВОДА чтение возвращает
        // защёлку; в режиме ВВОДА порт читает манипуляторы через
        // мультиплексор ЭУ8040:
        //   бит 7 (PPI3-C, «Разрешение»): 1 — сигналы манипуляторов НЕ
        //     проходят на PPI3 (штекер «вынут») → порт читает 0xFF;
        //   бит 6 (выбор манипулятора): 0 — слот ХР1 (мышь ММ8031),
        //     1 — слот ХР2 (джойстик МД8030);
        //   бит 4 («Синхр»): уровень линии синхронизации счётчиков ММ8031.
        if ((ppi3_ctrl_word_ & 0x02) == 0) {
            return ppi3_.port_b.data;  // вывод: защёлка
        }
        uint8_t pc = ppi3_.port_c.data;
        if (pc & 0x80)
        {
            return 0xFF;  // «Разрешение» = 1: устройство не подключено
        }
        if (pc & 0x40)
        {
            // Слот ХР2 — джойстик МД8030
            if (joystick_)
            {
                return joystick_->read_ppi3_port_b();
            }
            return 0xFF;
        }
        // Слот ХР1 — параллельная мышь ММ8031 (активна в режиме
        // mouse_set_mode(MOUSE_PARALLEL); в остальных режимах слот пуст)
        if (mouse_ && mouse_->get_mode() == mb_mouse_mode_t::MOUSE_PARALLEL)
        {
            return mouse_->read_mm8031_port_b((pc & 0x10) != 0);
        }
        return 0xFF;
    }

    uint8_t Ppi::read_ppi3_b_raw() const
    {
        // Прямое чтение порта B без мультиплексора ЭУ8040 (отладка)
        return read_ppi3_b();
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

    uint8_t Ppi::assemble_ppi1_a() const
    {
        uint8_t value = 0;

        // Биты адреса LAN (4-7) — из компонента LAN (LANADR, ADRMSK=0F0H)
        if (lan_)
        {
            value |= static_cast<uint8_t>((lan_->get_address() & 0x0F) << 4);
        }

        // Бит атрибута АЦЗУ (3) — VISTS: бит 0 переменной visst_,
        // обновляемой при чтении АЦЗУ в режиме ATRFRE (режим 3).
        if (video_)
            value |= static_cast<uint8_t>((video_->get_aczu_attr_read() & 0x01) << 3);

        // Бит статуса принтера (2) — из компонента принтера (LSTRDY)
        if (printer_)
        {
            value |= static_cast<uint8_t>((printer_->read_status() & 0x01) << 2);
        }
        else
        {
            value |= 0x04; // Default to ready state
        }

        // VBLANK бит (1) — равен 1 только первые ~8170 тактов КАЖДОГО
        // кадра (обратный ход луча). БИОС опрашивает этот бит в цикле,
        // ожидая его сброса в 0. Считаем динамически от текущего числа
        // тактов в кадре.
        if (video_ && frame_cycles_getter_ && video_->is_in_vblank(frame_cycles_getter_()))
            value |= (1 << 1);

        // Бит входа кассеты (0) — CASIN: уровень входного сигнала
        // адаптера магнитофона в момент опроса (T-1: раньше —
        // константный 0, лента «молчала»). Гостевой загрузчик опрашивает
        // бит в плотном цикле — ленивая модель времени по тактам CPU
        // даёт достаточную точность (полупериоды ≥ 0,2 мс = 500 тактов).
        if (tape_ && cycle_getter_)
        {
            value |= static_cast<uint8_t>(tape_->read_cas_level(cycle_getter_()) & 0x01);
        }

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
            value |= static_cast<uint8_t>((video_->get_gzu_read_page() & 0x03) << 6);           // страница доступа
            value |= static_cast<uint8_t>((video_->get_aczu_attr_write() & 0x03) << 4);         // режим записи атрибута
            value |= static_cast<uint8_t>((video_->get_aczu_wide_mode() ? 0x01 : 0x00) << 3);   // широкий режим
            value |= static_cast<uint8_t>((video_->get_aczu_second_font() ? 0x01 : 0x00) << 2); // второй шрифт
            value |= static_cast<uint8_t>((video_->get_gzu_show_page() & 0x03) << 0);           // страница отображения
        }

        return value;
    }

    void Ppi::disassemble_ppi2_c(int value)
    {
        // Гейт звука: бит 3 (SNDEN) управляет выводом звука
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

        // Строб принтера (бит 5): печать — по НАРАСТАЮЩЕМУ фронту
        bool strobe_state = (value & 0x20) != 0;
        if (printer_)
        {
            printer_->process_strobe(strobe_state);
        }

        // Адаптер магнитофона (T-1):
        //   биты 1,0 — трёхуровневый выход CASOUT (SETLOW=00B,
        //             SETMID=01B, SETHIGH=11B);
        //   бит 2 — реле «кратковременного стопа» лентопротяжки.
        if (tape_)
        {
            uint8_t casout_code = static_cast<uint8_t>(value & 0x03);
            if (casout_code != prev_casout_)
            {
                prev_casout_ = casout_code;
                uint64_t cycle = cycle_getter_ ? cycle_getter_() : 0;
                tape_->on_casout_write(casout_code, cycle);
            }

            bool relay = (value & 0x04) != 0;
            if (relay != prev_stop_relay_)
            {
                prev_stop_relay_ = relay;
                uint64_t cycle = cycle_getter_ ? cycle_getter_() : 0;
                tape_->set_stop_relay(relay, cycle);
            }
        }
    }

    int Ppi::assemble_ppi2_c() const
    {
        // X-1: чтение выходного порта C ППР№2 возвращает ЗАЩЁЛКУ —
        // последнее записанное гостем значение (семантика КР580ВВ55:
        // чтение выходного порта — чтение выходного регистра).
        // Раньше сюда подмешивался бит 0 = выход канала 0 таймера,
        // которого в защёлке порта нет.
        return ppi2_.port_c.data;
    }

    uint8_t Ppi::read_joystick() const
    {
        // Состояние джойстика: берём экземпляр, если подключён
        if (joystick_)
        {
            return joystick_->read_ppi3_port_b();
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
