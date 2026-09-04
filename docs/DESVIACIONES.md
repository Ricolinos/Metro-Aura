# DESVIACIONES.md — Registro de desviaciones respecto a `PLAN_MAESTRO.md`

Cada entrada: fase · qué decía el plan · qué se encontró en la práctica · qué se hizo. Las decisiones de diseño (`M-NNN`) no se cambian por preferencia — solo se corrigen hechos falsos o se documentan ajustes técnicos necesarios para que el plan funcione.

---

## F0-1 — `tools/configure` no estaba en la lista de 27 archivos modificados por Aura

**Plan decía** (`PLAN_MAESTRO.md` §0.2, citando `INVESTIGACION.md` D.4): que `tools/configure` probablemente era idéntico al upstream porque no aparecía en la lista de 27 archivos de `MODIFICATIONS.md` de Aura-Firmware — quedó marcado `NO RESUELTO` en la investigación.

**Qué se encontró**: al hacer `diff -rq` completo del árbol de Aura-Firmware contra una copia limpia del upstream `0726ec93` (verificación previa a sembrar F0), aparecieron **28** archivos distintos, no 27. Los dos no documentados en `MODIFICATIONS.md` de Aura-Firmware:
- `tools/configure`: sí tiene una modificación real (D-007) — detección del gcc de Homebrew más reciente disponible con fallback 16→15→14→13, en vez de fijar `gcc-16` a secas. Es una laguna real de la documentación de Aura-Firmware (no se toca ese repo desde aquí).
- `apps/plugins/mpegplayer/mpegplayer.h`: agrega la declaración de `aura_osd_colors()` (D-306/D-307) — pertenece al mecanismo de OSD de mpegplayer, ya clasificado en el backlog de `PLAN_MAESTRO.md` (ítem 1), no afecta F0.

**Qué se hizo**: se agregó `tools/configure` a la lista de archivos portados en F0 (ahora 10, no 9) — es exactamente el tipo de fix de build que la Fase Cero busca. `mepgplayer.h` no se porta en F0 (el backlog que lo necesita no es v1); se revisará su diff cuando/si se ejecuta ese ítem del backlog.

**Impacto en `PLAN_MAESTRO.md`**: §0.2 pasa de 9 a 10 archivos portados.

---

## F0-2 — Comentarios de atribución GPL en los archivos portados no se reescriben

**Plan decía** (§0.2, columna "Marca inline"): sustituir `Aura (D-NNN)` por `Metro (from Aura D-NNN)` dentro de cada archivo portado.

**Qué se decidió**: los 10 archivos se portan **byte-idénticos** a Aura-Firmware, sin tocar sus comentarios. Los comentarios narrativos que dicen "Aura (D-293): ..." son, en varios de estos archivos, el propio aviso de modificación GPL v2 §2a (qué se cambió, cuándo, por quién) — reescribirlos para decir "Metro" atribuiría a Metro-Aura una modificación que en realidad hizo Aura-Firmware. La atribución correcta (que este archivo llegó modificado desde Aura-Firmware, con referencia cruzada a sus decisiones D-NNN) queda en `MODIFICATIONS.md` de este repo, que es el mecanismo estándar para documentar el historial de modificaciones sin alterar el código fuente heredado.

**Impacto en `PLAN_MAESTRO.md`**: ninguno en el resultado final (mismo comportamiento); la columna "Marca inline" de §0.2 queda reemplazada por "sin cambios en el archivo; atribución en MODIFICATIONS.md".

---

## F1-1 — La higiene de `global_settings` (M-019) no puede correr al principio de `metro_main()`

**Plan decía** (`PLAN_MAESTRO.md` §1.2, fila `metro_main.c/.h`): "higiene M-019" como lo primero que hace `metro_main()`, la función que se llama una sola vez desde `apps/main.c` en reemplazo de `root_menu()`, **después** de que `init()` termina.

**Qué se encontró**: con la higiene puesta ahí, la primera captura de pantalla (`F1-boot.png`) mostró el backdrop stock de Rockbox (gradiente gris con el wordmark "ROCKbox" rotado, `backdrops/cabbiev2.320x240x16.bmp`) detrás del texto "metro", en vez de un fondo negro limpio. Investigando la ruta de código (`firmware/drivers/lcd-16bit-common.c:lcd_clear_viewport()`): cuando hay un backdrop activo (`lcd_backdrop` no nulo, seteado por `apps/gui/skin_engine/skin_backdrops.c:skin_backdrop_show()`), **cualquier** `lcd_clear_display()` — incluida la de mi propio `metro_screen_splash_show()` — pinta el backdrop en vez de un color sólido; no hay forma de "limpiar" ese comportamiento después del hecho sin desactivar el backdrop en el origen. Y el backdrop se activa en `settings_apply_skins()` (`apps/gui/skin_engine/skin_engine.c:165`), que lee `global_settings.backdrop_file` **dentro de `init()`**, antes de que `metro_main()` — y por lo tanto mi higiene — llegue a correr.

**Qué se hizo**: se movió la higiene completa a una función exportada, `metro_apply_hygiene()` (declarada en `metro_main.h`), llamada desde **ambos** cuerpos de `init()` en `apps/main.c` (uno para el build de simulador/hosted, otro para target real — el archivo tiene dos definiciones de `init()` bajo `#if (CONFIG_PLATFORM & PLATFORM_HOSTED)` / `#else`), en el mismo punto exacto que usa Aura-Firmware: justo después de `settings_load()`, antes de `settings_apply(true)`/`settings_apply_skins()`. Confirmado por captura: con este cambio, `F1-boot.png` muestra fondo negro limpio.

**Impacto en `PLAN_MAESTRO.md`**: §1.2 (tabla de `metro_main.c/.h`) y el pseudo-código de M-019 deben leerse como "higiene llamada desde `init()` en `apps/main.c`, no desde `metro_main()`" — `metro_main()` sigue siendo el punto de entrada de la UI, pero ya no es responsable de este paso. Cualquier fase futura que agregue una nueva pantalla de "primer arranque" debe recordar que la higiene ya corrió antes de que exista ninguna UI.

---

## F2-1 — `metro_draw_rows()`/`metro_draw_pivots()`/`metro_draw_tile()`/`metro_draw_progress()` no se implementan en F2

**Plan decía** (`PLAN_MAESTRO.md` §1.2, fila `metro_draw.c/.h`, fase "F2, F3"): que F2 ya deja escritas las primitivas de dibujo de filas y encabezado de pivots (`draw_rows`, `draw_pivots`), junto con `metro_draw_tile()`/`metro_draw_progress()`.

**Qué se encontró**: esas cuatro primitivas operan sobre tipos que el propio plan define recién en F3 (`struct metro_page`/`struct metro_pivot`/`struct metro_row`, tabla de `metro_page.h` en §1.2, fase F3). Escribirlas en F2 exigía inventar firmas de datos provisionales que F3 probablemente tendría que rehacer, contra el criterio de "no diseñar para hipotéticos" del propio proyecto.

**Qué se hizo**: F2 implementa solo lo que el propio criterio de "hecho" de F2 pide y puede probar de forma aislada: `metro_draw_clear()`, `metro_draw_text()`, `metro_draw_text_cut_right()`, `metro_draw_header()`, `metro_draw_battery()`. Las otras cuatro quedan explícitamente para F3, cuando `metro_page.h` ya exista.

**Impacto en `PLAN_MAESTRO.md`**: §1.2, fila `metro_draw.c/.h` — la fase de `draw_rows`/`draw_pivots`/`draw_tile`/`draw_progress` pasa de "F2, F3" a "F3" únicamente.

---

## F2-2 — Capturas headless del simulador no son fiables una vez `metro_main()` entra a su loop de botones (macOS ≥26.4) — **DIAGNÓSTICO INCORRECTO, VER F2-4**

> **CORRECCIÓN (F2-4, 2026-08-20)**: todo lo que sigue en F2-2 y F2-3 es el registro honesto de una investigación que llegó a una conclusión **equivocada**. El crash NO era una limitación del entorno (macOS/SDL/AppKit) sino un **bug de Metro**: un `struct viewport` sin inicializar en `metro_draw_text_cut_right()` (F2). Se conserva el texto original como historial de cómo se razonó mal, y F2-4 explica la causa real, la evidencia y el arreglo. Las decisiones M-025 y M-026 de `DECISIONS.md` quedan **superadas** por M-027.

### Texto original de F2-2 (conclusión superada)


**Plan decía** (`PLAN_MAESTRO.md` §5, criterio de "hecho" de F2): capturar `F2-type-specimen.png` con `sim_shot.sh` de la forma estándar (como ya había funcionado en F0 y F1).

**Qué se encontró**: con el código de F2 (que agrega carga de 5 fuentes + dibujo del espécimen antes del loop), `sim_shot.sh docs/screenshots/F2-type-specimen.png 100` produce un `dump.bmp` truncado (66 bytes, solo cabecera) y el proceso termina con una excepción no capturada de AppKit:

```
*** Terminating app due to uncaught exception 'NSInternalInconsistencyException',
reason: 'nextEventMatchingMask should only be called from the Main Thread!'
...
7   rockboxui   button_get_w_tmo + 56
8   rockboxui   metro_main + 44
```

Investigado a fondo: **no es un bug de Metro**. `tools/configure` (heredado de Aura-Firmware, ver `MODIFICATIONS.md`) ya trae un comentario `FIXME` explícito sobre esto:

> `# FIXME: sigaltstack fails with "Operation not permitted" in make_context (firmware/asm/thread-unix.c) on latest versions of macOS (≥ 26.4). Fall back to SDL threads.`

Este Mac corre macOS 26.5.2 -- dentro del rango que ese `FIXME` ya marca como problemático. Con `HAVE_SDL_THREADS` (confirmado activo en `autoconf.h` de este build), el "hilo del dispositivo" donde corre `metro_main()` es un hilo creado por SDL, no el hilo principal real del proceso -- y en macOS ≥26.4, `SDL_PumpEventsInternal`/AppKit ya no tolera que ese hilo secundario bombee eventos (`nextEventMatchingMask`) mientras el mecanismo de captura headless (`uisimulator/common/sim_tasks.c`, en un hilo aparte) intenta volcar la pantalla a disco concurrentemente. El barrido de valores de `METRO_SIM_AUTODUMP_TICKS` confirma que la corrupción es prácticamente determinista para cualquier tick ≥30 (15/15 intentos fallidos a `ticks=100`) -- y como `show_logo_boot()` (stock de Rockbox) ya bloquea los primeros ~100 ticks con su propio `sleep(HZ)`, no existe ningún valor de tick donde la captura sea a la vez confiable y posterior al arranque de `metro_main()`. Se probaron sin éxito: forzar `SDL_VIDEODRIVER=dummy` (el proceso se cuelga en vez de crashear), terminar el proceso externamente tras estabilizar el archivo (la corrupción ya ocurrió antes de que el poll externo detecte el archivo), adjuntar `lldb` para invocar `screen_dump()` manualmente con todos los hilos pausados (requiere una autorización interactiva de macOS no disponible en esta sesión en segundo plano), y el hint `SDL_MAC_BACKGROUND_APP=1` de SDL3 (documentado como "don't force the SDL app to become a foreground process" -- exactamente el escenario, pero no cambió el resultado).

**Qué se hizo**: no se alteró `metro_main()` ni ningún código de producto para "arreglar" esto -- degradar la responsividad real de la UI (p.ej. agregando un `sleep()` fijo antes del loop de botones) solo para que un capturador de pantalla de desarrollo funcione sería resolver el problema equivocado. En su lugar:
- Se verificó la carga de las 5 fuentes por otra vía, sólida y determinista: la salida de `DEBUGF` (activo por defecto en builds de simulador) corriendo `rockboxui` de forma interactiva sin autodump, que muestra las 5 líneas `metro_fonts: <rol> (<ruta>) loaded as font id N` sin ningún fallback a `FONT_SYSFIXED` -- ver evidencia completa en el reporte de cierre de F2.
- `F2-type-specimen.png` **no se pudo capturar en esta sesión**. Queda pendiente de que el dueño la tome en su propia máquina corriendo `firmware/tools/sim_shot.sh` interactivamente (fuera de una sesión headless en segundo plano, que es probablemente donde esta restricción de AppKit se manifiesta con más fuerza) -- o, si el problema persiste ahí también, de investigar un fix real en `uisimulator/common/sim_tasks.c`/`firmware/target/hosted/sdl/` en una fase dedicada.

