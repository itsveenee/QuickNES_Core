#pragma once
/* SPDX-License-Identifier: GPL-2.0-or-later
 * Aurora QuickNES Mapper 183 (Shui Guan / Gimmick! bootleg VRC4).
 * Hardware reference: negativeExponent/libretro-fceumm_next
 * src/mappers/mapper183.c and src/mappers/hw/vrc24.c.
 * Parent Mapper021 supplies VRC4 CHR, mirroring and IRQ; only PRG wiring
 * and A0/A1 decoder differ. The four extra PRG registers are appended to
 * the 18-byte parent snapshot instead of changing its registered buffer.
 * Does not model hardware DIP switches or external peripherals.
 */
#include "mapper021.hpp"
#include <stdint.h>
#include <string.h>

class AuroraVn44Mapper183 : public Mapper021 {
    uint8_t ex_prg[4];
    enum { parent_state_bytes=sizeof(::vrc2_state_t) };

    void sync_prg() {
        set_prg_bank(0x6000,bank_8k,ex_prg[0]);
        set_prg_bank(0x8000,bank_8k,ex_prg[1]);
        set_prg_bank(0xA000,bank_8k,ex_prg[2]);
        set_prg_bank(0xC000,bank_8k,ex_prg[3]);
        set_prg_bank(0xE000,bank_8k,last_bank);
    }
public:
    AuroraVn44Mapper183() {
        /* Actual 183 VRC4: CHR and IRQ address select A2/A3. */
        reg1mask=0x04;
        reg2mask=0x08;
        memset(ex_prg,0,sizeof ex_prg);
        ex_prg[3]=0xFE;
    }
    virtual void reset_state() {
        Mapper021::reset_state();
        memset(ex_prg,0,sizeof ex_prg);
        ex_prg[3]=0xFE;
    }
    virtual void apply_mapping() {
        Mapper021::apply_mapping();
        /* The parent VRC4 maps $6000 to WRAM; this board has PRG-ROM here.
           Do this AFTER the parent's mapping so the ROM window wins. */
        sync_prg();
        intercept_writes(0x6000,0x1000);
    }
    virtual bool write_intercepted(nes_time_t,nes_addr_t addr,int) {
        if(addr<0x6800||addr>0x6fff)return false;
        const uint8_t p=(uint8_t)(addr&0x3f);
        if(ex_prg[0]!=p){ex_prg[0]=p;sync_prg();}
        return true;
    }
    virtual void write(nes_time_t time,nes_addr_t addr,int data) {
        /* The four 8-KiB PRG windows have independent wiring. Leave all
           CHR/mirroring/IRQ transactions to the established VRC4 core. */
        switch(addr&0xf800) {
            case 0x8800: ex_prg[1]=(uint8_t)(data&0x3f);sync_prg();return;
            case 0xa800: ex_prg[2]=(uint8_t)(data&0x3f);sync_prg();return;
            case 0xa000: ex_prg[3]=(uint8_t)(data&0x3f);sync_prg();return;
            default: Mapper021::write(time,addr,data);sync_prg();return;
        }
    }
    virtual void save_state(mapper_state_t &out) {
        Mapper021::save_state(out);
        /* vrc2_state_t is 18 bytes in the real QuickNES core. */
        if(out.size==parent_state_bytes &&
                out.size+(int)sizeof ex_prg<=(int)max_mapper_state_size) {
            memcpy(out.data+out.size,ex_prg,sizeof ex_prg);
            out.size+=(int)sizeof ex_prg;
        }
    }
    virtual void read_state(mapper_state_t const &in) {
        /* The parent restores its own IRQ and CHR state. */
        Mapper021::read_state(in);
        if(in.size>=(int)(parent_state_bytes+sizeof ex_prg))
            memcpy(ex_prg,in.data+parent_state_bytes,sizeof ex_prg);
        else {
            memset(ex_prg,0,sizeof ex_prg);
            ex_prg[3]=0xFE;
        }
        sync_prg();
    }
};
