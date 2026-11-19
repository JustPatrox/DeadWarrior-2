# Windows x64

Desde PowerShell en la raíz del proyecto, con Visual Studio 2022 o 2026, la carga de C++ para escritorio, Windows SDK y CMake instalados:

```powershell
.\tools\build_windows.ps1
& '.\out\windows-x64\DeadWarrior 2.exe'
```

El directorio de entrega es `out/windows-x64/`. El ejecutable busca `assets/` junto a él. La compilación usa el runtime estático de MSVC. El ejecutable carga las texturas y audio preparados y ejecuta los controladores en C++.

**Insert** abre el menú de pantalla. Selecciona **Windowed**, **Borderless Window** o **Fullscreen** con el ratón o con las flechas y Enter. El modo sin bordes conserva visible la barra de tareas y la pantalla completa ocupa el monitor. Insert o Esc cierran el panel. F11 alterna entre modo ventana y pantalla completa. El área de juego siempre conserva 16:9. Si la resolución del monitor tiene otra proporción, las bandas quedan fuera del área de clic.

En el mismo menú, **FPS limit** ofrece 30, 60, 120, 240 y Unlimited. El valor inicial es 60. Con el teclado, baja hasta la fila de FPS, selecciona un valor con ← o → y pulsa Enter. Los FPS afectan a la presentación con Direct2D. El reloj de simulación da 30 pasos por segundo y recupera los pasos pendientes si un fotograma tarda más. GDI+ sirve para capturas privadas y como respaldo ante un fallo de Direct2D.

**V-Sync** se activa desde la fila debajo del límite de FPS, con un clic o Enter. Empieza en Off. Al activarlo, Direct2D presenta sincronizado con el refresco de la pantalla y sigue aplicando el límite de FPS seleccionado. [Opciones de presentación de Direct2D](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/ne-d2d1-d2d1_present_options)

`work/venv/Scripts/python.exe tools/check_display.py --fps-limits --vsync` verifica ambas posiciones del interruptor durante el tutorial. El límite de 30 dio 30,01 FPS con ambas posiciones. Con Unlimited y V-Sync, la presentación dio 31,78 FPS en esta sesión de 60 Hz. La simulación se mantuvo en torno a 30 pasos por segundo.

La prueba en un Intel i5-7400 con RTX 3050 a 1920 × 1080 registró 60,07 FPS con Direct2D y 1,88 FPS con GDI+ durante el tutorial, con desplazamiento de cámara y las texturas iniciales ya cargadas. De los 853 fotogramas medidos con Direct2D, uno superó 25 ms entre presentaciones. `work/venv/Scripts/python.exe tools/check_display.py --compare-software` reproduce la comparación y guarda los tiempos en `work/windows-display/`. El modificador `--compare-software` es opcional.

Con `work/venv/Scripts/python.exe tools/check_display.py --fps-limits --compare-software` se comprobó el cambio de los cinco valores durante la partida. La simulación se mantuvo entre 29,82 y 30,20 pasos por segundo con Direct2D y dio 30,01 con GDI+ mientras la presentación caía a 1,84 FPS. El informe `work/windows-display/display-checks.json` mide la cadencia de presentación del ejecutable. La frecuencia visible depende del monitor.

El icono de DeadWarrior 2 proviene de `icon.png`. Windows usa las diez resoluciones de `src/resources/deadwarrior2.ico`. El aviso de inicio dura tres segundos y precede al menú y a la música. Se usó el logo de Max Games proporcionado en `art/branding/max-games-source.png`. El recurso de `assets/branding/max-games.png` lleva fondo transparente, botones de mando blancos y un contorno blanco fino.

## Preparación de la fuente

La fuente de este proyecto es el SWF de 18 886 237 bytes cuyo hash figura en `AGENTS.md`. El ejecutable no requiere ese archivo. Para inspeccionar la traducción, colocar una copia en la raíz. Los recursos compilados se conservan en Git. No hace falta volver a extraerlos para compilar.

