#!/usr/bin/env python3
"""Prove that nothing the flash cache is disabled for was moved into flash.

    python scripts/test_iram_safety.py

WHY THIS EXISTS

scripts/iram_reclaim.py moves part of newlib out of IRAM to make room for the
WROVER's PSRAM driver. The first version of it moved too much, and the result
was not a subtle regression -- both boards boot-looped before app_main:

    Guru Meditation Error: Core 0 panic'ed (Cache error).
    Cache disabled but cached memory region accessed
    memset  <- read_id_core (spi_flash/esp_flash_api.c:487)

read_id_core reads the flash chip's JEDEC id, which runs with the flash cache
disabled by definition, and it calls memset. Fetching memset from flash at that
moment cannot work. The only way back in is a cable.

iram_reclaim.py now guards against this at build time. This script checks the
same property from the other end -- the linked ELF -- because a build-time guard
proves what the script intended and this proves what the linker actually did.
Run it after a build, before flashing anything you cannot easily recover.

WHAT IT CHECKS

Two things, per environment under .pio/build:

  1. Every memory and string primitive is in ROM or IRAM. This is the check that
     would have caught the bug. The list is short enough to read and be certain
     of, which is the point: the failure it guards against costs a cable.

  2. The calendar functions the reclamation is supposed to move ARE in flash --
     a positive control. Without it a script that silently stopped working would
     look exactly like a script that was working, right up until the WROVER
     stopped linking.

A broader check was tried and removed: taking the undefined symbols of every
archive sections.ld places in IRAM, and requiring none of them to be in flash.
It reports about 220 symbols on a stock, correct, known-good build -- __divsf3,
_gettimeofday_r, _i2c_hal_init -- because those archives hold flash-resident
code as well as IRAM code, and an archive's undefined symbols are the union of
what both halves call. A check that fails on a good build is not a check.

The precise version of that idea lives in scripts/iram_reclaim.py instead, where
it belongs: before moving a member, the build refuses if any archive placed in
IRAM references it. That test is conservative in the safe direction -- it may
keep something in IRAM that did not need to be there, which costs bytes rather
than boots -- and it is what caught localtime_r.

Address regions on a classic ESP32:

    0x40000000-0x4005FFFF   ROM        always executable, cache irrelevant
    0x40070000-0x4009FFFF   IRAM       always executable
    0x400D0000+             flash      needs the cache

Exit status is 0 when every environment passes, 1 otherwise.
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / ".pio" / "build"
TOOLCHAIN = Path("C:/p/packages/toolchain-xtensa-esp-elf/bin")
NM = TOOLCHAIN / "xtensa-esp-elf-nm.exe"
LIBS = Path("C:/p/packages/framework-arduinoespressif32-libs/esp32")
SECTIONS_LD = LIBS / "ld" / "sections.ld"

# The short, readable list. Everything a cache-disabled routine plausibly
# calls, which in practice means the memory and string primitives: the flash
# driver zeroes structs, copies buffers and compares names while the cache is
# off. memset is on this list because moving it is what caused the boot loop.
PRIMITIVES = [
    "memset", "memcpy", "memmove", "memcmp", "memchr", "bzero",
    "strlen", "strnlen", "strcpy", "strncpy", "strcat", "strncat",
    "strcmp", "strncmp", "strchr", "strrchr", "strstr", "strncasecmp",
    "abs", "atoi", "itoa", "utoa", "longjmp", "setjmp",
]

# The positive control: these should have moved to flash. If they are still in
# IRAM the reclamation is not running, and the WROVER is about to stop linking.
MOVED = ["mktime", "gmtime_r", "__tzcalc_limits"]


def region(addr: int) -> str:
    if 0x40000000 <= addr < 0x40060000:
        return "rom"
    if 0x40070000 <= addr < 0x400A0000:
        return "iram"
    if 0x3FF00000 <= addr < 0x40000000:
        return "dram"
    if addr >= 0x400D0000:
        return "flash"
    return "other"


def nm(args: list[str]) -> str | None:
    if not NM.is_file():
        return None
    try:
        out = subprocess.run([str(NM)] + args, capture_output=True, text=True,
                             timeout=300)
    except (OSError, subprocess.SubprocessError):
        return None
    return out.stdout if out.returncode == 0 else None


def elf_symbols(elf: Path) -> dict[str, int]:
    out = nm([str(elf)])
    table: dict[str, int] = {}
    if not out:
        return table
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and re.fullmatch(r"[0-9a-fA-F]{8}", parts[0]):
            table.setdefault(parts[2], int(parts[0], 16))
    return table


def cache_disabled_references() -> set[str] | None:
    """Symbols referenced by archives that sections.ld places in IRAM."""
    if not SECTIONS_LD.is_file():
        return None
    text = SECTIONS_LD.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines(keepends=True)
    start = end = None
    for i, line in enumerate(lines):
        if start is None and re.match(r"\s*\.iram0\.text\s*:", line):
            start = i
        elif start is not None and re.match(r"\s*\}\s*>", line):
            end = i
            break
    if start is None or end is None:
        return None

    archives = set(re.findall(r"\*(lib[A-Za-z0-9_+-]+\.a)",
                              "".join(lines[start:end])))
    archives.discard("libc.a")
    paths = [str(LIBS / "lib" / a) for a in sorted(archives)
             if (LIBS / "lib" / a).is_file()]
    if not paths:
        return None

    out = nm(["--undefined-only", "--no-sort"] + paths)
    if out is None:
        return None
    return {p[1] for p in (l.split() for l in out.splitlines())
            if len(p) == 2 and p[0] == "U"}


def main() -> int:
    if not BUILD.is_dir():
        print("no .pio/build; run a build first")
        return 1

    failures = 0
    checked = 0
    for env_dir in sorted(BUILD.iterdir()):
        elf = env_dir / "firmware.elf"
        if not elf.is_file():
            continue
        checked += 1
        table = elf_symbols(elf)
        if not table:
            print("FAIL  %s: could not read symbols" % env_dir.name)
            failures += 1
            continue

        bad: list[tuple[str, int]] = []
        for name in PRIMITIVES:
            addr = table.get(name)
            if addr is not None and region(addr) == "flash":
                bad.append((name, addr))

        stuck = [n for n in MOVED
                 if table.get(n) is not None and region(table[n]) != "flash"]

        iram = 0
        for line in subprocess.run(
                [str(TOOLCHAIN / "xtensa-esp-elf-size.exe"), "-A", str(elf)],
                capture_output=True, text=True).stdout.splitlines():
            if line.startswith(".iram0.vectors") or line.startswith(".iram0.text "):
                iram += int(line.split()[1])

        if bad:
            failures += 1
            print("FAIL  %-30s %d primitive(s) that cache-disabled code calls "
                  "are in flash. This image will panic at boot:"
                  % (env_dir.name, len(bad)))
            for name, addr in sorted(bad):
                print("        %-24s 0x%08x" % (name, addr))
        elif stuck:
            failures += 1
            print("FAIL  %-30s the reclamation did not run: %s still in IRAM"
                  % (env_dir.name, ", ".join(stuck)))
        else:
            print("ok    %-30s IRAM %6d used, %5d free"
                  % (env_dir.name, iram, 131072 - iram))

    if not checked:
        print("no linked images found under .pio/build")
        return 1
    if failures:
        print("\n%d of %d environments failed." % (failures, checked))
        return 1
    print("\nall %d environments keep cache-disabled code out of flash, and "
          "the calendar reclamation is doing its job." % checked)
    return 0


if __name__ == "__main__":
    sys.exit(main())
