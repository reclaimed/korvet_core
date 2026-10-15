/**
 * @file timer_dump.cpp
 * @brief Реализация функций дампа состояния таймера эмулятора
 * 
 * Содержит реализацию функций для создания дампов состояния
 * таймера эмулятора ПК8020 "Корвет" в файлы для отладки.
 * 
 * @author Korvet Team
 * @date 2024
 */

#include "timer_dump.hpp"
#include <cstdio>
#include <ctime>
#include <cstdarg>

namespace korvet {

bool TimerDump::dump_binary(const Timer& timer, const char* filename) {
    // Получение состояния таймера для дампа
    FILE* file = fopen(filename, "wb");
    if (!file) {
        return false;
    }
    
    // Пока — упрощённый бинарный дамп
    // Полная реализация сериализовала бы реальное состояние таймера
    uint8_t timer_state[256]; // Placeholder size
    memset(timer_state, 0, sizeof(timer_state));
    
    // Заполняем данными таймера (заглушечная реализация)
    // В полной реализации — реальное состояние таймера
    size_t written = fwrite(timer_state, 1, sizeof(timer_state), file);
    fclose(file);
    
    return written == sizeof(timer_state);
}

bool TimerDump::dump_binary_unique(const Timer& timer, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_timer_%04d%02d%02d_%02d%02d%02d.bin",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_binary(timer, unique_filename);
}

bool TimerDump::dump_text(const Timer& timer, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        return false;
    }
    
    fprintf(file, "Timer State Dump\n");
    fprintf(file, "================\n");
    fprintf(file, "Timestamp: %ld\n", std::time(nullptr));
    fprintf(file, "\n");
    
    // Канал 0
    fprintf(file, "Channel 0:\n");
    fprintf(file, "  Count: %d\n", timer.get_channel_count(0));
    fprintf(file, "  Mode: %d\n", timer.get_channel_mode(0));
    fprintf(file, "  Status: %s\n", timer.is_channel_active(0) ? "ACTIVE" : "INACTIVE");
    fprintf(file, "  Output: %s\n", timer.get_channel_output(0) ? "HIGH" : "LOW");
    fprintf(file, "\n");
    
    // Канал 1
    fprintf(file, "Channel 1:\n");
    fprintf(file, "  Count: %d\n", timer.get_channel_count(1));
    fprintf(file, "  Mode: %d\n", timer.get_channel_mode(1));
    fprintf(file, "  Status: %s\n", timer.is_channel_active(1) ? "ACTIVE" : "INACTIVE");
    fprintf(file, "  Output: %s\n", timer.get_channel_output(1) ? "HIGH" : "LOW");
    fprintf(file, "\n");
    
    // Канал 2
    fprintf(file, "Channel 2:\n");
    fprintf(file, "  Count: %d\n", timer.get_channel_count(2));
    fprintf(file, "  Mode: %d\n", timer.get_channel_mode(2));
    fprintf(file, "  Status: %s\n", timer.is_channel_active(2) ? "ACTIVE" : "INACTIVE");
    fprintf(file, "  Output: %s\n", timer.get_channel_output(2) ? "HIGH" : "LOW");
    fprintf(file, "\n");
    
    // Управление таймером
    fprintf(file, "Timer Control:\n");
    fprintf(file, "  Control Word: 0x%02X\n", timer.get_control_word());
    fprintf(file, "  Status: %s\n", timer.is_enabled() ? "ENABLED" : "DISABLED");
    fprintf(file, "  Frequency: %.2f Hz\n", timer.get_frequency());
    fprintf(file, "\n");
    
    // Аудио
    fprintf(file, "Audio Generator:\n");
    fprintf(file, "  Audio Enabled: %s\n", timer.is_audio_enabled() ? "YES" : "NO");
    fprintf(file, "  Sample Rate: %d Hz\n", timer.get_audio_sample_rate());
    fprintf(file, "  PWM Frequency: %.2f Hz\n", timer.get_pwm_frequency());
    fprintf(file, "  Current PWM Value: %d\n", timer.get_current_pwm_value());
    fprintf(file, "\n");
    
