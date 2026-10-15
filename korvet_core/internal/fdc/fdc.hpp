#pragma once
#include <functional>

#include <cstdint>
#include <array>
#include <string>
#include <cstdio>

#include "fdc_image_adapter.hpp"

namespace korvet {

class Pic;  // Forward declaration

// =====================================================================
//  Отложенные операции ВГ93 (FDC PendingOp).
//
//  Идея: команда ВГ93 (Restore/Seek/Step/Read/Write/ReadAddr) в режиме
//  deferred НЕ выполняется мгновенно — вместо этого формируется дескриптор
//  отложенной операции и выставляется BUSY. Внешний код (хост) сам решает,
//  когда закрыть операцию: сразу вызвать complete_pending() (быстрый
//  режим), либо подождать N мс, проиграть звук позиционирования, мигнуть
//  светодиодом — и только потом вызвать complete_pending(). Результат
//  (валидация сектора/диска, статусные биты) вычисляет сам контроллер.
//
//  Мотор, вставка/извлечение дискеты и step rate привода остаются снаружи
//  FDC как отдельные события/конфигурация хоста: это свойства платформы,
//  а не команды контроллера. Ядро остаётся platform-agnostic — никаких
//  wav-файлов, таймеров реального времени и политик хоста.
//
//  По умолчанию deferred-режим ВЫКЛЮЧЕН: поведение побитово идентично
//  эталону (мгновенное выполнение) — это гарантирует совместимость с
//  mcp_server/bridge, который ничего не знает о PendingOp.
// =====================================================================

/// Тип отложенной операции (категория команды ВГ93).
enum fdc_op_type_t : uint8_t {
    FDC_OP_RESTORE = 0,   ///< Restore (код 0x0x) — возврат на дорожку 0
    FDC_OP_SEEK = 1,      ///< Seek (0x1x) — переход на RegData
    FDC_OP_STEP = 2,      ///< Step (0x2x/0x3x) — шаг в прежнем направлении
    FDC_OP_STEP_IN = 3,   ///< Step In (0x4x/0x5x) — шаг вперёд
    FDC_OP_STEP_OUT = 4,  ///< Step Out (0x6x/0x7x) — шаг назад
    FDC_OP_READ_SECTOR = 5,   ///< Read Sector (0x8x/0x9x)
    FDC_OP_WRITE_SECTOR = 6,  ///< Write Sector (0xAx/0xBx)
    FDC_OP_READ_ADDRESS = 7,  ///< Read Address (0xCx)
};

/// Дескриптор отложенной операции — «снимок намерения» команды ВГ93.
struct fdc_pending_op_t {
    uint8_t type;         ///< fdc_op_type_t
    uint8_t drive;        ///< выбранный привод (system_&0x03) на момент команды
    uint8_t command;      ///< сырой код команды (RegCom)
    uint8_t from_track;   ///< физическая дорожка ДО операции (track_real_[drive])
    uint8_t to_track;     ///< целевая дорожка (Seek: RegData; Restore: 0; Step: from±1)
    uint8_t track;        ///< логический RegTrack на момент команды
    uint8_t side;         ///< сторона (system_&0x10 ? 1 : 0)
    uint8_t sector;       ///< RegSect на момент команды
    uint8_t multi_sector; ///< бит 0x10 команды (многосекторный обмен)
};

/// Результат завершения отложенной операции.
enum class fdc_op_result_t : uint8_t {
    FDC_OP_RESULT_NONE = 0,        ///< операции не было
    FDC_OP_RESULT_OK = 1,          ///< выполнено (или начата фаза данных)
    FDC_OP_RESULT_WRITE_PROTECT = 2,  ///< защита записи
    FDC_OP_RESULT_NOT_FOUND = 3,  ///< сектор/дорожка/диск не найдены
    FDC_OP_RESULT_CRC_ERROR = 4,  ///< ошибка чтения/контрольной суммы
};

/// Уведомление о создании отложенной операции (вызывается синхронно,
/// в момент записи команды в регистр — из потока эмуляции).
using FdcPendingOpCallback = std::function<void(const fdc_pending_op_t&)>;

// Публичный "снимок" состояния ВГ93 для отладчика/дампов (см. get_state()).
struct fdc_state_t {
    uint8_t status;     // RegStatus
    uint8_t track;      // RegTrack
    uint8_t sector;     // RegSect
    uint8_t data;       // RegData
    uint8_t command;    // RegCom
    uint8_t system;     // System (DRVREG, замаппленный)
};

// ---------------------------------------------------------------------------
// Fdc — построчный порт диспетчера ВГ93 (КР1818ВГ93) по эталону
// examples/original/vg.c (DskVG()) + examples/original/floppy.c (FDC_Read/
// FDC_Write/FDC_Write_DRVREG/FDC_Init/FDC_Reset).
//
// Ключевая идея эталона: единая функция-диспетчер DskVG(), вызываемая на
// КАЖДОЕ обращение к регистрам контроллера (не на такт CPU!) с кодом
// операции VG.OperIO (0..9). Она же на каждый вызов декрементирует
// служебные счётчики (CntReady, DskCountDatLost) — это НЕ времязависимая,
// а СЧЁТНАЯ (по числу обращений к порту) модель времени.
//
// Наша реализация верна этому же принципу: dsk_vg(OperIO) вызывается из
// read()/write()/read_drvreg()/write_drvreg() ровно там же, где в эталоне,
// с тем же кодом операции.
// ---------------------------------------------------------------------------
class Fdc {
public:
    static constexpr uint16_t SECTOR_SIZE = 1024;
    // Формат дискеты (см. vg.c::DiskPosition):
    //   один «логический цилиндр» = 10 секторов подряд: side0[1..5] затем side1[1..5].
    //   позиция в файле = track*10*SECTOR_SIZE + (side?5:0)*SECTOR_SIZE + (sect-1)*SECTOR_SIZE.
    //   Размер файла диска = 80 * 2 * 5 * 1024 = 819200 байт.
    static constexpr uint8_t TRACKS = 80;
    static constexpr uint8_t SIDES = 2;
    static constexpr uint8_t SECTORS_PER_TRACK = 5;
    static constexpr uint32_t DISK_SIZE = TRACKS * SIDES * SECTORS_PER_TRACK * SECTOR_SIZE;

