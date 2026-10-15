# FDC: отложенные операции (PendingOp)

## Назначение

Превращает ВГ93 (КР1818ВГ93) из «мгновенного» диспетчера в конечный
автомат с явным состоянием ожидания: команда Type I/II/III не исполняется
сразу, а формирует **дескриптор отложенной операции** и выставляет BUSY.
Внешний код решает, когда закрыть операцию — мгновенно (быстрый режим)
или после задержки/звука/мигания светодиода. Мотор, вставка/извлечение
дискеты и step rate — события/конфигурация платформы, остаются снаружи.

Ядро остаётся platform-agnostic: никаких wav-файлов, таймеров реального
времени и политик хоста.

## API (internal/fdc/fdc.hpp)

```cpp
// Типы
enum fdc_op_type_t { FDC_OP_RESTORE, FDC_OP_SEEK, FDC_OP_STEP,
                     FDC_OP_STEP_IN, FDC_OP_STEP_OUT, FDC_OP_READ_SECTOR,
                     FDC_OP_WRITE_SECTOR, FDC_OP_READ_ADDRESS };

struct fdc_pending_op_t {
    uint8_t type;         // fdc_op_type_t
    uint8_t drive;        // system_&0x03 на момент команды
    uint8_t command;      // сырой код команды (RegCom)
    uint8_t from_track;   // физическая дорожка ДО (track_real_[drive])
    uint8_t to_track;     // цель (Seek: RegData; Restore: 0; Step: ±1)
    uint8_t track;        // логический RegTrack
    uint8_t side;         // 0/1
    uint8_t sector;       // RegSect
    uint8_t multi_sector; // бит 0x10 команды
};

enum class fdc_op_result_t { FDC_OP_RESULT_NONE, FDC_OP_RESULT_OK,
                             FDC_OP_RESULT_WRITE_PROTECT,
                             FDC_OP_RESULT_NOT_FOUND,
                             FDC_OP_RESULT_CRC_ERROR };

using FdcPendingOpCallback = std::function<void(const fdc_pending_op_t&)>;

// Управление
void   set_deferred_ops(bool enable);       // по умолчанию ВЫКЛ
bool   has_pending_op() const;
fdc_pending_op_t get_pending_op() const;
fdc_op_result_t  complete_pending();
void   set_pending_op_callback(FdcPendingOpCallback cb);
```

Фасад Motherboard: `fdc_set_deferred_ops()`, `fdc_has_pending_op()`,
`fdc_get_pending_op()`, `fdc_complete_pending_op()`,
`fdc_set_pending_op_callback()`.

## Семантика

- **Deferred выключен** (по умолчанию): поведение побитово идентично
  эталону `vg.c`/`floppy.c` — команда исполняется при записи в регистр.
  Это гарантирует совместимость с mcp_server/bridge, который не знает о
  PendingOp. Подтверждено полным прогоном тестов mcp_server.
- **Deferred включён**: команда Type I (Restore/Seek/Step), Type II
  (Read/Write Sector) и Read Address откладываются: `reg_status_ = BUSY`,
  дескриптор копируется в `pending_op_`, вызывается колбэк. Хост закрывает
  операцию `complete_pending()` — исполняется ровно та же логика
  (`dsk_vg_command()`), что и в синхронном пути.
- Пока операция открыта: гость опрашивает статус и видит BUSY (реальная
  ВГ93 держит BUSY, пока головка едет); записи команд игнорируются
  (как и при BUSY фазы данных в эталоне).
- **Никакого реального времени в ядре**: задержка — целиком решение
  хоста (korvet_qt6: `QTimer` + звуки; тесты: мгновенно).
- Multi-sector продолжения в фазе данных (`goto op_read_sector` эталона)
  остаются мгновенными — движения головки между секторами одной дорожки
  нет.
- `reset()`/`init()` гасят pending (сброс контроллера снимает BUSY).
- Смена диска при BUSY: хост обязан отказаться или дождаться снятия
  BUSY (`is_busy()`).

## Референс корректности

Тест `korvet_qt6/tests/test_fdc_pending.cpp` (без Qt) проверяет:

1. синхронный Seek == эталон;
2. deferred Seek: pending создан, BUSY виден, complete → идентичное
   конечное состояние регистров;
3. Restore из дорожки 5 → 0;
4. Read Sector: данные после complete совпадают с sync-чтением;
5. Write Sector с защитой записи → `WRITE_PROTECT`, статус 0x40;
6. reset гасит pending; complete без pending → `NONE`;
7. колбэк вызывается с корректным дескриптором;
8. sync-режим не создаёт pending (совместимость).

## Пример хоста (korvet_qt6, FdcTimingController)

```cpp
mb->fdc_set_deferred_ops(true);
mb->fdc_set_pending_op_callback([this](const fdc_pending_op_t& op) {
    emit fdcPendingOpCreated(op);           // queued → GUI-поток
});

// GUI: задержка = steps × step_rate(биты [1:0] команды: 2/6/12/20 мс)
// + звук 525_step_1_1 / 525_seek_Nms → QTimer::singleShot(delay) →
mb->fdc_complete_pending_op();
```

## Отладка

- Переменная окружения `KORVET_FDC_DEFERRED=0` (korvet_qt6) принудительно
  включает эталонный синхронный режим ВГ93 — для сравнения поведения и
  диагностики проблем носителей.
