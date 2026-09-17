"""Exercise per-character parameters in isolated copies, leaving user art alone."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

p=argparse.ArgumentParser()
p.add_argument('--rom',required=True)
a=p.parse_args()
root=Path(__file__).resolve().parents[1]
work=Path(tempfile.mkdtemp(prefix='tuning-check-',dir=root/'build'))
print(work,flush=True)
tuned={'walk_percent':200,'crouch_percent':200,'jump_percent':150,
       'gravity_percent':75,'leather_damage':90,'cross_damage':100,
       'cross_cost':3,'leather_links':7}


def run(name):
    folder=work/name
    folder.mkdir()
    exe=folder/'SuperCastlevaniaIvSNESRecomp.exe'
    shutil.copy2(root/'build/Release'/exe.name,exe)
    shutil.copytree(root/'build/Release/assets',folder/'assets')
    source=root/'build/Release/characters/simon-template'
    shutil.copytree(source,folder/'characters/simon-template')
    shutil.copytree(source,folder/'characters/test-pack')
    manifest=folder/'characters/test-pack/character.json'
    data=json.loads(manifest.read_text())
    data['gameplay']=tuned if name in ('tuned','disabled') else {}
    manifest.write_text(json.dumps(data))
    (folder/'characters/selected.txt').write_text('' if name=='disabled' else 'test-pack')
    (folder/'config.ini').write_text('[General]\nAutosave=0\nSkipLauncher=1\n'
        '[Graphics]\nNewRenderer=1\nOutputMethod=OpenGL\nVSync=0\nWidescreen=1\n'
        '[Sound]\nEnableAudio=0\n')
    env={k:v for k,v in os.environ.items() if not k.startswith('SNESRECOMP_')}
    env.update(SNESRECOMP_RUN_FRAMES='1260',SNESRECOMP_TUNING_TRACE='1')
    startup=subprocess.STARTUPINFO()
    startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow=0
    with (folder/'game.log').open('w') as log:
        subprocess.run([str(exe),'--script',str(root/'scripts/character_tuning_smoke.input'),a.rom],
                       cwd=folder,env=env,startupinfo=startup,stdout=log,stderr=log,
                       check=True,timeout=150)
    log=(folder/'game.log').read_text()
    assert '[character_tuning]' not in log, log
    rows=[dict((k,int(v)) for k,v in re.findall(r'(\w+)=(\d+)',line))
          for line in log.splitlines() if line.startswith('[tuning]')]
    rows=[r for r in rows if r['frame']>=950]
    assert rows, 'No gameplay trace'
    desired=(90,100,3,7) if name=='tuned' else (32,48,1,5)
    assert all(tuple(r[k] for k in ('whip','cross','cost','links'))==desired for r in rows)
    assert rows[-1]['y']==165, 'Did not land on the floor'
    print(name,'PASS',flush=True)
    return {r['frame']:r for r in rows}


with ThreadPoolExecutor(max_workers=3) as pool:
    baseline,modified,disabled=list(pool.map(run,['default','tuned','disabled']))
assert all((baseline[f]['x'],baseline[f]['y'])==(disabled[f]['x'],disabled[f]['y'])
           for f in baseline), 'Disabled pack did not restore defaults'
assert modified[980]['x']>baseline[980]['x'], 'Walk speed did not change'
assert min(r['y'] for r in modified.values())<min(r['y'] for r in baseline.values()), 'Jump/gravity did not change'
assert modified[1010]['x']-modified[1040]['x']>baseline[1010]['x']-baseline[1040]['x'], 'Crouch speed did not change'
print('Movement, jump/gravity, damage, costs, lengths and disabled defaults: PASS')
