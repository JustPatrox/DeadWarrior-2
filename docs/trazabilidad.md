# Correspondencias del primer hito

| Original AS3 o símbolo | Traducción | Estado comprobado |
| --- | --- | --- |
| `com/brockw/stickwar/campaign/CampaignMenuScreen.as` | `src/campaign_menu.cpp` | Entrada, panel principal, campaña, sonido y créditos. Composición, animación de paneles y botones conservan colocaciones y tiempos del SWF. Pendientes guardado y recorrido completo. |
| `campaignMenuMc.as`, 1045 | `assets/menu/placements.tsv` y `CampaignMenu::compose` | Se separan escena, logotipo, botones y superposiciones. |
| `menuAssets_fla/Background_157.as`, 979 | `assets/menu/979-*.png`, `alpha.tsv` | Frames 1–97 conservan las colocaciones de 975 y las seis piezas de equipo detenidas en el primer frame. Salida de 975 en 98, aparición de 978 en 124, parada en 174. |
| 720, 976, 977, 978 | `art/menu/`, `assets/menu/976-wide.png` | La base negra de entrada cubre el área completa. El centro de la ilustración se conserva y los laterales se componen aparte. |
| `Title_184`, `Title_185`, 982 / 983 | 1044 / 699, `alpha.tsv` | Opacidades recuperadas de las dos líneas de tiempo originales. |
| 1006, 1011, 1024, 1028 | `assets/menu/placements.tsv` | Paneles. Las posiciones de clic se convierten con la inversa del viewport. SimpleButton usa su estado de hit; los MovieClips de campaña usan límites como `hitTestPoint(x,y)`. |
| 77, 193 | `assets/menu/*.mp3` y `src/windows.cpp` | Audio extraído. La reproducción auditiva y las transiciones entre pistas siguen pendientes. |
| 29 / 10208, `CampaignConstants` / `GameConstants` | `work/extract/binaryData/` | XML original. Campaña 1.24, nivel 1 Tutorial en Swamp Lands. |
| `CampaignTutorial`, `InGameMessage`, 10139 / 10138 / 1924 | `src/campaign_tutorial.cpp`, `src/tutorial_defence.cpp`, `assets/tutorial/messages.tsv` | Entrada desde «Skip Intro» y 22 mensajes. La defensa llega a la instrucción de destruir la estatua en simulación y en ventana a 720p y 1080p. |
| `Team.garrison`, `Team.defend`, `ArcheryBuilding`, `Tech.update` | `src/tutorial_defence.cpp`, `assets/tutorial/control-masks.tsv`, `rules.tsv` | Guarnición y defensa usan las áreas interactivas de 9984 y 9977. El edificio usa 1645. La primera mejora cuesta 300 de oro y arranca la cuenta en 300. |
| `CastleArchers.addUnit`, `Archer.aim`, `ProjectileManager`, `Projectile` | `src/tutorial_defence.cpp`, 4176 / 4168 / 3988 | Cuerpo y arco conservan sus registros y animaciones independientes. Los disparos usan altura y profundidad propias, gravedad y contactos de la flecha. La comparación fotograma por fotograma con Adobe sigue pendiente. |
| `CampaignTutorial`, `Team.updateUnitProduction`, `EnemyTeamAi`, `EnemyGoodTeamAi`, `Statue` | `src/tutorial_battle.cpp`, `assets/tutorial/rules.tsv` | Órdenes globales y destacamentos, producción y cancelación por edificio, economía de los dos bandos, ataque a las estatuas y refuerzo de nivel 1. Las rutas deterministas de victoria y derrota están registradas en `evidence/windows-tutorial/simulation.txt`. Falta cotejar los tiempos de una partida equivalente con Adobe. |
| `UserInterface`, estatua 1922, `PostGameScreen`, 286 / 312 / 335 / 338 / 343 | `src/campaign_result.cpp`, `src/windows.cpp` | La caída usa los 89 cuadros originales con la apariencia de la estatua de cada equipo. Los gráficos proceden de la población y de los trabajadores en la simulación. Se presentan la ficha de Spearton o la pista de derrota, con música y sonidos extraídos del SWF. |
| `SwampLands`, `Map.createMiningBlock`, `Background`, 4098 / 4102 / 1051 / 1078 | `tools/build_campaign_assets.py`, `CampaignTutorial::drawList` | Proyección con 250 unidades de profundidad, 700 de alto y paralaje independiente. Se prolonga el suelo bajo los laterales del HUD. |
| `UnitMove`, `UnitAi`, `Unit`, `Swordwrath`, 4922 | `src/campaign_tutorial.cpp`, `assets/tutorial/animations.tsv`, `markers.tsv` | Formación original de seis filas. Movimiento y contactos de la punta de la espada a 30 Hz. Quedan por cotejar colisiones, sincronizaciones entre unidades y la secuencia aleatoria completa. |
| `MinerAi`, `Miner`, `Statue`, 3104 / 1904 | `CampaignTutorial::workMiner`, `rules.tsv` | El primer trabajador reserva una posición de rezo o extracción. El maná y el oro se producen en los fotogramas de fin de animación. Verificación funcional en simulación hasta entrenar el Swordwrath. |
| `TeamGood`, `GoodHud`, `SelectedUnits`, `ActionInterface` | 10118 / 9726 / 1244 / 1447 y bitmaps de órdenes | Se muestran castillos, estatua, recursos, minimapa, retrato y órdenes. Acciones de la interfaz y otras composiciones aún en revisión. |

