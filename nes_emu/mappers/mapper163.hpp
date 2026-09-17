#pragma once
#include "Nes_Mapper.h"
#include <string.h>

/* AURORA_QUICKNES_MAPPER163_V7_20260917
 * Nanjing FC-001.  Implements the register-bit swap/protection feedback and
 * the hardware PPU A9 latch used by the automatic 4 KiB CHR-RAM switching. */
class Mapper163 : public Nes_Mapper
{
    struct state_t
    {
        uint8_t reg[4];
        uint8_t chr_half;
        uint16_t last_vram_addr;
    } s;

    void update_prg()
    {
        int bank = ((int)s.reg[2] << 4) |
                   (s.reg[0] & 0x0f) |
                   ((s.reg[3] & 0x04) ? 0x00 : 0x03);
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
        uint8_t index = (uint8_t)((addr >> 8) & 3);
        uint8_t value = (uint8_t)data;
        const uint8_t last_swapped =
            (uint8_t)(cart().prg_size() == 0x100000L ? 1 : 2);

        if ((s.reg[3] & 0x01) && index <= last_swapped)
            value = (uint8_t)((value & ~3) |
                              ((value >> 1) & 1) |
                              ((value << 1) & 2));

        if (addr & 1)
        {
            if ((s.reg[1] & 0x01) && !(value & 0x01))
                s.reg[1] ^= 0x04;
        }
        else
            s.reg[index] = value;

        update_prg();
        update_chr(false);
    }

public:
    Mapper163() { register_state(&s, sizeof s); }

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
            return (~s.reg[1]) & 0x04;
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
