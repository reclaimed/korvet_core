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
| `bool init(const mb_config_t* config)` | Инициализация. Повторный вызов при живом экземпляре вернёт `false`. Конфиг сохраняется — используется при `connect_components()`. |
| `void shutdown()` | Деинициализация: снимает лог-колбэк, гасит питание. |
| `void power_on()` | «Включить питание»: сброс всех компонентов, счётчик тиков с нуля, пауза снимается. |
| `void power_off()` | «Выключить»: чистое состояние, пауза тоже снимается (вкл/выкл не наследует паузу отладчика). |
| `void cold_reset()` | Холодный сброс = `power_off() + power_on()`. |
| `void warm_reset()` | Горячий сброс: только CPU и PIC. |
| `void pause() / resume()` | Пауза эмуляции: `run_*` мгновенно вернёт `MB_RUN_PAUSED`. |
| `bool is_powered() const` / `bool is_paused() const` | Состояние. |

`mb_config_t` — плоская структура с дефолтами
(`mb_config_t{}` пригоден к использованию). Ключевые поля: `fdc_enabled`,
`gzu_pages` (1/4), `sound_enabled`, `sound_volume`, `log_level`
(0=ERROR…3=DEBUG), `lut_r/g/b_scale` (яркость каналов 0–63 → 0–255).
Поля путей (`rom_path`, `disk_paths[]`…) используются только тестами —
ядро их не читает; хост сам грузит файлы через `load_data()`.

## 2. Исполнение (все тайминги — в тактах CPU)

| Метод | Описание |
|-------|----------|
| `void step()` | Одна инструкция (отладчик). Исполняется и при паузе; частичный кадр закрывается без конца кадра. |
| `void run(uint32_t)` | Legacy-псевдоним `run_frame()`. |
| `void run_frame()` | Один полный кадр (50 000 тактов): VBLANK, IRQ4, звук, мышь. |
| `mb_run_result_t run_ticks(uint64_t ticks)` | Не менее `ticks` тактов, целыми инструкциями. Позиция кадра сохраняется между вызовами. |
| `mb_run_result_t run_opcodes(uint64_t count)` | Ровно `count` инструкций (подтверждения прерываний входят в счётчик). |
| `mb_run_result_t run_while(mb_exec_predicate_t pred, void* user_data, uint64_t timeout_ticks)` | Пока предикат не вернёт `true` (проверяется до каждого шага) либо не исчерпан таймаут. Предикат обязан быть чистым. |
| `uint64_t get_current_cycles() const` | Полный счётчик тиков «прямо сейчас»: закрытые кадры + текущий частичный. |
| `uint64_t get_total_executed_cycles() const` | Счётчик закрытых кадров (используют GUI-снапшоты). |
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
| `void pic_trigger_irq(uint8_t irq)` | Программный запрос IRQ 0..7 (как от устройства). CPU примет при IFF=1. |

## 4. Видео

| Метод | Описание |
|-------|----------|
| `uint8_t* readout_video()` | Back-буфер кадров (512×256 байт, 1 байт = LUT-индекс) — legacy API. |
| `std::shared_ptr<const uint8_t[]> video_.get_front_framebuffer()` | **Рекомендуемый путь:** front-кадр через shared_ptr (zero-copy, без гонок). |
| `void video_.swap_frame_buffers()` | Переключение front/back после `video_.render_frame(...)`. |
| `void video_.render_frame(uint8_t*)` | Рендер кадра в back-буфер (dirty-флаги: перерисовка только изменённого). |
| `const RgbColor* readout_lut_rgb()` | Палитра: 16 × {r,g,b} (после масштабирования яркости). |
| `mb_video_state_t readout_video_state()` | Снимок состояния ГЗУ/АЦЗУ (страницы, режимы). |
| `uint8_t readout_sysreg() / readout_vireg() / readout_ncreg()` | Регистры: системный (банк памяти), VIREG, NCREG. |
| `uint8_t readout_gzu_page_access() / readout_gzu_page_show()` | Страница ГЗУ для CPU / для отображения. |
| `uint8_t readout_aczu_attr_write() / bool readout_aczu_wide_mode() / readout_aczu_second_font()` | Режимы АЦЗУ. |
| `uint8_t readout_video_line()` | Текущая строка развёртки (0–255). |
| Отладочные: `debug_write_sysreg(v)`, `debug_write_vireg(v)`, `debug_pic_write(port,v)`, `debug_timer_write(port,v)` | Прямая запись в регистры из отладчика. |
| Дампы: `readout_aczu()/_size`, `readout_gzu()/_size`, `readout_lut()/_size`, `readout_rom()/_size`, `readout_mapper()/_size`, `readout_ram()` | Сырые массивы для дампов/инструментов. |

Формат кадра: 512×256, байт = индекс палитры 0..15. «Широкая» палитра —
`set_lut_scale()` (по умолчанию 0–63 → 0–255).

## 5. Звук

| Метод | Описание |
|-------|----------|
| `const int16_t* get_audio_frame_buffer() const` | Готовый звук кадра: 441 сэмпл, 22050 Гц, моно, int16. Заполняется в конце каждого кадра, полностью перезаписывает предыдущий. |
| `int get_audio_frame_buffer_size() const` | Всегда `Audio::FRAME_SAMPLES` (441). |
| `void audio_set_mute(bool)` / `audio_set_volume(uint8_t %)` / `audio_set_enabled(bool)` | Управление выходом. |

