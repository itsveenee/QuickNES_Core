#pragma once
/* SNESticleAurora / QuickNES — reconstructed mapper 303, Kaiser KS7017.
 * Source: negativeExponent FCEUmm-next, src/mappers/mapper303.c.
 * PRG $8000=16 KiB latch-committed, $C000=bank 2. 8 KiB SRAM at $6000.
 * CPU-cycle one-shot IRQ: $4020/$4021, acknowledge at $4030.
 * Missing: FDS expansion sound. Not yet tested with a real cartridge ROM.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
#include <string.h>
class AuroraVn29Mapper303 : public Nes_Mapper {
    // All mapper state stored as bytes to avoid padding/endian state issues.
    // 0 latch, 1 committed PRG, 2 mirror, 3/4 counter LE, 5 enabled, 6 pending.
    uint8_t s[7];
    nes_time_t cursor;
    unsigned counter() const { return unsigned(s[3]) | (unsigned(s[4]) << 8); }
    void set_counter(unsigned v) { s[3] = uint8_t(v); s[4] = uint8_t(v >> 8); }
    void sync_prg() { set_prg_bank(0x8000,bank_16k,s[1]); set_prg_bank(0xc000,bank_16k,2); }
    void sync_mirror() { if(s[2]&8)mirror_horiz();else mirror_vert(); }
    void ack() { s[6]=0; irq_changed(); }
public:
    AuroraVn29Mapper303():cursor(0) { register_state(s,sizeof s); }
    virtual void reset_state() { memset(s,0,sizeof s); cursor=mapper_clock(); }
    virtual void apply_mapping() {
        enable_sram();sync_prg();sync_mirror();
        intercept_writes(0x4000,0x2000);
        intercept_reads(0x4030,1);
        irq_changed();
    }
    virtual void run_until(nes_time_t t) {
        if(t<=cursor)return;
        const unsigned delta=(unsigned)(t-cursor);
        if(s[5]) {
            const unsigned remaining=counter()?counter():1U;
            if(delta>=remaining) {
                set_counter(0);s[5]=0;s[6]=1;
                irq_changed();
            } else set_counter(counter()-delta);
        }
        cursor=t;
    }
    virtual nes_time_t next_irq(nes_time_t t) {
        run_until(t);
        if(s[6])return t;
        if(!s[5])return no_irq;
        return cursor + nes_time_t(counter()?counter():1U);
    }
    virtual void end_frame(nes_time_t length) {
        run_until(length);
        cursor-=length;
    }
    virtual int read(nes_time_t t,nes_addr_t addr) {
        if(addr!=0x4030) return Nes_Mapper::read(t,addr);
        run_until(t);
        const int result=s[6]?1:0;
        ack();
        return result;
    }
    virtual bool write_intercepted(nes_time_t t,nes_addr_t addr,int data) {
        if(addr==0x4020) {run_until(t);ack();set_counter((counter()&0xff00U)|(data&255));return true;}
        if(addr==0x4021) {run_until(t);ack();set_counter((counter()&0x00ffU)|((unsigned(data)&255U)<<8));s[5]=1;irq_changed();return true;}
        if(addr==0x4025) {s[2]=(uint8_t)data;sync_mirror();return true;}
        if(addr>=0x4a00 && addr<=0x4aff) {
            s[0]=uint8_t(((addr>>4)&4)|((addr>>2)&3));return true;
        }
        if(addr>=0x5100 && addr<=0x51ff) {
            s[1]=s[0];sync_prg();return true;
        }
        return false;
    }
    virtual void write(nes_time_t,nes_addr_t,int) {}
    virtual void save_state(mapper_state_t &out) {
        run_until(mapper_clock());Nes_Mapper::save_state(out);
    }
    virtual void read_state(mapper_state_t const& in) {
        Nes_Mapper::read_state(in);
        s[5]=uint8_t(s[5]!=0);s[6]=uint8_t(s[6]!=0);
        cursor=mapper_clock();irq_changed();
    }
};
