/**
 * @file video_dump.cpp
 * @brief Реализация функций дампа состояния видеоподсистемы эмулятора
 * 
 * Содержит реализацию функций для создания дампов состояния
 * видеоподсистемы эмулятора ПК8020 "Корвет" в файлы для отладки.
 * 
 * @author Korvet Team
 * @date 2024
 */

#include "video_dump.hpp"
#include <cstdio>
#include <ctime>
#include <cstdarg>

namespace korvet {

bool VideoDump::dump_gzu_binary(const Video& video, const char* filename) {
    const uint8_t* gzu = video.get_gzu();
    FILE* file = fopen(filename, "wb");
    if (!file) {
        return false;
    }
    
    size_t gzu_size = video.get_gzu_size();
    size_t written = fwrite(gzu, 1, gzu_size, file);
    fclose(file);
    
    return written == gzu_size;
}

bool VideoDump::dump_gzu_binary_unique(const Video& video, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_gzu_%04d%02d%02d_%02d%02d%02d.bin",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_gzu_binary(video, unique_filename);
}

bool VideoDump::dump_aczu_binary(const Video& video, const char* filename) {
    const uint8_t* aczu = video.get_aczu();
    FILE* file = fopen(filename, "wb");
    if (!file) {
        return false;
    }
    
    size_t aczu_size = video.get_aczu_size();
    size_t written = fwrite(aczu, 1, aczu_size, file);
    fclose(file);
    
    return written == aczu_size;
}

bool VideoDump::dump_aczu_binary_unique(const Video& video, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_aczu_%04d%02d%02d_%02d%02d%02d.bin",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_aczu_binary(video, unique_filename);
}

bool VideoDump::dump_framebuffer_binary(const Video& video, const char* filename) {
    const uint8_t* framebuffer = video.get_framebuffer();
    FILE* file = fopen(filename, "wb");
    if (!file) {
        return false;
    }
    
    // Кадровый буфер: 512x256 пикселей, 4 бита на пиксель
    // Итого (512 * 256) / 2 = 65536 байт
    size_t fb_size = 65536; // 512 * 256 / 2
    size_t written = fwrite(framebuffer, 1, fb_size, file);
    fclose(file);
    
    return written == fb_size;
}

bool VideoDump::dump_framebuffer_binary_unique(const Video& video, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_framebuffer_%04d%02d%02d_%02d%02d%02d.bin",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_framebuffer_binary(video, unique_filename);
}

bool VideoDump::dump_video_registers_text(const Video& video, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        return false;
    }
    
    fprintf(file, "Video Registers State Dump\n");
    fprintf(file, "==========================\n");
    
    fprintf(file, "NCREG (Color Register): 0x%02X\n", video.get_ncreg());
    fprintf(file, "  COLORMD: %s\n", (video.get_ncreg() & 0x80) ? "COLOR" : "PLANE");
    fprintf(file, "  RDMSK: 0x%02X\n", (video.get_ncreg() >> 4) & 0x07);
    fprintf(file, "  WRMSK: 0x%02X\n", (video.get_ncreg() >> 1) & 0x07);
    fprintf(file, "  WBIT: %s\n", (video.get_ncreg() & 0x01) ? "SET" : "CLEAR");
    
    fprintf(file, "\nVIREG (Video Register): 0x%02X\n", video.get_vireg());
    fprintf(file, "  scr_Page_Access: %d\n", ((video.get_vireg() >> 6) & 0x03));
    fprintf(file, "  scr_Attr_Write: %d\n", ((video.get_vireg() >> 4) & 0x03));
    fprintf(file, "  scr_Wide_Mode: %s\n", (video.get_vireg() & 0x08) ? "ON" : "OFF");
    fprintf(file, "  scr_Second_Font: %s\n", (video.get_vireg() & 0x04) ? "ON" : "OFF");
    fprintf(file, "  scr_Page_Show: %d\n", (video.get_vireg() & 0x03));
    
    fprintf(file, "\nGZU Pages:\n");
    fprintf(file, "  Read Page (CPU Access): %d\n", video.get_gzu_read_page());
    fprintf(file, "  Show Page (Display): %d\n", video.get_gzu_show_page());
    
    fprintf(file, "\nACZU Settings:\n");
    fprintf(file, "  Attr_Write Mode: %d\n", video.get_aczu_attr_write());
    fprintf(file, "  Wide Mode: %s\n", video.get_aczu_wide_mode() ? "ON" : "OFF");
    fprintf(file, "  Second Font: %s\n", video.get_aczu_second_font() ? "ON" : "OFF");
    
    fclose(file);
    return true;
}

bool VideoDump::dump_video_registers_text_unique(const Video& video, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_video_regs_%04d%02d%02d_%02d%02d%02d.txt",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_video_registers_text(video, unique_filename);
}

bool VideoDump::dump_palette_binary(const Video& video, const char* filename) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        return false;
    }
    
    // Палитра: 16 цветов по 32 бита (RGBA)
    uint32_t palette[16];
    for (int i = 0; i < 16; i++) {
        palette[i] = video.get_lut_entry(i);
    }
    
    size_t written = fwrite(palette, sizeof(uint32_t), 16, file);
    fclose(file);
    
    return written == 16;
}

bool VideoDump::dump_palette_binary_unique(const Video& video, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_palette_%04d%02d%02d_%02d%02d%02d.bin",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_palette_binary(video, unique_filename);
}

