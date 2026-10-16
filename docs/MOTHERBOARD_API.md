# Motherboard API — справочник фасада

`korvet::Motherboard` (singleton) — единственный публичный интерфейс
ядра. Всё, что не описано здесь — детали реализации (`internal/`).
Файл: `korvet_core/motherboard.hpp`, типы: `motherboard_types.hpp`.

```cpp
#include "korvet_core/motherboard.hpp"

auto& mb = korvet::Motherboard::get_instance();
```

> **Потокобезопасность:** все методы вызывает один поток (исключение —
> `Logger`). Колбэки приходят синхронно из цикла исполнения.

---

## 1. Жизненный цикл

| Метод | Описание |
|-------|----------|
| `bool init(const mb_config_t* config)` | Инициализация. Повторный вызов при живом экземпляре вернёт `false`. Конфиг сохраняется. |
| `void shutdown()` | Деинициализация: снимает лог-колбэк, гасит питание. |
| `void power_on()` | «Включить питание»: сброс всех компонентов, счётчик тиков с нуля, пауза снимается. |
| `void power_off()` | «Выключить»: чистое состояние, пауза тоже снимается (вкл/выкл не наследует паузу отладчика). |
| `void cold_reset()` | Холодный сброс = `power_off() + power_on()`. |
| `void warm_reset()` | Горячий сброс: только CPU и PIC. |
| `void pause() / resume()` | Пауза эмуляции: `run_*` мгновенно вернёт `MB_RUN_PAUSED`. |
| `bool is_powered() const` / `bool is_paused() const` | Состояние. |

`mb_config_t` — плоская структура со значениями по умолчанию
(`mb_config_t{}` пригоден к использованию). Ключевые поля:

| Поле | По умолчанию | Смысл |
|------|--------------|-------|
| `fdc_enabled` | false | узел НГМД |
| `gzu_pages` | — | 1 или 4 страницы ГЗУ |
| `lan_addr` | 1 | адрес РМУ ЛВС (LANADR, PPI1-A биты 4-7; 0 — групповой) |
| `timer_ch2_clock` | 0 (HBL) | источник канала 2 таймера: 0=HBL 65,6 мкс, 1=50 Гц, 2=2 МГц, 3=выкл (см. OVERVIEW.md §4) |
| `kbd_lang_flag_addr` | 0xF72E | адрес флага ЛАТ/РУС для `get_keyboard_language_layout()` (ПЗУ 2.0); 0xFFFF — хак выключен |
| `sound_enabled` | false | хостовой выход звука (гейт динамика — только бит 3 PPI2-C!) |
| `sound_volume` | 100 | громкость микшера (0-100) |
| `log_level`, `log_enabled` | — | лог (0=ERROR…3=DEBUG) |
| `lut_r/g/b_scale` | 255 | яркость каналов (0–63 → 0–255) |
| `lut_write_protect` | false | защита LUT (отладка) |

Поля путей (`rom_path`, `disk_paths[]`…) используются только тестами —
ядро их не читает; хост сам грузит файлы через `load_data()`.

## 2. Исполнение (все тайминги — в тактах CPU)

| Метод | Описание |
|-------|----------|
| `void step()` | Одна инструкция (отладчик). Исполняется и при паузе; частичный кадр закрывается без конца кадра. |
| `void run(uint32_t)` | Legacy-псевдоним `run_frame()`. |
| `void run_frame()` | Один полный кадр (50 000 тактов): IRQ4, звук, мышь, продвижение MON/таймаутов ВГ93. |
| `mb_run_result_t run_ticks(uint64_t ticks)` | Не менее `ticks` тактов, целыми инструкциями. Позиция кадра сохраняется между вызовами. |
| `mb_run_result_t run_opcodes(uint64_t count)` | Ровно `count` инструкций (подтверждения прерываний входят в счётчик). |
| `mb_run_result_t run_while(mb_exec_predicate_t pred, void* user_data, uint64_t timeout_ticks)` | Пока предикат не вернёт `true` (проверяется до каждого шага) либо не исчерпан таймаут. Предикат обязан быть чистым. |
| `uint64_t get_current_cycles() const` | Полный счётчик тиков «прямо сейчас»: закрытые кадры + текущий частичный. |
| `uint64_t get_total_executed_cycles() const` | Счётчик закрытых кадров. |
| `uint64_t get_frame_cycles() const` | Такты текущего (открытого) кадра — сбрасывается в начале кадра. |

