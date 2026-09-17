#pragma once
#include "Nes_Mapper.h"
#include <string.h>

/* AURORA_QUICKNES_MAPPER164_V7_20260917
 * Dongda PEC-9588 / Yancheng CY2000-3.
 *
 * - register-controlled PRG banking and mirroring
 * - 8 KiB CHR RAM
 * - PEC-586 1-bpp background mode (implemented in the PPU renderer)
 * - 93C66-compatible 512-byte serial EEPROM in 8-bit organisation
 * - separate 2 KiB volatile WRAM mirrored through $6000-$7fff
 *
 * EEPROM bytes occupy SRAM backing [0x000..0x1ff] so the frontend can expose
 * exactly 512 persistent bytes.  Volatile WRAM lives at backing offset 0x800
 * and is mapped explicitly; it is therefore present in savestates but outside
 * the mapper's persistent 512-byte save window. */
class Mapper164 : public Nes_Mapper
{
    enum
    {
        EE_STANDBY = 0,
        EE_START = 1,
        EE_OPCODE = 3,
        EE_ADDRESS = 12,
        EE_DATA = 20,
        EE_FINISHED = 99,
        EE_WRITE_DISABLE = 10,
        EE_WRITE_ALL = 11,
        EE_ERASE_ALL = 12,
        EE_WRITE_ENABLE = 13
    };

    struct state_t
    {
        uint8_t reg[8];
        uint8_t ee_opcode;
        uint8_t ee_state;
        uint8_t ee_last_clk;
        uint8_t ee_write_enabled;
        uint8_t ee_output;
        uint16_t ee_data;
        uint16_t ee_address;
    } s;

    uint8_t *storage() const
    {
        return sram_data_ptr();
    }

    uint8_t ee_read_storage(unsigned address) const
    {
        return storage()[address & 0x01ff];
    }

    void ee_write_storage(unsigned address, uint8_t value)
    {
        storage()[address & 0x01ff] = value;
    }

    void ee_fill(uint8_t value)
    {
        memset(storage(), value, 0x0200);
    }

    void ee_set_lines(int cs, int clk, int dat)
    {
        cs = !!cs;
        clk = !!clk;
        dat = !!dat;

        if (!cs && s.ee_state <= EE_ADDRESS)
            s.ee_state = EE_STANDBY;
        else if (s.ee_state == EE_STANDBY && cs && clk && !s.ee_last_clk)
        {
            if (dat)
                s.ee_state = EE_START;
            s.ee_opcode = 0;
            s.ee_address = 0;
            s.ee_output = 1;
        }
        else if (clk && !s.ee_last_clk && s.ee_state >= EE_START)
        {
            if (s.ee_state >= EE_START && s.ee_state < EE_OPCODE)
                s.ee_opcode = (uint8_t)((s.ee_opcode << 1) | dat);
            else if (s.ee_state >= EE_OPCODE && s.ee_state < EE_ADDRESS)
                s.ee_address = (uint16_t)((s.ee_address << 1) | dat);
            else if (s.ee_state >= EE_ADDRESS && s.ee_state < EE_DATA)
            {
                if (s.ee_opcode == 1 || s.ee_opcode == EE_WRITE_ALL)
                    s.ee_data = (uint16_t)((s.ee_data << 1) | dat);
                else if (s.ee_opcode == 2)
                {
                    s.ee_output = (uint8_t)!!(s.ee_data & 0x80);
                    s.ee_data <<= 1;
                }
            }

            ++s.ee_state;

            if (s.ee_state == EE_ADDRESS)
            {
                switch (s.ee_opcode)
                {
                    case 0:
                        s.ee_opcode = (uint8_t)((s.ee_address >> 7) + 10);
                        if (s.ee_opcode == EE_WRITE_DISABLE)
                        {
                            s.ee_write_enabled = 0;
                            s.ee_state = EE_FINISHED;
                        }
                        else if (s.ee_opcode == EE_WRITE_ENABLE)
                        {
                            s.ee_write_enabled = 1;
                            s.ee_state = EE_FINISHED;
                        }
                        else if (s.ee_opcode == EE_ERASE_ALL)
                        {
                            if (s.ee_write_enabled)
                                ee_fill(0xff);
                            s.ee_state = EE_FINISHED;
                        }
                        else if (s.ee_opcode == EE_WRITE_ALL)
                            s.ee_address = 0;
                        break;

                    case 2:
                        s.ee_data = ee_read_storage(s.ee_address);
                        ++s.ee_address;
                        break;

                    case 3:
                        if (s.ee_write_enabled)
                            ee_write_storage(s.ee_address, 0xff);
                        s.ee_state = EE_FINISHED;
                        break;
                }
            }
            else if (s.ee_state == EE_DATA)
            {
                if (s.ee_opcode == 1)
                {
                    if (s.ee_write_enabled)
                        ee_write_storage(s.ee_address, (uint8_t)s.ee_data);
                    ++s.ee_address;
                    s.ee_state = EE_FINISHED;
                }
                else if (s.ee_opcode == EE_WRITE_ALL)
                {
                    if (s.ee_write_enabled)
                        ee_write_storage(s.ee_address, (uint8_t)s.ee_data);
                    ++s.ee_address;
                    s.ee_state = (uint8_t)(cs && s.ee_address < 0x0200 ?
                                           EE_ADDRESS : EE_FINISHED);
                }
                else if (s.ee_opcode == 2)
                {
                    if (s.ee_address < 0x0200)
                        s.ee_data = ee_read_storage(s.ee_address);
                    ++s.ee_address;
                    s.ee_state = (uint8_t)(cs && s.ee_address <= 0x0200 ?
                                           EE_ADDRESS : EE_FINISHED);
                }
            }

            if (s.ee_state == EE_FINISHED)
            {
                s.ee_output = 0;
                s.ee_state = EE_STANDBY;
            }
        }

        /* 93C66 READ supplies a dummy zero before the first data byte. */
        if (s.ee_opcode == 2 && s.ee_state == EE_ADDRESS - 2)
            s.ee_output = 0;

        s.ee_last_clk = (uint8_t)clk;
    }