El índice completo es `evidence/inventory.json`. Las claves de `timelines` identifican al propietario, y cada cambio conserva frame, profundidad, registro y máscara cuando corresponda. Las entradas no cubiertas por las comprobaciones anteriores siguen pendientes.

### Entre las dos primeras misiones

| Original | Traducción y datos | Verificación |
| --- | --- | --- |
| `CampaignUpgradeScreen`, `Campaign.initUpgradeTree`, `GoodTech`, símbolo 694 | `src/campaign_intermission.cpp`, `assets/intermission/upgrades.tsv`. Las 20 tecnologías conservan sus prerrequisitos, estado y texto de ayuda. El fondo 507 se amplía por separado. | En la primera victoria se puede comprar Rage una vez, volver desde la confirmación o guardar el punto. Se compararon árbol, color del texto y tarjetas con Adobe. |
| `confirmTech` 693, campo 684 | `tools/build_intermission_assets.py` recupera los dos párrafos y usa las métricas de la fuente incrustada 672. El panel y el oscurecimiento del fondo se componen por separado. | FFDec superpone las letras del campo. La lectura interna del proyector no muestra esas dos líneas. El port presenta el texto del SWF legible. |
| `CampaignScreen`, 499 / 436 / 393 / 478 / 498 | `map-motion.tsv` contiene las dos transformaciones de cada uno de los 50 cuadros. Se mantienen las marcas del recorrido y los 15 cuadros del indicador de destino. La roca bajo los botones y el océano continúan hasta los bordes. | Se llega al rótulo original de los Archidons. El indicador, el panel, el guardado inmediato y el regreso al mapa se comprobaron en la ventana. La bandera abre CampaignArcher. |
| `CampaignGameScreen.endGame`, `Level.updateTime`, `Campaign.save/load` | `src/campaign_progress.cpp`, `src/windows_save.cpp`. Cada nivel registra los intentos, el tiempo acumulado y el mejor tiempo, también cuando se pierde. Se persisten dificultad, nivel, mejoras y puntos. | Dos recorridos cerraron la aplicación y reabrieron el mapa desde Continue, uno con el punto y otro con Rage. El archivo nativo se sustituye al terminar su escritura. |

### Archidons y Shadowrath

