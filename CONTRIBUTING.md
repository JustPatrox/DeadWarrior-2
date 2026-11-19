# Contribuir a DeadWarrior 2

Gracias por ayudar a probar o desarrollar el port. La prioridad actual es Windows x64 y la correspondencia con las reglas del original.

## Preparar el entorno

Sigue [la guía de compilación](docs/building.md). Cada colaborador aporta su propia copia compatible del SWF. No subas el original, scripts decompilados, recursos generados, ejecutables, guardados, logs ni capturas de pruebas al árbol de código.

## Proponer un cambio

1. Abre una rama a partir de `main`.
2. Mantén el cambio centrado en un problema o comportamiento concreto.
3. Para cambios jugables, indica las clases o reglas originales usadas como referencia y las diferencias que siguen pendientes.
4. Compila Windows y ejecuta las comprobaciones relacionadas con el cambio.
5. Abre un pull request con el resultado esperado y las pruebas realizadas.

Conserva la simulación a 30 Hz separada del renderizado, las proporciones de los recursos y el área de presentación 16:9. No declares validado un comportamiento sin probarlo.

## Comprobaciones

```powershell
python -m compileall -q tools
python tools/check_repository.py
.\tools\build_windows.ps1 -CheckTutorial -CheckArchidon -CheckMagic -CheckRebels -CheckGiants -CheckAudio
```

El último comando necesita recursos preparados. Las pruebas de interfaz se describen en [docs/windows.md](docs/windows.md); las capturas e informes se generan localmente bajo `evidence/` y `work/`.

Los reportes deben incluir pasos reproducibles, versión o commit, resolución y `lastest.log` cuando sea pertinente. Utiliza las plantillas de Issues y no incluyas datos personales.
