"""Focused original/reconstructed collision callback comparison."""
import argparse
import hashlib
import json
from pathlib import Path
import pefile
from build import PROJECT, GAME, TARGETS
from collision_oracle import run_collision, collision_cases


def compare_collision(original, original_pe, edited, edited_pe, spec, symbols):
    results = []
    for case in collision_cases():
        a = run_collision(original, original_pe, spec['collision'], spec, case)
        b = run_collision(edited, edited_pe, symbols['bfv_collision'], spec, case)
        assert a == b, (case,a,b)
        results.append(dict(inputs=case,**a))
    return results


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--game-dir',type=Path,default=GAME)
    args = parser.parse_args()
    for target,spec in TARGETS.items():
        work = PROJECT/'build'/target
        manifest = json.loads((work/'manifest.json').read_text())
        original = (args.game_dir/spec['file']).read_bytes()
        edited = Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest() == spec['sha']
        assert hashlib.sha256(edited).hexdigest() == manifest['output_sha256']
        results = compare_collision(original,pefile.PE(data=original),edited,pefile.PE(data=edited),
                                    spec,{k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'collision-verification.json').write_text(json.dumps(results,indent=2))
        print(f'{target}: {len(results)} collision callback comparisons passed',flush=True)
