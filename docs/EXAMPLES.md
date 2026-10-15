# Примеры интеграции korvet_core

Все примеры — **реальный рабочий код** GUI-эмулятора korvet_qt6
(репозиторий `korvet_qt6_emulator`), с цитатами и указанием файлов.
Порядок — «от запуска до экзотики».

Оглавление:
1. [Инициализация и загрузка ПЗУ](#1-инициализация-и-загрузка-пзу)
2. [Рендер звука](#2-рендер-звука)
3. [Рендер видео](#3-рендер-видео)
4. [Работа с дисками (KDI)](#4-работа-с-дисками-kdi)
5. [Озвучивание дисковода (отложенные операции ВГ93)](#5-озвучивание-дисковода)
6. [Цикл эмуляции в реальном времени](#6-цикл-эмуляции-в-реальном-времени)
7. [Трассировка исполнения](#7-трассировка-исполнения)
8. [Клавиатура](#8-клавиатура)
9. [Принтер, ИРПС, LAN](#9-принтер-ирпс-lan)

---

## 1. Инициализация и загрузка ПЗУ

Файлы (ПЗУ, шрифт, mapper) читает **хост** — ядро получает только
буферы. `korvet_qt6/src/core/core_service.cpp`:

```cpp
// Загрузка rom/font/mapper — файлы читает хост, ядру отдаются буферы.
const struct { const char* what; QString path; } files[3] = {
    { "rom",    cfg.resolve_data_file(cfg.rom) },
    { "font",   cfg.resolve_data_file(cfg.font) },
    { "mapper", cfg.resolve_data_file(cfg.mapper) },
};
std::vector<uint8_t> rom, font, mapper;
for (const auto& f : files) {
    QFile file(f.path);
    if (!file.open(QIODevice::ReadOnly)) { /* …ошибка… */ }
    const QByteArray data = file.readAll();
    if (f.what == QStringLiteral("rom")) {
        rom.assign(data.constBegin(), data.constEnd());
    } else if (f.what == QStringLiteral("font")) {
        font.assign(data.constBegin(), data.constEnd());
    } else {
        mapper.assign(data.constBegin(), data.constEnd());
    }
}

mb_->load_data(rom.data(), rom.size(),
               mapper.data(), mapper.size(),
               font.data(), font.size());

// Дискеты: единственный источник данных — адаптер (файлы на хосте).
mb_->fdd_set_image_adapter(&fdd_adapter_);
```

До этого — конфигурация и включение питания:

```cpp
mb_ = &korvet::Motherboard::get_instance();
mb_config_ = korvet::mb_config_t{};          // дефолты
mb_config_.fdc_enabled = cfg.fdc_enabled;
mb_config_.gzu_pages   = static_cast<uint8_t>(cfg.gzu_pages);
mb_->init(&mb_config_);
// … load_data() …
mb_->power_on();
```

## 2. Рендер звука

Ядро отдаёт **готовый кадр звука** (441 сэмпл / 20 мс / 22050 Гц / моно
/ int16) в конце каждого кадра эмуляции. Хост копирует его в свой
аудиостек. `core_service.cpp`, метод `run_one_frame()`:

```cpp
const int16_t* samples = mb_->get_audio_frame_buffer();
const int n = mb_->get_audio_frame_buffer_size();
if (samples && n > 0) {
    audio = QByteArray(reinterpret_cast<const char*>(samples),
                       n * int(sizeof(int16_t)));
}
```

Дальше кадры уходит в `SoundMixer` — единственный аудио-вывод
приложения (выделенный аудио-поток, один `QAudioSink`, голоса:
бипер + WAV-эффекты + процедурные). Ввод бипера:

```cpp
// audio/sound_mixer.h — голос бипера Корвета
// Бипер: сэмплы 22050 Гц mono Int16 (кадровые пачки ядра).
void beeper_write(const QByteArray& samples);
```

Типовая связка в GUI-потоке: `CoreService` эмитит сигнал с `audio`
(QByteArray кадра), аудио-воркер микшера вызывает `beeper_write()`.
Обратите внимание: ядро не хранит очередь — кадр, который хост не забрал
до следующего `run_frame()`, теряется. Это сознательная реплика модели
`MakeSound()/SOUNDBUF` эталона: темп звука обязан совпадать с темпом
кадров эмуляции.

## 3. Рендер видео

Конвейер: ядро рендерит в back-буфер → переключает буферы → GUI берёт
front через `shared_ptr` (zero-copy, без гонок: эмуляция пишет новый
back, пока GUI держит старый front). `core_service.cpp`:

```cpp
if (full_frame) {
    // Рендер кадра: back → front (zero-copy shared_ptr для GUI).
    mb_->video_.render_frame(mb_->video_.get_framebuffer());
    mb_->video_.swap_frame_buffers();
}
// …
fb  = mb_->video_.get_front_framebuffer();   // shared_ptr<const uint8_t[]>
lut = mb_->readout_lut_rgb();                // 16 × RgbColor
```

Кадр — 512×256 байт, байт = индекс палитры 0..15. Превращение в
изображение — `ScreenRenderer` (`korvet_qt6/src/video/screen_renderer.h`),
документированная шапка класса:

```cpp
// Вход: 512x256, 1 байт на пиксель = индекс в LUT (16 цветов), LUT 16xRGB.
// КОНВЕЙЕР (v1.2):
//   Стадия 1 (512x256): индексы → RGB32 (цветность: цветной/чб/янтарный/
//     зелёный/голубой CRT + насыщенность 0..200%).
//   Стадия 2 (512x256): CRT-артефакты —
//     - послесвечение фосфора (persistence 0..100%): кадр подмешивается
//       к предыдущему, яркие точки продолжают светиться;
//     - мягкость фокуса (blur 0..100%): горизонтальная расфокусировка;
//     - ореол (glow 0..100%): яркие участки «расплываются» (bloom).
//   Стадия 3 (только 4:3): 512x256 → 1024x512 → после КАЖДОЙ 2-строки
//     вставляется 1-строка с пониженной яркостью (scanline) → 1024x768.
```

«Полный кадр» GUI определяет по приросту счётчика закрытых кадров —
например, после HLT или точки останова кадр может оборваться раньше:

```cpp
const uint64_t cycles_before = mb_->get_total_executed_cycles();
mb_->run_frame();
full_frame = (mb_->get_total_executed_cycles() - cycles_before)
             >= Motherboard::TICKS_PER_FRAME;
```

## 4. Работа с дисками (KDI)

Ядро никогда не открывает файлы образов: доступ — через реализацию
интерфейса `IFddImageAdapter`. В korvet_qt6 это `QFileFddAdapter`
(`korvet_qt6/src/core/fdd_qfile_adapter.h`) — файл открывается лениво
на каждый запрос сектора (тот же контракт, что чтение с SD-карты на
RP2040):

```cpp
// QFileFddAdapter — IFddImageAdapter поверх QFile (хостовая сторона).
//
// Fdc (korvet_core) запрашивает секторы через интерфейс; сам файл открывается
// лениво по каждому read/write 1024 байта — тот же путь, что на RP2040
// (там сектора читаются напрямую с SD). Образ НЕ грузится в память.
//
// Геометрия KDI: 80 дорожек x 2 стороны x 5 секторов x 1024 байта = 819200.
// «Логический цилиндр» = side0[1..5], затем side1[1..5].

class QFileFddAdapter : public korvet::IFddImageAdapter {
public:
    bool insert(uint8_t drive, const QString& path, bool read_only = false);
    void eject(uint8_t drive);

    bool is_present(uint8_t drive) const override;
    bool is_write_protected(uint8_t drive) const override;
    bool read_sector(uint8_t drive, uint8_t track, uint8_t side,
                     uint8_t sector, uint8_t* buf, size_t buf_size) override;
    bool write_sector(uint8_t drive, uint8_t track, uint8_t side,
                      uint8_t sector, const uint8_t* buf, size_t buf_size) override;

    static qint64 offset(uint8_t track, uint8_t side, uint8_t sector) {
        return static_cast<qint64>(track) * 10 * SECTOR_SIZE
             + (side ? 5 * SECTOR_SIZE : 0)
             + static_cast<qint64>(sector) * SECTOR_SIZE;
    }
};
```

«Вставка дискеты» из GUI — `CoreService::insert_disk()`
(`core_service.cpp`; замок обязателен: пока GUI меняет файл, поток
эмуляции может быть внутри `read_sector()`):

```cpp
bool CoreService::insert_disk(int drive, const QString& kdi_path, bool read_only) {
    if (drive < 0 || drive >= QFileFddAdapter::DRIVES) return false;
    bool ok = false;
    {
        QMutexLocker lock(&core_mutex_);
        ok = fdd_adapter_.insert(static_cast<uint8_t>(drive), kdi_path, read_only);
        if (ok && initialized_) {
            mb_->fdd_set_write_protect(static_cast<uint8_t>(drive), read_only);
        }
    }
    // …лог…
    return true;
}
```

Для визуализации состояния приводов GUI раз в кадр снимает статус:

```cpp
snap.fdc = mb_->fdd_get_status();      // все 4 привода разом
```

## 5. Озвучивание дисковода

Сценарий «реалистичного» дисковода строится на **отложенных операциях
ВГ93**: ядро (в режиме deferred) при команде позиционирования/чтения
формирует `fdc_pending_op_t` и держит BUSY, а хост сам решает, когда
закрыть операцию — мгновенно, через N миллисекунд, с озвучиванием.

Подписка на операции — `core_service.cpp`:

```cpp
// FDC PendingOp: хост узнаёт о командах мгновенно (звук/LED/задержка).
mb_->fdc_set_pending_op_callback([this](const korvet::fdc_pending_op_t& op) {
    emit fdcPendingOpCreated(op);      // queued в GUI-поток
});
```

Хозяйство таймингов и звуков — `FdcTimingController`
(`korvet_qt6/src/core/fdc_timing.h`):

```cpp
// FdcTimingController — хостовое сопровождение отложенных операций ВГ93.
//
// Ядро (korvet_core, режим deferred) при команде Restore/Seek/Step/Read/
// Write/ReadAddr создаёт PendingOp и держит BUSY. Этот контроллер решает,
// КОГДА закрыть операцию (ТЗ §4.8):
//   - быстрый режим (fdd_sounds.instant_ops): закрыть немедленно;
//   - реалистичный режим: честная модель привода 5.25":
//       * раскрутка шпинделя ~500 мс при первом обращении после простоя
//         (мотор «заведён», пока идут операции; выключается через ~1.5 с
//         тишины);
//       * Type I (позиционирование): steps x step_rate (2/6/12/20 мс по
//         битам [1:0] команды) + успокоение головки ~15 мс;
//       * Type II/III (чтение/запись/чтение адреса): РОТАЦИОННАЯ задержка —
//         сектор приходит под головку через 0..200 мс (300 об/мин),
//         случайная фаза + пересчёт на длину сектора.
//
// Звуки: spin_start/spin/spin_end по раскрутке мотора, step/seek — по
// позиционированию (MAME-сэмплы 525_*.wav из data/sounds/).
```

Когда таймер контроллера срабатывает, операция закрывается одним
вызовом — результат (валидация сектора, статусные биты) вычисляет
сам контроллер ВГ93:

```cpp
void FdcTimingController::complete_now() {
    // …
    core_->fdc_complete_pending();   // → mb.fdc_complete_pending_op()
}
```

Звучит это через тот же `SoundMixer` (`play_file(path, volume, loop)`,
`set_voice_gain()` для играющего лупа мотора). Важно: режим deferred
выключен по умолчанию — MCP-сервер работает в мгновенном эталонном
режиме и про PendingOp не знает вообще.

## 6. Цикл эмуляции в реальном времени

Ядро не знает реального времени — «50 кадров в секунду» обеспечивает
хост. Поток эмуляции korvet_qt6 (`emulation_thread.cpp`) — эталонный
пример дедлайн-планирования:

```cpp
// Кадр Корвета = 1/50 с = 20 мс (2.5 МГц / 50000 тактов). Дедлайн-планирование:
// отставание догоняется без сна, замедление — sleep_until на пропорционально
// больший период.
constexpr auto kFramePeriod = std::chrono::microseconds(20000);
auto next_deadline = std::chrono::steady_clock::now();

while (running_.load(std::memory_order_relaxed) &&
       !stop_requested_.load(std::memory_order_relaxed)) {
    const int speed = speed_percent_.load(std::memory_order_acquire);

    if (core_ && core_->is_powered() && !core_->is_paused()) {
        core_->run_one_frame();               // → mb_->run_frame()
    }

    // Пауза/выключено: не крутим CPU, но продолжаем спать по сетке 20 мс.
    if (core_ && !core_->is_powered()) {
        next_deadline = std::chrono::steady_clock::now() + kFramePeriod;
    } else if (speed > 0) {
        const auto period = kFramePeriod * 100 / speed;   // скорость 1..100%
        next_deadline += period;
        const auto now = std::chrono::steady_clock::now();
        if (next_deadline > now) {
            std::this_thread::sleep_until(next_deadline); // не спешим
        }
        if (next_deadline + period < now) {
            next_deadline = now + period;    // сильно отстали — ресет сетки
        }
    } else {
        next_deadline = std::chrono::steady_clock::now(); // speed == -1: максимум
    }
}
```

## 7. Трассировка исполнения

Ядро отдаёт 34-байтовые записи перед каждой инструкцией; куда их
писать — решает хост. В korvet_qt6 это `TraceWriter`
(`korvet_qt6/src/core/trace_writer.h`): RAM-буфер с порогом сброса +
файл формата KVTR v1 (совместим с утилитой `korvet_trace` из
korvet_mcp_server):

```cpp
// TraceWriter — хостовая сторона трассировки исполнения korvet_core.
//
// Ядро (Motherboard::set_trace_callback) отдаёт 34-байтовые записи
// mb_trace_record_t перед каждой инструкцией; куда их писать — решает хост.
// Здесь: RAM-буфер с порогом сброса + файл формата KVTR v1 (см.
// korvet_core/docs/TRACE_FORMAT.md).

bool start(const QString& file_path, size_t ram_limit_bytes = 10 * 1024 * 1024);
void stop();
void on_record(const korvet::mb_trace_record_t* record);   // колбэк ядра
```

Подключение — одна строка в `CoreService::connect_core_callbacks()`:

```cpp
// Трассировка: ядро отдаёт записи, пишет их TraceWriter (хост).
mb_->set_trace_callback(&CoreService::s_trace_callback, this);
```

## 8. Клавиатура

Хост переводит свои клавиши в матричные коды `(row<<8)|col`; ядро знает
только физическую матрицу 8×12. Раскладка (ЙЦУКЕН/QWERTY) — тоже забота
хоста; ядро лишь честно читает индикатор активной раскладки гостя:

```cpp
// input/keyboard_router.cpp: перевод Qt-клавиши в код матрицы Корвета
core_->key_press(matrix_code);   // ядро не рефсчитает

// Индикатор раскладки гостя (для статус-бара):
snap.kbd_layout_latin = mb_->get_keyboard_language_layout();
```

## 9. Принтер, ИРПС, LAN

Все три порта — побайтовые потоки наружу через колбэки; транспорт и
файлы — на хосте. `core_service.cpp`:

```cpp
// Принтер: каждый байт сразу в GUI (queued) — контроллер сам решает,
// писать ли в TXT/PDF/хост-принтер.
mb_->printer_set_output_callback([this](uint8_t byte) {
    ++printer_bytes_;
    emit printerByte(int(byte));          // из потока эмуляции — только сигнал
});

// Serial: байты, которые гость передаёт наружу. Копим, GUI забирает
// пачкой раз в кадр (50 Гц достаточно для мыши/лога/mirroring).
mb_->serial_set_byte_out_callback([this](uint8_t byte) {
    QMutexLocker lock(&port_mutex_);
    serial_out_buf_.append(char(byte));
    ++serial_tx_;
});

// LAN — симметрично.
mb_->lan_set_byte_out_callback([this](uint8_t byte) { /* … */ });
```

Обратное направление — `serial_inject_byte()` / `lan_inject_byte()`
(байты «с провода» в приёмный буфер Корвета).

---

## Чек-лист интеграции нового хоста

1. `init(mb_config_t{})` → `load_data(rom, mapper, font)` → `power_on()`.
2. Подключить `IFddImageAdapter` (диски) — ДО первого обращения гостя.
3. Цикл: `run_frame()` (или `run_ticks` для пошагового режима) по сетке
   хоста; после каждого кадра забрать `get_front_framebuffer()` и
   `get_audio_frame_buffer()`.
4. Колбэки: лог — обязательно; принтер/ИРПС/LAN/PendingOp — по нужде.
5. Никогда не вызывать фасад из двух потоков одновременно (замок в
   korvet_qt6 — `core_mutex_` — образец).
