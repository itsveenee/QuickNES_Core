#pragma once
/* SPDX-License-Identifier: GPL-2.0-or-later
 * Mapper 223 / Waixing Type I. Standard MMC3 banking + extra onboard WRAM.
 * 8 KiB at $6000, 4 KiB visible at $5000; no DIP or extra peripherals.
 * Sources: Nestopia NstBoardWaixing.cpp and MAME nes_ines.hxx.
 */
#include "mapper004.hpp"
#include <stdint.h>
class AuroraVn48Mapper223 : public Mapper004 {
protected:
    enum { low_base=0x2000, low_size=0x1000 };
    void enable_low_window() {
        /* Force physical SRAM allocation first; preserve normal MMC3 $6000
         * gating and ensure $5000 opcode fetches use same SRAM backing. */
        enable_sram(true);
        map_sram_mirror(0x5000,low_size,low_base,low_size);
        Mapper004::write(0,0xA001,sram_mode);
        intercept_reads(0x5000,low_size);
        intercept_writes(0x5000,low_size);
    }
public:
    virtual long aurora_extra_sram_save_size()const{return 0x3000;}
    virtual void apply_mapping(){Mapper004::apply_mapping();enable_low_window();}
    virtual int read(nes_time_t,nes_addr_t a){
        if(a>=0x5000 && a<0x6000)
            return sram_data_ptr()[low_base+(a-0x5000)];
        return -1;
    }
    virtual bool write_intercepted(nes_time_t,nes_addr_t a,int d){
        if(a>=0x5000 && a<0x6000){
            sram_data_ptr()[low_base+(a-0x5000)]=(uint8_t)d;
            return true;
        }
        return false;
    }
};
