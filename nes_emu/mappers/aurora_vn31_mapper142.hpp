#pragma once
/* Aurora QuickNES / mapper 142, Kaiser KS7032 with KS202 ASIC.
 * IRQ latch is four writes of 4/4/4/8 address nibbles ($8000-$BFFF);
 * bank command $E000, bank value $F000.  No PRG WRAM on KS7032.
 * Sources: FCEUmm-next mapper142.c and src/mappers/hw/ks202.c.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
#include <string.h>
class AuroraVn31Mapper142 : public Nes_Mapper {
    uint8_t s[23]; /* regs[8], command, 32-bit latch, 32-bit count, enabled, pending, pad[5] */
    nes_time_t cursor;
    unsigned long get_u32(unsigned pos)const{
        return (unsigned long)s[pos]|((unsigned long)s[pos+1]<<8)|
               ((unsigned long)s[pos+2]<<16)|((unsigned long)s[pos+3]<<24);
    }
    void put_u32(unsigned pos,unsigned long v){
        s[pos]=(uint8_t)v;s[pos+1]=(uint8_t)(v>>8);
        s[pos+2]=(uint8_t)(v>>16);s[pos+3]=(uint8_t)(v>>24);
    }
    void sync(){
        set_prg_bank(0x6000,bank_8k,s[4]&0x0f);
        set_prg_bank(0x8000,bank_8k,s[1]&0x0f);
        set_prg_bank(0xa000,bank_8k,s[2]&0x0f);
        set_prg_bank(0xc000,bank_8k,s[3]&0x0f);
        set_prg_bank(0xe000,bank_8k,15);
    }
    void ack(){s[18]=0;irq_changed();}
public:
    AuroraVn31Mapper142():cursor(0){register_state(s,sizeof s);}
    virtual void reset_state(){memset(s,0,sizeof s);cursor=mapper_clock();}
    virtual void apply_mapping(){sync();irq_changed();}
    virtual void run_until(nes_time_t t){
        if(t<=cursor)return;
        if(s[17]&&!s[18]){
            const unsigned long count=get_u32(13),delta=(unsigned long)(t-cursor);
            const unsigned long limit=count<65535UL?65535UL-count:1UL;
            if(delta>=limit){put_u32(13,get_u32(9));s[17]=0;s[18]=1;irq_changed();}
            else put_u32(13,count+delta);
        }
        cursor=t;
    }
    virtual nes_time_t next_irq(nes_time_t t){
        run_until(t);
        if(s[18])return t;
        if(!s[17])return no_irq;
        const unsigned long n=get_u32(13);
        return cursor+nes_time_t(n<65535UL?65535UL-n:1UL);
    }
    virtual void end_frame(nes_time_t length){run_until(length);cursor-=length;}
    virtual void write(nes_time_t t,nes_addr_t a,int data){
        run_until(t);
        if(a<0x8000)return;
        const unsigned long d=(unsigned long)(unsigned(data)&255U);
        unsigned long latch=get_u32(9);
        switch(a&0xf000){
        case 0x8000:put_u32(9,(latch&0xfff0UL)|(d&15UL));break;
        case 0x9000:put_u32(9,(latch&0xff0fUL)|((d&15UL)<<4));break;
        case 0xa000:put_u32(9,(latch&0xf0ffUL)|((d&15UL)<<8));break;
        case 0xb000:put_u32(9,(latch&0x0fffUL)|(d<<12));break;
        case 0xc000:
            s[17]=(uint8_t)(d&15UL);if(s[17])put_u32(13,latch);ack();break;
        case 0xd000:ack();break;
        case 0xe000:s[8]=(uint8_t)(d&7UL);break;
        case 0xf000:s[s[8]]=(uint8_t)d;sync();break;
        }
    }
    virtual void save_state(mapper_state_t&out){run_until(mapper_clock());Nes_Mapper::save_state(out);}
    virtual void read_state(mapper_state_t const&in){
        Nes_Mapper::read_state(in);cursor=mapper_clock();irq_changed();
    }
};