Para regenerar la extracción se usaron el ZIP oficial FFDec 26.5.0 en `work/tools/ffdec` (SHA-256 `8d00c2713a724bb3f7e6c9a64994836ce3a953b5ac264f405d98f92e7c266d33`) y Temurin JRE 21.0.13+9 x64 en `work/tools/jdk-21.0.13+9-jre`. Python 3.11 con Pillow 13.0, numpy 2.4.6 y JPype1 1.7.1 prepara las capas.

```powershell
python -m venv work/venv
work/venv/Scripts/python.exe -m pip install pillow==13.0.0 numpy==2.4.6 JPype1==1.7.1
python tools/inventory.py stickwar2.swf evidence/inventory.json
& 'work/tools/jdk-21.0.13+9-jre/bin/java.exe' -Xmx5g '-Djava.awt.headless=true' -jar work/tools/ffdec/ffdec.jar -onerror ignore -export script,image,font,sound,binaryData,symbolClass,shape,morphshape work/extract stickwar2.swf *> work/extract.log
work/venv/Scripts/python.exe tools/build_menu_assets.py
```

En `art/menu` se conservan los dos laterales de paisaje generados para la ilustración aplanada 976. El compilador de arte ajusta el color en las uniones y conserva intactos los 850 × 700 píxeles del JPEG decodificado. Las partes superiores del título, los botones y los combatientes de la entrada provienen del SWF.

## Pruebas

```powershell
work/venv/Scripts/python.exe tools/check_menu.py
work/venv/Scripts/python.exe tools/check_window.py
```

`check_menu.py` ejecuta el mismo renderizador que la ventana con una limpieza magenta y analiza cada píxel de las capturas. Compara la pintura y los logos en el encuadre original con capturas del SWF. `check_window.py` abre una ventana, pulsa los controles y comprueba el resultado visible con el tamaño físico del cliente. La máquina de referencia ejecutó Windows x64 a 96 DPI. Las capturas y métricas quedan en `evidence/windows-menu/`.

Las referencias de `evidence/reference/` proceden de ejecutar localmente el SWF en Ruffle web 0.7.1, con el renderizador Canvas y la red externa del juego deshabilitada. Para repetirlas, extraer el ZIP web selfhosted de esa versión en `work/tools/reference-web/` y ejecutar:

```powershell
work/venv/Scripts/python.exe -m pip install playwright==1.65.2
$env:PLAYWRIGHT_BROWSERS_PATH = (Join-Path (Get-Location) 'work/tools/browser')
work/venv/Scripts/python.exe -m playwright install chromium --only-shell
work/venv/Scripts/python.exe tools/reference_web.py 1280 720 4 8 18 30
work/venv/Scripts/python.exe tools/reference_web.py 1920 1080 4 8 18
work/venv/Scripts/python.exe tools/reference_web.py 1280 720 20 --tutorial
```

Los tiempos en los nombres de las referencias se miden desde la carga. Las muestras de animación por tiempo real pueden corresponder a fotogramas diferentes entre ejecuciones. Los ticks de la captura C++ son deterministas.

## Primera misión

En una campaña nueva, elige Normal y pulsa «Skip Intro». La prueba recorre la selección, la cámara, el primer combate, el minero, el entrenamiento y la defensa del castillo. Envía clics a la ventana y registra el estado del juego en la misma ejecución. A 1280 × 720 y 1920 × 1080 los cuatro aliados obedecieron la orden de guarnición, la investigación produjo al arquero y seis flechas alcanzaron al Spearton. Las capturas, trazas e informes están en `evidence/windows-tutorial/`.

```powershell
.\tools\build_windows.ps1 -CheckTutorial
work/venv/Scripts/python.exe tools/check_tutorial.py 1280 720 --defence
work/venv/Scripts/python.exe tools/check_tutorial.py 1920 1080 --defence
```