```cpp
struct mb_run_result_t {
    uint64_t executed_ticks;         // фактически исполнено тактов
    uint64_t executed_instructions;  // фактически исполнено инструкций
    mb_run_stop_t stop_reason;       // COMPLETED / TIMEOUT / BREAKPOINT / HLT / PAUSED
};
```

`executed_ticks` может превысить запрошенный бюджет не более чем на
длину последней инструкции (4–17): останов всегда на границе инструкции.

## 3. Точки останова и прерывания

| Метод | Описание |
|-------|----------|
| `void set_breakpoint(uint16_t addr, bool enabled)` | Аппаратная точка останова CPU (и памяти). |
| `bool has_breakpoint(uint16_t addr) const` | Есть ли точка останова по адресу. |
| `bool was_breakpoint_hit() const` | Сработала ли при последнем исполнении. |
| `void cpu_set_breakpoint(...)` | Синоним для отладчика. |
| `void pic_trigger_irq(uint8_t irq)` | Программный запрос IRQ 0..7 (как от устройства; уровень 0 — разъём расширения). CPU примет при IFF=1. |

## 4. Видео

| Метод | Описание |
|-------|----------|
| `void render_video_frame()` | **Рендер текущего кадра** — вызывается хостом после `run_frame()` (ядро само не решает, когда кадр нужен GUI). После вызова кадр доступен через `readout_video()`. |
| `uint8_t* readout_video()` | Back-буфер кадра (512×256 байт, байт = LUT-индекс 0..15) — после `render_video_frame()`. |
| `std::shared_ptr<const uint8_t[]> video_.get_front_framebuffer()` | Zero-copy путь для GUI: front-кадр через shared_ptr. |
| `const RgbColor* readout_lut_rgb()` | Палитра: 16 × {r,g,b} (после масштабирования яркости). |
| `mb_video_state_t readout_video_state()` | Снимок состояния ГЗУ/АЦЗУ (страницы, режимы). |
| `uint8_t readout_sysreg() / readout_vireg() / readout_ncreg()` | Регистры: системный (банк памяти), VIREG, NCREG. |
| `uint8_t readout_gzu_page_access() / readout_gzu_page_show()` | Страница ГЗУ для CPU / для отображения. |
| `uint8_t readout_aczu_attr_write() / bool readout_aczu_wide_mode() / readout_aczu_second_font()` | Режимы АЦЗУ. |
| `uint8_t readout_video_line()` | Текущая строка развёртки (0–255). |
| Отладочные: `debug_write_sysreg(v)`, `debug_write_vireg(v)`, `debug_pic_write(port,v)`, `debug_timer_write(port,v)` | Прямая запись в регистры из отладчика. |
| Дампы: `readout_aczu()/_size`, `readout_gzu()/_size`, `readout_lut()/_size`, `readout_rom()/_size`, `readout_mapper()/_size`, `readout_ram()` | Сырые массивы для дампов/инструментов. |

Формат кадра: 512×256, байт = индекс палитры 0..15. Конверсия индексов
в RGB — `readout_lut_rgb()` (яркостная шкала 0–63 → 0–255
конфигурируется `lut_*_scale`).

## 5. Звук

| Метод | Описание |
|-------|----------|
| `const int16_t* get_audio_frame_buffer() const` | Готовый звук кадра: 441 сэмпл, 22050 Гц, моно, int16. Заполняется в конце каждого кадра, полностью перезаписывает предыдущий. |
| `int get_audio_frame_buffer_size() const` | Всегда `Audio::FRAME_SAMPLES` (441). |
| `void audio_set_mute(bool)` / `audio_set_volume(uint8_t %)` / `audio_set_enabled(bool)` | Управление хостовым выходом. Гейт динамика (бит 3 PPI2-C) ими НЕ управляется — только записью гостя. |

Модель синхронная, без очередей: не забрал кадр до следующего
`run_frame()` — звук этого кадра потерян. Хост сам копит/микширует
(пример — korvet_core/minigui/wav_writer.hpp).

## 6. Ввод

