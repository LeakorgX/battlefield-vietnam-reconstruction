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
from verify_events import compare_events
from verify_vectors import compare_vectors
from verify_artillery import compare_artillery
from verify_artillery_cache import compare_artillery_cache
from verify_target_history import compare_target_history
from verify_artillery_filter import compare_artillery_filter
from verify_scalar_vector_math import compare_scalar_vector_math
from verify_aim_limits import compare_aim_limits
from verify_affine_matrix import compare_affine_matrix
from verify_constant_returns import compare_constant_returns
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
    print(f'{target}: interpreter, cached ratings and bailout recomputation passed',flush=True)
    print(f'{target}: checking numerical curves',flush=True)
    curve_comparisons=compare_curves(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: checking geometry',flush=True)
    geometry_comparisons=compare_geometry(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: checking collision callback and notification',flush=True)
    collision_comparisons=compare_collision(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: checking actor dispatcher',flush=True)
    dispatch_comparisons=compare_dispatcher(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: checking event construction and lookups',flush=True)
    event_comparisons=compare_events(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: checking vector insertion and exception bridge',flush=True)
    vector_comparisons=compare_vectors(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: checking artillery driver',flush=True)
    artillery_comparisons=compare_artillery(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: checking artillery cached-target evaluation',flush=True)
    artillery_cache_comparisons=compare_artillery_cache(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: checking target history',flush=True)
    history_comparisons=compare_target_history(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: checking inline artillery candidate filter',flush=True)
    filter_comparisons=compare_artillery_filter(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: checking shared scalar/vector math',flush=True)
    math_comparisons=compare_scalar_vector_math(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(math_comparisons)} shared scalar/vector math comparisons passed',flush=True)
    print(f'{target}: checking aiming limits',flush=True)
    aim_comparisons=compare_aim_limits(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(aim_comparisons)} aiming-limit comparisons passed',flush=True)
    print(f'{target}: checking affine matrix composition',flush=True)
    matrix_comparisons=compare_affine_matrix(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(matrix_comparisons)} affine matrix comparisons passed',flush=True)
    print(f'{target}: checking constant-return entries',flush=True)
    constant_comparisons=compare_constant_returns(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(constant_comparisons)} constant-return comparisons passed',flush=True)
    notify_count=sum(bool(x['inputs'].get('notify_only',False)) for x in collision_comparisons)
    report=dict(target=target,original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],
        shared_math_cases=len(math_comparisons),artillery_filter_cases=len(filter_comparisons),target_history_cases=len(history_comparisons),artillery_cache_cases=len(artillery_cache_comparisons),artillery_cases=len(artillery_comparisons),interpreter_cases=len(cases),cached_bailout_cases=32,vehicle_wrapper_cases=32,
        bailout_recompute_cases=len(recompute_cases),curve_cases=len(curve_comparisons),geometry_cases=len(geometry_comparisons),event_cases=len(event_comparisons),vector_cases=len(vector_comparisons),
        collision_cases=len(collision_comparisons)-notify_count,collision_notify_cases=notify_count,collision_dispatch_cases=len(dispatch_comparisons),
        passed=len(cases)+len(rating_cases)+len(recompute_cases)+len(curve_comparisons)+len(collision_comparisons)+len(dispatch_comparisons)+len(geometry_comparisons)+len(event_comparisons)+len(vector_comparisons)+len(artillery_comparisons)+len(artillery_cache_comparisons)+len(history_comparisons)+len(filter_comparisons)+len(math_comparisons),
        scope='Interpreter with controlled methods/context/event helpers. Bailout control flow and reconstructed math compared; object methods controlled. Curves checked at 80-bit precision across rounding/precision modes and synthetic/original-initialized tables. Collision callback and notification helper compared with controlled services, exact call arguments, flag-word preservation, counter wrap and callback mutations. Dispatcher actor filtering, generation checks, dynamic count, strict distance gates and event arguments compared with controlled event services and both controlled/real geometry. Geometry helpers compared at 80-bit precision across rounding/precision modes. Event constructor, pool generation lookup and indexed interface lookup compared with controlled vector growth and manager methods; six collision/dispatcher cases execute real event construction/lookups, with two using real vector insertion. Vector insertion, helper calls, native-compatible handler metadata and catch funclets compared with controlled allocation, memmove and simulated fault dispatch. Eight constructor cases execute real insertion. Table initialization, allocation/CRT exception runtime, remaining event services and vehicle scoring remain native. Artillery driver routing, notification-dependent predicate, call arguments, low-byte flags, float32 stores and callback table mutations compared; artillery target evaluation and component conversion are controlled native dependencies. Cached-target validation executes real original/reconstructed branches, including generation checks, pattern rejection/reset, flag/cache updates, captured vtables, callback mutations, register preservation and x87 rounding/precision modes. Eight driver cases execute real cached evaluation; candidate searches remain native and are checked only for ABI and 80-bit forwarding. Target-history exact unsigned lookup and default timestamp construction compared through guarded entry detours, with real original tree traversal and controlled allocation/insertion callbacks. First-pass inline candidate filter compared at original accept/reject continuations with controlled identity/metric/history services, exact live frame/nonvolatile outputs and full x87 status across selected precision/rounding modes. Remaining candidate/scoring phases stay native.',
        interpreter=cases,ratings=rating_cases,bailout_recomputation=recompute_cases,curves=curve_comparisons,collision=collision_comparisons,dispatch=dispatch_comparisons,geometry=geometry_comparisons,events=event_comparisons,vectors=vector_comparisons,artillery=artillery_comparisons,artillery_cache=artillery_cache_comparisons,history=history_comparisons,artillery_filter=filter_comparisons,shared_math=math_comparisons)
    report['scope'] += ' Shared scalar selectors and vector length/division execute actual original/reconstructed entries without numeric service mocks, comparing extended returns, memory, ABI and full x87 status with occupied caller registers across selected exceptional inputs and all supported precision/rounding modes.'
    report['aim_cases']=len(aim_comparisons)
    report['aim']=aim_comparisons
    report['passed']+=len(aim_comparisons)
    report['matrix_cases']=len(matrix_comparisons)
    report['matrix']=matrix_comparisons
    report['passed']+=len(matrix_comparisons)
    report['constant_return_cases']=len(constant_comparisons)
    report['constant_returns']=constant_comparisons
    report['passed']+=len(constant_comparisons)
    report['scope']+=' Aiming event routing and angular-range predicate compared with controlled transform/trig services, callback mutations and x87 status; selected cases execute actual original/source matrix composition and retained inverse-sine runtime. Complete aiming/firing behavior remains unverified.'
    report['scope']+=' Affine matrix composition compared without math mocks, including full backing memory, partial/full buffer overlap, ABI, x87 status/control and retained caller values across all supported precision/rounding modes.'
    report['scope']+=' Audited complete constant-return bodies execute actual original/source entries, comparing EAX bits, stack cleanup, all non-result integer registers, CPU flags, complete x87 state and no writes; original-image return values retain data dependencies.'
    (work/'verification.json').write_text(json.dumps(report,indent=2))
    print(f'{target}: {len(cases)} interpreter, 32 cached-bailout, {len(recompute_cases)} recomputed-bailout, {len(curve_comparisons)} curve, {len(geometry_comparisons)} geometry, {len(collision_comparisons)-notify_count} collision, {notify_count} notification and {len(dispatch_comparisons)} dispatcher and {len(event_comparisons)} event and {len(vector_comparisons)} vector comparisons, {len(artillery_comparisons)} artillery driver comparisons, {len(artillery_cache_comparisons)} artillery evaluator/integration comparisons, {len(history_comparisons)} target-history comparisons, {len(filter_comparisons)} inline candidate-filter comparisons, 32 vehicle-wrapper ABI checks passed',flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--game-dir',type=Path,default=GAME)
    parser.add_argument("--target",choices=("client","server","both"),default="both")
    args=parser.parse_args()
    GAME=args.game_dir.resolve()
    for target in ('client','server') if args.target=='both' else (args.target,): verify(target)
