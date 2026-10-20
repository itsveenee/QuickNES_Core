#pragma once
/* Aurora QuickNES vN43, NES 2.0 mapper 42 submapper 2 (AC-08/LH09).
 * The supported board converts Green Beret FDS to a standard 60/72-pin
 * cartridge. This class intentionally does not emulate submappers 1 or 3
 * because those involve distinct sound/IRQ designs. No DIP switch, FDS
 * drive, copier hardware or other external peripheral is used.
 * Reference: FCEUmm-next mapper042.c (GPL-2.0-or-later).
 */
#include "Nes_Mapper.h"
#include <stdint.h>
class AuroraVn43Mapper042 : public Nes_Mapper {
    uint8_t r[2]; // PRG select at $8001, mirroring at $4025
    void sync() {
        const int bank_count_16k = cart().prg_size() / 0x4000;
        set_prg_bank(0x6000, bank_8k, (r[0] >> 1) & 0x0f);
        set_prg_bank(0x8000, bank_32k,
                     (bank_count_16k & 7) ? 4 : 7);
        if (cart().chr_size() == 0)
            set_chr_ram_bank(0, bank_8k, 0);
        else
            set_chr_bank(0, bank_8k, 0);
        if (r[1] & 8) mirror_vert();
        else mirror_horiz();
    }
public:
    AuroraVn43Mapper042() {
        r[0] = r[1] = 0;
        register_state(r, sizeof r);
    }
    virtual void reset_state() { r[0] = r[1] = 0; }
    virtual void apply_mapping() {
        sync();
        intercept_writes(0x4000, 0x1000); // ONLY $4025 is actually handled
    }
    virtual bool write_intercepted(nes_time_t, nes_addr_t addr, int value) {
        if ((addr & 0xf001) != 0x4001 || (addr & 0xff) != 0x25)
            return false;
        r[1] = (uint8_t)value;
        if (r[1] & 8) mirror_vert(); else mirror_horiz();
        return true;
    }
    virtual void write(nes_time_t, nes_addr_t addr, int value) {
        if ((addr & 0xf001) != 0x8001) return;
        r[0] = (uint8_t)value;
        set_prg_bank(0x6000, bank_8k, (r[0] >> 1) & 0x0f);
    }
};
