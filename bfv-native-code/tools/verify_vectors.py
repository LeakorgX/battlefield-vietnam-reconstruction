"""Vector arithmetic, native exception metadata and catch-funclet comparison."""
import argparse
import hashlib
import json
from pathlib import Path
import pefile
from build import PROJECT,GAME,TARGETS
from vector_oracle import run_vector,vector_cases,run_handler


def compare_vectors(original,original_pe,edited,edited_pe,spec,symbols):
    results=[]
    a=run_handler(original,original_pe,spec['vector_original_handler'],spec['vector_frame_handler'])
    b=run_handler(edited,edited_pe,symbols['bfv_vector_handler'],spec['vector_frame_handler'])
    assert a==b,(a,b);results.append(dict(kind='handler',info=a))
    for case in vector_cases():
        a=run_vector(original,original_pe,spec['collision_vector_insert'],spec,case)
        b=run_vector(edited,edited_pe,symbols['bfv_vector_insert'],spec,case,symbols)
        assert a==b,(case,a,b)
        results.append(dict(kind='insertion',inputs=case,**a))
    return results


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME);args=p.parse_args()
    for target,spec in TARGETS.items():
        work=PROJECT/'build'/target;manifest=json.loads((work/'manifest.json').read_text())
        original=(args.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha']
        assert hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        results=compare_vectors(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
                                {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'vector-verification.json').write_text(json.dumps(results,indent=2))
        print(f'{target}: {len(results)} vector/exception-bridge comparisons passed',flush=True)
