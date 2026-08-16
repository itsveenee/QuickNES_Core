#pragma once
#include "Nes_Mapper.h"
#include <string.h>

class Mapper210 : public Nes_Mapper
{
    struct state_t
    {
        uint8_t chr[8];
        uint8_t prg[3];
        uint8_t write_protect;
        uint8_t nt[4];
    } s;

    bool is_175() const
    {
        /* NES 2.0 submapper 2 = Namco 340. Submapper 1 = Namco 175.
         * For ambiguous iNES/submapper 0, prefer 175's conservative SRAM
         * behavior rather than inventing N163 audio/IRQ hardware. */
        return cart().submapper_code() != 2;
    }

    void set_header_mirror()
    {
        if (cart().mirroring() & 1)
        {
            s.nt[0] = 0; s.nt[1] = 1;
            s.nt[2] = 0; s.nt[3] = 1;
        }
        else
        {
            s.nt[0] = 0; s.nt[1] = 0;
            s.nt[2] = 1; s.nt[3] = 1;
        }
    }

    void update_nt()
    {
        for (int i = 0; i < 4; ++i)
        {
            if (s.nt[i] & 0x80)
                mirror_manual_page(i, s.nt[i] & 1);
            else
                mirror_chr(i, s.nt[i]);
        }
    }

    void update()
    {
        for (int i = 0; i < 8; ++i)
            set_chr_bank(i * 0x400, bank_1k, s.chr[i]);

        set_prg_bank(0x8000, bank_8k, s.prg[0] & 0x3f);
        set_prg_bank(0xa000, bank_8k, s.prg[1] & 0x3f);
        set_prg_bank(0xc000, bank_8k, s.prg[2] & 0x3f);
        set_prg_bank(0xe000, bank_8k, last_bank);

        if (is_175())
            enable_sram(true, (s.write_protect & 1) == 0);
        else
            enable_sram(false);

        update_nt();
    }

    void set_340_mirroring(int mode)
    {
        switch (mode & 3)
        {
            case 0:
                s.nt[0] = s.nt[1] = s.nt[2] = s.nt[3] = 0x80;
                break;
            case 1:
                s.nt[0] = 0x80; s.nt[1] = 0x81;
                s.nt[2] = 0x80; s.nt[3] = 0x81;
                break;
            case 2:
                s.nt[0] = s.nt[1] = s.nt[2] = s.nt[3] = 0x81;
                break;
            case 3:
                s.nt[0] = s.nt[1] = 0x80;
                s.nt[2] = s.nt[3] = 0x81;
                break;
        }
    }

public:
    Mapper210()
    {
        register_state(&s, sizeof s);
    }

    void reset_state()
    {
        memset(&s, 0, sizeof s);
        set_header_mirror();

        if (!is_175())
            for (int i = 0; i < 4; ++i)
                s.nt[i] |= 0x80;
    }

    void apply_mapping()
    {
        update();
    }

    void write(nes_time_t, nes_addr_t addr, int data)
    {
        int reg = addr & 0xf800;

        if (reg >= 0x8000 && reg <= 0xb800)
        {
            int bank = (reg - 0x8000) >> 11;
            s.chr[bank] = (uint8_t)data;
        }
        else
        {
            switch (reg)
            {
                case 0xc000:
                case 0xc800:
                case 0xd000:
                case 0xd800:
                    if (is_175())
                    {
                        s.write_protect = (uint8_t)data;
                    }
                    else
                    {
                        int page = (reg - 0xc000) >> 11;
                        if ((data & 0xff) >= 0xe0)
                            s.nt[page] = (uint8_t)(0x80 | (data & 1));
                        else
                            s.nt[page] = (uint8_t)data;
                    }
                    break;

                case 0xe000:
                    s.prg[0] = (uint8_t)(data & 0x3f);
                    if (!is_175())
                        set_340_mirroring((data >> 6) & 3);
                    break;

                case 0xe800:
                    s.prg[1] = (uint8_t)(data & 0x3f);
                    break;

                case 0xf000:
                    s.prg[2] = (uint8_t)(data & 0x3f);
                    break;
            }
        }

        update();
    }
};
