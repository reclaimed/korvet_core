#include "motherboard.hpp"
#include "internal/logger/logger.hpp"
#include <chrono>  // ФИКС 5: для FPS в frame stat
#include <cstring>
#include <cstdlib>
#include <cstdio>

namespace korvet
{

    // Глобальные экземпляры компонентов для подключений PPI
    Printer *printer_instance = nullptr;
    Mouse *mouse_instance = nullptr;
    Joystick *joystick_instance = nullptr;
    Serial *serial_instance = nullptr;
    Lan *lan_instance = nullptr; // Добавлен экземпляр LAN

    bool Motherboard::init(const mb_config_t *config)
    {
        // ФИКС 6: сохранить указатель для использования в connect_components().
        saved_config_ = config;
        if (initialized_)
        {
            return false;
        }

        // Инициализация логгера из конфигурации
        Logger::get_instance().init(
            static_cast<mb_log_level_t>(config->log_level),
            config->log_enabled ? "stdout" : "null");

        // T1.2: инициализация приложения — допустимый пользовательский Log.
        LOG_INFO("Motherboard initializing...");

        init_components();
        connect_components();

        // Инициализация по конфигу
        memory_.init(config, &video_, &fdc_);
        video_.init(config);
        fdc_.init();
        pic_.init();
        ppi_.init();
        serial_.init();
        lan_.init(); // Initialize new LAN component
        tape_.init();
        keyboard_.init();

        // Инициализация таймера
        timer_.init();
        audio_.init();
        timer_.set_audio(&audio_);
        timer_.set_sound_enable(config->sound_enabled);
        audio_.set_volume(static_cast<uint8_t>(config->sound_volume));

        mouse_.init();
        joystick_.init();
        printer_.init();
        disasm_.init();

        // ФИКС: раньше здесь конфигурировался транспорт RS232/LAN
        // (TCP/физический порт) через enum SerialMode/LanMode — но это
        // были пустые заглушки (setup_tcp_listener() и т.п. ничего не
        // делали) и лишний повод для korvet_core знать о сокетах и
        // физических портах хоста. Транспорт целиком на стороне хоста:
        // gui_qt5 сам решает, куда девать байты — см.
        // serial_set_byte_out_callback()/lan_set_byte_out_callback() ниже
        // и internal/serial/serial.hpp, internal/lan/lan.hpp.

        // Таймер -> PIC (прерывания)
        timer_.set_pic(&pic_);

        initialized_ = true;
        // T1.2: завершение инициализации — допустимый пользовательский Log.
        LOG_INFO("Motherboard initialized successfully");
        return true;
    }

    void Motherboard::shutdown()
    {
        if (!initialized_)
        {
            return;
        }

        // T8.2: отвязать callback до уничтожения компонентов, чтобы
        // Logger не вызвал его в деструкторах/при поздних событиях.
        Logger::get_instance().set_log_callback(nullptr, nullptr);

        initialized_ = false;
        powered_ = false;
        paused_ = false;
    }

    void Motherboard::power_on()
    {
        if (!initialized_)
        {
            return;
        }

        // Включение питания = машина начинает работать. paused_ здесь
        // снимаем ЯВНО (v1.2): отладчик ставит эмуляцию на паузу, и если
        // пользователь выключил/включил питание (или сделал «Сброс» =
        // power_off+power_on), машина обязана ЗАПУСТИТЬСЯ, а не остаться
        // замороженной (багрепорт: «после отладчика эмулятор перестаёт
        // выключаться и сбрасываться» — экран не гас и не перезагружался,
        // т.к. run_frame() молчал при paused_ == true).
        paused_ = false;

        powered_ = true;
        cpu_.init(&memory_, &pic_);
        cpu_.reset();
        memory_.reset();
        video_.reset();
        fdc_.reset(); // originals/_main.c: FDC_Reset()

        // Диагностическая инструментация удалена.

        timer_.reset();
        pic_.reset();
        ppi_.reset();
        serial_.reset();
        lan_.reset(); // Сброс компонента LAN
        tape_.reset();
        keyboard_.reset();
        mouse_.reset();
        joystick_.reset();

        // Счётчик «тиков с момента включения»: power_on — точка отсчёта.
        // Позиция кадра и детектор зависаний тоже начинают заново.
        total_executed_cycles_ = 0;
        frame_open_ = false;
        deadloop_last_pc_ = 0xFFFF;
        deadloop_same_pc_count_ = 0;
    }

