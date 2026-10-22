/* vN215 - 93C66 serial EEPROM behavioral boundary regression for mapper558.
 * Tests the same serial input/address bit stream that QuickNES receives from
 * $5200/$5600. No direct write_persistent() is used to bypass protection.
 */
#include <cstdio>
#include <cstring>
#include <stdint.h>
#include "nes_emu/mappers/aurora_vn112_mapper558_model.hpp"
static int errors=0,checks=0;
static void expect(bool cond,const char* what){
  ++checks;
  if(!cond){++errors;std::printf("FAIL %s\n",what);}
}
static void edge(Aurora93C66Stage& m,bool bit){
  m.signal(true,false,bit);
  m.signal(true,true,bit);
}
static void end(Aurora93C66Stage& m){m.signal(false,false,false);}
static void instruction(Aurora93C66Stage& m,unsigned op,unsigned addr){
  end(m);
  edge(m,true);                    // 93C66 start bit
  edge(m,(op>>1)&1U);edge(m,op&1U);
  for(int bit=8;bit>=0;--bit)edge(m,(addr>>bit)&1U);
}
static void write_byte(Aurora93C66Stage& m,unsigned addr,unsigned value){
  instruction(m,1,addr);
  for(int bit=7;bit>=0;--bit)edge(m,(value>>bit)&1U);
  end(m);
}
static unsigned read_byte(Aurora93C66Stage& m,unsigned addr){
  instruction(m,2,addr);
  unsigned data=0;
  for(int i=0;i<8;++i){
    edge(m,false);
    data=(data<<1)|(m.output()?1U:0U);
  }
  end(m);
  return data;
}
int main(){
  Aurora93C66Stage m;
  uint8_t external[512];std::memset(external,0xff,sizeof external);
  expect(m.bind_persistence(external,sizeof external),"bind 512 EEPROM bytes");
  expect(!m.bind_persistence(external,511),"reject too-small backing size");
  write_byte(m,5,0x5a);
  expect(external[5]==0xff,"WRITE rejected before EWEN");
  instruction(m,0,0x180);end(m);           // EWEN
  write_byte(m,5,0xa5);
  expect(external[5]==0xa5,"EWEN permits WRITE");
  expect(read_byte(m,5)==0xa5,"serial READ observes stored byte");
  write_byte(m,511,0x42);
  expect(external[511]==0x42 && read_byte(m,511)==0x42,"boundary address 511");
  write_byte(m,0,0x81);
  expect(external[0]==0x81 && read_byte(m,0)==0x81,"boundary address 0");
  uint8_t snapshot[Aurora93C66Stage::snapshot_size];
  instruction(m,2,5);
  edge(m,false);edge(m,false);
  m.save(snapshot);
  Aurora93C66Stage restored;
  expect(restored.bind_persistence(external,sizeof external),"rebind EEPROM memory");
  restored.load(snapshot);
  unsigned suffix=0;
  for(int i=0;i<6;++i){edge(restored,false);suffix=(suffix<<1)|(restored.output()?1U:0U);}
  expect(suffix==(0xa5&0x3f),"READ serial protocol roundtrip mid-byte");
  end(restored);
  instruction(restored,0,0x000);end(restored); // EWDS
  write_byte(restored,5,0x66);
  expect(external[5]==0xa5,"EWDS disables writes");
  instruction(restored,3,5);end(restored);
  expect(external[5]==0xa5,"ERASE rejected while EWDS");
  instruction(restored,0,0x180);end(restored); // EWEN
  instruction(restored,3,5);end(restored);
  expect(external[5]==0xff,"ERASE after EWEN");
  write_byte(restored,12,0x4e);
  instruction(restored,0,0x100);end(restored); // ERAL
  expect(external[12]==0xff && external[0]==0xff && external[511]==0xff,"ERAL clears all 512 bytes");
  // WRAL: special address top bits 01, data byte is repeated to all words.
  instruction(restored,0,0x080);
  for(int bit=7;bit>=0;--bit)edge(restored,(0x36U>>bit)&1U);
  end(restored);
  expect(external[0]==0x36 && external[5]==0x36 && external[511]==0x36,"WRAL fills all 512 bytes");
  restored.unbind_persistence();
  expect(!restored.external_persistence_bound() && restored.peek(511)==0x36,
         "disconnecting NVRAM preserves internal bytes");
  std::printf("vN215 EEPROM 93C66: %d checks, %d failures\n",checks,errors);
  return errors?1:0;
}
