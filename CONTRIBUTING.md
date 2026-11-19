# Contributing to DeadWarrior 2

Thank you for helping test or develop the port. The current priority is Windows x64 and preserving the original game's rules.

## Set up your environment

Follow the [build guide](docs/building.md). Each contributor supplies their own compatible SWF. Do not commit the original game, decompiled scripts, generated resources, executables, saves, logs, or test captures to the source tree.

## Propose a change

1. Create a branch from `main`.
2. Keep the change focused on a specific problem or behavior.
3. For gameplay changes, identify the original classes or rules used as references and any remaining differences.
4. Build Windows and run checks appropriate to the change.
5. Open a pull request describing the expected behavior and validation performed.

Keep the 30 Hz simulation separate from rendering, preserve resource proportions, and retain the 16:9 presentation area. Do not describe a behavior as validated unless it has been tested.

## Checks

```powershell
python -m compileall -q tools
python tools/check_repository.py
.\tools\build_windows.ps1 -CheckTutorial -CheckArchidon -CheckMagic -CheckRebels -CheckGiants -CheckAudio
```

The last command requires prepared assets. Interface tests are documented in [docs/windows.md](docs/windows.md). Captures and reports are generated locally under `evidence/` and `work/`.

Reports should include reproduction steps, version or commit, resolution, and `lastest.log` when relevant. Use the issue templates and avoid including personal information.
