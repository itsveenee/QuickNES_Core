#pragma once
/* Aurora QuickNES / mapper 304, UNL-SMB2J 09-034A.
 * PRG at $6000: bank 4 or 5. IRQ starts with $4068 and asserts after
 * the first cycle beyond 5750; `$4020..$4FFF` reads return FF.
 * Reference: negativeExponent/FCEUmm-next mapper304.c.
 * IRQ models per-cycle behavior; ROM boot tests still pending.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
#include <string.h>
class AuroraVn31Mapper304 : public Nes_Mapper {
    uint8_t r[7]; /* PRG latch; enable; pending; 32-bit little-endian cycle count */
    nes_time_t cursor;
    unsigned long count()const{
        return (unsigned long)r[3]|((unsigned long)r[4]<<8)|
               ((unsigned long)r[5]<<16)|((unsigned long)r[6]<<24);
    }
    void set_count(unsigned long n){
        r[3]=(uint8_t)n;r[4]=(uint8_t)(n>>8);r[5]=(uint8_t)(n>>16);r[6]=(uint8_t)(n>>24);
    }
    void sync(){set_prg_bank(0x6000,bank_8k,4|(r[0]&1));set_prg_bank(0x8000,bank_32k,0);}
public:
    AuroraVn31Mapper304():cursor(0){register_state(r,sizeof r);}
    virtual void reset_state(){memset(r,0,sizeof r);cursor=mapper_clock();}
    virtual void apply_mapping(){
        sync();intercept_writes(0x4000,0x1000);intercept_reads(0x4000,0x1000);irq_changed();
    }
    virtual void run_until(nes_time_t t){
        if(t<=cursor)return;
        if(r[1]&&!r[2]){
            const unsigned long n=count(),delta=(unsigned long)(t-cursor);
            /* A one-cycle hook on 5750 increments to 5751 and asserts
               on its NEXT invocation. */
            const unsigned long left=n<5750UL?5751UL-n:1UL;
            if(delta>=left){set_count(n+left);r[1]=0;r[2]=1;irq_changed();}
            else set_count(n+delta);
        }
        cursor=t;
    }
    virtual nes_time_t next_irq(nes_time_t t){
        run_until(t);
        if(r[2])return t;
        if(!r[1])return no_irq;
        return cursor+nes_time_t(count()<5750UL?5751UL-count():1UL);
    }
    virtual void end_frame(nes_time_t elapsed){run_until(elapsed);cursor-=elapsed;}
    virtual int read(nes_time_t t,nes_addr_t a){
        if(a>=0x4020&&a<0x5000){run_until(t);return 0xff;}
        return Nes_Mapper::read(t,a);
    }
    virtual bool write_intercepted(nes_time_t t,nes_addr_t a,int v){
        if(a==0x4027){r[0]=(uint8_t)(v&1);sync();return true;}
        if(a==0x4068){run_until(t);r[1]=(uint8_t)(v&1);r[2]=0;set_count(0);irq_changed();return true;}
        return false;
    }
    virtual void write(nes_time_t,nes_addr_t,int){}
    virtual void save_state(mapper_state_t&out){run_until(mapper_clock());Nes_Mapper::save_state(out);}
    virtual void read_state(mapper_state_t const&in){
        Nes_Mapper::read_state(in);cursor=mapper_clock();irq_changed();
    }
};