La prueba de simulación comprueba el coste de 300 de oro, la duración de la investigación, la protección ante una compra duplicada y los clics fuera del contorno del edificio. La referencia de entrada de Adobe Flash Player standalone está en `evidence/adobe/`. Queda pendiente la comparación visual de los combates posteriores con Adobe.

### Batalla libre y resultados

Tras defender el castillo, los botones del HUD permiten reclutar mineros y Swordwrath y ordenar el ataque. Una invasión de refuerzo se activa cuando la estatua enemiga baja del 75 % de vida. Destruir una estatua abre el resultado después de los 89 fotogramas de su caída. Al ganar se muestra la ficha de Spearton. Al perder se muestra un consejo y «Retry» vuelve a iniciar la misión con la dificultad elegida.

```powershell
work/venv/Scripts/python.exe tools/check_tutorial.py 1280 720 --battle
work/venv/Scripts/python.exe tools/check_tutorial.py 1920 1080 --battle
work/venv/Scripts/python.exe tools/check_tutorial.py 1280 720 --hard-loss
work/venv/Scripts/python.exe tools/check_tutorial.py 1920 1080 --hard-loss
```

La simulación verificó la contratación simultánea en dos edificios y la devolución íntegra de la última orden de producción. La prueba en ventana controló el reclutamiento, la llegada del refuerzo y la victoria por daño a la estatua. En la partida difícil, el salto de tutorial, la guarnición y el avance rápido llevaron a una derrota por daño a la estatua propia. Las dos rutas pasaron a 720p y 1080p. A 720p se comprobó además el reinicio con «Retry» y el salto del tutorial en la nueva partida. Capturas y filas de estado en `evidence/windows-tutorial/`.

### Mejoras, mapa y guardado

Después de ganar el tutorial, cierra la ficha de Spearton y pulsa «Continue». Puedes comprar «Rage» o conservar el punto al pulsar «Next Battle». El mapa avanza hasta el territorio de los Archidons. Pulsa «Autosave Disabled» en el mapa para activar el guardado y escribir la partida. Un nuevo arranque muestra «Continue» en el menú de campaña. El destino disponible en esta versión es el mapa de la segunda misión.

El archivo de campaña está en `%LOCALAPPDATA%\DeadWarrior 2\campaign.sav`. Cada prueba usa un directorio temporal mediante `--save-dir`. Se comprobó el cierre del proceso y la lectura desde uno nuevo con los dos resultados de la compra.

```powershell
work/venv/Scripts/python.exe tools/build_intermission_assets.py
.\tools\build_windows.ps1 -CheckTutorial
work/venv/Scripts/python.exe tools/check_tutorial.py 1280 720 --intermission
work/venv/Scripts/python.exe tools/check_tutorial.py 1920 1080 --intermission --keep-point
work/venv/Scripts/python.exe tools/reference_intermission.py 1280 720
work/venv/Scripts/python.exe tools/reference_intermission.py 1920 1080
work/venv/Scripts/python.exe tools/compare_intermission.py
```

Para comparar estas pantallas de forma repetible, el observador de prueba prepara el estado de campaña tras la victoria en una copia en memoria. Adobe ejecuta los controladores originales de mejoras y mapa y recibe los clics de la prueba. El observador devuelve los píxeles del documento al proceso local. Las referencias y el informe numérico están en `evidence/adobe-intermission/`. El recorrido del port sí comienza en el menú, juega el tutorial y gana la misión. Quedan por comprobar otros controles de batalla y el recorrido completo posterior.

### Referencia jugable en Adobe

```powershell
work/venv/Scripts/python.exe tools/reference_battle.py 1280 720
work/venv/Scripts/python.exe tools/reference_battle.py 1920 1080
```

