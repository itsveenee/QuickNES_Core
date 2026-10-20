#pragma once
/* Aurora QuickNES vN30 / Mapper 415
 * FCEUmm-next src/mappers/mapper415.c (single FDS cartridge conversion).
 * Latch data $8000-$FFFF: PRG-ROM 8K at $6000, last 32K at $8000;
 * bit4 selects nametable mirroring. No external FDS sound in reference.
 * Candidate: no PS2DEV/ROM compatibility claim.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
class AuroraVn30Mapper415 : public Nes_Mapper {
    uint8_t latch_data;
public:
    AuroraVn30Mapper415() { register_state(&latch_data,sizeof latch_data); }
    virtual void reset_state() { latch_data=0; }
    virtual void apply_mapping() {
        set_prg_bank(0x6000,bank_8k,latch_data);
        set_prg_bank(0x8000,bank_32k,last_bank);
        if(latch_data&0x10) mirror_horiz(); else mirror_vert();
    }
    virtual void write(nes_time_t,nes_addr_t addr,int data) {
        if(addr<0x8000) return;
        latch_data=uint8_t(data);
        set_prg_bank(0x6000,bank_8k,latch_data);
        if(latch_data&0x10) mirror_horiz(); else mirror_vert();
    }
};
