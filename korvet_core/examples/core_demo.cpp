#include "korvet_core/motherboard.hpp"
#include "korvet_core/internal/logger/logger.hpp"
#include <iostream>
#include <memory>

int main(int argc, char** argv) {
    LOG_INFO("Korvet Emulator Core - Refactored Version");
    
    try {
        // Singleton материнской платы
        auto& mb = korvet::Motherboard::get_instance();
        
        // Базовая конфигурация
        korvet::mb_config_t config = {};
        config.rom_path = "data/rom.rom";
        config.font_path = "data/korvet.fnt";
        config.mapper_path = "data/mapper.mem";
        config.disk_paths[0] = "data/disk1.kdi";
        config.disk_paths[1] = "data/disk2.kdi";
        config.disk_paths[2] = "data/disk3.kdi";
        config.disk_paths[3] = "data/disk4.kdi";
        config.tape_path = "data/cassette.kas";
        
        config.fdc_enabled = true;
        config.gzu_pages = 4;
        config.lan_addr = 1;
        
        config.emulation_speed = 100;
        config.sound_enabled = true;
        config.keyboard_layout = 2;  // AUTO
        
        config.osd_lut = true;
        config.osd_fps = true;
        config.osd_fdd = true;
        config.osd_cas = true;
        
        // Инициализация материнской платы
        if (!mb.init(&config)) {
            LOG_ERROR("Failed to initialize motherboard");
            return 1;
        }
        
        LOG_INFO("Motherboard initialized successfully");
        
        // Включение питания
        mb.power_on();
        LOG_INFO("System powered on");
        
        // Несколько циклов для проверки
        for (int i = 0; i < 1000; ++i) {
            mb.step();
        }
        
        LOG_INFO("Executed 1000 CPU cycles");
        
        // Состояние CPU для контроля
        korvet::cpu_state_t cpu_state = mb.cpu_get_state();
        LOG_INFO("CPU PC: 0x%04X", cpu_state.pc);
        
        // Завершение работы
        mb.shutdown();
        LOG_INFO("System shutdown completed");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception occurred: %s", e.what());
        return 1;
    }
    
    return 0;
}