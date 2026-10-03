"""Focused original/reconstructed collision callback comparison."""
import argparse
import hashlib
import json
from pathlib import Path
import pefile
from build import PROJECT, GAME, TARGETS
from collision_oracle import run_collision, collision_cases
from dispatch_oracle import run_dispatcher, dispatcher_cases


def compare_collision(original, original_pe, edited, edited_pe, spec, symbols):
    results = []
    for case in collision_cases():
        key='collision_notify' if case.get('notify_only',False) else 'collision'
        a = run_collision(original, original_pe, spec[key], spec, case)
        b = run_collision(edited, edited_pe, symbols['bfv_'+key], spec, case,
                          dispatch_entry=symbols['bfv_collision_dispatch'],
                          helpers=dict(pool=symbols['bfv_object_lookup'],interface=symbols['bfv_event_interface'],construct=symbols['bfv_collision_construct'],insert=symbols['bfv_vector_insert']))
        assert a == b, (case,a,b)
        results.append(dict(inputs=case,**a))
    return results


def compare_dispatcher(original, original_pe, edited, edited_pe, spec, symbols):
    results = []
    for case in dispatcher_cases():
        a = run_dispatcher(original, original_pe, spec['collision_dispatch'], spec, case)
        b = run_dispatcher(edited, edited_pe, symbols['bfv_collision_dispatch'], spec, case,
                           distance_entry=symbols['bfv_collision_distance'],
                           helpers=dict(pool=symbols['bfv_object_lookup'],interface=symbols['bfv_event_interface'],construct=symbols['bfv_collision_construct'],insert=symbols['bfv_vector_insert']))
        assert a == b, (case, a, b)
        results.append(dict(inputs=case, **a))
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
        dispatch = compare_dispatcher(original,pefile.PE(data=original),edited,pefile.PE(data=edited),
                                      spec,{k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'dispatch-verification.json').write_text(json.dumps(dispatch,indent=2))
        (work/'collision-verification.json').write_text(json.dumps(results,indent=2))
        n=sum(bool(x['inputs'].get('notify_only',False)) for x in results)
        print(f'{target}: {len(results)-n} collision callback and {n} notification and {len(dispatch)} dispatcher comparisons passed',flush=True)
