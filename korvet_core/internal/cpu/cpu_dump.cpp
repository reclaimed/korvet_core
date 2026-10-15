/**
 * @file cpu_dump.cpp
 * @brief Реализация функций дампа состояния процессора КР580ВМ80А
 * 
 * Содержит реализацию функций для создания дампов состояния
 * процессора КР580ВМ80А в файлы для отладки.
 * 
 * @author Korvet Team
 * @date 2024
 */

#include "cpu_dump.hpp"
#include <cstdio>
#include <ctime>
#include <cstdarg>

namespace korvet {

static inline bool flag_s(uint8_t f)  { return (f & 0x80) != 0; }
static inline bool flag_z(uint8_t f)  { return (f & 0x40) != 0; }
static inline bool flag_h(uint8_t f)  { return (f & 0x10) != 0; }
static inline bool flag_p(uint8_t f)  { return (f & 0x04) != 0; }
static inline bool flag_n(uint8_t f)  { return (f & 0x02) != 0; }
static inline bool flag_c(uint8_t f)  { return (f & 0x01) != 0; }

bool CpuDump::dump_binary(const Cpu& cpu, const char* filename) {
    cpu_state_t state = cpu.get_state();
    FILE* file = fopen(filename, "wb");
    if (!file) {
        return false;
    }
    
    size_t written = fwrite(&state, sizeof(state), 1, file);
    fclose(file);
    
    return written == 1;
}

bool CpuDump::dump_binary_unique(const Cpu& cpu, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_%04d%02d%02d_%02d%02d%02d.bin",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_binary(cpu, unique_filename);
}

bool CpuDump::dump_text(const Cpu& cpu, const char* filename) {
    cpu_state_t state = cpu.get_state();
    
    FILE* file = fopen(filename, "w");
    if (!file) {
        return false;
    }
    
    fprintf(file, "CPU State Dump\n");
    fprintf(file, "==============\n");
    fprintf(file, "Registers:\n");
    fprintf(file, "  A: 0x%02X (%3d)\n", state.a, state.a);
    fprintf(file, "  B: 0x%02X (%3d)\n", state.b, state.b);
    fprintf(file, "  C: 0x%02X (%3d)\n", state.c, state.c);
    fprintf(file, "  D: 0x%02X (%3d)\n", state.d, state.d);
    fprintf(file, "  E: 0x%02X (%3d)\n", state.e, state.e);
    fprintf(file, "  H: 0x%02X (%3d)\n", state.h, state.h);
    fprintf(file, "  L: 0x%02X (%3d)\n", state.l, state.l);
    fprintf(file, "  PC: 0x%04X (%5d)\n", state.pc, state.pc);
    fprintf(file, "  SP: 0x%04X (%5d)\n", state.sp, state.sp);
    fprintf(file, "  IFF: %d\n", state.iff);
    fprintf(file, "  Last PC: 0x%04X (%5d)\n", state.last_pc, state.last_pc);
    
    // Печать флагов
    char flag_str[10];
    snprintf(flag_str, sizeof(flag_str), "%c%c%c%c%c%c",
        flag_s(state.f) ? 'S' : '-',
        flag_z(state.f) ? 'Z' : '-',
        flag_h(state.f) ? 'H' : '-',
        flag_p(state.f) ? 'P' : '-',
        flag_n(state.f) ? 'N' : '-',
        flag_c(state.f) ? 'C' : '-');
    
    fprintf(file, "Flags: %s\n", flag_str);
    
    fclose(file);
    return true;
}

bool CpuDump::dump_text_unique(const Cpu& cpu, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_%04d%02d%02d_%02d%02d%02d.txt",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_text(cpu, unique_filename);
}

bool CpuDump::dump_full(const Cpu& cpu, const char* filename) {
    cpu_state_t state = cpu.get_state();
    
    FILE* file = fopen(filename, "w");
    if (!file) {
        return false;
    }
    
    fprintf(file, "Full CPU State Dump\n");
    fprintf(file, "===================\n");
    fprintf(file, "Timestamp: %ld\n", std::time(nullptr));
    fprintf(file, "\n");
    
    fprintf(file, "Registers:\n");
    fprintf(file, "  A (Accumulator): 0x%02X (%3d)\n", state.a, state.a);
    fprintf(file, "  B: 0x%02X (%3d)\n", state.b, state.b);
    fprintf(file, "  C: 0x%02X (%3d)\n", state.c, state.c);
    fprintf(file, "  D: 0x%02X (%3d)\n", state.d, state.d);
    fprintf(file, "  E: 0x%02X (%3d)\n", state.e, state.e);
    fprintf(file, "  H: 0x%02X (%3d)\n", state.h, state.h);
    fprintf(file, "  L: 0x%02X (%3d)\n", state.l, state.l);
    fprintf(file, "  PC (Program Counter): 0x%04X (%5d)\n", state.pc, state.pc);
    fprintf(file, "  SP (Stack Pointer): 0x%04X (%5d)\n", state.sp, state.sp);
    fprintf(file, "  IFF (Interrupt Flip-Flop): %d\n", state.iff);
    fprintf(file, "  Last PC: 0x%04X (%5d)\n", state.last_pc, state.last_pc);
    fprintf(file, "\n");
    
    // Печать флагов
    char flag_str[10];
    snprintf(flag_str, sizeof(flag_str), "%c%c%c%c%c%c",
        flag_s(state.f) ? 'S' : '-',
        flag_z(state.f) ? 'Z' : '-',
        flag_h(state.f) ? 'H' : '-',
        flag_p(state.f) ? 'P' : '-',
        flag_n(state.f) ? 'N' : '-',
        flag_c(state.f) ? 'C' : '-');
    
    fprintf(file, "Flags:\n");
    fprintf(file, "  Sign (S): %s\n", flag_s(state.f) ? "SET" : "CLEAR");
    fprintf(file, "  Zero (Z): %s\n", flag_z(state.f) ? "SET" : "CLEAR");
    fprintf(file, "  Half Carry (H): %s\n", flag_h(state.f) ? "SET" : "CLEAR");
    fprintf(file, "  Parity/Overflow (P): %s\n", flag_p(state.f) ? "SET" : "CLEAR");
    fprintf(file, "  Add/Subtract (N): %s\n", flag_n(state.f) ? "SET" : "CLEAR");
    fprintf(file, "  Carry (C): %s\n", flag_c(state.f) ? "SET" : "CLEAR");
    fprintf(file, "  Combined: %s\n", flag_str);
    fprintf(file, "\n");
    
    fprintf(file, "Additional Info:\n");
    fprintf(file, "  State Size: %zu bytes\n", sizeof(cpu_state_t));
    
    fclose(file);
    return true;
}

bool CpuDump::dump_full_unique(const Cpu& cpu, const char* base_filename) {
    char unique_filename[512];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    snprintf(unique_filename, sizeof(unique_filename), 
             "%s_%04d%02d%02d_%02d%02d%02d_full.txt",
             base_filename,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return dump_full(cpu, unique_filename);
}

}  // namespace korvet