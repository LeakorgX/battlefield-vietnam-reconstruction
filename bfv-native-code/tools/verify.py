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
from verify_history_tree import compare_history_tree
from verify_artillery_filter import compare_artillery_filter
from verify_artillery_weapons import compare_artillery_weapons
from verify_artillery_movement import compare_artillery_movement
from verify_movement_score import compare_movement_score
from verify_artillery_query import compare_artillery_query
from verify_region_geometry import compare_region_geometry
from verify_artillery_region_score import compare_artillery_region_score
from verify_category_score import compare_category_score
from verify_target_eligibility import compare_target_eligibility
from verify_alternate_gate import compare_alternate_gate
from verify_nested_score import compare_nested_score
from verify_alternate_finish import compare_alternate_finish
from verify_target_traversal import compare_target_traversal
from verify_next_candidate import compare_next_candidate
from verify_second_setup import compare_second_setup
from verify_second_filter import compare_second_filter
from verify_second_weight import compare_second_weight
from verify_vector_normalize import compare_vector_normalize
from verify_second_aim import compare_second_aim
from verify_second_position import compare_second_position
from verify_second_distance import compare_second_distance
from verify_second_weapons import compare_second_weapons
from verify_second_movement import compare_second_movement
from verify_second_movement_score import compare_second_movement_score
from verify_second_driver import compare_second_driver
from verify_second_region import compare_second_region
from verify_scalar_vector_math import compare_scalar_vector_math
from verify_aim_limits import compare_aim_limits
from verify_aim_geometry import compare_aim_geometry
from verify_affine_matrix import compare_affine_matrix
from verify_constant_returns import compare_constant_returns
from verify_word_getters import compare_word_getters
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
    print(f'{target}: checking target-history insertion and balancing',flush=True)
    tree_comparisons=compare_history_tree(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(tree_comparisons)} history-tree comparisons passed',flush=True)
    print(f'{target}: checking inline artillery candidate filter',flush=True)
    filter_comparisons=compare_artillery_filter(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: checking artillery weapon selection',flush=True)
    weapon_comparisons=compare_artillery_weapons(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(weapon_comparisons)} artillery weapon comparisons passed',flush=True)
    print(f'{target}: checking shared scalar/vector math',flush=True)
    math_comparisons=compare_scalar_vector_math(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(math_comparisons)} shared scalar/vector math comparisons passed',flush=True)
    print(f'{target}: checking aiming limits',flush=True)
    aim_comparisons=compare_aim_limits(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: checking artillery aiming geometry and candidate gate',flush=True)
    aim_geometry_comparisons=compare_aim_geometry(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(aim_geometry_comparisons)} aiming geometry/gate comparisons passed',flush=True)
    print(f'{target}: {len(aim_comparisons)} aiming-limit comparisons passed',flush=True)
    print(f'{target}: checking affine matrix composition',flush=True)
    matrix_comparisons=compare_affine_matrix(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(matrix_comparisons)} affine matrix comparisons passed',flush=True)
    print(f'{target}: checking constant-return entries',flush=True)
    constant_comparisons=compare_constant_returns(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(constant_comparisons)} constant-return comparisons passed',flush=True)
    print(f'{target}: checking object-word getter entries',flush=True)
    getter_comparisons=compare_word_getters(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(getter_comparisons)} object-word getter comparisons passed',flush=True)
    notify_count=sum(bool(x['inputs'].get('notify_only',False)) for x in collision_comparisons)
    print(f'{target}: checking artillery movement gates',flush=True)
    movement_comparisons=compare_artillery_movement(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(movement_comparisons)} artillery movement comparisons passed',flush=True)
    print(f'{target}: checking movement score and helpers',flush=True)
    movement_score_comparisons=compare_movement_score(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(movement_score_comparisons)} movement score/helper comparisons passed',flush=True)
    print(f'{target}: checking artillery query gate and helpers',flush=True)
    query_comparisons=compare_artillery_query(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(query_comparisons)} artillery query gate/helper comparisons passed',flush=True)
    print(f'{target}: checking region geometry and artillery region score',flush=True)
    region_comparisons=compare_region_geometry(original,original_pe,edited,edited_pe,spec,symbols)
    region_score_comparisons=compare_artillery_region_score(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(region_comparisons)} region geometry and {len(region_score_comparisons)} region score comparisons passed',flush=True)
    print(f'{target}: checking category score and node search',flush=True)
    category_comparisons=compare_category_score(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(category_comparisons)} category score/search comparisons passed',flush=True)
    print(f'{target}: checking alternate target eligibility and event-3 predicate',flush=True)
    eligibility_comparisons=compare_target_eligibility(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(eligibility_comparisons)} target eligibility comparisons passed',flush=True)
    print(f'{target}: checking alternate gate and linked-object score',flush=True)
    alternate_comparisons=compare_alternate_gate(original,original_pe,edited,edited_pe,spec,symbols)
    nested_comparisons=compare_nested_score(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(alternate_comparisons)} alternate gate and {len(nested_comparisons)} nested score comparisons passed',flush=True)
    print(f'{target}: checking alternate score finish and target traversal',flush=True)
    finish_comparisons=compare_alternate_finish(original,original_pe,edited,edited_pe,spec,symbols)
    traversal_comparisons=compare_target_traversal(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(finish_comparisons)} alternate finish and {len(traversal_comparisons)} traversal comparisons passed',flush=True)
    print(f'{target}: checking first-pass advance and second-pass query setup',flush=True)
    next_comparisons=compare_next_candidate(original,original_pe,edited,edited_pe,spec,symbols)
    setup_comparisons=compare_second_setup(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(next_comparisons)} node advance and {len(setup_comparisons)} second setup comparisons passed',flush=True)
    print(f'{target}: checking second-pass candidate filter',flush=True)
    second_filter_comparisons=compare_second_filter(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(second_filter_comparisons)} second filter comparisons passed',flush=True)
    print(f'{target}: checking second-pass weight, normalization and aim gate',flush=True)
    weight_comparisons=compare_second_weight(original,original_pe,edited,edited_pe,spec,symbols)
    normalize_comparisons=compare_vector_normalize(original,original_pe,edited,edited_pe,spec,symbols)
    second_aim_comparisons=compare_second_aim(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(weight_comparisons)} weight, {len(normalize_comparisons)} normalization and {len(second_aim_comparisons)} second aim comparisons passed',flush=True)
    print(f'{target}: checking second-pass position, distance and weapon scan',flush=True)
    second_position_comparisons=compare_second_position(original,original_pe,edited,edited_pe,spec,symbols)
    second_distance_comparisons=compare_second_distance(original,original_pe,edited,edited_pe,spec,symbols)
    second_weapons_comparisons=compare_second_weapons(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(second_position_comparisons)} position, {len(second_distance_comparisons)} distance and {len(second_weapons_comparisons)} weapon comparisons passed',flush=True)
    print(f'{target}: checking second-pass movement gate and score',flush=True)
    second_movement_comparisons=compare_second_movement(original,original_pe,edited,edited_pe,spec,symbols)
    second_movement_score_comparisons=compare_second_movement_score(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(second_movement_comparisons)} movement gate and {len(second_movement_score_comparisons)} movement score comparisons passed',flush=True)
    print(f'{target}: checking second-pass driver predicate',flush=True)
    second_driver_comparisons=compare_second_driver(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(second_driver_comparisons)} driver predicate comparisons passed',flush=True)
    print(f'{target}: checking second-pass region modifier',flush=True)
    second_region_comparisons=compare_second_region(original,original_pe,edited,edited_pe,spec,symbols)
    print(f'{target}: {len(second_region_comparisons)} second-pass region comparisons passed',flush=True)
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
    report['aim_geometry_cases']=len(aim_geometry_comparisons)
    report['aim_geometry']=aim_geometry_comparisons
    report['passed']+=len(aim_geometry_comparisons)
    report['scope']+=' Artillery position transforms, vector differences, world/component position helpers and the post-weapon aiming gate compared through original continuations. Math, aliasing, callback order, dynamic pointers, x87 state and gate branches execute; object methods and selected aim-direction services remain controlled dependencies. Complete trajectory and firing behavior remain unverified.'
    report['matrix_cases']=len(matrix_comparisons)
    report['matrix']=matrix_comparisons
    report['passed']+=len(matrix_comparisons)
    report['constant_return_cases']=len(constant_comparisons)
    report['constant_returns']=constant_comparisons
    report['passed']+=len(constant_comparisons)
    report['word_getter_cases']=len(getter_comparisons)
    report['word_getters']=getter_comparisons
    report['passed']+=len(getter_comparisons)
    report['history_tree_cases']=len(tree_comparisons)
    report['history_tree']=tree_comparisons
    report['passed']+=len(tree_comparisons)
    report['artillery_weapon_cases']=len(weapon_comparisons)
    report['artillery_weapons']=weapon_comparisons
    report['passed']+=len(weapon_comparisons)
    report['artillery_movement_cases']=len(movement_comparisons)
    report['artillery_movement']=movement_comparisons
    report['passed']+=len(movement_comparisons)
    report['movement_score_cases']=len(movement_score_comparisons)
    report['movement_score']=movement_score_comparisons
    report['passed']+=len(movement_score_comparisons)
    report['artillery_query_cases']=len(query_comparisons)
    report['artillery_query']=query_comparisons
    report['passed']+=len(query_comparisons)
    report['region_geometry_cases']=len(region_comparisons)
    report['region_geometry']=region_comparisons
    report['artillery_region_score_cases']=len(region_score_comparisons)
    report['artillery_region_score']=region_score_comparisons
    report['passed']+=len(region_comparisons)+len(region_score_comparisons)
    report['category_score_cases']=len(category_comparisons)
    report['category_score']=category_comparisons
    report['passed']+=len(category_comparisons)
    report['target_eligibility_cases']=len(eligibility_comparisons)
    report['target_eligibility']=eligibility_comparisons
    report['passed']+=len(eligibility_comparisons)
    report['alternate_gate_cases']=len(alternate_comparisons)
    report['alternate_gate']=alternate_comparisons
    report['nested_score_cases']=len(nested_comparisons)
    report['nested_score']=nested_comparisons
    report['passed']+=len(alternate_comparisons)+len(nested_comparisons)
    report['alternate_finish_cases']=len(finish_comparisons)
    report['alternate_finish']=finish_comparisons
    report['target_traversal_cases']=len(traversal_comparisons)
    report['target_traversal']=traversal_comparisons
    report['passed']+=len(finish_comparisons)+len(traversal_comparisons)
    report['next_candidate_cases']=len(next_comparisons)
    report['next_candidate']=next_comparisons
    report['second_setup_cases']=len(setup_comparisons)
    report['second_setup']=setup_comparisons
    report['second_filter_cases']=len(second_filter_comparisons)
    report['second_filter']=second_filter_comparisons
    report['second_weight_cases']=len(weight_comparisons)
    report['second_weight']=weight_comparisons
    report['vector_normalize_cases']=len(normalize_comparisons)
    report['vector_normalize']=normalize_comparisons
    report['second_aim_cases']=len(second_aim_comparisons)
    report['second_aim']=second_aim_comparisons
    report['second_position_cases']=len(second_position_comparisons)
    report['second_position']=second_position_comparisons
    report['second_distance_cases']=len(second_distance_comparisons)
    report['second_distance']=second_distance_comparisons
    report['second_weapons_cases']=len(second_weapons_comparisons)
    report['second_weapons']=second_weapons_comparisons
    report['second_movement_cases']=len(second_movement_comparisons)
    report['second_movement']=second_movement_comparisons
    report['second_movement_score_cases']=len(second_movement_score_comparisons)
    report['second_movement_score']=second_movement_score_comparisons
    report['second_driver_cases']=len(second_driver_comparisons)
    report['second_driver']=second_driver_comparisons
    report['second_region_cases']=len(second_region_comparisons)
    report['second_region']=second_region_comparisons
    report['passed']+=len(second_region_comparisons)
    report['scope']+=' Second-pass region modifier compares score refresh, captured EBX/driver, actual region containment helper, aliases, nonfinite values and x87 state. The surrounding iterator/category and complete target evaluation remain native or unverified.'
    report['passed']+=len(second_driver_comparisons)
    report['scope']+=' Second-pass driver predicate compares actual event-word helper, captured target/driver/receiver/table, two position calls, x87 point conversion, callback mutations, EDI/EBP/EBX, memory and accept/reject routes. Virtual methods are controlled. Subsequent region/category stages and complete target selection/firing remain unverified.'
    report['passed']+=len(second_movement_comparisons)+len(second_movement_score_comparisons)
    report['scope']+=' Second-pass movement gate and distinct movement score execute actual source scalar, cross-product and length math with controlled vector/event methods. Full fixture memory, callback captures, EDI/EBX, three routes, aliases, exceptional values and x87 states compare; eight cases execute the source gate and score together. Unit-score and driver-reload entries remain native. Later evaluation, firing and complete live-game behavior remain unverified.'
    report['passed']+=len(second_position_comparisons)+len(second_distance_comparisons)+len(second_weapons_comparisons)
    report['scope']+=' Second-pass position retrieval executes the reconstructed event-interface helper with controlled virtual methods, preserving sequential fallback copies and captured EDI. Distance arithmetic executes actual length, maximum and division helpers with native rounded/intervening stores, aliases, exceptional values and occupied x87 state. Second-pass weapon scan compares actual signed rating arithmetic, dynamic vector reads, callback/table captures, strict best selection, scan count, accepted padding, frame memory and x87 state with controlled inventory/category/availability. Later movement, complete target evaluation and firing remain native or unverified.'
    report['passed']+=len(weight_comparisons)+len(normalize_comparisons)+len(second_aim_comparisons)
    report['scope']+=' Second-pass timestamp weight executes actual clamp arithmetic with controlled timestamp lookup. Complete normalization compares rounded squared length, tolerance gates, reciprocal square root, EAX and ECX outputs, aliasing, exceptional inputs and occupied x87 states. The second-pass aiming gate executes actual position, difference and normalization helpers, comparing callback rereads, captured component, full fixture memory and continuations with controlled object methods and final aiming predicate. Later movement, target scores, cleanup and firing remain unverified.'
    report['passed']+=len(second_filter_comparisons)
    report['scope']+=' Second-pass candidate filter executes pool/generation checks, frame rereads after callbacks, flag gates and actual x87 metric/history comparisons across precision/rounding modes and occupied stacks; identity, metric and history services remain controlled. Later scoring and vector cleanup remain native.'
    report['passed']+=len(next_comparisons)+len(setup_comparisons)
    report['scope']+=' First-pass node advance and second-pass query routing/initialization compare original/source instructions with controlled object methods. Full fixture memory, callback order, captured table/node, frame aliases, low-byte flags, raw query arguments, native continuations, live EAX allocation pointer, preserved nonvolatile registers and x87 state compare across pointer patterns and precision/rounding modes. Query allocation/unwinding, cleanup, later second-pass scoring and complete evaluator behavior remain unverified.'
    report['scope']+=' Final alternate score scaling and rounded best-target selection execute actual float selectors and the original-equivalent single setting field read. The complete traversal wrapper compares returned handles, stack cleanup, full fixture memory, callback captures, scratch/argument aliasing, nonvolatile registers and x87 state. No numeric service is mocked; virtual object methods remain controlled. Remaining evaluator phases and live-match behavior are unverified.'
    report['scope']+=' Alternate flag gate and linked-object score execute real original/source eligibility, component predicate, interface lookup, list search, traversal wrapper and x87 arithmetic. Pool lookup remains native and executes without mocks; virtual methods and component conversion are controlled. Full frame/arena memory, callback order, argument captures, descriptor aliasing, raw fields, loop exit, live nonvolatile registers and x87 state compare across linked targets, generation misses and precision/rounding modes. Later evaluator phases and live-match behavior remain unverified.'
    report['scope']+=' Alternate first-pass target eligibility and its event-3 predicate execute actual original/source handle lookup, generation checks, callback rereads, nested traversal and component conversion. Full fixture memory, low-byte returns, callback order, scratch aliasing, preserved nonvolatile registers and x87 state are compared across invalid handles, nested misses and precision/rounding modes. Object methods and component conversion remain controlled; later target-state scoring, second pass and firing remain unverified.'
    report['scope']+=' First-pass category score and node search execute actual original/source interface lookup, list traversal and scalar selectors. Full frame/arena memory, callback rereads, aliasing, low-byte predicates, unsigned list bounds, retained extended score comparison, best-target stores, live registers and x87 state compared across nonfinite values and precision/rounding modes. Virtual object methods are controlled; alternate target flags, remaining evaluator phases and live-match behavior remain unverified.'
    report['scope']+=' Region/projection helpers and following first-pass score modifier execute actual original/source math without numeric mocks. Complete memory, partial projection overlap, raw and rounded stores, unordered/inclusive/strict comparisons, ABI and x87 state compared across nonfinite inputs and precision/rounding modes. Full region ownership/lifecycle, unmasked exceptions and live-match behavior remain unverified.'
    report['scope']+=' First-pass distance/driver/query gate and four helpers compare complete fixture memory, ABI, callback order, captured tables versus receiver rereads, buffer overlap, low-byte predicates and x87 state. Actual point transforms execute; object methods, protected vector growth and raw free are controlled. Both query outcomes destroy the ignored-handle vector exactly once. Full evaluator unwinding, live queries and complete artillery behavior remain unverified.'
    report['scope']+=' Following first-pass movement scoring, in-place cross product and component event-2 scalar wrapper compared through guarded entries, including callback-dependent driver rereads, vector overlap, rounded projection stores and native negative/unordered gates, actual vector-length integration, live registers and x87 state. Eight cases execute the previous gate and score together. Object methods remain controlled, the earlier gate retains its native score=1 alternate entry, and complete candidate scoring/firing remains unverified.'
    report['scope']+=' First-pass movement-vector, flag and distance gates compared at all three native continuations, including full frame/arena memory, callback order, live movement receiver, vector aliasing, low-byte predicates, unordered comparisons, x87 status/control and retained values. Movement-vector and owner-predicate methods are controlled; later velocity arithmetic and scoring remain native.'
    report['scope']+=' First-pass artillery weapon scoring and selection compared at native continuations, including exact frame/arena memory, preserved live registers, x87 control/status and retained values. Inventory/category/availability callbacks are controlled; arithmetic, dynamic iteration, distance rejection, strict best-score selection and accepted-candidate padding execute. Other candidate phases and complete artillery behavior remain native or unverified.'
    report['scope']+=' Target-history insertion, duplicate handling, predecessor traversal, node construction and red-black rotations/balancing compared through guarded entries, including timestamp-helper integration. Normal insertion executes the retained protected node allocator with a controlled raw heap; capacity-error tests control native string/exception services and stop at the throw boundary. These checks do not establish real capacity-error unwinding, allocation failure, deletion, concurrent access or full artillery behavior.'
    report['scope']+=' Aiming event routing and angular-range predicate compared with controlled transform/trig services, callback mutations and x87 status; selected cases execute actual original/source matrix composition and retained inverse-sine runtime. Complete aiming/firing behavior remains unverified.'
    report['scope']+=' Affine matrix composition compared without math mocks, including full backing memory, partial/full buffer overlap, ABI, x87 status/control and retained caller values across all supported precision/rounding modes.'
    report['scope']+=' Audited complete constant-return bodies execute actual original/source entries, comparing EAX bits, stack cleanup, all non-result integer registers, CPU flags, complete x87 state and no writes; original-image return values retain data dependencies.'
    report['scope']+=' Audited complete ECX word getters compare exact field loads, unaligned objects, returned EAX bits, stack, all preserved integer registers, CPU flags, full x87 state and exactly one field read with no writes; object ownership and semantic field types remain unresolved.'
    (work/'verification.json').write_text(json.dumps(report,indent=2))
    print(f'{target}: {len(cases)} interpreter, 32 cached-bailout, {len(recompute_cases)} recomputed-bailout, {len(curve_comparisons)} curve, {len(geometry_comparisons)} geometry, {len(collision_comparisons)-notify_count} collision, {notify_count} notification and {len(dispatch_comparisons)} dispatcher and {len(event_comparisons)} event and {len(vector_comparisons)} vector comparisons, {len(artillery_comparisons)} artillery driver comparisons, {len(artillery_cache_comparisons)} artillery evaluator/integration comparisons, {len(history_comparisons)} target-history comparisons, {len(filter_comparisons)} inline candidate-filter comparisons, 32 vehicle-wrapper ABI checks passed',flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--game-dir',type=Path,default=GAME)
    parser.add_argument("--target",choices=("client","server","both"),default="both")
    args=parser.parse_args()
    GAME=args.game_dir.resolve()
    for target in ('client','server') if args.target=='both' else (args.target,): verify(target)