| Original | Implementación nativa | Evidencia |
| --- | --- | --- |
| CampaignArcher, Forest y Hill | Bosque 4119/4117/4114/4124, torre 1162 y llama 1158. El registro de cada capa se conserva en assets/archidon. La presencia alrededor del centro cambia el control y la renta de oro. | Despliegue de Adobe y capturas de la ventana en evidence/adobe-archidon y evidence/windows-archidon. |
| Archer, ArcherAi y Bow | Unidad 4236. Cuerpo y arco 4168 se componen por separado con la posición de cada fotograma en bows.tsv. Las flechas avanzan en profundidad y altura con la gravedad del XML. | archidon_probe ejercita reclutamiento, cola mixta, devolución, torre, disparos, resultado y viaje al cuadro 92. |
| CampaignShadow, Mountains, Ninja, NinjaAi, EnemyGoodTeamAi.updateNinjas | Capas 4044/4048/4053. El actor 7289 usa 7060, 7071, 7134, 7181, 7278 y 7274. El contacto usa las puntas del arma por cuadro. Los datos XML controlan camuflaje, veneno y acumulación de daño. | Entrada en Adobe, prueba de simulación con un aliado envenenado y victoria. El viaje siguiente termina en el cuadro 151. |
| CampaignScreen y símbolo 499 | La bandera 393 acompaña las transformaciones originales. El encuadre 16:9 de cada cuadro determina la extensión de océano y el borde superior. | El control de cobertura detectó una franja de tres píxeles al llegar al cuadro 92 a 720p. Se amplió la placa de ese tramo según la transformación real. |
### Magia y ejército unido

| Original | Traducción | Validación |
| --- | --- | --- |
| Magikill, Nuke, ElectricWall, PoisonSpray | campaign_magic.cpp y assets/magic. Se conservan los costes, las recargas, los cuadros de lanzamiento y los recursos 3172, 3120 y 3968. La explosión utiliza el radio de Chaos.Units.bomber como Nuke.as. | magic_probe ejercita tres tipos de lanzamiento, curación, defensa del castillo, victoria y derrota. Pendientes colisiones completas con edificios y todos los eventos de sonido. |
| Monk, MonkAi | Poses 8067, 8104, 8183 y 8229, curación en el cuadro 25. El templo mantiene una cola propia. | Curaciones durante defensa prolongada y reclutamiento con devolución del coste original. |
| CampaignGameScreen.endGame, PostGameScreen.showNextUnitUnlocked | Dos tarjetas sucesivas para Magikill y Meric tras ganar la cuarta misión. Rebels United muestra directamente el resultado al no desbloquear nuevas unidades. | Capturas de ambas tarjetas, resultados y mapas en evidence/windows-magic y evidence/windows-rebels. |
| GrassHills y nivel XML 5 | Capas 4058, 4056, 4062 y 4067. La extensión bajo el HUD comienza en la última fila de suelo opaco. Rutas originales del mapa hasta el cuadro 298. | Despliegue y paisaje cotejados con Adobe. Victoria y guardado a 720p/1080p. |

### Interacción y mezcla de audio

| Original | Implementación | Comprobación |
| --- | --- | --- |
| MoveCommand.drawCursorPreClick, AttackMoveCommand, Cursor | campaign_interaction.cpp usa Cursor 2310, orderAttackMoveMc 2313, prayCursor 8821 y pickaxeAnimation 8824. La consulta de objetivos se comparte con las órdenes. Se conserva el tinte rojo del cursor de ataque. | Los probes verifican los tres contextos, la cancelación de ataque en movimiento, la guarnición de la selección y el marcador de una orden del minimapa. |
| NukeCommand, PoisonDartCommand, StunCommand | nukeCursor 8785 y electricWallCursor 8800, con las dimensiones indicadas por AS3. | Explosión y muro comprobados en rebels_probe. |
| SoundLoader, SoundManager y eventos de Unit/Team | build_interaction_assets.py exporta las muestras a PCM y guarda los modificadores de volumen. WindowsAudio mantiene música, narración y efectos en voces independientes de XAudio2. | audio_probe verifica mezcla simultánea y mide el pico de la sesión del proceso antes y después de silenciarla. |
| FogOfWar | La textura 1595 conserva el centro y prolonga el extremo opaco 400 unidades para la cámara 16:9. | Revisión de las capturas del tutorial y comprobaciones de cobertura. |

