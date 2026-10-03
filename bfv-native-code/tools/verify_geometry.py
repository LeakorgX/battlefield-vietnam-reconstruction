"""Focused geometry comparison, also consumed by the complete verifier."""
import argparse
import hashlib
import json
from pathlib import Path
import pefile
from build import PROJECT, GAME, TARGETS
from geometry_oracle import run_geometry, geometry_cases


def compare_geometry(original, original_pe, edited, edited_pe, spec, symbols):
    results=[]
    for kind in ('line_distance_squared','collision_distance'):
        native=spec['collision_line_distance' if kind=='line_distance_squared' else 'collision_distance']
        for words,cw in geometry_cases():
            a=run_geometry(original,original_pe,native,words,cw)
            b=run_geometry(edited,edited_pe,symbols['bfv_'+kind],words,cw)
            assert a==b,(kind,words,hex(cw),a,b)
            results.append(dict(kind=kind,words=[f'{w:08x}' for w in words],control_word=f'{cw:04x}',result80=a))
    return results


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME);args=p.parse_args()
    for target,spec in TARGETS.items():
        work=PROJECT/'build'/target;manifest=json.loads((work/'manifest.json').read_text())
        original=(args.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha']
        assert hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        results=compare_geometry(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
                                 {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'geometry-verification.json').write_text(json.dumps(results,indent=2))
        print(f'{target}: {len(results)} exact x87 geometry comparisons passed',flush=True)
