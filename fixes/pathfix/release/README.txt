Shogun 2 Path Fix 1.0
=====================

An unofficial fix for Total War: SHOGUN 2 battles.

In the unmodified game, a unit ordered through a gate, a doorway or round a
corner stops in a stretched block in front of the gap, then files through
one man at a time, and the men jam at the corner post. With this fix the
unit runs through the gap as a column and forms up again on the far side,
and the men take wider lines round corners instead of all aiming at the
same post. On open ground nothing changes.

The fix is always on. There are no settings and no keys.


INSTALL

1. Close the game.
2. Copy dinput8.dll into the Shogun 2 game folder, next to Shogun2.exe.
   In Steam: right-click Total War: SHOGUN 2 > Manage > Browse local files.
   The usual folder is:
   C:\Program Files (x86)\Steam\steamapps\common\Total War SHOGUN 2\
3. Start the game.


UNINSTALL

Close the game and delete dinput8.dll from the game folder.
The fix writes no other files and changes nothing on disk.


GOOD TO KNOW

- Single player and replays only. Remove the file before playing
  multiplayer: a changed movement rule would put your game out of sync
  with the other player's.
- For the original 32-bit Total War: SHOGUN 2. The 2026 Complete Edition
  is a different program and is not supported.
- If the game files differ from the version the fix was built for, the fix
  installs nothing and the game runs unmodified. If one of its run-time
  safety checks fails during a battle, the fix switches itself off until the
  game is restarted.
- Another mod that also uses a file called dinput8.dll cannot be installed
  at the same time.
- The fix was measured in battles at 3x game speed. Its timing follows the
  game clock, so other speeds should behave the same.


WHAT IT CHANGES, IN SHORT

- Formation release: when the unit's next formation position would lie
  inside a narrow passage, the formation is placed beyond the passage, so
  the men run through it instead of walking into a stretched block.
- Wider corner lines: men queueing for the same corner use the corner's
  width by rank, and men close to a corner aim further round it at a free
  spot.
- No crowd slowdown at corners: the game's slowdown for men pressed from
  all sides is skipped for men rounding a corner.

The file only changes values the game is about to use (a waypoint, a
clearance radius, a distance). Pathfinding, collision and combat are the
game's own.


Version 1.0, built from development build v41.
Copyright 2026 ShakuEight. Licensed under the Apache License 2.0 (see LICENSE and NOTICE).
Source: https://github.com/ShakuEight/Shogun2-fix
