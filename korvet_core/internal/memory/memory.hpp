#pragma once

#include <cstdint>
#include <array>
#include <string>

namespace korvet {

class Video;
class Fdc;
class Cpu;
class Ppi;
class Serial;
class Lan;
class Timer;
class Keyboard;
class Pic;

struct mb_config_t;

class Memory {
public:
    static constexpr uint32_t RAM_SIZE = 64 * 1024;  // Changed to uint32_t to prevent overflow
    static constexpr uint32_t ROM_SIZE = 24 * 1024;
    
    void init(const mb_config_t* config, Video* video, Fdc* fdc);
    void reset();
    void set_rom(const uint8_t* data, size_t size);
    void set_mapper(const uint8_t* data, size_t size);
    
    uint8_t read(uint16_t addr);
    void write(uint16_t addr, uint8_t data);
    uint16_t read_word(uint16_t addr);
    void write_word(uint16_t addr, uint16_t data);

    /// Чтение «без побочных эффектов» — тот же маршрут через маппер,
    /// что и read(), но для страниц портов периферии (M_PORTBASE)
    /// возвращается 0xFF: реальное чтение порта меняет состояние
    /// устройств (статус ВГ93, защёлки таймера, PIC). Для отладчика,
    /// трассировки и дизассемблирования «живой» памяти i8080.
    /// Страницы RAM/ROM/ACZU/GZU/KEYBOARD читаются честно (их чтение
    /// пассивно); ACZU читается напрямую из массива (без установки
    /// флага attr_written, который ставит aczu_read).
    uint8_t peek(uint16_t addr) const;

    void set_sysreg(uint8_t value);
    uint8_t get_sysreg() const { return sysreg_; }
    
    uint8_t* get_ram() { return ram_.data(); }
    const uint8_t* get_ram() const { return ram_.data(); }
    const uint8_t* get_rom() const { return rom_.data(); }
    size_t get_rom_size() const { return rom_.size(); }

    /// Доступ к ROM для записи (горячая замена/патч образа ПЗУ со стороны
    /// хоста). Размер — get_rom_size() (24 КБ; образы меньше — поверх,
    /// хвост заполняется нулями через set_rom()).
    uint8_t* get_rom_writable() { return rom_.data(); }
    const auto& get_mapper() const { return mem_mapper_; }
    size_t get_mapper_size() const { return sizeof(mem_mapper_); }
    
    // Для отладки
    void set_breakpoint(uint16_t addr, bool enabled);
    bool get_breakpoint(uint16_t addr) const;
    
    // Сеттеры для подключения компонентов
    void set_cpu(Cpu* cpu) { cpu_ = cpu; }
    void set_video(Video* video) { video_ = video; }
    void set_fdc(Fdc* fdc) { fdc_ = fdc; }
    void set_ppi(Ppi* ppi) { ppi_ = ppi; }
    void set_serial(Serial* serial) { serial_ = serial; }
    void set_lan(Lan* lan) { lan_ = lan; }
    void set_timer(Timer* timer) { timer_ = timer; }
    void set_keyboard(Keyboard* keyboard) { keyboard_ = keyboard; }
    void set_pic(Pic* pic) { pic_ = pic; }

private:
    std::array<uint8_t, RAM_SIZE> ram_;
    std::array<uint8_t, ROM_SIZE> rom_;
    uint8_t sysreg_;
    
    // Mapper: 32 регистра × 256 страниц
    std::array<std::array<uint8_t, 256>, 32> mem_mapper_;
    
    // Состояние LED клавиатуры (выбор раскладки)

    // Подключения компонентов (default-инициализаторы — W-7: раньше
    // Memory::init() обнулял lan_ ПОСЛЕ connect_components() из-за
    // порядка вызовов в Motherboard::init(), и маршрутизация портов
    // 0x20/0x21 ЛВС была «мёртвой» для гостевого ПО — находка
    // верификации гостевым кодом (periph_guest_test).
    Video* video_ = nullptr;
    Fdc* fdc_ = nullptr;
    Cpu* cpu_ = nullptr;
    Ppi* ppi_ = nullptr;
    Serial* serial_ = nullptr;
    Lan* lan_ = nullptr;
    Timer* timer_ = nullptr;
    Keyboard* keyboard_ = nullptr;
    Pic* pic_ = nullptr;
    
    uint8_t map_address(uint16_t addr);
    void handle_peripheral_write(uint16_t addr, uint8_t data);
    uint8_t handle_peripheral_read(uint16_t addr);
    
    // Для отладки
    std::array<uint8_t, RAM_SIZE> breakpoints_;
    
    // Типы памяти для маппера
    enum MemoryType {
        M_RAM = 0,
        M_ROM0 = 1,
        M_ROM1 = 2,
        M_ROM2 = 3,
        M_KEYBOARD = 4,
        M_PORTBASE = 5,
        M_REGBASE = 6,
        M_ACZU = 7,
        M_GZU = 8
    };
};

}  // namespace korvet