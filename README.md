# Saiyan in the Old West

A Windows ASI prototype for RDR2 Story Mode. The current playable slice adds KI charging, a KI bar, and a forward blast using RDR2's native explosion effect.

## Current controls

- Hold `F8` to charge KI.
- Tap `F9` to spend KI and fire a blast in the direction Arthur faces.
- Hold and release `F10` after the minimum charge to fire a stronger super blast; longer charge increases its damage scale.
- Tap `F11` while aiming at a target to vanish to a nearby safe point, or vanish forward when not aiming at an entity.
- Tap `F12` to toggle a KI-draining speed and weapon-damage boost.
- Tap `F7` near a living friendly/neutral human or incapacitated hostile to fuse; Arthur keeps his outfit, gains the target's hair and up to two compatible clothing categories, and receives a timed stat boost.
- The KI bar is drawn along the bottom of the screen.

Hotkeys and tuning are configurable in `SaiyanInTheOldWest.ini`. F7-F12 are provisional defaults; change them if they conflict with your RDR2 bindings. Keyboard is supported in this slice; gamepad bindings are not implemented yet.

The current defaults are starting values, not balance decisions: max KI 100, initial KI 25, charge 30/s, regeneration 8/s after a 2-second delay, blast cost 25, range 12m, and damage scale 25%. The super blast costs 50 KI and scales from 50% to 100% over 2.5 seconds; vanish costs 20; transformation costs 20 to activate, then drains 5 KI/s. Fusion costs 40 KI, lasts 30 seconds, grants 125% speed and 150% weapon damage, and drains 2 KI/s.

## Requirements

- Windows x64
- RDR2 Story Mode
- Ultimate ASI Loader
- Script Hook RDR2 runtime (`ScriptHookRDR2.dll`)
- CMake 3.20+ and Visual Studio 2022 C++ tools

The mod checks `NETWORK_IS_SESSION_ACTIVE` and disables its gameplay loop during active network sessions. It is not for Red Dead Online. No game assets are included; the blast uses a built-in RDR2 effect.

## Build and install

1. Keep the Ultimate ASI Loader already installed in the RDR2 game directory. From the Script Hook RDR2 runtime package, copy `ScriptHookRDR2.dll` into that directory; do not overwrite the existing Ultimate ASI Loader proxy DLL or install the Script Hook package's separate proxy loader.
2. From PowerShell, run:

	```powershell
	.\scripts\one_click_check.ps1
	```

3. Copy `build\Release\SaiyanInTheOldWest.asi` and `build\Release\SaiyanInTheOldWest.ini` into the RDR2 game directory.
4. Launch Story Mode. The plugin writes `SaiyanInTheOldWest.log` beside the ASI.

The container can run the portable KI tests with `./scripts/one_click_check.sh`. Windows CI also builds the ASI artifact. This workspace has no RDR2 install, so in-game effects remain untested here.

## Not implemented yet

Gamepad controls, custom Kamehameha beam visuals, transformation aura/music, and runtime reads of Sparking! ZERO assets are not in this slice. The charged super currently uses a built-in RDR2 explosion effect. Fusion transfers native RDR2 ped components only; incompatible clothing categories are skipped. The repository does not bundle Script Hook files or assets from either game.
