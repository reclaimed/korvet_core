/**
 * @file cpu.cpp
 * @brief Реализация обёртки korvet::Cpu над i8080::Cpu
 */
#include "cpu.hpp"
#include "../memory/memory.hpp"
#include "../pic/pic.hpp"

namespace korvet {

// =====================================================================
// MemoryToIMemory — адаптер korvet::Memory -> i8080::IMemory
// =====================================================================

uint8_t MemoryToIMemory::read(uint16_t addr) {
    if (!mem_) return 0xFF;
    return mem_->read(addr);
}

void MemoryToIMemory::write(uint16_t addr, uint8_t data) {
    if (!mem_) return;
    mem_->write(addr, data);
}

uint16_t MemoryToIMemory::read_word(uint16_t addr) {
    if (!mem_) return 0xFFFF;
    return mem_->read_word(addr);
}

void MemoryToIMemory::write_word(uint16_t addr, uint16_t data) {
    if (!mem_) return;
    mem_->write_word(addr, data);
}

// =====================================================================
// PicToIPic — адаптер korvet::Pic -> i8080::IPic
// =====================================================================

bool PicToIPic::has_pending_interrupt() {
    if (!pic_) return false;
    uint8_t vector = 0;
    return pic_->has_pending_interrupt(&vector);
}

uint16_t PicToIPic::acknowledge_interrupt() {
    if (!pic_) return 0xFFFF;
    return pic_->acknowledge_interrupt();
}

// =====================================================================
// Cpu — обёртка над i8080::Cpu
// =====================================================================

Cpu::Cpu() : mem_adapter_(nullptr), pic_adapter_(nullptr) {}

void Cpu::init(Memory* memory, Pic* pic) {
    mem_adapter_.set_memory(memory);
    if (pic) {
        pic_adapter_.set_pic(pic);
        cpu_.init(&mem_adapter_, &pic_adapter_);
        pic_set_ = true;
    } else {
        cpu_.init(&mem_adapter_, nullptr);
        pic_set_ = false;
    }
    cpu_.reset();
}

void Cpu::reset() {
    cpu_.reset();
}

int Cpu::execute_one_instruction() {
    return cpu_.execute_one_instruction();
}

void Cpu::set_pic(Pic* pic) {
    pic_adapter_.set_pic(pic);
    // Передать новый адаптер в CPU
    if (pic) {
        cpu_.init(&mem_adapter_, &pic_adapter_);
    } else {
        cpu_.init(&mem_adapter_, nullptr);
    }
}

void Cpu::disassemble(uint16_t addr, char* buffer, size_t size) {
    cpu_.disassemble(addr, buffer, size);
}

}  // namespace korvet
