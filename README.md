# korvet_core — ядро эмуляции ПК8020 «Корвет»

Platform-agnostic ядро эмуляции советского персонального компьютера
**ПК8020 «Корвет»** (1985, НИИ ядерной физики МГУ): процессор
КР580ВМ80А (аналог Intel 8080) на 2,5 МГц, 64 КБ ОЗУ, графическая
подсистема ГЗУ/АЦЗУ 512×256×16 цветов, контроллер дисководов
КР1818ВГ93 и периферия на БИС серии КР580.

korvet_core — ядро эмуляции: CPU, память, видео, звук, дисковод,
порты. Ядро не знает об ОС и хосте: всё общение с внешним миром —
колбэки, фасад и указатели.

## Ключевые свойства

- **Полная изоляция от хоста.** Ни файлового I/O, ни потоков, ни
  сокетов, ни реального времени. Всё общение — через колбэки и
  интерфейсы (`IFddImageAdapter`, `set_*_callback`). Файлы открывает
  хост, ядро получает только байты.
- **Все тайминги — в тактах CPU** (2,5 МГц). Кадр = 50 000 тактов
  (50 Гц), звук — тики канала 0 таймера. Реального времени в ядре нет
  ни на одном уровне — «сколько ждать» решает хост.
- **Портируемость.** Компилируется под Linux/Windows; для bare-metal
  (RP2040/STM32) есть опция `KORVET_SINGLE_THREADED` — сборка без
  `std::mutex`. Экономия ОЗУ учтена: шрифты разворачиваются
  16-байтовой таблицей (без буферов), видеостраницы — по 64 КБ на
  страницу ГЗУ.
- **Точность по технической документации.** Регистры, команды и
  протоколы — по «Описанию программно-доступных элементов ПЭВМ ПК8020»
  (НИИСЧЕТМАШ, 10.12.1986) и комментариям к принципиальной схеме
  (pk8020.ru). Поведение каждого узла сверено запуском гостевого ПО.
- **Отложенные операции ВГ93** (PendingOp) — опциональный режим с
  реалистичными таймингами дискеты для GUI (звуки, светодиод), при этом
  по умолчанию команды исполняются мгновенно — см.
  [docs/FDC_PENDINGOP.md](docs/FDC_PENDINGOP.md).

## Эмулируемое железо

| Узел | БИС | Модуль ядра |
|------|-----|-------------|
| CPU | КР580ВМ80А (i8080), 2,5 МГц | `i8080/` (standalone) + `internal/cpu/` |
| ОЗУ/ПЗУ + маппер страниц | 64 КБ RAM, 8–24 КБ ROM, 32×256 рег/стр | `internal/memory/` |
| Видео ГЗУ/АЦЗУ | 512×256, 16 цветов, 4 страницы графики, 1 страница текста | `internal/video/` |
| Таймер | КР580ИК53 (i8253), 3 канала: 0 — звук (2 МГц), 1 — скорость ИРПС (2 МГц), 2 — системный таймер (HBL 65,6 мкс / 50 Гц / выкл) | `internal/timer/` |
| Контроллер прерываний | КР580ВН59 (i8259), 8 уровней | `internal/pic/` |
| Параллельные порты | 3 × КР580ВВ55 (i8255): видео/диск, звук/кассета/принтер, расширение | `internal/ppi/` |
| Дисковод | КР1818ВГ93 (WD1793), 4 привода, мотор — одновибратор MON 3 с | `internal/fdc/` |
| Последовательный порт | КР580ВВ51А (8251A) «ИРПС» | `internal/serial/` |
| Локальная сеть | КР580ВВ51А (8251A), ~19500 бит/с, LANINT | `internal/lan/` |
| Принтер | порт LST: данные PPI2-A, строб PPI2-C бит 5 | `internal/printer/` |
| Клавиатура | матрица 8×13 (KB00–KB11 + ПЧТ) | `internal/keyboard/` |
| Магнитофон | однобитовый канал CASIN/CASOUT + реле стопа | `internal/tape/` |
| Мышь | последовательная (ИРПС, Microsoft) / параллельная (ММ8031 через ЭУ8040) | `internal/mouse/` |
| Джойстик | МД8030 через ЭУ8040 (порт расширения) | `internal/joystick/` |
| Звук | пьезоизлучатель, канал 0 таймера через гейт SNDEN, 22050 Гц | `internal/audio/` |