| Original de interacción | Código y recursos | Evidencia |
| --- | --- | --- |
| ActionInterface, MoveCommand, AttackMoveCommand | campaign_interaction.cpp comparte las áreas interactivas con las órdenes y consume los cursores 2310, 2313, 8821, 8824, 8785 y 8800. windows.cpp restaura el cursor de Windows fuera del contexto de juego y al perder foco. | Capturas de ataque, oración, minería y movimiento a 720p y 1080p. |
| SoundLoader, SoundManager, eventos de Unit y Team | build_interaction_assets.py conserva los nombres y modificadores de las 89 muestras recuperadas. windows_audio.cpp mezcla PCM por XAudio2 y separa el canal de música de las voces y los efectos. | audio_probe midió audio en la sesión del propio proceso y silencio con el mezclador desactivado. |
| FogOfWar.setTint y _fog 1595 | Se conserva el multiplicador RGB 0,1 y la transición de alfa. El extremo opaco continúa otros 400 píxeles lógicos para cubrir el margen 16:9. | Recorrido de ventana sin magenta y capturas en evidence/windows-tutorial. |

### Massive Battle y correcciones de presentación

| Original / solicitud | Traducción | Evidencia |
| --- | --- | --- |
| CampaignCutScene1 | campaign_giants.cpp conserva estatua enemiga 180, economía enemiga cero, muerte del primer Giant, fundidos de 61 cuadros, Medusa, llegada de cinco rebeldes y siete voces. La última voz provoca la victoria. | giants_probe y evidence/adobe-giants/prepared-death. La referencia Adobe prepara la muerte del Giant; no acredita paridad de la batalla natural. |
| Giant, MinerChaos, CastleDeads / Dead | Golpe frontal de hasta tres blancos, aturdimiento, mineros Chaos, defensa elevada con proyectil de veneno, castillo Chaos y terreno Desert. | Despliegue, golpe y veneno en giants_probe; capturas en evidence/windows-giants. |
| EnslavedGiant.setItem / giantbag 1950 | El compilador mantiene Default en lugar de reproducir la línea de apariencias Default/Wood/Bones/Stone. Se conservan las transformaciones y los 319 cuadros de caminar, reposo y muerte. | basket-checks.json registra cero píxeles blancos opacos en todas esas poses. |
| SoundLoader / SoundManager | Los 187 registros originales se convierten a PCM sin rehacer la extracción. Se conectan UnitMake, UnitMakeFail, MouseoverStructure y captura de torre. Las músicas de resultado se repiten como playSoundInBackground. | audio_probe decodifica y envía las 187 muestras, mide salida y silencio, y comprueba la música de victoria después de 23 segundos. audio-assets.json registra duración y pico de cada recurso. |
| Límite de vista solicitado | La cámara queda dentro de -Viewport::left y map.length-Viewport::right; el mundo simulado mantiene sus dimensiones originales. La esquina transparente del botón de velocidad ya no intercepta el minimapa. | Prueba de entrada de giants_probe y capturas castle-left / castle-right. Este ajuste sustituye el anterior límite x=3 solicitado por la comparación del original. |
| Fin provisional y registro solicitados | work_in_progress.cpp usa fondo 976 y estados 1002 con textos propios. windows_log.cpp mantiene un único lastest.log. Reiniciar permite conservar o borrar solo campaign.sav. | evidence/windows-unfinished conserva capturas, logs, pruebas de los tres botones y rechazo de clics detrás de Insert. |

La corrección general de apariencias se aplica en ArtCompiler mediante loadout_assets.py. Se identifican únicamente los receptores de gotoAndStop(weapon/armor/misc) de las clases de unidades y se selecciona Default; CastleDead conserva Wrapped Helmet. Los cuerpos y las acciones siguen animados. refresh_actor_loadouts.py regeneró 4726 cuadros de 61 poses, manteniendo los registros y la composición separada del arco. Esto corrige también los cambios de equipo visibles en Shadowrath, Magikill y Meric.

La música de menú se comprobó aislada en la sesión real del ejecutable: pico 0,222104 con gain 0,672. El valor procede de 0,2 por volumeMap(loginMusic), tal como SoundManager.playSoundInBackground. La prueba anterior de mezcla no aislaba la contribución de la música.
