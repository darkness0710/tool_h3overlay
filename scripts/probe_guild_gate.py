#!/usr/bin/env python3
"""Watch what decides whether the Thieves' Guild reveals a player's best hero.

The overlay wants to mirror the guild: show an opponent's primary skills when
the guild would show them, show nothing when it would not. The best hero array
at exe + 0x2AAA20 is the natural gate, but it is only useful if we can read it
without asking the player to open the table first.

That turns on one question this script answers:

  Does the game keep that array up to date on its own, or does it only fill it
  in at the moment it draws the table?

If it is maintained on its own, the array is already correct at the start of a
match and the gate needs nothing else. If it is only written when the table is
drawn, the overlay cannot tell "the guild will not reveal this" apart from "the
table has not been drawn yet", and we have to find where the guild count lives.

Run this, then play normally:

    python scripts/probe_guild_gate.py --watch

It prints a line whenever the array or the tavern pointer changes. What matters
is whether the array fills in before you ever open the table.

Take a single reading instead with no arguments.
"""

import argparse
import sys
import time

sys.path.insert(0, __file__.rsplit("\\", 1)[0].rsplit("/", 1)[0])
import find_guild_stats as probe

# From src/processhandler.cpp
EXE_TO_STATUS = 0x2992B8
PLAYER_SECTION_STEP1 = 0x5C
PLAYER_SECTION_STEP2 = 0xF60

# From src/memoryscanner.cpp
EXE_TO_ACTIVE_TAVERN = 0x2AA694
EXE_TO_BEST_HERO = 0x2AAA20

# PlayerBaseStruct, from src/gamestructs.h
PLAYER_STRUCT_SIZE = 360
OFF_COLOR = 0
OFF_NR_HEROES = 1
OFF_HERO_SLOTS = 8
OFF_NR_TOWNS = 62
OFF_OWNED_TOWNS = 64
OFF_NAME = 204
OFF_IS_LOCAL = 225
OFF_IS_HUMAN = 226

MAX_PLAYERS = 8
NO_HERO = 0xFFFFFFFF
POLL_SECONDS = 0.25


def u32(data, offset):
    return int.from_bytes(data[offset:offset + 4], "little")


class Game:
    def __init__(self, handle, exe_base):
        self.handle = handle
        self.exe_base = exe_base

    def tavern_pointer(self):
        raw = probe.read(self.handle, self.exe_base + EXE_TO_ACTIVE_TAVERN, 4)
        return int.from_bytes(raw, "little") if raw else None

    def best_heroes(self):
        raw = probe.read(self.handle, self.exe_base + EXE_TO_BEST_HERO,
                         MAX_PLAYERS * 4)
        if not raw:
            return None
        return [u32(raw, i * 4) for i in range(MAX_PLAYERS)]

    def player_section(self):
        step1 = probe.read(self.handle, self.exe_base + EXE_TO_STATUS, 4)
        if not step1:
            return None
        step2 = probe.read(self.handle,
                           int.from_bytes(step1, "little") + PLAYER_SECTION_STEP1, 4)
        if not step2:
            return None
        return int.from_bytes(step2, "little") + PLAYER_SECTION_STEP2

    def players(self):
        section = self.player_section()
        if not section:
            return []
        raw = probe.read(self.handle, section, PLAYER_STRUCT_SIZE * MAX_PLAYERS)
        if not raw:
            return []
        out = []
        for index in range(MAX_PLAYERS):
            block = raw[index * PLAYER_STRUCT_SIZE:(index + 1) * PLAYER_STRUCT_SIZE]
            name = block[OFF_NAME:OFF_NAME + 20].split(b"\x00")[0]
            out.append({
                "slot": index,
                "color": block[OFF_COLOR],
                "heroes": block[OFF_NR_HEROES],
                "towns": block[OFF_NR_TOWNS],
                "ownedTowns": list(block[OFF_OWNED_TOWNS:OFF_OWNED_TOWNS + 8]),
                "heroIDs": [u32(block, OFF_HERO_SLOTS + i * 4) for i in range(8)],
                "isLocal": block[OFF_IS_LOCAL],
                "isHuman": block[OFF_IS_HUMAN],
                "name": name.decode("latin-1", "replace"),
            })
        return out


