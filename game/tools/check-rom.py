"""Validate the actual release ROM, including both standard checksums."""
import hashlib,json,sys
from pathlib import Path
p=Path(sys.argv[1]);rom=p.read_bytes()
assert len(rom)==262144, f'Expected 256 KiB, got {len(rom)}'
assert rom[0x134:0x143].rstrip(b'\0')==b'CIPHERSPACE', 'Expected CIPHERSPACE cartridge title'
assert rom[0x143]==0xC0, 'Expected Color-only header'
assert rom[0x147]==0x1B, 'Expected MBC5 + SRAM + battery'
assert rom[0x148]==3 and rom[0x149]==2, 'ROM or RAM size header mismatch'
check=0
for b in rom[0x134:0x14D]:check=(check-b-1)&255
assert check==rom[0x14D], 'Header checksum mismatch'
assert ((sum(rom)-rom[0x14E]-rom[0x14F])&65535)==int.from_bytes(rom[0x14E:0x150],'big'), 'Global checksum mismatch'
report={'file':p.name,'title':'CIPHERSPACE','bytes':len(rom),'sha256':hashlib.sha256(rom).hexdigest(),'color_only':True,'mapper':'MBC5 + battery RAM','ram_bytes':8192,'checksums':'passed'}
p.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