    void Motherboard::power_off()
    {
        // Выключение — чистое состояние: пауза тоже снимается (v1.2),
        // иначе «выкл → вкл» наследовал бы паузу от отладчика.
        paused_ = false;
        powered_ = false;
    }

    void Motherboard::pause()
    {
        paused_ = true;
    }

    void Motherboard::resume()
    {
        paused_ = false;
    }

    void Motherboard::cold_reset()
    {
        power_off();
        power_on();
    }

    void Motherboard::warm_reset()
    {
        cpu_.reset();
        pic_.reset();
    }

    void Motherboard::step()
    {
        if (!powered_)
        {
            return;
        }

        // Отладочный одиночный шаг. Если остался незакрытый (частичный)
        // кадр от run_ticks/run_while — закрываем его без конца кадра
        // (цикл VBLANK/IRQ4 начнётся заново со следующего run_*).
        if (frame_open_)
        {
            total_executed_cycles_ += cpu_.get_frame_cycles();
            frame_open_ = false;
        }

        emit_trace_record();

        // Одиночный шаг CPU методом отладчика.
        // paused_ здесь НЕ проверяем: отладчик сам вызывает pause() перед
        // шагом, а метод обязан исполнять и при paused_ == true.
        cpu_.reset_frame_cycles();
        cpu_.execute_single_step();
        total_executed_cycles_ += cpu_.get_frame_cycles();
    }

    void Motherboard::set_breakpoint(uint16_t addr, bool enabled)
    {
        cpu_.set_breakpoint(addr, enabled);
        memory_.set_breakpoint(addr, enabled);
    }

    bool Motherboard::has_breakpoint(uint16_t addr) const
    {
        return cpu_.has_breakpoint(addr);
    }

    void Motherboard::run(uint32_t cycles)
    {
        (void)cycles;
        run_frame();
    }

    void Motherboard::begin_frame()
    {
        cpu_.reset_frame_cycles();

        // Устанавливаем VBLANK в начале кадра для PPI1-A бит 1
        // В emu80: KorvetCore::vrtc() → m_ppiCircuit->setVbl(true) при старте неактивной области
        // В оригинале: All_TAKT=50000, VBLANK_TICKS=8170 — первые ~8170 тактов это VBLANK
        ppi_.set_vbl_state(true);

        // Передаём ACZU attr written flag из video в ppi перед циклом
        // ПЗУ опрашивает бит 3 PPI1-A в начале загрузки, чтобы понять,
        // был ли доступ к ACZU (холодный старт vs тёплый старт)
        ppi_.set_aczu_attr_read(video_.get_aczu_attr_written());

        frame_open_ = true;
    }

    void Motherboard::end_frame()
    {
        // Сбрасываем VBLANK в конце кадра (после VBLANK_TICKS=8170 тактов луч переходит в активную зону)
        ppi_.set_vbl_state(false);

        // IRQ4 VBLANK поднимается ПОСЛЕ полной порции тактов кадра
        pic_.raise_irq(4);
        total_executed_cycles_ += cpu_.get_frame_cycles();
        frame_open_ = false;

        // Синхронный звук: ровно один раз за кадр, как MakeSound() в
        // эталоне — переписывает буфер целиком поверх предыдущего кадра.
        audio_.generate_frame_buffer();

        mouse_.check_mouse();
    }

