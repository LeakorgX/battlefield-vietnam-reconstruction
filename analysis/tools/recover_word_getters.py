"""Recover individually editable thiscall word getters from audited complete bodies."""
import argparse
import csv
import hashlib
import json
import struct
from pathlib import Path
import pefile
from recover_constant_returns import ROOT,HASHES

def recover(game_dir,audit_dir,overwrite=False):
    destination=ROOT/'bfv-native-code/src/word_getters.c'
    if destination.exists() and not overwrite:raise RuntimeError('Source exists; --overwrite intentionally replaces edits.')
    source=['/* Complete ECX object-field getters. Field meanings and owning classes remain',
            ' * unproven. Packed volatile loads retain unaligned word reads and native flags.',
            ' * This file is editable and is not regenerated during builds. */',
            '#include <stdint.h>','#include "target.h"',
            'struct __attribute__((packed)) BfvWord { uint32_t value; };',
            '#define TC __attribute__((thiscall))','']
    catalog=dict(scope='Complete thiscall EAX-word field getters. Object ownership and semantic field types remain unresolved.',targets={})
    for target,(filename,digest) in HASHES.items():
        image=(game_dir/filename).read_bytes();assert hashlib.sha256(image).hexdigest()==digest
        pe=pefile.PE(data=image);audit=audit_dir/(target+'-audit.tsv');entries=[]
        source.append('#if BFV_BUILD_'+target.upper())
        with audit.open(encoding='utf-8') as f:
            for row in csv.DictReader(f,delimiter='\t'):
                if row['status']!='eligible':continue
                address=int(row['address'],16);offset=int(row['field_offset'])
                assert address%16==0 and int(row['stack_cleanup'])==0 and int(row['indexed_bytes'])==7
                assert int(row['interior_references'])==0 and 128<=offset<=0x7fffffff
                body=b'\x8b\x81'+struct.pack('<I',offset)+b'\xc3'
                assert pe.get_data(address-0x400000,7)==body
                symbol=f'bfv_{target}_read_word_{address:08x}'
                entries.append(dict(address=f'{address:08x}',symbol=symbol,field_offset=offset,
                    guarded_prefix=body[:5].hex(),native_body=body.hex(),entry_references=int(row['entry_references'])))
                source += [f'/* Native {address:08x}; object word at byte offset {offset}. */',
                    f'uint32_t TC {symbol}(const void *object)','{',
                    f'    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + {offset}u);',
                    '    return field->value;','}','']
        source += ['#endif','']
        catalog['targets'][target]=dict(original_sha256=digest,audit_sha256=hashlib.sha256(audit.read_bytes()).hexdigest(),entries=entries)
        (ROOT/'reports'/target/'word-getter-audit.tsv').write_bytes(audit.read_bytes())
    destination.write_text('\n'.join(source),encoding='utf-8')
    (ROOT/'bfv-native-code/word-getters.json').write_text(json.dumps(catalog,indent=2)+'\n',encoding='utf-8')
    print({t:len(d['entries']) for t,d in catalog['targets'].items()})

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,required=True)
    p.add_argument('--audit-dir',type=Path,required=True);p.add_argument('--overwrite',action='store_true')
    a=p.parse_args();recover(a.game_dir,a.audit_dir,a.overwrite)
