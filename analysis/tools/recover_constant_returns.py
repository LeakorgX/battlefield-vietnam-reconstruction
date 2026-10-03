"""Generate editable C from audited, complete constant-word return entries.

This generator does not run during builds: hand edits remain intact. Native
semantic return types and names of ignored inputs are not inferred from EAX.
"""
import argparse
import csv
import hashlib
import json
import re
import struct
from pathlib import Path
import pefile

ROOT=Path(__file__).resolve().parents[2]
HASHES={'client':('BfVietnam.exe','79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5'),
        'server':('bfvietnam_w32ded.exe','86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d')}

def recover(game_dir,audit_dir,overwrite=False):
    source_path=ROOT/'bfv-native-code/src/constant_returns.c'
    if source_path.exists() and not overwrite:raise RuntimeError('Source already exists; use --overwrite only to intentionally replace edits.')
    catalog=dict(scope='Complete constant EAX-return bodies reconstructed as editable C; returned semantic types and ignored argument names remain unknown. Original-image address values retain original data dependencies.',targets={})
    source=['/* Individually editable constant-return functions. Generated once from audited',
            ' * complete entries; the build does not regenerate this file. Static volatile',
            ' * values force a MOV load instead of a zeroing XOR, preserving native flags.',
            ' * uint32_t represents EAX bits; semantic pointer/integer types are not guessed. */',
            '#include <stdint.h>','#include "target.h"','#define SC __attribute__((stdcall))','']
    for target,(filename,expected_hash) in HASHES.items():
        image=(game_dir/filename).read_bytes();assert hashlib.sha256(image).hexdigest()==expected_hash
        pe=pefile.PE(data=image);rows=[];source.append('#if BFV_BUILD_'+target.upper())
        audit=audit_dir/(target+'-audit.tsv')
        with audit.open(encoding='utf-8') as f:
            for row in csv.DictReader(f,delimiter='\t'):
                if row['status']!='eligible':continue
                address=int(row['address'],16);value=int(row['constant'],16);cleanup=int(row['stack_cleanup']);size=int(row['indexed_bytes'])
                assert address%16==0 and cleanup%4==0 and cleanup<=64 and int(row['interior_references'])==0
                expected=b'\xb8'+struct.pack('<I',value)+(b'\xc3' if cleanup==0 else b'\xc2'+struct.pack('<H',cleanup))
                assert len(expected)==size and pe.get_data(address-0x400000,size)==expected
                symbol=f'bfv_{target}_return_word_{address:08x}'
                in_image=0x400000<=value<0x400000+pe.OPTIONAL_HEADER.SizeOfImage
                identifier=None
                if in_image:
                    try:
                        text=pe.get_data(value-0x400000,96).split(b'\0',1)[0].decode('ascii')
                        if re.fullmatch(r'[A-Za-z_][A-Za-z0-9_:<>.]{1,95}',text):identifier=text
                    except (UnicodeDecodeError,pefile.PEFormatError):pass
                item=dict(address=f'{address:08x}',symbol=symbol,word=f'{value:08x}',stack_cleanup=cleanup,
                    guarded_prefix=expected[:5].hex(),entry_references=int(row['entry_references']),
                    original_image_value=in_image)
                if identifier:item['observed_identifier']=identifier
                rows.append(item)
                comment=f'/* Native {address:08x}; EAX word {value:08x}; RET {cleanup}.'
                if identifier:comment+=' Original data identifier: '+identifier+'.'
                elif in_image:comment+=' Value falls within original image; data dependency retained.'
                source.append(comment+' */')
                args=', '.join(f'uint32_t unused{n} __attribute__((unused))' for n in range(cleanup//4)) or 'void'
                source += [f'uint32_t SC {symbol}({args})','{',
                           f'    static const volatile uint32_t result = 0x{value:08x}u;',
                           '    return result;','}','']
        source += ['#endif','']
        catalog['targets'][target]=dict(original_sha256=expected_hash,audit_sha256=hashlib.sha256(audit.read_bytes()).hexdigest(),entries=rows)
        (ROOT/'reports'/target/'constant-return-audit.tsv').write_bytes(audit.read_bytes())
    source_path.write_text('\n'.join(source),encoding='utf-8')
    (ROOT/'bfv-native-code/constant-returns.json').write_text(json.dumps(catalog,indent=2)+'\n',encoding='utf-8')
    print({target:len(data['entries']) for target,data in catalog['targets'].items()})

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,required=True)
    p.add_argument('--audit-dir',type=Path,required=True);p.add_argument('--overwrite',action='store_true');args=p.parse_args()
    recover(args.game_dir,args.audit_dir,args.overwrite)
