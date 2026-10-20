# Player unit groups (x86 Empire.Retail.dll)

Battle groups the player makes with G, Ctrl+G and Ctrl+number, from the shortcut to the engine
group. Found 2026-10-19 for single-unit-group-fix (groupfix) by static scans of the on-disk DLL
(string references, call sites, vtables) and Ghidra stolen-byte dumps. Ghidra names at image base
0x10000000.

## Shortcuts

`data/text/default_keys.xml`, group `group_controls`: `toggle_group` (G, UI component
`button_group`), `toggle_locked_group` (Ctrl+G, component `button_lock_formation`) and
`usegroup0` to `usegroup9` (Ctrl+number, component `card_manager`).

The UI shortcut dispatcher `FUN_10fa37e0` compares the shortcut name against `deselect_all`,
`select_all`, `select_next`, `select_prev`, `toggle_group` and the `usegroup` prefix. For
`toggle_group` it calls `FUN_10f9de40`; for `usegroupN` it calls `FUN_10f91e40` with slot N-1
(slot 9 for `usegroup0`). Both run on the selection manager.

## The selection manager

A battle UI component held in the singleton at `0x11bbead8` (set by `FUN_10f9a860`, cleared by
`FUN_10f9ba40`).

| Offset | Content |
|---|---|
| +0x20 | byte, must be set for G to act |
| +0x24 | the selected units: a list object with its count at +0x28 and the array at +0x2C |
| +0x3C | list of the groups made under a number |
| +0x40 / +0x44 | the unit groups (count, array). An entry: unit count +0x0C, unit array +0x10, number slot +0x14 |
| vtable +0x18 | used by Ctrl+number to select the group of a slot |
| vtable +0x20 | group create from the selection (the battle command, below) |
| vtable +0x24 | card refresh after a create |
| vtable +0x34 | number-slot record of a new group |

The number slots are a 10-entry table at `0x117b8954` (-1 = free); `FUN_10f8d410` returns the
first free one.

## Making a group: FUN_10f89a40 (the only count check on the player's path)

`FUN_10f89a40` (ECX = manager, one stack argument: the slot, or -1) makes a group of the current
selection. Its first test compares the selected count (+0x28) with 1 and returns at once when it
is not greater: one selected unit is refused before anything is issued. Otherwise it calls the
manager's virtual +0x20 with the selection and the slot, records the new group under its slot
(virtual +0x34, list +0x3C) when the slot is not -1, and refreshes the cards (virtual +0x24).

Callers (two):

- `FUN_10f9de40` (G): when the selection is exactly one existing group (same unit count, every
  unit in the selection; `FUN_10f86a70`), the group is dissolved (`FUN_10f97e80`) and the cards
  refreshed. Otherwise the selected units leave their old groups (`FUN_10f97e80`) and a group is
  made in the first free slot (`FUN_10f8d410`, then `FUN_10f89a40`).
- `FUN_10f91e40` (Ctrl+number): returns early while two global UI flags are set
  (`0x11bbe84f`, `0x11bbe852`); when the slot already holds a group, selects it; otherwise
  removes the selected units from the groups they are in and makes the group in that slot
  through `FUN_10f89a40`.

No other compare of the selection count with 1 or 2 exists in the selection manager's code
(byte scan of the UI range for compares of a +0x28 count with 1 or 2: only this one and two in
unrelated panels).

## The lock button (Ctrl+G)

UI class `BattleButtonLockFormation` (constructor `FUN_11217120`, vtable `0x11726edc`). Its click
handler (vtable slot 10, `0x11211d20`) works on the battle UI object `[[0x11bc8180] +0x5B4]`:
when that object's list at +0x34 / +0x38 is not empty it runs `FUN_110e5f20` (per entry,
`FUN_110e5af0` on the entry's +0x14), then, when `FUN_110dbda0` finds an entry it accepts, the
button's own action `FUN_1121ff80`. Its shortcut slot handles ungrouped units through the G
handler (next section).

## How the keys reach the groups

- G (`toggle_group`) is mapped to the UI component `button_group`, a `BattleButtonGroup`
  (constructor `FUN_11216b20`, vtable `0x11725208`; the base constructor `FUN_10fa8d40` stores
  the UI component at +4). Its shortcut slot (+0x58, the generic `FUN_11065720`) runs the click
  (+0x28, `0x1120ce30`) only when its third argument is set. The click calls the battle panel's
  `FUN_11100f50` (`[[0x11bc8180] +0x5B4]`), which runs the G handler `FUN_10f9de40`.
- Ctrl+G (`toggle_locked_group`) is mapped to `button_lock_formation`. Its shortcut slot
  (`0x11211d60`) also needs the third argument; when the panel's list at +0x34 is empty
  (no group in the selection) it clears the panel byte +0xF8, calls the same `FUN_11100f50`
  and sets +0xF8 again. So Ctrl+G on ungrouped units runs the G handler too.
- Ctrl+number (`usegroupN`) and `select_next` / `select_prev` are mapped to `card_manager`; its
  shortcut handler `FUN_10fa37e0` also knows `toggle_group` and calls `FUN_10f9de40` directly.
- The battle panel is the selection manager's class: `FUN_11100f50` calls `FUN_10f9de40` with
  the panel itself, and the frame builder below works on the same object.