Модель синхронная, без очередей: не забрал кадр до следующего
`run_frame()` — звук этого кадра потерян (реплика `MakeSound()/SOUNDBUF`
эталона). Хост сам копит/микширует (см. EXAMPLES.md §2).

## 6. Ввод

| Метод | Описание |
|-------|----------|
| `void keyboard_key_press(uint16_t key_code)` / `keyboard_key_release(...)` | Матричный код `(row<<8)|col`. Трансляция host-клавиш — целиком на хосте. |
| `bool get_keyboard_language_layout() const` | Активная раскладка в госте: `true` = латиница/QWERTY. Читает тот же байт RAM+порта, что и BIOS Корвета. |
| `void mouse_move(int16_t dx, int16_t dy)` / `mouse_set_buttons(uint8_t mask)` | Мышь (дельты, кнопки). |
| `void joystick_set_axis(uint8_t, uint8_t)` / `joystick_set_button(uint8_t, bool)` | Джойстик. |

## 7. Дискеты (ВГ93)

| Метод | Описание |
|-------|----------|
| `void fdd_set_image_adapter(IFddImageAdapter* adapter)` | Подключить источник образов — ЕДИНСТВЕННЫЙ способ дать ядру диск. |
| `bool fdd_is_disk_inserted(uint8_t drive) const` | Есть ли носитель (спрашивает адаптер). |
| `void fdd_set_write_protect(uint8_t drive, bool read_only)` | Принудительная защита записи (дополнительно к правам файла). |
| `Fdc::FdcStatus fdd_get_status() const` | Статус 4 приводов: шторка, мотор, трек/сектор, чтение/запись, защита. Для GUI-индикаторов. |

`IFddImageAdapter` (`internal/fdc/fdc_image_adapter.hpp`) — 4 метода:
`is_present`, `is_write_protected`, `read_sector`, `write_sector`.
Геометрия KDI: см. глоссарий [OVERVIEW.md](OVERVIEW.md#дисковод).

### Отложенные операции (реалистичные тайминги)

| Метод | Описание |
|-------|----------|
| `void fdc_set_deferred_ops(bool)` | Включить режим: команда ВГ93 не исполняется мгновенно, а формирует дескриптор + BUSY. По умолчанию ВЫКЛ (эталонное поведение). |
| `bool fdc_has_pending_op() const` | Есть незакрытая операция. |
| `fdc_pending_op_t fdc_get_pending_op() const` | Дескриптор (тип, приводы, дорожки, сектор, multi-sector). |
| `fdc_op_result_t fdc_complete_pending_op()` | Закрыть операцию: исполнить команду, обновить регистры. Возвращает OK / WRITE_PROTECT / NOT_FOUND / CRC_ERROR / NONE. |
| `void fdc_set_pending_op_callback(FdcPendingOpCallback)` | Уведомление о создании операции (звук, LED, таймер — на хосте). Вызывается синхронно из потока эмуляции. |

Подробности и сценарии — [FDC_PENDINGOP.md](FDC_PENDINGOP.md).

## 8. Порты: принтер, ИРПС, LAN, кассета

| Метод | Описание |
|-------|----------|
| `void printer_set_output_callback(PrinterByteCallback)` | Приёмник готовых байтов принтера (хост пишет в TXT/PDF/физический принтер). |
| `void printer_set_enabled(bool)` / `printer_is_enabled()` | Вкл/выкл эмуляции принтера («не подключён»). |
| `void serial_set_byte_out_callback(SerialByteOutCallback)` | Байты, которые гость передаёт по ИРПС. Транспорт (TCP/RS485/ttyUSB) — хост. |
| `void serial_inject_byte(uint8_t)` | Байт «с провода» в приёмный буфер Корвета. |
| `uint8_t serial_read_status()` | Статусный регистр УСАПП (0xFB11): TxRDY/RxRDY/TxEMPTY/OE/DSR. |
| `uint32_t serial_rx_count() / serial_tx_count()` | Счётчики активности ИРПС (индикаторы GUI). |
| `void lan_set_byte_out_callback(LanByteOutCallback)` / `lan_inject_byte(uint8_t)` | Симметрично для LAN-порта. |
| `bool tape_insert_buffer(const uint8_t*, size_t)` / `bool tape_eject()` | Кассета из буфера (файл читает хост). |

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
| `mb_log_callback_t` | Сообщения лога (ERROR/WARNING/INFO/DEBUG + подсистема). Устанавливается через `set_log_callback()` — пробрасывается в синглтон `Logger`. |
| `mb_video_frame_callback_t` | Зарезервировано (кадры отдаются через `get_front_framebuffer()`). |
| `mb_interrupt_callback_t` | Зарезервировано (прерывания наблюдаются через трассу/PIC). |
| `mb_trace_callback_t` | Запись трассы перед каждой инструкцией. |

## 12. Загрузка данных

```cpp
void load_data(const uint8_t* rom, size_t rom_size,
               const uint8_t* mapper, size_t mapper_size,
               const uint8_t* font, size_t font_size);
```

Хост читает файлы (ПЗУ 8–24 КБ, mapper 32×256, шрифт 8 КБ) и передаёт
буферы. Образы ПЗУ можно менять «на ходу»: `memory_.get_rom_writable()`
(горячая замена ROM — см. EXAMPLES.md §7). Запись системного регистра
для отладки — `debug_write_sysreg()`.
