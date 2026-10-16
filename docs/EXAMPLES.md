# Примеры интеграции korvet_core

Все примеры — **реальный рабочий код** из состава korvet_core:
консольный инструмент проверки `korvet_core/minigui/` (CLI + опция
SDL2). Порядок — «от запуска до экзотики». Каждый пример указывает
файл с полным кодом.

Оглавление:
1. [Инициализация и загрузка ПЗУ](#1-инициализация-и-загрузка-пзу)
2. [Рендер звука](#2-рендер-звука)
3. [Рендер видео и скриншоты](#3-рендер-видео-и-скриншоты)
4. [Работа с дисками (KDI)](#4-работа-с-дисками-kdi)
5. [Клавиатура: ввод по сценарию](#5-клавиатура-ввод-по-сценарию)
6. [Цикл эмуляции](#6-цикл-эмуляции)
7. [Кассета, ИРПС, ЛВС, принтер](#7-кассета-ирпс-лвс-принтер)
8. [Трассировка исполнения](#8-трассировка-исполнения)

---

## 1. Инициализация и загрузка ПЗУ

Файлы (ПЗУ, шрифт, mapper, образы дискет) читает **хост** — ядро
получает только буферы. `korvet_core/minigui/main.cpp`:

```cpp
// Чтение файла целиком — забота хоста
auto rom = read_file(args.rom.c_str());
auto mapper = read_file(args.mapper.c_str());
auto font = read_file(args.font.c_str());
if (rom.size() > 24 * 1024) {
    rom.resize(24 * 1024);  // ПЗУ 8–24 КБ; хвост-мусор срезаем
}

auto& mb = korvet::Motherboard::get_instance();
korvet::mb_config_t cfg = {};          // разумные дефолты
cfg.fdc_enabled = true;
cfg.gzu_pages = 4;
cfg.lan_addr = 1;                      // LANADR: адрес РМУ
cfg.sound_enabled = true;              // хостовой выход звука
if (!mb.init(&cfg)) { /* …ошибка… */ }

// Ядру отдаются только буферы
mb.load_data(rom.data(), rom.size(),
             mapper.data(), mapper.size(),
             font.data(), font.size());   // шрифт 8192 = ДВА набора

// Дискеты: единственный источник данных — адаптер (см. §4)
mb.fdd_set_image_adapter(adapter.get());

mb.power_on();
```

Конфигурация `mb_config_t{}` пригодна к использованию сразу; ключевые
поля — `timer_ch2_clock` (источник канала 2 таймера), `lan_addr`,
`kbd_lang_flag_addr` — см. [MOTHERBOARD_API.md §1](MOTHERBOARD_API.md).

## 2. Рендер звука

Звук — синхронные кадровые буферы: 441 сэмпл (20 мс) на каждый
`run_frame()`, 22050 Гц, моно, int16. Не забрал кадр — он потерян;
копит хост. `korvet_core/minigui/wav_writer.hpp` + `main.cpp`:

```cpp
korvet::minigui::WavWriter wav;
wav.open("game.wav", korvet::Audio::SAMPLE_RATE);

for (uint64_t frame = 1; frame <= frames; frame++) {
    mb.run_frame();
    if (wav.is_open()) {
        wav.write_samples(mb.get_audio_frame_buffer(),
                          mb.get_audio_frame_buffer_size());
    }
}
wav.close();
```

Гейт динамика открывается записью гостя в PPI2-C (бит 3, SNDEN) —
конфигурация `sound_enabled`/`sound_volume` управляет только
масштабированием сэмплов хоста.

## 3. Рендер видео и скриншоты

Кадр — 512×256 байт, байт = индекс палитры 0..15. Рендер выполняется
по явному вызову хоста; конверсия в RGB — через LUT ядра
(`korvet_core/minigui/main.cpp`):

```cpp
mb.run_frame();
mb.render_video_frame();          // рендер текущего кадра

const uint8_t* frame = mb.readout_video();       // индексы 0..15
const korvet::RgbColor* lut = mb.readout_lut_rgb(); // 16 × {r,g,b}

std::vector<uint8_t> rgb(512 * 256 * 3);
for (int i = 0; i < 512 * 256; i++) {
    uint8_t c = frame[i] & 0x0F;
    rgb[i*3+0] = lut[c].r;
    rgb[i*3+1] = lut[c].g;
    rgb[i*3+2] = lut[c].b;
}
// PNG — минимальный энкодер minigui/png_writer.hpp (zlib)
korvet::minigui::write_png_rgb("shot.png", rgb.data(), 512, 256);
```

GUI с непрерывным выводом: `video_.get_front_framebuffer()` — front-кадр
через `shared_ptr` (zero-copy), хост держит указатель, пока копирует.

## 4. Работа с дисками (KDI)

Ядро не открывает файлы: сектора запрашиваются через
`IFddImageAdapter`. Готовая реализация поверх `std::FILE` —
`korvet_core/minigui/fdd_adapter.hpp`:

```cpp
class FileFddAdapter : public korvet::IFddImageAdapter {
public:
    bool insert(uint8_t drive, const char* path, bool write_protect = false);
    bool is_present(uint8_t drive) const override;
    bool is_write_protected(uint8_t drive) const override;
    bool read_sector(uint8_t drive, uint8_t track, uint8_t side,
                     uint8_t sector, uint8_t* buf, size_t n) override;
    bool write_sector(uint8_t drive, uint8_t track, uint8_t side,
                      uint8_t sector, const uint8_t* buf, size_t n) override;
};

auto adapter = std::make_unique<korvet::minigui::FileFddAdapter>();
adapter->insert(0, "data/kdi/disk__system.kdi");
mb.fdd_set_image_adapter(adapter.get());
```

**Контракт сектора: ядро передаёт НУЛЕВОЙ номер (0..4)** — оно само
вычитает единицу из регистра сектора ВГ93. Раскладка КДИ-файла:

```
позиция = track*10*1024 + side*5*1024 + sector*1024
(80 дорожек × 2 стороны × 5 секторов × 1024 байта = 819 200 Б)
```

На RP2040 тот же интерфейс реализуется поверх FatFs/SD — ядро не
меняется. Мотор: одновибратор MON 3 с (общая линия, IRQ7); статус
для GUI — `fdd_get_status().motors_on[]`, LED-активность —
`frames_since_last_access`.

## 5. Клавиатура: ввод по сценарию

Ядро знает только физическую матрицу: `key_press((row<<8)|col)`,
col 0..12 (12 = ПЧТ). Таблица «хост-клавиша → позиция» — на хосте;
калиброванная по ПЗУ 2.0 таблица — `korvet_core/minigui/kbd_map.hpp`
(латиница, эмпирическая; см. [KEYBOARD.md](KEYBOARD.md)):

```cpp
// Посимвольный ввод текста (12 кадров нажата + 4 пауза):
korvet::minigui::KeyCoord kc;
if (korvet::minigui::lookup_key('P', kc)) {
    mb.keyboard_key_press((kc.row << 8) | kc.col);
    for (int k = 0; k < 12; k++) mb.run_frame();
    mb.keyboard_key_release((kc.row << 8) | kc.col);
    for (int k = 0; k < 4; k++) mb.run_frame();
}

// Именованные клавиши (ВК, F1..F10, АЛФ, СТОП, ПЧТ):
uint16_t code;
if (korvet::minigui::lookup_named_key("ENTER", code))
    mb.keyboard_key_press(code);
```

Раскладка зависит от версии ПЗУ: линия KB05 в ПЗУ 2.0 фактически
`8 9 : ; , - . /` (в таблицах документации — `8 9 * + < = > /`).
Индикация ЛАТ/РУС — `get_keyboard_language_layout()` (адрес флага
конфигурируется `kbd_lang_flag_addr`).

## 6. Цикл эмуляции

CLI-харнесс `main.cpp`: кадровый цикл с вводом по сценарию:

```cpp
for (uint64_t frame = 1; frame <= frames; frame++) {
    // 1) Ввод по сценарию (клавиши/мышь/джойстик на кадре N)
    apply_script(mb, frame);

    // 2) Один кадр эмуляции (50 000 тактов)
    mb.run_frame();

    // 3) Рендер + звук кадра
    mb.render_video_frame();
    wav.write_samples(mb.get_audio_frame_buffer(), 441);

    // 4) Скриншоты по расписанию
    if (shot_due(frame)) save_screenshot(mb, path);
}
```

Реальное время (50 кадров/с) — забота хоста: SDL-вариант
(`minigui/sdl_window.cpp`) синхронизируется `SDL_Delay(20)`.
Точное управление — `run_ticks/run_opcodes/run_while`
(см. [MOTHERBOARD_API.md §2](MOTHERBOARD_API.md)).

## 7. Кассета, ИРПС, ЛВС, принтер

Периферийные потоки — побайтовые/побитовые колбэки (ядро не знает
транспортов):

```cpp
// Принтер: готовые байты (инверсия LSTINV уже применена)
mb.printer_set_enabled(true);
mb.printer_set_output_callback([](uint8_t b) {
    fwrite(&b, 1, 1, printer_file);
});

// ИРПС: переданное гостем наружу / байт «с провода»
mb.serial_set_byte_out_callback([&](uint8_t b) { serial_log.push_back(b); });
mb.serial_inject_byte(0x40);

// ЛВС: общая линия — каждый байт доставляется ВСЕМ станциям;
// многоадресный транспорт (TCP/RS485) строит хост
mb.lan_set_byte_out_callback([&](uint8_t b) { multicast(b); });
mb.lan_inject_byte(b);

// Кассета: полупериоды в тактах CPU (1 с = 2 500 000 тактов)
std::vector<uint32_t> periods = {600, 600, 600, 600};
mb.tape_set_input(0, periods.data(), periods.size());
mb.tape_set_out_callback([](uint8_t level, uint64_t cycle) {
    // События записи: уровень 0/1/2 (SETLOW/SETMID/SETHIGH) в такте N
});
```

Мышь/джойстик: `mouse_set_mode(MOUSE_SERIAL / MOUSE_PARALLEL / MOUSE_OFF)`
выбирает интерфейс (ИРПС или ММ8031 через ЭУ8040); ввод — `mouse_move()`,
`mouse_set_buttons()`, `joystick_set_connected()`,
`joystick_set_direction()`.

## 8. Трассировка исполнения

Колбэк перед каждой инструкцией; буфер/файл — на стороне хоста
(формат KVTR v1 — [TRACE_FORMAT.md](TRACE_FORMAT.md)):

```cpp
mb.set_trace_callback([](const korvet::mb_trace_record_t* r, void*) {
    // r->cycles, r->pc, r->opcode_bytes[3], регистры, SP, IFF,
    // IRR/ISR/IMR, CE трёх каналов таймера, выходы каналов
    trace_file.write(r, sizeof(*r));
}, nullptr);
```

`peek()` без побочных эффектов делает снимок опкода безопасным для
страниц портов. Детектор зависаний (тот же цикл) — WARN/ERROR в лог
при 10k/100k инструкций на одном PC.
