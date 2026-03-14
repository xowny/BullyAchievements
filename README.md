# BullyAchievements

`BullyAchievements` is a standalone `.asi` mod for `Bully: Scholarship Edition`.

This folder is the GitHub-ready source export for the achievements backend source code.

## Contents

- `AchievementsASI.cpp`
  - native backend implementation
- `AchievementsASI.ini`
  - sample public config
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

## Notes

- This export does not auto-deploy into the game folder.
- The project is intended to be built as `32-bit (Win32)`.
- `AchievementsASI.ini` is included as the public-facing sample config.
