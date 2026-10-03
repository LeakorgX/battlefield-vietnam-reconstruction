"""Compare recompiled machine code to original machine code under the same inputs."""
import argparse
import hashlib
import itertools
import json
from pathlib import Path
import pefile
from build import PROJECT, GAME, TARGETS
from native_oracle import run_case, run_dispatch
from bailout_oracle import run_bailout
from verify_curves import compare_curves
from verify_geometry import compare_geometry
from verify_collision import compare_collision, compare_dispatcher


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
    recompute_cases=[]
    metrics=(-1.0,0.0,0.001,0.5,0.999,1.0,2.0)
    scales=(-2.0,0.0,0.3333333,1.0,100.0)
    # Exercise numeric combinations where influence is used, then guard paths,
    # low-byte predicates and callback mutations without redundant cross products.
    inputs=[(metric,influence,scale,1,1,changing,False)
            for metric,influence,scale,changing in itertools.product(
                metrics,(-0.0,0.01,0.125,1.0,1.3333333),scales,(False,True))]
    inputs += [(metric,1.3333333,scale,group,special,changing,False)
               for metric,scale,(group,special),changing in itertools.product(
                   metrics,scales,((0,1),(1,0)),(False,True))]
    inputs += [(0.3333333,0.125,1.3333333,group,special,changing,relocate)
               for group,special,changing,relocate in itertools.product(
                   (0,1,256,257),(0,1,256,257),(False,True),(False,True))]
    for args in inputs:
        try:
            a=run_bailout(original,original_pe,spec['native_bailout'],spec,*args)
            b=run_bailout(edited,edited_pe,symbols['bfv_bailout'],spec,*args)
            assert a==b,(args,a,b)
        except Exception as error:
            raise RuntimeError(f'{target} bailout recomputation input {args}') from error
        recompute_cases.append(dict(inputs=args,**a))
    curve_comparisons=compare_curves(original,original_pe,edited,edited_pe,spec,symbols)
    geometry_comparisons=compare_geometry(original,original_pe,edited,edited_pe,spec,symbols)
    collision_comparisons=compare_collision(original,original_pe,edited,edited_pe,spec,symbols)
    dispatch_comparisons=compare_dispatcher(original,original_pe,edited,edited_pe,spec,symbols)
    notify_count=sum(bool(x['inputs'].get('notify_only',False)) for x in collision_comparisons)
    report=dict(target=target,original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],
        interpreter_cases=len(cases),cached_bailout_cases=32,vehicle_wrapper_cases=32,
        bailout_recompute_cases=len(recompute_cases),curve_cases=len(curve_comparisons),geometry_cases=len(geometry_comparisons),
        collision_cases=len(collision_comparisons)-notify_count,collision_notify_cases=notify_count,collision_dispatch_cases=len(dispatch_comparisons),
        passed=len(cases)+len(rating_cases)+len(recompute_cases)+len(curve_comparisons)+len(collision_comparisons)+len(dispatch_comparisons)+len(geometry_comparisons),
        scope='Interpreter with controlled methods/context/event helpers. Bailout control flow and reconstructed math compared; object methods controlled. Curves checked at 80-bit precision across rounding/precision modes and synthetic/original-initialized tables. Collision callback and notification helper compared with controlled services, exact call arguments, flag-word preservation, counter wrap and callback mutations. Dispatcher actor filtering, generation checks, dynamic count, strict distance gates and event arguments compared with controlled event services and both controlled/real geometry. Geometry helpers compared at 80-bit precision across rounding/precision modes. Table initialization, event services and vehicle scoring remain native.',
        interpreter=cases,ratings=rating_cases,bailout_recomputation=recompute_cases,curves=curve_comparisons,collision=collision_comparisons,dispatch=dispatch_comparisons,geometry=geometry_comparisons)
    (work/'verification.json').write_text(json.dumps(report,indent=2))
    print(f'{target}: {len(cases)} interpreter, 32 cached-bailout, {len(recompute_cases)} recomputed-bailout, {len(curve_comparisons)} curve, {len(geometry_comparisons)} geometry, {len(collision_comparisons)-notify_count} collision, {notify_count} notification and {len(dispatch_comparisons)} dispatcher comparisons, 32 vehicle-wrapper ABI checks passed',flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--game-dir',type=Path,default=GAME)
    args=parser.parse_args()
    GAME=args.game_dir.resolve()
    for target in ('client','server'): verify(target)
