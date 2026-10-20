#pragma once
/* Kaiser KS-7010 FDS Castlevania conversion (NES2 mapper 554).
 * Source FCEUmm-next mapper554.c (2026). No FDS audio, DIP or accessory.
 * High-ROM CPU *data* reads are notified by a narrow opt-in page hook.
 * Instruction/opcode fetches are still direct and not notified. The
 * reset-vector-side effect is initialized explicitly before vector fetch.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
class AuroraVn54Mapper554 : public Nes_Mapper {
    uint8_t bank;
    bool powered;
    void sync() {
        set_prg_bank(0x6000,bank_8k,bank);
        set_prg_bank(0x8000,bank_8k,10);
        set_prg_bank(0xa000,bank_8k,11);
        set_prg_bank(0xc000,bank_8k,6);
        set_prg_bank(0xe000,bank_8k,7);
        set_chr_bank(0,bank_8k,bank);
        mirror_vert();
    }
public:
    AuroraVn54Mapper554() : bank(0), powered(false) {register_state(&bank,1);}
    virtual void reset_state() {
        /* Core::read_vector($FFFC) bypasses mapper reads. Hardware's
           $FFFC read selects bank F; establish the equivalent *before*
           Core::read_vector so the first opcode sees the proper CHR/PRG. */
        bank=15;
        intercept_reads(0xc000,0x4000);
    }
    virtual void apply_mapping() {sync();}
    virtual void reset() {
        uint8_t kept=powered?bank:15;
        const bool is_soft=powered;
        Nes_Mapper::reset();
        if(is_soft) {bank=kept;sync();}
        powered=true;
    }
    virtual int read(nes_time_t,nes_addr_t address) {
        const unsigned a=address&0xfffeU;
        unsigned selected=bank;
        if((address&0xf000U)==0xc000U && a>=0xcab6U && a<=0xcad6U)
            selected=(a>>2)&15U;
        else if((address&0xf000U)==0xe000U && (a==0xebe2U || a==0xee32U))
            selected=(address>>2)&15U;
        else if(a==0xfffcU)
            selected=(address>>2)&15U;
        if(selected!=bank) {bank=(uint8_t)selected;sync();}
        /* Let Nes_Core re-read code_map AFTER this bank update. */
        return -1;
    }
    virtual void write(nes_time_t,nes_addr_t,int) { }
};
