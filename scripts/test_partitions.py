#!/usr/bin/env python3
"""Check partitions/esp32_16mb_ota.csv against what the chip and the build need.

Run it directly:

    python scripts/test_partitions.py

It is a plain script rather than a PlatformIO extra_script on purpose: a
partition table is checked when it is edited, not on every compile, and a
pre-build step that reads a 16 MB layout on every build is noise.

WHAT IT ASSERTS, AND WHY EACH ONE IS HERE

  tiling          Every partition must start where the previous one ended, and
                  the last must end exactly at the flash size. A gap is wasted
                  flash and is harmless; an OVERLAP is not, and is the failure
                  this file exists to prevent -- two partitions sharing a sector
                  means writing one silently destroys the other, and the symptom
                  arrives weeks later as corrupted settings.

  64 KB align     App partitions must start on a 64 KB boundary. The flash MMU
                  maps instruction memory in 64 KB pages, so a misaligned app
                  partition does not fail to flash, it fails to boot.

  OTA symmetry    ota_0 and ota_1 must be the same size. An update installs into
                  whichever slot is not running, so the smaller of the two is
                  the real image ceiling -- and if they differ, half the updates
                  succeed and half do not, depending on which slot the unit
                  happens to be booted from.

  image fits      If a build exists in .pio/build, its firmware.bin is measured
                  against the app slot with the growth allowance below. This is
                  the check that turns "43.5% used" into a number somebody
                  decided rather than one that happened.

  NVS preserved   The nvs partition must stay at offset 0x9000 with at least its
                  present size. Moving or shrinking it discards every stored
                  setting, credential, station and alarm on every unit already
                  in the field -- and unlike everything else here, that is not
                  recoverable by reflashing.

Exit status is 0 when everything passes and 1 when anything fails, so it can go
in front of a release.
"""

from __future__ import annotations

import sys
from pathlib import Path

FLASH_BYTES = 16 * 1024 * 1024
TABLE = Path(__file__).resolve().parent.parent / "partitions" / "esp32_16mb_ota.csv"
BUILD_ROOT = Path(__file__).resolve().parent.parent / ".pio" / "build"

# The offset and minimum size the deployed fleet already has. These two numbers
# are a compatibility contract, not a preference: see "NVS preserved" above.
NVS_REQUIRED_OFFSET = 0x9000
NVS_REQUIRED_SIZE = 0x5000

# How much of an app slot a release is allowed to occupy. 75% leaves a quarter
# of the slot for growth, which at the current image size is well over a
# megabyte. Chosen so that the check fails while there is still time to do
# something about it rather than on the release that finally does not fit.
APP_BUDGET = 0.75

SUBTYPE_ALIGN = {"app": 0x10000}


def parse(path: Path) -> list[dict]:
    rows = []
    for lineno, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        parts = [p.strip() for p in line.split(",")]
        if len(parts) < 5:
            raise SystemExit("%s:%d: expected at least 5 fields, got %d"
                             % (path.name, lineno, len(parts)))
        rows.append({
            "line": lineno,
            "name": parts[0],
            "type": parts[1],
            "subtype": parts[2],
            "offset": int(parts[3], 0),
            "size": int(parts[4], 0),
        })
    return rows


