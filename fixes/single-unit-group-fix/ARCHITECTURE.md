# single-unit-group-fix: design

## The request

In battle, selecting two or more units and pressing G makes a soft group (a group card and a
number to select it with), Ctrl+G or Ctrl+number makes a locked group (the units keep their
formation relative to each other). With a single unit selected nothing happens: a group needs at
least two units. Players ask for groups of one unit, soft and locked, to get the card, the number
hotkey and the lock for a single unit. Not a bug, a requested feature.

## Finding the check

1. The shortcut file (`data/text/default_keys.xml`) names the functions `toggle_group`,
   `toggle_locked_group` and `usegroup0..9`. The UI shortcut dispatcher compares those names and
   calls the G handler and the Ctrl+number handler on the battle UI's selection manager.
2. Both handlers end in one function that makes a group of the selection. Its first test returns
   when one unit or none is selected. That is the refusal the player meets.
3. The game's messages "group must have 2 or more units" belong to the Lua script functions, not
   to the player's path. The battle command the UI issues (create unit group) has no count test,
   and the engine's unit add, unit remove and group destroy never look at the count either: a
   group that loses units down to one is not disbanded by the engine.
4. A byte scan of the selection manager's code finds no other compare of the selection count with
   1 or 2.

Details and addresses: `docs/re/unit_groups_x86.md`.

## The fix

The test's immediate becomes 0: the function refuses only an empty selection. The whole compare
and branch (6 bytes) is byte-checked at install; only the immediate byte is written, so no
trampoline and no detour are involved. I writes the game's 1 back (fix off) and 0 again (on).
Unload restores the 1.

Everything after the test is the game's own code for two or more units: the group create
command, the number slot, the card refresh, the dissolve on a second G.

The first live check showed two more limits in the interface:

- **The group frame.** The function that places a group's yellow frame (with its number) hides
  it when the group has fewer than two units. Its count test gets the same immediate patch. Past
  the test, the two-row card layout has a branch for an odd count that reads a second card, so
  an entry hook clears the two-row flag for a one-unit group: its frame takes the single-row
  branch, which needs only the first card.
- **G.** G is bound to the group button, and an inactive button ignores its key. The second live
  check, with the probe on, showed the button set "inactive" at one selected unit and "active" at
  two, always from the land orders panel. The panel's button state table sets the group button
  active only when more than one unit is selected; that test gets the same immediate patch.

Dev build extras:

- a hook right after the test logs every group made from a single unit into `pathfix_log.txt`;
- the probe (`GROUPFIX_PROBE=1`) logs the G and Ctrl+number shortcuts, the lock button, every
  group made from the selection and the engine's group finalise, unit add, unit remove and group
  destroy, with the selected count, the group's unit counts, the caller and the stack.

## Risks for the live check

The group code was written for two or more units. Places that may treat a group of one oddly:
the group formations (`GroupFormations.bin`) applied to one unit, a locked group's move and
rotation with one member, the group card, adding a second unit later, and a replay recorded with
groups of one played without the fix (the commands are recorded; the engine accepts them either
way, see above). A crash in one of these needs its own guard; the lever stays the same.

Multiplayer: each client runs its own UI check, so a group of one is only made on the client
with the fix; the battle command itself is accepted by the engine. Single player only, as for
every fix in this repository.
