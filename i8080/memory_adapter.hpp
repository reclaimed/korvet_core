/**
 * @file memory_adapter.hpp
 * @brief Адаптер korvet_core::Memory -> i8080::IMemory
 *
 * Позволяет использовать i8080::Cpu с korvet_core::Memory без
 * модификации последнего. Memory не реализует IMemory напрямую,
 * потому что у него другой API (read(addr) с page-aware логикой).
 */
#pragma once

#include "i8080.hpp"
#include "../korvet_core/internal/memory/memory.hpp"

namespace i8080 {

/**
 * @brief Адаптер Memory -> IMemory для i8080::Cpu
 */
class MemoryAdapter : public IMemory {
public:
    explicit MemoryAdapter(korvet::Memory* mem) : mem_(mem) {}

    void set_memory(korvet::Memory* mem) { mem_ = mem; }
    korvet::Memory* memory() const { return mem_; }

    uint8_t read(uint16_t addr) override {
        if (!mem_) return 0xFF;
        return mem_->read(addr);
    }

    void write(uint16_t addr, uint8_t data) override {
        if (!mem_) return;
        mem_->write(addr, data);
    }

    uint16_t read_word(uint16_t addr) override {
        if (!mem_) return 0xFFFF;
        return mem_->read_word(addr);
    }

    void write_word(uint16_t addr, uint16_t data) override {
        if (!mem_) return;
        mem_->write_word(addr, data);
    }

private:
    korvet::Memory* mem_;
};

}  // namespace i8080