    void Motherboard::track_dead_loop()
    {
        // ФИКС C5: dead-loop detector. Порог WARN — 10k инструкций (~0.4 сек при 50Hz),
        // ERROR — 100k. Логика идентична прежней run_frame (состояние — в членах,
        // чтобы сохранялось между вызовами и сбрасывалось при power_on).
        static constexpr uint64_t DEAD_LOOP_WARN  = 10000;    // ~0.4 sec at 50Hz
        static constexpr uint64_t DEAD_LOOP_ERROR = 100000;   // ~4 sec
        uint16_t cur_pc = cpu_.get_pc();
        if (cur_pc == deadloop_last_pc_)
        {
            if (++deadloop_same_pc_count_ == DEAD_LOOP_WARN)
            {
                // T1.2: WARN допустим (пользователь должен увидеть зависание
                // даже без трассировки). Это граничное событие — приложение
                // реально не отвечает.
                LOG_WARN("DEAD LOOP at PC=0x%04X, ~%llu instrs (likely waiting for HW that never responds)",
                         cur_pc, (unsigned long long)DEAD_LOOP_WARN);
            }
            else if (deadloop_same_pc_count_ == DEAD_LOOP_ERROR)
            {
                LOG_ERROR("STUCK at PC=0x%04X, ~%llu instrs — definite deadlock",
                          cur_pc, (unsigned long long)DEAD_LOOP_ERROR);
            }
        }
        else
        {
            deadloop_last_pc_ = cur_pc;
            deadloop_same_pc_count_ = 0;
        }
    }

    void Motherboard::emit_trace_record()
    {
        if (!trace_callback_)
        {
            return;
        }

        mb_trace_record_t r = {};
        r.cycles = get_current_cycles();

        uint16_t pc = cpu_.get_pc();
        r.pc = pc;
        // Три байта инструкции — через peek(): чтение через маппер, но
        // БЕЗ побочных эффектов (не трогаем порты периферии).
        r.opcode_bytes[0] = memory_.peek(pc);
        r.opcode_bytes[1] = memory_.peek(static_cast<uint16_t>(pc + 1));
        r.opcode_bytes[2] = memory_.peek(static_cast<uint16_t>(pc + 2));

        auto st = cpu_.get_state();
        r.a = st.a; r.f = st.f;
        r.b = st.b; r.c = st.c;
        r.d = st.d; r.e = st.e;
        r.h = st.h; r.l = st.l;
        r.sp = st.sp;
        r.iff = static_cast<uint8_t>(st.iff);

        r.pic_irr = pic_.read_irr();
        r.pic_isr = pic_.read_isr();
        r.pic_imr = pic_.read_imr();

        for (int i = 0; i < 3; i++)
        {
            auto ch = timer_.get_channel(static_cast<uint8_t>(i));
            r.timer_ch[i] = ch.counting_element;
            if (ch.out)
            {
                r.timer_out |= static_cast<uint8_t>(1u << i);
            }
        }

        trace_callback_(&r, trace_callback_data_);
    }

    uint64_t Motherboard::get_current_cycles() const
    {
        // Закрытые кадры + текущий открытый (частичный) кадр.
        return total_executed_cycles_ + (frame_open_ ? cpu_.get_frame_cycles() : 0);
    }