def format_heroes(values):
    return "[" + ", ".join("-" if v in (0, NO_HERO) else str(v)
                           for v in values) + "]"


def report_players(game):
    players = game.players()
    if not players:
        print("  (could not read the player section)")
        return
    for player in players:
        if not player["isHuman"] and player["heroes"] == 0 and player["towns"] == 0:
            continue
        owned = [h for h in player["heroIDs"] if h != NO_HERO]
        print("  slot %d  color %d  %-12s heroes=%d towns=%d  %s%sowns %s"
              % (player["slot"], player["color"], player["name"],
                 player["heroes"], player["towns"],
                 "local " if player["isLocal"] else "",
                 "human " if player["isHuman"] else "ai ",
                 owned))


def once(game):
    pointer = game.tavern_pointer()
    heroes = game.best_heroes()
    print("Tavern pointer : 0x%X (%s)"
          % (pointer, "table is open" if pointer else "table is not open"))
    print("Best hero array: %s" % format_heroes(heroes))
    print("Players:")
    report_players(game)
    print("\nWhat to look for: if the array already holds heroes here and you "
          "have not opened\nthe Thieves' Guild this match, the game maintains "
          "it on its own and the overlay\ncan use it as the gate from turn one.")


def watch(game):
    print("Watching. Play normally: take a turn, then open the Thieves' Guild "
          "and close it.\nPress Ctrl+C to stop.\n")
    print("Baseline:")
    once(game)
    print("\n%-10s %-13s %s" % ("time", "tavern", "best hero array"))

    last = None
    opened_yet = False
    started = time.time()
    while True:
        pointer = game.tavern_pointer()
        heroes = game.best_heroes()
        if pointer is None or heroes is None:
            print("  lost the process")
            return
        state = (pointer != 0, tuple(heroes))
        if state != last:
            note = ""
            if last is not None and state[1] != last[1]:
                note = "  <- array changed"
                # Only meaningful while the table is shut and has never been
                # open. A change that lands on the same tick the table opens
                # is the game filling the array in as it draws, which is the
                # opposite of what we are looking for.
                if not opened_yet and not state[0]:
                    note += " with the table never opened"
                elif state[0] and not opened_yet:
                    note += " as the table opened, so it is filled on draw"
            if state[0]:
                opened_yet = True
            print("%-10.1f %-13s %s%s"
                  % (time.time() - started,
                     "open" if state[0] else "closed",
                     format_heroes(heroes), note))
            last = state
        time.sleep(POLL_SECONDS)


# Fields of PlayerBaseStruct we already understand, so a diff can say which
# changes are already explained and which are in unmapped padding.
KNOWN_FIELDS = [
    (0, 1, "color"), (1, 1, "nrOfHeroes"), (4, 4, "selectedHeroID"),
    (8, 32, "heroIDSlot"), (62, 1, "nrOfTowns"), (63, 1, "selectedTown"),
    (64, 8, "ownedTownID"), (156, 28, "resources"), (204, 20, "playerName"),
    (225, 1, "isLocal"), (226, 1, "isHuman"), (228, 1, "isRemote"),
]


def name_field(offset):
    for start, size, name in KNOWN_FIELDS:
        if start <= offset < start + size:
            return "%s+%d" % (name, offset - start)
    return "unmapped"


def dump_players(game, path):
    section = game.player_section()
    if not section:
        print("Could not read the player section.")
        return 2
    raw = probe.read(game.handle, section, PLAYER_STRUCT_SIZE * MAX_PLAYERS)
    if not raw:
        print("Could not read the player section.")
        return 2
    with open(path, "wb") as out:
        out.write(raw)
    print("Saved %d bytes of player structs to %s" % (len(raw), path))
    return 0