    void map_wram()
    {
        /* Do not expose the EEPROM bytes themselves as CPU RAM. */
        enable_sram(false);
        map_sram_mirror(0x6000, 0x2000, 0x0800, 0x0800);
        intercept_reads(0x6000, 0x2000);
        intercept_writes(0x6000, 0x2000);
    }

    uint8_t wram_read(nes_addr_t addr) const
    {
        return storage()[0x0800 + (addr & 0x07ff)];
    }

    void wram_write(nes_addr_t addr, uint8_t value)
    {
        storage()[0x0800 + (addr & 0x07ff)] = value;
    }

    void sync()
    {
        int prg_low;
        int prg_high;
        int mode;

        prg_low = (s.reg[0] & 0x0f) | ((s.reg[0] >> 1) & 0x10);
        prg_high = (int)s.reg[1] << 5;
        mode = ((s.reg[0] >> 5) & 2) | ((s.reg[0] >> 4) & 1);

        switch (mode)
        {
            case 0:
                set_prg_bank(0x8000, bank_16k, prg_high | prg_low);
                set_prg_bank(0xc000, bank_16k, prg_high | 0x1f);
                break;

            case 1:
                /* Hardware is open bus / expansion-ROM selected here.
                 * FCEUmm intentionally leaves the previous PRG mapping in
                 * place; do the same until QuickNES grows an upper-memory
                 * open-bus mapper primitive. */
                break;

            case 2:
                set_prg_bank(0x8000, bank_16k, prg_high | prg_low);
                set_prg_bank(0xc000, bank_16k,
                             prg_high | (prg_low >= 0x1c ? 0x1c : 0x1e));
                break;

            default:
                if (prg_low & 0x10)
                {
                    set_prg_bank(0x8000, bank_16k,
                                 prg_high | ((prg_low << 1) & 0x10) |
                                 (prg_low & 0x0f));
                    set_prg_bank(0xc000, bank_16k,
                                 prg_high | ((prg_low << 1) & 0x10) | 0x0f);
                }
                else
                    set_prg_bank(0x8000, bank_32k,
                                 (prg_high >> 1) | prg_low);
                break;
        }

        set_chr_bank(0x0000, bank_8k, 0);

        if ((s.reg[0] & 0x10) && !(s.reg[3] & 0x80))
            mirror_horiz();
        else
            mirror_vert();

        emu().ppu.set_pec586_1bpp_mode((s.reg[0] & 0x80) != 0);
        ee_set_lines(s.reg[2] & 0x10, s.reg[2] & 0x04, s.reg[2] & 0x01);
        map_wram();
    }

    void write_register(nes_addr_t addr, int data)
    {
        s.reg[(addr >> 8) & 7] = (uint8_t)data;
        sync();
    }

public:
    Mapper164() { register_state(&s, sizeof s); }

    void reset_state()
    {
        memset(&s, 0, sizeof s);
    }

    void apply_mapping()
    {
        intercept_reads(0x5400, 0x0400);
        intercept_writes(0x5000, 0x0800);
        sync();
    }

    int read(nes_time_t time, nes_addr_t addr)
    {
        if (addr >= 0x5400 && addr <= 0x57ff)
            return s.ee_output ? 0x00 : 0x04;
        if (addr >= 0x6000 && addr <= 0x7fff)
            return wram_read(addr);
        return Nes_Mapper::read(time, addr);
    }

    bool write_intercepted(nes_time_t, nes_addr_t addr, int data)
    {
        if (addr >= 0x5000 && addr <= 0x57ff)
        {
            write_register(addr, data);
            return true;
        }
        if (addr >= 0x6000 && addr <= 0x7fff)
        {
            wram_write(addr, (uint8_t)data);
            return true;
        }
        return false;
    }

    void write(nes_time_t, nes_addr_t, int) { }
};
