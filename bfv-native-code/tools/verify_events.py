"""Focused constructor and object/event interface comparisons."""
import argparse
import hashlib
import json
from pathlib import Path
import pefile
from build import PROJECT, GAME, TARGETS
from event_oracle import run_constructor, constructor_cases, run_lookup, lookup_cases


def compare_events(original, original_pe, edited, edited_pe, spec, symbols):
    results=[]
    for case in constructor_cases():
        a=run_constructor(original,original_pe,spec['collision_construct'],spec,case)
        b=run_constructor(edited,edited_pe,symbols['bfv_collision_construct'],spec,case)
        assert a==b,(case,a,b)
        results.append(dict(kind='constructor',inputs=case,**a))
    for kind,case in lookup_cases():
        native=spec['collision_pool_entry' if kind=='object' else 'collision_event_interface']
        symbol=symbols['bfv_object_lookup' if kind=='object' else 'bfv_event_interface']
        a=run_lookup(original,original_pe,native,kind,case)
        b=run_lookup(edited,edited_pe,symbol,kind,case)
        assert a==b,(kind,case,a,b)
        results.append(dict(kind=kind,inputs=case,result=a))
    return results


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME);args=p.parse_args()
    for target,spec in TARGETS.items():
        work=PROJECT/'build'/target;manifest=json.loads((work/'manifest.json').read_text())
        original=(args.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha']
        assert hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        results=compare_events(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
                              {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'event-verification.json').write_text(json.dumps(results,indent=2))
        print(f'{target}: {len(results)} event constructor/lookup comparisons passed',flush=True)