| Метод | Описание |
|-------|----------|
| `void keyboard_key_press(uint16_t key_code)` / `keyboard_key_release(...)` | Матричный код `(row<<8)|col`, col 0..12 (12 = ПЧТ). Трансляция host-клавиш — целиком на хосте (таблицы — [KEYBOARD.md](KEYBOARD.md)). |
| `bool get_keyboard_language_layout() const` | Активная раскладка в госте: `true` = латиница/QWERTY. Читает байт RAM по адресу `kbd_lang_flag_addr` + бит клавиши АЛФ — деталь конкретной версии ПЗУ (по умолчанию 0xF72E, ПЗУ 2.0). |
| `void mouse_set_mode(mb_mouse_mode_t)` / `mouse_get_mode()` | Режим мыши: `MOUSE_OFF` / `MOUSE_SERIAL` (ИРПС, Microsoft) / `MOUSE_PARALLEL` (ММ8031 через ЭУ8040). |
| `void mouse_move(int16_t dx, int16_t dy)` / `mouse_set_buttons(uint8_t mask)` | Ввод мыши (бит 0 — левая, бит 1 — правая). В serial-режиме — 3-байтовые посылки по событию; в parallel — счётчики ММ8031. |
| `void joystick_set_connected(bool)` | Подключение МД8030 (слот ХР2 ЭУ8040). |
| `void joystick_set_direction(joystick_direction_t, bool)` | Направления: JOY_UP/DOWN/LEFT/RIGHT. |
| `void joystick_set_button(uint8_t, bool)` | Кнопки «Пуск1»/«Пуск2». |

## 7. Дискеты (ВГ93)

| Метод | Описание |
|-------|----------|
| `void fdd_set_image_adapter(IFddImageAdapter* adapter)` | Подключить источник образов — ЕДИНСТВЕННЫЙ способ дать ядру диск. |
| `bool fdd_is_disk_inserted(uint8_t drive) const` | Есть ли носитель (спрашивает адаптер). |
| `void fdd_set_write_protect(uint8_t drive, bool read_only)` | Принудительная защита записи (дополнительно к правам файла). |
| `Fdc::FdcStatus fdd_get_status() const` | Статус: `motors_on[4]` — общая линия MON; `selected_drive`, треки, чтение/запись, защита, `frames_since_last_access` (LED). |

`IFddImageAdapter` (`internal/fdc/fdc_image_adapter.hpp`) — 4 метода:
`is_present`, `is_write_protected`, `read_sector`, `write_sector`.
**Контракт сектора: ядро передаёт НУЛЕВОЙ номер (0..4)** — оно само
вычитает единицу из регистра сектора ВГ93. Раскладка КДИ:
`track*10*1024 + side*5*1024 + sector*1024`. Готовая реализация поверх
`std::FILE` — `korvet_core/minigui/fdd_adapter.hpp`.

Мотор: перезапускаемый одновибратор MON на 3 с (запуск по
положительному перепаду бита 5 DRVREG; линия общая). По истечении —
IRQ7 (MOTINT). Разгон ~0,5 с — информационный флаг `Fdc::is_spinning_up()`.

### Отложенные операции (реалистичные тайминги)

| Метод | Описание |
|-------|----------|
| `void fdc_set_deferred_ops(bool)` | Включить режим: команда ВГ93 не исполняется мгновенно, а формирует дескриптор + BUSY. По умолчанию ВЫКЛ. |
| `bool fdc_has_pending_op() const` | Есть незакрытая операция. |
| `fdc_pending_op_t fdc_get_pending_op() const` | Дескриптор (тип, приводы, дорожки, сектор, multi-sector). |
| `fdc_op_result_t fdc_complete_pending_op()` | Закрыть операцию: исполнить команду, обновить регистры. Возвращает OK / WRITE_PROTECT / NOT_FOUND / CRC_ERROR / NONE. |
| `void fdc_set_pending_op_callback(FdcPendingOpCallback)` | Уведомление о создании операции (звук, LED, таймер — на хосте). Вызывается синхронно из потока эмуляции. |

Контракт: ровно один `complete_pending()` на каждый колбэк; при
потерянном колбэке ядро автозавершит операцию через 0,5 с (WARN).
FORCE INTERRUPT (0xD0) принимается и при BUSY — отменяет отложенную
операцию. Подробности — [FDC_PENDINGOP.md](FDC_PENDINGOP.md).

## 8. Порты: принтер, ИРПС, ЛВС, кассета

| Метод | Описание |
|-------|----------|
| `void printer_set_output_callback(PrinterByteCallback)` | Приёмник готовых байтов принтера (инверсия LSTINV уже применена; хост пишет в TXT/PDF/на физический принтер). |
| `void printer_set_enabled(bool)` / `printer_is_enabled()` | Вкл/выкл эмуляции принтера («не подключён»; байты в отключенном состоянии теряются с WARN в лог). |
| `void serial_set_byte_out_callback(SerialByteOutCallback)` | Байты, которые гость передаёт по ИРПС. Транспорт (TCP/RS485/ttyUSB) — хост. |
| `void serial_inject_byte(uint8_t)` | Байт «с провода» в приёмный буфер Корвета (RRDY + IRQ1 при включённом приёме). |
| `uint8_t serial_read_status()` | Статусный регистр УСАПП (0xFB11): TxRDY/RxRDY/TxEMPTY/OE/DSR. |
| `uint32_t serial_rx_count() / serial_tx_count()` | Счётчики активности ИРПС (индикаторы GUI). |
| `void lan_set_byte_out_callback(LanByteOutCallback)` / `lan_inject_byte(uint8_t)` | Симметрично для LAN-порта (RRDY + IRQ3). Семантика общей линии: каждый переданный байт доставляется всем станциям — многоадресный транспорт строит хост. |
| `void lan_set_address(uint8_t)` / `lan_get_address()` | Адрес РМУ (LANADR, 1-15). |