El proyector recibe clics por su ventana desde el menú normal. El observador recoge los píxeles y verifica en el controlador original el texto mostrado, la selección, el fotograma de la simulación y la cámara. Se usa un socket local previamente conectado porque al entrar en combate el escenario pasa a 0,01 fps y dejan de llegar las respuestas HTTP de captura. El filtro de salida del puntero se limita al escenario de la copia diagnóstica. Las capturas hasta el primer minero se guardan en `evidence/adobe-tutorial/`. La referencia de arranque del SWF intacto continúa en `evidence/adobe/`.

### Segunda misión

La bandera de los Archidons abre el bosque. El jugador empieza con tres mineros, dos Swordwraths y estatuas de 1000/800 de vida. En normal recibe 700 de oro y 200 de maná. El botón del Spearton está en la segunda fila de entrenamiento y comparte la cola del cuartel con Swordwrath. La cruz de cada tipo devuelve su última orden y conserva el progreso de la cabeza de la cola. Los Archidons se producen en el campo de tiro enemigo y disparan flechas con trayectoria, profundidad y recarga independientes.

La torre central se captura al mantener unidades a su alrededor y proporciona el ingreso del XML. Si compraste Rage, selecciona Swordwraths y pulsa Q o su icono de habilidad. La victoria abre la ficha de Archidon, las mejoras y el recorrido del mapa desde el cuadro 50 hasta el 92. La bandera del cuadro 92 abre la batalla de los Shadowrath.

`work/venv/Scripts/python.exe tools/build_archidon_assets.py` regenera el arte del bosque, arqueros y resultados. `work/venv/Scripts/python.exe tools/build_archidon_map.py` prepara el siguiente recorrido. Para compilar y comprobar la simulación, ejecuta `.\tools\build_windows.ps1 -CheckTutorial -CheckArchidon`. La prueba de ventana es `work/venv/Scripts/python.exe tools/check_archidon.py 1280 720`, con `1920 1080` para la otra resolución. Usa una partida temporal preparada al final del tutorial.

El modo `--archidon` de `tools/reference_battle.py` prepara ese mismo nivel en el controlador original de Adobe. Las referencias están en `evidence/adobe-archidon/`. La escena estática del bosque dentro del encuadre original dio una diferencia RGB media de 2,074 sobre 255 en la primera comparación a 720p. La paridad completa del combate, los sonidos por evento y las demás tecnologías continúan en revisión.

La captura diagnóstica `--capture-client ruta.png` permite que las pruebas soliciten la composición actual al ejecutable mediante WM_APP+15. En esta sesión PrintWindow devolvía el aviso de arranque tras entrar en combate. Las pruebas de campaña usan la composición actual y las mediciones de FPS siguen registrando las presentaciones de Direct2D.

### Tercera misión

Silent Assassins abre Mountains con tres mineros, un Archidon y un Swordwrath aliados. Ambas estatuas tienen 1200 de vida. La cola del campo de tiro permite reclutar Archidons mientras el cuartel entrena soldados. La IA de Shadowrath usa el cuartel para los Swordwraths y los ninjas. Un ninja que se acerca puede activar el camuflaje, envenenar al salir de él y acumular daño en golpes consecutivos contra el mismo blanco. La guarnición cura el veneno. La victoria desbloquea al Shadowrath y lleva la bandera desde el cuadro 92 hasta el 151. La cuarta batalla, Magic in the Air, está pendiente.

Para rehacer los recursos desde el SWF local se usan estos comandos desde la raíz.

```powershell
work/venv/Scripts/python.exe tools/build_shadow_assets.py
work/venv/Scripts/python.exe tools/build_archidon_map.py --shadow
.\tools\build_windows.ps1 -CheckTutorial -CheckArchidon
```

La prueba de simulación incluye la tercera batalla. Comprueba que un ninja enemigo envenena a un aliado, el jugador recluta un Archidon, la batalla llega a un resultado y el mapa avanza tras la victoria. Para la prueba de ventana se utiliza una partida temporal al inicio del tercer territorio.