    void init();
    void reset();

    // Управление дисками — ИСКЛЮЧИТЕЛЬНО через адаптер (см.
    // fdc_image_adapter.hpp). Fdc не хранит и не загружает образы сам;
    // "вставка"/"извлечение" дискеты — дело хоста (gui_qt5 на Win/Linux
    // открывает/закрывает файл, RP2040 — работает с SD-картой напрямую).
    void set_image_adapter(IFddImageAdapter* adapter) { adapter_ = adapter; }
    bool is_disk_inserted(uint8_t drive) const {
        return adapter_ && drive < 4 && adapter_->is_present(drive);
    }

    // Защита записи ("это НЕ разрешено на запись"). Срабатывает любое из
    // двух: явно установлено отсюда (GUI пометил конкретный вставленный
    // KDI как read-only) ИЛИ сам файл на хосте только для чтения
    // (adapter_->is_write_protected()). GUI вызывает set_write_protect()
    // сам, когда сочтёт нужным — Fdc не пытается угадывать "новый ли это
    // диск" и ничего не сбрасывает автоматически.
    bool is_write_protected(uint8_t drive) const {
        if (drive >= 4) return true;
        if (forced_write_protect_[drive]) return true;
        return adapter_ && adapter_->is_write_protected(drive);
    }
    void set_write_protect(uint8_t drive, bool read_only) {
        if (drive < 4) forced_write_protect_[drive] = read_only;
    }

    // Управление приводом (используется только для отображения в GUI —
    // в эталоне "мотор" не влияет на тайминг доступа к данным).
    void set_motor(bool on);
    void set_drive(uint8_t drive);
    void set_side(uint8_t side);
    bool is_motor_on() const { return motor_on_; }
    uint8_t get_current_drive() const { return current_drive_; }

    // ------------------------------------------------------------------
    // Порты ввода-вывода — соответствуют floppy.c::FDC_Read/FDC_Write.
    // port&0x03 выбирает регистр:
    //   0 = Status(R)/Command(W)
    //   1 = Track(R/W)
    //   2 = Sector(R/W)
    //   3 = Data(R/W)
    // ------------------------------------------------------------------
    uint8_t read(uint8_t port);
    void write(uint8_t port, uint8_t data);

    // DRVREG — отдельный регистр (PPI1 порт B), floppy.c::FDC_Read_DRVREG/
    // FDC_Write_DRVREG. Запись меняет системный регистр ВГ93 (OperIO=9).
    uint8_t read_drvreg() const;
    void write_drvreg(uint8_t value);

