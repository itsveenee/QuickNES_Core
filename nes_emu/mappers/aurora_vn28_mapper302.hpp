#pragma once
/* QuickNES / Aurora - NES 2.0 mapper 302 (KS7057, Gyruss FDS conversion)
 * C++98 standalone mapper.  Eight addressable 2-KiB PRG windows; four at
 * $6000 and four at $8000, fixed $A000 bank #13 and $C000 16-KiB bank #7.
 * Register writes are nibble-sized and decoded by address; A0 chooses nibble.
 * FDS expansion audio is not implemented.
 * References: NESdev NES 2.0 Mapper 302, FCEUmm-next mapper302.c.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
#include <string.h>

class AuroraVn28Mapper302 : public Nes_Mapper {
    uint8_t r[9]; // eight PRG bank numbers, then mirroring
    void map_slot(int slot) {
        const nes_addr_t base=(slot<4?0x8000:0x6000);
        set_prg_bank(base + (slot&3)*0x800, bank_2k, r[slot]);
    }
public:
    AuroraVn28Mapper302() { register_state(r, sizeof r); }
    virtual void reset_state() { memset(r,0,sizeof r); }
    virtual void apply_mapping() {
        // Default memory state supplies the 8 KiB unbanked CHR RAM.
        for(int i=0;i<8;++i) map_slot(i);
        set_prg_bank(0xa000, bank_8k, 13);
        set_prg_bank(0xc000, bank_16k, 7);
        if(r[8]&1) mirror_vert(); else mirror_horiz();
    }
    virtual void write(nes_time_t,nes_addr_t a,int data) {
        if(a>=0x8000 && a<0xa000) {
            r[8]=(uint8_t)(data&1);
            if(r[8]) mirror_vert(); else mirror_horiz();
        } else if(a>=0xb000 && a<0xf000) {
            // Four 4-KiB register groups, only A1:A0 decoded within each.
            const int reg=(((a>>12)-11)<<1) | ((a>>1)&1);
            const uint8_t nibble=(uint8_t)(data&0x0f);
            if(a&1) r[reg]=(uint8_t)((r[reg]&0x0f)|(nibble<<4));
            else r[reg]=(uint8_t)((r[reg]&0xf0)|nibble);
            map_slot(reg);
        }
    }
};
