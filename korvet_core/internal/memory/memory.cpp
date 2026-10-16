/**
 * @file memory.cpp
 * @brief Реализация модуля памяти эмулятора Korvet
 * 
 * Содержит реализацию класса Memory, который отвечает за эмуляцию
 * системы памяти персонального компьютера ПК8020 "Корвет".
 * 
 * @author Original authors
 * @date 2024
 */

#include "memory.hpp"
#include "../../motherboard_types.hpp"
#include "../video/video.hpp"
#include "../fdc/fdc.hpp"
#include "../timer/timer.hpp"
#include "../ppi/ppi.hpp"
#include "../serial/serial.hpp"
#include "../lan/lan.hpp"
#include "../pic/pic.hpp"
#include "../keyboard/keyboard.hpp"
#include "../logger/logger.hpp"
#include "../cpu/cpu.hpp"  // ФИКС 3: для get_pc() в set_sysreg trace


namespace korvet {

void Memory::init(const mb_config_t* config, Video* video, Fdc* fdc) {
    video_ = video;
    fdc_ = fdc;
    
    // Инициализация RAM нулями
    for (auto& byte : ram_) {
        byte = 0;
    }
    
    // Инициализация ROM нулями
    for (auto& byte : rom_) {
        byte = 0;
    }
    
    // Инициализация маппера
    for (auto& reg : mem_mapper_) {
        for (int i = 0; i < 256; i++) {
            reg[i] = 0;  // Default to RAM
        }
    }
    
    // Инициализация точек останова
    breakpoints_.fill(0);

    sysreg_ = 0;
    // W-7: lan_ БОЛЬШЕ не обнуляется здесь — connect_components()
    // вызывается ДО memory_.init() в Motherboard::init(), и эта строка
    // убивала маршрутизацию портов ЛВС (0x20/0x21) для всего гостевого
    // ПО. Указатели компонентов имеют default-инициализаторы в .hpp.
}

void Memory::reset() {

    // Сброс состояния памяти
    // при сбросе эмулятора память не стирается "автоматически", это делает БИОС
    // for (auto& byte : ram_) {
    //     byte = 0;
    // }
    
    // Reset breakpoints
    // нет смысла стирать брейки, может помешать дебагу
    // breakpoints_.fill(0);
    
    sysreg_ = 0;
}

void Memory::set_rom(const uint8_t* data, size_t size) {
    if (!data) {
        LOG_WARN("Cannot load ROM: data is null");
        return;
    }
    size_t copy_size = (size < ROM_SIZE) ? size : ROM_SIZE;
    std::copy(data, data + copy_size, rom_.data());
    for (size_t i = copy_size; i < ROM_SIZE; i++) {
        rom_[i] = 0;
    }
}

// не заполнять маппер нулями по умолчанию
// void Memory::set_mapper(const uint8_t* data, size_t size) {
//     if (!data) return;
//     size_t total = 32 * 256;
//     size_t copy_size = (size < total) ? size : total;
//     const uint8_t* src = data;
//     for (int reg = 0; reg < 32; reg++) {
//         size_t chunk = (copy_size > 256) ? 256 : copy_size;
//         std::copy(src, src + chunk, mem_mapper_[reg].data());
//         // удалить: for (size_t i = chunk; i < 256; i++) mem_mapper_[reg][i] = 0;
//         src += chunk;
//         copy_size -= chunk;
//     }
// }


// маппер заполнен нулями по умолчанию для защиты от мусора, данные из файла накладываются поверх
void Memory::set_mapper(const uint8_t* data, size_t size) {
    if (!data) return;
    size_t total = 32 * 256;
    size_t copy_size = (size < total) ? size : total;
    const uint8_t* src = data;
    for (int reg = 0; reg < 32; reg++) {
        size_t chunk = (copy_size > 256) ? 256 : copy_size;
        std::copy(src, src + chunk, mem_mapper_[reg].data());
        for (size_t i = chunk; i < 256; i++) {
            mem_mapper_[reg][i] = 0;
        }
        src += chunk;
        copy_size -= chunk;
    }
}

uint8_t Memory::read(uint16_t addr) {
    uint8_t page = mem_mapper_[sysreg_][(addr & 0xFF00) >> 8];
    uint8_t value = 0;

    switch (page) {
        case M_RAM:
            value = ram_[addr];
            break;

        case M_ROM0:
        case M_ROM1:
        case M_ROM2:
            if (addr < ROM_SIZE) {
                value = rom_[addr];
            } else {
                LOG_WARN("Memory: ROM read out of bounds at addr=0x%04X", addr);
                value = 0xFF;
            }
            break;

        case M_KEYBOARD:
            if (keyboard_) {
                value = keyboard_->read(addr);
            } else {
                value = 0xFF;
            }
            break;

        case M_PORTBASE:
            value = handle_peripheral_read(addr);
            break;

        case M_REGBASE:
            // Оригинал: Value=RAM[Addres&0xffff] — чтение идёт от шины памяти
            value = ram_[addr];
            break;

        case M_ACZU:
            if (video_) {
                value = video_->aczu_read(addr & 0x3FF);
            } else {
                value = 0xFF;
            }
            break;

        case M_GZU:
            if (video_) {
                value = video_->gzu_read(addr & 0x3FFF);
            } else {
                value = 0xFF;
            }
            break;

        default:
            value = ram_[addr];
            break;
    }

    return value;
}

uint8_t Memory::peek(uint16_t addr) const {
    // Тот же маршрут, что и read(), но без побочных эффектов:
    // страницы портов никогда не опрашиваются (возвращаем 0xFF),
    // ACZU читается напрямую из массива (aczu_read() ставит флаг
    // aczu_attr_written_ — побочный эффект, недопустимый для трассы).
    uint8_t page = mem_mapper_[sysreg_][(addr & 0xFF00) >> 8];

    switch (page) {
        case M_RAM:
        case M_REGBASE:  // чтение REGBASE идёт от шины памяти (как в read())
            return ram_[addr];

        case M_ROM0:
        case M_ROM1:
        case M_ROM2:
            return (addr < ROM_SIZE) ? rom_[addr] : 0xFF;

        case M_KEYBOARD:
            // чтение матрицы пассивно (метод const)
            return keyboard_ ? keyboard_->read(addr) : 0xFF;

        case M_ACZU: {
            if (!video_) return 0xFF;
            // aczu_[i*2] = символ, aczu_[i*2+1] = атрибут — как пишет aczu_write()
            const uint8_t* aczu = video_->get_aczu_data();
            size_t sz = video_->get_aczu_size();
            size_t idx = static_cast<size_t>((addr & 0x3FF) << 1);
            return (idx < sz) ? aczu[idx] : 0xFF;
        }

        case M_GZU:
            // gzu_read() пассивен: только читает плоскости и NCREG
            return video_ ? video_->gzu_read(addr & 0x3FFF) : 0xFF;

        case M_PORTBASE:
        default:
            // Порты — не память: чтение имело бы побочные эффекты.
            return 0xFF;
    }
}

void Memory::write(uint16_t addr, uint8_t data) {
    uint8_t page = mem_mapper_[sysreg_][(addr & 0xFF00) >> 8];

    switch (page) {
        case M_RAM:
        case M_ROM0:  // Allow writing to ROM area if mapped as RAM
        case M_ROM1:
        case M_ROM2:
        case M_KEYBOARD:
            ram_[addr] = data;
            break;
            
        case M_PORTBASE:
            // Работа с портами ввода-вывода
            handle_peripheral_write(addr, data);
            break;
            
        case M_REGBASE: {
            // Оригинал memory.c:156-174 — три независимых условия по битам адреса
            if ((addr & (1 << 2)) == 0) {  // A2=0 → LUT (xxFB)
                if (video_) {
                    video_->lut_write(data);
                }
            }
            if ((addr & (1 << 6)) == 0) {  // A6=0 → NCREG (xBF)
                if (video_) {
                    video_->write_ncreg(data);
                }
            }
            if ((addr & (1 << 7)) == 0) {  // A7=0 → SYSREG (xx7F)
                set_sysreg(data);  // set_sysreg делает (data>>2)&0x1f
            }
            break;
        }
            
        case M_ACZU:
            if (video_) {
                video_->aczu_write(addr & 0x3FF, data);
            }
            break;
            
        case M_GZU:
            if (video_) {
                video_->gzu_write(addr & 0x3FFF, data);
            }
            break;
            
        default:
            // в эмуляторе @examples/original нет записи в память "по дефолту" -- может вызвать повреждения содержимого
            // ram_[addr] = data;
            
            break;
    }
}

uint16_t Memory::read_word(uint16_t addr) {
    uint8_t low = read(addr);
    uint8_t high = read(addr + 1);
    return (high << 8) | low;
}

void Memory::write_word(uint16_t addr, uint16_t data) {
    write(addr, data & 0xFF);
    write(addr + 1, (data >> 8) & 0xFF);
}

void Memory::set_sysreg(uint8_t value) {
    sysreg_ = (value >> 2) & 0x1F;
}

void Memory::set_breakpoint(uint16_t addr, bool enabled) {
    breakpoints_[addr] = enabled ? 1 : 0;
}

bool Memory::get_breakpoint(uint16_t addr) const {
    return breakpoints_[addr] != 0;
}

uint8_t Memory::map_address(uint16_t addr) {
    return mem_mapper_[sysreg_][(addr & 0xFF00) >> 8];
}

void Memory::handle_peripheral_write(uint16_t addr, uint8_t data) {
    // Маршрутизация записи в периферию по адресу
    uint8_t port = addr & 0xFF;
    
    switch ((port >> 3) & 0x07) {
        case 0:  // Timer
            if (timer_) timer_->write(port, data);
            break;
        case 1:  // PPI3
            if (ppi_) ppi_->write_port(2, port & 0x03, data);
            break;
        case 2:  // Serial/RS232: 0x10 — данные, 0x11 — управление
            if (serial_) serial_->write(port, data);
            break;
        case 3:  // FDC
            if (fdc_) fdc_->write(port, data);
            break;
        case 4:  // LAN: 0x20 — данные, 0x21 — управление/статус (бит A0)
            if (lan_) {
                lan_->write(port, data);
            }
            break;
        case 5:  // PIC
            if (pic_) pic_->write(port, data);
            break;
        case 6:  // PPI2
            if (ppi_) ppi_->write_port(1, port & 0x03, data);
            break;
        case 7:  // PPI1
            if (ppi_) ppi_->write_port(0, port & 0x03, data);
            break;
    }
}

uint8_t Memory::handle_peripheral_read(uint16_t addr) {
    // Маршрутизация чтения периферии по адресу
    uint8_t port = addr & 0xFF;
    
    switch ((port >> 3) & 0x07) {
        case 0:  // Timer
            if (timer_) return timer_->read(port);
            break;
        case 1:  // PPI3
            if (ppi_) return ppi_->read_port(2, port & 0x03);
            break;
        case 2:  // Serial/RS232: 0x10 — данные, 0x11 — статус
            if (serial_) return serial_->read(port);
            break;
        case 3:  // FDC
            if (fdc_) return fdc_->read(port);
            break;
        case 4:  // LAN: 0x20 — данные, 0x21 — управление/статус (бит A0)
            if (lan_) {
                return lan_->read(port);
            }
            break;
        case 5:  // PIC
            if (pic_) return pic_->read(port);
            break;
        case 6:  // PPI2
            if (ppi_) return ppi_->read_port(1, port & 0x03);
            break;
        case 7:  // PPI1
            if (ppi_) return ppi_->read_port(0, port & 0x03);
            break;
    }
    
    // LOG_WARN("Memory: unhandled peripheral read at port 0x%02X (PC will get 0xFF)", port);
    return 0xFF;  // Default value for unhandled reads
}

}  // namespace korvet