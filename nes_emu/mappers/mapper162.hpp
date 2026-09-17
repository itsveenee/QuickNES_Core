#pragma once
#include "Nes_Mapper.h"
#include <string.h>

/* AURORA_QUICKNES_MAPPER162_V7_20260917
 * Waixing FS304.  PRG decode follows the documented register truth table;
 * CHR-RAM auto switching uses the real PPU-address hook already present in
 * Aurora QuickNES, latching PPU A9 on each PPU A13 rise instead of guessing
 * from the current scanline. */
class Mapper162 : public Nes_Mapper
{
    struct state_t
    {
        uint8_t reg[4];
        uint8_t chr_half;
        uint16_t last_vram_addr;
    } s;

    void update_prg()
    {
        int bank = ((int)s.reg[2] << 4) | (s.reg[0] & 0x0c);

        if (s.reg[3] & 0x04)
            bank |= s.reg[0] & 0x02;
        else
            bank |= 0x02;

        if (!(s.reg[3] & 0x01))
            bank |= (s.reg[1] >> 1) & 0x01;
        else if (!(s.reg[3] & 0x04))
            bank |= 0x01;
        else
            bank |= s.reg[0] & 0x01;

        set_prg_bank(0x8000, bank_32k, bank);
    }

    void update_chr(bool from_ppu)
    {
        int half = s.chr_half & 1;

        if (!from_ppu)
        {
            if (s.reg[0] & 0x80)
            {
                set_chr_bank(0x0000, bank_4k, half);
                set_chr_bank(0x1000, bank_4k, half);
            }
            else
                set_chr_bank(0x0000, bank_8k, 0);
            return;
        }

        /* The PPU hook runs from inside rendering, so it must not recurse via
         * Nes_Mapper::set_chr_bank(), which calls render_until(). */
        if (s.reg[0] & 0x80)
        {
            long offset = (long)half << 12;
            emu().ppu.set_chr_bank(0x0000, 0x1000, offset);
            emu().ppu.set_chr_bank(0x1000, 0x1000, offset);
        }
        else
            emu().ppu.set_chr_bank(0x0000, 0x2000, 0);
    }

    void write_register(nes_addr_t addr, int data)
    {
        s.reg[(addr >> 8) & 3] = (uint8_t)data;
        update_prg();
        update_chr(false);
    }

public:
    Mapper162() { register_state(&s, sizeof s); }

    void reset_state()
    {
        memset(&s, 0, sizeof s);
    }

    void apply_mapping()
    {
        enable_sram(true);
        intercept_reads(0x5000, 0x0800);
        intercept_writes(0x5000, 0x0800);
        update_prg();
        update_chr(false);
    }

    bool needs_vram_address_hook() const { return true; }

    void vram_address_changed(nes_addr_t addr)
    {
        if (!(s.last_vram_addr & 0x2000) && (addr & 0x2000))
        {
            uint8_t half = (uint8_t)((addr >> 9) & 1);
            if (half != s.chr_half)
            {
                s.chr_half = half;
                if (s.reg[0] & 0x80)
                    update_chr(true);
            }
        }
        s.last_vram_addr = (uint16_t)addr;
    }

    int read(nes_time_t time, nes_addr_t addr)
    {
        if (addr >= 0x5000 && addr <= 0x57ff)
            return 0x00;
        return Nes_Mapper::read(time, addr);
    }

    bool write_intercepted(nes_time_t, nes_addr_t addr, int data)
    {
        if (addr >= 0x5000 && addr <= 0x57ff)
        {
            write_register(addr, data);
            return true;
        }
        return false;
    }

    void write(nes_time_t, nes_addr_t, int) { }
};
