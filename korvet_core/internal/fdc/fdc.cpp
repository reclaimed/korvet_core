#include "fdc.hpp"
#include "../pic/pic.hpp"
#include "../logger/logger.hpp"

namespace korvet {

// floppy.c:38 — int drv[]={0,0,1,1,2,1,2,1,3,0,1,0,1,0,1,0};
const uint8_t Fdc::DRV_TABLE[16] = {0,0,1,1, 2,1,2,1, 3,0,1,0, 1,0,1,0};

// vg.c: static unsigned char DskIndex[5]={0,1,1,0,0}; (используется Read Address)
constexpr uint8_t Fdc::DSK_INDEX[5];

void Fdc::init() {
    // floppy.c::FDC_Init(): VG.OperIO=0x0a; DskVG(); (InUseFDD сброс — не нужен, это GUI-таймер "диск используется")
    reset();
    dsk_vg(0x0A);
}

void Fdc::reset() {
    // Базовая инициализация полей (не через DskVG — это конструкторные значения).
    data_io_ = 0;
    reg_status_ = 0;
    reg_command_ = 0;
    reg_track_ = 0;
    reg_sector_ = 1;
    reg_data_ = 0;
    system_ = 0x3C;
    raw_drvreg_ = 0;
    step_direct_ = -1;
    for (int i = 0; i < 4; i++) track_real_[i] = 0;

    dsk_status_ = 0;
    dsk_index_counter_ = 0;
    cnt_data_ = 0;
    dsk_data_size_ = 0;
    dsk_count_dat_lost_ = 0;
    cnt_ready_ = 0;
    disk_buf_.fill(0);

    motor_on_ = false;
    current_drive_ = 0;
    current_side_ = 0;
    dma_request_ = false;

    // Отложенных операций после сброса не существует (совместимо с
    // «сброс контроллера» реальной ВГ93 — BUSY гаснет).
    pending_active_ = false;
    pending_op_ = fdc_pending_op_t{};

    // floppy.c::FDC_Reset(): VG.OperIO=0x0c; DskVG(); VG.OperIO=0x0a; DskVG();
    dsk_vg(0x0C);
    dsk_vg(0x0A);
}

void Fdc::set_motor(bool on) { motor_on_ = on; }
void Fdc::set_drive(uint8_t drive) { if (drive < 4) current_drive_ = drive; }
void Fdc::set_side(uint8_t side) { current_side_ = side & 1; }

bool Fdc::read_sector_from_disk(int track, uint8_t side, uint8_t sector, uint8_t* buf) {
    uint8_t drive = system_ & 0x03;
    if (!adapter_) return false;
    return adapter_->read_sector(drive, static_cast<uint8_t>(track), side, sector, buf, SECTOR_SIZE);
}

bool Fdc::write_sector_to_disk(int track, uint8_t side, uint8_t sector, const uint8_t* buf) {
    uint8_t drive = system_ & 0x03;
    if (!adapter_) return false;
    return adapter_->write_sector(drive, static_cast<uint8_t>(track), side, sector, buf, SECTOR_SIZE);
}

// ---------------------------------------------------------------------------
// disk_init()/disk_uninit() — аналог DiskVG(0xA)/DiskVG(0xB) в эталоне.
// В эталоне это реально открывает/закрывает файл на диске; у нас "открытие"
// диска — это просто вопрос к адаптеру, вставлен ли физически носитель
// в ТЕКУЩИЙ выбранный (system_&0x03) дисковод. Сам файл/сектор адаптер
// открывает лениво, по каждому read_sector()/write_sector() — Fdc об этом
// ничего не знает.
// ---------------------------------------------------------------------------
void Fdc::disk_init() {
    // vg.c case 0xA: if (DskStatus&0x80) return;
    if (dsk_status_ & 0x80) return;
    uint8_t drive = system_ & 0x03;
    dsk_status_ = drive;
    if (adapter_ && adapter_->is_present(drive)) {
        dsk_status_ |= 0x04;  // «отформатирован»/успешно открыт
    }
    // Защита записи: либо явно установлена GUI через set_write_protect()
    // (см. fdc.hpp), либо сам файл на хосте только для чтения — сработает
    // любое из двух.
    if (is_write_protected(drive)) {
        dsk_status_ |= 0x40;
    }
    dsk_status_ |= 0x80;      // «инициализирован»
}

void Fdc::disk_uninit() {
    // vg.c case 0xB: if (!(DskStatus&0x80)) return; ... DskStatus=0x00;
    if (!(dsk_status_ & 0x80)) return;
    dsk_status_ = 0x00;
}

// ---------------------------------------------------------------------------
// set_state_step() — точный порт SetStateStep() из vg.c.
// Вызывается после любой команды Type I (Restore/Seek/Step).
// ---------------------------------------------------------------------------
static inline void set_state_step_impl(int& reg_track, uint8_t& track_real_slot,
                                        uint8_t reg_com, uint8_t dsk_status,
                                        uint8_t& reg_status, unsigned& cnt_ready) {
    if (reg_track < 0) {
        track_real_slot = 0;
        reg_track = 0;
    }
    if (reg_track > 0x4F) {
        track_real_slot = 0x4F;
        reg_track = 0x4F;
    }
    reg_status = dsk_status & 0x40;

    cnt_ready = 0x10;  // ESL: «неготовность» на несколько команд чтения статуса

    if (reg_com & 0x08) reg_status |= 0x20;
    if (!reg_track) reg_status |= 0x04;
}

// ---------------------------------------------------------------------------
// Команды ВГ93 — бывшие блоки switch(reg_command_>>4) внутри case 0x1.
// Логика побитово сохранена из эталона; изменена только упаковка в методы,
// чтобы один и тот же код работал и в синхронном пути, и в complete_pending().
// ---------------------------------------------------------------------------

void Fdc::cmd_restore() {
    // Восстановление (0x0x)
    track_real_[system_ & 0x03] = 0;
    reg_track_ = 0;
    step_direct_ = -1;
    set_state_step_impl(reg_track_, track_real_[system_ & 0x03],
                         reg_command_, dsk_status_, reg_status_, cnt_ready_);
}

void Fdc::cmd_seek() {
    // Поиск (0x1x)
    int target = reg_data_;
    track_real_[system_ & 0x03] = static_cast<uint8_t>(target);
    reg_track_ = target;
    // (эталонная проверка знака тут вырождена, т.к. RegData-RegTrack==0 всегда
    //  сразу после присваивания — сохраняем как есть для точности порта)
    step_direct_ = -1;
    set_state_step_impl(reg_track_, track_real_[system_ & 0x03],
                         reg_command_, dsk_status_, reg_status_, cnt_ready_);
}

void Fdc::cmd_step_in() {
    // Шаг вперёд (0x4x/0x5x)
    if (reg_track_ >= 0) {
        reg_track_++;
        track_real_[system_ & 0x03] = static_cast<uint8_t>(reg_track_);
    }
    step_direct_ = 1;
    set_state_step_impl(reg_track_, track_real_[system_ & 0x03],
                         reg_command_, dsk_status_, reg_status_, cnt_ready_);
}

void Fdc::cmd_step_out() {
    // Шаг назад (0x6x/0x7x; также Step 0x2x/0x3x при step_direct_==-1)
    if (reg_track_) {
        reg_track_--;
        track_real_[system_ & 0x03] = static_cast<uint8_t>(reg_track_);
    }
    step_direct_ = -1;
    set_state_step_impl(reg_track_, track_real_[system_ & 0x03],
                         reg_command_, dsk_status_, reg_status_, cnt_ready_);
}

Fdc::Exit Fdc::cmd_read_sector() {
    // Чтение сектора (0x8x/0x9x) — блок op_read_sector
    if (reg_sector_ > 0x10 ||
        reg_track_ != track_real_[system_ & 0x03] ||
        !(adapter_ && adapter_->is_present(system_ & 0x03))) {
        return Exit::OP_NOTFOUND;
    }
    if (!read_sector_from_disk(reg_track_, (system_ & 0x10) ? 1 : 0,
                                static_cast<uint8_t>(reg_sector_ - 1), disk_buf_.data())) {
        return Exit::OP_CONTSUM;
    }
    dsk_data_size_ = SECTOR_SIZE - 1;
    reg_data_ = disk_buf_[0];
    return Exit::OP_COM;
}

Fdc::Exit Fdc::cmd_write_sector() {
    // Запись сектора (0xAx/0xBx) — блок op_write_sector
    if (dsk_status_ & 0x40) { return Exit::OP_WRPROT; }
    if (reg_sector_ > 0x10 ||
        reg_track_ != track_real_[system_ & 0x03] ||
        !(adapter_ && adapter_->is_present(system_ & 0x03))) {
        return Exit::OP_NOTFOUND;
    }
    dsk_data_size_ = SECTOR_SIZE;
    return Exit::OP_COM;
}

Fdc::Exit Fdc::cmd_read_address() {
    // Чтение адреса (0xCx)
    dsk_data_size_ = 0x0005;
    reg_data_ = 1; // ReadIndexTrack всегда 0 в эталоне
    return Exit::OP_COM;
}

Fdc::Exit Fdc::cmd_write_track() {
    // Запись дорожки (0xFx)
    if (dsk_status_ & 0x40) { return Exit::OP_WRPROT; }
    dsk_data_size_ = 1; // FormatDatLen всегда 1 в эталоне
    return Exit::OP_COM;
}

Fdc::Exit Fdc::continue_read_multi() {
    // Из case 0x6: RegSect++ и переход на op_read_sector
    reg_sector_++;
    return cmd_read_sector();
}

Fdc::Exit Fdc::continue_write_multi() {
    // Из case 0x7: RegSect++ и переход на op_write_sector
    reg_sector_++;
    return cmd_write_sector();
}

Fdc::Exit Fdc::dsk_vg_command() {
    // Тело case 0x1 (без BUSY-проверки — её делает вызывающий).
    // reg_command_ = data_io_ присваивается ЗДЕСЬ, чтобы complete_pending()
    // не повторял присваивание (команда уже сохранена в begin_pending_op()).
    reg_command_ = data_io_;
    switch (reg_command_ >> 4) {
        case 0x0: { // Восстановление
            cmd_restore();
            return Exit::NONE;
        }
        case 0x1: { // Поиск
            cmd_seek();
            return Exit::NONE;
        }
        case 0x2:
        case 0x3: { // Шаг (направление = предыдущее)
            if (step_direct_ == -1) {
                cmd_step_out();
                return Exit::NONE;
            }
            cmd_step_in();
            return Exit::NONE;
        }
        case 0x4:
        case 0x5: { // Шаг вперёд
            cmd_step_in();
            return Exit::NONE;
        }
        case 0x6:
        case 0x7: { // Шаг назад
            cmd_step_out();
            return Exit::NONE;
        }
        case 0x8:
        case 0x9: { // Чтение сектора
            return cmd_read_sector();
        }
        case 0xA:
        case 0xB: { // Запись сектора
            return cmd_write_sector();
        }
        case 0xC: { // Чтение адреса
            return cmd_read_address();
        }
        case 0xE: { // Чтение дорожки — не реализовано в эталоне
            return Exit::NONE;
        }
        case 0xF: { // Запись дорожки
            return cmd_write_track();
        }
        default: return Exit::NONE;
    }
}

void Fdc::apply_exit(Exit ex) {
    // Финальный switch(exit_via) эталонного DskVG().
    switch (ex) {
        case Exit::OP_OK:       reg_status_ = 0x00; return;
        case Exit::OP_WRPROT:   reg_status_ = 0x40; return;
        case Exit::OP_NOTFOUND: reg_status_ = 0x10; return;
        case Exit::OP_CONTSUM:  reg_status_ = 0x08; return;
        case Exit::OP_COM: {
            dsk_count_dat_lost_ = 0x00;
            cnt_data_ = 0x0000;
            reg_status_ = 0x03;
            // Ready в эталоне всегда 1 (глобальная переменная, нигде не
            // сбрасывается) — ветка "if (!Ready)" в эталоне мертва, поэтому
            // здесь намеренно опущена: reg_status_ всегда остаётся 0x03.
            return;
        }
        case Exit::NONE:
        default:
            return;
    }
}

// ---------------------------------------------------------------------------
// Отложенные операции.
// ---------------------------------------------------------------------------

void Fdc::begin_pending_op() {
    // Классификация команды и построение дескриптора.
    const uint8_t cmd = data_io_;
    const uint8_t hi = cmd >> 4;
    const uint8_t drive = system_ & 0x03;

    pending_op_ = fdc_pending_op_t{};
    pending_op_.command = cmd;
    pending_op_.drive = drive;
    pending_op_.track = static_cast<uint8_t>(reg_track_);
    pending_op_.side = (system_ & 0x10) ? 1 : 0;
    pending_op_.sector = reg_sector_;
    pending_op_.multi_sector = (cmd & 0x10) ? 1 : 0;
    pending_op_.from_track = track_real_[drive];

    switch (hi) {
        case 0x0: // Restore → дорожка 0
            pending_op_.type = FDC_OP_RESTORE;
            pending_op_.to_track = 0;
            break;
        case 0x1: // Seek → RegData
            pending_op_.type = FDC_OP_SEEK;
            pending_op_.to_track = reg_data_;
            break;
        case 0x2:
        case 0x3: { // Step в прежнем направлении
            pending_op_.type = FDC_OP_STEP;
            pending_op_.to_track = (step_direct_ >= 0)
                ? static_cast<uint8_t>(track_real_[drive] + 1)
                : static_cast<uint8_t>(track_real_[drive] - 1);
            break;
        }
        case 0x4:
        case 0x5: // Step In
            pending_op_.type = FDC_OP_STEP_IN;
            pending_op_.to_track = static_cast<uint8_t>(track_real_[drive] + 1);
            break;
        case 0x6:
        case 0x7: // Step Out
            pending_op_.type = FDC_OP_STEP_OUT;
            pending_op_.to_track = static_cast<uint8_t>(track_real_[drive] - 1);
            break;
        case 0x8:
        case 0x9:
            pending_op_.type = FDC_OP_READ_SECTOR;
            pending_op_.to_track = track_real_[drive];
            break;
        case 0xA:
        case 0xB:
            pending_op_.type = FDC_OP_WRITE_SECTOR;
            pending_op_.to_track = track_real_[drive];
            break;
        case 0xC:
            pending_op_.type = FDC_OP_READ_ADDRESS;
            pending_op_.to_track = track_real_[drive];
            break;
        default:
            pending_op_.type = FDC_OP_READ_SECTOR;
            break;
    }

    // Команда принимается, статус — BUSY (реальная ВГ93 держит BUSY,
    // пока головка едет/данные готовятся).
    reg_command_ = cmd;
    pending_active_ = true;
    reg_status_ = 0x01;

    if (pending_cb_) {
        pending_cb_(pending_op_);
    }
}

fdc_op_result_t Fdc::complete_pending() {
    if (!pending_active_) {
        return fdc_op_result_t::FDC_OP_RESULT_NONE;
    }
    pending_active_ = false;

    // data_io_ за время ожидания мог быть перезаписан (опросы статуса
    // пишут туда RegStatus) — восстанавливаем код команды из дескриптора.
    // dsk_vg_command() переприсвоит им reg_command_.
    data_io_ = pending_op_.command;
    const Exit ex = dsk_vg_command();
    apply_exit(ex);

    switch (ex) {
        case Exit::OP_OK:       return fdc_op_result_t::FDC_OP_RESULT_OK;
        case Exit::OP_WRPROT:   return fdc_op_result_t::FDC_OP_RESULT_WRITE_PROTECT;
        case Exit::OP_NOTFOUND: return fdc_op_result_t::FDC_OP_RESULT_NOT_FOUND;
        case Exit::OP_CONTSUM:  return fdc_op_result_t::FDC_OP_RESULT_CRC_ERROR;
        case Exit::OP_COM:      return fdc_op_result_t::FDC_OP_RESULT_OK;
        case Exit::NONE:
        default:                return fdc_op_result_t::FDC_OP_RESULT_OK;
    }
}

// ---------------------------------------------------------------------------
// dsk_vg() — построчный порт DskVG() из vg.c. oper_io соответствует VG.OperIO.
// Вызывается на КАЖДОЕ обращение к регистру контроллера, как в эталоне —
// не на такт CPU. Это принципиально: служебные счётчики (cnt_ready_,
// dsk_count_dat_lost_) считают ЧИСЛО ОБРАЩЕНИЙ, а не время.
// ---------------------------------------------------------------------------
void Fdc::dsk_vg(uint8_t oper_io) {
    // Счётчики «потери данных»/«неготовности» (DskCountDatLost/CntReady) —
    // модель ФАЗЫ ДАННЫХ эталона: они затухают от числа ОБРАЩЕНИЙ К ПОРТАМ,
    // пока контроллер BUSY. В эталоне (мгновенные команды) фаза ожидания
    // команды гостем не существует, поэтому счётчики начинают жизнь только
    // после исполнения команды. В deferred-режиме гость честно ждёт BUSY
    // (позиционирование!) и успевает опросить статус много раз — затухание
    // срабатывало бы СРЕДИ ожидания и портило статус (0x04/0x03 до
    // завершения команды). Поэтому на время открытой отложенной операции
    // счётчики заморожены. Синхронный режим (mcp_server) не задет:
    // pending_active_ там всегда false.
    if (!pending_active_) {
        if ((reg_status_ & 0x01) && !--dsk_count_dat_lost_) reg_status_ = 0x04;
        if (cnt_ready_) { if (!--cnt_ready_) reg_status_ = 0x03; }
    }

    // goto-метки эталона реализованы через локальный enum-результат.
    Exit exit_via = Exit::NONE;

    switch (oper_io) {
        case 0x0: { // Чтение состояния
            data_io_ = reg_status_;
            if (reg_command_ & 0x80) break;
            if ((--dsk_index_counter_) & 0xE) data_io_ |= 0x02;
            break;
        }
        case 0x1: { // Запись команды
            if (reg_status_ & 0x01) {
                // Принудительное прерывание — в эталоне тут пусто (не Force
                // Interrupt как отдельная ветка, см. ниже: пока BUSY,
                // единственное что реально принимается — 0xD0 фильтруется
                // следующей строкой до диспетчеризации).
            } else {
                if ((data_io_ & 0xF0) == 0xD0) { break; } // return — команда Force Interrupt здесь игнорируется (не BUSY)
                // ---------------------------------------------------------
                // Режим отложенных операций: команда Type I/II/III не
                // исполняется сейчас — формируется дескриптор PendingOp и
                // выставляется BUSY. Хост закроет операцию вызовом
                // complete_pending() (сразу или после задержки/звука).
                // ---------------------------------------------------------
                const uint8_t hi = data_io_ >> 4;
                // Type I (0x0-0x7), Type II (0x8-0xB) и Read Address (0xC).
                // 0xD (Force Interrupt) фильтруется выше; 0xE/0xF (дорожки)
                // остаются мгновенными, как в эталоне.
                const bool deferrable = deferred_ops_ && !pending_active_ && (hi <= 0xC);
                if (deferrable) {
                    begin_pending_op();
                } else {
                    exit_via = dsk_vg_command();
                }
                // Бывшие goto-цели op_read_sector/op_write_sector из этого
                // блока удалены: многосекторные продолжения в фазе данных
                // обрабатываются в case 0x6/0x7 через continue_*_multi().
            }
            break;
        }
        case 0x2: { // Чтение дорожки
            data_io_ = static_cast<uint8_t>(reg_track_);
            break;
        }
        case 0x3: { // Запись дорожки
            if (!(reg_status_ & 0x01)) reg_track_ = data_io_;
            break;
        }
        case 0x4: { // Чтение регистра сектора
            data_io_ = reg_sector_;
            break;
        }
        case 0x5: { // Запись регистра сектора
            if (!(reg_status_ & 0x01)) reg_sector_ = data_io_;
            break;
        }
        case 0x6: { // Чтение данных
            if (reg_status_ & 0x01) {
                switch (reg_command_ >> 4) {
                    case 0x8:
                    case 0x9: { // Чтение данных команды "Чтение сектора"
                        data_io_ = reg_data_;
                        dsk_count_dat_lost_ = 0x10;
                        if (cnt_data_ < dsk_data_size_) {
                            reg_data_ = disk_buf_[1 + cnt_data_];
                            cnt_data_++;
                        } else {
                            if (reg_command_ & 0x10) {
                                exit_via = continue_read_multi();
                            } else {
                                exit_via = Exit::OP_OK;
                            }
                        }
                        break;
                    }
                    case 0xC: { // Чтение данных команды "Чтение адреса"
                        data_io_ = reg_data_;
                        dsk_count_dat_lost_ = 0x10;
                        if (cnt_data_ < dsk_data_size_) {
                            reg_data_ = DSK_INDEX[cnt_data_];
                            cnt_data_++;
                        } else {
                            exit_via = Exit::OP_OK;
                        }
                        break;
                    }
                    default: break;
                }
            } else {
                data_io_ = reg_data_;
            }
            break;
        }
        case 0x7: { // Запись данных
            if (reg_status_ & 0x01) {
                switch (reg_command_ >> 4) {
                    case 0xA:
                    case 0xB: { // Запись данных команды "Запись сектора"
                        disk_buf_[cnt_data_] = data_io_;
                        reg_data_ = data_io_;
                        cnt_data_++;
                        dsk_count_dat_lost_ = 0x10;
                        if (cnt_data_ >= dsk_data_size_) {
                            if (!write_sector_to_disk(reg_track_, (system_ & 0x10) ? 1 : 0,
                                                       static_cast<uint8_t>(reg_sector_ - 1), disk_buf_.data())) {
                                exit_via = Exit::OP_CONTSUM;
                                break;
                            }
                            if (reg_command_ & 0x10) {
                                exit_via = continue_write_multi();
                            } else {
                                exit_via = Exit::OP_OK;
                            }
                        }
                        break;
                    }
                    case 0xF: { // Запись данных команды "Запись дорожки" — не задействуется этими ПЗУ
                        reg_data_ = data_io_;
                        dsk_count_dat_lost_ = 0x10;
                        cnt_data_++;
                        if (cnt_data_ >= dsk_data_size_) {
                            exit_via = Exit::OP_OK;
                        }
                        break;
                    }
                    default: break;
                }
            } else {
                reg_data_ = data_io_;
            }
            break;
        }
        case 0x8: { // Чтение сигналов Intrq и Drq (не задействовано текущей раскладкой портов Корвета,
                    // оставлено для полноты порта floppy.c)
            uint8_t intrq_drq = 0xBF;
            if (reg_status_ & 0x01) intrq_drq = 0x3F;
            if (reg_status_ & 0x02) intrq_drq = 0x7F;
            data_io_ = intrq_drq;
            break;
        }
        case 0x9: { // Запись системного регистра контроллера дисководов
            if ((reg_status_ & 0x01) && (reg_command_ & 0x80)) { exit_via = Exit::OP_CONTSUM; break; }
            system_ = data_io_;
            if ((system_ ^ dsk_status_) & 0x03) {
                disk_uninit();
                disk_init();
            }
            break;
        }
        case 0xA: { // Инициализация диска
            disk_init();
            break;
        }
        case 0xB: { // Снятие диска
            disk_uninit();
            break;
        }
        case 0xC: { // Reset ВГ93
            reg_status_ = 0x24;
            reg_command_ = 0x00;
            reg_track_ = 0x00;
            reg_sector_ = 0x01;
            reg_data_ = 0x00;
            system_ = 0x3C;
            step_direct_ = -1; // 0xFF как signed = -1
            // Сброс гасит и отложенную операцию (BUSY снят).
            pending_active_ = false;
            pending_op_ = fdc_pending_op_t{};
            if ((dsk_status_ & 0x80) && (dsk_status_ & 0x03)) {
                disk_uninit();
                disk_init();
            }
            break;
        }
        default: break;
    }

    apply_exit(exit_via);
}

// ---------------------------------------------------------------------------
// Порты — floppy.c::FDC_Read/FDC_Write/FDC_Read_DRVREG/FDC_Write_DRVREG
// ---------------------------------------------------------------------------
uint8_t Fdc::read(uint8_t port) {
    uint8_t oper_io;
    switch (port & 0x03) {
        case 0: oper_io = 0x0; break;
        case 1: oper_io = 0x2; break;
        case 2: oper_io = 0x4; break;
        default: oper_io = 0x6; break;
    }
    dsk_vg(oper_io);
    return data_io_;
}

void Fdc::write(uint8_t port, uint8_t data) {
    data_io_ = data;
    uint8_t oper_io;
    switch (port & 0x03) {
        case 0: oper_io = 0x1; break;
        case 1: oper_io = 0x3; break;
        case 2: oper_io = 0x5; break;
        default: oper_io = 0x7; break;
    }
    dsk_vg(oper_io);
}

uint8_t Fdc::read_drvreg() const {
    return raw_drvreg_;
}

void Fdc::write_drvreg(uint8_t value) {
    raw_drvreg_ = value;
    uint8_t raw_drive = value & 0x0F;
    uint8_t real_drive = DRV_TABLE[raw_drive];
    data_io_ = (value & 0xF0) | real_drive;


    dsk_vg(0x9);

    // Отражаем в служебные поля для GUI/мониторинга (не влияет на dsk_vg()).
    current_drive_ = system_ & 0x03;
    current_side_  = (system_ & DRVREG_SIDE) ? 1 : 0;
    motor_on_      = (value & DRVREG_MOTOR) != 0;
}

Fdc::FdcStatus Fdc::get_status() const {
    FdcStatus s{};
    for (int i = 0; i < 4; i++) {
        s.drives[i] = adapter_ && adapter_->is_present(static_cast<uint8_t>(i));
        s.motors_on[i] = (i == current_drive_) && motor_on_;
        s.current_tracks[i] = track_real_[i];
        s.current_sectors[i] = (i == current_drive_) ? reg_sector_ : 0;
        s.write_protected[i] = is_write_protected(static_cast<uint8_t>(i));
    }
    s.selected_drive = system_ & 0x03;
    s.command_status = reg_status_;
    s.read_active = (reg_status_ & 0x01) && ((reg_command_ >> 4) == 0x8 || (reg_command_ >> 4) == 0x9);
    s.write_active = (reg_status_ & 0x01) && ((reg_command_ >> 4) == 0xA || (reg_command_ >> 4) == 0xB);
    s.seek_active = pending_active_ &&
        (pending_op_.type == FDC_OP_RESTORE || pending_op_.type == FDC_OP_SEEK ||
         pending_op_.type == FDC_OP_STEP || pending_op_.type == FDC_OP_STEP_IN ||
         pending_op_.type == FDC_OP_STEP_OUT);
    s.idle = !(reg_status_ & 0x01);
    s.sectors_read = 0;
    s.sectors_written = 0;
    s.pending_op_active = pending_active_;
    s.pending_op = pending_active_ ? pending_op_ : fdc_pending_op_t{};
    return s;
}

}  // namespace korvet