```powershell
work/venv/Scripts/python.exe tools/check_archidon.py 1280 720 --shadow
work/venv/Scripts/python.exe tools/check_archidon.py 1920 1080 --shadow
work/venv/Scripts/python.exe tools/reference_battle.py 1280 720 --shadow
work/venv/Scripts/python.exe tools/reference_battle.py 1920 1080 --shadow
```

La referencia original de Adobe confirmó 9 de población, 700 de oro y 200 de maná en normal. En el rectángulo lógico de paisaje x=250…820 e y=35…230 de la entrada, el error RGB medio respecto al original fue 0,71 sobre 255 a 720p y 0,80 a 1080p, alineando el encuadre original con el centro 16:9. La comparación completa del combate sigue pendiente.
### Magic in the Air y Rebels United

El ejecutable entra en la cuarta y quinta batalla desde sus banderas. Magic in the Air usa SwampLands, estatuas de 700/1200 y un arquero en el castillo enemigo. Los Meric curan a sus aliados durante la animación. Los Magikill lanzan explosión, muro eléctrico y veneno en los cuadros 36, 47 y 44 de sus poses. La prueba prolongada de defensa registra los tres hechizos, curaciones y derrota. Un recorrido de ataque independiente comprueba victoria, las dos fichas de desbloqueo y la llegada al cuadro 238.

Rebels United usa GrassHills y las seis unidades de Order. El jugador comienza con cuatro mineros, Swordwrath, Spearton, Shadowrath, Magikill y Meric, con 24 de población y estatuas de 1500/1500. El templo y el gremio mágico mantienen colas independientes del cuartel y el campo de tiro. El Magikill seleccionado prepara la explosión con Q. W prepara veneno con la mejora Poison Spray y E prepara el muro con Electric Wall. Un clic dentro de alcance elige la posición. El botón derecho cancela la selección de objetivo. El Meric permite alternar su curación con Q y su cura con W si se compró Cure. Sigue pendiente reproducir la orden original de caminar automáticamente hasta el alcance de un lanzamiento solicitado fuera de rango.

La victoria de Rebels United abre directamente las estadísticas y viaja al cuadro 298 de Massive Battle. La sexta batalla y su cinemática siguen pendientes. Las pruebas de ventana de las misiones cuatro y cinco parten de partidas temporales preparadas en esos territorios. Se verificaron victoria y escritura del guardado a 1280×720 y 1920×1080. Evidencias en evidence/windows-magic y evidence/windows-rebels. Las referencias del controlador original están en evidence/adobe-magic y evidence/adobe-rebels. El paisaje de la entrada, dentro del encuadre original, dio un error RGB medio de 1,10/1,38 sobre 255 para SwampLands y 1,54/1,79 para GrassHills a 720p/1080p. La paridad completa de colisiones, duelos y asedios continúa abierta.

La inspección visual detectó que el terreno de GrassHills era transparente bajo y=615 aunque su rectángulo llegaba hasta 700. El compilador continúa el suelo desde la última fila opaca. La prueba de ventana comprueba además que la parte inferior del lateral derecho tenga suelo. Los probes comparten campaign_core con el ejecutable.

```powershell
work/venv/Scripts/python.exe tools/build_magic_assets.py
work/venv/Scripts/python.exe tools/build_archidon_map.py --magic
work/venv/Scripts/python.exe tools/build_rebels_assets.py
work/venv/Scripts/python.exe tools/build_archidon_map.py --level 6
.\tools\build_windows.ps1 -CheckTutorial -CheckArchidon -CheckMagic -CheckRebels
work/venv/Scripts/python.exe tools/check_archidon.py 1280 720 --magic
work/venv/Scripts/python.exe tools/check_archidon.py 1920 1080 --magic
work/venv/Scripts/python.exe tools/check_archidon.py 1280 720 --rebels
work/venv/Scripts/python.exe tools/check_archidon.py 1920 1080 --rebels
```

### Sonido y cursores

