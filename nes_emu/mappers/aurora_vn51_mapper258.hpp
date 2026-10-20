#pragma once
/* Mapper 258 - UNL-158B (Blood of Jurassic / Super Hang-On bootleg).
 * MAME reference: nes_158b_device plus nes_8237_device in mmc3_clones.cpp.
 * Shares MMC3 IRQ state and scheduler; no CPU/PPU shared-code modifications.
 * MAME lists its implementation as preliminary; game compatibility is not certified.
 * The CPU open-bus high nibble is unavailable in QuickNES; use its conventional
 * address-high approximation only for the undriven protection bits.
 */
#include "mapper004.hpp"
#include <stdint.h>
#include <string.h>

class AuroraVn51Mapper258 : public Mapper004 {
    /* [0]=$5000 PRG/CHR mode, [1]=$5001 outer banks, [2]=$5007 command map,
     * [3]=$5002 protection selector. Reset protection row 7 (MAME). */
    uint8_t ext[4];
    static unsigned translated_address(unsigned mode, unsigned addr) {
        static const uint8_t table[8][8] = {
            {0,1,2,3,4,5,6,7}, {3,2,0,4,1,5,6,7},
            {0,1,2,3,4,5,6,7}, {5,0,1,2,3,7,6,4},
            {3,1,0,4,2,5,6,7}, {0,1,2,3,4,5,6,7},
            {0,1,2,3,4,5,6,7}, {0,1,2,3,4,5,6,7}
        };
        static const unsigned addr_lut[8] = {
            0x8000,0x8001,0xa000,0xa001,0xc000,0xc001,0xe000,0xe001
        };
        return addr_lut[table[mode & 7U][((addr >> 12) & 6U) | (addr & 1U)]];
    }
    static unsigned translated_command(unsigned m, unsigned v) {
        static const uint8_t table[8][8] = {
            {0,1,2,3,4,5,6,7}, {0,2,6,1,7,3,4,5},
            {0,5,4,1,7,2,6,3}, {0,6,3,7,5,2,4,1},
            {0,2,5,3,6,1,7,4}, {0,1,2,3,4,5,6,7},
            {0,1,2,3,4,5,6,7}, {0,1,2,3,4,5,6,7}
        };
        return (v & 0xC0U) | table[m & 7U][v & 7U];
    }
    static uint8_t protection(unsigned row,unsigned low) {
        static const uint8_t table[8][8] = {
            {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,3,0},
            {0,0,0,0,3,4,0,0}, {0,0,0,1,0,4,5,0},
            {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0,0}, {0,0,0,1,2,4,15,0}
        };
        return table[row & 7U][low & 7U];
    }
    unsigned prg_base() const {
        return ((unsigned(ext[1]) & 3U) << 5) |
               ((ext[0] & 0x40U) ? (unsigned(ext[1]) & 0x10U) : 0U);
    }
    unsigned chr_base() const {
        return ((unsigned(ext[1]) & 0x0CU) << 6) |
               ((ext[0] & 0x40U) ? ((unsigned(ext[1]) & 0x20U) << 2) : 0U);
    }
    int resolve_prg(unsigned bank) const {
        return int(prg_base() | (bank & ((ext[0] & 0x40U) ? 0x0FU : 0x1FU)));
    }
    int resolve_chr(unsigned bank) const {
        return int(chr_base() | (bank & ((ext[0] & 0x40U) ? 0x7FU : 0xFFU)));
    }
    void update_prg_banks() {
        if (ext[0] & 0x80U) {
            const unsigned bank=(prg_base() >> 1) |
                (unsigned(ext[0]) & ((ext[0] & 0x40U) ? 7U : 15U));
            const unsigned swap=(ext[0] >> 5) & 1U;
            set_prg_bank(0x8000, bank_16k, int(bank & ~swap));
            set_prg_bank(0xC000, bank_16k, int(bank | swap));
            return;
        }
        const unsigned swap=(unsigned(mode) >> 6) & 1U;
        set_prg_bank(0x8000 ^ (swap << 14), bank_8k, resolve_prg(banks[6]));
        set_prg_bank(0xA000, bank_8k, resolve_prg(banks[7]));
        set_prg_bank(0xC000 ^ (swap << 14), bank_8k, resolve_prg(0xFEU));
        set_prg_bank(0xE000, bank_8k, resolve_prg(0xFFU));
    }
    void update_chr_banks() {
        const unsigned x=(unsigned(mode) & 0x80U) ? 0x1000U : 0U;
        const unsigned b0=unsigned(banks[0]) & 0xFEU;
        const unsigned b1=unsigned(banks[1]) & 0xFEU;
        for(unsigned n=0;n<8;++n) {
            unsigned bank;
            if(n<2)bank=b0+n;
            else if(n<4)bank=b1+n-2;
            else bank=unsigned(banks[n-2]);
            set_chr_bank((n*0x400U)^x,bank_1k,resolve_chr(bank));
        }
    }
public:
    AuroraVn51Mapper258() { memset(ext,0,sizeof ext); }
    virtual void reset_state() {
        Mapper004::reset_state();
        memset(ext,0,sizeof ext);
        ext[1]=0x0F;ext[3]=7;
        /* No external board peripherals, DIP or physical options. */
    }
    virtual void apply_mapping() {
        /* Do not call Mapper004::apply_mapping(): it writes via virtual
         * write(), which here transforms cartridge register addresses. */
        Mapper004::write(0,0xA000,mirror);
        Mapper004::write(0,0xA001,sram_mode);
        update_chr_banks();update_prg_banks();start_frame();
        intercept_reads(0x5000,0x1000);
        intercept_writes(0x5000,0x1000);
    }
    virtual void write(nes_time_t t,nes_addr_t addr,int value) {
        unsigned mapped=translated_address(ext[2],addr);
        unsigned d=unsigned(value)&255U;
        if(mapped==0x8000U)d=translated_command(ext[2],d);
        Mapper004::write(t,mapped,int(d));
    }
    virtual bool write_intercepted(nes_time_t,nes_addr_t addr,int value) {
        if(addr<0x5000U || addr>=0x6000U)return false;
        switch(addr & 0x1007U) {
            case 0x1000U:ext[0]=(uint8_t)value;update_prg_banks();update_chr_banks();break;
            case 0x1001U:ext[1]=(uint8_t)value;update_prg_banks();update_chr_banks();break;
            case 0x1002U:ext[3]=(uint8_t)(value&7);break;
            case 0x1007U:ext[2]=(uint8_t)(value&7);break;
        }
        return true;
    }
    virtual int read(nes_time_t,nes_addr_t addr) {
        if(addr<0x5000U||addr>=0x6000U)return -1;
        return int(((addr>>8)&0xF0U)|protection(ext[3],addr&7U));
    }
    virtual void save_state(mapper_state_t&out) {
        uint8_t data[sizeof(mmc3_state_t)+sizeof ext];
        memcpy(data,static_cast<const mmc3_state_t*>(this),sizeof(mmc3_state_t));
        memcpy(data+sizeof(mmc3_state_t),ext,sizeof ext);
        out.write(data,sizeof data);
    }
    virtual void read_state(mapper_state_t const&in) {
        uint8_t data[sizeof(mmc3_state_t)+sizeof ext];
        memset(data,0,sizeof data);
        in.read(data,sizeof data);
        memcpy(static_cast<mmc3_state_t*>(this),data,sizeof(mmc3_state_t));
        memcpy(ext,data+sizeof(mmc3_state_t),sizeof ext);
    }
};
