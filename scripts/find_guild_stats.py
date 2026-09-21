#!/usr/bin/env python3
"""Find where the Thieves' Guild keeps the primary skills it displays.

The guild table shows each revealed player's best hero with Attack, Defense,
Power and Knowledge. Read those numbers off the screen, pass them in, and this
searches the whole game process for them.

What we are looking for is a buffer that holds what the guild *displayed*,
because that is the only source that respects what the player was allowed to
see. Where it lives decides whether the overlay can use it:

  * inside h3hota.exe  -> usable, the exe has not been rebuilt since 2023
  * inside hota.dll    -> usable but fragile, that module moves every release
  * on the heap        -> needs a pointer chain from somewhere stable

Run it with the guild table open on screen:

    python scripts/find_guild_stats.py --stats 1,0,3,2 0,2,1,2 2,2,1,1

Order the --stats arguments the same way the columns appear (1st, 2nd, 3rd).
Hits where several players sit close together are the interesting ones: that
is the shape of a display buffer rather than a coincidence.

The game has more than one way into this table. If a run finds nothing, try
the other route in as well, since they may not go through the same code.
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import sys

EXE_NAMES = ("h3hota.exe", "h3hota HD.exe")
EXE_TO_ACTIVE_TAVERN = 0x2AA694
EXE_TO_BEST_HERO = 0x2AAA20
MAX_PLAYERS = 8
NO_HERO = 0xFFFFFFFF

TH32CS_SNAPPROCESS = 0x00000002
TH32CS_SNAPMODULE = 0x00000008
TH32CS_SNAPMODULE32 = 0x00000010
PROCESS_VM_READ = 0x0010
PROCESS_QUERY_INFORMATION = 0x0400

MEM_COMMIT = 0x1000
PAGE_GUARD = 0x100
PAGE_NOACCESS = 0x01
READABLE = (0x02, 0x04, 0x08, 0x20, 0x40, 0x80)  # READONLY .. EXECUTE_WRITECOPY

# Two players' rows landing within this many bytes of each other means we are
# probably looking at one table rather than unrelated coincidences.
NEIGHBOUR_WINDOW = 0x400

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)


class PROCESSENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD),
                ("th32ProcessID", wt.DWORD),
                ("th32DefaultHeapID", ctypes.POINTER(ctypes.c_ulong)),
                ("th32ModuleID", wt.DWORD), ("cntThreads", wt.DWORD),
                ("th32ParentProcessID", wt.DWORD),
                ("pcPriClassBase", ctypes.c_long), ("dwFlags", wt.DWORD),
                ("szExeFile", ctypes.c_char * 260)]


class MODULEENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("th32ModuleID", wt.DWORD),
                ("th32ProcessID", wt.DWORD), ("GlblcntUsage", wt.DWORD),
                ("ProccntUsage", wt.DWORD),
                ("modBaseAddr", ctypes.POINTER(ctypes.c_byte)),
                ("modBaseSize", wt.DWORD), ("hModule", wt.HMODULE),
                ("szModule", ctypes.c_char * 256),
                ("szExePath", ctypes.c_char * 260)]


class MEMORY_BASIC_INFORMATION64(ctypes.Structure):
    """The layout a 64 bit process gets back, even for a 32 bit target."""
    _fields_ = [("BaseAddress", ctypes.c_ulonglong),
                ("AllocationBase", ctypes.c_ulonglong),
                ("AllocationProtect", wt.DWORD),
                ("__alignment1", wt.DWORD),
                ("RegionSize", ctypes.c_ulonglong),
                ("State", wt.DWORD), ("Protect", wt.DWORD),
                ("Type", wt.DWORD), ("__alignment2", wt.DWORD)]


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


def list_modules(pid):
    snap = kernel32.CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)
    entry = MODULEENTRY32()
    entry.dwSize = ctypes.sizeof(MODULEENTRY32)
    modules = []
    if kernel32.Module32First(snap, ctypes.byref(entry)):
        while True:
            base = ctypes.cast(entry.modBaseAddr, ctypes.c_void_p).value or 0
            modules.append((entry.szModule.decode("latin-1"), base,
                            entry.modBaseSize))
            if not kernel32.Module32Next(snap, ctypes.byref(entry)):
                break
    kernel32.CloseHandle(snap)
    return modules


def describe(address, modules):
    for name, base, size in modules:
        if base <= address < base + size:
            return "%s+0x%X" % (name, address - base)
    return "heap or unnamed region"


def read(handle, address, size):
    buf = (ctypes.c_char * size)()
    got = ctypes.c_size_t(0)
    if not kernel32.ReadProcessMemory(handle, ctypes.c_void_p(address), buf,
                                      size, ctypes.byref(got)):
        return None
    return bytes(buf[:got.value])


def regions(handle):
    info = MEMORY_BASIC_INFORMATION64()
    address = 0
    limit = 0x7FFFFFFF  # the game is 32 bit, nothing above this matters
    while address < limit:
        ok = kernel32.VirtualQueryEx(handle, ctypes.c_void_p(address),
                                     ctypes.byref(info),
                                     ctypes.sizeof(info))
        if not ok:
            break
        size = int(info.RegionSize)
        if size == 0:
            break
        usable = (info.State == MEM_COMMIT
                  and info.Protect in READABLE
                  and not (info.Protect & PAGE_GUARD))
        if usable:
            yield int(info.BaseAddress), size
        address = int(info.BaseAddress) + size


def patterns_for(stats):
    """A stat row could be stored as four bytes or as four 32 bit values."""
    as_bytes = bytes(stats)
    as_dwords = b"".join(int(v).to_bytes(4, "little") for v in stats)
    return [("bytes", as_bytes), ("uint32", as_dwords)]


def search(handle, stats_list, modules):
    hits = []
    scanned = 0
    for base, size in regions(handle):
        data = read(handle, base, size)
        if data is None:
            continue
        scanned += len(data)
        for index, stats in enumerate(stats_list):
            for kind, pattern in patterns_for(stats):
                at = data.find(pattern)
                while at != -1:
                    hits.append((base + at, index, kind, stats))
                    at = data.find(pattern, at + 1)
    return hits, scanned


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--stats", nargs="+", required=True, metavar="A,D,P,K",
                        help="one attack,defense,power,knowledge per revealed "
                             "player, in column order")
    args = parser.parse_args()

    stats_list = []
    for raw in args.stats:
        parts = raw.replace(" ", "").split(",")
        if len(parts) != 4 or not all(p.isdigit() for p in parts):
            print("Bad --stats value %r, expected four numbers like 1,0,3,2"
                  % raw)
            return 1
        values = [int(p) for p in parts]
        if any(v > 255 for v in values):
            print("Bad --stats value %r, a primary skill does not exceed 255"
                  % raw)
            return 1
        stats_list.append(values)

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

    modules = list_modules(pid)
    exe_base = next((b for n, b, _ in modules if n.lower() == exe.lower()), 0)
    if exe_base:
        tavern = read(handle, exe_base + EXE_TO_ACTIVE_TAVERN, 4)
        if tavern is not None:
            value = int.from_bytes(tavern, "little")
            print("Active tavern pointer: 0x%X (%s)"
                  % (value, "set" if value else "null"))
            if not value:
                print("  Note: null does not mean the table is closed, only "
                      "that this particular pointer is not set. The game has "
                      "more than one way into the table.")
        heroes = read(handle, exe_base + EXE_TO_BEST_HERO, MAX_PLAYERS * 4)
        if heroes:
            ids = [int.from_bytes(heroes[i * 4:i * 4 + 4], "little")
                   for i in range(MAX_PLAYERS)]
            print("Best hero array: %s"
                  % ["none" if h == NO_HERO else h for h in ids])

    print("\nSearching for %d stat row(s)..." % len(stats_list))
    hits, scanned = search(handle, stats_list, modules)
    print("Scanned %.1f MB, %d raw hit(s)." % (scanned / 1048576.0, len(hits)))

    if not hits:
        print("\nNothing matched. Either the table is not on screen, or the "
              "numbers passed in do not match what it shows, or the game "
              "stores them somewhere this scan cannot reach.")
        kernel32.CloseHandle(handle)
        return 0

    # A lone match of four small numbers is almost always noise. Matches from
    # different players sitting close together are not.
    hits.sort()
    print("\nClusters holding more than one player (these are the ones that "
          "matter):")
    clusters = []
    current = [hits[0]]
    for hit in hits[1:]:
        if hit[0] - current[-1][0] <= NEIGHBOUR_WINDOW:
            current.append(hit)
        else:
            clusters.append(current)
            current = [hit]
    clusters.append(current)

    interesting = [c for c in clusters if len({h[1] for h in c}) > 1]
    if not interesting:
        print("  none - every match was isolated, so they are probably "
              "coincidence rather than a table")
    for cluster in interesting:
        print("  cluster at 0x%X (%s):"
              % (cluster[0][0], describe(cluster[0][0], modules)))
        for address, index, kind, stats in cluster:
            print("    0x%-10X player %d  %s  as %s  [%s]"
                  % (address, index + 1,
                     ",".join(str(s) for s in stats), kind,
                     describe(address, modules)))

    print("\nAll matches by location:")
    for address, index, kind, stats in hits:
        print("  0x%-10X player %d  %s  as %s  [%s]"
              % (address, index + 1, ",".join(str(s) for s in stats), kind,
                 describe(address, modules)))

    kernel32.CloseHandle(handle)
    return 0


if __name__ == "__main__":
    sys.exit(main())