Прерывания: IRQ0 AUXINT (расширение) · IRQ1 SERINT1 (приём ИРПС) ·
IRQ2 SERINT2 (передатчик ИРПС) · IRQ3 LANINT (ЛВС) · IRQ4 PERINT
(кадровые 50 Гц) · IRQ5 TMRINT (канал 2 таймера) · IRQ6 LSTINT
(принтер) · IRQ7 MOTINT (выключение мотора НГМД).

## Быстрый старт

```bash
# Ядро как статическая библиотека (+ демо + minigui)
cmake -S . -B build && cmake --build build

# Самопроверка подсистем (52 проверки)
./build/korvet_core/minigui/minigui --selftest

# ИРПС/мышь/джойстик/LAN гостевым кодом 8080 под CP/M (10/10 на режим)
./build/korvet_core/minigui/periph_guest_test <ПЗУ> <системный KDI> serial
./build/korvet_core/minigui/periph_guest_test <ПЗУ> <системный KDI> parallel

# Загрузка CP/M с дискетой (данные в отдельном репозитории korvet_data):
./build/korvet_core/minigui/minigui \
    --rom korvet_data/data/rom/cpm22_shkomp.rom \
    --mapper korvet_data/data/rom/mapper.mem \
    --font korvet_data/data/rom/korvet.fnt \
    --disk 0=korvet_data/data/kdi/disk__system.kdi \
    --frames 900 --shot 900=boot.png --status
```

Файлы данных (ПЗУ, маппер, шрифты, KDI-образы) в комплект ядра не
входят — подготовьте их отдельно (см. проект korvet_data).

Минимальное подключение (хост читает файлы сам):

```cpp
#include "korvet_core/motherboard.hpp"

auto& mb = korvet::Motherboard::get_instance();
korvet::mb_config_t cfg = {};              // разумные дефолты
mb.init(&cfg);
mb.load_data(rom, rom_size, mapper, mapper_size, font, font_size);
mb.power_on();
mb.run_ticks(50'000);                      // один кадр (50 Гц)
mb.render_video_frame();                   // кадр готов: readout_video()
```

Дальше — [docs/MOTHERBOARD_API.md](docs/MOTHERBOARD_API.md)
(справочник фасада) и [korvet_core/minigui/README.md](korvet_core/minigui/README.md)
(инструмент проверки с примерами запуска гостевого ПО).

## Состав репозитория

```
korvet_core/
├── CMakeLists.txt          — сборка ядра + примеры + minigui
├── korvet_core/
│   ├── motherboard.{hpp,cpp}      — фасад ядра (единственный публичный вход)
│   ├── motherboard_types.hpp      — конфигурация, колбэки, трасса
│   ├── examples/core_demo.cpp     — минимальная инициализация
│   ├── minigui/                   — CLI-инструмент проверки (+ опция SDL2)
│   └── internal/                  — подсистемы (см. таблицу выше)
├── i8080/                  — standalone-библиотека CPU
└── docs/                   — документация ядра
```

## Документация

| Документ | Содержание |
|---|---|
| [docs/OVERVIEW.md](docs/OVERVIEW.md) | архитектура, глоссарий, карта подсистем |
| [docs/MOTHERBOARD_API.md](docs/MOTHERBOARD_API.md) | полный справочник фасада |
| [docs/FDC_PENDINGOP.md](docs/FDC_PENDINGOP.md) | отложенные операции ВГ93 |
| [docs/KEYBOARD.md](docs/KEYBOARD.md) | матрица клавиатуры и таблицы KB00–KB12 |
| [docs/TRACE_FORMAT.md](docs/TRACE_FORMAT.md) | двоичный формат трассы KVTR v1 |
| [korvet_core/minigui/README.md](korvet_core/minigui/README.md) | инструмент проверки, верификация гостевым ПО |

## Лицензия

См. [LICENSE](LICENSE).
