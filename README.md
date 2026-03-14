# BullyAchievements

`BullyAchievements` Adds a full achievements system to the game `Bully: Scholarship Edition`, including both Anniversary Edition and PS4 exclusive achievements.

This repository contains both halves of the mod:

- the native `.asi` backend
- the Derpy Script Loader Lua bridge that feeds runtime telemetry, state, and popup data into the backend

## Download

- Nexus Mods: https://www.nexusmods.com/bullyscholarshipedition/mods/346

## Repository Layout

- `AchievementsASI.cpp`
  - native backend implementation
- `AchievementsASI.ini`
  - sample public config for the `.asi`
- `_derpy_script_loader/scripts/0_AchievementsMod.lua`
  - loader entry script that boots the runtime bridge
- `_derpy_script_loader/scripts/AchievementsMod/main.lua`
  - main Lua runtime bridge
- `_derpy_script_loader/scripts/AchievementsMod/runtime.lua`
  - runtime loader wrapper
- `_derpy_script_loader/scripts/AchievementsMod/AchievementsMod.ini`
  - Lua-side bridge config
- `_derpy_script_loader/scripts/AchievementsMod/rarity_snapshot.ini`
  - achievement rarity reference data used by the popup/state UI
- `compile.bat`
  - standalone build script for Visual Studio Build Tools
- `build.bat`
  - thin wrapper around `compile.bat`

## Build

Requirements:

- Visual Studio with C++ tools
- Windows 10 SDK
- Win32 target environment

Build example:

```bat
build.bat
```

Output:

- `BullyAchievements.asi`

## Install Layout

The built `.asi` belongs in the game root:

```text
Bully Scholarship Edition/
  BullyAchievements.asi
  AchievementsASI.ini
```

The Lua bridge belongs under Derpy Script Loader:

```text
Bully Scholarship Edition/
  _derpy_script_loader/
    scripts/
      0_AchievementsMod.lua
      AchievementsMod/
        main.lua
        runtime.lua
        AchievementsMod.ini
        rarity_snapshot.ini
```

## Notes

- This export does not auto-deploy into the game folder.
- The project is intended to be built as `32-bit (Win32)`.
- Runtime-generated files such as telemetry/state/popup `.ini` outputs are not committed here.