    // INTRQ — программно вычисляемый флаг (см. vg.c: !RegStatus&0x01, т.е.
    // "не занят"). В эталоне НЕ генерирует аппаратное прерывание PIC —
    // это чисто программный сигнал, который ПЗУ само опрашивает.
    bool get_intrq() const { return !(reg_status_ & 0x01); }
    void clear_intrq() { /* совместимость со старым API; в эталоне INTRQ не «сбрасывается» отдельно */ }

    /// ВГ93 BUSY (бит 0 RegStatus) — контроллер выполняет команду.
    /// Пока BUSY, физическая смена дискеты невозможна: хост обязан либо
    /// отказаться, либо дождаться снятия BUSY (обычно POLL-статус
    /// восстанавливается сам после завершения команды/INT-команды).
    bool is_busy() const { return (reg_status_ & 0x01) != 0; }

    // DMA — не используется в эталоне (полноценного DMA-контроллера в
    // Корвете нет), оставлено для совместимости интерфейса.
    void request_dma(bool request) { dma_request_ = request; }
    bool is_dma_requested() const { return dma_request_; }

    void set_pic(Pic* pic) { pic_ = pic; }

    // ------------------------------------------------------------------
    //  Отложенные операции (см. шапку файла). По умолчанию выключены.
    // ------------------------------------------------------------------
    /// Включить/выключить режим отложенных операций. В выключенном
    /// состоянии поведение идентично эталону (мгновенное выполнение).
    void set_deferred_ops(bool enable) { deferred_ops_ = enable; }
    bool is_deferred_ops() const { return deferred_ops_; }

    /// Есть ли незакрытая отложенная операция (контроллер BUSY по ней).
    bool has_pending_op() const { return pending_active_; }

    /// Дескриптор текущей отложенной операции. Осмыслен только при
    /// has_pending_op(); иначе возвращает нулевой дескриптор.
    fdc_pending_op_t get_pending_op() const {
        return pending_active_ ? pending_op_ : fdc_pending_op_t{};
    }

    /// Закрыть отложенную операцию: выполнить команду и обновить
    /// регистры/статус. Возвращает результат. Если операции нет —
    /// FDC_OP_RESULT_NONE (и ничего не меняет).
    fdc_op_result_t complete_pending();

    /// Уведомление о создании отложенной операции (хост играет звук,
    /// зажигает светодиод, взводит свой таймер). Вызывается синхронно
    /// из потока эмуляции — колбэк обязан быть быстрым и не блокировать.
    void set_pending_op_callback(FdcPendingOpCallback cb) { pending_cb_ = std::move(cb); }

    // Снимок состояния для отладчика/дампов.
    fdc_state_t get_state() const {
        fdc_state_t s;
        s.status  = reg_status_;
        s.track   = static_cast<uint8_t>(reg_track_);
        s.sector  = reg_sector_;
        s.data    = reg_data_;
        s.command = reg_command_;
        s.system  = system_;
        return s;
    }

    // Monitoring interface (для GUI)
    struct FdcStatus {
        bool drives[4];
        bool motors_on[4];
        uint8_t current_tracks[4];
        uint8_t current_sectors[4];
        bool write_protected[4];
        uint8_t selected_drive;   // system_ & 0x03 — какой дисковод активен сейчас
        uint8_t command_status;
        bool read_active;
        bool write_active;
        bool seek_active;
        bool idle;
        uint32_t sectors_read;
        uint32_t sectors_written;
        bool pending_op_active;   // открыта отложенная операция (BUSY по ней)
        fdc_pending_op_t pending_op; // её дескриптор (валиден при pending_op_active)
    };
    FdcStatus get_status() const;

private:
    // ------------------------------------------------------------------
    // Состояние ВГ93 — точная копия struct DatVG из vg.h.
    // ------------------------------------------------------------------
    uint8_t data_io_    = 0;     // VG.DataIO
    uint8_t reg_status_ = 0;     // VG.RegStatus
    uint8_t reg_command_ = 0;    // VG.RegCom
    int     reg_track_  = 0;     // VG.RegTrack (может кратко уходить в -1 до клампа, как signed char)
    uint8_t reg_sector_ = 1;     // VG.RegSect
    uint8_t reg_data_   = 0;     // VG.RegData
    uint8_t system_     = 0x3C;  // VG.System (замаппленный: (raw&0xF0)|real_drive)
    uint8_t raw_drvreg_ = 0x00;  // DRVREG — сырое значение регистра (floppy.c: отдельная переменная от VG.System!)
    int     step_direct_ = -1;   // VG.StepDirect (-1 назад, 1 вперёд)
    uint8_t track_real_[4] = {0,0,0,0}; // VG.TrackReal[4]

