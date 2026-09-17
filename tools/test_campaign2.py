"""Isolated smoke checks for START 2; never writes the installed character pack."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument('--rom', required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
work = Path(tempfile.mkdtemp(prefix='campaign-check-', dir=root / 'build'))
print(work, flush=True)


def run(case):
    name, script, frames, shot_frame = case
    folder = work / name
    folder.mkdir()
    exe = folder / 'SuperCastlevaniaIvSNESRecomp.exe'
    shutil.copy2(root / 'build/Release' / exe.name, exe)
    shutil.copytree(root / 'build/Release/assets', folder / 'assets')
    (folder / 'config.ini').write_text(
        '[General]\nAutosave=0\nSkipLauncher=1\n'
        '[Graphics]\nNewRenderer=1\nWidescreen=1\nOutputMethod=OpenGL\nVSync=0\n'
        '[Sound]\nEnableAudio=0\n')
    env = {k: v for k, v in os.environ.items() if not k.startswith('SNESRECOMP_')}
    env.update(SNESRECOMP_RUN_FRAMES=str(frames),
               SNESRECOMP_SCREENSHOT_FRAME=str(shot_frame),
               SNESRECOMP_SCREENSHOT=str(folder / 'frame.ppm'),
               SNESRECOMP_CAMPAIGN_PROBE='1')
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    with (folder / 'game.log').open('w') as log:
        subprocess.run([str(exe), '--script', str(root / 'scripts' / script), args.rom],
                       cwd=folder, env=env, startupinfo=startup, stdout=log,
                       stderr=log, timeout=180, check=True)
    log = (folder / 'game.log').read_text()
    shot = Image.open(folder / 'frame.ppm').convert('RGB')
    shot.save(folder / 'frame.png')
    assert shot.getbbox(), f'{name}: black screen'
    if name == 'room':
        assert 'choice=1 active=1' in log
        positions = [tuple(map(int, p)) for p in re.findall(
            r'\[campaign_room\] frame=(\d+) x=(\d+) y=(\d+)', log)]
        assert any(y < 150 for _, _, y in positions), 'jump did not leave floor'
        assert positions[-1][2] == 165, 'player did not land'
        assert max(x for _, x, _ in positions) == 240, 'right boundary failed'
        # No inherited scenery above or left of the player at the right wall.
        assert shot.crop((0, 0, shot.width, 120)).getbbox() is None
        assert shot.crop((0, 120, shot.width // 2, 192)).getbbox() is None
        assert all(shot.getpixel((x, 200)) != (0, 0, 0)
                   for x in range(shot.width)), 'missing widescreen floor'
    elif name == 'original':
        assert 'phase=4/5' in log and 'active=1' not in log
        assert len(shot.getcolors(100000)) > 30, 'original scenery missing'
    elif name == 'options':
        assert 'phase=5/0' in log and 'active=1' not in log
    print(f'{name}: PASS', flush=True)


with ThreadPoolExecutor(max_workers=2) as pool:
    list(pool.map(run, [
        ('room', 'campaign2_smoke.input', 1320, 1300),
        ('original', 'widescreen_smoke.input', 1320, 1300),
        ('options', 'campaign2_options_smoke.input', 680, 650),
        ('menu', 'campaign2_smoke.input', 330, 325),
    ]))