### Кассета (однобитовый канал по тактам CPU)

| Метод | Описание |
|-------|----------|
| `void tape_set_input(uint8_t initial_level, const uint32_t* half_period_cycles, size_t count)` | Вставить «кассету»: начальный уровень + полупериоды (такты CPU; 1 с = 2 500 000). |
| `void tape_input_append(const uint32_t*, size_t)` | Потоковое добавление (line-in в реальном времени). |
| `void tape_input_rewind()` | Перемотка в начало. |
| `bool tape_is_input_available() const` | Есть ли входные данные. |
| `void tape_set_out_callback(TapeOutCallback)` | События записи: `(уровень 0/1/2, такт CPU)`; уровни SETLOW/SETMID/SETHIGH по битам 0-1 PPI2-C. |
| `void tape_set_relay_callback(TapeRelayCallback)` | События реле «кратковременного стопа» (бит 2 PPI2-C). |
| `bool tape_is_stop_relay() const` / `uint64_t tape_get_position() const` | Состояние реле / виртуальная позиция ленты (такты, без пауз). |

## 9. Трассировка исполнения

| Метод | Описание |
|-------|----------|
| `void set_trace_callback(mb_trace_callback_t, void* user_data)` | Колбэк перед КАЖДОЙ инструкцией (и подтверждением прерывания). `nullptr` — выключить. |
| `bool is_trace_enabled() const` | Колбэк установлен? |

Запись `mb_trace_record_t` — 34 байта, little-endian: тики, PC, 3 байта
опкода (через `peek`, без побочных эффектов), все регистры, SP, IFF,
IRR/ISR/IMR, CE трёх каналов таймера, выходы каналов. Формат файла и
утилита разбора — [TRACE_FORMAT.md](TRACE_FORMAT.md). Ядро ничего не
хранит: буфер/файл/UART — на стороне хоста.

## 10. CPU, дизассемблер, диагностика

| Метод | Описание |
|-------|----------|
| `cpu_state_t cpu_get_state() const` / `void cpu_set_state(const cpu_state_t&)` | Полное состояние регистров (снапшоты/восстановление). |
| `uint16_t readout_pc() const` | Текущий PC. |
| `void disassemble(uint16_t addr, char* buffer, int size)` | Дизассемблирование одной инструкции. |
| `Disasm& get_disasm()` | Прямой доступ к дизассемблеру (классификация инструкций для отладчика). |
| `mb_diag_result_t run_diagnostic(mb_diag_category_t)` + семейство | Зарезервировано: возвращает `MB_DIAG_SKIP`. |

## 11. Колбэки уровня машины

| Тип | Когда вызывается |
|-----|------------------|
| `mb_log_callback_t` | Сообщения лога (ERROR/WARNING/INFO/DEBUG + подсистема). Устанавливается через `set_log_callback()`. |
| `mb_video_frame_callback_t` | Зарезервировано (кадры отдаются через `readout_video()`/`get_front_framebuffer()`). |
| `mb_interrupt_callback_t` | Зарезервировано (прерывания наблюдаются через трассу/PIC). |
| `mb_trace_callback_t` | Запись трассы перед каждой инструкцией. |

## 12. Загрузка данных

```cpp
void load_data(const uint8_t* rom, size_t rom_size,
               const uint8_t* mapper, size_t mapper_size,
               const uint8_t* font, size_t font_size);
```

Хост читает файлы и передаёт буферы:

- **ПЗУ** 8–24 КБ (образы версий 1.1/2.0; хвост больше 24 КБ обрезается);
- **mapper** 32×256 = 8192 байт (таблица страниц памяти);
- **шрифт** 8192 байт = ДВА знакогенератора (основной +
  псевдографика, выбор битом 2 VIREG).

Образы ПЗУ можно менять «на ходу»: `memory_.get_rom_writable()`
(горячая замена). Запись системного регистра для отладки —
`debug_write_sysreg()`.
