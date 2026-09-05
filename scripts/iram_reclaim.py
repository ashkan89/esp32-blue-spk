"""Reclaim IRAM that is being spent on a silicon erratum neither target has.

THE PROBLEM

iram0_0_seg on a classic ESP32 is 128 KB and cannot be traded for any other
memory. This firmware linked with 684 bytes of it left. That is not a margin,
it is a coincidence, and it had already bitten once: platformio.ini records that
adding the WS2812 driver overflowed the segment by 216 bytes and would not link.

Then the WROVER target arrived. External RAM needs its low-level quad-SPI
driver, esp_psram_impl_quad.c, resident in IRAM -- 4,215 bytes of it, because it
runs while the cache it would otherwise be fetched through is being
reconfigured. The build overflowed by 3,844 bytes.

WHERE THE ROOM IS

The precompiled framework libraries are built from an sdkconfig with
CONFIG_SPIRAM_CACHE_WORKAROUND=y, for the PSRAM cache erratum on ESP32
revisions below 3. One of the things that option does is place the whole of
newlib in IRAM. Two artefacts show up in a build:

  1. esp32.rom.libc-funcs.ld is not linked. It assigns atoi, strcpy, memcmp and
     friends to their ROM addresses by strong assignment, so libc.a is never
     pulled in for them at all. Adding it is worth about 2 KB.

  2. sections.ld carries about 125 lines of the form

         *libc.a:libc_a-mktime.*(.literal .literal.* .text .text.*)

     inside .iram0.text -- whole archive members, in IRAM.

THE MISTAKE THIS FILE IS THE FIX FOR

The first version of this script moved *all* of (2) to flash, on the reasoning
that nothing in ESP-IDF calls libc from a cache-disabled context. That reasoning
was wrong, and both boards panicked on the first boot after flashing:

    Guru Meditation Error: Core 0 panic'ed (Cache error).
    Cache disabled but cached memory region accessed
    memset  <- read_id_core (spi_flash/esp_flash_api.c:487)
            <- esp_flash_read_chip_id <- esp_flash_init_default_chip

read_id_core reads the flash chip's JEDEC id, which by definition runs with the
flash cache disabled -- and it calls memset. With memset moved to flash, the
fetch to execute it is a cached read that cannot be serviced. The chip panics
before app_main, in a boot loop, with no way in but a cable.

So the whole class of memory and string primitives -- memset, memcpy, strlen and
the rest -- must stay in IRAM. They are in IRAM for flash-safety, NOT only for
the PSRAM erratum, and sections.ld does not distinguish the two reasons.

WHAT IT DOES NOW

Only newlib's calendar code is moved, by explicit name. Time arithmetic is not
reachable from a cache-disabled path -- the flash driver does not compute dates
-- and it is where the bulk of the wasted IRAM was anyway: strptime_l (1,589),
_tzset_unlocked_r (1,118), mktime (892), localtime_r (584), gmtime_r (446) and
__tzcalc_limits (402) are over 5 KB between them.

An allowlist is a judgement, and this file has already been wrong once about a
judgement, so it is checked mechanically as well. Before moving a member, the
script reads the undefined symbols of every archive that sections.ld places in
IRAM -- that being exactly the set of code that may run with the cache off --
and refuses to move any libc member that defines one of them. If memset had been
on the allowlist, that check would have caught it.

Anything it cannot determine -- no nm, no libc.a, an unrecognised sections.ld --
means nothing is moved and the link is the stock one. Failing back to a build
that boots is the only acceptable failure here.

WHY THE ROM REDIRECT IS DIFFERENT, AND ALWAYS SAFE

Part (1) is not affected by any of the above. ROM lives at 0x40000000-0x40060000
and is always executable regardless of cache state, so a flash-driver call into
a ROM libc function works with the cache off. It is also what the pioarduino
platform does by itself for a non-PSRAM board -- see has_psram_config() in
builder/frameworks/arduino.py. It stays on unconditionally.

IS THE CALENDAR MOVE SAFE ON THESE TARGETS

Beyond the cache-disabled question above, which the guard now answers, there is
the erratum itself:

  WROOM-32D    No external RAM, so the erratum cannot occur at all.
  WROVER-E     ESP32-D0WD-V3 silicon, revision 3, where it is fixed in hardware.
               Checked rather than assumed: board_caps_begin() reads the chip
               revision at boot, and below revision 3 it refuses to use external
               RAM and keeps plain malloc() out of it.

TURNING IT OFF

    build_flags = -DIRAM_RECLAIM=0     ; stock link, nothing reclaimed
    build_flags = -DIRAM_RECLAIM=1     ; ROM redirect only, no sections.ld edit

With either, the WROVER fails to link with a plain IRAM overflow, which is an
honest outcome rather than a silent one.

The generated linker script is written into the build directory and regenerated
from the framework's own sections.ld on every build, so there is no forked copy
to drift out of date when the platform is updated.
"""