    // Прерывания
    fprintf(file, "Interrupts:\n");
    fprintf(file, "  Interrupt Enabled: %s\n", timer.is_interrupt_enabled() ? "YES" : "NO");
    fprintf(file, "  Interrupt Pending: %s\n", timer.is_interrupt_pending() ? "YES" : "NO");
    fprintf(file, "  IRQ Vector: 0x%02X\n", timer.get_irq_vector());
    
    fclose(file);
    return true;
}

bool TimerDump::dump_text_unique(const Timer& timer, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_timer_%04d%02d%02d_%02d%02d%02d.txt",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_text(timer, unique_filename);
}

bool TimerDump::dump_full(const Timer& timer, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        return false;
    }
    
    fprintf(file, "Full Timer State Dump\n");
    fprintf(file, "=====================\n");
    fprintf(file, "Timestamp: %ld\n", std::time(nullptr));
    fprintf(file, "\n");
    
    // Подробно по каналам
    for (int i = 0; i < 3; i++) {
        fprintf(file, "Channel %d:\n", i);
        fprintf(file, "  Count: %d\n", timer.get_channel_count(i));
        fprintf(file, "  Initial Count: %d\n", timer.get_channel_initial_count(i));
        fprintf(file, "  Mode: %d", timer.get_channel_mode(i));
        switch (timer.get_channel_mode(i)) {
            case 0: fprintf(file, " (Interrupt on Terminal Count)"); break;
            case 1: fprintf(file, " (Hardware Retriggerable One-Shot)"); break;
            case 2: fprintf(file, " (Rate Generator)"); break;
            case 3: fprintf(file, " (Square Wave Generator)"); break;
            case 4: fprintf(file, " (Software Triggered Strobe)"); break;
            case 5: fprintf(file, " (Hardware Triggered Strobe)"); break;
            default: fprintf(file, " (Unknown)"); break;
        }
        fprintf(file, "\n");
        
        fprintf(file, "  Binary Counter: %s\n", timer.is_channel_binary(i) ? "YES" : "NO");
        fprintf(file, "  Status: %s\n", timer.is_channel_active(i) ? "ACTIVE" : "INACTIVE");
        fprintf(file, "  Output: %s\n", timer.get_channel_output(i) ? "HIGH" : "LOW");
        fprintf(file, "  Counting: %s\n", timer.is_channel_counting(i) ? "YES" : "NO");
        fprintf(file, "  Gate: %s\n", timer.get_channel_gate(i) ? "HIGH" : "LOW");
        fprintf(file, "\n");
    }
    
    // Регистр управления
    fprintf(file, "Control Register:\n");
    fprintf(file, "  Control Word: 0x%02X\n", timer.get_control_word());
    fprintf(file, "  RW Mode: %d\n", timer.get_rw_mode());
    fprintf(file, "  Counter Selection: %d\n", timer.get_counter_selection());
    fprintf(file, "  Operating Mode: %d\n", timer.get_operating_mode());
    fprintf(file, "  BCD Mode: %s\n", timer.is_bcd_mode() ? "YES" : "NO");
    fprintf(file, "\n");
    
    // Рабочее состояние таймера
    fprintf(file, "Operational State:\n");
    fprintf(file, "  Timer Enabled: %s\n", timer.is_enabled() ? "YES" : "NO");
    fprintf(file, "  Frequency: %.2f Hz\n", timer.get_frequency());
    fprintf(file, "  Period: %.4f ms\n", 1000.0 / timer.get_frequency());
    fprintf(file, "  Elapsed Time: %.4f ms\n", timer.get_elapsed_time_ms());
    fprintf(file, "\n");
    
    // Состояние аудиогенератора
    fprintf(file, "Audio Generator State:\n");
    fprintf(file, "  Audio Enabled: %s\n", timer.is_audio_enabled() ? "YES" : "NO");
    fprintf(file, "  Sample Rate: %d Hz\n", timer.get_audio_sample_rate());
    fprintf(file, "  PWM Frequency: %.2f Hz\n", timer.get_pwm_frequency());
    fprintf(file, "  Current PWM Value: %d\n", timer.get_current_pwm_value());
    fprintf(file, "  PWM Duty Cycle: %.2f%%\n", timer.get_pwm_duty_cycle() * 100.0);
    fprintf(file, "  High Quality PWM: %s\n", timer.is_high_quality_pwm() ? "YES" : "NO");
    fprintf(file, "\n");
    
    // Состояние прерываний
    fprintf(file, "Interrupt State:\n");
    fprintf(file, "  Interrupt Enabled: %s\n", timer.is_interrupt_enabled() ? "YES" : "NO");
    fprintf(file, "  Interrupt Pending: %s\n", timer.is_interrupt_pending() ? "YES" : "NO");
    fprintf(file, "  IRQ Vector: 0x%02X\n", timer.get_irq_vector());
    fprintf(file, "  PIC Connected: %s\n", timer.has_pic_connection() ? "YES" : "NO");
    fprintf(file, "\n");
    
    // Статистика
    fprintf(file, "Statistics:\n");
    fprintf(file, "  Total Timer Ticks: %llu\n", timer.get_total_ticks());
    fprintf(file, "  Total Interrupts Generated: %llu\n", timer.get_total_interrupts());
    fprintf(file, "  Total PWM Samples Generated: %llu\n", timer.get_total_pwm_samples());
    
    fclose(file);
    return true;
}

