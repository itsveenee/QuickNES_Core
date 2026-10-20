#pragma once
/* SPDX-License-Identifier: GPL-2.0-or-later
 * Aurora vN45 mapper 553, Sachen 3013 (Penguin and Seal Chinese release).
 * $8000-$BFFF is physically an address-independent $3A read value, for
 * opcode fetches as well as data accesses. $C000-$FFFF is 16KiB PRG ROM 0.
 * No separate audio, DIP or extra peripheral.
 * Reference: FCEUmm-next src/mappers/mapper553.c.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
#include <string.h>
class AuroraVn45Mapper553 : public Nes_Mapper {
    uint8_t fixed_page[Nes_Cpu::page_size];
public:
    AuroraVn45Mapper553() { memset(fixed_page,0x3A,sizeof fixed_page); }
    virtual void apply_mapping() {
        set_prg_bank(0xC000,bank_16k,0);
        set_chr_bank(0,bank_8k,0);
        for(unsigned address=0x8000;address<0xC000;address+=Nes_Cpu::page_size)
            map_cpu_rom_page(address,fixed_page);
    }
    virtual void write(nes_time_t,nes_addr_t,int) { }
};