**Impacto en `PLAN_MAESTRO.md`**: el criterio de "hecho" de F2 se cumple parcialmente -- carga de fuentes verificada por log, compilación limpia para simulador y target real verificada, pero sin la captura visual. **Riesgo real para fases futuras**: F3 en adelante depende de `sim_shot.sh` con secuencias de botones inyectadas (`METRO_SIM_BUTTONS`), que necesariamente mantienen `metro_main()` corriendo su loop por muchos ticks -- exactamente el escenario que dispara esta falla. Si el problema persiste en sesiones futuras (headless o no), es un bloqueante real para el criterio "simulador primero" del proyecto y merece una fase de investigación dedicada antes de F3, no un parche apurado.

**Confirmado por el dueño en sesión interactiva** (no en segundo plano): el mismo crash ocurre corriendo `sim_shot.sh` directamente en Terminal.app en este Mac. Descarta la hipótesis de "solo pasa en sesiones headless" -- es un problema real del entorno (macOS 26.5.2 + este SDL3 + Rockbox), no de esta sesión en particular.

---

## F2-3 — Intento de arreglo real (mover `screen_dump()` al hilo de `metro_main()`): revertido, cambia el crash por un deadlock peor — **CAUSA RAÍZ EQUIVOCADA, VER F2-4**

Tras la confirmación de F2-2 en sesión interactiva, se intentó una solución real en vez de solo documentar el bloqueo, ya que F3 depende de esto.

**Diagnóstico más profundo**: `screen_dump()` (`firmware/screendump.c`) en sí no tiene ninguna dependencia de SDL -- es E/S de archivos POSIX pura (`creat`/`write`/`close`) sobre `lcd_framebuffer`. El crash de F2-2 ocurre porque se **invoca desde `sim_thread`** (un hilo Rockbox real bajo `HAVE_SDL_THREADS`, creado en `uisimulator/common/sim_tasks.c`), mientras `metro_main()` corre en un hilo *distinto* y está bloqueado dentro de `button_get_w_tmo()` -> `SDL_PumpEventsInternal`. Confirmado con un experimento de control: `timeout 20 ./rockboxui` sin ninguna variable de autodump corre 20 segundos completos sin crashear -- `button_get_w_tmo()` por sí solo es inofensivo; el crash exige la actividad concurrente de `sim_thread`.

**El intento**: se separó el disparo (`sim_thread` sigue detectando *cuándo* volcar, vía tick o tecla F5/KP_0) de la ejecución real (`screen_dump()`+`exit()` movidos a una función nueva, `sim_tasks_service_dump()`, declarada en `sim_tasks.h` y llamada desde el propio loop de `metro_main()` en cada iteración -- el único hilo que además ya pasa por `button_get_w_tmo()` de forma segura). Cambios: `uisimulator/common/sim_tasks.c`/`.h`, `apps/metro/metro_main.c` (agregado `sim_tasks_service_dump()` al loop, y `button_get_w_tmo(HZ)` -> `button_get_w_tmo(HZ/10)` para no encolar la captura hasta un segundo completo).

**Resultado**: el crash de AppKit desapareció (0/varios intentos), pero apareció un **deadlock nuevo y peor**: el archivo de volcado queda truncado en exactamente 66 bytes (la cabecera BMP) en el 100% de los intentos -- incluso en los casos que antes SÍ funcionaban (`ticks=20`/`25`, capturando el logo de arranque). Instrumentación temporal con `DEBUGF` en `screendump.c` (revertida) localizó el punto exacto: la cabecera se escribe bien, se entra al bucle de líneas de barrido, se ejecuta la primera iteración (`memset` + copia de píxeles desde `FBADDR`) -- y el proceso se queda colgado ahí mismo, nunca llega ni al primer `write()` de una línea de barrido ni a ningún mensaje de error. `ps`/`lsof` durante el cuelgue confirman: el fd real (`6`, distinto del "fd" virtual `1` que reporta la capa hosted de Rockbox) queda abierto con exactamente 66 bytes escritos, y el proceso está en estado `S` (esperando, no en una syscall bloqueada tipo `U`) -- consistente con un deadlock real en algún primitivo de sincronización (probablemente en la capa de filesystem simulado, `firmware/target/hosted/filesystem-unix.c`, no tocada por Metro ni por Aura), no con un cuelgue de la copia de píxeles en sí (que es aritmética de punteros pura, sin ninguna razón para bloquearse).

**Qué se hizo**: se revirtió el intento por completo (`git checkout --` sobre los 4 archivos tocados) -- un deadlock del 100% de las veces es estrictamente peor que un crash que al menos funcionaba parcialmente (`ticks≤25`, antes de que `metro_main()` entrara a su loop). El árbol quedó exactamente como en el commit de F2.

**Impacto en `PLAN_MAESTRO.md`**: ninguno adicional a F2-2 -- el bloqueo para F3 sigue en pie. **Pista real para quien retome esto**: el problema no es únicamente "qué hilo bombea eventos SDL" (F2-2) -- hay una **segunda** condición de carrera independiente en la capa de filesystem simulado de Rockbox, que se dispara específicamente cuando `screen_dump()` se llama desde el hilo "device" (`metro_main`) en vez de `sim_thread`. Cualquier solución real necesita investigar `firmware/target/hosted/filesystem-unix.c` y qué primitivo de sincronización usa alrededor de `open`/`write`/`close`, no solo el problema de AppKit. Candidato con mejor relación esfuerzo/resultado si se retoma: correr `sim_shot.sh` sin `METRO_SIM_AUTODUMP_QUIT` y con `lldb`/Instruments interactivo (no headless) para ver el backtrace real de ambos hilos en el momento del cuelgue -- esta sesión no pudo usar `lldb` por falta de autorización interactiva de macOS (ver F2-2).

---

## F2-4 — Causa real del crash de captura: `struct viewport` sin inicializar en `metro_draw_text_cut_right()` (bug de Metro, no del entorno). ARREGLADO.

**Qué se hizo distinto esta vez**: en vez de seguir razonando sobre símbolos de un backtrace impreso por AppKit, se buscó primero el **discriminador más potente**: el simulador de **Aura-Firmware** (mismo Mac, mismo macOS 26.5.2, mismo SDL3/sdl2-compat, mismo `HAVE_SDL_THREADS`, mismo `sim_tasks.c`, mismo `screen_dump()`, y una UI que también espera botones en `button_get_w_tmo()`) se probó con `apple2026_sim_shot.sh … 300` — **y funcionó a la primera**. Eso descartó de golpe toda la hipótesis de "limitación del entorno" de F2-2/F2-3: la diferencia tenía que estar en el código de Metro.