bool TimerDump::dump_full_unique(const Timer& timer, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_timer_full_%04d%02d%02d_%02d%02d%02d.txt",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_full(timer, unique_filename);
}

bool TimerDump::dump_channels(const Timer& timer, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        return false;
    }
    
    fprintf(file, "Timer Channels State Dump\n");
    fprintf(file, "=========================\n");
    fprintf(file, "Timestamp: %ld\n", std::time(nullptr));
    fprintf(file, "\n");
    
    for (int i = 0; i < 3; i++) {
        fprintf(file, "Channel %d:\n", i);
        fprintf(file, "  Initial Count: %d\n", timer.get_channel_initial_count(i));
        fprintf(file, "  Current Count: %d\n", timer.get_channel_count(i));
        fprintf(file, "  Mode: %d", timer.get_channel_mode(i));
        switch (timer.get_channel_mode(i)) {
            case 0: fprintf(file, " (Interrupt on Terminal Count)"); break;
            case 1: fprintf(file, " (Hardware Retriggerable One-Shot)"); break;
            case 2: fprintf(file, " (Rate Generator)"); break;
            case 3: fprintf(file, " (Square Wave Generator)"); break;
            case 4: fprintf(file, " (Software Triggered Strobe)"); break;
            case 5: fprintf(file, " (Hardware Triggered Strobe)"); break;
            default: fprintf(file, " (Unknown)"); break;
        }
        fprintf(file, "\n");
        
        fprintf(file, "  Binary Counter: %s\n", timer.is_channel_binary(i) ? "YES" : "NO");
        fprintf(file, "  Status: %s\n", timer.is_channel_active(i) ? "ACTIVE" : "INACTIVE");
        fprintf(file, "  Output: %s\n", timer.get_channel_output(i) ? "HIGH" : "LOW");
        fprintf(file, "  Counting: %s\n", timer.is_channel_counting(i) ? "YES" : "NO");
        fprintf(file, "  Gate: %s\n", timer.get_channel_gate(i) ? "HIGH" : "LOW");
        fprintf(file, "  Elapsed Cycles: %llu\n", timer.get_channel_elapsed_cycles(i));
        fprintf(file, "  Next Event Cycles: %d\n", timer.get_channel_next_event_cycles(i));
        fprintf(file, "\n");
    }
    
    fclose(file);
    return true;
}

bool TimerDump::dump_channels_unique(const Timer& timer, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_timer_channels_%04d%02d%02d_%02d%02d%02d.txt",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_channels(timer, unique_filename);
}

