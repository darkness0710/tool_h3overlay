#!/usr/bin/env python3
"""Look for the stat labels the Thieves' Guild draws under a best hero.

The guild reveals a player's best hero in two stages: the portrait first, and
the Attack / Defense / Power / Know. box only at a higher number of Thieves'
Guilds. The overlay may only show those numbers when the guild does, and
nothing found so far tells the two stages apart - the best hero array is
already full while the box is still blank.

When the box is drawn the game has to put those four labels somewhere. The
strings also sit in the static string tables of the exe and the dll whether or
not anything is drawn, so a hit inside a module proves nothing. A copy on the
heap is what a drawn box looks like.

Run it twice, with the table open both times:

    # a save where the stat box is blank
    python scripts/find_guild_labels.py

    # a save where the stat box shows numbers
    python scripts/find_guild_labels.py

If heap copies show up only in the second, their presence is the gate the
overlay needs, and it is reachable without any hota.dll offset. Unlike the
dialog comparison this does not care that the two saves have different player
counts, because it counts occurrences rather than lining up offsets.
"""

import argparse
import sys

sys.path.insert(0, __file__.rsplit("\\", 1)[0].rsplit("/", 1)[0])
import find_guild_stats as probe

EXE_TO_ACTIVE_TAVERN = 0x2AA694
EXE_TO_BEST_HERO = 0x2AAA20
MAX_PLAYERS = 8
NO_HERO = 0xFFFFFFFF

# As they appear in the box, longest first so the distinctive ones lead.
LABELS = [b"Defense", b"Attack", b"Power", b"Know."]

CONTEXT = 24


def classify(address, modules):
    for name, base, size in modules:
        if base <= address < base + size:
            return name, "%s+0x%X" % (name, address - base)
    return None, "heap 0x%X" % address


def context_of(handle, address):
    start = max(0, address - CONTEXT)
    raw = probe.read(handle, start, CONTEXT * 3)
    if not raw:
        return ""
    return "".join(chr(b) if 32 <= b <= 126 else "." for b in raw)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--all", action="store_true",
                        help="list module hits too, not just heap ones")
    args = parser.parse_args()

    found = probe.find_process()
    if found is None:
        print("Heroes 3 is not running (looked for %s)."
              % ", ".join(probe.EXE_NAMES))
        return 2
    pid, exe = found
    handle = probe.kernel32.OpenProcess(
        probe.PROCESS_VM_READ | probe.PROCESS_QUERY_INFORMATION, False, pid)
    if not handle:
        print("Could not open the process. Try running as administrator.")
        return 2

    modules = probe.list_modules(pid)
    exe_base = next((b for n, b, _ in modules if n.lower() == exe.lower()), 0)
    print("Attached to %s, pid %d" % (exe, pid))

    if exe_base:
        raw = probe.read(handle, exe_base + EXE_TO_ACTIVE_TAVERN, 4)
        pointer = int.from_bytes(raw, "little") if raw else 0
        print("Tavern pointer : 0x%X (%s)"
              % (pointer, "table is open" if pointer else "table is NOT open"))
        if not pointer:
            print("\nOpen the Thieves' Guild before running this, otherwise "
                  "there is nothing drawn\nto find.")
        raw = probe.read(handle, exe_base + EXE_TO_BEST_HERO, MAX_PLAYERS * 4)
        if raw:
            ids = [int.from_bytes(raw[i * 4:i * 4 + 4], "little")
                   for i in range(MAX_PLAYERS)]
            print("Best hero array: %s"
                  % ["-" if h in (0, NO_HERO) else h for h in ids])

    print("\nScanning for %s ...\n"
          % ", ".join(label.decode() for label in LABELS))

    hits = {label: {"module": 0, "heap": []} for label in LABELS}
    scanned = 0
    for base, size in probe.regions(handle):
        data = probe.read(handle, base, size)
        if data is None:
            continue
        scanned += len(data)
        for label in LABELS:
            at = data.find(label)
            while at != -1:
                address = base + at
                module, _ = classify(address, modules)
                if module:
                    hits[label]["module"] += 1
                else:
                    hits[label]["heap"].append(address)
                at = data.find(label, at + 1)

    print("Scanned %.1f MB.\n" % (scanned / 1048576.0))
    print("%-10s %-14s %s" % ("label", "in modules", "on the heap"))
    for label in LABELS:
        print("%-10s %-14d %d"
              % (label.decode(), hits[label]["module"],
                 len(hits[label]["heap"])))

    heap_any = [(label, address) for label in LABELS
                for address in hits[label]["heap"]]
    if heap_any:
        print("\nHeap copies, with the bytes around them:")
        for label, address in heap_any[:40]:
            print("  0x%-10X %-8s |%s|"
                  % (address, label.decode(), context_of(handle, address)))
        if len(heap_any) > 40:
            print("  ... and %d more" % (len(heap_any) - 40))
        print("\nIf this run had the stat box visible and a run with it blank "
              "shows none of\nthese, then a heap copy means the guild is "
              "showing the numbers, and that is\nthe gate.")
    else:
        print("\nNo heap copies. Every hit is in a module string table, which "
              "is there whether\nor not anything is drawn. If the stat box "
              "was visible during this run, the\ngame draws the labels from "
              "somewhere this cannot see, and this approach is\nout.")

    if args.all:
        print("\nModule hits:")
        for base, size in probe.regions(handle):
            data = probe.read(handle, base, size)
            if data is None:
                continue
            for label in LABELS:
                at = data.find(label)
                while at != -1:
                    module, where = classify(base + at, modules)
                    if module:
                        print("  %-8s %s" % (label.decode(), where))
                    at = data.find(label, at + 1)

    probe.kernel32.CloseHandle(handle)
    return 0


if __name__ == "__main__":
    sys.exit(main())