import re
import subprocess
from pathlib import Path

Import("env")  # type: ignore[name-defined]  # supplied by PlatformIO/SCons

ROM_LIBC_LD = "esp32.rom.libc-funcs.ld"
SECTIONS_LD = "sections.ld"

# ---------------------------------------------------------------------------
# The allowlist: newlib's calendar code, and nothing else.
#
# Every name here computes with dates. None of it is reachable from the flash
# driver, an IRAM interrupt handler, or anything else that runs with the cache
# disabled -- which is the property that matters, and which the guard below
# verifies rather than trusts.
#
# Deliberately NOT here, and the reason it is worth saying so: memset, memcpy,
# memmove, bzero, strlen, strcpy, strcmp and every other memory or string
# primitive. Those are exactly what the flash driver calls with the cache off.
# Moving memset is what caused the boot-loop panic this file's header describes.
# ---------------------------------------------------------------------------
CALENDAR_MEMBERS = {
    "libc_a-asctime", "libc_a-asctime_r",
    "libc_a-ctime", "libc_a-ctime_r",
    "libc_a-gettzinfo",
    "libc_a-gmtime", "libc_a-gmtime_r",
    "libc_a-lcltime", "libc_a-lcltime_r",
    "libc_a-mktime", "libc_a-month_lengths",
    "libc_a-strftime", "libc_a-strptime",
    "libc_a-time", "libc_a-timelocal",
    "libc_a-tzcalc_limits", "libc_a-tzlock",
    "libc_a-tzset", "libc_a-tzset_r", "libc_a-tzvars",
}

board = env.BoardConfig()  # type: ignore[name-defined]


def reclaim_level() -> int:
    """2 = everything, 1 = ROM redirect only, 0 = stock link."""
    for item in env.get("CPPDEFINES", []):  # type: ignore[name-defined]
        if isinstance(item, (list, tuple)) and len(item) == 2:
            if str(item[0]) == "IRAM_RECLAIM":
                try:
                    return max(0, min(2, int(str(item[1]))))
                except ValueError:
                    return 2
    return 2


def find_ld_dir():
    """The framework's ld directory, found through the linker's search path.

    Located rather than hardcoded: the packages directory moves with core_dir,
    which this project relocates to C:/p to stay inside Windows' 260-character
    path limit.
    """
    for entry in env.get("LIBPATH", []):  # type: ignore[name-defined]
        candidate = Path(env.subst(str(entry)))  # type: ignore[name-defined]
        if (candidate / SECTIONS_LD).is_file():
            return candidate
    return None


def nm_tool() -> str:
    """The toolchain's nm, derived from the compiler PlatformIO is using."""
    cc = env.subst("$CC")  # type: ignore[name-defined]
    return cc[:-3] + "nm" if cc.endswith("gcc") else cc.replace("gcc", "nm")


def run_nm(args):
    try:
        out = subprocess.run([nm_tool()] + args, capture_output=True, text=True,
                             timeout=180)
    except (OSError, subprocess.SubprocessError):
        return None
    return out.stdout if out.returncode == 0 else None


