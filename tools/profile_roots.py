#!/usr/bin/env python3
"""Fold tier-2 coverage profiles into AOT roots in recomp/bank*.cfg.

Profiles (recomp/profiles/*.json) are captured with SNESRECOMP_TIER2_CAPTURE=1
while running scripts/*.input. Every clean `call_gap` landing -- a hardware
call the interpreter had to run because no native body existed -- becomes a
`func` declaration. With --cfg-roots (passed by tools/regen.sh and by the
launcher's Generate & rebuild) those declarations are AOT roots, so both
build paths produce the same native coverage from the same committed files.

Only call landings are promoted, mirroring the framework's own profile rule:
an indirect jump can land mid-function and must stay an LLE resume point.
Bailed observations are bug evidence and are never promoted. The generator
still rejects any root it cannot prove; those keep running through LLE.

The block between the markers in each bankNN.cfg is rewritten on every run.

Usage: python tools/profile_roots.py [--profiles DIR] [--cfg-dir DIR]
"""
import argparse
import collections
import json
import pathlib
import re

BEGIN = "# >>> BEGIN profile roots (tools/profile_roots.py — do not edit)"
END = "# <<< END profile roots"


def collect(profile_dir):
    hits = collections.Counter()
    for path in sorted(profile_dir.glob("*.json")):
        manifest = json.loads(path.read_text(encoding="utf-8"))
        if not str(manifest.get("schema", "")).startswith("snesrecomp tier2 coverage"):
            raise SystemExit(f"{path}: not a tier-2 coverage manifest")
        for d in manifest.get("discoveries", ()):
            if d.get("site_kind") != "call_gap" or d.get("bail_hits", 0):
                continue
            match = re.fullmatch(r"M([01])X([01])", d["entry_mx"])
            if not match:
                continue
            pc = int(d["target_pc24"], 16)
            hits[(pc, int(match[1]), int(match[2]))] += int(d.get("clean_hits", 0))
    # One func per PC: keep the most frequently observed entry width.
    best = {}
    for (pc, m, x), count in hits.items():
        if pc not in best or count > best[pc][2]:
            best[pc] = (m, x, count)
    return best


def rewrite(cfg_path, bank, lines):
    if cfg_path.exists():
        text = cfg_path.read_text(encoding="utf-8")
    else:
        text = f"# Super Castlevania IV (USA) — bank ${bank:02X}\nbank = {bank:x}\n"
    block = "\n".join([BEGIN, *lines, END]) + "\n"
    pattern = re.compile(re.escape(BEGIN) + r".*?" + re.escape(END) + r"\n?", re.S)
    if pattern.search(text):
        text = pattern.sub(lambda _: block, text)
    else:
        text = text.rstrip("\n") + "\n\n" + block
    cfg_path.write_text(text, encoding="utf-8", newline="\n")


def main():
    root = pathlib.Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--profiles", type=pathlib.Path, default=root / "recomp" / "profiles")
    parser.add_argument("--cfg-dir", type=pathlib.Path, default=root / "recomp")
    args = parser.parse_args()

    by_bank = collections.defaultdict(list)
    for pc, (m, x, count) in sorted(collect(args.profiles).items()):
        bank = (pc >> 16) & 0x7F  # LoROM $80+ banks mirror $00+
        addr = pc & 0xFFFF
        by_bank[bank].append(
            f"func profile_{bank:02X}_{addr:04X} {addr:04x} entry_mx:{m},{x}"
            f"  # {count} clean hit(s)")
    for bank, lines in sorted(by_bank.items()):
        rewrite(args.cfg_dir / f"bank{bank:02x}.cfg", bank, lines)
        print(f"bank{bank:02x}.cfg: {len(lines)} profile root(s)")


if __name__ == "__main__":
    main()
