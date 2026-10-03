"""Export compact public evidence only after both current builds pass verification.

Run after verify.py and verify_live_exceptions.py. Full local cases stay in build/;
public reports contain counts, scope, addresses and hashes, never game binaries.
"""
import hashlib
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
