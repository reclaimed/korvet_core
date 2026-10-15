# i8080 — Универсальный эмулятор Intel 8080 / КР580ВМ80А

Standalone C++ библиотека для эмуляции процессора **Intel 8080** (и его
советского аналога **КР580ВМ80А**, использовавшегося в ПК8020 «Корвет»).

## Особенности

- **Не зависит от korvet-core** — может быть использована в любом проекте.
- **256 опкодов** проверены построчно по эталону
  [`examples/original/i8080inc.c`](../examples/original/i8080inc.c).
- **Все исправленные баги** оригинальной эмуляции korvet-core перенесены сюда.
- Чистый API: 3 интерфейса (`IMemory`, `IPic`, `ILogger`).

## API

```cpp
#include "i8080.hpp"

class MyMemory : public i8080::IMemory {
    // реализация read/write/read_word/write_word
};

i8080::Cpu cpu;
MyMemory mem;
cpu.init(&mem);          // без прерываний
cpu.reset();
cpu.set_pc(0x0100);      // старт с адреса

while (running) {
    int cycles = cpu.execute_one_instruction();
    if (cpu.is_halted()) break;
}
```

## Сборка

```bash
cd build
cmake ..
cmake --build . --target i8080
```

## Структура

```
src/i8080/
├── i8080.hpp          # Главный header (IMemory, IPic, ILogger, Cpu)
├── cpu.cpp            # Реализация
└── CMakeLists.txt     # Standalone сборка
```

## Использование в другом проекте

1. Скопировать `src/i8080/` в свой проект.
2. Добавить в `CMakeLists.txt`:
   ```cmake
   add_subdirectory(i8080)
   target_link_libraries(my_app PRIVATE i8080)
   ```
3. Реализовать свой `IMemory` или использовать готовый.

## Что было исправлено при выделении

См. [`docs/dead_loop_cpu_analysis.md`](../docs/dead_loop_cpu_analysis.md).

| # | Файл/функция | Что исправлено |
|---|--------------|-----------------|
| 1 | `case 0xBF: CMP A` | Флаги были захардкожены (P_FLAG=true для всех A) |
| 2 | `decimal_adjust_accumulator` (DAA) | Не учитывалась спец. коррекция 0x9A/0xA0/0xFA для SUB |
| 3 | `case 0x2F: CMA` | N_FLAG не устанавливался |

## Расширения

- **Intel 8085** — RIM/SIM инструкции (в коде уже есть, но могут требовать
  доработки).
- **Zilog Z80** — обратно совместимое расширение i8080: добавить IX/IY, префиксы
  `0xDD/0xFD`, второй регистровый набор, инструкции типа `LDIR`.

## Лицензия

Внутренний модуль проекта korvet-emulator.
