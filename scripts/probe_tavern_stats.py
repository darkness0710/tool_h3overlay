#!/usr/bin/env python3
"""Probe the memory around the Thieves' Guild "best hero" array.

The overlay already reads one array from the game: the best hero per player,
at exe + 0x2AAA20. The game fills that in when it draws the Thieves' Guild,
which is why it only ever holds a hero the guild actually revealed.

The open question is whether the game also stages the *displayed* primary
skills somewhere near it. If it does, that buffer is the right source for
showing an opponent's attack/defence/power/knowledge, for two reasons: it
holds exactly what the player was allowed to see, and it lives in the exe,
which HotA has not rebuilt since 2023, rather than in hota.dll, which moves
every release.

Usage, with Heroes 3 running:

    # 1. snapshot with the Thieves' Guild closed
    python scripts/probe_tavern_stats.py --save closed.bin

    # 2. open the Thieves' Guild in game, then
    python scripts/probe_tavern_stats.py --save open.bin --diff closed.bin

Anything that changes between the two is a candidate. Run as administrator if
attaching fails.
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import sys

EXE_NAMES = ("h3hota.exe", "h3hota HD.exe")

# Offsets the overlay already relies on, from src/memoryscanner.cpp
EXE_TO_ACTIVE_TAVERN = 0x2AA694
EXE_TO_BEST_HERO = 0x2AAA20

# How much to dump on either side of the best hero array.
BEFORE = 0x200
AFTER = 0x400

NO_HERO = 0xFFFFFFFF
MAX_PLAYERS = 8

TH32CS_SNAPPROCESS = 0x00000002
TH32CS_SNAPMODULE = 0x00000008
TH32CS_SNAPMODULE32 = 0x00000010
PROCESS_VM_READ = 0x0010
PROCESS_QUERY_INFORMATION = 0x0400

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)


class PROCESSENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD),
                ("cntUsage", wt.DWORD),
                ("th32ProcessID", wt.DWORD),
                ("th32DefaultHeapID", ctypes.POINTER(ctypes.c_ulong)),
                ("th32ModuleID", wt.DWORD),
                ("cntThreads", wt.DWORD),
                ("th32ParentProcessID", wt.DWORD),
                ("pcPriClassBase", ctypes.c_long),
                ("dwFlags", wt.DWORD),
                ("szExeFile", ctypes.c_char * 260)]


class MODULEENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD),
                ("th32ModuleID", wt.DWORD),
                ("th32ProcessID", wt.DWORD),
                ("GlblcntUsage", wt.DWORD),
                ("ProccntUsage", wt.DWORD),
                ("modBaseAddr", ctypes.POINTER(ctypes.c_byte)),
                ("modBaseSize", wt.DWORD),
                ("hModule", wt.HMODULE),
                ("szModule", ctypes.c_char * 256),
                ("szExePath", ctypes.c_char * 260)]


def find_process():
    snap = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    entry = PROCESSENTRY32()
    entry.dwSize = ctypes.sizeof(PROCESSENTRY32)
    found = None
    if kernel32.Process32First(snap, ctypes.byref(entry)):
        while True:
            name = entry.szExeFile.decode("latin-1")
            if name in EXE_NAMES:
                found = (entry.th32ProcessID, name)
                break
            if not kernel32.Process32Next(snap, ctypes.byref(entry)):
                break
    kernel32.CloseHandle(snap)
    return found


def module_base(pid, name):
    snap = kernel32.CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)
    entry = MODULEENTRY32()
    entry.dwSize = ctypes.sizeof(MODULEENTRY32)
    base = 0
    if kernel32.Module32First(snap, ctypes.byref(entry)):
        while True:
            if entry.szModule.decode("latin-1").lower() == name.lower():
                base = ctypes.cast(entry.modBaseAddr, ctypes.c_void_p).value
                break
            if not kernel32.Module32Next(snap, ctypes.byref(entry)):
                break
    kernel32.CloseHandle(snap)
    return base


def read(handle, address, size):
    buf = (ctypes.c_char * size)()
    got = ctypes.c_size_t(0)
    ok = kernel32.ReadProcessMemory(handle, ctypes.c_void_p(address), buf,
                                    size, ctypes.byref(got))
    if not ok:
        return None
    return bytes(buf[:got.value])


def hexdump(data, base_address, changed=None):
    for row in range(0, len(data), 16):
        chunk = data[row:row + 16]
        cells = []
        for i, byte in enumerate(chunk):
            mark = "*" if changed and (row + i) in changed else " "
            cells.append("%02X%s" % (byte, mark))
        text = "".join(chr(b) if 32 <= b <= 126 else "." for b in chunk)
        print("  %08X  %-48s |%s|" % (base_address + row, "".join(cells), text))


def looks_like_stats(data, start):
    """A primary skill row is four small numbers in a row. Most heroes sit
    well under 40 in each, so treat a run of four bytes in that range as a
    candidate worth a human look."""
    hits = []
    for i in range(len(data) - 4):
        quad = data[i:i + 4]
        if all(1 <= b <= 40 for b in quad):
            hits.append((start + i, tuple(quad)))
    return hits


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--save", metavar="FILE",
                        help="write the dumped region to FILE")
    parser.add_argument("--diff", metavar="FILE",
                        help="compare against a region saved earlier")
    args = parser.parse_args()

    process = find_process()
    if process is None:
        print("Heroes 3 is not running (looked for %s)." % ", ".join(EXE_NAMES))
        return 2
    pid, exe = process
    print("Attached to %s, pid %d" % (exe, pid))

    handle = kernel32.OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
                                  False, pid)
    if not handle:
        print("Could not open the process. Try running as administrator.")
        return 2

    base = module_base(pid, exe)
    if base == 0:
        print("Could not find the base address of %s." % exe)
        return 2
    print("Module base: 0x%X" % base)

    guild_open = False
    tavern = read(handle, base + EXE_TO_ACTIVE_TAVERN, 4)
    if tavern is not None:
        value = int.from_bytes(tavern, "little")
        guild_open = value != 0
        print("Active tavern pointer: 0x%X (%s)"
              % (value, "guild is open" if guild_open else "guild is closed"))

    heroes = read(handle, base + EXE_TO_BEST_HERO, MAX_PLAYERS * 4)
    if heroes is None:
        print("Could not read the best hero array.")
        return 2
    print("Best hero per player:")
    for slot in range(MAX_PLAYERS):
        hero = int.from_bytes(heroes[slot * 4:slot * 4 + 4], "little")
        shown = "not revealed" if hero == NO_HERO else "hero id %d" % hero
        print("  player %d: %s" % (slot, shown))

    start = base + EXE_TO_BEST_HERO - BEFORE
    region = read(handle, start, BEFORE + AFTER)
    if region is None:
        print("Could not read the surrounding region.")
        return 2

    changed = None
    if args.diff:
        try:
            with open(args.diff, "rb") as saved:
                previous = saved.read()
        except OSError as error:
            print("Could not read %s: %s" % (args.diff, error))
            return 1
        if len(previous) != len(region):
            print("Saved region is a different size, cannot compare.")
        else:
            changed = {i for i in range(len(region)) if region[i] != previous[i]}
            print("\n%d of %d bytes changed since %s."
                  % (len(changed), len(region), args.diff))

    print("\nRegion around the best hero array (marked * if changed, "
          "the array itself starts at 0x%X):" % (base + EXE_TO_BEST_HERO))
    hexdump(region, start, changed)

    candidates = looks_like_stats(region, start)
    if candidates:
        print("\nRuns of four small numbers, any of which could be "
              "attack/defence/power/knowledge:")
        for address, quad in candidates[:40]:
            note = ""
            if changed and any((address - start + i) in changed for i in range(4)):
                note = "  <- changed"
            print("  0x%X: %d %d %d %d%s" % (address, quad[0], quad[1],
                                             quad[2], quad[3], note))
        if len(candidates) > 40:
            print("  ... and %d more" % (len(candidates) - 40))
    else:
        print("\nNo plausible stat rows in this region.")

    if args.save:
        with open(args.save, "wb") as out:
            out.write(region)
        print("\nSaved the region to %s" % args.save)
        if args.diff and not guild_open:
            print("\nWARNING: this snapshot was taken with the guild closed, "
                  "so it cannot show what the guild displays. Hovering the "
                  "Tavern building in the town screen is not enough: open the "
                  "Tavern and go into the Thieves' Guild, so the table with "
                  "the Best Hero row is on screen, then take this snapshot "
                  "again. The line above must read \"guild is open\".")

    kernel32.CloseHandle(handle)
    return 0


if __name__ == "__main__":
    sys.exit(main())
