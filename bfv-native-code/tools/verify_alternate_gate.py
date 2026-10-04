"""Compare alternate target gate and nested-score initialization.

Actual target eligibility, event predicate and interface lookup execute;
object methods and the component-view conversion remain controlled.
"""
import itertools
from verify_target_eligibility import run_eligibility
from verify_target_eligibility import ADDRESSES
import argparse, hashlib, json, struct
from pathlib import Path
import pefile
from build import TARGETS, PROJECT
from verify_category_score import GAME

def alternate_cases():
    cases=[{},dict(flags=0),dict(flags=1),dict(flags=7),dict(flags=0xffffffff),
        dict(flags=0x80000000),dict(handle=0),dict(handle=0x20001),dict(null_object=True),
        dict(no_component=True),dict(predicates=[0,0]),dict(direct=0),dict(scratch=True),
        dict(gate_mutation='record'),dict(gate_mutation='receiver'),dict(gate_mutation='frame')]
    cases += [dict(metric=v,classification=v) for v in [0,0xffffffff,0x80000000,0x7f812345]]
    cases += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(
        [{},dict(flags=0),dict(predicates=[0,0]),dict(gate_mutation='record')],
        [0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,5])]
    return [dict(c,gate=True) for c in cases]

def compare_alternate_gate(original,original_pe,edited,edited_pe,spec,symbols):
    entry=spec['artillery_alternate_gate'];guard=bytes.fromhex('8b4d10c1e903')
    assert original_pe.get_data(entry-original_pe.OPTIONAL_HEADER.ImageBase,len(guard))==guard
    patched=edited_pe.get_data(entry-edited_pe.OPTIONAL_HEADER.ImageBase,len(guard))
    assert patched[0]==0xe9 and entry+5+struct.unpack('<i',patched[1:5])[0]==symbols['bfv_artillery_alternate_gate_bridge']
    assert patched[5:]==b'\x90'*(len(guard)-5)
    addresses=ADDRESSES['client' if spec['target_handle_eligible']==0x97ede0 else 'server']
    results=[]
    for i,case in enumerate(alternate_cases()):
        old=run_eligibility(original,original_pe,spec,addresses,case)
        new=run_eligibility(edited,edited_pe,spec,addresses,case)
        assert old==new,(i,case,{k:(old[k],new[k]) for k in old if old[k]!=new[k]})
        results.append(dict(inputs=case,calls=old['calls'],accepted=old['accepted']))
    return results

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--game-dir',type=Path,default=GAME)
    parser.add_argument('--target',choices=['client','server','both'],default='both');args=parser.parse_args()
    for target in ['client','server'] if args.target=='both' else [args.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target
        manifest=json.loads((work/'manifest.json').read_text(encoding='utf-8'))
        original=(args.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        results=compare_alternate_gate(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'alternate-gate-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(results),cases=results),indent=2)+'\n',encoding='utf-8')
        print(f'{target}: {len(results)} alternate gate comparisons passed',flush=True)