    // Служебные переменные диспетчера DskVG() — см. vg.c верх файла.
    uint8_t dsk_status_ = 0;          // DskStatus (биты: 0-1 номер диска, 2 отформатирован,
                                       // 6 защита записи, 7 инициализирован)
    uint8_t dsk_index_counter_ = 0;   // DskIndexCounter
    int     cnt_data_ = 0;            // CntData
    int     dsk_data_size_ = 0;       // DskDataSize
    uint8_t dsk_count_dat_lost_ = 0;  // DskCountDatLost
    unsigned cnt_ready_ = 0;          // CntReady
    std::array<uint8_t, SECTOR_SIZE> disk_buf_{}; // DiskBuf
    static constexpr uint8_t DSK_INDEX[5] = {0,1,1,0,0}; // DskIndex[5] (Read Address)

    // Единственный источник данных дискет — см. set_image_adapter().
    IFddImageAdapter* adapter_ = nullptr;
    bool forced_write_protect_[4] = {false, false, false, false};

    bool motor_on_ = false;
    uint8_t current_drive_ = 0;
    uint8_t current_side_ = 0;

    bool dma_request_ = false;

    Pic* pic_ = nullptr;

    // ------------------------------------------------------------------
    //  Отложенные операции: состояние.
    // ------------------------------------------------------------------
    bool deferred_ops_ = false;      ///< режим отложенных операций (выключен по умолчанию)
    bool pending_active_ = false;    ///< открыта ли отложенная операция
    fdc_pending_op_t pending_op_{};  ///< её дескриптор
    FdcPendingOpCallback pending_cb_;///< уведомление хоста о создании операции

    // Диспетчер — точный порт DskVG() из vg.c. oper_io — код операции
    // (см. floppy.c комментарий DC_READSTATUS..DC_RESET).
    void dsk_vg(uint8_t oper_io);

    // ------------------------------------------------------------------
    //  Исполнение команды (тело case 0x1 без BUSY-проверки). Вызывается
    //  как из синхронного пути (сразу при записи команды), так и из
    //  complete_pending() — код один и тот же, меняется только момент.
    //  Возвращает код результата Exit (маппится в статус вызывающим).
    // ------------------------------------------------------------------
    enum class Exit { NONE, OP_OK, OP_WRPROT, OP_NOTFOUND, OP_CONTSUM, OP_COM };
    Exit dsk_vg_command();

    // Отдельные команды (бывшие блоки switch case 0x1).
    void cmd_restore();
    void cmd_seek();
    void cmd_step_in();
    void cmd_step_out();
    Exit cmd_read_sector();      // блок op_read_sector
    Exit cmd_write_sector();     // блок op_write_sector
    Exit cmd_read_address();
    Exit cmd_write_track();

    // Продолжение многосекторного обмена из фазы данных (case 0x6/0x7):
    // RegSect++ и следующая порция — как goto op_read/op_write_sector.
    Exit continue_read_multi();
    Exit continue_write_multi();

    // Маппинг Exit → RegStatus (финальный switch dsk_vg эталона).
    void apply_exit(Exit ex);

    // Построение дескриптора отложенной операции по команде data_io_.
    void begin_pending_op();

    // init/eject текущего выбранного диска (аналог DiskVG(0xA)/(0xB)) —
    // теперь это лишь пересчёт dsk_status_ по ответу adapter_->is_present().
    // Раскладка track/side/sector в смещение файла — больше не забота
    // Fdc, это решает конкретная реализация IFddImageAdapter (см.
    // fdc_image_adapter.hpp): на Win/Linux это QFile::seek(), на RP2040 —
    // адресация внутри FatFs/SD.
    void disk_init();
    void disk_uninit();

    bool read_sector_from_disk(int track, uint8_t side, uint8_t sector, uint8_t* buf);
    bool write_sector_to_disk(int track, uint8_t side, uint8_t sector, const uint8_t* buf);

    // Таблица маппинга «сырого» кода дисковода (биты 0-3 DRVREG) в реальный
    // номер дисковода — floppy.c:38 (int drv[]={0,0,1,1,2,1,2,1,3,0,1,0,1,0,1,0};).
    static const uint8_t DRV_TABLE[16];

    // DRVREG биты (см. floppy.c и korvet.h)
    static constexpr uint8_t DRVREG_MOTOR = 0x20;
    static constexpr uint8_t DRVREG_SIDE  = 0x10;
};

}  // namespace korvet
