#pragma once
/* Aurora QuickNES vN32 / Subor mapper 166.
 * Independent conversion of FCEUmm-next mapper166.c, not the unfinished
 * staging variant. Four $8000-$FFFF 8K write regions, two 16K PRG slots.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
#include <string.h>
class AuroraVn32Mapper166 : public Nes_Mapper {
    uint8_t r[4];
    void sync(){
        const int base=((r[0]^r[1])&0x10)<<1;
        int bank=(r[2]^r[3])&0x1f;
        if(r[1]&8) {
            bank&=~1;
            set_prg_bank(0x8000,bank_16k,base+bank);
            set_prg_bank(0xc000,bank_16k,base+bank+1);
        }else if(r[1]&4){
            set_prg_bank(0x8000,bank_16k,31);
            set_prg_bank(0xc000,bank_16k,base+bank);
        }else{
            set_prg_bank(0x8000,bank_16k,base+bank);
            set_prg_bank(0xc000,bank_16k,7);
        }
    }
public:
    AuroraVn32Mapper166(){register_state(r,sizeof r);}
    virtual void reset_state(){memset(r,0,sizeof r);}
    virtual void apply_mapping(){enable_sram();sync();}
    virtual void write(nes_time_t,nes_addr_t a,int data){
        if(a<0x8000)return;
        r[(a>>13)&3]=(uint8_t)data;sync();
    }
};
