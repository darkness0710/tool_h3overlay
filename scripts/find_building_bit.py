#!/usr/bin/env python3
"""Find where a town records which buildings it has built.

The Thieves' Guild reveal level is what decides whether the guild shows a best
hero's primary skills, and it comes from how many Thieves' Guilds the player
owns. That count is not in PlayerBaseStruct - measured, cleanly: building one
changes only the selection fields and the resources. Heroes 3 records
buildings per town, so it has to be in the town data instead.

Constructing a building sets exactly one bit in a bitmask, which is a rare
enough thing to happen in a few seconds of game time that it can be found by
brute force. Snapshot the game's private memory, build the Thieves' Guild,
snapshot again, and report every byte where exactly one bit went from 0 to 1.

    # standing in the town, before building
    python scripts/find_building_bit.py --save before.dump

    # build the Thieves' Guild, then straight away
    python scripts/find_building_bit.py --diff before.dump

Keep everything else still between the two: do not end the turn, move a hero,
or open another screen. The snapshot is a few hundred megabytes, so put it
somewhere with room.
"""

import argparse
import os
import struct
import sys

sys.path.insert(0, __file__.rsplit("\\", 1)[0].rsplit("/", 1)[0])
import find_guild_stats as probe

MEM_PRIVATE = 0x20000

# A record is: base, size, then that many bytes.
HEADER = struct.Struct("<QQ")

# Buildings are unlikely to live in a region this big; those are image and
# sound buffers, and reading them all makes the snapshot unwieldy.
MAX_REGION = 64 * 1024 * 1024


def private_regions(handle):
    for base, size in probe.regions(handle):
        if size > MAX_REGION:
            continue
        yield base, size


def region_type(handle, base):
    info = probe.MEMORY_BASIC_INFORMATION64()
    if not probe.kernel32.VirtualQueryEx(handle, probe.ctypes.c_void_p(base),
                                         probe.ctypes.byref(info),
                                         probe.ctypes.sizeof(info)):
        return 0
    return int(info.Type)


def save(handle, path):
    written = 0
    count = 0
    with open(path, "wb") as out:
        for base, size in private_regions(handle):
            if region_type(handle, base) != MEM_PRIVATE:
                continue
            data = probe.read(handle, base, size)
            if data is None or len(data) == 0:
                continue
            out.write(HEADER.pack(base, len(data)))
            out.write(data)
            written += len(data)
            count += 1
    print("Saved %d regions, %.1f MB, to %s"
          % (count, written / 1048576.0, path))
    return 0


def single_bit_set(old, new):
    """True when new is old with exactly one extra bit set."""
    if new == old:
        return False
    if old & new != old:
        return False
    difference = new ^ old
    return difference & (difference - 1) == 0


def load_ignore(path):
    """Addresses seen changing when nothing was done, so they say nothing."""
    if not path:
        return set()
    try:
        with open(path, "r") as handle_in:
            return {int(line.strip(), 16) for line in handle_in if line.strip()}
    except OSError as error:
        print("Could not read %s: %s" % (path, error))
        return set()


def diff(handle, path, modules, ignore_path=None, out_path=None):
    ignore = load_ignore(ignore_path)
    try:
        size = os.path.getsize(path)
    except OSError as error:
        print("Could not read %s: %s" % (path, error))
        return 1

    hits = []
    changed_bytes = 0
    compared = 0
    skipped = 0
    with open(path, "rb") as saved:
        while saved.tell() < size:
            header = saved.read(HEADER.size)
            if len(header) < HEADER.size:
                break
            base, length = HEADER.unpack(header)
            before = saved.read(length)
            if len(before) < length:
                break
            now = probe.read(handle, base, length)
            if now is None or len(now) != length:
                skipped += 1
                continue
            compared += length
            if now == before:
                continue
            for index in range(length):
                old = before[index]
                new = now[index]
                if old == new:
                    continue
                changed_bytes += 1
                if single_bit_set(old, new) and (base + index) not in ignore:
                    hits.append((base + index, old, new))

    print("Compared %.1f MB, %d bytes changed, %d regions could not be reread."
          % (compared / 1048576.0, changed_bytes, skipped))
    if ignore:
        print("Ignoring %d address(es) that change on their own." % len(ignore))
    if out_path:
        with open(out_path, "w") as out:
            for address, _, _ in hits:
                out.write("%X\n" % address)
        print("Wrote %d hit address(es) to %s" % (len(hits), out_path))

    if not hits:
        print("\nNo byte gained exactly one bit. Either the build did not "
              "happen between the\ntwo snapshots, or the bitmask is not in a "
              "private region this scan covers.")
        return 0

    print("\n%d byte(s) gained exactly one bit. A building bitmask looks like "
          "this;\nanything else that does is coincidence, so the useful ones "
          "cluster with\nother town data:\n" % len(hits))

    # Adjacent hits are far more interesting than lone ones: a town's
    # bitmask is several bytes wide and sits next to the rest of its record.
    hits.sort()
    groups = []
    current = [hits[0]]
    for hit in hits[1:]:
        if hit[0] - current[-1][0] <= 64:
            current.append(hit)
        else:
            groups.append(current)
            current = [hit]
    groups.append(current)

    for group in groups:
        where = probe_describe(group[0][0], modules)
        print("  at 0x%X (%s)%s:"
              % (group[0][0], where,
                 "  <- %d bytes together" % len(group) if len(group) > 1 else ""))
        for address, old, new in group:
            bit = (new ^ old).bit_length() - 1
            print("    0x%-10X %3d -> %3d   set bit %d" % (address, old, new, bit))
    return 0


def probe_describe(address, modules):
    for name, base, size in modules:
        if base <= address < base + size:
            return "%s+0x%X" % (name, address - base)
    return "heap"


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--save", metavar="FILE",
                        help="snapshot the game's private memory")
    parser.add_argument("--diff", metavar="FILE",
                        help="compare against a snapshot taken earlier")
    parser.add_argument("--out", metavar="FILE",
                        help="write the addresses that were hit, to feed "
                             "back in as --ignore")
    parser.add_argument("--ignore", metavar="FILE",
                        help="skip addresses listed in FILE, so a run that "
                             "changed nothing can cancel out the noise")
    args = parser.parse_args()
    if not args.save and not args.diff:
        parser.error("pass --save or --diff")

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
    print("Attached to %s, pid %d" % (exe, pid))

    if args.save:
        return save(handle, args.save)
    return diff(handle, args.diff, probe.list_modules(pid),
                args.ignore, args.out)


if __name__ == "__main__":
    sys.exit(main())