def main() -> int:
    if not TABLE.is_file():
        print("FAIL  %s does not exist" % TABLE)
        return 1

    rows = parse(TABLE)
    failures: list[str] = []
    notes: list[str] = []

    print("partition table: %s" % TABLE.name)
    print("%-10s %-6s %-9s %10s %10s %10s"
          % ("name", "type", "subtype", "offset", "size", "end"))
    for r in rows:
        print("%-10s %-6s %-9s %10s %10s %10s"
              % (r["name"], r["type"], r["subtype"],
                 hex(r["offset"]), hex(r["size"]), hex(r["offset"] + r["size"])))
    print()

    # --- tiling and alignment ----------------------------------------------
    ordered = sorted(rows, key=lambda r: r["offset"])
    if ordered != rows:
        notes.append("rows are not in ascending offset order (harmless, but "
                     "the file is easier to check when they are)")

    cursor = ordered[0]["offset"]
    for r in ordered:
        if r["offset"] < cursor:
            failures.append("%s starts at %s, inside the previous partition "
                            "which ends at %s -- writing one would destroy the "
                            "other" % (r["name"], hex(r["offset"]), hex(cursor)))
        elif r["offset"] > cursor:
            notes.append("%d bytes unused between %s and %s"
                         % (r["offset"] - cursor, hex(cursor), r["name"]))
        cursor = r["offset"] + r["size"]

        align = SUBTYPE_ALIGN.get(r["type"])
        if align and r["offset"] % align:
            failures.append("%s starts at %s, which is not a multiple of %s -- "
                            "an app partition must be page-aligned for the "
                            "flash MMU or the image will not boot"
                            % (r["name"], hex(r["offset"]), hex(align)))

    if cursor > FLASH_BYTES:
        failures.append("the table runs to %s, past the %s of flash fitted"
                        % (hex(cursor), hex(FLASH_BYTES)))
    elif cursor < FLASH_BYTES:
        notes.append("%d bytes of flash past %s are not in any partition"
                     % (FLASH_BYTES - cursor, hex(cursor)))

    # --- OTA slots ----------------------------------------------------------
    apps = [r for r in rows if r["type"] == "app"]
    if len(apps) != 2:
        failures.append("expected two app partitions for OTA, found %d"
                        % len(apps))
    elif apps[0]["size"] != apps[1]["size"]:
        failures.append("the OTA slots are different sizes (%s and %s). An "
                        "update installs into whichever slot is not running, so "
                        "the smaller one is the real ceiling and updates would "
                        "succeed or fail depending on which slot booted"
                        % (hex(apps[0]["size"]), hex(apps[1]["size"])))

    # --- NVS compatibility --------------------------------------------------
    nvs = next((r for r in rows if r["subtype"] == "nvs"), None)
    if nvs is None:
        failures.append("there is no nvs partition; every stored setting lives "
                        "there")
    else:
        if nvs["offset"] != NVS_REQUIRED_OFFSET:
            failures.append("nvs is at %s but every deployed unit has it at %s. "
                            "Moving it discards all stored settings, and no "
                            "reflash brings them back"
                            % (hex(nvs["offset"]), hex(NVS_REQUIRED_OFFSET)))
        if nvs["size"] < NVS_REQUIRED_SIZE:
            failures.append("nvs is %s, smaller than the %s deployed units "
                            "have. Shrinking it discards stored settings"
                            % (hex(nvs["size"]), hex(NVS_REQUIRED_SIZE)))

    # --- do the built images fit? ------------------------------------------
    slot = apps[0]["size"] if apps else 0
    if slot and BUILD_ROOT.is_dir():
        found = False
        for env_dir in sorted(BUILD_ROOT.iterdir()):
            image = env_dir / "firmware.bin"
            if not image.is_file():
                continue
            found = True
            size = image.stat().st_size
            pct = 100.0 * size / slot
            budget = slot * APP_BUDGET
            status = "ok" if size <= budget else "OVER BUDGET"
            print("image  %-26s %9d bytes  %5.1f%% of slot  %s"
                  % (env_dir.name, size, pct, status))
            if size > slot:
                failures.append("%s is %d bytes and does not fit the %d-byte "
                                "app slot at all"
                                % (env_dir.name, size, slot))
            elif size > budget:
                failures.append("%s uses %.1f%% of its app slot, past the %.0f%% "
                                "budget. Either shrink the image or resize the "
                                "partitions deliberately"
                                % (env_dir.name, pct, APP_BUDGET * 100))
        if not found:
            notes.append("no built images under .pio/build to measure; run a "
                         "build first to check the fit")
        else:
            print()

    # --- report -------------------------------------------------------------
    for note in notes:
        print("note  %s" % note)
    if notes:
        print()
    for failure in failures:
        print("FAIL  %s" % failure)

    if failures:
        print("\n%d check(s) failed." % len(failures))
        return 1
    print("All partition checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