bool TimerDump::dump_audio_generator(const Timer& timer, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        return false;
    }
    
    fprintf(file, "Timer Audio Generator State Dump\n");
    fprintf(file, "==================================\n");
    fprintf(file, "Timestamp: %ld\n", std::time(nullptr));
    fprintf(file, "\n");
    
    fprintf(file, "Audio Configuration:\n");
    fprintf(file, "  Audio Enabled: %s\n", timer.is_audio_enabled() ? "YES" : "NO");
    fprintf(file, "  Sample Rate: %d Hz\n", timer.get_audio_sample_rate());
    fprintf(file, "  Bit Depth: %d bits\n", timer.get_audio_bit_depth());
    fprintf(file, "  Channels: %d\n", timer.get_audio_channels());
    fprintf(file, "  High Quality PWM: %s\n", timer.is_high_quality_pwm() ? "YES" : "NO");
    fprintf(file, "\n");
    
    fprintf(file, "PWM Configuration:\n");
    fprintf(file, "  PWM Frequency: %.2f Hz\n", timer.get_pwm_frequency());
    fprintf(file, "  Current PWM Value: %d\n", timer.get_current_pwm_value());
    fprintf(file, "  PWM Duty Cycle: %.2f%%\n", timer.get_pwm_duty_cycle() * 100.0);
    fprintf(file, "  PWM Period: %.4f ms\n", 1000.0 / timer.get_pwm_frequency());
    fprintf(file, "\n");
    
    fprintf(file, "Timer-to-Audio Mapping:\n");
    fprintf(file, "  Channel Used: %d\n", timer.get_audio_source_channel());
    fprintf(file, "  Frequency Multiplier: %.2fx\n", timer.get_audio_frequency_multiplier());
    fprintf(file, "  Amplitude: %.2f\n", timer.get_audio_amplitude());
    fprintf(file, "\n");
    
    fprintf(file, "Audio Output Statistics:\n");
    fprintf(file, "  Total Samples Generated: %llu\n", timer.get_total_pwm_samples());
    fprintf(file, "  Current Buffer Position: %d\n", timer.get_audio_buffer_position());
    fprintf(file, "  Buffer Size: %d\n", timer.get_audio_buffer_size());
    
    fclose(file);
    return true;
}

bool TimerDump::dump_audio_generator_unique(const Timer& timer, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_timer_audio_%04d%02d%02d_%02d%02d%02d.txt",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_audio_generator(timer, unique_filename);
}

bool TimerDump::dump_interrupts(const Timer& timer, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        return false;
    }
    
    fprintf(file, "Timer Interrupt State Dump\n");
    fprintf(file, "============================\n");
    fprintf(file, "Timestamp: %ld\n", std::time(nullptr));
    fprintf(file, "\n");
    
    fprintf(file, "Interrupt Configuration:\n");
    fprintf(file, "  Interrupt Enabled: %s\n", timer.is_interrupt_enabled() ? "YES" : "NO");
    fprintf(file, "  Interrupt Pending: %s\n", timer.is_interrupt_pending() ? "YES" : "NO");
    fprintf(file, "  IRQ Vector: 0x%02X\n", timer.get_irq_vector());
    fprintf(file, "  PIC Connected: %s\n", timer.has_pic_connection() ? "YES" : "NO");
    fprintf(file, "\n");
    
    fprintf(file, "Channel Interrupt States:\n");
    for (int i = 0; i < 3; i++) {
        fprintf(file, "  Channel %d:\n", i);
        fprintf(file, "    Interrupt Enabled: %s\n", timer.is_channel_interrupt_enabled(i) ? "YES" : "NO");
        fprintf(file, "    Interrupt Pending: %s\n", timer.is_channel_interrupt_pending(i) ? "YES" : "NO");
        fprintf(file, "    Triggered: %s\n", timer.did_channel_trigger_interrupt(i) ? "YES" : "NO");
        fprintf(file, "    Next Interrupt: %d cycles\n", timer.get_channel_next_interrupt_cycles(i));
        fprintf(file, "\n");
    }
    
    fprintf(file, "Interrupt Statistics:\n");
    fprintf(file, "  Total Interrupts Generated: %llu\n", timer.get_total_interrupts());
    fprintf(file, "  Total Timer Interrupt Requests: %llu\n", timer.get_total_timer_irqs());
    fprintf(file, "  Last Interrupt Time: %.4f ms\n", timer.get_last_interrupt_time_ms());
    fprintf(file, "  Average Interrupt Rate: %.2f Hz\n", timer.get_average_interrupt_rate_hz());
    
    fclose(file);
    return true;
}

bool TimerDump::dump_interrupts_unique(const Timer& timer, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_timer_ints_%04d%02d%02d_%02d%02d%02d.txt",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_interrupts(timer, unique_filename);
}

}  // namespace korvet