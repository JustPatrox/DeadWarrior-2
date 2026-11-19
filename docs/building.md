# Compilar desde tu propia copia del juego

La fase actual es Windows x64. El repositorio distribuye código, herramientas y branding; los recursos del juego se generan en tu equipo.

## Dependencias

| Herramienta | Versión usada / requisito | Fuente |
| --- | --- | --- |
| Visual Studio | 2022 o 2026, escritorio C++ x64 y herramientas CMake | [Microsoft](https://visualstudio.microsoft.com/downloads/) |
| CMake | 3.24 o superior; el script usa el incluido con Visual Studio | [CMake](https://cmake.org/download/) |
| Python | 3.11 x64 | [Python](https://www.python.org/downloads/windows/) |
| Java | 21 x64, JRE o JDK; probado con Temurin 21.0.13+9 | [Eclipse Temurin](https://adoptium.net/temurin/releases/?version=21) |
| JPEXS FFDec | ZIP completo de 26.5.0, con su carpeta `lib` | [JPEXS](https://github.com/jindrapetrik/jpexs-decompiler/releases/tag/version26.5.0) |

No hace falta instalar Flash Player ni un emulador para ejecutar el port. Algunas herramientas opcionales de comparación con el original utilizan reproductores de referencia y Playwright, que no son dependencias del juego.

## Preparar los recursos

```powershell
git clone https://github.com/JustPatrox/DeadWarrior-2.git
cd DeadWarrior-2
python -m venv work/venv
work/venv/Scripts/python.exe -m pip install -r requirements-assets.txt
```

Obtén Java y FFDec desde sus fuentes oficiales y extrae sus ZIP completos. Puedes colocarlos en cualquier ruta; por ejemplo, `work/tools/java-21` y `work/tools/ffdec`.

Aporta tu copia de `stickwar2.swf`. La revisión compatible tiene **18 886 237 bytes** y SHA-256:

```text
03aa17bc25851fb14aefbe9a7223c738f427bff22580e16ec93e5e1232b167b5
```

El preparador rechaza otras revisiones porque los símbolos y las reglas se han traducido desde esta. No descarga el juego y no modifica el archivo original.

```powershell
work/venv/Scripts/python.exe tools/prepare_assets.py `
  --swf "C:\ruta\a\stickwar2.swf" `
  --java-home "C:\ruta\a\java-21" `
  --ffdec-home "C:\ruta\a\ffdec" `
  --check
```

Quita `--check` para extraer y compilar los recursos. El proceso necesita varios GB de memoria y espacio para miles de archivos. Cada etapa escribe `work/assets-NN.log`; la extracción escribe `work/extract.log`.

`--reuse-extraction` permite reutilizar `work/extract` cuando ya existe el inventario correspondiente al mismo SWF. Regenera igualmente los recursos nativos. Los compiladores individuales siguen disponibles para actualizar solo un módulo; consulta [windows.md](windows.md).

## Compilar el ejecutable

```powershell
.\tools\build_windows.ps1
& '.\out\windows-x64\DeadWarrior 2.exe'
```

La salida contiene el ejecutable y `assets/`. Conserva ambos juntos al mover tu instalación local. Las partidas se guardan en `%LOCALAPPDATA%\DeadWarrior 2\campaign.sav`.

Para comprobar las simulaciones y el audio:

```powershell
.\tools\build_windows.ps1 -CheckTutorial -CheckArchidon -CheckMagic -CheckRebels -CheckGiants -CheckAudio
```

## Comprobaciones sin los recursos del juego

La compilación de código no requiere el SWF. En una terminal con CMake y Visual Studio:

```powershell
cmake -S . -B build/windows -G "Visual Studio 17 2022" -A x64
cmake --build build/windows --config Release --parallel
python -m compileall -q tools
python tools/check_repository.py
```

Ese ejecutable todavía necesita la preparación de recursos para jugar. GitHub Actions compila el juego y los probes, sin ejecutar partidas ni publicar una instalación con recursos originales. Los informes y capturas mencionados en la documentación de desarrollo son archivos locales excluidos de la publicación.