Los iconos originales del ratón se dibujan desde assets/interaction. Al pasar sobre un enemigo aparece la flecha de ataque. Un minero seleccionado muestra la piqueta sobre el oro y el cursor de maná sobre la estatua. Las órdenes muestran la animación Cursor de 12 cuadros. El botón de ataque en movimiento permite elegir un destino, también en el minimapa, y el botón derecho cancela ese modo. Los controles de mantener posición, detener y entrar o salir de la guarnición actúan sobre la selección. Los cursores de explosión, veneno y muro usan los recursos originales y sus dimensiones de alcance.

Windows oculta el cursor del sistema mientras dibuja un cursor contextual. Las bandas, el HUD, el panel Insert y las ventanas ajenas conservan su cursor normal. Perder el foco o la captura del ratón cancela el arrastre. El clic derecho en el minimapa envía la selección a la posición indicada.

windows_audio.cpp utiliza XAudio2 para música, narración y efectos simultáneos. Las muestras originales se decodifican una vez al preparar los recursos. La música usa su propio canal y los efectos tienen posiciones estéreo. El botón de silencio controla toda la mezcla. Las voces conservan las duraciones que usa el controlador del tutorial. Se añadieron los sonidos de órdenes, impactos, flechas, bajas, entrenamiento, habilidades y desbloqueos. Las acciones de mecánicas todavía pendientes de portear conservan esa condición.

La textura de niebla se prolonga hasta el borde derecho del formato 16:9 manteniendo su transición original. Las capturas de los cursores están en evidence/windows-tutorial, con sufijos attack-cursor, prayer-cursor, mining-cursor y movement-marker. La comprobación de audio mide la sesión del propio proceso y está en evidence/windows-interaction/audio-checks.json.

```powershell
work/venv/Scripts/python.exe tools/build_interaction_assets.py
.\tools\build_windows.ps1 -CheckTutorial -CheckArchidon -CheckMagic -CheckRebels -CheckAudio
work/venv/Scripts/python.exe tools/check_tutorial.py 1280 720 --intermission
work/venv/Scripts/python.exe tools/check_tutorial.py 1920 1080 --economy
```


### Registro y fin provisional

Cada arranque sobrescribe `lastest.log` junto al ejecutable. Si esa carpeta no permite escribir, se utiliza `%LOCALAPPDATA%\DeadWarrior 2\lastest.log`. No se generan archivos numerados ni se escribe el antiguo `deadwarrior2-error.log`. El registro incluye la build, arranque, directorio y validez del guardado, pantalla, nivel, pasos del controlador, tiempo de simulación, cámara, vida de las estatuas, economía, voces, configuración de pantalla, controles pendientes y errores. Se vacía a disco después de cada entrada para conservar la última información disponible si el proceso termina de forma inesperada.

Los controles visibles todavía pendientes, como Menu, calidad y navegación de mineros del HUD, muestran durante seis segundos el aviso `This button does not work yet, please report it with the log`. El registro identifica el control y el estado de la misión. No se trata la falta de oro, una mejora bloqueada o un área vacía como una función pendiente.

Intentar abrir una misión no traducida muestra la ilustración del menú principal y los tres estados originales del botón 1002. El título comienza con Thanks for playing. Los botones se disponen con dos opciones arriba y Quit centrado debajo. Restart keeping saves escribe el avance actual y regresa al arranque; Restart and delete save elimina únicamente `campaign.sav`; Quit cierra el juego. Insert sigue disponible y bloquea los botones que quedan detrás. También se puede navegar con Tab/flechas y confirmar con Enter/Espacio. Una partida guardada de un territorio aún pendiente entra directamente en esta pantalla, evitando cargar recursos que no existen.

Comprobación reproducible: `work/venv/Scripts/python.exe tools/check_unfinished.py`. Las capturas y los registros de las pruebas están en `evidence/windows-unfinished/`. Para comprobar una instalación alternativa, establecer la variable de proceso `DW2_TEST_EXE` con la ruta absoluta al ejecutable.


### Massive Battle y límite de cámara