def section_bounds(lines, name):
    """Line range of one linker output section, or None."""
    start = None
    for index, line in enumerate(lines):
        if re.match(r"\s*" + re.escape(name) + r"\s*:", line):
            start = index
            break
    if start is None:
        return None
    for index in range(start + 1, len(lines)):
        # These scripts close a section with "} > region" on its own line.
        if re.match(r"\s*\}\s*>", lines[index]):
            return start, index
    return None


def cache_disabled_undefined(lines, iram, ld_dir):
    """Symbols referenced by code that may run with the flash cache disabled.

    Derived from sections.ld rather than from a hand-written list: every archive
    it places in .iram0.text is there because some of its code must run without
    the cache, so the union of those archives' undefined symbols is exactly what
    must not be moved out of IRAM.

    Returns None if it cannot be determined, which the caller treats as "move
    nothing".
    """
    archives = set(re.findall(r"\*(lib[A-Za-z0-9_+-]+\.a)",
                              "".join(lines[iram[0]:iram[1]])))
    archives.discard("libc.a")  # the thing being moved, not a consumer of it

    search = [ld_dir.parent / "lib", ld_dir.parent]
    for entry in env.get("LIBPATH", []):  # type: ignore[name-defined]
        search.append(Path(env.subst(str(entry))))  # type: ignore[name-defined]

    found = []
    for name in sorted(archives):
        for directory in search:
            candidate = directory / name
            if candidate.is_file():
                found.append(str(candidate))
                break
    if not found:
        return None

    out = run_nm(["--undefined-only", "--no-sort"] + found)
    if out is None:
        return None
    undefined = set()
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[0] == "U":
            undefined.add(parts[1])
    return undefined or None


def libc_member_symbols(ld_dir):
    """member name -> symbols it defines, read from the toolchain's libc.a.

    Returns None if libc.a cannot be found or read, which means nothing moves.
    """
    cc = env.subst("$CC")  # type: ignore[name-defined]
    out = run_nm(["--print-file-name", "--defined-only", "-g", "libc.a"])
    libc = None
    # The multilib copy the linker will actually use. Ask the compiler rather
    # than guessing at the "no-rtti" / "esp32" directory names.
    try:
        where = subprocess.run([cc, "-print-file-name=libc.a"],
                               capture_output=True, text=True, timeout=60)
        if where.returncode == 0:
            path = Path(where.stdout.strip())
            if path.is_file():
                libc = path
    except (OSError, subprocess.SubprocessError):
        libc = None
    if libc is None:
        return None

    out = run_nm(["--defined-only", "-g", str(libc)])
    if out is None:
        return None

    symbols = {}
    member = None
    for line in out.splitlines():
        stripped = line.strip()
        if stripped.endswith(":") and stripped[:-1].endswith(".o"):
            member = stripped[:-1][:-2]  # drop the trailing ".o"
            symbols.setdefault(member, set())
            continue
        parts = stripped.split()
        if member and len(parts) >= 3 and len(parts[1]) == 1:
            symbols[member].add(parts[2])
    return symbols or None


