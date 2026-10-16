#pragma once

#include <cstdint>
#include <array>

namespace korvet {

class Pic {
public:
    void init();
    void reset();
    
    void raise_irq(uint8_t irq);
    void lower_irq(uint8_t irq);
    
    uint8_t read(uint8_t port);
    void write(uint8_t port, uint8_t data);
    
    bool has_pending_interrupt(uint8_t* vector);
    uint16_t acknowledge_interrupt();
    
    uint8_t read_isr() const { return isr_; }
    uint8_t read_irr() const { return irr_; }
    uint8_t read_imr() const { return imr_; }

private:
    uint8_t irr_;
    uint8_t isr_;
    uint8_t imr_;
    
    uint8_t icw1_;
    uint8_t icw2_;
    uint8_t icw3_;
    uint8_t icw4_;

    bool icw_load_active_;          // Programming mode flag (matches original picPRG)
    int icw_load_step_;         // 1 = ждём ICW2, 2 = ждём ICW3, 0 = не в init
    bool icw_cascade_;          // SNGL=0: после ICW2 ожидается ICW3
    
    uint8_t ocw3_;
    
    bool special_mask_mode_;
    bool poll_command_;
    
    uint8_t low_priority_;  // Rotating priority (IRQ number of lowest priority)
    
    uint8_t find_highest_priority_irq(uint8_t mask) const;
    void update_interrupt_line();
};

}  // namespace korvet
