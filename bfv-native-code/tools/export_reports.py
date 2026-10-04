"""Export compact public evidence only after both current builds pass verification.

Run after verify.py and verify_live_exceptions.py. Full local cases stay in build/;
public reports contain counts, scope, addresses and hashes, never game binaries.
"""
import hashlib
import csv
import json
from collections import Counter
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
REPORTS = PROJECT.parent / 'reports'


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def prepare(target):
    work = PROJECT / 'build' / target
    manifest = read(work / 'manifest.json')
    full = read(work / 'verification.json')
    live = read(work / 'live-exceptions' / 'verification.json')
    digest = manifest['output_sha256']
    assert hashlib.sha256(Path(manifest['output']).read_bytes()).hexdigest() == digest
    for report in (full, live):
        assert report['target'] == target
        assert report['original_sha256'] == manifest['input_sha256']
        assert report['compiled_sha256'] == digest, 'Stale verification: rerun on the current build'
    assert full['passed'] == sum(len(v) for v in full.values() if isinstance(v, list))
    assert full['passed'] == sum(v for k, v in full.items() if k.endswith('_cases'))
    assert live['passed'] == len(live['cases'])

    # Structural evidence is tied to the same originals and guarded bytes as
    # the build. It must exist before publishing the new inline stages/helpers.
    patches = {p['source']: p for p in manifest['entry_patches']}
    with (REPORTS / target / 'aim-geometry-audit.tsv').open(encoding='utf-8') as f:
        audit = list(csv.DictReader(f, delimiter='\t'))
    expected = {'bfv_transform_point': 90, 'bfv_vector_difference': 37,
                'bfv_aim_world_position': 113, 'bfv_component_position': 31,
                'bfv_artillery_aim_gate_bridge': 208}
    assert len(audit) == len(expected) and {r['symbol'] for r in audit} == set(expected)
    for row in audit:
        patch = patches[row['symbol']]
        assert row['original_sha256'] == manifest['input_sha256'] and row['status'] == 'eligible'
        assert row['address'] == patch['entry'] and row['patch_hex'] == patch['original']
        assert int(row['patch_bytes']) * 2 == len(patch['original'])
        assert int(row['bytes']) == expected[row['symbol']]
        assert int(row['external_interior_references']) == int(row['overwritten_interior_references']) == 0
    with (REPORTS / target / 'artillery-movement-audit.tsv').open(encoding='utf-8') as f:
        movement_audit = list(csv.DictReader(f, delimiter='\t'))
    assert len(movement_audit) == 1
    row = movement_audit[0]; patch = patches['bfv_artillery_movement_gate_bridge']
    assert row['original_sha256'] == manifest['input_sha256'] and row['status'] == 'eligible'
    assert row['start'] == patch['entry'] and row['patch_hex'] == patch['original']
    assert int(row['patch_bytes']) * 2 == len(patch['original']) and int(row['bytes']) == 208
    assert int(row['external_interior_references']) == int(row['overwritten_interior_references']) == 0

    with (REPORTS / target / 'movement-score-audit.tsv').open(encoding='utf-8') as f:
        score_audit = list(csv.DictReader(f, delimiter='\t'))
    expected_score = {'bfv_vector_cross_assign': 86, 'bfv_component_event2_scalar': 23,
                      'bfv_artillery_movement_score_bridge': 570}
    assert len(score_audit) == len(expected_score) and {r['symbol'] for r in score_audit} == set(expected_score)
    for row in score_audit:
        patch = patches[row['symbol']]
        assert row['original_sha256'] == manifest['input_sha256'] and row['status'] == 'eligible'
        assert row['address'] == patch['entry'] and row['patch_hex'] == patch['original']
        assert int(row['patch_bytes']) * 2 == len(patch['original']) and int(row['bytes']) == expected_score[row['symbol']]
        assert int(row['external_interior_references']) == int(row['overwritten_interior_references']) == 0
        if row['symbol'] == 'bfv_artillery_movement_score_bridge':
            assert int(row['retained_unit_references']) == 1
            assert int(row['end_exclusive'], 16) - int(row['retained_unit_entry'], 16) == 8

    with (REPORTS / target / 'artillery-query-audit.tsv').open(encoding='utf-8') as f:
        query_audit = list(csv.DictReader(f, delimiter='\t'))
    expected_query = {'bfv_component_event2_word': 23, 'bfv_component_query_data': 38,
                      'bfv_query_vector_append': 77, 'bfv_query_vector_destroy': 42,
                      'bfv_artillery_query_gate_bridge': 502}
    assert len(query_audit) == len(expected_query) and {r['symbol'] for r in query_audit} == set(expected_query)
    for row in query_audit:
        patch = patches[row['symbol']]
        assert row['original_sha256'] == manifest['input_sha256'] and row['status'] == 'eligible'
        assert row['address'] == patch['entry'] and row['patch_hex'] == patch['original']
        assert int(row['patch_bytes']) * 2 == len(patch['original']) and int(row['bytes']) == expected_query[row['symbol']]
        assert int(row['external_interior_references']) == int(row['overwritten_interior_references']) == 0
        assert int(row['repaired_body_bytes']) == (3 if row['symbol'] == 'bfv_query_vector_destroy' else 0)
        assert int(row['indirect_tail_exits']) == (1 if row['symbol'] == 'bfv_component_query_data' else 0)

    with (REPORTS / target / 'region-geometry-audit.tsv').open(encoding='utf-8') as f:
        region_audit = list(csv.DictReader(f, delimiter='\t'))
    expected_region = {'bfv_point_xz': 13, 'bfv_point_in_symmetric_bounds': 89,
                       'bfv_region_contains_point2': 144, 'bfv_region_contains_point3': 34}
    assert len(region_audit) == len(expected_region) and {r['symbol'] for r in region_audit} == set(expected_region)
    for row in region_audit:
        patch = patches[row['symbol']]
        assert row['original_sha256'] == manifest['input_sha256'] and row['status'] == 'eligible'
        assert row['address'] == patch['entry'] and row['patch_hex'] == patch['original']
        assert int(row['patch_bytes']) * 2 == len(patch['original']) and int(row['bytes']) == expected_region[row['symbol']]
        assert int(row['external_interior_references']) == int(row['overwritten_interior_references']) == 0
        assert int(row['ret_cleanup']) == (0 if row['symbol'] == 'bfv_point_xz' else 4)
    with (REPORTS / target / 'artillery-region-score-audit.tsv').open(encoding='utf-8') as f:
        region_score_audit = list(csv.DictReader(f, delimiter='\t'))
    assert len(region_score_audit) == 1
    row = region_score_audit[0]; patch = patches['bfv_artillery_region_score_bridge']
    assert row['original_sha256'] == manifest['input_sha256'] and row['status'] == 'eligible'
    assert row['address'] == patch['entry'] and row['patch_hex'] == patch['original']
    assert int(row['patch_bytes']) * 2 == len(patch['original']) and int(row['bytes']) == 73
    assert int(row['external_interior_references']) == int(row['overwritten_interior_references']) == 0

    with (REPORTS / target / 'category-score-audit.tsv').open(encoding='utf-8') as f:
        category_audit = list(csv.DictReader(f, delimiter='\t'))
    expected_category = {'bfv_find_word_in_nodes': 40, 'bfv_artillery_category_score_bridge': 383}
    assert len(category_audit) == len(expected_category) and {r['symbol'] for r in category_audit} == set(expected_category)
    for row in category_audit:
        patch = patches[row['symbol']]
        assert row['original_sha256'] == manifest['input_sha256'] and row['status'] == 'eligible'
        assert row['address'] == patch['entry'] and row['patch_hex'] == patch['original']
        assert int(row['patch_bytes']) * 2 == len(patch['original']) and int(row['bytes']) == expected_category[row['symbol']]
        assert int(row['external_interior_references']) == int(row['overwritten_interior_references']) == 0
        assert int(row['ret_cleanup']) == (8 if row['symbol'] == 'bfv_find_word_in_nodes' else 0)

    with (REPORTS / target / 'target-eligibility-audit.tsv').open(encoding='utf-8') as f:
        eligibility_audit = list(csv.DictReader(f, delimiter='\t'))
    expected_eligibility = {'bfv_target_handle_eligible': 399}
    assert len(eligibility_audit) == len(expected_eligibility) and {r['symbol'] for r in eligibility_audit} == set(expected_eligibility)
    for row in eligibility_audit:
        patch = patches[row['symbol']]
        assert row['original_sha256'] == manifest['input_sha256'] and row['status'] == 'eligible'
        assert row['address'] == patch['entry'] and row['patch_hex'] == patch['original']
        assert int(row['patch_bytes']) * 2 == len(patch['original']) and int(row['bytes']) == expected_eligibility[row['symbol']]
        assert int(row['external_interior_references']) == int(row['overwritten_interior_references']) == 0
        assert int(row['indirect_tail_jump']) == 0


    for filename,symbol,size in [('artillery-alternate-gate-audit.tsv','bfv_artillery_alternate_gate_bridge',107),
                                 ('artillery-nested-score-audit.tsv','bfv_artillery_nested_score_bridge',360),
                                 ('artillery-alternate-finish-audit.tsv','bfv_artillery_alternate_finish_bridge',170),
                                 ('target-traversal-audit.tsv','bfv_next_target_handle',41),
                                 ('artillery-next-candidate-audit.tsv','bfv_artillery_next_candidate_bridge',29),
                                 ('artillery-second-setup-audit.tsv','bfv_artillery_second_setup_bridge',93)]:
        with (REPORTS / target / filename).open(encoding='utf-8') as f:
            rows=list(csv.DictReader(f,delimiter='\t'))
        assert len(rows)==1
        row=rows[0];patch=patches[symbol]
        assert row['symbol']==symbol and row['original_sha256']==manifest['input_sha256'] and row['status']=='eligible'
        assert row['address']==patch['entry'] and row['patch_hex']==patch['original']
        assert int(row['patch_bytes'])*2==len(patch['original']) and int(row['bytes'])==size
        assert int(row['external_interior_references'])==int(row['overwritten_interior_references'])==0
        if symbol=='bfv_next_target_handle':assert int(row['ret_cleanup'])==8

    identity = {key: full[key] for key in ('target', 'original_sha256', 'compiled_sha256')}
    summary = {key: value for key, value in full.items() if not isinstance(value, list)}
    summary['constant_return_entries'] = len({c['address'] for c in full['constant_returns']})
    summary['word_getter_entries'] = len({c['address'] for c in full['word_getters']})
    summary['history_tree_entries'] = 6
    public_manifest = dict(manifest, output=Path(manifest['output']).name)
    outputs = {'verification-summary.json': summary,
               'native-build-manifest.json': public_manifest,
               'live-exceptions.json': live}

    def focused(filename, group, scope, **details):
        outputs[filename] = dict(identity, passed=len(full[group]), scope=scope, **details)

    focused('next-candidate-verification.json','next_candidate',
            'Original/source first-pass node advance, captured node/table, callback-dependent '
            'list boundary and frame aliasing. Complete fixture memory, continuations, live '
            'EDI, nonvolatile registers and x87 state compare with controlled list methods. '
            'Complete evaluator and live-match behavior remain unverified.',
            original_block_bytes=29,replacement_scope='partial_native_evaluator')
    focused('second-setup-verification.json','second_setup',
            'Original/source second-pass pattern routing and query-vector initialization. '
            'Full fixture memory, low-byte flags, callback order, raw query arguments, pointer '
            'patterns, live EAX allocation pointer, nonvolatile registers and x87 state compare '
            'with controlled object methods. Actual query allocation/unwinding, buffer cleanup, '
            'later filtering and full evaluator behavior remain unverified.',
            original_block_bytes=93,replacement_scope='partial_native_evaluator')

    focused('alternate-gate-verification.json','alternate_gate',
            'Original/source alternate flag gate, actual eligibility and interface lookup, '
            'callback-dependent record/receiver rereads, field stores, ABI and x87 state. '
            'Object methods and component conversion remain controlled; complete evaluator '
            'and live-match behavior remain unverified.',original_block_bytes=107,
            replacement_scope='partial_native_evaluator')
    focused('alternate-finish-verification.json','alternate_finish',
            'Original/source final alternate score scaling, signed setting comparison and '
            'rounded best-target selection. Actual float selectors execute without numeric '
            'mocks; complete fixture memory, field aliases, nonvolatile registers and x87 '
            'state compare across nonfinite inputs, precision/rounding modes and exact ties. '
            'Remaining evaluator phases and live-match behavior are unverified.',
            original_block_bytes=170,replacement_scope='partial_native_evaluator')
    focused('target-traversal-verification.json','target_traversal',
            'Original/source complete traversal wrapper; object methods controlled. Returned '
            'handle, full fixture memory, stack cleanup, callback captures, scratch/argument '
            'aliasing, nonvolatile registers and x87 state compare across null results and '
            'precision/rounding modes. Full object ownership and live traversal are unverified.',
            reconstructed_entries=1,original_function_bytes=41)
    focused('nested-score-verification.json','nested_score',
            'Original/source linked-object score accumulation, actual list search, source '
            'traversal wrapper and retained pool lookup. Full fixture memory, callback order, argument '
            'captures, descriptor aliasing, loop exit, nonvolatile registers and x87 state '
            'compare across exceptional inputs and precision/rounding modes. Virtual methods '
            'remain controlled; final scaling, remaining evaluator and live matches are unverified.',
            original_block_bytes=360,replacement_scope='partial_native_evaluator')

    focused('artillery-weapons-verification.json', 'artillery_weapons',
            'Original/source first-pass weapon scoring and selection at native continuations. '
            'Exact frame/arena memory, callback order/arguments, live registers, x87 control/status '
            'and retained values compared across boundary/nonfinite inputs, dynamic containers and '
            'all supported precision/rounding modes. Inventory/category/availability methods remain '
            'controlled services. The full artillery evaluator and live-match behavior remain unverified.',
            accepted_cases=sum(c['accepted'] for c in full['artillery_weapons']),
            rejected_cases=sum(not c['accepted'] for c in full['artillery_weapons']),
            original_block_bytes=394, replacement_scope='partial_native_evaluator')

    focused('history-tree-verification.json', 'history_tree',
            'Actual original/source insertion, duplicate handling, predecessor traversal, rotations, '
            'node construction and timestamp integration through guarded entries. Complete arena memory, '
            'allocation requests, ABI, x87 state and red-black invariants checked. Normal paths execute '
            'the retained protected node allocator with a controlled raw heap. Capacity-error tests '
            'control native string/exception services and stop at the throw boundary; real unwinding, '
            'allocation failure, deletion, concurrent access and complete artillery behavior remain unverified.',
            reconstructed_entries=6,
            case_counts=dict(Counter(c['kind'] for c in full['history_tree'])),
            payload_bytes=manifest['payload_bytes'],
            guarded_entry_replacements=len(manifest['entry_patches']),
            guarded_vtable_replacements=len(manifest['patches']))
    focused('artillery-movement-verification.json', 'artillery_movement',
            'Original/source first-pass movement-vector retrieval and flag/distance gates at '
            'all three native continuations. Full frame/arena memory, callback order, live EDI '
            'receiver, aliasing, low-byte predicates and x87 status/control/retained values '
            'compared across boundaries, nonfinite inputs, callback mutations and precision/rounding '
            'modes. Object methods are controlled services; later velocity arithmetic, complete '
            'candidate scoring and live-match behavior remain unverified.',
            exit_counts=dict(Counter(c['exit'] for c in full['artillery_movement'])),
            original_block_bytes=208, replacement_scope='partial_native_evaluator')
    focused('movement-score-verification.json', 'movement_score',
            'Original/source following movement score and complete cross-product/event-2 scalar '
            'helpers. Full fixture memory, ABI, callback ordering/pointer rereads, vector overlap, '
            'rounded projection stores, ordered-negative/unordered branches and full x87 state '
            'compared across boundary/nonfinite inputs and precision/rounding modes. Vector-length '
            'math executes the actual recovered helper; object methods are controlled. Eight '
            'cases execute the previous gate and score together. The earlier gate retains its '
            'native score=1 alternate entry. Complete target scoring, firing and live-match '
            'equivalence remain unverified.',
            group_counts=dict(Counter(c['group'] for c in full['movement_score'])),
            helper_counts=dict(Counter(c['inputs']['kind'] for c in full['movement_score'] if c['group']=='helper')),
            integrated_cases=sum(bool(c['inputs'].get('integrated')) for c in full['movement_score']),
            reconstructed_entries=2, original_scoring_block_bytes=570,
            replacement_scope='partial_native_evaluator_with_two_complete_helpers')
    focused('artillery-query-verification.json', 'artillery_query',
            'Original/source first-pass distance/driver/query gate and four complete helpers. '
            'Complete frame/arena memory, ABI, callback sequence, captured tables versus receiver '
            'rereads, overlap, raw versus rounded coordinate copies, low-byte predicates and x87 '
            'control/status/retained values compared across boundary/nonfinite inputs and precision/'
            'rounding modes. Actual point transforms execute. Object methods, protected vector '
            'growth and raw free are controlled; both query outcomes perform one vector cleanup. '
            'Full evaluator exception unwinding, live query behavior and complete artillery '
            'equivalence remain unverified.',
            group_counts=dict(Counter(c['group'] for c in full['artillery_query'])),
            helper_counts=dict(Counter(c['inputs']['kind'] for c in full['artillery_query'] if c['group']=='helper')),
            exit_counts=dict(Counter(c['exit'] for c in full['artillery_query'] if c['group']=='gate')),
            reconstructed_entries=4, original_gate_bytes=502,
            replacement_scope='partial_native_evaluator_with_four_complete_helpers')
    focused('region-geometry-verification.json', 'region_geometry',
            'Actual original/source x/z projection, symmetric bounds and 2D/3D region predicates '
            'without numeric mocks. Complete memory, raw projection overlap, extended x versus '
            'rounded y reflection, inclusive/unordered bounds, strict radial limit, ABI and x87 '
            'state compared across finite/nonfinite inputs and precision/rounding modes. Boolean '
            'returns compare AL as required by the native callers. Region ownership/lifecycle, '
            'unmasked exceptions and live-match behavior remain unverified.',
            operation_counts=dict(Counter(c['inputs']['kind'] for c in full['region_geometry'])),
            reconstructed_entries=4)
    focused('artillery-region-score-verification.json', 'artillery_region_score',
            'Actual original/source following first-pass region score modifier and region math '
            'without numeric mocks. Complete frame/arena memory, raw score copies, rounded 0.75 '
            'multiplication, low-byte predicate, guarded region retrieval, aliases, preserved live '
            'registers and x87 state compared across nonfinite values and precision/rounding modes. '
            'Later flag/category scoring and complete artillery/live-match behavior remain unverified.',
            original_block_bytes=73, replacement_scope='partial_native_evaluator')
    focused('category-score-verification.json', 'category_score',
            'Actual original/source first-pass category-list factor, candidate score and iterator '
            'search through guarded entries. Interface lookup, list search and scalar selectors execute; '
            'virtual methods are controlled. Complete frame/arena memory, callback mutation/rereads, '
            'unsigned bounds, aliased output, retained extended score versus rounded best, strict '
            'selection, live registers and x87 state compared across nonfinite inputs and all supported '
            'precision/rounding modes. Alternate target flags, remaining phases and live-match '
            'behavior remain unverified.',
            operation_counts=dict(Counter(c['group'] for c in full['category_score'])),
            reconstructed_entries=1, original_block_bytes=383,
            replacement_scope='partial_native_evaluator_with_one_complete_helper')
    focused('target-eligibility-verification.json', 'target_eligibility',
            'Actual original/source alternate first-pass target eligibility and event-3 predicate. '
            'Generation-handle lookup, callback rereads, nested traversal and component conversion '
            'execute; object methods and conversion remain controlled. Full fixture memory, low-byte '
            'returns, scratch aliasing, live registers and x87 state compare across invalid handles, '
            'nested misses and supported precision/rounding modes. Later target-state scoring, '
            'second pass, firing and live-match behavior remain unverified.',
            reconstructed_entries=1)
    focused('constant-return-verification.json', 'constant_returns',
            'Actual original/source constant-return entries compare exact EAX bits, stack cleanup, '
            'preserved integer registers, CPU flags, complete x87 state and no writes. '
            'Original-image return addresses retain data dependencies.',
            reconstructed_entries=summary['constant_return_entries'], scenarios_per_entry=6)
    focused('word-getter-verification.json', 'word_getters',
            'Original/source getter comparisons cover eight word patterns, six flag/x87 scenarios, '
            'four receiver alignments, EAX, stack, preserved integer registers, CPU flags, complete '
            'x87 state and exactly one field read without writes or delegated execution. The verifier '
            'checks that all compiled getter bodies are byte-identical to the original MOV/RET bodies. '
            'The full regression suite passed for this same executable hash.',
            reconstructed_entries=summary['word_getter_entries'], scenarios_per_entry=48,
            compiled_bodies_byte_identical=True, payload_bytes=manifest['payload_bytes'],
            guarded_entry_replacements=len(manifest['entry_patches']),
            guarded_vtable_replacements=len(manifest['patches']),
            full_regression_cases=full['passed'], full_regression_passed=True)
    focused('shared-math-verification.json', 'shared_math',
            'Actual original/source scalar/vector math entries without numeric service mocks. '
            'Extended returns, memory, ABI, x87 status and retained caller registers across selected '
            'exceptional inputs and all supported precision/rounding modes. Unmasked exceptions '
            'and complete runtime engine behavior remain unverified.',
            operation_counts=dict(Counter(c['inputs']['operation'] for c in full['shared_math'])))
    aim = Counter('matrix' if c['inputs'].get('real_compose') else
                  'inverse_sine' if c['inputs'].get('real_trig') else 'controlled'
                  for c in full['aim'])
    focused('aim-verification.json', 'aim',
            'Original/source event wrapper and aiming-limit predicate with controlled services and '
            'callback mutations. Selected cases execute original/source matrix composition and '
            'retained native inverse-sine runtime. Complete aiming/firing and live-match '
            'equivalence remain unverified.',
            controlled_dependency_cases=aim['controlled'],
            real_inverse_sine_cases=aim['inverse_sine'], real_matrix_and_inverse_sine_cases=aim['matrix'])
    geometry = Counter(c['group'] for c in full['aim_geometry'])
    focused('aim-geometry-verification.json', 'aim_geometry',
            'Original/source artillery position transforms, vector differences, world/component '
            'position helpers and post-weapon aiming gate. Full fixture memory, callback order, '
            'dynamic pointer rereads, ABI, aliasing and x87 state are compared across boundary and '
            'nonfinite values. Object methods and selected aim-direction services remain controlled; '
            'complete trajectory and firing behavior remain unverified.',
            geometry_cases=geometry['geometry'], gate_cases=geometry['gate'])
    focused('matrix-verification.json', 'matrix',
            'Original/source guarded matrix composition entries without numeric service mocks. '
            'Complete backing memory includes partial/full input/output overlap; EAX destination '
            'return, stack cleanup, preserved registers, x87 status/control and retained caller values '
            'checked across finite/nonfinite inputs and supported precision/rounding modes. '
            'Unmasked exceptions, invalid pointers and full engine runtime equivalence remain unverified.',
            buffer_layout_counts=dict(Counter(c['inputs'].get('layout', 'separate') for c in full['matrix'])),
            retained_depth_counts=dict(Counter(str(len(c['retained'])) for c in full['matrix'])))
    return outputs


if __name__ == '__main__':
    # Validate both targets before replacing any report. A passing old build is
    # never relabeled with a newly compiled executable's hash.
    prepared = {target: prepare(target) for target in ('client', 'server')}
    for target, outputs in prepared.items():
        for name, report in outputs.items():
            (REPORTS / target / name).write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print(f"{target}: exported {outputs['verification-summary.json']['passed']} comparisons "
              f"and {outputs['live-exceptions.json']['passed']} live executions")