def move_calendar_to_flash(text, ld_dir):
    """Move the allowlisted libc members from .iram0.text to .flash.text.

    Two edits, and BOTH are required -- doing only the first is worse than doing
    neither. The .flash.text catch-all rule excludes every one of these members
    by name:

        *(EXCLUDE_FILE(... *libc.a:libc_a-mktime.* ...) .literal ... .text.*)

    so deleting the IRAM rule alone leaves the member with no home at all. The
    linker then places it as an orphan outside every memory region, and the link
    fails naming a hundred sections and not the cause.

    Returns (text, moved_members, skipped_by_guard) or (text, 0, 0) to do
    nothing.
    """
    lines = text.splitlines(keepends=True)
    iram = section_bounds(lines, ".iram0.text")
    flash = section_bounds(lines, ".flash.text")
    if not iram or not flash:
        print("iram_reclaim: %s has an unfamiliar shape; leaving it alone"
              % SECTIONS_LD)
        return text, 0, 0

    # --- the guard ---------------------------------------------------------
    undefined = cache_disabled_undefined(lines, iram, ld_dir)
    defined = libc_member_symbols(ld_dir)
    if undefined is None or defined is None:
        print("iram_reclaim: could not read the archives needed to prove the "
              "move is safe; leaving the link stock")
        return text, 0, 0

    movable = set()
    skipped = []
    for member in sorted(CALENDAR_MEMBERS):
        exported = defined.get(member)
        if not exported:
            continue  # not in this libc build; nothing to move
        clash = exported & undefined
        if clash:
            skipped.append((member, sorted(clash)[:3]))
            continue
        movable.add(member)

    for member, names in skipped:
        print("iram_reclaim: keeping %s in IRAM -- cache-disabled code "
              "references %s" % (member, ", ".join(names)))
    if not movable:
        return text, 0, len(skipped)

    whole = re.compile(
        r"^\s*\*libc\.a:(libc_a-[A-Za-z0-9_]+)\.\*"
        r"\(\.literal \.literal\.\* \.text \.text\.\*\)\s*$")

    out = []
    moved = 0
    for index, line in enumerate(lines):
        if iram[0] < index < iram[1]:
            m = whole.match(line)
            if m and m.group(1) in movable:
                moved += 1
                continue
        if flash[0] < index < flash[1] and "EXCLUDE_FILE" in line:
            for member in movable:
                line = line.replace("*libc.a:%s.* " % member, "")
                line = line.replace("*libc.a:%s.*" % member, "")
        out.append(line)
    return "".join(out), moved, len(skipped)


# ===========================================================================

level = reclaim_level()

if board.get("build.mcu", "") != "esp32":
    # These ROM addresses and this erratum belong to the classic ESP32 alone.
    print("iram_reclaim: not an esp32 target, leaving the link alone")
elif level == 0:
    print("iram_reclaim: disabled by -DIRAM_RECLAIM=0; stock link")
else:
    linkflags = [str(flag) for flag in env.get("LINKFLAGS", [])]  # type: ignore[name-defined]

    # --- part 1: newlib from ROM (always safe; ROM ignores the cache) -------
    if ROM_LIBC_LD in " ".join(linkflags):
        # Adding a linker script twice is a hard error ("appears multiple
        # times"), so this guard is load-bearing rather than tidiness.
        print("iram_reclaim: %s already linked" % ROM_LIBC_LD)
    else:
        env.Append(LINKFLAGS=["-T", ROM_LIBC_LD])  # type: ignore[name-defined]
        print("iram_reclaim: linking newlib from ROM (%s)" % ROM_LIBC_LD)

    # --- part 2: newlib's calendar code out of IRAM ------------------------
    ld_dir = find_ld_dir() if level >= 2 else None
    if level < 2:
        print("iram_reclaim: -DIRAM_RECLAIM=1; not touching %s" % SECTIONS_LD)
    elif ld_dir is None:
        print("iram_reclaim: could not find %s in the linker search path; "
              "leaving the link stock" % SECTIONS_LD)
    else:
        source = ld_dir / SECTIONS_LD
        patched, moved, skipped = move_calendar_to_flash(
            source.read_text(encoding="utf-8", errors="replace"), ld_dir)
        if not moved:
            print("iram_reclaim: nothing moved out of IRAM")
        else:
            out_dir = Path(env.subst("$BUILD_DIR"))  # type: ignore[name-defined]
            out_dir.mkdir(parents=True, exist_ok=True)
            out = out_dir / "sections-calendar-in-flash.ld"
            # Written only when it would change, so an unchanged script does
            # not force a relink on every build.
            if not out.is_file() or out.read_text(encoding="utf-8") != patched:
                out.write_text(patched, encoding="utf-8", newline="\n")

            swapped = False
            for index, flag in enumerate(linkflags):
                if Path(flag).name == SECTIONS_LD:
                    linkflags[index] = out.as_posix()
                    swapped = True
                    break
            if swapped:
                env.Replace(LINKFLAGS=linkflags)  # type: ignore[name-defined]
                print("iram_reclaim: moved %d newlib calendar member(s) out of "
                      "IRAM%s" % (moved,
                                  ", kept %d back" % skipped if skipped else ""))
            else:
                print("iram_reclaim: %s is not in LINKFLAGS; link left stock"
                      % SECTIONS_LD)