def diff_players(game, path):
    section = game.player_section()
    raw = probe.read(game.handle, section, PLAYER_STRUCT_SIZE * MAX_PLAYERS) \
        if section else None
    if not raw:
        print("Could not read the player section.")
        return 2
    try:
        with open(path, "rb") as saved:
            before = saved.read()
    except OSError as error:
        print("Could not read %s: %s" % (path, error))
        return 1
    if len(before) != len(raw):
        print("Saved snapshot is a different size, cannot compare.")
        return 1

    print("Differences against %s:\n" % path)
    found = False
    for index in range(MAX_PLAYERS):
        base = index * PLAYER_STRUCT_SIZE
        rows = []
        for i in range(PLAYER_STRUCT_SIZE):
            if before[base + i] != raw[base + i]:
                rows.append((i, before[base + i], raw[base + i]))
        if not rows:
            continue
        found = True
        print("player %d:" % index)
        for offset, old, new in rows:
            print("  +%-4d %-16s %3d -> %3d%s"
                  % (offset, name_field(offset), old, new,
                     "   <- candidate" if name_field(offset) == "unmapped"
                     else ""))
    if not found:
        print("  nothing changed")
    print("\nA guild count would show up as an unmapped byte going up by one "
          "on the player\nwho built it, and staying put for everyone else.")
    return 0


# The guild dialog is rebuilt at a fresh heap address every time it opens, so
# its contents are compared by offset within the object rather than by address.
GUILD_DIALOG_BYTES = 0x400


def dump_dialog(game, path):
    pointer = game.tavern_pointer()
    if not pointer:
        print("The table is not open, so there is no dialog to read. Open the "
              "Thieves' Guild and run this again.")
        return 2
    raw = probe.read(game.handle, pointer, GUILD_DIALOG_BYTES)
    if not raw:
        print("Could not read the dialog at 0x%X." % pointer)
        return 2
    with open(path, "wb") as out:
        out.write(raw)
    print("Dialog at 0x%X, saved %d bytes to %s" % (pointer, len(raw), path))
    return 0


def diff_dialog(game, path):
    pointer = game.tavern_pointer()
    if not pointer:
        print("The table is not open. Open the Thieves' Guild and run again.")
        return 2
    raw = probe.read(game.handle, pointer, GUILD_DIALOG_BYTES)
    if not raw:
        print("Could not read the dialog at 0x%X." % pointer)
        return 2
    try:
        with open(path, "rb") as saved:
            before = saved.read()
    except OSError as error:
        print("Could not read %s: %s" % (path, error))
        return 1
    if len(before) != len(raw):
        print("Saved dialog is a different size, cannot compare.")
        return 1

    print("Dialog now at 0x%X, compared against %s by offset.\n"
          % (pointer, path))
    # Heap addresses differ between openings, so a changed 32 bit value that
    # looks like a pointer says nothing. Small values are what a reveal level
    # would look like.
    interesting = []
    for i in range(GUILD_DIALOG_BYTES):
        if before[i] != raw[i]:
            interesting.append((i, before[i], raw[i]))
    if not interesting:
        print("  nothing changed")
        return 0
    print("  %d of %d bytes differ. Small values on both sides first, since a "
          "reveal level\n  is a small number and a rebuilt pointer is not:\n"
          % (len(interesting), GUILD_DIALOG_BYTES))
    small = [row for row in interesting if row[1] < 16 and row[2] < 16]
    for offset, old, new in small:
        print("    +0x%-4X %3d -> %3d" % (offset, old, new))
    if not small:
        print("    none")
    print("\n  everything else:")
    for offset, old, new in interesting:
        if (offset, old, new) not in small:
            print("    +0x%-4X %3d -> %3d" % (offset, old, new))
    return 0


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--watch", action="store_true",
                        help="keep printing whenever something changes")
    parser.add_argument("--dialog-save", metavar="FILE",
                        help="save the open guild dialog for later comparison")
    parser.add_argument("--dialog-diff", metavar="FILE",
                        help="compare the open guild dialog against a saved one")
    parser.add_argument("--save", metavar="FILE",
                        help="save every player struct for later comparison")
    parser.add_argument("--diff", metavar="FILE",
                        help="compare the player structs against a saved file")
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
    if not exe_base:
        print("Could not find the base address of %s." % exe)
        return 2
    print("Attached to %s, pid %d, base 0x%X\n" % (exe, pid, exe_base))

    game = Game(handle, exe_base)
    try:
        if args.diff:
            return diff_players(game, args.diff)
        if args.save:
            return dump_players(game, args.save)
        if args.watch:
            watch(game)
        else:
            once(game)
    except KeyboardInterrupt:
        print("\nStopped.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
