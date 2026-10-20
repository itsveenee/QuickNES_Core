#pragma once
/* Aurora QuickNES vN32 / Subor mapper 167.
 * FCEUmm-next mapper167.c: sibling of 166, with reversed 32K mode and
 * fixed C000 bank 0x20 instead of bank 7.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
#include <string.h>
class AuroraVn32Mapper167 : public Nes_Mapper {
    uint8_t r[4];
    void sync(){
        const int base=((r[0]^r[1])&0x10)<<1;
        int bank=(r[2]^r[3])&0x1f;
        if(r[1]&8){
            bank&=~1;
            set_prg_bank(0x8000,bank_16k,base+bank+1);
            set_prg_bank(0xc000,bank_16k,base+bank);
        }else if(r[1]&4){
            set_prg_bank(0x8000,bank_16k,31);
            set_prg_bank(0xc000,bank_16k,base+bank);
        }else{
            set_prg_bank(0x8000,bank_16k,base+bank);
            set_prg_bank(0xc000,bank_16k,32);
        }
    }
public:
    AuroraVn32Mapper167(){register_state(r,sizeof r);}
    virtual void reset_state(){memset(r,0,sizeof r);}
    virtual void apply_mapping(){enable_sram();sync();}
    virtual void write(nes_time_t,nes_addr_t a,int data){
        if(a<0x8000)return;
        r[(a>>13)&3]=(uint8_t)data;sync();
    }
};
