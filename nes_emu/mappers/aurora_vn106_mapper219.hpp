#pragma once
/* Aurora vN106 - iNES 219 (Ka Sheng A9461), single-game Toy Story board.
 * Experimental native QuickNES integration; MMC3 A12/IRQ inherited from Mapper004.
 * Reference behavior: FCEUmm mapper219.c; independently adapted for QuickNES.
 * No external hardware, ROM preprocessing, or multicart menu dependency.
 */
#include "mapper004.hpp"
#include <stdint.h>

class AuroraVn106Mapper219 : public Mapper004 {
    uint8_t outer_[2];
    uint8_t extended_;

    void update_prg_banks() {
        const int mask = (outer_[0] & 0x40) ? 0x0f : 0x1f;
        const int base = (outer_[1] & 0x20) | ((outer_[0] << 4) & 0x10);
        const int last = (int)(cart().prg_size() >> 13) - 1;
        if (last < 1) return;
        const int b6 = (base & ~mask) | (banks[6] & mask);
        const int b7 = (base & ~mask) | (banks[7] & mask);
        const int penultimate = (base & ~mask) | ((last - 1) & mask);
        const int finalbank = (base & ~mask) | (last & mask);
        const int swap = ((mode >> 6) & 1);
        set_prg_bank(0x8000, bank_8k, swap ? penultimate : b6);
        set_prg_bank(0xa000, bank_8k, b7);
        set_prg_bank(0xc000, bank_8k, swap ? b6 : penultimate);
        set_prg_bank(0xe000, bank_8k, finalbank);
    }

    void update_chr_banks() {
        const int mask = (outer_[0] & 0x80) ? 0x7f : 0xff;
        const int base = ((outer_[1] << 3) & 0x100) | ((outer_[0] << 4) & 0x80);
        const int invert = ((mode >> 7) & 1) ? 0x1000 : 0;
        for (int pair=0;pair<2;++pair) {
            int src=(banks[pair] & ~1);
            for (int half=0;half<2;++half) {
                int b=(base & ~mask) | ((src+half) & mask);
                set_chr_bank((pair*0x800+half*0x400) ^ invert,bank_1k,b);
            }
        }
        for (int i=0;i<4;++i) {
            int b=(base & ~mask) | (banks[i+2] & mask);
            set_chr_bank((0x1000+i*0x400) ^ invert,bank_1k,b);
        }
    }

public:
    AuroraVn106Mapper219() : extended_(0) { outer_[0]=0;outer_[1]=0x20; }

    void reset_state() {
        Mapper004::reset_state();
        outer_[0]=0;
        outer_[1]=0x20;
        extended_=0;
    }

    void save_state(mapper_state_t& out) {
        Mapper004::save_state(out);
        if (out.size == (int)sizeof(mmc3_state_t)) {
            out.data[out.size++] = outer_[0];
            out.data[out.size++] = outer_[1];
            out.data[out.size++] = extended_;
        }
    }

    void read_state(mapper_state_t const& in) {
        outer_[0] = in.size > (int)sizeof(mmc3_state_t) ? in.data[sizeof(mmc3_state_t)] : 0;
        outer_[1] = in.size > (int)sizeof(mmc3_state_t)+1 ? in.data[sizeof(mmc3_state_t)+1] : 0x20;
        extended_ = in.size > (int)sizeof(mmc3_state_t)+2 ? in.data[sizeof(mmc3_state_t)+2] : 0;
        Nes_Mapper::read_state(in);
    }

    void apply_mapping() {
        intercept_writes(0x5000,0x1000);
        Mapper004::apply_mapping();
        update_prg_banks(); // base PRG writer is deliberately non-virtual
    }

    bool write_intercepted(nes_time_t, nes_addr_t addr, int data) {
        if ((unsigned)(addr - 0x5000) >= 0x1000U) return false;
        outer_[addr & 1] = (uint8_t)data;
        update_prg_banks();
        update_chr_banks();
        return true;
    }

    void write(nes_time_t time, nes_addr_t addr, int data) {
        if ((addr & 0xe001) == 0x8000) {
            /* $8002 toggles extended register format; $8000 remains MMC3.
             * Both load the MMC3 mode/control latch, matching FCEUmm. */
            if (addr & 2) extended_ = (data & 0x20) ? 1 : 0;
            Mapper004::write(time,addr,data);
            update_prg_banks();
            return;
        }
        if ((addr & 0xe001) == 0x8001 && extended_) {
            const unsigned cmd=(unsigned)mode;
            if (cmd>=0x08 && cmd<=0x1f) {
                const unsigned index=(cmd-8)>>2;
                if (index < 6) {
                    if (cmd & 1) banks[index]=(uint8_t)((banks[index] & 0xf0) | ((data >> 1) & 0x0f));
                    else banks[index]=(uint8_t)((banks[index] & 0x0f) | ((data << 4) & 0xf0));
                }
                update_chr_banks();
            } else if (cmd>=0x25 && cmd<=0x26) {
                const unsigned scrambled=((unsigned(data)<<1)&8)|((unsigned(data)>>1)&4)|((unsigned(data)>>3)&2)|((unsigned(data)>>5)&1);
                banks[6 | (cmd&1)]=(uint8_t)scrambled;
                update_prg_banks();
            }
            return;
        }
        Mapper004::write(time,addr,data);
        if ((addr & 0xe001) == 0x8001) update_prg_banks();
    }
};