La sexta misión incorpora Desert, Giant, trabajadores Chaos, CastleDeads y CampaignCutScene1. El controlador mantiene la estatua enemiga en 180 y su oro en cero. Derrotar al primer gigante inicia el fundido, las tres voces de Medusa y las cuatro del portavoz rebelde. La última voz provoca la victoria y desbloquea Enslaved Giant. El siguiente mapa llega al cuadro 341. La séptima batalla todavía muestra el fin provisional. La comparación completa de IA, colisiones y asedios con Adobe permanece pendiente.

La vista 16:9 se limita al terreno de los castillos: cámara mínima 197,222 y máxima map.length - 1047,222 en las coordenadas lógicas del juego. Esto impide revelar la franja situada detrás de la base. No cambia el tamaño del mapa ni la simulación. El botón de velocidad usa su área circular y deja pasar los clics de la esquina transparente al minimapa.

Los 187 registros de SoundLoader se reutilizan desde la extracción existente. Las músicas de victoria y derrota se repiten. El log identifica recursos de sonido ausentes y fallos del dispositivo XAudio2. Para regenerar únicamente audio: `work/venv/Scripts/python.exe tools/build_interaction_assets.py --audio-only`.

La canasta 1950 del gigante esclavizado conserva Default como EnslavedGiant.setItem. No reproduce las variantes Bones Bag / Stone Bag que provocaban el destello blanco. Para actualizar solo las tres poses de ese actor: `work/venv/Scripts/python.exe tools/build_giants_assets.py --enslaved-only`.

```powershell
.\tools\build_windows.ps1 -CheckTutorial -CheckArchidon -CheckMagic -CheckRebels -CheckGiants -CheckAudio
work/venv/Scripts/python.exe tools/check_giants.py 1280 720
work/venv/Scripts/python.exe tools/check_giants.py 1920 1080
work/venv/Scripts/python.exe tools/check_unfinished.py
work/venv/Scripts/python.exe tools/reference_battle.py 1280 720 --giants
work/venv/Scripts/python.exe tools/reference_battle.py 1920 1080 --giants
```

La referencia --giants prepara únicamente la muerte del gigante para aislar la cinemática. Sus capturas están en evidence/adobe-giants/prepared-death; no equivalen a una victoria natural de la batalla original. El SWF raíz se conserva intacto.

El compilador detiene también las apariencias de ninja, Magikill, Meric y las demás unidades, según los receptores de gotoAndStop(weapon/armor/misc) de sus setItem. Las animaciones corporales conservan todos sus cuadros. Para regenerarlas sin repetir fondos, UI ni audio: `work/venv/Scripts/python.exe tools/refresh_actor_loadouts.py`. La correspondencia de cada símbolo y su apariencia está en evidence/windows-interaction/loadout-checks.json.

El volumen de fondo respeta `SoundManager.targetBackgroundVolume = 0.2 * volumeMap[name]`. La música loginMusic usa 0,672; anteriormente el port imponía 0,2 a todas las pistas. `tools/check_menu_audio.py` mide la sesión del ejecutable con el menú solo, evitando atribuir a la música el pico de una voz o un efecto.

### Contador de FPS

Insert abre la opción Show FPS. Empieza en Off y se activa con el ratón o seleccionándola con flechas/Tab y Enter/Espacio. End selecciona este último interruptor. El contador se dibuja arriba a la izquierda, dentro del área 16:9, y sigue visible al cerrar el panel. Cuenta las presentaciones completadas durante el último segundo; funciona con Direct2D y el respaldo GDI+. Las capturas privadas no se cuentan como fotogramas presentados. El reloj de simulación conserva sus 30 Hz.

`work/venv/Scripts/python.exe tools/check_fps_counter.py` comprueba encendido/apagado, lectura a 30 y 60 FPS, teclado, V-Sync y posición con bandas. Las capturas, tiempos e informes están en evidence/windows-fps-counter/.
