#pragma once
/* QuickNES / Aurora - NES 2.0 mapper 549, Kaiser KS-7016B.
 * FDS conversion board: register is the CPU write ADDRESS, data ignored.
 * Select an 8 KiB PRG-ROM bank in $6000, fixed PRG $8000-$FFFF bank #2,
 * unbanked CHR RAM.  Does not supply FDS expansion sound.
 * Reference: libretro FCEUmm src/boards/549.c (Latch_address).
 */
#include "Nes_Mapper.h"
#include <stdint.h>

class AuroraVn28Mapper549 : public Nes_Mapper {
    // Preserve full address, not only low nibble: physical ROM bank wrapping
    // depends on PRG size and belongs to set_prg_bank().
    uint8_t addr[2];
    unsigned latch_address() const {
        return (unsigned)addr[0] | ((unsigned)addr[1]<<8);
    }
    void select_bank() {
        const unsigned addr=latch_address();
        const unsigned bank=(addr>>2) | ((addr>>3)&4U);
        set_prg_bank(0x6000,bank_8k,(int)bank);
    }
public:
    AuroraVn28Mapper549() { register_state(addr,sizeof addr); }
    virtual void reset_state() { addr[0]=0;addr[1]=0; }
    virtual void apply_mapping() {
        set_prg_bank(0x8000,bank_32k,2);
        select_bank();
        // Fixed mirroring comes from the ROM header, not an invented register.
    }
    virtual void write(nes_time_t,nes_addr_t addr,int) {
        // Write address is latched through the high-PRG standard interception.
        this->addr[0]=(uint8_t)addr;
        this->addr[1]=(uint8_t)(addr>>8);
        select_bank();
    }
};
