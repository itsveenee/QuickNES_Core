#pragma once
/* Mapper 43 / YS-612, FDS cartridge conversion.
 * Reference: FCEUmm-next mapper043.c, version d94fa342.
 * $5000-$5FFF is a 2KiB ROM page mirrored twice. No external FDS required.
 * No unsupported global CPU/PPU changes; mapper-owned IRQ and DIP state.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
#include <string.h>
class AuroraVn41Mapper043 : public Nes_Mapper {
    uint8_t s[8]; // bank/IRQmode/countLE2/pending/DIP/powered/reserved
    nes_time_t cursor;
    unsigned count() const {return (unsigned)s[2] | ((unsigned)s[3]<<8);}
    void count_set(unsigned c){s[2]=(uint8_t)c;s[3]=(uint8_t)(c>>8);}
    unsigned dip_bank() const {return 32U+(unsigned)(s[5]&3U);}
    void sync(){
        static const uint8_t order[8]={4,3,4,4,4,7,5,6};
        // Write-through to CPU opcode map; regular DATA reads handled by read().
        set_prg_bank(0x5000,bank_2k,(int)dip_bank());
        set_prg_bank(0x5800,bank_2k,(int)dip_bank());
        set_prg_bank(0x6000,bank_8k,2);
        set_prg_bank(0x8000,bank_8k,1);
        set_prg_bank(0xa000,bank_8k,0);
        set_prg_bank(0xc000,bank_8k,order[s[0]&7U]);
        set_prg_bank(0xe000,bank_8k,9);
        set_chr_bank(0,bank_8k,0);
    }
public:
    AuroraVn41Mapper043():cursor(0){memset(s,0,sizeof s);register_state(s,sizeof s);}
    virtual void reset_state(){
        memset(s,0,sizeof s);s[5]=1;s[6]=1;cursor=mapper_clock();
    }
    virtual void reset(){
        if(!s[6]){Nes_Mapper::reset();return;}
        // Real mapper 43 reset advances the DIP switch; does NOT clear IRQ.
        s[5]=(uint8_t)((s[5]+1U)&3U);cursor=mapper_clock();sync();irq_changed();
    }
    virtual void apply_mapping(){
        sync();intercept_reads(0x5000,0x1000);
        intercept_writes(0x4000,0x1000);
        irq_changed();
    }
    virtual int read(nes_time_t,nes_addr_t a){
        if(a<0x5000 || a>=0x6000)return -1;
        const unsigned long size=(unsigned long)cart().prg_size();
        if(!size)return -1;
        unsigned long n=size/2048UL;
        const unsigned long offset=n ?
            ((unsigned long)dip_bank()%n)*2048UL+((unsigned long)a&0x7ffUL) :
            ((unsigned long)a&0x7ffUL)%size;
        if(offset>=size)return -1;
        return (int)cart().prg()[offset];
    }
    virtual bool write_intercepted(nes_time_t t,nes_addr_t a,int data){
        const unsigned decoded=(unsigned)a&0xf1ffU;
        if(decoded==0x4022){s[0]=(uint8_t)data;sync();return true;}
        if(decoded==0x4122){write(t,a,data);return true;}
        return false;
    }
    virtual void write(nes_time_t t,nes_addr_t a,int data){
        if(((unsigned)a&0xf1ffU)==0x4122U ||
           ((unsigned)a&0xf1ffU)==0x8122U){
            run_until(t);
            s[1]=(uint8_t)data;
            if(s[1]&2U)count_set(0);
            if(!(s[1]&1U))s[4]=0;
            irq_changed();
        }
    }
    virtual void run_until(nes_time_t t){
        if(t<=cursor)return;
        unsigned long delta=(unsigned long)(t-cursor);
        unsigned old=count();
        // FCEUmm counts every CPU cycle, including when IRQ is disabled.
        // IRQ begins when a sampled count reaches >=4096 while enabled.
        if((s[1]&1U) && (old>=4096U || delta>=4096UL-old))s[4]=1;
        count_set((unsigned)((old+delta)&65535UL));
        cursor=t;
    }
    virtual nes_time_t next_irq(nes_time_t t){
        run_until(t);
        if(s[4])return t;
        if(!(s[1]&1U))return no_irq;
        unsigned c=count();
        return t+nes_time_t(c>=4096U ? 1U : 4096U-c);
    }
    virtual void end_frame(nes_time_t length){run_until(length);cursor-=length;}
    virtual void save_state(mapper_state_t&out){
        run_until(mapper_clock());Nes_Mapper::save_state(out);
    }
    virtual void read_state(mapper_state_t const&in){
        Nes_Mapper::read_state(in);cursor=mapper_clock();irq_changed();
    }
};
