"""Compare recompiled machine code to original machine code under the same inputs."""
import argparse
import hashlib
import itertools
import json
from pathlib import Path
import pefile
from build import PROJECT, GAME, TARGETS
from native_oracle import run_case, run_dispatch


def verify(target):
    spec=TARGETS[target]
    work=PROJECT/'build'/target
    manifest=json.loads((work/'manifest.json').read_text())
    original=(GAME/spec['file']).read_bytes()
    edited=Path(manifest['output']).read_bytes()
    assert hashlib.sha256(original).hexdigest()==spec['sha']
    assert hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
    original_pe,edited_pe=pefile.PE(data=original),pefile.PE(data=edited)
    symbols={k:int(v,16) for k,v in manifest['symbols'].items()}
    native_addresses={k:spec[k] for k in ('pool','interpreter','context_begin','context_end','context_record')}
    compiled_addresses={**native_addresses,'interpreter':symbols['bfv_interpret']}
    cases=[]
    combinations=itertools.product((0,1,7,31,33),(0,3),(False,True),(False,True),
        (False,True),(0,1),(False,True),(False,True),((1,0),(0,0),(1,1)))
    for args in combinations:
        try:
            a=run_dispatch(original,original_pe,native_addresses,*args)
            b=run_dispatch(edited,edited_pe,compiled_addresses,*args)
        except Exception as error:
            raise RuntimeError(f'{target} interpreter input {args}') from error
        assert a==b,(args,a,b)
        cases.append(dict(inputs=args,result=a['result'],flags=a['flags'],calls=a['calls']))
    rating_cases=[]
    ratings=[-100.,-0.,0.,0.125,1.,1.25,0.333333333,123.456]*4
    for kind in ('bailout','vehicle'):
        for index in range(32):
            if kind=='bailout':
                a=run_case(original,original_pe,spec['native_'+kind],index,ratings)
                b=run_case(edited,edited_pe,symbols['bfv_'+kind],index,ratings)
            else:
                # The underlying scoring function remains unmodified. Test the
                # compiled wrapper's thiscall forwarding and returned float bits.
                a=run_case(original,original_pe,spec['native_'+kind],index,ratings,delegated=spec['native_'+kind])
                b=run_case(edited,edited_pe,symbols['bfv_'+kind],index,ratings,delegated=spec['native_'+kind])
            assert a==b,(kind,index,a,b)
            rating_cases.append(dict(kind=kind,**a))
    report=dict(target=target,original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],
        interpreter_cases=len(cases),cached_bailout_cases=32,vehicle_wrapper_cases=32,passed=len(cases)+len(rating_cases),
        scope='Original and compiled interpreter instructions compared with controlled methods/context/event helpers. Cached bailout compared exactly. Vehicle wrapper forwarding/float bits tested with controlled native callee; full scoring remains native.',
        interpreter=cases,ratings=rating_cases)
    (work/'verification.json').write_text(json.dumps(report,indent=2))
    print(f'{target}: {len(cases)} interpreter comparisons, 32 cached-bailout comparisons, 32 vehicle-wrapper ABI checks passed')


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--game-dir',type=Path,default=GAME)
    args=parser.parse_args()
    GAME=args.game_dir.resolve()
    for target in ('client','server'): verify(target)
