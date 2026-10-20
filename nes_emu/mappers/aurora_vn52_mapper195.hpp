#pragma once
/* Aurora QuickNES vN52 - Waixing FS303, NES2 mapper 195.
 * FCEUmm-next src/mappers/mapper195.c and hw/mmc3.c.
 * MMC3 IRQ and registers are inherited; only FS303 CHR-RAM latch logic,
 * auxiliary WRAM, and the per-mapper PPU $2007 write event are new.
 * The PPU callback is dispatched only for this mapper, before writing CHR.
 * Does NOT support physical switches, external devices or FDS copiers.
 */
#include "mapper004.hpp"
#include <stdint.h>
#include <string.h>

class AuroraVn52Mapper195 : public Mapper004 {
    uint8_t chr_mask, chr_compare;
    static const uint8_t *latch_table() {
        static const uint8_t table[8] = {
            0x28,0x00,0x4c,0x64,0x46,0x7c,0x04,0xff
        };
        return table;
    }
    unsigned chr_value(unsigned page) const {
        page ^= (mode & 0x80U) ? 4U : 0U;
        if (page & 4U) return banks[page - 2U];
        return (unsigned(banks[page >> 1U]) & 0xfeU) | (page & 1U);
    }
    virtual void update_chr_banks() {
        for (unsigned page = 0; page < 8U; ++page) {
            const unsigned bank = chr_value(page);
            const nes_addr_t address = page * 0x400U;
            if ((bank & chr_mask) == chr_compare)
                set_chr_ram_bank(address, bank_1k, int(bank));
            else
                set_chr_bank(address, bank_1k, int(bank));
        }
    }
public:
    AuroraVn52Mapper195() : chr_mask(0xfc), chr_compare(0) {}
    virtual long aux_chr_ram_size() const { return 0x1000; }
    virtual long aurora_extra_sram_save_size() const { return 0x4000; }
    virtual bool needs_vram_address_hook() const { return true; }
    virtual void reset_state() {
        Mapper004::reset_state();
        chr_mask=0xfc;chr_compare=0;
    }
    virtual void apply_mapping() {
        Mapper004::apply_mapping();
        /* The reference maps WRAM bank 2 ($2000-$2fff) at $5000.
           QuickNES' 32 KiB SRAM buffer can hold this window without
           allocating new global memory. Mirrored opcode fetches follow
           the same physical RAM as intercepted ordinary data reads. */
        map_sram_mirror(0x5000,0x1000,0x2000,0x1000);
        intercept_reads(0x5000,0x1000);
        intercept_writes(0x5000,0x1000);
    }
    virtual int read(nes_time_t, nes_addr_t addr) {
        if (addr < 0x5000U || addr >= 0x6000U) return -1;
        return sram_data_ptr()[0x2000U + (addr & 0xfffU)];
    }
    virtual bool write_intercepted(nes_time_t, nes_addr_t addr,int data) {
        if (addr < 0x5000U || addr >= 0x6000U) return false;
        sram_data_ptr()[0x2000U + (addr & 0xfffU)] = uint8_t(data);
        return true;
    }
    virtual void ppu_data_written(nes_addr_t addr) {
        if (addr >= 0x2000U) return;
        const unsigned bank = chr_value((addr >> 10) & 7U);
        if (!(bank & 0x80U)) return;
        uint8_t mask,compare;
        if (bank & 0x10U) {
            mask=0;compare=0xff; // disable CHR-RAM mapping
        } else {
            const unsigned index=((bank >> 4)&4U) |
                                 ((bank >> 2)&2U) | ((bank >> 1)&1U);
            mask = (bank & 0x40U) ? 0xfeU : 0xfcU;
            compare=latch_table()[index];
        }
        if (mask!=chr_mask || compare!=chr_compare) {
            chr_mask=mask;chr_compare=compare;
            update_chr_banks();
        }
    }
    virtual void save_state(mapper_state_t &out) {
        uint8_t state_bytes[sizeof(mmc3_state_t)+2];
        memcpy(state_bytes,static_cast<const mmc3_state_t*>(this),sizeof(mmc3_state_t));
        state_bytes[sizeof(mmc3_state_t)]=chr_mask;
        state_bytes[sizeof(mmc3_state_t)+1]=chr_compare;
        out.write(state_bytes,sizeof state_bytes);
    }
    virtual void read_state(mapper_state_t const&in) {
        uint8_t state_bytes[sizeof(mmc3_state_t)+2];
        memset(state_bytes,0,sizeof state_bytes);
        in.read(state_bytes,sizeof state_bytes);
        memcpy(static_cast<mmc3_state_t*>(this),state_bytes,sizeof(mmc3_state_t));
        chr_mask=state_bytes[sizeof(mmc3_state_t)];
        chr_compare=state_bytes[sizeof(mmc3_state_t)+1];
        /* The outer Nes_Mapper::load_state invokes apply_mapping. */
    }
};
