"""Focused numerical-helper comparison, also included by the full verifier."""
import argparse
import hashlib
import json
from pathlib import Path
import pefile
from build import PROJECT, GAME, TARGETS
from curve_oracle import run_curve, curve_cases, native_tables


def compare_curves(original, original_pe, edited, edited_pe, spec, symbols):
    cases = []
    tables = native_tables(original, original_pe, spec)
    for kind in ('curve', 'score'):
        for table_name, values, raw, cw in curve_cases(tables[kind]):
            a = run_curve(original, original_pe, spec['bailout_'+kind],
                          spec['bailout_'+kind+'_table'], raw, values, cw)
            b = run_curve(edited, edited_pe, symbols['bfv_bailout_'+kind],
                          spec['bailout_'+kind+'_table'], raw, values, cw)
            assert a == b, (kind, table_name, f'{raw:08x}', f'{cw:04x}', a, b)
            cases.append(dict(kind=kind, table=table_name, input_bits=f'{raw:08x}',
                              control_word=f'{cw:04x}', result80=a))
    return cases


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--game-dir', type=Path, default=GAME)
    parser.add_argument('--target', choices=('client','server','both'), default='both')
    args = parser.parse_args()
    for target in ('client','server') if args.target == 'both' else (args.target,):
        spec = TARGETS[target]
        work = PROJECT/'build'/target
        manifest = json.loads((work/'manifest.json').read_text())
        original = (args.game_dir/spec['file']).read_bytes()
        edited = Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest() == spec['sha']
        assert hashlib.sha256(edited).hexdigest() == manifest['output_sha256']
        cases = compare_curves(original, pefile.PE(data=original), edited, pefile.PE(data=edited),
                               spec, {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'curve-verification.json').write_text(json.dumps(cases, indent=2))
        print(f'{target}: {len(cases)} exact x87 80-bit curve comparisons passed', flush=True)
