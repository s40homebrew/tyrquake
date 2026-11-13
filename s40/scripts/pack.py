#!/usr/bin/env python3
"""Flatten ELF32 PT_LOAD segments and extract ONLY R_ARM_RELATIVE fixups.
No stock dlopen dependency, no dynamic import lookup, no ROM patching.
"""
import struct
from pathlib import Path
P=Path(__file__).resolve().parents[1]
b=(P/'build/quake.elf').read_bytes()
u16=lambda p:struct.unpack_from('<H',b,p)[0]
u32=lambda p:struct.unpack_from('<I',b,p)[0]
assert b[:7]==b'\x7fELF\x01\x01\x01' and u16(16)==3 and u16(18)==40
phoff,shoff=u32(28),u32(32)
loads=[]
for i in range(u16(44)):
 t,off,va,pa,fs,ms,fl,al=struct.unpack_from('<8I',b,phoff+i*u16(42))
 if t==1:loads.append((off,va,fs,ms))
assert min(x[1] for x in loads)==0
mem=(max(va+ms for _,va,_,ms in loads)+3)&~3
end=(max(va+fs for _,va,fs,_ in loads)+3)&~3
image=bytearray(end)
for off,va,fs,ms in loads:image[va:va+fs]=b[off:off+fs]
rel=[]
for i in range(u16(48)):
 sh=struct.unpack_from('<10I',b,shoff+i*u16(46))
 if sh[1]==9: # SHT_REL
  for p in range(sh[4],sh[4]+sh[5],sh[9]):
   off,info=struct.unpack_from('<II',b,p)
   assert info==23, f'unsupported relocation {info:#x} at {off:#x}'
   assert off%4==0 and off+4<=end
   addend=struct.unpack_from('<I',image,off)[0]
   assert (addend&~1)<mem, f'non-image relative addend {addend:#x}'
   rel.append(off)
assert len(set(rel))==len(rel)
entry=u32(24)
assert 0<entry<end and len(rel)>0
header=struct.pack('<8I',0x30443453,1,end,mem,entry,len(rel),196608,0)
(P/'build/quake.bin').write_bytes(header+image+struct.pack('<%dI'%len(rel),*rel))
print(f'Native image: {end} file bytes, {mem} memory bytes, {len(rel)} relative relocations; entry {entry:#x}')
