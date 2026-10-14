"""Generate whole-project milestone progress and separate source inventory statistics.

The primary percentage counts completed full-engine acceptance milestones.
It is not an estimate of effort, time remaining or partial code coverage.
"""
import csv
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]


def c_share(t):
    return 100*t['reconstructed_bytes']/t['indexed_bytes']


def text_bar(percent,width=30):
    filled=min(width,round(percent/100*width)) if percent<=0 else max(1,min(width,round(percent/100*width)))
    return '['+'#'*filled+'.'*(width-filled)+']'


def running_share(t):
    return 100*t['running_c_bytes']/t['indexed_bytes']


def c_progress_svg(targets):
    """Per target: share of original code bytes that run as verified C at all
    (hand-written or L1 lifted), and the share that is hand-written readable C."""
    rows=[]
    def bar(y,label,share,colour,note):
        w=max(2,round(3.2*share))
        return f'''<text x="16" y="{y}" fill="#d1d5db" font-family="Arial,sans-serif" font-size="12">{label}</text>
<rect x="150" y="{y-12}" width="320" height="14" rx="7" fill="#374151"/>
<rect x="150" y="{y-12}" width="{w}" height="14" rx="7" fill="{colour}"/>
<text x="540" y="{y}" text-anchor="end" fill="{colour}" font-family="Arial,sans-serif" font-size="13" font-weight="bold">{share:.2f}%</text>
<text x="150" y="{y+15}" fill="#9ca3af" font-family="Arial,sans-serif" font-size="10">{note}</text>'''
    y=44
    for name,t in targets.items():
        rows.append(bar(y,f'{name.title()}: runs as C',running_share(t),'#60a5fa',
            f"{t['running_c_functions']:,} of {t['indexed_functions']:,} functions ({t['lifted_functions']:,} lifted L1 C)"))
        rows.append(bar(y+36,f'{name.title()}: readable C',c_share(t),'#34d399',
            f"{t['reconstructed_functions']:,} hand-written functions"))
        y+=80
    body=chr(10).join(rows)
    height=y-8
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="556" height="{height}" role="img" aria-label="Code converted to C">
<rect width="556" height="{height}" rx="10" fill="#111827"/>
<text x="16" y="20" fill="#f9fafb" font-family="Arial,sans-serif" font-size="13" font-weight="bold">CODE RUNNING AS VERIFIED C (share of original code bytes)</text>
{body}
</svg>
'''


def generate():
    registry=json.loads((ROOT/'reports/recovery-registry.json').read_text(encoding='utf-8'))
    targets={};seen=set();total=0;recovered=0;partial=0;source_cache={}
    for target in ['client','server']:
        path=ROOT/'reports'/target/'all-functions.tsv'
        with path.open(encoding='utf-8') as f:sizes={row['address']:int(row['bytes']) for row in csv.DictReader(f,delimiter='\t')}
        inventory=set(sizes)
        complete=set();incomplete=set()
        for entry in registry:
            if target not in entry['addresses']:continue
            address=entry['addresses'][target]
            assert address in inventory,(target,address)
            assert (target,address) not in seen,('duplicate native function',target,address)
            seen.add((target,address))
            source=ROOT/entry['source'];assert source.is_file()
            if entry['source'] not in source_cache:source_cache[entry['source']]=source.read_text(encoding='utf-8')
            assert entry['symbol'] in source_cache[entry['source']]
            assert entry['status'] in ['reconstructed','partial']
            (complete if entry['status']=='reconstructed' else incomplete).add(address)
        # L1 lifted C detoured by the current build (bfv-native-code/build/<target>/manifest.json)
        manifest=ROOT/'bfv-native-code/build'/target/'manifest.json';lifted=set()
        if manifest.exists():
            for patch in json.loads(manifest.read_text(encoding='utf-8'))['entry_patches']:
                if f'bfv_{target}_l1_' in patch['source'] and patch['entry'] in inventory:lifted.add(patch['entry'])
        running=complete|lifted
        targets[target]=dict(indexed_functions=len(inventory),reconstructed_functions=len(complete),
            lifted_functions=len(lifted-complete),running_c_functions=len(running),
            running_c_bytes=sum(sizes[a] for a in running),
            partial_functions=len(incomplete),indexed_bytes=sum(sizes.values()),
            reconstructed_bytes=sum(sizes[a] for a in complete),inventory_sha256=hashlib.sha256(path.read_bytes()).hexdigest())
        total+=len(inventory);recovered+=len(complete);partial+=len(incomplete)
    milestones=json.loads((ROOT/'reports/project-milestones.json').read_text(encoding='utf-8'))
    assert len(milestones)==7
    assert {m['id'] for m in milestones}==set(range(7))
    assert all(m['status'] in ['not_started','in_progress','complete'] for m in milestones)
    done=sum(m['status']=='complete' for m in milestones)
    percent=100*done/len(milestones);label=f'{percent:.0f}%'
    report=dict(metric='whole_game_completed_acceptance_milestones',percent=percent,
        display_percent=label,completed_milestones=done,total_milestones=len(milestones),
        milestones=milestones,
        definition_of_100_percent='All seven ROADMAP acceptance milestones complete: trustworthy original reference and interfaces, fully reconstructed AI, gameplay/physics and multiplayer, source-owned engine services and standalone build, and documented original-game parity.',
        counting_rule='Each full-project acceptance milestone counts equally and only when all of its acceptance requirements are met. Partial work is recorded but does not count as a completed milestone.',
        limitations='This is a coarse whole-project acceptance tracker, not an estimate of work already performed or remaining effort. Zero completed milestones does not mean no source has been recovered. Milestones differ in size; partial work cannot support a defensible precise whole-game decompilation percentage.',
        source_inventory=dict(indexed_functions=total,reconstructed_functions=recovered,
            partial_functions_excluded=partial,targets=targets,
            limitations='Secondary inventory statistics only, not whole-project progress. Native functions differ in complexity, boundaries remain disputed, and original service dependencies remain.'))
    (ROOT/'reports/progress.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    svg=f'''<svg xmlns="http://www.w3.org/2000/svg" width="366" height="64" role="img" aria-label="Whole-game completion: {label}, {done} of {len(milestones)} acceptance milestones complete; partial reconstruction underway">