- An inactive button swallows its key. The land orders panel (`FUN_111024e0`) builds a state
  table for its 15 buttons with `FUN_111016d0` (0x30-byte entries, state code at +0x10: 1
  inactive, 2 active, 4 selected, 8 another inactive; setters `0x11100570`, `0x11100650`,
  `0x11100630`, `0x11100580`) and applies changed entries through `FUN_110e22f0` (code to state
  name, then the component state set `FUN_10f9bf20`). The group button is entry 12 (+0x240):
  "selected" when the selection is a group; otherwise "active" only when the number of selected
  units (the list `FUN_110ee490` fills, at `[ESP+0x38]`) is above 1 and the panel has room for
  another group (+0x40 below +0x50), else "inactive". The test is `FUN_111016d0` +0x366
  (RVA 0x1101A36). The naval panel `FUN_11102f80` calls the same state setter.
- Found with the groupfix probe in a live session: the group button's component got "inactive"
  with one unit selected and "active" with two, from `FUN_110e22f0` called by `FUN_111024e0`.
- Soft groups (G) exist only in the selection manager's list; no engine command is issued.
  Locked groups (Ctrl+G, Ctrl+number) also create an engine group
  (`BCQ_ARMY_CREATE_UNIT_GROUP`).

The orders panel `FUN_111024e0` collects its 15 land buttons by name, from the table at
`0x117bac8c`, into its list at +0x538 (`button_group` is entry 12, `button_lock_formation` 13).

## The group frame

The yellow frame with the number around a group's unit cards. The selection manager's
`FUN_110e7280` keeps one frame component per UI group (list +0x90 / +0x94 of 12-byte entries:
used flag, slot, component), creates missing ones from `data/ui/battle ui/group_frame`
("group_frame%d", `FUN_10f89590`), and places each with `FUN_11223470` (ECX = frame, the group's
unit list at group +8 with its count at +4 and array at +8, and the panel byte +0xF6, a two-row
card layout flag).

`FUN_11223470` starts by comparing the unit count with 1: one unit or none hides the frame
(`FUN_10f9c360(0)`) and returns. Past that test it shows the frame and sizes it from the cards:

- single row (flag 0): from the first card and the count, valid for any count;
- two rows, even count: from the first card and the count;
- two rows, odd count: walks the cards looking for the row break and reads the next card while
  doing so, so with one card it would read past the list.

So a group of one gets no frame, and simply lowering the test would send it into the odd-count
branch in a two-row layout.

## The engine side: no unit count check

- The battle command `BCQ_ARMY_CREATE_UNIT_GROUP` (name holder `FUN_100180c0`) has the stream
  creator and executor `FUN_102a5e40`: it reads the army and the unit count, makes the group on
  the army (`FUN_10166e40`: allocates 0x5FC bytes, constructor `FUN_10417060`, vtable
  `0x115e5ce4`, army's group count +0x154 and list +0x158), adds each unit (group virtual +0x70,
  `FUN_1041dcc0`; the unit's group pointer is unit +0xBAC), finalises the group
  (`FUN_101ffa70`) and posts UI event 0xD9. No test of the count.
- `BCQ_UNIT_GROUP_REMOVE_UNITS` (executor `FUN_102ab0b0`) removes each unit through the group's
  virtual +0x74 (a jump to `FUN_10472ed0`), which looks the unit up in the group's two unit lists
  (count +0x20 / array +0x24, count +0x60 / array +0x64) and removes it. It never disbands a
  group, whatever is left.
- `BCQ_ARMY_DESTROY_UNIT_GROUP` (executor `FUN_102a6050`) disbands a group by id through
  `FUN_10169170` (the army's list at +0x164 / +0x168, group virtual +0x6C = id).
- `BCQ_UNIT_GROUP_ADD_UNITS` has the executor `0x102aaec0`.

## The "2 or more units" messages are the scripts'

"group must have 2 or more units" / "group must have more than 1 unit" are raised by
`FUN_101c4a60`, and `FUN_10202520` refuses a list of one too. Both belong to the Lua
`unit_controller` group functions (script errors through `FUN_10ea3db0`), not to the player's
grouping. The ship versions ("group must have 2 or more ships", `BCQ_ARMY_CREATE_SHIP_GROUP`)
are the naval counterparts.

## Replays

A replay re-issues the engine's group commands (create, add, remove, destroy; groups of one
included) but runs no selection manager: the singleton at `0x11bbead8` stays null, so the UI
groups and their frames cannot be studied from a replay.

## Fix levers

- The immediate of the count test at `FUN_10f89a40` +0x3 (RVA 0xF89A46) set to 0 lets one
  selected unit through; an empty selection is still refused.
- The immediate of the frame's count test at `FUN_11223470` +0xF (RVA 0x1223482) set to 0 shows
  the frame of a one-unit group, with the two-row flag cleared for a one-unit group so it takes
  the single-row branch.
- The immediate of the group button's state test at `FUN_111016d0` +0x366 (RVA 0x1101A3A, the
  immediate at offset 4) set to 0 keeps the button active for one selected unit.

groupfix writes only those bytes after checking each whole test (`fixes/single-unit-group-fix/`).
