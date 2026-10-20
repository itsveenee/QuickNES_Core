#pragma once
/* Aurora QuickNES / mapper 50, FDS conversion with CPU cycle IRQ.
 * PRG 8 KiB at $6000 and four PRG windows above $8000.
 * IRQ counts enabled CPU M2 cycles, asserts upon reaching bit 12; clearing
 * enable clears the IRQ and count. Source: FCEUmm-next mapper050.c.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
#include <string.h>
class AuroraVn31Mapper050 : public Nes_Mapper {
    uint8_t r[8]; /* bank, enable, pending, reserved, 32-bit elapsed little endian */
    unsigned long cycles;
    nes_time_t cursor;
    void sync() {
        const unsigned p=((r[0]&1U)<<2)|((r[0]&2U)>>1)|((r[0]&4U)>>1)|(r[0]&8U);
        set_prg_bank(0x6000,bank_8k,15);
        set_prg_bank(0x8000,bank_8k,8);
        set_prg_bank(0xa000,bank_8k,9);
        set_prg_bank(0xc000,bank_8k,(int)p);
        set_prg_bank(0xe000,bank_8k,11);
    }
public:
    AuroraVn31Mapper050():cycles(0),cursor(0){register_state(r,sizeof r);}
    virtual void reset_state(){memset(r,0,sizeof r);cycles=0;cursor=mapper_clock();}
    virtual void apply_mapping(){sync();intercept_writes(0x4000,0x1000);irq_changed();}
    virtual void run_until(nes_time_t time){
        if(time<=cursor)return;
        if(r[1]&&!r[2]) {
            const unsigned long delta=(unsigned long)(time-cursor);
            const unsigned long remain=4096UL-(cycles&4095UL);
            if(delta>=remain){r[2]=1;cycles+=remain;irq_changed();}
            else cycles+=delta;
        }
        cursor=time;
    }
    virtual nes_time_t next_irq(nes_time_t time){
        run_until(time);
        if(r[2])return time;
        if(!r[1])return no_irq;
        return cursor+nes_time_t(4096UL-(cycles&4095UL));
    }
    virtual void end_frame(nes_time_t duration){run_until(duration);cursor-=duration;}
    virtual bool write_intercepted(nes_time_t time,nes_addr_t addr,int data){
        if(addr<0x4020||addr>=0x5000)return false;
        switch(addr&0xd160) {
        case 0x4120:
            run_until(time);r[1]=(uint8_t)(data&1);
            if(!r[1]){cycles=0;r[2]=0;}irq_changed();return true;
        case 0x4020:
            r[0]=(uint8_t)data;sync();return true;
        }
        return false;
    }
    virtual void write(nes_time_t,nes_addr_t,int) {}
    virtual void save_state(mapper_state_t&out){
        run_until(mapper_clock());
        /* Save the live IRQ elapsed count as 32-bit little endian. */
        r[4]=(uint8_t)cycles;r[5]=(uint8_t)(cycles>>8);
        r[6]=(uint8_t)(cycles>>16);r[7]=(uint8_t)(cycles>>24);
        Nes_Mapper::save_state(out);
    }
    virtual void read_state(mapper_state_t const&in){
        Nes_Mapper::read_state(in);
        cycles=(unsigned long)r[4]|((unsigned long)r[5]<<8)|
               ((unsigned long)r[6]<<16)|((unsigned long)r[7]<<24);
        cursor=mapper_clock();irq_changed();
    }
};
