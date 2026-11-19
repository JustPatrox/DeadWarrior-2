# Building from your own copy of the game

The current target is Windows x64. The repository distributes source, tools, and branding; game resources are generated on your computer.

## Dependencies

| Tool | Tested version / requirement | Source |
| --- | --- | --- |
| Visual Studio | 2022 or 2026, desktop C++ x64 and CMake tools | [Microsoft](https://visualstudio.microsoft.com/downloads/) |
| CMake | 3.24 or later; the script uses Visual Studio's bundled version | [CMake](https://cmake.org/download/) |
| Python | 3.11 x64 | [Python](https://www.python.org/downloads/windows/) |
| Java | 21 x64, JRE or JDK; tested with Temurin 21.0.13+9 | [Eclipse Temurin](https://adoptium.net/temurin/releases/?version=21) |
| JPEXS FFDec | Complete 26.5.0 ZIP, including its `lib` directory | [JPEXS](https://github.com/jindrapetrik/jpexs-decompiler/releases/tag/version26.5.0) |

Flash Player and emulation software are not required to run the port. Optional comparison tools use reference players and Playwright; these are not game dependencies.

## Prepare resources

```powershell
git clone https://github.com/JustPatrox/DeadWarrior-2.git
cd DeadWarrior-2
python -m venv work/venv
work/venv/Scripts/python.exe -m pip install -r requirements-assets.txt
```

Obtain Java and FFDec from their official sources and extract their complete ZIPs. They can live anywhere; for example, `work/tools/java-21` and `work/tools/ffdec`.

Supply your copy of `stickwar2.swf`. The compatible revision is **18,886,237 bytes**, with SHA-256:

```text
03aa17bc25851fb14aefbe9a7223c738f427bff22580e16ec93e5e1232b167b5
```

The preparer rejects other revisions because symbol IDs and rules were translated from this version. It does not download the game or modify the original file.

```powershell
work/venv/Scripts/python.exe tools/prepare_assets.py `
  --swf "C:\path\to\stickwar2.swf" `
  --java-home "C:\path\to\java-21" `
  --ffdec-home "C:\path\to\ffdec" `
  --check
```

Remove `--check` to extract and compile resources. Preparation needs several GB of memory and space for thousands of files. Each stage writes `work/assets-NN.log`; extraction writes `work/extract.log`.

Use `--reuse-extraction` to reuse `work/extract` when its matching SWF inventory already exists. Native resources are still regenerated. Individual module compilers remain available for targeted updates; see [windows.md](windows.md).

## Build the executable

```powershell
.\tools\build_windows.ps1
& '.\out\windows-x64\DeadWarrior 2.exe'
```

The output contains the executable and `assets/`. Keep both together when moving your local installation. Saves are stored in `%LOCALAPPDATA%\DeadWarrior 2\campaign.sav`.

To check simulations and audio:

```powershell
.\tools\build_windows.ps1 -CheckTutorial -CheckArchidon -CheckMagic -CheckRebels -CheckGiants -CheckAudio
```

## Checks without game resources

The source build does not require the SWF. In a terminal with CMake and Visual Studio:

```powershell
cmake -S . -B build/windows -G "Visual Studio 17 2022" -A x64
cmake --build build/windows --config Release --parallel
python -m compileall -q tools
python tools/check_repository.py
```

The resulting executable still needs prepared resources to play. GitHub Actions compiles the game and diagnostic probes without running gameplay or publishing original game content. Reports and captures mentioned in development documentation are local files excluded from the public repository.
