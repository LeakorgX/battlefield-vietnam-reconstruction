"""Package only authored world effects into a new effects_003.rfa, and the HD
sky's reflection cube into a new texture_003.rfa (--sky-env: a folder holding
W_hd_sky_env_0..5.dds, the GGX-prefiltered faces in D3D order).

Retail files and the RPG/sky archives are read-only. The archive tool validates
every packaged member after writing. --check uses the game's actual D3DX via
the private existing gamefx tool, before installing anything.
"""
import argparse
import shutil
import subprocess
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
VARIANTS = {
    'PVLS1Dif': (),
    'PVLS1DifLmp': ('WR_LIGHTMAP', 'WR_LIGHTMAP_FF'),
    'PVLPPSMask1Dif': ('WR_GLOSS_ALPHA',),
    'PPLSTs1Dif': ('WR_NORMAL',),
    'PPLSTs1DifLmp': ('WR_NORMAL', 'WR_LIGHTMAP'),
    'SknPVLS1Dif': ('WR_SKIN',),
    'SknPPLSTs1Dif': ('WR_SKIN', 'WR_NORMAL'),
    'Environment': ('WR_ENV',),
}

def run(args, cwd=None):
    result = subprocess.run([str(x) for x in args], capture_output=True, text=True, cwd=cwd)
    print(result.stdout, end='')
    if result.stderr:
        print(result.stderr)
    if result.returncode:
        raise SystemExit(f'Failed: {args[0]} ({result.returncode})')
    return result.stdout

def main():
    p = argparse.ArgumentParser(__doc__)
    p.add_argument('--game-dir', type=Path, required=True)
    p.add_argument('--rftool', type=Path, required=True)
    p.add_argument('--gamefx', type=Path, required=True)
    p.add_argument('--retail-common', type=Path, required=True)
    p.add_argument('--sky-env', type=Path, required=True)
    p.add_argument('--install', action='store_true')
    a = p.parse_args()
    work = PROJECT / 'build' / 'world-shaders'
    stage = work / 'stage' / 'effects'
    check = work / 'check' / 'effects'
    stage.mkdir(parents=True, exist_ok=True)
    check.mkdir(parents=True, exist_ok=True)
    src = PROJECT / 'mods' / 'shaders' / 'world'
    for f in src.glob('*.fx'):
        shutil.copy2(f, stage / f.name)
    for name, defines in VARIANTS.items():
        text = '// World material upgrade; authored replacement.\n'
        text += ''.join(f'#define {v} 1\n' for v in defines)
        text += '#include "effects/WorldSurface.fx"\n'
        (stage / f'RaShader{name}.fx').write_text(text)
    for name, source in [('Terrain', 'WorldTerrain.fx'), ('Water', 'WorldWater.fx')]:
        (stage / f'RaShader{name}.fx').write_text(f'#include "effects/{source}"\n')
    for f in stage.iterdir():
        shutil.copy2(f, check / f.name)
    shutil.copy2(a.retail_common, check / 'RaCommon.fx')
    logs = []
    for f in sorted(stage.glob('RaShader*.fx')):
        output = run([a.gamefx.resolve(), a.game_dir.resolve() / 'BfVietnam.exe', check.resolve().parent, 'effects/' + f.name], cwd=a.game_dir)
        if ('D3DXCreateEffect (game D3DX 9.0b): ok' not in output
                or 'FindNextValidTechnique: 00000000' not in output
                or 'SetTechnique 00000000' not in output
                or ': validate 00000000' not in output
                or 'Pass(0) 00000000' not in output):
            raise SystemExit('Effect did not compile, validate and begin: ' + f.name)
        if 'world_' not in output:
            raise SystemExit('Upgrade technique was not selected: ' + f.name)
        logs.append(f.name + '\n' + output)
    (work / 'gamefx-check.txt').write_text('\n'.join(logs))
    archives = a.game_dir / 'Mods/BfVietnam/Archives'
    run([a.rftool, 'pack', work / 'effects_003.rfa', stage.parent, archives / 'effects.rfa'])
    sky = work / 'sky-stage' / 'texture'
    sky.mkdir(parents=True, exist_ok=True)
    faces = ['PositiveX', 'NegativeX', 'PositiveY', 'NegativeY', 'PositiveZ', 'NegativeZ']
    for k in range(6):
        shutil.copy2(a.sky_env / f'W_hd_sky_env_{k}.dds', sky / f'W_hd_sky_env_{k}.dds')
    (sky / 'W_hd_sky_env.rcm').write_text('[CubeMap]\n' + ''.join(
        f'{f} = texture\\W_hd_sky_env_{k}.dds\n' for k, f in enumerate(faces)), newline='\r\n')
    run([a.rftool, 'pack', work / 'texture_003.rfa', sky.parent, archives / 'texture.rfa'])
    if a.install:
        import hashlib
        for name in ('effects_003.rfa', 'texture_003.rfa'):
            dest = archives / name
            if dest.exists():
                digest = hashlib.sha256(dest.read_bytes()).hexdigest()[:16]
                backup = a.game_dir / 'bfv-reference-local/world-render' / f'{dest.stem}-{digest}.rfa'
                backup.parent.mkdir(parents=True, exist_ok=True)
                if not backup.exists():
                    shutil.copy2(dest, backup)
            shutil.copy2(work / name, dest)
            print('Installed', dest)

if __name__ == '__main__':
    main()