<rect width="366" height="64" rx="10" fill="#111827"/>
<text x="16" y="25" fill="#d1d5db" font-family="Arial,sans-serif" font-size="14">WHOLE-GAME COMPLETION</text>
<text x="350" y="25" text-anchor="end" fill="#34d399" font-family="Arial,sans-serif" font-size="17" font-weight="bold">{label}</text>
<text x="16" y="48" fill="#9ca3af" font-family="Arial,sans-serif" font-size="12">{done} / {len(milestones)} acceptance milestones complete</text>
<text x="350" y="48" text-anchor="end" fill="#fbbf24" font-family="Arial,sans-serif" font-size="12">IN PROGRESS</text>
</svg>
'''
    (ROOT/'reports/progress.svg').write_text(svg,encoding='utf-8')
    (ROOT/'reports/c-progress.svg').write_text(c_progress_svg(targets),encoding='utf-8')
    description=f'🛠️ Whole-game completion: {label} ({done}/{len(milestones)} acceptance milestones). Partial Battlefield Vietnam client/server source reconstruction toward a fully editable standalone engine with original gameplay and multiplayer.'
    (ROOT/'reports/about-description.txt').write_text(description+'\n',encoding='utf-8')
    return report


if __name__=='__main__':
    report=generate()
    for name,t in report['source_inventory']['targets'].items():
        print(f"{name:7} runs as C   {text_bar(running_share(t))} {running_share(t):6.2f}%  ({t['running_c_functions']}/{t['indexed_functions']} functions, {t['lifted_functions']} lifted)")
        print(f"{name:7} readable C  {text_bar(c_share(t))} {c_share(t):6.2f}%  ({t['reconstructed_functions']}/{t['indexed_functions']} functions)")
    print(f"Whole-game completion: {report['display_percent']} ({report['completed_milestones']}/{report['total_milestones']} acceptance milestones)")
