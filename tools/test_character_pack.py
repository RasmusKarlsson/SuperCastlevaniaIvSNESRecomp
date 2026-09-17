"""Local, ROM-required integration test. Generates fixtures only under build/.

Usage: python tools/test_character_pack.py --rom PATH
Requires Pillow. Does not modify the player's character packs or configuration.
"""
import argparse
import csv
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

from PIL import Image, ImageChops


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rom', required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    exe = root / 'build/Release/SuperCastlevaniaIvSNESRecomp.exe'
    work = Path(tempfile.mkdtemp(prefix='character-test-', dir=root / 'build'))
    # The host anchors its working directory to the executable's directory.
    # Copy the binary as well, keeping all generated data isolated.
    shutil.copy2(exe, work / exe.name)
    if (exe.parent / 'assets').exists():
        shutil.copytree(exe.parent / 'assets', work / 'assets')
    exe = work / exe.name
    (work / 'config.ini').write_text('[General]\nAutosave=0\nSkipLauncher=1\n'
                                   '[Graphics]\nNewRenderer=1\nNoSpriteLimits=1\nVSync=0\n'
                                   '[Sound]\nEnableAudio=0\n')
    print(f'Test artifacts: {work}', flush=True)

    def run(name, on_started=None):
        env = dict(os.environ, SNESRECOMP_RUN_FRAMES='1600',
                   SNESRECOMP_SCREENSHOT_FRAME='1300',
                   SNESRECOMP_PRESENT_LOG=str(work / f'{name}.csv'),
                   SNESRECOMP_SCREENSHOT_FROM='1100', SNESRECOMP_SCREENSHOT_TO='1500',
                   SNESRECOMP_SCREENSHOT=str(work / f'{name}.ppm'))
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
        with (work / f'{name}.log').open('w') as log:
            proc = subprocess.Popen([str(exe), '--script', str(root / 'scripts/character_workshop_smoke.input'),
                                     str(args.rom.resolve())], cwd=work, env=env,
                                    stdout=log, stderr=log, startupinfo=startup)
            try:
                if on_started:
                    deadline = time.monotonic() + 30
                    while 'first frame simulated' not in (work / f'{name}.log').read_text():
                        assert time.monotonic() < deadline and proc.poll() is None, 'Game did not start'
                        time.sleep(.1)
                    on_started()
                assert proc.wait(timeout=150) == 0, 'Game exited with error'
            finally:
                if proc.poll() is None:
                    proc.kill()
                    proc.wait()
        result = Image.open(work / f'{name}.ppm').convert('RGB')
        result.save(work / f'{name}.png')
        return result

    native = run('native')
    template = work / 'characters/simon-template'
    manifest = json.loads((template / 'character.json').read_text())
    assert manifest['frames'], 'No complete body poses captured'
    custom = work / 'characters/test-copy'
    shutil.copytree(template, custom)
    (work / 'characters/selected.txt').write_text('test-copy')
    unchanged = run('unchanged')
    difference = ImageChops.difference(native, unchanged)
    assert difference.getbbox() is None, f'Unchanged pack differs: {difference.getbbox()}'
    def checksums(name):
        with (work / f'{name}.csv').open() as f:
            return [(row['frame'], row['crc32']) for row in csv.DictReader(f)]
    original_checksums = checksums('native')
    assert len(original_checksums) >= 400, 'Frame comparison did not capture the motion sequence'
    assert original_checksums == checksums('unchanged'), 'Unchanged pack differs during motion sequence'
    print(f'PASS: {len(manifest["frames"])} poses captured; {len(original_checksums)} untouched frames pixel-identical', flush=True)

    sheet = Image.open(custom / 'body.png').convert('RGBA')
    # Keep silhouette/alpha, use an unmistakable non-Simon color.
    sheet.paste((0, 255, 255, 255), mask=sheet.getchannel('A'))
    # Modify only after the running game has initialized its original pack.
    edited = run('edited', lambda: sheet.save(custom / 'body.png'))
    difference = ImageChops.difference(native, edited)
    bounds = difference.getbbox()
    assert bounds is not None, 'Edited artwork was not used'
    assert bounds[2] - bounds[0] <= 96 and bounds[3] - bounds[1] <= 96, bounds
    assert (0, 255, 255) in edited.get_flattened_data(), 'Custom color missing'
    print(f'PASS: live-reloaded full-body art rendered; changes confined to {bounds}', flush=True)


if __name__ == '__main__':
    main()
