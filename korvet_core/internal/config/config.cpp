/**
 * @file config.cpp
 * @brief Реализация класса конфигурации эмулятора Korvet
 * 
 * Содержит реализацию методов класса Config, который управляет
 * изменяемой конфигурацией эмулятора ПК8020 "Корвет".
 * 
 * @author Korvet Team
 * @date 2024
 */

#include "config.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>

namespace korvet {

Config::Config() {
    reset_to_defaults();
}

bool Config::load_from_file(const char* filename) {
    if (!filename) {
        return false;
    }
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Убираем пробелы по краям и комментарии
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        
        // Пропускаем пустые строки
        if (line.empty()) {
            continue;
        }
        
        // Ищем символ '='
        size_t equals_pos = line.find('=');
        if (equals_pos == std::string::npos) {
            continue; // Invalid line format
        }
        
        std::string key = line.substr(0, equals_pos);
        std::string value = line.substr(equals_pos + 1);
        
        // Убираем пробелы
        key.erase(0, key.find_first_not_of(" \t\r\n"));
        key.erase(key.find_last_not_of(" \t\r\n") + 1);
        value.erase(0, value.find_first_not_of(" \t\r\n"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);
        
        // Разбираем пару ключ-значение
        if (key == "emulation_speed") {
            try {
                int speed = std::stoi(value);
                if (speed >= 1 && speed <= 1000) {
                    emulation_speed_ = speed;
                } else if (speed == -1) {
                    emulation_speed_ = -1;  // Maximum speed
                }
            } catch (...) {
                // Некорректное значение — оставляем default
            }
        } else if (key == "fdc_enabled") {
            fdc_enabled_ = (value == "1" || value == "true" || value == "yes");
        } else if (key == "gzu_pages") {
            try {
                int pages = std::stoi(value);
                if (pages >= 1 && pages <= 8) {
                    gzu_pages_ = static_cast<uint8_t>(pages);
                }
            } catch (...) {
                // Некорректное значение — оставляем default
            }
        } else if (key == "lan_addr") {
            try {
                int addr = std::stoi(value);
                if (addr >= 1 && addr <= 16) {
                    lan_addr_ = static_cast<uint8_t>(addr);
                }
            } catch (...) {
                // Некорректное значение — оставляем default
            }
        } else if (key == "sound_enabled") {
            sound_enabled_ = (value == "1" || value == "true" || value == "yes");
        } else if (key == "sound_volume") {
            try {
                int vol = std::stoi(value);
                if (vol >= 0 && vol <= 100) {
                    sound_volume_ = static_cast<uint8_t>(vol);
                }
            } catch (...) {
                // Некорректное значение — оставляем default
            }
        } else if (key == "sound_sample_rate") {
            try {
                uint32_t rate = static_cast<uint32_t>(std::stoi(value));
                if (rate >= 8000 && rate <= 192000) {
                    sound_sample_rate_ = rate;
                }
            } catch (...) {
                // Некорректное значение — оставляем default
            }
        } else if (key == "sound_frequency") {
            try {
                double freq = std::stod(value);
                if (freq >= 20.0 && freq <= 20000.0) {
                    sound_frequency_ = freq;
                }
            } catch (...) {
                // Некорректное значение — оставляем default
            }
        } else if (key == "sound_bit_depth") {
            try {
                int depth = std::stoi(value);
                if (depth == 8 || depth == 16) {
                    sound_bit_depth_ = static_cast<uint8_t>(depth);
                }
            } catch (...) {
                // Некорректное значение — оставляем default
            }
        } else if (key == "sound_channels") {
            try {
                int channels = std::stoi(value);
                if (channels >= 1 && channels <= 2) {
                    sound_channels_ = static_cast<uint8_t>(channels);
                }
            } catch (...) {
                // Некорректное значение — оставляем default
            }
        } else if (key == "sound_high_quality") {
            sound_high_quality_ = (value == "1" || value == "true" || value == "yes");
        } else if (key == "keyboard_layout") {
            try {
                int layout = std::stoi(value);
                if (layout >= 0 && layout <= 2) {
                    keyboard_layout_ = layout;
                }
            } catch (...) {
                // Некорректное значение — оставляем default
            }
        } else if (key == "kbd_led_initial_state") {
            try {
                int state = std::stoi(value);
                if (state >= 0 && state <= 255) {
                    kbd_led_initial_state_ = static_cast<uint8_t>(state);
                }
            } catch (...) {
                // Некорректное значение — оставляем default
            }
        } else if (key == "windowed") {
            windowed_ = (value == "1" || value == "true" || value == "yes");
        } else if (key == "osd_lut") {
            osd_lut_ = (value == "1" || value == "true" || value == "yes");
        } else if (key == "osd_fps") {
            osd_fps_ = (value == "1" || value == "true" || value == "yes");
        } else if (key == "osd_fdd") {
            osd_fdd_ = (value == "1" || value == "true" || value == "yes");
        } else if (key == "osd_cas") {
            osd_cas_ = (value == "1" || value == "true" || value == "yes");
        } else if (key == "debugger_on_start") {
            debugger_on_start_ = (value == "1" || value == "true" || value == "yes");
        } else if (key == "log_level") {
            try {
                int level = std::stoi(value);
                if (level >= 0 && level <= 4) {
                    log_level_ = static_cast<LogLevel>(level);
                }
            } catch (...) {
                // Некорректное значение — оставляем default
            }
        } else if (key == "log_enabled") {
            log_enabled_ = (value == "1" || value == "true" || value == "yes");
        } else if (key == "log_target") {
            log_target_ = value;
        } else if (key == "peripherals_polling_interval") {
            try {
                uint32_t interval = static_cast<uint32_t>(std::stoi(value));
                if (interval > 0) {
                    peripherals_polling_interval_ = interval;
                }
            } catch (...) {
                // Некорректное значение — оставляем default
            }
        } else if (key == "joystick_enabled") {
            joystick_enabled_ = (value == "1" || value == "true" || value == "yes");
        } else if (key == "joystick_type") {
            try {
                int type = std::stoi(value);
                if (type >= 0 && type <= 2) {
                    joystick_type_ = static_cast<uint8_t>(type);
                }
            } catch (...) {
                // Некорректное значение — оставляем default
            }
        } else if (key == "mouse_enabled") {
            mouse_enabled_ = (value == "1" || value == "true" || value == "yes");
        } else if (key == "mouse_sensitivity") {
            try {
                double sens = std::stod(value);
                if (sens >= 0.1 && sens <= 10.0) {
                    mouse_sensitivity_ = sens;
                }
            } catch (...) {
                // Некорректное значение — оставляем default
            }
        }
    }
    
    file.close();
    return true;
}

bool Config::save_to_file(const char* filename) const {
    if (!filename) {
        return false;
    }
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    file << "# Configuration file for Korvet emulator\n";
    file << "# Generated on " << std::time(nullptr) << "\n";
    file << "\n";
    
    // Настройки эмуляции
    file << "# Emulation settings\n";
    file << "emulation_speed=" << emulation_speed_ << "\n";
    file << "fdc_enabled=" << (fdc_enabled_ ? "1" : "0") << "\n";
    file << "gzu_pages=" << static_cast<int>(gzu_pages_) << "\n";
    file << "lan_addr=" << static_cast<int>(lan_addr_) << "\n";
    file << "\n";
    
    // Настройки звука
    file << "# Sound settings\n";
    file << "sound_enabled=" << (sound_enabled_ ? "1" : "0") << "\n";
    file << "sound_volume=" << static_cast<int>(sound_volume_) << "\n";
    file << "sound_sample_rate=" << sound_sample_rate_ << "\n";
    file << "sound_frequency=" << sound_frequency_ << "\n";
    file << "sound_bit_depth=" << static_cast<int>(sound_bit_depth_) << "\n";
    file << "sound_channels=" << static_cast<int>(sound_channels_) << "\n";
    file << "sound_high_quality=" << (sound_high_quality_ ? "1" : "0") << "\n";
    file << "\n";
    
    // Настройки клавиатуры
    file << "# Keyboard settings\n";
    file << "keyboard_layout=" << keyboard_layout_ << "\n";
    file << "kbd_led_initial_state=" << static_cast<int>(kbd_led_initial_state_) << "\n";
    file << "\n";
    
    // Настройки отображения
    file << "# Display settings\n";
    file << "windowed=" << (windowed_ ? "1" : "0") << "\n";
    file << "osd_lut=" << (osd_lut_ ? "1" : "0") << "\n";
    file << "osd_fps=" << (osd_fps_ ? "1" : "0") << "\n";
    file << "osd_fdd=" << (osd_fdd_ ? "1" : "0") << "\n";
    file << "osd_cas=" << (osd_cas_ ? "1" : "0") << "\n";
    file << "\n";
    
    // Настройки отладки
    file << "# Debug settings\n";
    file << "debugger_on_start=" << (debugger_on_start_ ? "1" : "0") << "\n";
    file << "\n";
    
    // Настройки логирования
    file << "# Logging settings\n";
    file << "log_level=" << static_cast<int>(log_level_) << "\n";
    file << "log_enabled=" << (log_enabled_ ? "1" : "0") << "\n";
    file << "log_target=" << log_target_ << "\n";
    file << "\n";
    
    // Настройки джойстика
    file << "# Joystick settings\n";
    file << "joystick_enabled=" << (joystick_enabled_ ? "1" : "0") << "\n";
    file << "joystick_type=" << static_cast<int>(joystick_type_) << "\n";
    file << "\n";
    
    // Настройки мыши
    file << "# Mouse settings\n";
    file << "mouse_enabled=" << (mouse_enabled_ ? "1" : "0") << "\n";
    file << "mouse_sensitivity=" << mouse_sensitivity_ << "\n";
    file << "\n";
    
    // Настройки опроса периферии
    file << "# Peripherals polling settings\n";
    file << "peripherals_polling_interval=" << peripherals_polling_interval_ << "\n";
    
    file.close();
    return true;
}

void Config::reset_to_defaults() {
    emulation_speed_ = 100;
    fdc_enabled_ = true;
    gzu_pages_ = 4;
    lan_addr_ = 1;
    
    sound_enabled_ = true;
    sound_volume_ = 50;
    sound_sample_rate_ = 44100;
    sound_frequency_ = 440.0;
    sound_bit_depth_ = 8;
    sound_channels_ = 1;
    sound_high_quality_ = false;
    
    keyboard_layout_ = 2;  // AUTO
    kbd_led_initial_state_ = 2;  // For AUTO mode
    
    windowed_ = true;
    osd_lut_ = true;
    osd_fps_ = true;
    osd_fdd_ = true;
    osd_cas_ = true;
    
    debugger_on_start_ = false;
    
    log_level_ = LogLevel::INFO;
    log_enabled_ = true;
    log_target_ = "console";
    
    joystick_enabled_ = true;
    joystick_type_ = 0;  // Digital deflector
    
    mouse_enabled_ = true;
    mouse_sensitivity_ = 1.0;
    
    peripherals_polling_interval_ = 100;
}

bool Config::validate() const {
    // Валидация настроек эмуляции
    if (emulation_speed_ != -1 && (emulation_speed_ < 1 || emulation_speed_ > 1000)) {
        return false;
    }
    
    if (gzu_pages_ < 1 || gzu_pages_ > 8) {
        return false;
    }
    
    if (lan_addr_ < 1 || lan_addr_ > 16) {
        return false;
    }
    
    // Валидация настроек звука
    if (sound_volume_ > 100) {
        return false;
    }
    
    if (sound_sample_rate_ < 8000 || sound_sample_rate_ > 192000) {
        return false;
    }
    
    if (sound_frequency_ < 20.0 || sound_frequency_ > 20000.0) {
        return false;
    }
    
    if (sound_bit_depth_ != 8 && sound_bit_depth_ != 16) {
        return false;
    }
    
    if (sound_channels_ < 1 || sound_channels_ > 2) {
        return false;
    }
    
    // Валидация настроек клавиатуры
    if (keyboard_layout_ < 0 || keyboard_layout_ > 2) {
        return false;
    }
    
    // Валидация уровня лога
    if (static_cast<int>(log_level_) < 0 || static_cast<int>(log_level_) > 4) {
        return false;
    }
    
    // Валидация настроек джойстика
    if (joystick_type_ > 2) {
        return false;
    }
    
    // Валидация настроек мыши
    if (mouse_sensitivity_ < 0.1 || mouse_sensitivity_ > 10.0) {
        return false;
    }
    
    // Валидация интервала опроса периферии
    if (peripherals_polling_interval_ == 0) {
        return false;
    }
    
    return true;
}

}  // namespace korvet