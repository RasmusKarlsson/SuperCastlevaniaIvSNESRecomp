"""Check boot with saved run-ahead preferences in isolated executable folders."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from PIL import Image

p = argparse.ArgumentParser()
p.add_argument('--rom', required=True)
p.add_argument('--expect-visible', action='store_true')
p.add_argument('--exe', type=Path)
args = p.parse_args()
root = Path(__file__).resolve().parents[1]
work = Path(tempfile.mkdtemp(prefix='startup-check-', dir=root/'build'))
print(work, flush=True)

def run(ahead):
    folder = work / str(ahead)
    folder.mkdir()
    exe = folder/'SuperCastlevaniaIvSNESRecomp.exe'
    shutil.copy2(args.exe or root/'build/Release'/exe.name, exe)
    shutil.copytree(root/'build/Release/assets', folder/'assets')
    shutil.copytree(root/'build/Release/characters', folder/'characters')
    (folder/'config.ini').write_text(f'[General]\nAutosave=0\nSkipLauncher=1\nRunAhead={ahead}\n'
                                   '[Graphics]\nNewRenderer=1\nOutputMethod=OpenGL\nVSync=0\n'
                                   '[Sound]\nEnableAudio=0\n')
    env = {k:v for k,v in os.environ.items() if not k.startswith('SNESRECOMP_')}
    env.update(SNESRECOMP_RUN_FRAMES='1400', SNESRECOMP_SCREENSHOT_FRAME='1300',
               SNESRECOMP_SCREENSHOT=str(folder/'game.ppm'))
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    with (folder/'game.log').open('w') as log:
        subprocess.run([str(exe), '--script', str(root/'scripts/character_workshop_smoke.input'),
                        args.rom], cwd=folder, env=env, startupinfo=startup,
                       stdout=log, stderr=log, timeout=160, check=True)
    shot = Image.open(folder/'game.ppm').convert('RGB')
    shot.save(folder/'game.png')
    visible = shot.getbbox() is not None
    print(f'RunAhead={ahead}: visible={visible}', flush=True)
    if args.expect_visible:
        assert visible, f'Black screen with RunAhead={ahead}'

with ThreadPoolExecutor(max_workers=2) as pool:
    list(pool.map(run, [0, 1]))
