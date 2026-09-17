"""Exercise exact OAM matching on stairs without modifying user artwork."""
import argparse
import csv
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from PIL import Image

p=argparse.ArgumentParser()
p.add_argument('--rom', required=True)
p.add_argument('--script',default='character_stairs_smoke.input')
p.add_argument('--frames',type=int,default=1850)
p.add_argument('--screenshot',type=int,default=1400)
args=p.parse_args()
root=Path(__file__).resolve().parents[1]
work=Path(tempfile.mkdtemp(prefix='stairs-check-',dir=root/'build'))
print(work,flush=True)

def run(name):
    folder=work/name
    folder.mkdir()
    exe=folder/'SuperCastlevaniaIvSNESRecomp.exe'
    shutil.copy2(root/'build/Release'/exe.name,exe)
    shutil.copytree(root/'build/Release/assets',folder/'assets')
    shutil.copytree(root/'build/Release/characters/simon-template',folder/'characters/simon-template')
    source='starter-character' if name=='edited' else 'simon-template'
    shutil.copytree(root/'build/Release/characters'/source,folder/'characters/test-pack')
    (folder/'characters/selected.txt').write_text('' if name=='baseline' else 'test-pack')
    (folder/'config.ini').write_text('[General]\nAutosave=0\nSkipLauncher=1\n'
                                   '[Graphics]\nNewRenderer=1\nOutputMethod=OpenGL\nVSync=0\nWidescreen=1\n'
                                   '[Sound]\nEnableAudio=0\n')
    env={k:v for k,v in os.environ.items() if not k.startswith('SNESRECOMP_')}
    env.update(SNESRECOMP_RUN_FRAMES=str(args.frames),SNESRECOMP_CHARACTER_TRACE='1',
               SNESRECOMP_CHARACTER_DIAG='1',SNESRECOMP_SCREENSHOT_FRAME=str(args.screenshot),
               SNESRECOMP_SCREENSHOT=str(folder/'game.ppm'),
               SNESRECOMP_PRESENT_LOG=str(folder/'frames.csv'))
    startup=subprocess.STARTUPINFO()
    startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow=0
    with (folder/'game.log').open('w') as log:
        subprocess.run([str(exe),'--script',str(root/'scripts'/args.script),args.rom],
                       cwd=folder,env=env,startupinfo=startup,stdout=log,stderr=log,check=True,timeout=160)
    Image.open(folder/'game.ppm').save(folder/'game.png')
    rows=[line for line in (folder/'game.log').read_text().splitlines() if line.startswith('[character_frame]')]
    print(name, 'frames',len(rows),'unmatched',sum('matched=0' in x for x in rows),
          'replaced',sum('replaced=1' in x for x in rows),flush=True)

with ThreadPoolExecutor(max_workers=3) as pool:
    list(pool.map(run,['baseline','native','edited']))

def checksums(name):
    with (work/name/'frames.csv').open() as f:
        return [(r['frame'],r['crc32']) for r in csv.DictReader(f)]

baseline=checksums('baseline')
assert len(baseline)==args.frames, 'Incomplete capture'
assert baseline==checksums('native'), 'Full-frame rendering changed native artwork'
movement=[line for line in (work/'edited/game.log').read_text().splitlines()
          if line.startswith('[character_frame]') and int(line.split()[1])>=1100]
assert len(movement)==args.frames-1099, 'Incomplete movement trace'
assert all('replaced=1' in line for line in movement), 'Replacement dropped out during movement'
if args.script=='character_top_edge_smoke.input':
    clipped=[line for line in (work/'edited/game.log').read_text().splitlines()
             if line.startswith('[character_frame]') and 'clipped=1' in line]
    assert clipped and all('replaced=1' in line for line in clipped), 'Top-edge replacement not covered'
    assert any('id=a32c-' in line for line in clipped), 'Airborne attack did not cross the top edge'
    print(f'PASS: {len(clipped)} edge-clipped frames replaced',flush=True)
print(f'PASS: {len(baseline)} native frames identical; {len(movement)} movement frames replaced without dropouts',flush=True)