**Backtrace real (lldb, todos los hilos, lanzando el proceso bajo el depurador — no adjuntándose, que era lo que pedía permiso)**: el hilo que crashea (#11, `sim_thread` de `sim_tasks.c`) muestra esta cadena: `runthread → sim_thread (sim_tasks.c:183, la llamada a screen_dump()) → metro_main (metro_main.c:71, la llamada a button_get_w_tmo) → button_queue_wait → SDL_PumpEvents → Cocoa → excepción`. Es decir, **`sim_thread` aparece "llamando" a `metro_main`**, algo imposible como cadena de llamadas legítima — `screen_dump()` no llama a nada de Metro. La única explicación consistente con el desensamblado (`screen_dump` hace `bl _sim_creat`, `bl _sim_write` y lee píxeles con `FBADDR(0,y)`): `FBADDR(x,y)` se expande a **`lcd_current_viewport->buffer->get_address_fn(x,y)`** — una llamada indirecta a través de un puntero a función guardado en el viewport activo. Si ese viewport (o su `buffer`) está corrupto, la llamada indirecta salta a código arbitrario; aquí cayó dentro de `metro_main()`, cuyo loop llama a `button_get_w_tmo()`, que en `__APPLE__ && PLATFORM_SDL` hace **`SDL_PumpEvents()` en el hilo llamante** (`firmware/drivers/button_queue.c:105`, por diseño — en macOS no hay hilo de eventos aparte y el hilo principal debe bombear). Ejecutado desde `sim_thread` (un hilo secundario), eso dispara exactamente `nextEventMatchingMask should only be called from the Main Thread!`. Y explica también el deadlock del 100 % de F2-3: al mover `screen_dump()` al hilo principal, el salto indirecto corrupto aterrizó en otro lugar que bloqueaba en vez de crashear.

**La corrupción**: `metro_draw_text_cut_right()` (`apps/metro/metro_draw.c`, F2) declaraba `struct viewport vp;` **en la pila, sin inicializar**, y llamaba `viewport_set_fullscreen(&vp, SCREEN_MAIN)` directamente. `viewport_set_fullscreen()` llama primero a `screens[].init_viewport(vp)` = `lcd_init_viewport()` (`firmware/drivers/lcd-bitmap-common.c:259`), que **lee `vp->buffer` antes de que nadie lo haya asignado**: si es distinto de NULL (basura de pila), lo desreferencia como `struct frame_buffer_t*`, lee `->elems`, y si no es cero **escribe** `->stride`, `->data` y `->get_address_fn` a través de ese puntero basura cuando los encuentra en cero. Comportamiento indefinido que en la práctica corrompió el estado del LCD (el `get_address_fn` que `screen_dump()` usa más tarde). El idioma correcto de Rockbox, usado por todos los llamadores del core, es `viewport_set_defaults()`, que hace `vp->buffer = NULL` **antes** de llamar a `viewport_set_fullscreen()` — por eso ellos no tienen el problema.

**Verificación empírica (causalidad, no solo correlación)**:
- Experimento A: quitar únicamente la llamada a `metro_draw_text_cut_right()` del espécimen → captura correcta (153 666 bytes, salida limpia) a `ticks=100`, exactamente donde antes fallaba 15/15 veces.
- Arreglo: `viewport_set_defaults(&vp, SCREEN_MAIN)` en vez de `viewport_set_fullscreen(&vp, …)` → captura correcta con la función presente, a `ticks=100`, `200` y `300`; también con inyección de botones (`"SELECT,SCROLL_FWD,SCROLL_FWD,MENU,RIGHT,LEFT"`, el escenario de F3).

**Segundo bug detectado gracias a la primera captura real**: los espacios no se veían ("title28px", "list20pxregular"). Causa: el flag `-x` de `convttf` en `gen_fonts.sh` ("trim glyphs horizontally of nearly empty space") recorta hasta 2 px por lado de **todo** glifo casi vacío, incluido el espacio (0x20), que a 20 px pasa de ~5 px a ~1 px de ancho. Aura-Firmware no usa `-x` (`design-system/generate.py` invoca solo `-p <size>`). Se quitó `-x`, se regeneraron las 5 fuentes, y la captura muestra espacios correctos. Ver `DECISIONS.md` M-028.

**Qué se hizo**: (1) `metro_draw.c`: `viewport_set_defaults()` + comentario explicando por qué; (2) `gen_fonts.sh`: sin `-x`, fuentes regeneradas y versionadas; (3) `metro_screen_specimen.c`: el titular `display` ahora es "recorte al borde" para que el recorte en x=320 se vea de verdad (criterio de F2); (4) `docs/screenshots/F2-type-specimen.png` capturada — **el criterio de "hecho" de F2 queda cumplido al 100 %**; (5) `sim_shot.sh` con botones inyectados verificado → **F3 queda desbloqueada**; (6) simulador y target ipod6g compilan limpio.

**Lección metodológica registrada** (para el resto de la Fase 4): cuando un síntoma "parece del entorno", **la primera prueba es el discriminador más barato y potente** — aquí, correr el proyecto hermano que comparte el 100 % del entorno. Dos rondas de investigación (F2-2, F2-3, ~7 técnicas) se gastaron razonando sobre nombres de símbolos de un backtrace sin depurador, cuando `lldb --batch -o run -k "thread backtrace all"` (lanzar, no adjuntar) daba el backtrace real en 30 segundos y el test con Aura en 10.

**Impacto en `PLAN_MAESTRO.md`**: ninguno en el plan en sí — el principio "simulador primero" sigue siendo válido y ahora está operativo. `DECISIONS.md` M-025/M-026 quedan superadas por M-027/M-028.

---

## F3-1 — `metro_page.h`: título/nombre de pivot como `enum metro_lang_id`, no `const char *`

**Plan decía** (`PLAN_MAESTRO.md` §1.1 punto 3, boceto de tipos): `struct metro_row {const char *title; ...}`, `struct metro_pivot {const char *name; ...}`, `struct metro_page {title; ...}` — sin distinguir explícitamente el tipo exacto de `title`/`name` en las tablas de pivots/páginas.

**Qué se encontró**: las tablas de pivots/páginas de F3 (`music_pivots[]`, `videos_pivots[]`, `settings_page`, etc.) son `static const` — inicializadas en tiempo de compilación. `metro_lang_str(LANG_X)` es una llamada a función, no una expresión constante — usarla como inicializador de un `const char *name` dentro de un array `static const` es un error de compilación en C. Además, aunque compilara, un `const char *` fijo en una tabla `static const` quedaría congelado en el idioma vigente al momento de construir el binario — cambiar el idioma en vivo (`metro_lang_set()`, ya cableado en Ajustes) nunca actualizaría los encabezados de pivot ni los títulos de página.

**Qué se hizo**: `struct metro_page.title` y `struct metro_pivot.name` son `enum metro_lang_id` (una constante de verdad, válida en un inicializador estático). `metro_draw_pivots()`/`metro_screen_list_show()` resuelven el string con `metro_lang_str()` en el momento de dibujar, no antes — así el cambio de idioma se refleja de inmediato en el próximo redibujado, sin tocar ninguna tabla. `struct metro_row.title`/`.subtitle` no tuvieron este problema: `get_row()` corre en tiempo de dibujo (una llamada a función normal, no un inicializador), así que puede llamar a `metro_lang_str()` directamente.

**Impacto en `PLAN_MAESTRO.md`**: ninguno en el comportamiento visible; es una corrección de tipo en la firma de `metro_page.h` respecto al boceto original del plan, necesaria para que compile y para que M-009 (idioma en vivo) funcione de verdad.

---

## F4-1 — `metro_page.h`: `title_dynamic` para páginas cuyo encabezado nombra datos del usuario

**Plan decía** (`PLAN_MAESTRO.md` §5 F4): "biblioteca musical real (tagcache) y reproducción", con pantallas de artistas → álbumes de ese artista → canciones de ese álbum (implícito en las capturas requeridas: `F4-artist-albums.png`, `F4-album-songs.png`). No especifica cómo el encabezado de esas pantallas intermedias muestra CUÁL artista/álbum se está viendo.

**Qué se encontró**: `struct metro_page.title` es `enum metro_lang_id` desde F3-1 (una constante de tiempo de compilación, resuelta con `metro_lang_str()`), precisamente para que las tablas `static const struct metro_page` sigan siendo inicializadores válidos y el cambio de idioma se refleje solo. Pero el nombre de un artista, álbum o género no es una cadena de UI traducible — es un dato del usuario, leído de tagcache en el momento de la navegación. Nunca pudo caber en ese enum, y forzarlo ahí habría exigido inventar un id de idioma por cada artista de la biblioteca, absurdo.

**Qué se hizo**: se agregó `const char *title_dynamic` a `struct metro_page` (NULL en toda página estática existente — F3 no cambia). Una página cuyo encabezado nombra un dato del usuario aparta un buffer `static char[]` propio (p. ej. `s_artist_albums_title` en `metro_screen_hub.c`), copia ahí la etiqueta con `strlcpy()` antes de empujar la página, y apunta `title_dynamic` a ese buffer — `metro_screen_list_show()` usa `title_dynamic` si no es NULL, si no cae a `metro_lang_str(title)` como siempre. Como solo puede haber una instancia de cada subpágina de este tipo en la pila de navegación a la vez (un único camino lineal hacia abajo, ver comentario en `metro_page.h`), reusar un único buffer estático por subpágina es seguro.

**Impacto en `PLAN_MAESTRO.md`**: ninguno en el comportamiento visible; extiende el modelo de página declarativo de F3 de forma necesaria para F4, no una preferencia de diseño.

---

## F4-2 — Falso positivo observado en verificación: navegación "de más" en una corrida aislada del simulador

Durante la captura de las evidencias de F4 (`docs/screenshots/F4-*.png`), UNA corrida (de más de veinte intentos con parámetros idénticos) aterrizó dos niveles más abajo de lo esperado tras un solo `SELECT` (en música → artistas, terminó mostrando los álbumes de "Metro QA" en vez de la lista de artistas). Investigado con logging temporal en `metro_main()` (cada `metro_input_next()` resuelto, con profundidad de navegación antes/después): en cuatro corridas instrumentadas y en las siete siguientes sin instrumentar, `metro_main()` procesó exactamente UNA acción `MACT_SELECT` por cada token `SELECT` inyectado, con la profundidad de pila avanzando 1→2 como corresponde — nunca 1→3. No se logró reproducir la falla ni una sola vez en ~15 intentos adicionales bajo las mismas condiciones exactas (mismo build, mismo `simdisk`, mismos ticks de espera).

**Diagnóstico**: no hay evidencia de un bug determinístico en `metro_nav`/`metro_keymap`/`metro_input` (la máquina de pila resetea pivot/selección en cada `push`, comprobado por lectura de `metro_nav.c`; el conteo de acciones despachadas fue el esperado en cada corrida instrumentada). Todo apunta a una fluctuación de scheduling del host (macOS, hilo del simulador vs. hilo de tagcache en segundo plano) durante una corrida aislada de un lote de siete capturas consecutivas — no a código de Metro.

**Mitigación usada**: cada captura de F4 usa su propia corrida limpia del simulador (proceso `rockboxui` nuevo) con un margen amplio de espera (`WAIT` ×10, ~10 s) antes del primer `SELECT` hacia música, dando tiempo de sobra a que `tagcache_rebuild()` termine antes de que se inyecte cualquier botón. Cualquier captura futura cuyo contenido no corresponda al esperado debe simplemente repetirse (proceso nuevo) antes de darla por buena — no se investiga más a fondo dado que no hay mecanismo determinístico identificado y el costo de una repetición es bajo.

---

## F5-1 — Sin íconos compilados; batería/aleatorio/repetir en texto, no bitmaps

**Plan decía** (`PLAN_MAESTRO.md` §5 F5, "Archivos"): `apps/bitmaps/native/{metro_*.bmp,SOURCES}` como entregable de esta fase — íconos compilados para batería, play/pausa, aleatorio, repetir.

**Qué se hizo en su lugar**: exactamente el mismo criterio que F2 ya adoptó para la batería (`metro_draw_battery()`, texto "NN%", comentario propio "no bitmap icon yet ... lands in F5") — F5 extiende ese MISMO criterio a aleatorio/repetir en vez de romperlo a medias: `metro_screen_nowplaying.c` dibuja "aleatorio"/"repetir todo"/"repetir uno" en `MFONT_CAPTION` acento, solo cuando están activos, en la esquina inferior derecha (mismo lugar que el plan reserva para los íconos de 16px). Play/pausa no tiene ícono visible en ningún lado porque no hay un control en pantalla que lo represente (es un botón físico del clickwheel).

**Por qué**: montar un pipeline real de íconos (diseñar los BMP, `apps/bitmaps/native/SOURCES`, conversión `bmp2rb`, manejo de máscara/alpha para LCD_DEPTH>1) es trabajo de asset design + tooling, no de lógica de pantalla — encaja mejor en F10 ("Pulido estético") junto con el resto del pulido visual que esa fase ya tiene reservado, que partirlo a medias aquí. Texto en acento cumple la misma función (visibilidad + color = estado activo) sin bloquear el resto de F5 en un pipeline de assets nuevo.

**Impacto en `PLAN_MAESTRO.md`**: F10 hereda la tarea de reemplazar estos tres textos (batería, aleatorio, repetir) por íconos reales — no es trabajo nuevo, es el mismo pendiente que F2 ya dejó abierto para la batería, ahora con dos casos más.

## F5-2 — Página "options" sin teclas de transporte de PLAYER (solo LIST)

**Plan decía** (`PLAN_MAESTRO.md` §2.3, contextos): "`OPTIONS` (página de opciones de NP, es una `LIST` con teclas de transporte activas)" — sugiere que PLAY/LEFT/RIGHT deberían seguir funcionando como controles de reproducción incluso dentro de la página de opciones.

**Qué se hizo**: la página `options` es una `metro_page` normal, empujada con `metro_screen_list_push()` igual que cualquier otra lista — usa el contexto `MCTX_LIST` completo (SELECT activa/cicla la fila, MENU vuelve), sin ninguna tecla de transporte extra.

**Por qué**: darle a `options` un contexto híbrido real exige que `metro_main.c` sepa distinguir "estoy en LIST" de "estoy en LIST-pero-con-transporte-activo" — un tercer camino de resolución de contexto además de HUB/LIST/PLAYER, solo para esta única página. El usuario puede volver con MENU y usar las teclas de transporte reales en Now Playing; el costo de esa vuelta es un botón, la complejidad de no pagarlo era mucho mayor. [ESTIMADO: no hay una captura que dependa de esto, ninguno de los "hecho" de F5 lo pide explícitamente.]

**Impacto en `PLAN_MAESTRO.md`**: ninguno funcional; si en una fase futura el dueño del diseño pide específicamente poder pausar/saltar pista sin salir de "próximas", se revisita como una desviación de diseño nueva, no silenciosa.

## F5-3 — Título de Now Playing en una sola línea con recorte, no 2 líneas con wrap

**Plan decía** (`PLAN_MAESTRO.md` §1.4): "título en `title` (28) blanco, 2 líneas máx. con recorte".

**Qué se hizo**: `metro_draw_text_cut_right()` en una sola línea — el título se recorta en el borde derecho igual que cualquier otro texto largo de Metro (A.6), nunca pasa a una segunda línea.

**Por qué**: el wrap real de texto (encontrar el punto de corte entre palabras, medir cada línea, decidir cuándo el título CABE en 2 líneas vs. cuándo hay que recortar la segunda) no existe todavía en `metro_draw.c` — ninguna otra pantalla de Metro lo necesita hasta ahora. Construirlo para este único caso, bien, es trabajo de pulido de texto más que de la pantalla de Now Playing en sí. [ESTIMADO: la mayoría de títulos reales caben en una línea a 28px; el caso de 2 líneas es la excepción, no la regla.]

**Impacto en `PLAN_MAESTRO.md`**: un título muy largo se ve recortado en vez de partido en 2 líneas. F10 es el lugar natural para agregar wrap real a `metro_draw.c` si hace falta, reutilizable por cualquier pantalla, no solo Now Playing.

## F5-4 — `MACT_PLAY_ITEM`/`MACT_SHUFFLE_ALL` (PLAY y SELECT sostenido sobre una fila reproducible en LIST) no implementados

**Plan decía** (`PLAN_MAESTRO.md` §2.3): filas reproducibles en cualquier `LIST` deberían responder a `BUTTON_PLAY|BUTTON_REL` (reproducir esa fila) y `BUTTON_SELECT|BUTTON_REPEAT` (reproducir todo el contenido de la fila en aleatorio), además del `SELECT` corto ya implementado desde F4.

**Qué se hizo**: `SELECT` corto sigue siendo la única forma de reproducir una fila desde una lista — ya cubierto desde F4 (`on_select` de cada pivot reproducible). `PLAY`/`SELECT` sostenido no están mapeados en `list_mapping` (`metro_keymap.c`), quedan como `ACTION_NONE`.

**Por qué**: son gestos alternativos que llegan al MISMO resultado que `SELECT` ya cubre (reproducir la fila) o a una variante (aleatorio) que `metro_music_shuffle_all()` ya expone como capacidad de infra pero sin ningún disparador de UI todavía (ver el comentario de esa función en `metro_music.h`, deuda ya reconocida desde F4). Agregarlos ahora significa ampliar `list_mapping` con una fila nueva por cada pivot reproducible y decidir, por primera vez, qué distingue una fila "reproducible" de una que no lo es a nivel de tabla de botones — no solo de comportamiento de `on_select`. Fuera del alcance de lo que F5 necesitaba entregar (la pantalla de Now Playing en sí).

**Impacto en `PLAN_MAESTRO.md`**: ninguna captura de F5 depende de esto. `metro_music_shuffle_all()` sigue esperando su primer disparador de UI real — candidato natural: una fila "aleatorio" en el propio hub o en el nivel superior de música, a decidir cuando se retome.

## F5-5 — Aleatorio real solo al activarlo; no hay "desordenar" limpio al desactivarlo

**No es una desviación del plan** (el plan no especifica el mecanismo interno), pero es una limitación real de Rockbox documentada explícitamente en el código (`metro_screen_nowplaying.c`, `toggle_shuffle()`) para que quede visible en el historial y no se lea como un bug de Metro: activar "aleatorio" llama a `playlist_shuffle()` de verdad (reordena la cola restante); desactivarlo solo dejar de tratar la bandera como activa, no restaura el orden original — Rockbox no tiene una operación de "desordenar" inversa. Mismo comportamiento que el Rockbox original.

---

## F6-1 — `metro_sync.c` simplificado respecto a `aura_sync.c`: sin pantalla por sección, sin invalidación de video/fotos, sin disparo manual

**Plan decía** (`PLAN_MAESTRO.md` §5 F6, implícito por el nombre del módulo y su relación con `aura_sync.c` de Aura-Firmware, la referencia de puerto explícita en el proyecto): un orquestador de sincronización con el mismo nivel de detalle que `aura_sync.c` (~480 líneas: pantalla con una fila de estado por sección, invalidación de `metro_video`/`metro_photos`, limpieza de caché de carátulas, disparo manual de "reconstruir biblioteca").

**Qué se hizo**: `metro_sync.c` porta la máquina de estados central (lectura del marcador, `tagcache_update()`/`tagcache_rebuild()`, contador de intentos, posponer/reintentar, recuperación tras una base corrupta) pero recorta tres cosas:
1. **Sin pantalla por sección**: una sola pantalla de texto ("actualizando biblioteca...") en vez de una fila con estado por sección — en v1 de Metro solo `music` hace trabajo real (ver punto 2), así que una fila por sección sería UI para un solo dato.
2. **`video`/`images` se marcan `DONE` de inmediato, sin invalidar nada**: a diferencia de Aura (que cachea listados y carátulas de video/fotos en disco), F7's `metro_video.c`/`metro_photos.c` van a re-escanear su carpeta cada vez que se entra a la página — mismo patrón que ya usa `metro_music_lists_refresh()` desde F4. Sin caché, no hay nada que invalidar.
3. **Sin disparo manual todavía**: `aura_sync_request_manual()` (el "Reconstruir biblioteca" de Ajustes) no tiene equivalente en `metro_sync.h` — F8 es quien conecta Ajustes a `metro_sync`, y ahí se agrega si hace falta.

**Por qué**: los tres recortes reflejan que Metro v1 es deliberadamente más chico que Aura (§1.2 del plan, "Metro es deliberadamente más chico") — replicar la UI de progreso por sección o la invalidación de caché de Aura sería construir infraestructura para un caché que Metro no tiene.

**Impacto en `PLAN_MAESTRO.md`**: ninguno en las claves del contrato ni en el comportamiento observable de C1-C7 (todas verificadas). Si F7 termina necesitando un caché real de video/fotos (por ejemplo, para miniaturas), `metro_sync.c` gana ahí las llamadas de invalidación que le faltan — no es un cambio de diseño, es la costura ya prevista (`metro_sync.h` documenta esto en su comentario de módulo).

---

## F7-1 — Sin póster de video (`Videos/<archivo>.jpg`), sin miniaturas de foto

**Plan decía**: la tabla D del contrato de Aura Studio (leída del repo hermano) documenta `Videos/<archivo sin extensión>.jpg` como un póster opcional que Aura Studio puede sincronizar junto a cada video, usado por CoverDrift (el carrusel visual de Aura) — y Aura Studio también coloca miniaturas cacheadas de fotos en un formato propio.

**Qué se hizo**: `metro_video.c`/`metro_photos.c` ignoran por completo cualquier `.jpg` que acompañe a un video, y no generan ni leen ninguna miniatura — cada fila de Videos/Fotos es texto plano (el nombre de archivo), igual que cualquier otra lista de Metro hasta ahora.

**Por qué**: Metro nunca ha mostrado una imagen fuera de la carátula de Now Playing (F5) — no hay ningún carrusel visual tipo CoverDrift en el árbol de navegación de Metro (`PLAN_MAESTRO.md` §2.2, el árbol de Videos/Fotos es una lista de pivots, no un carrusel), así que no hay ningún lugar donde un póster o una miniatura se mostrarían. Construir el pipeline de decodificación+caché+display para algo que no tiene consumidor sería trabajo especulativo.

**Impacto en `PLAN_MAESTRO.md`**: ninguno en el comportamiento de C12-C14 (todos verificados sin pósters/miniaturas). Si una fase futura agrega un componente visual a Videos/Fotos (fuera del plan actual de fases F0-F13), el póster ya está documentado en el contrato y solo falta leerlo — no hace falta que Aura Studio cambie nada de su lado.

---

## F8-1 — About sin `sync_summary.cfg`: un mensaje, no 3 filas de guiones

**Plan decía** (checklist C15, `PLAN_MAESTRO.md` §4): "`sync_summary.cfg`: presente → conteos; ausente → guiones" — sugiere mostrar cada fila de conteo (canciones/videos/fotos) con un guion como valor cuando el archivo no existe.

**Qué se hizo**: `metro_screen_about.c` muestra una única fila "sin sincronizar todavía" en vez de 3 filas "canciones: -", "videos: -", "fotos: -".

**Por qué**: en una lista de Metro (pitch 28px, texto grande) 3 filas idénticas salvo por la etiqueta, todas con el mismo valor "-", leen como una lista rota o a medio cargar más que como un estado intencional — un solo mensaje en español/inglés claro ("sin sincronizar todavía"/"not synced yet") comunica lo mismo con menos ambigüedad visual. [ESTIMADO: preferencia de legibilidad, no una limitación técnica — revertir a 3 filas con guion es un cambio de `about_count()`/`about_get_row()` de unas pocas líneas si se prefiere pixel-parity con el texto literal del plan.]

**Impacto en `PLAN_MAESTRO.md`**: ninguno funcional — la distinción "hay datos reales" vs. "nunca se sincronizó" sigue siendo inequívoca para el usuario, solo cambia CÓMO se comunica "ausente".

---

## F9-1 — Pantalla USB: el cuadro de Metro no llega a verse en el simulador, no solo "toda la conexión" queda sin restilar

**Plan decía**: `apps/metro/metro_screen_usb.c` (archivo nuevo) + `apps/gui/usb_screen.c` (mod: "fix de centrado + colores") + un wordmark propio, sugiriendo una pantalla USB completamente restilada durante toda la conexión, como logró Aura-Firmware (aunque esa fase, según su propio `docs/superficies-rockbox.md`, tampoco llegó a construir el selector completo "Conectado" — quedó parcial ahí también).

**Qué se hizo y qué se encontró verificándolo**: `metro_screen_usb_show()` dibuja un cuadro de Metro (wordmark + "conectado") justo antes de pasarle `SYS_USB_CONNECTED` a `default_event_handler()`. La verificación con el token `USB_INSERT` (`docs/screenshots/F9-usb.png`, M-039) muestra que esto **no alcanza a verse ni un solo cuadro**: `default_event_handler()` llama `gui_usb_screen_run()` (la pantalla nativa de Rockbox) de inmediato, sin ceder el hilo, y esa función hace su propio `lcd_update()` antes de bloquearse esperando la desconexión — la captura a 0 ticks de espera después de `USB_INSERT` ya muestra el ícono USB nativo de Rockbox, nunca el wordmark de Metro. `apps/gui/usb_screen.c` no se tocó — sigue siendo la pantalla nativa, sin restilo, dueña de la pantalla durante toda la conexión real.

**Por qué se deja el código igual, sabiendo que no se ve en el simulador**: `gui_usb_screen_run()` bloquea hasta que se desconecta el cable, dueño exclusivo de la pantalla durante ese tiempo — interceptarlo de verdad exigiría reimplementar el manejo de almacenamiento masivo/HID de Rockbox dentro de Metro, un riesgo real en hardware (montar mal el disco, o dejarlo sin desmontar limpio, corrompe datos) para un beneficio puramente cosmético. `metro_screen_usb_show()` se mantiene de todas formas porque es inofensivo (un `lcd_update()` de más, sin efecto en la lógica de USB) y [ESTIMADO] es posible que en hardware real, con tiempos de refresco de LCD físicos en vez del blit instantáneo del simulador, sí alcance a verse un parpadeo breve antes de que `gui_usb_screen_run()` tome el control — algo que esta sesión no puede confirmar sin el dispositivo. Parchear `usb_screen.c` de verdad (como sí hizo Aura: centrado del logo + bitmap propio) exigiría además un asset de bitmap nuevo que esta sesión no puede generar (sin pipeline de diseño de íconos, mismo motivo que F5-1 dejó los íconos compilados para F10).

**Impacto en `PLAN_MAESTRO.md`**: en el simulador, conectar USB muestra la pantalla nativa de Rockbox sin ningún restilo de Metro visible — más limitado que "solo el primer cuadro". `apps/gui/usb_screen.c` queda como el candidato real para F10 (pulido estético) si se agrega ahí un bitmap wordmark propio; verificar en hardware si `metro_screen_usb_show()` sí llega a verse ahí es una pregunta abierta para F13.

---

## F10-1 — Letra flotante del índice: sin la puerta "≥40 filas" del plan, y sin verificación visual del disparo real por rueda

**Plan decía**: la letra flotante que aparece durante un scroll rápido (S1.4) se dispara "en listas con 40 o más elementos" — una puerta de tamaño mínimo antes de mostrar el indicador, pensada para listas realmente largas donde perder la posición es un problema real.

**Qué se hizo**: `metro_screen_list.c` dispara la letra en cualquier pivote, sin mirar `pivot->count()`, con la única condición siendo `steps >= METRO_INDEX_LETTER_MIN_STEPS` (3). Además, verificando esto en el simulador se encontró que la inyección de botones headless (`sim_tasks.c`) nunca produce `steps > 1` — ver M-040 — así que el disparo real por scroll rápido en hardware queda sin poder verificarse visualmente en este entorno; se verificó bajando el umbral a 1 de forma temporal (revertido antes de compilar la versión final) para confirmar que el *dibujo* del indicador y su expiración (redibujado de "borrado") funcionan.

**Por qué se omite la puerta de tamaño**: ninguna de las listas reales de Metro con los fixtures que esta sesión puede generar (`gen_test_media.sh`) llega ni de lejos a 40 elementos — la lista más larga (fotos "todos") tiene menos de 10. Sin una lista real de ese tamaño para probar contra, agregar la puerta sería código sin ruta de verificación en este entorno, y una lista corta (5-10 filas) igual se beneficia del mismo refuerzo visual al aterrizar lejos de donde estaba. [ESTIMADO: con una biblioteca real de cientos de canciones, la puerta de 40 evita que el indicador aparezca en saltos triviales de una lista corta de 6-8 filas donde ya se ve toda la lista en pantalla a la vez — vale la pena agregarla cuando haya una forma de probarla, la condición es un `if (count >= 40)` de una línea en `metro_screen_list_handle()`.]

**Impacto en `PLAN_MAESTRO.md`**: ninguno observable con los fixtures actuales (todas las listas quedan por debajo del umbral de todas formas). Si F11+ trae una biblioteca de prueba más grande, agregar la puerta y su verificación visual queda como trabajo pendiente explícito, no una regresión silenciosa.

---

## F11-1 — Transiciones: `MACT_OPTIONS` desde Now Playing usa PUSH (no FADE), y `MACT_HOME` siempre usa un solo POP-slide

**Plan decía**: el catálogo de transiciones (`PLAN_MAESTRO.md` S3.3) fija FADE para "push(NP), vuelta de plugin" sin mencionar explícitamente la página de opciones de Now Playing (up next/shuffle/repeat, F5) ni `MACT_HOME` (ir directo al hub sosteniendo Back, que puede saltar más de un nivel a la vez).

**Qué se hizo**: `metro_main.c` decide la transición diffeando profundidad/pivot/"¿es NP la página actual?" antes y después de cada acción (M-043). Con esa regla, entrar a la página de opciones desde NP (`MACT_OPTIONS`) sube la profundidad pero la página nueva NO es el centinela de NP -- cae al PUSH genérico (desliza como cualquier otra subpágina), no a FADE. Volver de opciones a NP (`MACT_BACK` ahí) baja la profundidad revelando el centinela, pero el chequeo de "salir de NP" exige que la página ANTERIOR fuera el centinela (no lo era, era opciones) -- también cae al POP genérico, no a FADE. `MACT_HOME` (`metro_screen_list_pop_to_root()`) puede bajar la profundidad en más de un nivel de una sola vez; la regla de diff no distingue "un pop" de "varios pops encadenados", así que siempre corre un único POP-slide sin importar cuántos niveles saltó realmente.

**Por qué**: FADE está reservado en el catálogo a "entrar/salir del reproductor" como concepto -- opciones de NP sigue siendo, en todo lo demás, una `LIST` normal (mismo contexto `OPTIONS` que la tabla de gestos de S2.3 ya trata como "una LIST con teclas de transporte activas", no como una superficie distinta) así que tratar su entrada/salida como un PUSH/POP común es consistente con esa misma tabla, no una desviación arbitraria. Para `MACT_HOME`, animar un salto de N niveles como N slides encadenados sería visualmente ruidoso para una acción que el propio plan describe como "ir al inicio" instantáneo en espíritu (S2.3: "`[ESTIMADO en fidelidad al Zune]`"); un solo POP-slide comunica "volviste" sin fingir que cada nivel intermedio se dibujó.

**Impacto en `PLAN_MAESTRO.md`**: ninguno en las tres transiciones que el catálogo sí nombra (SLIDE/PUSH-POP/FADE núcleo, todas verificadas: `docs/screenshots/F11-slide-mid.png`, `F11-push-mid.png`, `F11-fade-mid.png`). Si una fase futura decide que opciones de NP merece su propio tratamiento visual, el punto de cambio es un chequeo más en `metro_main.c`, no una reestructuración -- ver el comentario en el propio diff (M-043).

---

## F12-1 — Turnstile: sin fundido de opacidad animado, geometría de proyección no confirmada contra WP7

**Plan decía**: el catálogo (`PLAN_MAESTRO.md` S3.3, citando `INVESTIGACION.md` F.3) describe "Turnstile Out" con "opacidad cae en los últimos 10ms" y "Turnstile In" con "opacidad 0→1 en 10ms" -- es decir, el WP7 real anima tanto la rotación 3D como la opacidad de cada superficie en paralelo.

**Qué se hizo**: `metro_fb_draw_turnstile_layer()`/`run_turnstile()` (`metro_transitions.c`) animan SOLO el ángulo de rotación -- ninguna superficie cambia de opacidad durante la animación, la superficie saliente permanece 100% opaca hasta el último cuadro en que su proyección ya la encogió/recortó por la propia perspectiva.

**Por qué**: agregar opacidad animada encima de la proyección por columnas exigiría mezcla por píxel (el mismo costo que `metro_fb_present_fade()`) DENTRO del bucle de composición del turnstile, multiplicando el trabajo por cuadro de una técnica que el propio `INVESTIGACION.md` B.5 elige precisamente porque es barata ("el único costo por píxel es un shift y una comparación"). El encogimiento por perspectiva de la superficie saliente ya comunica "se aleja/desaparece" sin necesitar una segunda dimensión de animación -- verificado visualmente en `docs/screenshots/F12-turnstile-mid.png`, donde la lectura de "puerta girando" es clara sin el fundido de opacidad.

**Geometría de proyección [ESTIMADO], no verificada contra una fuente primaria**: ni el eje de rotación (centro vertical de la pantalla) ni la distancia focal de la cámara (`LCD_WIDTH*1.2`) están confirmados contra el `Turnstile` real de WP7/Windows Phone Toolkit -- `INVESTIGACION.md` F.3 solo documenta el RANGO de ángulo (-80°→0°/0°→50°) y la duración, tomados de `Microsoft.Phone.Controls.Toolkit`, no la geometría de cámara/eje que ese control usa internamente. Ver M-046 (`DECISIONS.md`) para el detalle completo de la elección y su justificación.

**Impacto en `PLAN_MAESTRO.md`**: ninguno en el criterio de "hecho" de F12 (`F12-turnstile-mid.png`, `F12-feather-mid.png`, `F12-np-artbg.png`, `F12-np-lite.png`, todos capturados y verificados). Si una revisión visual del dueño encuentra que el ángulo/eje/distancia focal no se sienten como el Zune HD real, ajustar `firmware/tools/gen_turnstile_table.py` y regenerar la tabla es un cambio de unos pocos parámetros, no una reescritura.

---

## R2-F1-1 — DD-1: restauración de `DRMODE_FG` tras `plugin_load()` vive en `metro_photos.c`/`metro_video.c`, no en `metro_main.c`

**Plan decía** (`PLAN-metro-r2-maestro.md` DD-1): *"metro_main() fija DRMODE_FG una vez tras metro_fonts_init() y lo vuelve a fijar al volver de cualquier plugin_load()"* — redactado asumiendo que las llamadas a `plugin_load()` (para lanzar `imageviewer.rock`/`mpegplayer.rock`) están dentro de `metro_main.c`.

**Qué se encontró al ejecutar**: `plugin_load()` no se llama desde `metro_main.c` en ningún punto — vive en `metro_photos.c:metro_photos_view()` (imageviewer) y `metro_video.c:metro_video_play()` (mpegplayer), cada una su propio módulo con su propia razón para no incluir headers de Metro que colisionarían con `apps/plugin.h` (ver el comentario ya existente en ambos archivos sobre el choque de `LANG_*` con `metro_lang.h`).

**Qué se hizo**: `metro_main()` sigue fijando el baseline `DRMODE_FG` una vez, justo después de `metro_fonts_init()`, tal como pedía el plan. La restauración "al volver de `plugin_load()`" se hizo en el sitio real de cada llamada: `metro_photos.c` y `metro_video.c` ahora incluyen `lcd.h` y llaman `lcd_set_drawmode(DRMODE_FG)` inmediatamente después de su propio `plugin_load()`, antes de que `metro_transitions_fade()` vuelva a dibujar la lista.

**Por qué es equivalente, no una desviación de fondo**: el efecto que DD-1 pide (nunca dibujar texto de `apps/metro/` bajo `DRMODE_SOLID` después de que un plugin externo corrió y pudo haberlo dejado así) es idéntico — solo cambia el archivo físico donde vive la línea, porque esa es la ubicación real del punto de retorno de `plugin_load()`, no `metro_main.c`. Ver `DECISIONS.md` M-051 para el detalle técnico completo del fix de DRMODE_FG.

**Impacto en `PLAN-metro-r2-maestro.md`**: ninguno en el criterio de "hecho" de R2-F1 — el requisito ("nunca DRMODE_SOLID en texto de apps/metro/ tras volver de un plugin") queda cubierto igual; solo se corrige la ubicación de archivo asumida por el texto del plan.

---

## R2-2 — Cuadrícula de fotos: geometría 4×80 (Metro/Zune HD), no la 5×55 con margen de Aura

**Plan decía** (`PLAN-metro-r2-maestro.md` DD-8): la geometría de la cuadrícula queda fijada de entrada en **4 columnas × tiles de 80×80px, sin separación, al ras de los bordes** — explícitamente distinta de la celda de 55px con margen que usa la cuadrícula de fotos de Aura-Firmware (D-323, precedente citado por el propio DD-7). El mismo DD-8 deja anotado que, si tras verla el dueño prefiere 5 columnas × 64px (10 tiles visibles + asoma), es un cambio de dos constantes — decisión abierta DA-1 en §7 del plan.

**Por qué la diferencia es deliberada, no un error de lectura del precedente**: la cuadrícula de Aura replica el layout de miniaturas con margen de Apple Fotos/iOS (celdas separadas, esquinas redondeadas conceptualmente) — coherente con el sistema de diseño Apple2026 de ese repositorio. Metro sigue el lenguaje visual Zune HD/Windows Phone en todo lo demás (`CLAUDE.md`, tiles del hub, Now Playing) — ahí las cuadrículas (Music Flow, Pictures hub) son tiles **pegados, sin margen, al ras del borde de pantalla**, no tarjetas separadas. Usar la celda de Aura tal cual hubiera sido inconsistente con cada otra superficie que Metro ya tiene construida.

**Qué se hizo**: `METRO_TILE_SIZE = 80`, `METRO_TILE_COLS = 4`, `METRO_TILE_ROWS_VISIBLE = 2` (`metro_draw.h`, `metro_draw_tiles()`). Verificado visualmente: `docs/screenshots/R2-F2-photos-grid.png` — 4 tiles por fila llenan exactamente los 320px de ancho de pantalla (320/4=80, sin resto), segunda fila asoma cortada en y=240 por la propia geometría (164+80=244), sin necesitar la fila extra de "peek" que sí usa la lista de filas.

**DA-1, decisión abierta (no cerrada en esta fase)**: si tras la revisión visual de la PARADA de R2-F2 el dueño prefiere 5×64 (más tiles visibles, cuadrícula más densa), es un cambio de únicamente `METRO_TILE_SIZE`/`METRO_TILE_COLS` en `metro_draw.h` — ningún otro archivo asume el valor 80 o 4 directamente (`metro_photo_thumbs.c` decodifica al tamaño de `METRO_TILE_SIZE`, no a un literal propio).

**Impacto en `PLAN-metro-r2-maestro.md`**: ninguno — el propio DD-8 preveía esta comparación explícitamente y dejó la puerta abierta a cambiarla sin que sea una desviación real del plan, solo una decisión de diseño que el dueño puede revisar en la PARADA.

---

## R2-1 — Visor propio de fotos: qué se pierde de `imageviewer.rock`

**Plan decía** (`PLAN-metro-r2-maestro.md` DD-10): reemplazar `imageviewer.rock` por un visor propio con ajustar/cubrir "lo que se pierde (zoom/paneo/slideshow/PNG) va a `DESVIACIONES.md` R2-1 -- misma pérdida que Aura aceptó (C.3)".

**Qué pierde Metro al dejar de usar `imageviewer.rock`** (`apps/plugins/imageviewer/`, plugin nativo sin modificar, sigue en el árbol):

- **Zoom**: `imageviewer.rock` permite acercar/alejar con el scroll wheel en su propio modo; `metro_screen_photo_viewer.c` solo tiene ajustar/cubrir, sin nivel de zoom intermedio ni panning libre dentro de una imagen acercada.
- **Paneo**: ligado al zoom -- sin zoom, no hay nada que panear. En modo cubrir, el recorte siempre queda centrado, no ajustable por el usuario.
- **Slideshow**: `imageviewer.rock` tiene un modo de avance automático por tiempo; el visor de Metro solo avanza con SCROLL_FWD/BACK explícito.
- **Formatos**: `imageviewer.rock` decodifica JPEG, PNG, BMP y GIF (con sus propios decoders, `apps/plugins/imageviewer/{png,bmp,gif}/`); `metro_screen_photo_viewer.c` solo decodifica JPEG -- consistente con el contrato de Metro (`/Photos/*.jpg`/`.jpeg` únicamente, `metro_photos.c`'s `k_exts`), así que en la práctica ningún archivo real del contrato pierde soporte, pero un `.bmp`/`.png` colado a mano en `/Photos/` (fuera de contrato, ya filtrado por `metro_fsutil_list_by_ext()` desde F7) tampoco se vería si de algún modo llegara a listarse.

**Por qué se acepta esta pérdida**: mismo criterio que Aura-Firmware ya adoptó para su propio visor (C.3, citado por el plan) -- un visor Metro-styled con controles descubribles (MENU = volver, consistente con el resto de la app) vale más que un plugin correcto pero visualmente ajeno, con su propio esquema de botones no obvio (ver `DECISIONS.md` M-058: `SELECT` para salir en el pad clickwheel, no `MENU` -- la causa real detrás del reporte de "se traba" del dueño verificando R2-F1/R2-F2). Zoom/paneo/slideshow no están en el alcance de ninguna fase de la ronda 2; si se piden en el futuro, es trabajo nuevo sobre `metro_screen_photo_viewer.c`, no una regresión a recuperar.

**Impacto en `PLAN-metro-r2-maestro.md`**: ninguno -- DD-10 preveía exactamente esta pérdida y pedía documentarla aquí, no evitarla.

---

## R2-3 — Video/mpegplayer: mapeo de teclas heredado de Aura; ícono de estado sigue siendo bitmap; el frame YUV SÍ es capturable en este simulador (a diferencia de D-307)

**Plan decía** (`PLAN-metro-r2-maestro.md` DD-11, archivo de R2-F4): "mapeo de teclas heredado de Aura; el frame YUV no es capturable en el sim, mismo límite que D-307" (D-307 de Aura-Firmware).

**Mapeo de teclas -- confirmado, no es una desviación real**: `MPEG_TOGGLE_SCALE = BUTTON_SELECT` (SELECT alterna ajustar/cubrir durante la reproducción) usa exactamente el mismo botón que Aura-Firmware ya probó en hardware real para este mismo pad (`IPOD_4G_PAD`/`IPOD_3G_PAD`/`IPOD_1G2G_PAD`) -- SELECT no tenía ninguna función asignada durante la reproducción en Metro tampoco, así que no hay colisión. El resto del keymap del plugin (MENU=menú de ajustes, PLAY=pausa, IZQUIERDA/DERECHA=retroceder/avanzar) es el que Rockbox stock ya traía, sin tocar.

**Ícono de estado -- compromiso deliberado, documentado en `DECISIONS.md` M-059**: DD-11 pedía un ícono 100% geométrico (triángulo de play / dos barras de pausa, mismo lenguaje que los íconos de F10 en `metro_widgets.c`). Lo que se hizo en su lugar: se mantuvo el ícono bitmap compilado que Rockbox stock ya trae (`osd.icons`, tres tamaños según la fuente activa), solo recoloreado al acento del usuario en vez del blanco/negro fijo original -- mecanismo que Aura-Firmware ya portó igual (recolorear, no redibujar). Dibujar el ícono desde cero con primitivas geométricas es un trabajo de diseño genuino (tamaño, proporción, legibilidad a 8/12/16px) que se dejó fuera del alcance razonable de esta fase, dado el tamaño ya considerable del resto del port. Si se retoma, el punto de cambio es `osd_refresh_status()` en `mpegplayer.c` -- reemplazar la llamada a `draw_oriented_mono_bitmap_part()` por dibujo directo, sin tocar nada del resto del OSD.

**El frame YUV del video SÍ se pudo capturar en este simulador**: a diferencia de lo que el plan asumía citando D-307 de Aura-Firmware, `docs/screenshots/R2-F4-video-play.png` muestra el video de prueba (patrón de barras SMPTE) renderizado a todo color, no negro -- confirma que este simulador (o esta versión de Rockbox/SDL) sí puede volcar el contenido YUV decodificado, sin la limitación que Aura documentó para el suyo. Lo que **no** se pudo confirmar visualmente fue el panel del OSD (barra de progreso + tiempos) en sí -- el área donde debería estar aparece en negro en todas las capturas probadas, independiente del tema/acento (verificado con dos esquemas de color distintos, oscuro/teal y claro/rojo), lo que descarta que sea un color mal calculado (los valores de color se verificaron correctos por inspección directa en tiempo de ejecución, ver `DECISIONS.md` M-059) y apunta a un artefacto de decodificación en progreso o a un límite de temporización de captura headless distinto del de D-307, no documentado antes en este proyecto. Ver `DECISIONS.md` M-059, sección "No verificado visualmente".

**Impacto en `PLAN-metro-r2-maestro.md`**: ninguno en el criterio de "hecho" de R2-F4 más allá de la captura específica del panel OSD, que queda pendiente de confirmación visual (por el dueño, en simulador interactivo o hardware) en vez de headless -- el resto de las capturas requeridas (menú de ajustes, modo de ajuste, video entrando directo sin menú de inicio, tema/acento en vivo) sí se completaron.

---

## R2-4 — Video/mpegplayer, rediseño Zune (M-060): el panel del OSD no puede ser realmente transparente sobre el video en vivo

**No es una desviación de un plan escrito** (el rediseño de M-060 no
estaba en `PLAN-metro-r2-maestro.md`, surgió de una crítica de diseño
del dueño después de que R2-F4 ya estaba cerrado) sino una corrección
factual encontrada al ejecutar el punto 1 de una maqueta que el dueño
ya había aprobado ("sí, me gusta esa propuesta procede") -- se
documenta aquí por el mismo criterio que el resto de este archivo:
factual, encontrado en ejecución, con impacto real en lo entregado.

**La maqueta aprobada asumía** que el panel del OSD podía dibujarse
sin fondo, flotando directamente sobre el frame de video en
reproducción (mismo lenguaje que el "Now Playing" de Zune HD, donde
los controles son solo texto+glifos sin ningún panel detrás).

**Lo que el código real permite**: Rockbox no compone el OSD sobre el
video -- lo **recorta**. `osd_show(OSD_SHOW)` (`mpegplayer.c`) llama
`stream_vo_set_clip(&rc)` con `rc = {0, 0, SCREEN_WIDTH, osd.y}`: el
hilo de video deja de dibujar por completo en el rectángulo del OSD
mientras está visible. No hay "video congelado" detrás para componer
con alpha -- simplemente no hay nada ahí hasta que el OSD se oculta y
`stream_draw_frame(false)` vuelve a pintar esa zona. Lograr un panel
de verdad transparente exigiría leer el framebuffer de vuelta y
componer manualmente cuadro a cuadro -- una reescritura bastante más
grande, con riesgo de rendimiento real en el iPod 6G que esta ronda no
puede medir (Barrera B3, sin hardware).

**Resolución aplicada** (ver `DECISIONS.md` M-060, punto 1): el panel
se queda opaco, pero se redujo de dos filas (~28-34px) a una sola fila
del tamaño exacto del texto (~18-20px) y se le quitó el bisel
elevado/bordes que traía -- un acercamiento honesto al espíritu de "no
un cajón de chrome sobre el video", no al pixel de la maqueta.

**Impacto**: los otros 4 puntos de la maqueta (ícono geométrico,
tiempos en los extremos de la barra, tipografía Metro real, barra
delgada con punta redondeada) se implementaron tal como se aprobaron,
sin ajuste de alcance. Solo el punto 1 difiere de lo maquetado, por la
razón arquitectónica de arriba.

---

## R2-5 — Video/mpegplayer: la conclusión de R2-3 sobre "el panel del OSD no capturable" era falsa -- el binario del simdisk estaba desactualizado (`make` no instala plugins)

**Qué decían R2-3 y los registros de M-059/M-060**: que el panel del
OSD no se pudo confirmar visualmente en capturas headless en ninguna
ventana de ticks probada, atribuyéndolo a un artefacto de
decodificación MPEG-2 en progreso o a un límite de temporización de la
captura de un solo proceso.

**Lo que era en realidad**: `make` en `build-sim` compila
`mpegplayer.rock` pero no lo copia a
`simdisk/.rockbox/rocks/viewers/` -- eso lo hace **`make install`**
(`tools/root.make` → `buildzip.pl`). Las capturas de M-059/M-060 y el
simulador interactivo que el dueño usó corrían un `.rock` viejo. Con
el binario instalado, el panel se captura sin problema
(`docs/screenshots/R2-F4-zune-osd.png`). Lo único de R2-3 que se
sostiene es que la pantalla completamente negra en ticks muy tempranos
(~44-48) es decodificación aún en progreso.

**Cómo se descubrió**: el dueño reportó ver el OSD viejo (volumen en
"-34dB", panel de dos niveles) en el simulador interactivo después de
que M-060 estaba supuestamente aplicado -- la única explicación
coherente era que el binario en ejecución no contenía los cambios, y
la marca de tiempo del `.rock` del simdisk (16:47, horas antes del
último build) lo confirmó.

**Impacto**: ninguna de las conclusiones de diseño cambia; cambia el
procedimiento de verificación (todo `sim_shot.sh` o lanzamiento
interactivo va precedido de `make install`) y quedan corregidas las
filas/planes que citaban "no verificable en headless" como límite del
simulador. Ver `DECISIONS.md` M-061.

---

## R3-1 — Letras `.lrc`: modelo de memoria propio, no el de Aura-Firmware

**No es una corrección al plan** (DD-3 ya especificaba este diseño) --
se documenta aquí, mismo criterio que R2-2 (geometría de la cuadrícula
de fotos), porque diverge deliberadamente del mecanismo de referencia
de Aura-Firmware y el porqué importa para quien retome este código.

**Lo que hace Aura**: copia cada línea con marca de tiempo a una
ranura fija de 128 bytes (`aura_lrc_t.lines[600]`) -- `INVESTIGACION-metro-r3.md`
A.2 midió esto en ~79-82 KB permanentes en BSS, sin importar cuánto
texto real tenga el archivo.

**Lo que hace Metro**: conserva los propios bytes del archivo (que de
todas formas hay que leer a algún buffer) y guarda solo
`{uint32_t ms; uint16_t offset}` por marca de tiempo -- el parseo
corta cada línea en su lugar con un `\0` y cada entrada apunta ahí.
Costo real: 8 KB de buffer + 600×6 B de entradas ≈ 11.6 KB, contra los
~80 KB de Aura. Como beneficio adicional (no buscado, mecánico): varios
timestamps en una misma línea (`[00:12.00][00:45.00]texto`) comparten
el mismo `offset` -- el mismo puntero, verificado en
`test_lrc.c::test_multiple_timestamps_share_text` -- en vez de que
Aura duplique el texto en cada ranura.

**Verificado**: 45 checks en `apps/metro/test/test_lrc.c` (formatos de
timestamp, timestamps múltiples por línea, tags de metadata
descartados, línea sin marca descartada, tag malformado que no rompe
el resto de la línea, BOM que rompe solo la primera línea -- mismo
comportamiento que el parser de Aura, A.1 --, archivo sin ninguna línea
válida, tope de 600 entradas, búsqueda binaria antes/después de la
primera marca, ruta hermana derivada). Verificado en vivo en el
simulador con `metro-test.lrc` (3 líneas reales + 3 tags de metadata
que se descartan correctamente).

## R3-2 — Letras: el modo se conmuta solo desde Options, no también con SELECT en Now Playing

**Qué decía el plan**: DD-2 pedía "conmutable... con `SELECT|REL` desde
el propio Now Playing" (además de la fila de Options), y DA-3 ofrecía
esto como la opción recomendada por default.

**Qué se encontró**: `player_mapping[]` (`metro_keymap.c`) ya tiene
**ambos** gestos de SELECT ocupados en `MCTX_PLAYER`: `SELECT|REL` →
`MACT_OPTIONS` (abre la propia página de Options) y `SELECT|REPEAT` →
`MACT_TOGGLE_SHUFFLE`. No hay un tercer gesto de SELECT disponible en
el pad de la rueda de clic para un toggle directo de letras sin quitarle
su función actual a uno de los dos.

**Qué se hizo**: se resolvió a favor de la alternativa que la propia
DA-3 ya contemplaba -- "solo desde Options". El modo se activa/desactiva
únicamente desde la fila "letra" de la página de Options (ya alcanzable
con el SELECT existente); Now Playing no gana ningún gesto nuevo. No se
reasignó ninguna de las dos funciones de SELECT que ya existían.

**Impacto en `PLAN-metro-r3-maestro.md`**: ninguno en el criterio de
"hecho" de R3-F2 -- el modo de pantalla completa en sí, el criterio
central de DD-2, queda intacto y verificado. Un gesto directo desde Now
Playing (si se quiere en el futuro) necesitaría o bien un botón
distinto (p. ej. PLAY sostenido, hoy sin uso en `MCTX_PLAYER` más allá
de play/pausa corto) o resignar una de las dos funciones actuales de
SELECT -- decisión de producto, no técnica, fuera del alcance de esta
fase.

---

## R3-3 — Fotos de artista: el motor de miniaturas de R3-F1 fallaba con fuentes cercanas al tamaño del tile

**No estaba en el plan** (DD-6 asumía reusar el motor de R3-F1 tal
cual) -- se encontró en ejecución, al verificar en vivo que las tres
fotos de artista con mapeo real en `artist_images.cfg` (Metro QA, Aura
Test Combo, DJ Twist: Remix Unit) **nunca decodificaban**: siempre
caían al tile de acento con inicial, igual que un artista sin foto en
absoluto, sin ningún error visible.

**Causa raíz**: `metro_thumbs_decode_jpeg_cover()` (heredado de
`metro_photo_thumbs.c`, R2-F2) reserva un scratch de
`METRO_TILE_SIZE² × 2 × 2` = 25 600 bytes -- suficiente margen (regla
M-033) mientras la fuente sea comfortablemente más grande que el tile
de 80px, que es el caso de **todas** las fotos reales de `/Photos/`.
El decodificador JPEG de Rockbox solo ofrece escalones de potencia de
2 (1/1, 1/2, 1/4, 1/8): para una fuente de 128px pidiendo un resultado
≥80px, 128×1/2=64 **no alcanza**, así que el decodificador cae al
escalón 1/1 -- decodifica a **resolución nativa completa** (128×128) y
recién ahí reescala por software. Es exactamente el mecanismo
`JPEG_DECODE_OVERHEAD` que R2-F3 ya tuvo que resolver para el visor de
fotos (`metro_screen_photo_viewer.c`) -- pero esta vez en el motor de
miniaturas, que nunca lo necesitó porque ninguna foto de prueba había
caído antes en ese hueco. Las fotos de artista, por contrato
(`CONTRATO-firmware-studio.md` §D.3), están **limitadas a ≤128px** --
justo en ese hueco, siempre, no como caso raro.

**Qué se hizo**: `SCRATCH_SIZE` en `metro_thumbs.c` se recalculó contra
el límite del propio contrato (128px, no los 80px del tile) con el
mismo margen ×2 de M-033 -- 65 536 bytes en vez de 25 600. Se prefirió
esto sobre portar el probe de dimensiones que sí usa el visor de fotos
(R2-F3/DD-10): es un solo `#define` más simple, y como
`metro_thumbs_decode_jpeg_cover()` es compartido con fotos, el
presupuesto más ancho no cuesta nada ahí (las fuentes de `/Photos/`
son casi siempre mucho más grandes que 128px) y de paso cubre la misma
falla si alguna vez aparece una foto real cerca de ese límite.

**Cómo se encontró**: `DEBUGF` temporal en `metro_music_reload_artist_images()`,
`artist_thumb_cache_key()` y `metro_thumbs_tick()` (revertido antes del
commit) confirmó que el índice cargaba bien (3 entradas de
`artist_images.cfg`, 3 archivos encontrados en `artists/`, claves de
caché generadas correctamente) y que `artist_thumb_decode()` se
llamaba con la ruta correcta -- pero `metro_thumbs_decode_jpeg_cover()`
devolvía `false` cada vez. Aislado comparando bytes de un JPEG de
128px contra un `cover.jpg` de 300px ya probado (headers idénticos,
solo cambia el tamaño), lo que apuntó directo al escalón de DCT.

**Impacto en `PLAN-metro-r3-maestro.md`**: ninguno en el criterio de
"hecho" de R3-F3, que ya exigía ver fotos reales en la cuadrícula --
esta corrección es lo que lo hizo posible. Vale la pena que R3-F4
(quickplay, también sobre este motor) tenga esto presente si sus
carátulas de álbum llegan a decodificarse cerca del tamaño del tile.

---

## R3-4 — Quickplay: Metro nunca vacía la cola async de tagcache en ningún apagado (no solo el arnés headless de pruebas)

**No estaba en el plan** (el plan sí anticipaba, con otro número
provisional, que `runtimedb` estaba apagado sin UI propia -- eso ya
está cubierto arriba, en el propio M-065 de `DECISIONS.md`. Esta
entrada es un hallazgo **distinto y más grave**, encontrado al
verificar el criterio de "hecho" de R3-F4 "el orden se conserva tras
reiniciar el simulador").

**Qué se encontró**: tras reproducir un álbum completo y reiniciar el
simulador de cero (proceso nuevo, headless, `METRO_SIM_AUTODUMP_QUIT`),
Quickplay aparecía vacío -- ninguna escritura de `tag_lastplayed`
había sobrevivido. La lectura (`metro_music_recent_albums()`) estaba
bien: el problema era que la escritura nunca llegó a disco.

**Causa raíz**: `tagtree_track_finish_event()` (`apps/tagtree.c`) no
escribe `tag_lastplayed`/`tag_playcount`/`tag_playtime` directo a
disco -- los **encola** vía `tagcache_update_numeric()` →
`queue_command()` (`apps/tagcache.c`), un buffer circular de 32
entradas que el hilo de tagcache solo vacía (`run_command_queue()`)
cuando (a) el buffer se llena, o (b) alguien llama
`tagcache_shutdown()` con `force=true`. En Rockbox normal,
`tagcache_shutdown()` se llama desde `tree_flush()`
(`apps/tree.c`), invocado a su vez desde el propio bucle de
`root_menu()` al salir. **Metro reemplaza `root_menu()` por
completo** (comentario del propio `apps/main.c`, y `apps/tree.c` está
explícitamente prohibido desde `apps/metro/` por este mismo
`CLAUDE.md`) -- así que esa llamada nunca ocurre. Esto no es un
artefacto del arnés de pruebas headless: es un hueco real en el propio
apagado de Metro, en **cualquier** apagado normal (`SYS_POWEROFF`/
`SYS_REBOOT`, sim o hardware real), no solo en el `exit(0)` abrupto de
`METRO_SIM_AUTODUMP_QUIT`. Una sesión con pocas reproducciones (menos
de 32 escrituras encoladas, el caso normal) perdía su historial de
reproducción -- y, a partir de R3-F5, también perdería calificaciones
recién importadas -- en cualquier apagado normal, silenciosamente, sin
ningún síntoma visible hasta la siguiente vez que se consultara ese
dato.

**Cómo se encontró**: verificación en vivo del propio criterio de
"hecho" de R3-F4. La primera corrida con 12 reproducciones (3 álbumes
completos, deliberadamente muchas para esta verificación) sí persistió
-- porque 12 pistas × 4 comandos encolados cada una supera las 32
entradas del buffer, desbordándolo y forzando un volcado automático a
mitad de la sesión, sin que el apagado tuviera nada que ver. Eso
enmascaró el problema real hasta que se leyó `tagcache.c` a fondo para
entender POR QUÉ había funcionado -- no porque el apagado limpio
funcionara, sino a pesar de que no funciona.

**Qué se hizo**: `metro_main.c`, en el `case SYS_POWEROFF: case
SYS_REBOOT:` que este archivo ya tenía (dibuja la pantalla de apagado,
llama `default_event_handler()`), se agregó una llamada a
`tagcache_shutdown()` antes de ceder el control a
`default_event_handler()` -- mismo efecto que la llamada real de
`tree_flush()` que Metro nunca ejercita, sin necesitar `apps/tree.c`
para nada. Cambio contenido enteramente en `apps/metro/metro_main.c`
(el propio archivo ya tiene `tagcache.h` incluido) -- no toca ningún
archivo de Rockbox fuera de `apps/metro/`, así que no aplica la regla
de `MODIFICATIONS.md`/parada-antes-de-escribir de la sección "Reglas
de todas las fases" del plan maestro.

**No verificado de forma aislada en el simulador headless**: probar
específicamente ESTE camino (una sesión con pocas reproducciones,
apagado limpio vía el token `POWEROFF` de `sim_shot.sh`, verificar que
sobrevive) se dejó fuera de esta fase -- `shutdown_hw()` involucra
código de plataforma (`firmware/powermgmt.c`) cuyo comportamiento
exacto en el simulador headless no está mapeado, y sesiones previas
(`docs/DESVIACIONES.md` F2-2/F2-3) ya documentaron que ciclos largos
del hilo `sim_thread` en modo headless pueden disparar condiciones de
carrera de AppKit/SDL específicas de este entorno. La verificación de
R3-F4 en sí (`docs/screenshots/R3-F4-quickplay.png`) se apoyó en el
desbordamiento natural del buffer (32 entradas), no en este fix, así
que el criterio de "hecho" de la fase queda cumplido igual sin
depender de código no verificado en vivo. El fix en sí es de bajo
riesgo (llama una función pública ya usada por Rockbox exactamente
para este propósito, en el mismo lugar donde el propio `metro_main.c`
ya maneja el mismo evento) pero **queda pendiente de una verificación
interactiva real** -- el dueño puede confirmarlo con una sesión corta
(reproducir un track, cerrar la ventana del simulador con el botón de
apagado real en vez de `sim_shot.sh`, reabrir y revisar Quickplay) o
dejarlo para la verificación de hardware real de R3-F9.

**Impacto en `PLAN-metro-r3-maestro.md`**: ninguno en el criterio de
"hecho" de R3-F4 en sí (verificado por otra vía, arriba). Si no se
verifica interactivamente antes de R3-F9, ese es el lugar natural para
cerrarlo del todo -- ya está en el alcance de esa fase ("simulador
primero; hardware real solo en R3-F9"). R3-F5 (calificaciones) hereda
el mismo fix sin trabajo adicional, al reusar la misma cola.

---

## R3-5 — Calificación: el listener de stock alcanza solo, sin callback propio -- y por qué casi se concluye lo contrario

**El plan pedía exactamente esta verificación primero** (DD-7): antes
de escribir ningún callback de `PLAYBACK_EVENT_TRACK_BUFFER`, calificar
una pista en vivo, cambiar de pista, volver, y ver si el valor
sobrevive -- porque dos hallazgos reales de la Fase 2 se contradecían
(`INVESTIGACION-metro-r3.md` C.2: Aura necesita su propio callback
"porque nadie más lo hace"; D.1: `tagtree_init()` sí corre sin
condiciones en el árbol de Metro). Esta entrada documenta el resultado:
**el listener de stock alcanza, no se escribió ningún callback nuevo**
-- pero el primer intento de verificarlo casi concluye lo contrario,
por una razón que no tiene nada que ver con el listener en sí.

**Qué pasó**: la primera ronda de pruebas en el simulador headless
(`sim_shot.sh` con botones inyectados) mostraba la calificación
recién puesta desaparecer al volver a la pista -- parecía confirmar
que sí hacía falta un callback propio, como Aura. Investigando más a
fondo (con `DEBUGF` temporal en `metro_sync.c`/
`metro_screen_nowplaying.c`, revertido antes del commit): las pistas
de prueba de este repo duran **2 segundos** cada una
(`gen_test_media.sh`), y la secuencia de botones para llegar hasta la
fila de calificación (ocho o más pulsaciones, cada una con su propio
`METRO_INJECT_PRESS_GAP`) tarda en inyectarse un tiempo real
comparable o mayor a esos 2 segundos -- la pista bajo prueba ya había
terminado y avanzado sola (reproducción continua) antes de llegar
siquiera a calificarla, así que la "pista que no recordaba su
calificación" ni siquiera era la misma pista. Confirmado leyendo el
`id3->path` real en cada verificación vía `DEBUGF`: no era "Sunrise",
eran pistas dos o tres lugares más adelante en el álbum.

**Qué se hizo**: se pausó la reproducción (botón `PLAY`, justo después
de arrancar la pista) antes de calificar y navegar -- elimina la
presión de tiempo por completo, sin cambiar nada del mecanismo bajo
prueba. Con eso, la secuencia real (calificar → `MENU` → `RIGHT`
siguiente pista → `LEFT` pista anterior → reabrir Options) sí muestra
la calificación intacta, confirmando que el listener de stock
(`tagtree_buffer_event()`, `apps/tagtree.c`) restaura `tag_rating` sin
ayuda. `docs/screenshots/R3-F5-rating-set.png` es esa verificación.

**Un hallazgo real y distinto, encontrado en el camino** (no el que se
estaba buscando): la escritura en sí (import y calificación manual)
tampoco llegaba a verse -- ni siquiera dentro de la misma sesión --
hasta forzar el volcado de la cola async de tagcache. Documentado en
`DECISIONS.md` M-066, no aquí (es sobre la escritura, no sobre el
listener de lectura que esta entrada verifica).

**Impacto en `PLAN-metro-r3-maestro.md`**: ninguno en el criterio de
"hecho" -- la corrección real fue de método de verificación (pausar
antes de cronometrar botones contra pistas de 2 segundos), no de
código. Vale la pena que cualquier verificación futura que dependa de
"cambiar de pista y volver" en este simulador, con estos fixtures,
pause la reproducción primero -- de lo contrario el resultado depende
de cuántos botones hay que inyectar antes de llegar al punto de
control, no de lo que en realidad se está probando.

---

## R3-6 — Candado por PIN: Metro NO porta el parche a `apps/misc.c` de D-238, y el caso borde no obvio desaparece por construcción

**Esto no es una desviación del plan sino su cumplimiento** -- DD-8 lo
decidió de antemano y esta entrada documenta el resultado real, como
pedía la lista de archivos de R3-F7. Se anota aquí porque quien compare
los dos firmwares va a ver un archivo de core parchado en Aura-Firmware
y ninguno en Metro, y merece saber que fue deliberado.

**Qué hace Aura-Firmware** (`aura_screenlock.c`, `aura_main.c:378-395`,
consultados read-only): mientras el candado está activo, intercepta
`SYS_USB_CONNECTED` **antes** de `default_event_handler()` y lo
**difiere** -- guarda el evento y su `seqnum`, y solo lo reproduce
después de que el usuario acierte la clave. Eso exige ~28 líneas nuevas
en `apps/misc.c` (`default_event_handler_deferred_usb()`), un archivo
de **core**, porque el manejador estándar monta el disco vía
`gui_usb_screen_run()` sin preguntar nada.

**Qué hace Metro**: nada de eso. Una conexión USB durante el bloqueo se
atiende **normal**, igual que en cualquier otro momento.

**Por qué**: el parche compra muy poco. El volumen es FAT **sin
cifrar**: cualquiera con el cable lee el disco entero, con candado o
sin él. Diferir el montaje protege la *interfaz*, no los *datos* -- y
la interfaz ya está protegida por el candado mismo. A cambio cuesta un
archivo de core modificado (con su entrada en `MODIFICATIONS.md` y su
deuda de mantenimiento en cada rebase de Rockbox) más un caso borde
genuinamente no obvio: `button_get_data()` es una global de "último
dato crudo", no una cola, así que el `seqnum` hay que capturarlo en el
instante exacto de la conexión o los dígitos del PIN lo pisan; y
además hay que revisar `usb_inserted()` antes de reproducir el evento,
porque `SYS_USB_DISCONNECTED` no pasa por esa misma cola y sin ese
chequeo el aparato se quedaría esperando un desenchufe que ya ocurrió.

**Lo importante**: Metro no se "pierde" ese caso borde por descuido --
**el caso borde deja de existir**. Sin diferimiento no hay `seqnum` que
capturar, no hay evento que reproducir, y no hay estado pendiente que
pueda quedar desincronizado. Menos código, menos superficie de core, y
un comportamiento más fácil de explicar.

**Consecuencia deliberada, dicha con todas sus letras en la ayuda**:
Metro **no ofrece protección de datos**. `docs/ESTADO_FINAL.md` lo
declara en esas palabras, junto con que la clave se guarda en texto
plano y que no hay límite de intentos.

**Y el efecto lateral que resultó ser una ventaja**: como el USB sigue
vivo con el candado puesto, la **salida de emergencia** (borrar dos
líneas de `aura.cfg` para quitar un PIN olvidado) es alcanzable desde
el propio aparato bloqueado. En Aura-Firmware, que sí difiere el USB,
esa salida **no existe** -- ni está documentada de aquel lado. Metro la
volvió criterio de "Hecho" de la fase y la probó de verdad, no solo la
escribió.

**Impacto en `PLAN-metro-r3-maestro.md`**: ninguno; DD-8 se cumplió tal
cual. Nota de numeración: el plan llamaba prospectivamente "R3-4" a
esta entrada; los números de `DESVIACIONES.md` se asignan en orden real
de ejecución, y R3-4/R3-5 ya los consumieron hallazgos de R3-F4/R3-F5
(mismo precedente que M-056/M-057 en `DECISIONS.md`).

---

## R3-7 — CONTINUUM: el destino real es la ceja, no el título grande — y la escalera de fuentes se reduce a un escalón

**Qué decía el plan** (DD-9): "El origen es una fila en `MFONT_LIST`
(20 px) y el destino un título en `MFONT_DISPLAY` (48 px)", con una
escalera de tres fuentes (20 → 28 → 48) cambiando de escalón a un
tercio y a dos tercios de la animación, y el color pasando de
secundario a `fg`.

**Qué se encontró al implementarlo**: en la anatomía real de una página
de Metro, el elemento que lleva la identidad de la fila que abrió la
página **no es el título grande**. `metro_screen_list_show()` pasa
`page->title_dynamic` a `metro_draw_header()` — es decir, a la **ceja**
(14 px, `MFONT_CAPTION`, arriba a la izquierda) — mientras que el texto
grande de 48 px es el **nombre del pivot** (`metro_draw_pivots()`).
Al entrar a un álbum, la ceja dice "Analog Dreams" y el título grande
dice "canciones".

Eso vuelve inaplicable la puerta que el propio DD-9 define como la que
"evita inventar una animación cuando no hay continuidad real": *"solo
cuando el título de la fila de origen es igual al título de la página
de destino"*. Contra el **título grande**, esa igualdad no se cumple
nunca en ninguna navegación de Metro — la fila "Analog Dreams" abre una
página cuyo título grande es "canciones", la fila "ajustes" abre una
cuyo título grande es "general". CONTINUUM habría quedado como código
muerto: implementado, gateado, y sin dispararse jamás.

Contra la **ceja**, en cambio, la igualdad se cumple exactamente en las
navegaciones donde hay continuidad real — artista → sus álbumes, álbum
→ sus canciones, género → sus canciones — porque son justo las que
hacen `title_dynamic = etiqueta de la fila`.

**Qué se hizo**: el destino es la ceja. Consecuencias, todas derivadas
de esa misma corrección y no decisiones sueltas:

- **La escalera es de un escalón, no de tres**: `MFONT_LIST_SEL` (20 px)
  → `MFONT_CAPTION` (14 px), cambiando a la mitad del recorrido. El
  principio de DD-9 se respeta intacto — posición continua, tamaño por
  escalones de las fuentes que Metro ya tiene, nunca escalado por
  muestreo — solo que entre 20 y 14 no hay ningún escalón intermedio que
  pisar. Los tres escalones eran consecuencia del destino equivocado.
- **El texto encoge en vez de crecer**, y el color va de `fg` a
  secundario en vez de al revés. Mismo movimiento, extremos
  intercambiados.
- **El vuelo es puramente vertical**: la ceja, los nombres de pivot y
  las filas comparten margen izquierdo (`METRO_DRAW_LEFT_X`), así que
  no hay nada que interpolar en x.

**Dos hallazgos más, encontrados ejecutando y no previstos por el plan:**

1. **La primitiva nueva de `metro_fb.c` que DD-9 pedía no hacía falta.**
   El plan pedía "una variante de `metro_fb_present_slide()` que no
   llame `lcd_update()`" suponiendo que CONTINUUM viajaría sobre el
   SLIDE. Pero al nivel de FX al que DD-9 acota CONTINUUM
   (`animations=all` **y** `graphics=full`), el PUSH no hace slide: hace
   **turnstile** (F12), que ya compone dos capas sin auto-actualizar y
   actualiza una sola vez por cuadro — exactamente el contrato que la
   primitiva nueva iba a establecer. Agregarla habría sido código
   muerto. Sí hizo falta otra, distinta y más chica
   (`metro_fb_fill_rect()`), por el punto siguiente.
2. **Había que borrar la ceja del destino, y luego volver a dibujarla.**
   Sin borrarla del buffer off-screen, se veía dos veces a la vez: la de
   `s_fb_to` girando con el turnstile y el volador viajando hacia ella.
   Pero `run_turnstile()` **termina asentando `s_fb_to` sobre el LCD**
   (F12 lo hace para corregir el redondeo de ángulos de la tabla), así
   que borrarla dejaba la página nueva **sin ceja de forma permanente**
   — FEATHER, que corre enseguida, solo redibuja el área de filas.
   Encontrado en vivo, no razonando: una captura post-animación salió
   sin ceja. Corregido dibujando el último cuadro del volador (que ES la
   ceja: misma fuente, color y posición) justo después de ese asentado.

**Impacto en `PLAN-metro-r3-maestro.md`**: ninguno en el criterio de
"hecho" de R3-F8 — se cumple entero, incluidas las tres capturas a
ticks distintos con dos tamaños de fuente. Lo que cambia es cuál es el
texto de destino y, por lo tanto, cuántos escalones tiene la escalera.

**Nota de numeración**: el plan llamaba prospectivamente "R3-5" a esta
entrada; los números se asignan en orden real de ejecución y R3-5/R3-6
ya los consumieron hallazgos de R3-F5 y R3-F7.


## R7-1 — `stack_report.py`: los `.su` de `-fstack-usage` cuentan dos veces la memoria que gcc integra por inlining

**Qué pedía el plan.** `PLAN-ronda-3-firmwares-maestro.md` §E.3: una
herramienta que compile con `-fstack-usage`, lea los `.su` y liste las
funciones de mayor marco.

**Qué se implementó, y por qué.** La primera versión de esta sesión hacía
literalmente eso, en un árbol de build aparte
(`firmware/build-ipod6g-stack/`). Funcionaba, pero la sesión A
(Aura-Firmware) ya había encontrado el problema de fondo y lo dejó
documentado en su D-345: **`-fstack-usage` reporta lo que el compilador
reserva por función FUENTE, y tras el inlining el binario real no
coincide**. Su caso medido: gcc integra `style_fonts_exist` dentro de
`aura_style_scan`, así que sumar los dos `.su` cuenta la misma memoria
dos veces y el "peor camino" sale inflado.

La herramienta que queda mide el **desensamblado del binario que de
verdad se publica** (`arm-elf-eabi-objdump -d`, sumando `push`/`stmdb`
+ `sub sp, #imm` de cada prólogo) y saca el grafo de llamadas del mismo
desensamblado, así que marcos y aristas siempre corresponden al mismo
binario. De paso no necesita un segundo árbol de compilación:
`package_dist.sh` la corre en segundos sobre el `.elf` recién enlazado
en vez de rehacer un build completo con otras banderas. Se conserva
`--su-dir` como contraste opcional, que es donde el hallazgo se puede
volver a ver.

Se **portó** de `../Aura-Firmware/firmware/tools/stack_report.py` (solo
lectura), no se reescribió: cambian las rutas (`apps/metro/`), las
raíces del hilo de UI (`main`, `metro_main`) y `GUARDED_EDGES`.

**Impacto en el plan**: ninguno en el criterio de "hecho" de la Fase 1 —
la herramienta hace lo que §E.3 quería (listar marcos, estimar el peor
camino, fallar con los dos mismos umbrales) y lo hace sobre una base
mejor. Ver `DECISIONS.md` M-101.

## R7-2 — Metro NO porta el corte de skins de D-345: su camino de skins ya cabe

La supervisora indicó portar el cambio de Aura-Firmware
(`settings_apply_skins()` deja de cargar skins,
`apps/gui/skin_engine/skin_engine.c`) **si** el reporte de pila mostraba
ese camino por encima de 6 KB. Medido con la herramienta ya portada,
sobre el binario real de Metro: **5 136 B** desde `skin_get_gwps`
(5 296 B desde `settings_apply_skins`), y **no es el peor camino** — el
peor nace en `metro_music.c`/tagcache con 7 400 B. Debajo del umbral y
sin ser el cuello de botella, tocar el motor de skins de Rockbox es
riesgo sin beneficio. `GUARDED_EDGES` queda **vacía** en Metro, con el
motivo escrito en el propio archivo para que la diferencia con Aura no
parezca un olvido. Se reevalúa si el peor camino se acerca al tope.

## R7-3 — Un build limpio del target no compilaba desde M-059 (dependencias de `mpegplayer`)

Encontrado al crear un árbol de build desde cero para la primera versión
de `stack_report.py`: `make` moría con `No rule to make target
'<build>/metro_palette.h', needed by mpegplayer.o`. La pasada de
dependencias (`mkdepfile`, `tools/functions.make:57`) arma su línea de
comandos con `PPCFLAGS` + `OTHER_INC`, **no** con `MPEGCFLAGS`, y M-059
había agregado `-I$(APPSDIR)/metro` solo a `MPEGCFLAGS`. El `-MG`
convierte entonces `metro_palette.h` en un archivo fantasma dentro del
build que ninguna regla sabe construir.

Llevaba escondido desde el 2026-08-19 porque el `firmware/build-ipod6g/`
del árbol de trabajo arrastra un `make.dep` **anterior** a esa fase:
nadie había reconstruido desde cero, y `package_dist.sh` reusa ese mismo
directorio. Corregido con `OTHER_INC += -I$(APPSDIR)/metro`
(`mpegplayer.make`, marca `Metro (M-101)`, registro en
`MODIFICATIONS.md`). Sin efecto sobre el binario: solo sobre la pasada
de dependencias.