bool VideoDump::dump_palette_text(const Video& video, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        return false;
    }
    
    fprintf(file, "Video Palette (LUT) State Dump\n");
    fprintf(file, "===============================\n");
    fprintf(file, "Index\tColor (RGBA)\tDescription\n");
    fprintf(file, "-----\t------------\t-----------\n");
    
    for (int i = 0; i < 16; i++) {
        uint32_t color = video.get_lut_entry(i);
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        
        fprintf(file, "%d\t0x%08X\t(R:%d, G:%d, B:%d)\n", i, color, r, g, b);
    }
    
    fclose(file);
    return true;
}

bool VideoDump::dump_palette_text_unique(const Video& video, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_palette_%04d%02d%02d_%02d%02d%02d.txt",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_palette_text(video, unique_filename);
}

bool VideoDump::dump_video_state_text(const Video& video, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        return false;
    }
    
    fprintf(file, "Full Video Subsystem State Dump\n");
    fprintf(file, "=================================\n");
    fprintf(file, "Timestamp: %ld\n", std::time(nullptr));
    fprintf(file, "\n");
    
    // Видеорегистры
    fprintf(file, "Video Registers:\n");
    fprintf(file, "  NCREG: 0x%02X\n", video.get_ncreg());
    fprintf(file, "  VIREG: 0x%02X\n", video.get_vireg());
    fprintf(file, "\n");
    
    // Страницы GZU
    fprintf(file, "GZU Pages:\n");
    fprintf(file, "  Read Page (CPU Access): %d\n", video.get_gzu_read_page());
    fprintf(file, "  Show Page (Display): %d\n", video.get_gzu_show_page());
    fprintf(file, "\n");
    
    // Настройки ACZU
    fprintf(file, "ACZU Settings:\n");
    fprintf(file, "  Attr_Write Mode: %d\n", video.get_aczu_attr_write());
    fprintf(file, "  Wide Mode: %s\n", video.get_aczu_wide_mode() ? "ON" : "OFF");
    fprintf(file, "  Second Font: %s\n", video.get_aczu_second_font() ? "ON" : "OFF");
    fprintf(file, "\n");
    
    // Состояние видео
    fprintf(file, "Video State:\n");
    fprintf(file, "  Current Line: %d\n", video.get_current_line());
    fprintf(file, "  Color Mode: %s\n", video.is_color_mode() ? "COLOR" : "PLANE");
    fprintf(file, "  GZU Enabled: %s\n", video.is_gzu_enabled() ? "YES" : "NO");
    fprintf(file, "  ACZU Enabled: %s\n", video.is_aczu_enabled() ? "YES" : "NO");
    fprintf(file, "\n");
    
    // Размеры памяти
    fprintf(file, "Memory Sizes:\n");
    fprintf(file, "  GZU Size: %zu bytes\n", video.get_gzu_size());
    fprintf(file, "  ACZU Size: %zu bytes\n", video.get_aczu_size());
    fprintf(file, "  Framebuffer Size: %d bytes\n", 65536); // 512*256/2
    fprintf(file, "\n");
    
    fclose(file);
    return true;
}

bool VideoDump::dump_video_state_text_unique(const Video& video, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_video_state_%04d%02d%02d_%02d%02d%02d.txt",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_video_state_text(video, unique_filename);
}

bool VideoDump::dump_full(const Video& video, const char* base_filename) {
    bool success = true;
    
    // Отдельные имена файлов для каждого компонента
    char gzu_filename[512];
    char aczu_filename[512];
    char framebuffer_filename[512];
    char regs_filename[512];
    char palette_filename[512];
    char state_filename[512];
    
    // Уникальные имена файлов для каждого компонента
    snprintf(gzu_filename, sizeof(gzu_filename), "%s_gzu", base_filename);
    snprintf(aczu_filename, sizeof(aczu_filename), "%s_aczu", base_filename);
    snprintf(framebuffer_filename, sizeof(framebuffer_filename), "%s_framebuffer", base_filename);
    snprintf(regs_filename, sizeof(regs_filename), "%s_regs", base_filename);
    snprintf(palette_filename, sizeof(palette_filename), "%s_palette", base_filename);
    snprintf(state_filename, sizeof(state_filename), "%s_state", base_filename);
    
    // Дамп каждого компонента
    success &= dump_gzu_binary_unique(video, gzu_filename);
    success &= dump_aczu_binary_unique(video, aczu_filename);
    success &= dump_framebuffer_binary_unique(video, framebuffer_filename);
    success &= dump_video_registers_text_unique(video, regs_filename);
    success &= dump_palette_binary_unique(video, palette_filename);
    success &= dump_video_state_text_unique(video, state_filename);
    
    return success;
}

bool VideoDump::dump_screenshot_png(const Video& video, const char* filename) {
    // Здесь должен быть конвертер кадрового буфера в PNG;
    // пока — заглушка с пояснением
    
    FILE* file = fopen(filename, "w");
    if (!file) {
        return false;
    }
    
    fprintf(file, "PNG Screenshot Placeholder\n");
    fprintf(file, "==========================\n");
    fprintf(file, "This would be a PNG screenshot of the current frame.\n");
    fprintf(file, "Resolution: 512x256 pixels\n");
    fprintf(file, "Color Depth: 16 colors (from palette)\n");
    fprintf(file, "Timestamp: %ld\n", std::time(nullptr));
    
    fclose(file);
    return true;
}

bool VideoDump::dump_screenshot_png_unique(const Video& video, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_screenshot_%04d%02d%02d_%02d%02d%02d.png",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_screenshot_png(video, unique_filename);
}

}  // namespace korvet