    mb_run_result_t Motherboard::execute_loop(mb_exec_predicate_t pred, void* user_data,
                                              uint64_t max_ticks, uint64_t max_instructions)
    {
        mb_run_result_t res = {};
        res.stop_reason = mb_run_stop_t::MB_RUN_COMPLETED;

        if (!powered_ || paused_)
        {
            res.stop_reason = mb_run_stop_t::MB_RUN_PAUSED;
            return res;
        }

        // Продолжаем открытый (частичный) кадр либо открываем новый.
        if (!frame_open_)
        {
            begin_frame();
        }

        // Главный цикл: 1/50 секунды гостевого времени = TICKS_PER_FRAME процессорных тактов.
        // CPU возвращает cost > 0 для обычных инструкций (и подтверждений
        // прерываний — 11 тиков), cost = 0 — точка останова (кадр завершаем
        // досрочно, как в прежней run_frame).
        // Эталон: examples/original/_main.c:454 — if (Takt >= TICKS_PER_FRAME).
        while (true)
        {
            // 1) Условие остановки (перед каждым шагом — и до первого).
            if (pred && pred(user_data))
            {
                break;
            }
            // 2) Лимит инструкций.
            if (max_instructions != 0 && res.executed_instructions >= max_instructions)
            {
                break;
            }
            // 3) Лимит тиков (бюджет/таймаут).
            if (max_ticks != 0 && res.executed_ticks >= max_ticks)
            {
                break;
            }

            // 4) Снимок трассы ДО исполнения шага.
            emit_trace_record();

            // 5) Исполнение.
            int cost = cpu_.execute_one_instruction();
            if (cost <= 0)
            {
                // Точка останова — кадр завершён досрочно (как в прежней
                // run_frame: VBLANK-конец, IRQ4, звук, учёт тиков).
                res.stop_reason = cpu_.was_breakpoint_hit()
                                      ? mb_run_stop_t::MB_RUN_BREAKPOINT
                                      : mb_run_stop_t::MB_RUN_HLT;
                if (frame_open_)
                {
                    end_frame();
                }
                break;
            }

            res.executed_ticks += static_cast<uint64_t>(cost);
            res.executed_instructions++;

            // Обновляем счётчик dead-loop ПОСЛЕ выполнения инструкции
            track_dead_loop();

            // tick() ведёт канал 0 (звук), аудио и проверку прерываний
            timer_.tick(static_cast<uint32_t>(cost));

            // Channels 1-2 NOT ticked per-instruction — matches original emulator
            // where DoTMR(1,N) and DoTMR(2,N) are commented out in Timer_Write().
            // Каналы 1/2 обновляются только прямыми записями в порты.

            // ФИКС: в эталоне (vg.c/floppy.c/_main.c) ВГ93 НЕ генерирует
            // аппаратное прерывание через PIC вообще — нигде не вызывается
            // PIC_IntRequest() для FDC. INTRQ — чисто программный флаг
            // (вычисляется как !RegStatus&0x01, "не занят"), который ПЗУ
            // само опрашивает через регистр состояния. Ранее здесь стоял
            // pic_.raise_irq(7), которого в оригинале нет — убран.

            // 6) Конец кадра — каждые TICKS_PER_FRAME фактических тиков внутри кадра.
            //    Сразу открываем следующий кадр: begin_frame() сбрасывает
            //    frame_cycles (иначе условие >= TICKS_PER_FRAME останется истинным
            //    и end_frame() будет зваться на каждой инструкции).
            if (cpu_.get_frame_cycles() >= TICKS_PER_FRAME)
            {
                end_frame();
                begin_frame();
            }
        }

        // Частичный кадр остаётся открытым: следующий run_* продолжит его
        // (границы кадров не сдвигаются — точная суммируемая timeline).
        return res;
    }

    mb_run_result_t Motherboard::run_ticks(uint64_t ticks)
    {
        if (ticks == 0)
        {
            mb_run_result_t res = {};
            res.stop_reason = mb_run_stop_t::MB_RUN_COMPLETED;
            return res;
        }
        return execute_loop(nullptr, nullptr, ticks, 0);
    }

    mb_run_result_t Motherboard::run_opcodes(uint64_t count)
    {
        if (count == 0)
        {
            mb_run_result_t res = {};
            res.stop_reason = mb_run_stop_t::MB_RUN_COMPLETED;
            return res;
        }
        return execute_loop(nullptr, nullptr, 0, count);
    }

    mb_run_result_t Motherboard::run_while(mb_exec_predicate_t pred, void* user_data,
                                           uint64_t timeout_ticks)
    {
        mb_run_result_t res = execute_loop(pred, user_data, timeout_ticks, 0);
        // Различаем «условие выполнено» и «исчерпан таймаут» финальной
        // проверкой предиката (он обязан быть чистым).
        if (res.stop_reason == mb_run_stop_t::MB_RUN_COMPLETED && pred)
        {
            if (!pred(user_data))
            {
                res.stop_reason = mb_run_stop_t::MB_RUN_TIMEOUT;
            }
        }
        return res;
    }

    void Motherboard::set_trace_callback(mb_trace_callback_t callback, void* user_data)
    {
        trace_callback_ = callback;
        trace_callback_data_ = user_data;
    }

    void Motherboard::pic_trigger_irq(uint8_t irq)
    {
        pic_.raise_irq(irq);
    }

