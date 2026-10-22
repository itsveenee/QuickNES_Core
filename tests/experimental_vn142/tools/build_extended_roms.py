#!/usr/bin/env python3
"""Extra ROM tests for 219 extended register format and 249 full permutations."""
import importlib.util,sys,hashlib,json
from pathlib import Path
here=Path(__file__).resolve().parent
old=here/'build_diagnostic_roms.py'
spec=importlib.util.spec_from_file_location('old_builder',old);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
out=here.parent/'smoke';out.mkdir(parents=True,exist_ok=True)

def imm_sta(p,a,v):p.imm(0xa9,v);p.abs(0x8d,a)

def check_bank(p,addr,exp,n):
    p.abs(0xad,addr);p.imm(0xc9,exp)
    p.branch(0xf0,'ok_bank_'+str(n));p.imm(0xa2,n);p.abs(0x8e,0x0012);p.jmp('fail');p.mark('ok_bank_'+str(n))

def chr_check(p, expected, n):
    p.imm(0xa9,0);p.abs(0x8d,0x2006)
    p.imm(0xa9,0x80);p.abs(0x8d,0x2006)
    p.abs(0xad,0x2007);p.abs(0xad,0x2007);p.abs(0x8d,0x0013)
    p.imm(0xc9,m.chr_marker(expected))
    p.branch(0xf0,'chr_pass_'+str(n))
    p.imm(0xa2,100+n);p.abs(0x8e,0x0012);p.jmp('fail')
    p.mark('chr_pass_'+str(n))

def end(p):
    p.imm(0xa2,0x2a);p.jmp('color')
    p.mark('fail');p.imm(0xa2,0x16)
    p.mark('color');p.imm(0xa9,0x3f);p.abs(0x8d,0x2006);p.imm(0xa9,0);p.abs(0x8d,0x2006)
    p.abs(0x8e,0x2007);p.abs(0x8e,0x2007)
    p.mark('end');p.jmp('end')
    return p.finish()

def begin():
    p=m.A();p.emit(0x78,0xd8);p.imm(0xa2,0);p.abs(0x8e,0x2000);p.abs(0x8e,0x2001)
    return p

def build219(preserve_extended=False):
    p=begin()
    imm_sta(p,0x8002,0x25);imm_sta(p,0x8001,0x2c)
    # permutation from FCEUmm mapper219.c, 0x2c -> $0d
    check_bank(p,0xb000,0x2d,1)
    imm_sta(p,0x5001,0)
    check_bank(p,0xb000,0x0d,2)
    imm_sta(p,0x8002,0x26);imm_sta(p,0x8001,0x38)
    check_bank(p,0x9000,7,3)
    imm_sta(p,0x5001,0x20)
    imm_sta(p,0x8000,0x08);imm_sta(p,0x8001,0x03)
    imm_sta(p,0x8000,0x09);imm_sta(p,0x8001,0x08)
    chr_check(p,0x134,1)
    if not preserve_extended:
        imm_sta(p,0x8002,0x08) # exit extended mode (bit5 cleared)
        imm_sta(p,0x8000,0);imm_sta(p,0x8001,0x54)
        chr_check(p,0x154,2)
    return end(p)

def build249(canonical):
    p=begin();n=1
    prgpat=[[3,4,2,1],[4,3,1,2],[1,2,3,4],[2,1,4,3]]
    chrpat=[[5,2,6,7,4,3],[4,5,3,2,7,6],[2,3,4,5,6,7],
            [6,4,2,3,7,5],[5,3,7,6,2,4],[4,2,5,6,7,3],[3,6,4,5,2,7],[2,5,6,7,3,4]]
    dest=prgpat[2 if canonical else 0];destchr=chrpat[2 if canonical else 0]
    imm_sta(p,0x8000,7)
    for v in (0x12,0x23):
        imm_sta(p,0x8001,v)
        for mode in range(4):
            imm_sta(p,0x5000,mode)
            check_bank(p,0xb000,m.reorder(v,prgpat[mode],dest),n);n+=1
    imm_sta(p,0x8000,0)
    for v in (0x24,0x4a):
        imm_sta(p,0x8001,v)
        for mode in range(8):
            imm_sta(p,0x5000,mode)
            chr_check(p,m.reorder(v&~1,chrpat[mode],destchr),n);n+=1
    return end(p)

def save(tag,mid,sub,program):
    assert len(program)<0x1000
    prg=bytearray()
    for bank in range(64):
      page=bytearray([0xea]*8192);page[:len(program)]=program
      page[0x1000]=bank
      page[0x1ffa:0x2000]=bytes([0,0x80,0,0x80,0,0x80]);prg+=page
    h=bytearray(16);h[:4]=b'NES\x1a';h[4]=32;h[5]=64;h[6]=(mid&15)<<4;h[7]=(mid&0xf0)|0x08;h[8]=(sub<<4)|((mid>>8)&15)
    raw=bytes(h)+bytes(prg)+m.chr_data
    f=out/f'Aurora_Extended_Diagnostic_{tag}.nes';f.write_bytes(raw)
    return dict(name=f.name,sha256=hashlib.sha256(raw).hexdigest(),length=len(raw),program=len(program))

manifest=[save('219',219,0,build219()),save('219_ext_active',219,0,build219(True)),save('249',249,0,build249(False)),save('4_sub5',4,5,build249(True))]
(out/'extended_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('PASS created',len(manifest),'extended ROMs',[(x['name'],x['program']) for x in manifest])