    void Motherboard::run_frame()
    {
        // Один полный кадр (TICKS_PER_FRAME тиков) через общий цикл исполнения.
        // Поведение идентично прежней монолитной реализации: кадр всегда
        // закрывается (VBLANK-конец, IRQ4, звук, мышь, учёт тиков).
        execute_loop(nullptr, nullptr, TICKS_PER_FRAME, 0);
    }

const int16_t* Motherboard::get_audio_frame_buffer() const
{
    return audio_.get_frame_buffer();
}

int Motherboard::get_audio_frame_buffer_size() const
{
    return audio_.get_frame_buffer_size();
}

void Motherboard::audio_set_mute(bool muted)
{
    audio_.set_mute(muted);
}

void Motherboard::audio_set_volume(uint8_t volume_percent)
{
    audio_.set_volume(volume_percent);
}

void Motherboard::audio_set_enabled(bool enabled)
{
    audio_.set_sound_enabled(enabled);
}

void Motherboard::set_video_callback(mb_video_frame_callback_t callback, void *user_data)
{
    video_callback_ = callback;
    video_callback_data_ = user_data;
}

void Motherboard::set_interrupt_callback(mb_interrupt_callback_t callback, void *user_data)
{
    interrupt_callback_ = callback;
    interrupt_callback_data_ = user_data;
}

// T4.4 / Критерий приёмки №3: сигнатуры callback'ов должны быть
// идентичны в korvet_core и gui_qt5. Здесь мы просто пробрасываем
// GUI-callback в Logger. Никаких адаптеров-обёрток с потерей
// user_data — компилятор проверит совместимость через using-объявления
// в motherboard_types.hpp.
void Motherboard::set_log_callback(mb_log_callback_t callback, void *user_data)
{
    log_callback_ = callback;
    log_callback_data_ = user_data;
    Logger::get_instance().set_log_callback(callback, user_data);
}

void Motherboard::keyboard_key_press(uint16_t key_code)
{
    keyboard_.key_press(key_code);
}

void Motherboard::keyboard_key_release(uint16_t key_code)
{
    keyboard_.key_release(key_code);
}

bool Motherboard::get_keyboard_language_layout() const
{
    // Тот же механизм, что и в эталоне (_main.c/keyboard.c):
    //   KBD_LED = RAM[0xF72E] ^ (KEYBOARD_Read(0x80) & 2);
    //   true (QWERTY) при KBD_LED == 2.
    // RAM[0xf72e] в эталоне — прямой доступ к физическому массиву памяти,
    // в обход маппера страниц (это не чтение шиной CPU, а сам эмулятор
    // подглядывает состояние). KEYBOARD_Read(0x80) — тоже прямой вызов
    // компонента клавиатуры с "сырым" адресом 0x80, а не чтение через
    // шину. Повторяем оба нюанса буквально.
    uint8_t ram_flag = memory_.get_ram()[0xF72E];
    uint8_t port_bit = keyboard_.read(0x80) & 0x02;
    uint8_t kbd_led = ram_flag ^ port_bit;
    return kbd_led == 2;
}

void Motherboard::mouse_move(int16_t dx, int16_t dy)
{
    mouse_.move(dx, dy);
}

void Motherboard::mouse_set_buttons(uint8_t buttons)
{
    mouse_.set_buttons(buttons);
}

void Motherboard::joystick_set_axis(uint8_t axis, uint8_t value)
{
    (void)axis;
    (void)value;
}

void Motherboard::joystick_set_button(uint8_t button, bool pressed)
{
    joystick_.set_button(button, pressed);
}

void Motherboard::fdd_set_image_adapter(IFddImageAdapter *adapter)
{
    fdc_.set_image_adapter(adapter);
}

bool Motherboard::fdd_is_disk_inserted(uint8_t drive) const
{
    return fdc_.is_disk_inserted(drive);
}

void Motherboard::fdd_set_write_protect(uint8_t drive, bool read_only)
{
    fdc_.set_write_protect(drive, read_only);
}

Fdc::FdcStatus Motherboard::fdd_get_status() const
{
    return fdc_.get_status();
}

void Motherboard::printer_set_output_callback(PrinterByteCallback cb)
{
    printer_.set_output_callback(std::move(cb));
}

void Motherboard::serial_set_byte_out_callback(SerialByteOutCallback cb)
{
    serial_.set_byte_out_callback(std::move(cb));
}

void Motherboard::serial_inject_byte(uint8_t byte)
{
    serial_.process_rs232_input(byte);
}

uint8_t Motherboard::serial_read_status()
{
    return serial_.read(0x11);
}

void Motherboard::lan_set_byte_out_callback(LanByteOutCallback cb)
{
    lan_.set_byte_out_callback(std::move(cb));
}

void Motherboard::lan_inject_byte(uint8_t byte)
{
    lan_.process_input(byte);
}

bool Motherboard::tape_insert_buffer(const uint8_t *data, size_t size)
{
    return tape_.set_data(data, size);
}

bool Motherboard::tape_eject()
{
    tape_.eject();
    return true;
}

void Motherboard::load_data(const uint8_t *rom, size_t rom_size,
                            const uint8_t *mapper, size_t mapper_size,
                            const uint8_t *font, size_t font_size)
{
    memory_.set_rom(rom, rom_size);
    memory_.set_mapper(mapper, mapper_size);
    video_.set_font(font, font_size);
}

void Motherboard::cpu_set_breakpoint(uint16_t addr, bool enabled)
{
    cpu_.set_breakpoint(addr, enabled);
    memory_.set_breakpoint(addr, enabled);
}

void Motherboard::disassemble(uint16_t addr, char *buffer, int size)
{
    // RAM напрямую в трёхаргументную перегрузку (двухаргументная без памяти)
    disasm_result_t result;
    disasm_.disassemble(addr, memory_.get_ram(), &result);
    snprintf(buffer, static_cast<size_t>(size), "%s", result.mnemonic.c_str());
}

void Motherboard::init_components()
{
    // Инициализация компонентов с зависимостями
    cpu_.init(&memory_, &pic_);
    lan_.init(); // Инициализация компонента LAN
}

void Motherboard::connect_components()
{
    // Соединение компонентов
    ppi_.set_video(&video_);
    ppi_.set_fdc(&fdc_);
    ppi_.set_timer(&timer_);
    ppi_.set_frame_cycles_getter([this]()
                                 { return static_cast<uint32_t>(cpu_.get_frame_cycles()); });

    // Цепочка прерываний
    cpu_.set_pic(&pic_);

    // Таймер -> CPU и PIC
    timer_.set_pic(&pic_);

    // Serial + мышь (генерация пакетов мыши)
    serial_.set_pic(&pic_);
    mouse_.set_serial(&serial_);

    // FDC -> PIC (IRQ6, данные готовы)
    fdc_.set_pic(&pic_);

    // Память -> компоненты (доступ к периферии)
    memory_.set_cpu(&cpu_);
    memory_.set_video(&video_);
    memory_.set_fdc(&fdc_);
    memory_.set_ppi(&ppi_);
    memory_.set_serial(&serial_);
    memory_.set_lan(&lan_);
    memory_.set_timer(&timer_);
    memory_.set_keyboard(&keyboard_);
    memory_.set_pic(&pic_);

    // Глобальные экземпляры
    printer_instance = &printer_;
    mouse_instance = &mouse_;
    joystick_instance = &joystick_;
    serial_instance = &serial_;
    lan_instance = &lan_; // Подключение экземпляра LAN
}

// Конфигурация теперь выполняется собственными методами компонентов
// по enum-режимам из структуры конфигурации
// (раньше в init() были вызовы configure_rs232_mode/
//  configure_lan_mode — убраны вместе с заглушками транспорта)
// lan_.configure_lan_mode(static_cast<Lan::LanMode>(config->lan_mode), config->lan_addr);

mb_diag_result_t Motherboard::run_diagnostic(mb_diag_category_t category)
{
    mb_diag_result_t r = {};
    r.category = category;
    r.result = mb_diag_result_code_t::MB_DIAG_SKIP;
    r.message = "Not implemented";
    return r;
}

int Motherboard::run_all_diagnostics(mb_diag_result_t *results, int max_results)
{
    (void)results;
    (void)max_results;
    return 0;
}

int Motherboard::run_category_diagnostics(mb_diag_category_t category, mb_diag_result_t *results, int max_results)
{
    (void)category;
    (void)results;
    (void)max_results;
    return 0;
}

void Motherboard::print_diagnostic_results(const mb_diag_result_t *results, int count)
{
    (void)results;
    (void)count;
}

int Motherboard::get_diagnostic_test_count(mb_diag_category_t category)
{
    (void)category;
    return 0;
}

int Motherboard::get_total_diagnostic_test_count()
{
    return 0;
}

} // namespace korvet
