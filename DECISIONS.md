# DECISIONS.md — Registro de decisiones técnicas (Metro-Aura)

Bitácora de decisiones tomadas en este repositorio, numeración
**M-001** en adelante. Referencias `D-NNN` apuntan al `DECISIONS.md`/
`DECISIONS-ARCHIVE.md` de `Aura-Firmware`; referencias `ST-NNN` al de
`Aura-Studio`.

⚠️ Esto es una BITÁCORA, no una spec. La fuente de verdad del diseño
vigente es `docs/plans/PLAN_MAESTRO.md` §D (resumen tabulado) y este
archivo (detalle y decisiones nuevas posteriores a la Fase 3); para el
estado del código, el propio código.

---

## M-001 — Commit base: Rockbox `0726ec93517a61f602679ab052b083217ec9c96d`

Mismo commit base que usa `Aura-Firmware`. **Por qué**: el toolchain, el
simulador SDL y todos los fixes de hardware que se portan en F0 ya
están probados contra ese commit exacto en este entorno; un commit
upstream más nuevo introduce una variable no probada sin beneficio
concreto documentado (`docs/plans/INVESTIGACION.md` D.7).

## M-002 — Toolchain propio, no compartido con Aura-Firmware

`firmware/toolchain/` se compila localmente con `firmware/tools/build_toolchain.sh`
(mismo procedimiento que D-032 de Aura-Firmware). Nunca se referencia
por ruta relativa un toolchain de otro checkout — regla de contención
de la carpeta padre (`Aura/CLAUDE.md`).

## M-003 — Rutas en disco: se conserva el prefijo `.rockbox/aura/`

Metro-Aura escribe y lee `.rockbox/aura/aura.cfg`,
`.rockbox/aura/sync_summary.cfg`, `/.aura/sync-pending.json`, etc. —
los mismos nombres de ruta que usa Aura-Firmware. **Por qué**: es la
única señal que `AuraDeviceProbe.swift` de Aura Studio usa para
reconocer un dispositivo compatible (existencia de `.rockbox/aura/` +
`aura.cfg`) — no depende del nombre visible del firmware. Cambiar el
prefijo rompería la detección automática sin ninguna ganancia
(`docs/plans/INVESTIGACION.md` E.2).

## M-004 — Distribución propia; riesgo de `AuraUpdateChecker` documentado, no resuelto

Metro-Aura publica sus propios releases (tag, `rockbox.ipod`,
`rockbox.zip`, `bootloader-ipod6g.ipod`, `mks5lboot`, `checksums.txt`)
e instalación manual. Escribe la clave `firmware_family: metro` en
`aura.cfg` — clave propia de Metro, sin equivalente en el contrato de
Aura-Firmware/Aura-Studio.

**Riesgo abierto, no resuelto en este proyecto**: si el usuario tiene
Aura Studio instalado con el `rockbox.ipod` de Aura embebido y conecta
un iPod con Metro, `AuraUpdateChecker.swift` comparará hashes, no
coincidirán, y ofrecerá "actualizar" — lo que sobrescribiría Metro con
Aura si el usuario acepta. Corregir esto requiere que `Aura-Studio/`
aprenda a leer `firmware_family` y no ofrezca esa acción — trabajo en
otro repositorio, fuera del alcance de este proyecto (regla de
contención de la carpeta padre). Documentado como advertencia en
`docs/ESTADO_FINAL.md`.

**Actualización R2-F1/DD-6 (M-056, no se borra esta entrada)**: la
frase original decía que Metro "no escribe `.rockbox/aura/version.txt`
(evita aparentar ser una versión de Aura)". Eso cambió --
`package_dist.sh --release-tag` ahora SÍ lo escribe (ver M-056),
porque con `PLAN-metro-r2-maestro.md` DD-13 Aura Studio va a distinguir
familias por el contenido real de `firmware_family` en vez de por la
mera presencia de `version.txt` -- una vez que eso exista (`ST-045`,
Aura-Studio, fuera de esta sesión), `version.txt` deja de "aparentar
ser Aura" y pasa a ser el único canal para que Studio sepa qué versión
de **Metro** hay instalada. El riesgo de este M-004 **sigue vigente
sin cambios para cualquier versión de Aura Studio anterior a ST-045**
-- esas versiones no saben leer `firmware_family` y seguirán
comparando por hash igual que hoy.

## M-005 — Marcador de sincronización desde el día uno

Se portan `metro_sync.c`/`metro_sync_marker.c` (adaptados de
`aura_sync.c`/`aura_sync_marker.c` de Aura-Firmware) en F6, no se
depende del fallback de Aura Studio (borrar `.tcd` y reconstruir
completo). `aura.cfg` anuncia `sync_marker_supported: 1` desde F6.

## M-006 — Toda la UI en C propio, bucle propio reemplaza `root_menu()`

`apps/main.c` llama a `metro_main()` en vez de `root_menu()`, una sola
vez, sin retorno — mismo patrón que D-001/D-014/D-015 de Aura-Firmware.
`root_menu()`, el skin engine, `gui_synclist`, `do_menu()`, `tree.c`,
`tagtree.c` quedan compilados pero inalcanzables. **Por qué**: el skin
engine no tiene primitivas de animación ni de movimiento horizontal
(`docs/plans/INVESTIGACION.md` A.4); `gui_synclist` tiene altura de
fila fija y sin soporte de pivots (A.2).

## M-007 — Input vía `get_custom_action()` con keymap propio, no lectura cruda

A diferencia de Aura-Firmware (que lee botones crudos con
`button_get()`), Metro-Aura usa `get_custom_action(ctx, timeout, ...)`
con tablas `button_mapping[]` propias (`apps/metro/metro_keymap.c`).
**Por qué**: el filtrado de `BUTTON_REL` espurio ya lo resuelve
`action.c` con el patrón "prereq button" — evita reimplementar a mano
el fix que Aura tuvo que escribir (`next_button()`,
`docs/plans/INVESTIGACION.md` C.3/C.4); las tablas declarativas
encajan mejor con la jerarquía de profundidad del twist.

## M-008 — Piezo de clic apagado por defecto

`global_settings.keyclick` queda en su valor de fábrica (0 = off), sin
ajuste expuesto en la UI v1. **Por qué**: el pad del Zune 30 no tenía
el mismo feedback táctil-sonoro que el clickwheel del iPod
(`docs/plans/INVESTIGACION.md` F.8); además es la consecuencia directa
de usar `get_custom_action()` con `keyclick=0` (C.7).

## M-009 — Idioma: tabla propia bilingüe ES/EN, español por defecto

`apps/metro/metro_lang.c`, sin el sistema `.lang` de Rockbox — mismo
mecanismo que `aura_lang.c` de Aura-Firmware (D-013). Ajuste "Idioma"
en Ajustes generales.

## M-010 — Tipografía: Selawik (SIL OFL), 5 roles, carga completa

Fuente: Microsoft Selawik, licencia SIL Open Font License 1.1 —
redistribuible, sin restricción, cero material Zegoe/Segoe en el
árbol. Vendoreada en `firmware/assets/fonts-src/`, convertida con
`tools/convttf` por `firmware/tools/gen_fonts.sh` a
`firmware/assets/fonts/metro-*.fnt` (versionados).

Roles: `display` (Light 48px), `title` (Light 28px), `list` (Regular
20px), `list_sel` (Semibold 20px), `caption` (Regular 14px). Rango de
caracteres 0x20–0x17F (latín básico + Latin-1 + Latin Extended-A).

Carga con `font_load_ex(path, 0, N)` (N ≥ número de glifos de la
fuente) — carga **completa** desde buflib, sin modo caché. **Nota
sobre una corrección de la Fase 2**: la investigación original (Fase
2, `INVESTIGACION.md` A.7) concluyó erróneamente que había un límite
duro de 10 KB por fuente (`MAX_FONT_SIZE`) por asumir que `MEMORYSIZE`
no estaba definido para `ipod6g`. Verificado en la Fase 3: `MEMORYSIZE=64`
sí se inyecta vía `-D` desde el `Makefile` que genera `tools/configure`
(`--ram=$(MEMORYSIZE)`), así que `MAX_FONT_SIZE=60000` y no hay tope de
10 KB — ver la errata explícita en `docs/plans/INVESTIGACION.md` (A.7,
G.3). Selawik Light a 64px pesa 284 KB en disco, ~1 950 B/glifo — carga
completa sin problema sobre 64 MB de RAM.

Flujo opcional "el usuario aporta Zegoe/Segoe": documentado en
`docs/guia-desarrollo.md` — mismos parámetros de `convttf`, mismos
nombres de archivo, colocados en `.rockbox/fonts/`.

## M-011 — `MAXUSERFONTS` se queda en 12 (valor de upstream, sin tocar)

Metro necesita 5 slots de fuente; el valor de upstream (12) alcanza de
sobra. `firmware/export/font.h` no se modifica — menos diff contra
upstream que Aura-Firmware (que lo subió a 14 para sus 12+2 estilos).

## M-012 — Paleta: base oscura por defecto, 10 acentos WP7, magenta por defecto

Ver `docs/plans/PLAN_MAESTRO.md` §1.4 para la especificación completa
y `docs/plans/INVESTIGACION.md` F.5 para la tabla de conversión a
RGB565 (error de cuantización ≤ 1% en los 10 acentos). Acento por
defecto: magenta `#FF0097`.

## M-013 — Buffers de transición: 2 framebuffers estáticos en BSS, 150 KB c/u

Mismo patrón verificado en `aura_transitions.c` de Aura-Firmware
(`docs/plans/INVESTIGACION.md` B.3) — sin alocación dinámica.

## M-014 — Presupuesto de animación: 30 fps nominal, transiciones ≤ 300 ms, boost por transición

Ver `docs/plans/PLAN_MAESTRO.md` §3.2. Cifras derivadas
(`docs/plans/INVESTIGACION.md` B.2), no medidas en hardware real hasta
F13 — sujetas a ajuste.

## M-015 — Niveles de FX: matriz de 2 ejes (`animations` × `graphics`), con auto-degradación por sesión

Mismo sistema que Aura-Firmware (`docs/plans/INVESTIGACION.md` B.10),
persistido en `aura.cfg`. Umbral de auto-degradación (> 2× presupuesto
en 3 mediciones consecutivas) es una estimación inicial, ajustable con
datos reales de F13.

## M-016 — Video/fotos con plugins stock en v1; visor propio y modo "cubrir" al backlog

`mpegplayer.rock`/`imageviewer.rock` sin modificar en v1 (con
`plugin_set_silent_open_errors(true)`). El modo "cubrir pantalla" y un
visor de fotos propio quedan en el backlog de
`docs/plans/PLAN_MAESTRO.md`.

## M-017 — Ajustes: `global_settings` de Rockbox para lo genérico, `aura.cfg` para lo propio de Metro

Ver tabla completa de claves en `docs/plans/PLAN_MAESTRO.md` §1.2
(`metro_settings.c`) y §4 (checklist de compatibilidad).

## M-018 — Iconografía mínima compilada en el binario, nunca leída de disco por cuadro

Ver `docs/plans/INVESTIGACION.md` A.11/B.10 (regla de oro: nunca leer
bitmaps de disco dentro de un bucle de animación).

## M-019 — Higiene de `global_settings` al primer arranque de `metro_main()`

`statusbar=STATUSBAR_OFF`, `backdrop_file="-"`, `show_shutdown_message=false`,
`talk_menu=false`, `clear_settings_on_hold=false`, `usb_hid=false`
(bajo `USB_ENABLE_HID`), `tagcache_ram=true`, `keyclick=0` (M-008).

## M-020 — Nombre visible "Metro"; cero material Microsoft en pantalla o en el árbol

Wordmark "metro" en minúsculas (Selawik Light). Cero logotipos o
nombres de Zune/Windows Phone/Zegoe/Segoe en la UI, en el árbol de
código, ni en el `.zip` distribuido. Documentación de proyecto usa
"Metro-Aura"; la UI del dispositivo usa "Metro".

---

## Decisiones de la Fase 4 (ejecución)

### M-021 — Fase Cero: se agrega `tools/configure` a los archivos portados (no eran 9, son 10)

Ver `docs/DESVIACIONES.md` F0-1. `tools/configure` tiene una
modificación real de Aura-Firmware (D-007, detección de gcc de
Homebrew con fallback) que no estaba documentada en la lista de 27
archivos de `MODIFICATIONS.md` de ese repositorio. Se verificó con
`diff -rq` completo del árbol de Aura-Firmware contra una copia limpia
del upstream antes de sembrar este repositorio.

### M-022 — Fase Cero: los archivos portados de Aura-Firmware se copian byte-idénticos, sin reescribir su atribución GPL inline

Ver `docs/DESVIACIONES.md` F0-2. Los comentarios `Aura (D-NNN)` dentro
de los 10 archivos portados **no** se reescriben a `Metro (from Aura
D-NNN)` — son avisos de modificación GPL v2 §2a que documentan quién
hizo el cambio real (Aura-Firmware). La atribución de que además
llegaron a Metro-Aura por copia vive en `MODIFICATIONS.md`, no en el
código. Única excepción: `uisimulator/common/sim_tasks.c` sí se
modificó (variables de entorno `AURA_SIM_*` → `METRO_SIM_*`, son
identificadores de proyecto, no aviso legal).

### M-023 — F1: la higiene M-019 corre dentro de `init()` (apps/main.c), no al principio de `metro_main()`

Ver `docs/DESVIACIONES.md` F1-1. El backdrop Cabbie v2 stock se pinta
sobre el LCD en `settings_apply_skins()`, llamado dentro de `init()`
**antes** de que `metro_main()` corra — esperar a `metro_main()` para
desactivar `global_settings.backdrop_file` es demasiado tarde, el
backdrop ya quedó activo y `lcd_clear_display()` lo redibuja en cada
limpieza de pantalla (comportamiento nativo de
`lcd_clear_viewport()`, no un bug). `metro_apply_hygiene()` (declarada
en `metro_main.h`, ya no `static`) se llama ahora desde ambos cuerpos
de `init()` en `apps/main.c`, justo después de `settings_load()` —
mismo punto exacto que usa Aura-Firmware.

### M-024 — F2: `metro_draw_rows()`/`draw_pivots()`/`draw_tile()`/`draw_progress()` se difieren a F3

Ver `docs/DESVIACIONES.md` F2-1. Esas cuatro primitivas operan sobre
tipos (`struct metro_page`/`metro_pivot`/`metro_row`) que el propio
plan define recién en F3 — escribirlas antes exigía inventar firmas
provisionales. `metro_draw.c` de F2 solo trae lo que su propio
criterio de "hecho" necesita: `clear`, `text`, `text_cut_right`,
`header`, `battery`.

### M-025 — F2: captura visual de `F2-type-specimen.png` no lograda en esta sesión (limitación de entorno, no de código) — **SUPERADA por M-027: el diagnóstico era incorrecto, SÍ era código de Metro**

Ver `docs/DESVIACIONES.md` F2-2 — hallazgo importante para toda fase
futura que dependa de `sim_shot.sh` con `metro_main()` corriendo su
loop de botones. Causa raíz identificada con alta confianza: falla de
AppKit (`nextEventMatchingMask should only be called from the Main
Thread!`) al bombear eventos SDL desde el hilo del "dispositivo"
(`HAVE_SDL_THREADS`, activo por el propio `FIXME` ya presente en
`tools/configure` para macOS ≥ 26.4 — este Mac corre 26.5.2)
concurrente con el hilo de volcado headless (`uisimulator/common/sim_tasks.c`).
Determinista para cualquier `METRO_SIM_AUTODUMP_TICKS` ≥ 30 en esta
sesión; sin ventana segura posible porque el propio arranque de
Rockbox (`show_logo_boot()`) ya ocupa los primeros ~100 ticks.
Verificado por 6+ técnicas distintas sin éxito (detalle completo en
`docs/DESVIACIONES.md` F2-2). No se alteró `metro_main()` para
maquillar el síntoma (un `sleep()` fijo antes del loop degradaría la
responsividad real del producto). Verificación de F2 se apoyó en el
log `DEBUGF` de carga de fuentes (evidencia sólida y completa) más
compilación limpia para simulador y target real. **Pendiente**: el
dueño debe intentar la captura en una sesión interactiva propia (no en
segundo plano) — si el problema persiste ahí, amerita una fase de
investigación dedicada antes de F3, dado que F3+ depende de captura
con botones inyectados.

**Actualización — confirmado en sesión interactiva**: el dueño
reprodujo el mismo crash corriendo `sim_shot.sh` directamente en
Terminal.app. No es un artefacto de sesión en segundo plano — es un
problema real de este entorno (macOS 26.5.2 + SDL3 + Rockbox).

### M-026 — F2: intento de mover `screen_dump()` al hilo de `metro_main()`, revertido (cambia el crash por un deadlock) — **SUPERADA por M-027 (misma causa raíz equivocada)**

Ver `docs/DESVIACIONES.md` F2-3. Se separó el disparo del volcado
(`sim_thread`, sin cambios) de su ejecución real (`screen_dump()`/
`exit()`, movidos a una función nueva llamada desde el propio loop de
`metro_main()`, el único hilo que ya pasa de forma segura por
`button_get_w_tmo()`). Eliminó el crash de AppKit, pero introdujo un
**deadlock nuevo, 100% determinista** — el volcado queda colgado a
mitad del bucle de copia de píxeles del framebuffer (antes de la
primera escritura de línea), incluso en los ticks bajos que antes sí
funcionaban. Diagnosticado con instrumentación `DEBUGF` temporal
(revertida) hasta ubicar el punto exacto del cuelgue; descarta que sea
el `exit()` (el mismo cuelgue ocurre sin `METRO_SIM_AUTODUMP_QUIT`).
**Revertido por completo** (`git checkout --` sobre los 4 archivos:
`uisimulator/common/sim_tasks.c`/`.h`, `apps/metro/metro_main.c`,
`firmware/screendump.c`) — un deadlock del 100% de las veces es
estrictamente peor que un crash parcial. El árbol de F2 quedó
exactamente como en su commit, ningún cambio de este intento llegó a
comitearse.

**Pista para quien retome esto**: hay una segunda condición de carrera
independiente de la de M-025, en la capa de filesystem simulado de
Rockbox (`firmware/target/hosted/filesystem-unix.c`, sin tocar por
Metro ni Aura) — se dispara específicamente al llamar `screen_dump()`
desde el hilo "device" en vez de `sim_thread`. Una solución real
necesita depurar ambos hilos con `lldb`/Instruments en una sesión
interactiva (no disponible en esta sesión por falta de autorización
de macOS), no solo resolver el problema de AppKit de M-025.

### M-027 — F2: causa real del crash de captura — `struct viewport` sin inicializar en `metro_draw_text_cut_right()`; M-025/M-026 quedan superadas

Ver `docs/DESVIACIONES.md` F2-4 (detalle completo, evidencia y
lección metodológica). Resumen: el simulador de **Aura-Firmware**
captura sin problema en esta misma máquina con el mismo entorno — el
problema era de Metro. `metro_draw_text_cut_right()` declaraba un
`struct viewport` en la pila sin inicializar y llamaba
`viewport_set_fullscreen()` directamente; `lcd_init_viewport()` lee
`vp->buffer` (basura) antes de que nadie lo asigne y lo desreferencia/
escribe a través de él (UB) — corrompiendo el estado del LCD que
`screen_dump()` usa después vía `FBADDR()` → `buffer->get_address_fn`
(llamada indirecta que saltó a código arbitrario: a `metro_main`, cuyo
`button_get_w_tmo()` hace `SDL_PumpEvents()` en el hilo llamante por
diseño en `__APPLE__`, de ahí la excepción de AppKit desde
`sim_thread`). **Arreglo**: `viewport_set_defaults()` (idioma de
Rockbox, pone `buffer = NULL` primero). Verificado: captura
determinista a 100/200/300 ticks y con botones inyectados; simulador y
target compilan limpio. **Regla nueva para `apps/metro/`**: todo
`struct viewport` local se inicializa con `viewport_set_defaults()`
(o `memset` 0 + `viewport_set_fullscreen()`), nunca con
`viewport_set_fullscreen()` a secas. Se añade a `CLAUDE.md`.

### M-028 — F2: `gen_fonts.sh` sin `-x` (trim horizontal) — recortaba el glifo del espacio a ~1 px

Detectado en la primera captura real de F2: el texto aparecía sin
espacios ("title28px"). `convttf -x` recorta hasta 2 px por lado de
todo glifo "casi vacío", incluido el espacio (0x20): a 20 px pasa de
~5 px a ~1 px. Aura-Firmware no usa `-x` (`design-system/generate.py`
invoca solo `-p <size>`). Quitado; las 5 fuentes se regeneraron y se
versionaron de nuevo. M-010 sigue vigente en todo lo demás.

## M-029 — F3: `get_custom_action()` con contexto propio exige el bit `CONTEXT_PLUGIN`

Detalle load-bearing de M-007, no obvio por la documentación de
`apps/action.h`: `action_code_lookup()` (`apps/action.c`) solo llama al
callback `get_context_map` que se le pasa a `get_custom_action()`
**si** `context & CONTEXT_PLUGIN` es cierto — si no, usa
`get_context_mapping(context)` (la tabla interna del core, que no
conoce `MCTX_HUB`/`MCTX_LIST`/`MCTX_DIALOG` y devolvería `NULL`).
`metro_input_next()` siempre llama
`get_custom_action((int)ctx | CONTEXT_PLUGIN, ...)`; las tablas de
`metro_keymap.c` terminan con `LAST_ITEM_IN_LIST` (no la variante
`__NEXTLIST`) para que un botón sin mapear resuelva a `ACTION_NONE` en
vez de caer a `CONTEXT_STD` — el espacio de acciones de Metro
(`MACT_*`, arrancando en `LAST_ACTION_PLACEHOLDER+1`) es disjunto del
del core, no hay nada útil a lo que encadenar.

## M-030 — F4: `config.h` antes que `tagcache.h` en cualquier archivo nuevo que use tagcache

`tagcache.h` comprueba `#ifdef HAVE_TAGCACHE` antes de incluir `config.h`
él mismo (esa macro la define `config.h` vía `config/ipod6g.h`). Si
`tagcache.h` es el primer header de un archivo, `HAVE_TAGCACHE` todavía
no existe y todo el contenido del header desaparece en silencio -- el
archivo compila "bien" pero cada función de tagcache queda sin
declarar, con errores de compilación confusos y lejos de la causa real.
Mismo gotcha ya documentado por Aura-Firmware (su D-021); `metro_music.c`
lo replica con `#include "config.h"` primero. Cualquier archivo nuevo de
`apps/metro/` que use tagcache respeta este orden.

## M-031 — `metro_page.h`: `title_dynamic` para encabezados que nombran datos del usuario

Extensión de M-009/F3-1 necesaria en F4 (detalle completo en
`docs/DESVIACIONES.md` F4-1): `struct metro_page.title` sigue siendo
`enum metro_lang_id` (cadena de UI, resuelta en tiempo de dibujo), pero
una página cuyo encabezado nombra un artista/álbum/género real de la
biblioteca del usuario no puede usar ese campo -- no es una cadena de
UI traducible. `title_dynamic` (un `const char *`, NULL por defecto)
la sustituye cuando no es NULL. Cualquier pantalla futura cuyo
encabezado necesite mostrar un dato del usuario (F7 fotos/videos por
categoría, etc.) usa este mismo mecanismo en vez de inventar uno
nuevo.

## M-032 — F4: `metro_music_db_ready()` también dispara `tagcache_start_scan()` sobre una base ya usable

Encontrado verificando C8 (`docs/COMPAT_STUDIO.md`): una base de
tagcache construida en una sesión anterior del simulador no se enteraba
de pistas agregadas después (mismo bug real que Aura-Firmware documentó
en D-206: "archivos copiados por USB, biblioteca vacía en el
aparato") -- Rockbox solo refresca la base desde la pantalla
"Base de datos > Actualizar ahora" del navegador de carpetas, que
Metro no tiene por diseño. `metro_music_db_ready()` ahora dispara
`tagcache_start_scan()` una vez por arranque cuando la base YA es
usable (además de `tagcache_rebuild()` para el caso de base
inexistente, ya presente desde el primer commit de F4) -- mismo patrón
que `aura_music_db_ready()`, con el mismo cuidado de esperar a
`tagcache_is_fully_initialized()` antes de decidir (D-206: `is_usable()`
puede volverse verdadero antes de que la determinación de fondo
termine).

## M-033 — F5: el buffer de decodificación JPEG necesita margen real sobre el tamaño final, no solo `width*height*sizeof(fb_data)`

Bug real encontrado verificando la carátula de Now Playing: con
`s_scratch` dimensionado exactamente a `METRO_ALBUMART_SIZE^2 *
sizeof(fb_data)` (136×136×2 = 36 992 bytes), `read_jpeg_file()` con
`FORMAT_RESIZE` escribía **fuera de los límites** de `s_scratch` antes
de que su propio chequeo de tamaño lo detectara, corrompiendo la
siguiente variable estática del binario (`s_vol_overlay_until` en
`metro_screen_nowplaying.c`, un `long` de 8 bytes, terminó con un
valor de 51 539 607 555 — ni cero ni nada relacionado con volumen).
`JPEG_DECODE_OVERHEAD` (`apps/recorder/jpeg_load.h`) es ~39 KB por sí
solo, sin contar el espacio real que pide el downscale intermedio
antes de reducir a 136×136. Mismo hallazgo y misma fórmula que
Aura-Firmware ya documentó en `aura_albumart.c` (margen `x2` sobre el
tamaño final en bytes) tras un bug parecido (imagen fallando en
silencio, no corrompiendo memoria — la variante de Metro fue peor
porque el buffer nunca antes se había ejercitado con una carátula
real). `metro_albumart.c` reserva `METRO_ALBUMART_SIZE^2 * 2 * 2`
bytes ahora. Cualquier decodificador de imagen nuevo en `apps/metro/`
reserva scratch con el mismo margen, nunca el tamaño final a secas.

## M-034 — `firmware/tools/gen_test_media.sh`: las carátulas de prueba se generan vía PNG + `sips`, nunca `ffmpeg` directo a `.jpg`

Bug real encontrado en la misma verificación: incluso con `-pix_fmt
yuvj420p` (D-030, ya documentado), el encoder `mjpeg` de `ffmpeg`
produce archivos que el decodificador JPEG de Rockbox lee "con éxito"
(`read_jpeg_file()` devuelve `>0`, dimensiones correctas) pero **sin
color** — la crominancia se pierde y el resultado sale gris plano (el
nivel de gris coincide con la luminancia real del color pedido,
confirmado a mano: `RGB(202,119,51)` esperado, `RGB(139,137,139)`
decodificado — casi exactamente el luma de ese naranja). Verificado
que el archivo fuente SÍ tiene el color correcto (`sips -g all`,
Pillow) — el defecto está específicamente en cómo Rockbox interpreta
el JPEG que produce el encoder `mjpeg` de `ffmpeg`, no en el color de
origen. La misma imagen recodificada con Pillow o con `sips -s format
jpeg` decodifica perfecto. `gen_test_media.sh` genera ahora toda
carátula de prueba como PNG (sin subsampling, formato sin pérdida) y
la convierte a `.jpg` con `sips` — nunca `ffmpeg -f lavfi ... .jpg`
directo. `firmware/tools/gen_fonts.sh`/`gen_logo.py` no generan JPEG,
no les aplica.

## M-035 — `playlist_get_track_info()` devuelve 0 en éxito / -1 en error, no un booleano

Bug real encontrado armando la cola de "próximas" de Now Playing:
`if (!playlist_get_track_info(...))` se salta silenciosamente **cada
pista resuelta con éxito** (que devuelve `0`, falsy en C) y solo
"acierta" en el caso de error (`-1`, truthy) — exactamente al revés de
la intención. A diferencia de `tagcache_search()`/`find_albumart()`
(booleanos de verdad, `true`=éxito), esta función usa una convención
estilo `errno`: comparar explícitamente contra `!= 0`, nunca negar el
resultado directamente. Cualquier código nuevo en `apps/metro/` que
llame `playlist_get_track_info()` sigue esta misma comparación.

## M-036 — F6: `metro_music_db_ready()` cede mientras `metro_sync_job_active()`

Con `metro_sync.c` existiendo desde F6, hay dos caminos que podrían
llamar `tagcache_rebuild()`/`tagcache_update()` sobre la misma base:
el disparado por un marcador real de Aura Studio (`metro_sync.c`) y el
disparo de emergencia de `metro_music_db_ready()` (M-032, para cuando
nunca hubo marcador -- música copiada a mano por USB). Sin la guarda,
ambos podrían competir por la base en el mismo arranque. Regla: mientras
`metro_sync_job_active()` es verdadero, `metro_music_db_ready()` solo
devuelve `tagcache_is_usable()` sin tocar tagcache -- mismo criterio
que `aura_music_db_ready()` ya usa (`if (aura_sync_job_active()) return
tagcache_is_usable();`).

## M-037 — F9: gancho de una línea en `apps/gui/splash.c` para traducir mensajes que Metro no genera

Puerto directo del mecanismo de Aura-Firmware (`aura_splash_lang.c`,
D-055 en ese repo): `splash()`/`splashf()` del core de Rockbox son el
único punto por el que pasan los mensajes de tagcache ("Committing
database..."), de carga de playlist/plugin ("Loading..."), y de
apagado por batería baja — ninguno de esos call sites vive en
`apps/metro/`, así que traducirlos uno por uno significaría tocar
varios archivos del core. En cambio, `metro_splash_translate()`
(`apps/metro/metro_splash_lang.c`, código nuevo) reescribe el mensaje
YA resuelto (con sus argumentos dinámicos ya sustituidos por
`vsnprintf()`) contra una tabla de reglas conocidas, justo antes del
ajuste de línea de `splash()` — un solo gancho en `apps/gui/splash.c`
cubre los cuatro. Ver `MODIFICATIONS.md` F9.

## M-038 — F9: la pantalla USB de Metro no llega a verse en el simulador; `gui_usb_screen_run()` sigue dueño de la pantalla mientras dura la conexión

`default_event_handler()` (`apps/misc.c`) llama a `gui_usb_screen_run()`
(la pantalla USB nativa de Rockbox, `apps/gui/usb_screen.c`) de forma
bloqueante hasta que se desconecta el cable -- Metro no puede
interceptar ese bloqueo sin reimplementar el propio manejo de
almacenamiento masivo/HID, fuera de alcance y de riesgo real en
hardware. `metro_screen_usb_show()` dibuja un cuadro con wordmark +
"conectado" ANTES de llamar a `default_event_handler()` -- pero
verificado con el token `USB_INSERT` (M-039), `gui_usb_screen_run()`
hace su propio `lcd_update()` de inmediato, sin ceder el hilo, así que
en el simulador el cuadro de Metro nunca llega a mostrarse ni un solo
frame (`docs/screenshots/F9-usb.png` ya muestra el ícono nativo de
Rockbox). Se conserva de todas formas por ser inofensivo y porque
[ESTIMADO] podría verse brevemente en hardware real, donde el refresco
del LCD físico no es instantáneo -- sin confirmar, requiere el
dispositivo. Alcance más limitado que lo que Aura-Firmware documentó
para la misma superficie (`docs/superficies-rockbox.md` de ese repo,
fila "Pantalla USB": 🟡, con logo+wordmark SÍ visibles ahí porque
Aura modificó `usb_screen.c` directamente en vez de dibujar antes de
llamarlo). Ver `docs/DESVIACIONES.md` F9-1.

## M-039 — F9: token `USB_INSERT` en `METRO_SIM_BUTTONS` (`sim_tasks.c`)

Necesario para capturar `metro_screen_usb.c` de forma headless (mismo
mecanismo que toda otra captura de F0-F8): `METRO_SIM_BUTTONS` ya
soportaba botones reales y el token `WAIT`; agregar `USB_INSERT` que
llama `sim_trigger_usb(true)` directamente (declarado en
`sim_tasks.h`, ya usado por el menú interactivo del simulador) evita
inventar una variable de entorno nueva (`PLAN_MAESTRO.md` F9 proponía
`METRO_SIM_FORCE_USB=1`) cuando el mecanismo de inyección existente ya
alcanzaba con un token más. Ver `MODIFICATIONS.md` F9.

## M-040 — F10: la letra flotante del índice no se puede disparar por botones inyectados en el simulador headless

`metro_screen_list.c`'s índice flotante (S1.4) exige `steps >= 3`,
donde `steps` sale de `button_apply_acceleration(get_action_data())`
(`firmware/drivers/button.c`) -- una función que solo devuelve más de
1 cuando el bit 31 de `data` viene encendido con una velocidad de
rueda real. `uisimulator/common/sim_tasks.c` inyecta cada botón vía
`button_queue_post(codigo, 0)` (dato siempre 0, ver `MODIFICATIONS.md`
F0/F9): sin ese bit, `button_apply_acceleration()` devuelve 0 y
`metro_input.c` cae siempre a `steps=1`, sin importar cuántos
`SCROLL_FWD` se inyecten seguidos ni con qué separación. Verificado
bajando `METRO_INDEX_LETTER_MIN_STEPS` a 1 de forma temporal, tomando
la captura, y devolviéndolo a 3 antes de compilar la versión final --
mismo patrón que la instrumentación de depuración temporal de F4/F5,
nunca llega a un commit. El umbral real de 3 queda sin verificar
visualmente en este entorno (misma categoría que la pantalla USB de
F9, M-038): [ESTIMADO] correcto por inspección de código
(`metro_nav_move_sel()` recibe el mismo `steps` que ya usa para mover
la selección, así que si la selección salta 3+ filas la letra
correspondiente es la de la fila donde aterriza), pero requiere
hardware real (o una rueda simulada con velocidad, fuera de alcance
de este sim) para confirmar el disparo natural. Ver
`docs/DESVIACIONES.md` F10-1.

## M-041 — F10: `metro_draw_tile()` con etiqueta en blanco no dibuja nada, en vez de depender de un glifo de espacio

`metro_widgets_draw_empty_state()` reutiliza `metro_draw_tile()` para
el cuadro acento sin letra (S1.4), pasándole `" "` como `label` con la
intención de que el glifo de espacio no dibuje nada visible. La fuente
bitmap propia de `MFONT_DISPLAY` no tiene un glifo real para el
carácter espacio -- `lcd_putsxy()` cae a otro glifo de la tabla en su
lugar (visto en el simulador como una "I" mayúscula grande, sin
relación con el contenido esperado; capturado antes del fix
vaciando `Playlists/` para forzar la pivote vacía). Encontrado
verificando visualmente el estado vacío recién implementado, no antes
de esta fase porque `metro_draw_tile()` no tenía ningún llamador con
una etiqueta en blanco hasta ahora. Fix: `metro_draw_tile()` ahora
salta el `lcd_putsxy()` por completo cuando el carácter resuelto es
un espacio, en vez de confiar en que la fuente lo dibuje en blanco.

## M-042 — F10: iconos geométricos reales para batería/aleatorio/repetir, reemplazando los textos de M-018/F5-1

`metro_draw_battery()` pasa de texto "`N%`" a un ícono real (rectángulo
+ pestaña, relleno proporcional al nivel vía `lcd_fillrect`) -- mismo
ancho reservado en el encabezado (`metro_draw_header()`), sin cambios
ahí. Las insignias de texto "aleatorio"/"repetir todo" de Now Playing
(F5-1, deferidas explícitamente a F10) se reemplazan con
`metro_widgets_draw_shuffle_icon()` (trazos cruzados con puntas de
flecha) y `metro_widgets_draw_repeat_icon()` (cuadro-lazo con una
punta de flecha rompiendo la esquina superior derecha, más un "1"
superpuesto cuando `REPEAT_ONE` está activo) -- ambos construidos solo
con primitivas `lcd_drawline`/`lcd_drawrect`/`lcd_fillrect` (M-018:
nunca se lee un bitmap de disco), verificados visualmente en
`docs/screenshots/F10-np-icons.png`.

## M-043 — F11: qué transición corre se decide diffeando `metro_nav_t` en `metro_main.c`, no con llamadas explícitas desde cada pantalla

`PLAN_MAESTRO.md` F11 lista `metro_screen_list/hub/nowplaying` como
"mod: llaman a transiciones", sugiriendo que cada pantalla anuncia su
propia transición en el sitio donde empuja/saca una página. En cambio,
`metro_main.c` captura `metro_nav_depth()`/`metro_nav_pivot()`/"¿es
Now Playing la página actual?" ANTES de llamar al `*_handle()` de
turno, y vuelve a leer los tres DESPUÉS -- la diferencia decide sola
qué transición corre (profundidad sube y la página nueva es el
centinela de NP -> FADE; profundidad sube sin más -> PUSH; profundidad
baja saliendo específicamente de NP -> FADE; profundidad baja sin más
-> POP; misma profundidad con el pivot cambiado -> SLIDE; nada de lo
anterior -> redibujo instantáneo). `metro_screen_hub.c`,
`metro_screen_list.c` y `metro_screen_nowplaying.c` quedan intocados
salvo por lo que ya tenían de F1-F10 -- ninguno sabe que
`metro_transitions.c` existe.

**Por qué**: la lógica "¿esto fue un push, un pop, o solo cambiar de
pivot?" ya vive por completo en `metro_nav_t` (F3, `metro_nav.c`) --
triplicarla como una llamada explícita en cada uno de los ~15 sitios
que empujan o sacan una página (contando `metro_screen_hub.c` solo)
sería repetir la misma pregunta que `metro_nav_t` ya puede responder
por diferencia de estado, y cada sitio nuevo (un pivot futuro, una
página nueva) tendría que acordarse de anotarlo. Un único punto de
diff no puede desincronizarse del comportamiento real del stack de
navegación porque LEE ese stack, no lo declara aparte.

**Caso límite real, resuelto por el orden de las comparaciones**: el
centinela de Now Playing SÍ se empuja como cualquier otra página
(F5, `metro_screen_nowplaying_push()` llama a
`metro_screen_list_push()`), así que entrar a NP sube la profundidad
Y cambia a NP en el mismo evento -- de ahí que el chequeo específico
de NP tenga que ganarle al genérico de "profundidad subió" en el
orden de `if`, o "push(NP)" saldría con SLIDE en vez de FADE.
`MACT_OPTIONS` desde NP tiene la relación inversa: profundidad sube,
pero la página nueva NO es el centinela (es la lista de opciones) --
cae al PUSH genérico a propósito, ver `docs/DESVIACIONES.md` F11-1.

## M-044 — F11: `metro_draw_text_cut_right()` (F10) ignoraba el framebuffer redirigido de una transición, dejando filas en blanco

`metro_fb_render()` redirige todo el dibujo de `lcd_*` a un buffer
fuera de pantalla (`viewport_set_buffer(NULL, &fb, SCREEN_MAIN)`) para
prerrenderizar el destino de una transición ANTES de animarla.
`metro_draw_text_cut_right()` (F10, recorte de títulos largos) crea su
propio viewport temporal para el recorte con
`viewport_set_defaults(&vp, SCREEN_MAIN)`, que deja `vp.buffer = NULL`
-- resuelto SIEMPRE contra el framebuffer real de la pantalla
(`lcd_init_viewport()`), sin importar qué buffer esté activo en ese
momento. Mientras `metro_draw_text_cut_right()` solo se llamaba
durante un dibujo normal (pantalla real == framebuffer real) esto era
invisible; F11 lo expuso porque el primer llamador real fuera de ese
caso es exactamente el prerrenderizado offscreen de una transición --
el título de cada fila se dibujaba contra la pantalla real (invisible
en ese instante, luego pisado por los cuadros de la animación) en vez
del framebuffer offscreen, así que el `to` capturado nunca tenía
texto de fila. Encontrado comparando capturas con `animations=off`
(filas visibles) contra `animations=all` (filas en blanco tras un
PUSH/SLIDE ya asentado) -- mismo síntoma en cualquier pantalla de
lista, no específico de música. Fix: una línea,
`vp.buffer = lcd_current_viewport->buffer;` justo después de
`viewport_set_defaults()`, heredando el buffer que esté activo de
verdad en vez de forzar el real -- sigue siendo la misma garantía de
M-027 (nunca un puntero de basura de stack), solo que ahora copia uno
ya válido en vez de inventar `NULL`.

## M-045 — F11: matriz `animations` (off/minimal/all) × `graphics` (lite/full) -- semántica real, no solo persistencia

`metro_settings.animations`/`.graphics` existían desde F6/F8 como
enteros persistidos "sin efecto visual hasta F11+" (comentario textual
en `metro_settings.h`). F11 les da tipo (`enum metro_anim_level`,
`enum metro_gfx_level`) y efecto real:
`metro_transitions_slide()`/`_fade()` calculan un nivel EFECTIVO
(`metro_settings.animations` menos las muescas de auto-degradación de
sesión, M-015) y lo traducen a cuadros/retardo
(`all`=8×3 ticks, `minimal`=4×3, `off`=instantáneo, sin animación).
`graphics` solo importa para FADE: `metro_fb_present_fade()` (blend
por píxel) está reservado a `graphics=full`
(`PLAN_MAESTRO.md` S3.1: "Solo con `graphics=full`") -- con
`graphics=lite`, incluso con `animations=all`, FADE cae al mismo
SLIDE que ya usa PUSH/POP/twist en vez de un segundo mecanismo de
respaldo. Verificado visualmente: `docs/screenshots/F11-anim-off.png`
(mismo tick que F11-push-mid.png, destino ya asentado),
`docs/screenshots/F11-fade-lite-mid.png` (borde nítido de slide, no
mezcla de píxeles, con `graphics=lite`). Default cambiado de las
constantes provisionales de F8 (`animations=1`, `graphics=1`) a
`METRO_ANIM_DEFAULT`=ALL/`METRO_GFX_DEFAULT`=FULL (M-015: "canon =
máxima fidelidad") en `metro_settings.c` y en "restablecer ajustes"
(`metro_screen_settings.c`).

## M-046 — F12: geometría del turnstile (eje central, distancia focal, tabla offline) y el asentado final tras el último cuadro

`metro_fb_present_turnstile()`/`metro_fb_draw_turnstile_layer()`
modelan una rotación RÍGIDA de toda la pantalla alrededor de su propio
centro vertical (`x = LCD_WIDTH/2`), bajo una cámara de perspectiva
simple a distancia focal `D = LCD_WIDTH*1.2` — ni el eje ni `D` están
confirmados contra una fuente primaria de WP7 (`INVESTIGACION.md` F.3
solo documenta el rango de ángulo/duración del turnstile real, no su
geometría de proyección exacta), así que ambos son **[ESTIMADO]**,
elección de diseño propia. La tabla (32 ángulos × 320 columnas,
`metro_turnstile_xs`/`metro_turnstile_step`) se resuelve hacia atrás
(backward-mapping, sin huecos) con Python
(`firmware/tools/gen_turnstile_table.py`, nunca trigonometría en el
dispositivo — sin FPU, `INVESTIGACION.md` B.5/B.6) y se consume en
tiempo real solo con lookup + acumulador de paso fijo por columna,
técnica de PictureFlow (B.5). Cada cuadro se redondea al ÍNDICE de
tabla más cercano en vez de interpolar entre dos filas — con solo 8
cuadros de animación bajo `animations=all`, el error de precisión no
es perceptible, y ahorra tener que mezclar 320 pares de valores por
cuadro en el dispositivo.

**Bug real encontrado y arreglado**: como la tabla solo tiene 32
muestras discretas, el último cuadro de la animación (ángulo real
exactamente 0°) nunca cae justo en una muestra de la tabla — el
resultado proyectado es una aproximación cercana a la pantalla final,
no un píxel a píxel exacto. Como `FEATHER` (M-047) redibuja SOLO el
área de filas después, el encabezado/pivots se habrían quedado
ligeramente deformados para siempre (nada los vuelve a dibujar).
Encontrado visualmente comparando el estado asentado antes/después de
agregar un blit final directo (`lcd_bitmap_part` sin proyección) al
cierre de `run_turnstile()` — mismo patrón que
`metro_fb_present_slide()` ya logra de forma exacta en su último
cuadro (`dx == LCD_WIDTH`), pero que el turnstile no puede replicar
por construcción (tabla discreta, no álgebra exacta).

**Aliasing esperado, sin arreglar a propósito**: en ángulos cercanos a
los extremos del rango (-80°/50°), columnas muy comprimidas (`step`
grande) pueden mostrar texto con un patrón de rayas visible
(sub-muestreo sin filtro) — exactamente la técnica que
`INVESTIGACION.md` B.5 documenta de PictureFlow ("el único costo por
píxel es un shift y una comparación", sin antialiasing). Visible solo
durante los ~240-360ms de la animación, nunca en el resultado asentado
(el blit final de arriba lo reemplaza por completo) — no vale la pena
un filtro de resampleo para un efecto que dura una fracción de
segundo. Ver `docs/screenshots/F12-turnstile-mid.png`.

## M-047 — F12: FEATHER como cascada por fila en `metro_screen_list.c`, no como transición de framebuffer completo

A diferencia de SLIDE/PUSH/POP/FADE (todas en `metro_transitions.c`,
operando sobre framebuffers completos de 320×240), FEATHER anima FILAS
INDIVIDUALES de una sola página de lista -- vive en
`metro_screen_list.c` en su lugar, con su propio bucle bloqueante de 6
cuadros que solo redibuja el área de filas
(`metro_draw_clear_rows_area()` + `metro_draw_rows_ex()` con un
arreglo de offsets Y, uno por fila visible). Escalonado de 1
cuadro/fila (fila N no empieza a caer hasta el cuadro N), cada fila
usando `metro_ease(OUT_QUAD, local_frame+1, local_total)` sobre los
cuadros que le quedan en el presupuesto -- así la fila SIEMPRE llega
exactamente a offset 0 en su propio último cuadro
(`metro_ease()`'s `i==frames` → 256) sin importar cuán pocos cuadros
le tocaron (incluso la última fila visible, con un solo cuadro
asignado). Disparado por una bandera (`s_feather_pending`) que
`metro_screen_list_push()` levanta en CADA push (incluido el del
centinela de Now Playing, que nunca la consume porque esa rama
siempre termina en FADE -- la siguiente vez que se empuje una lista de
verdad, la bandera ya se volvió a levantar de todas formas, así que no
hay arrastre real de estado viejo). Gate propio, más estricto que
PUSH/POP/twist: solo corre bajo el nivel EFECTIVO `all`
(`metro_transitions_effective_all()`, nueva función expuesta desde
`metro_transitions.c` para no duplicar el estado de auto-degradación
de sesión de M-015 en dos módulos) -- "off" bajo `minimal` y `off`,
igual que dicta la tabla de `PLAN_MAESTRO.md` S3.3.

## M-048 — F12: fondo de carátula atenuada en Now Playing -- buffer de ~300 KB, no 150 KB, misma razón que M-033

`metro_albumart.c` gana un segundo par caché/buffer
(`s_bg_scratch`/`s_bg_loaded_path`) independiente del tile pequeño ya
existente, decodificando la MISMA carátula a `LCD_WIDTH x LCD_HEIGHT`
en vez de `METRO_ALBUMART_SIZE`. `PLAN_MAESTRO.md` S3.3 estima este
buffer en "150 KB" (el tamaño final `320*240*2` sin margen) pero M-033
(F5) ya estableció, con un bug de corrupción de memoria real como
prueba, que `FORMAT_RESIZE` necesita espacio para el decode NATIVO
completo antes de reducir la imagen -- no solo el tamaño final. Este
buffer usa la misma fórmula de margen `*2*2` que el buffer del tile
pequeño (`METRO_ALBUMART_SCRATCH_SIZE`), dando ~300 KB reales, no 150.
Trivial sobre 64 MB de RAM total (`INVESTIGACION.md` B.3 ya lo
calificaba de "insignificante" incluso a 150 KB), así que la prioridad
es la misma que en M-033: seguridad de memoria por encima de fidelidad
al número exacto del plan. La composición en sí
(`metro_fb_blend_over_color()`, mismo blend por píxel que
`metro_fb_present_fade()`) NO llama `lcd_update()` -- a diferencia de
toda otra función `present_*`/`blend_*` de `metro_fb.c`, esta es un
fondo ESTÁTICO con más dibujo encima (tile pequeño, texto, controles)
antes de un único `lcd_update()` al final de
`metro_screen_nowplaying_show()`, no el único paso de una animación
por cuadro.

## M-049 — F13: `METRO_TRACE` gana `logf()` -- `DEBUGF` solo es visible en el simulador, nunca en hardware real sin equipo adicional

`PLAN_MAESTRO.md` S3.4 pide `METRO_TRACE` para medir el costo real de
cada transición en hardware (F13). Investigando cómo verla ahí se
encontró que `DEBUGF` (`firmware/debug.c`, target nativo) formatea el
mensaje a un buffer local y llama a `debug()` -- una función
`static inline` que **no hace nada** (`(void)msg;`) salvo que el
target tenga su propio `HAVE_GDB_API`/`breakpoint()` conectado a un
depurador JTAG real. En el simulador SÍ se ve (siempre va a `stderr`,
`firmware/target/hosted/debug-hosted.c`, ya usado para verificar F11 —
`docs/DESVIACIONES.md` no tiene entrada porque no era un hallazgo
nuevo ahí, solo una comprobación), pero en un iPod real la promesa del
plan de "medir en hardware con METRO_TRACE" habría sido letra muerta
sin cambiar nada. Rockbox sí tiene un mecanismo pensado exactamente
para esto: `logf()` (`firmware/export/logf.h`), un buffer circular de
16 KB en RAM (`ROCKBOX_HAS_LOGF`, definido para `ipod6g`) visible
desde el propio dispositivo, sin cable ni depurador -- menú de
depuración → "Show Log File" (`apps/debug_menu.c`, `logfdisplay`).
Requiere `#define LOGF_ENABLE` ANTES de `#include "logf.h"` en CADA
archivo que quiera usarlo de verdad (si no, `logf()` se compila a
no-op en ese archivo -- el propio header lo hace así para que el
buffer no se llene con el logging de todo el firmware a la vez);
`metro_transitions.c` es el único archivo de Metro que lo necesita, así
que es la única definición. `METRO_TRACE` ahora llama a ambos
(`DEBUGF` para el simulador, `logf` para hardware real) -- ver
`docs/ESTADO_FINAL.md` para el procedimiento completo de medición.

## M-050 — F13: `tools/version.sh` marca "M" (árbol sucio) incluso con todo comiteado -- estructura del repo, no un bug de Metro

`firmware/rockbox/tools/version.sh` decide si el árbol está "sucio"
corriendo `git -C "$1" diff --name-only HEAD` con
`GIT_WORK_TREE="$1"`, donde `$1` es la ruta al código fuente de
Rockbox (`firmware/rockbox/`, pasada como `ROOTDIR` desde
`firmware.make`). Ese mecanismo asume que esa ruta ES la raíz del
repo git -- cierto en un checkout de Rockbox standalone, **falso**
en Metro-Aura: el repo real es la carpeta padre completa (`DECISIONS.md`
M-006 y ss., todo el árbol de fases vive ahí). Con `GIT_WORK_TREE`
apuntando a un subdirectorio en vez de la raíz real, el comando falla
(`fatal: this operation must be run in a work tree`) y el resultado se
interpreta como "sucio" -- comprobado generando un release con el
árbol perfectamente limpio (`git status --porcelain` vacío) y viendo
igual una "M" en `rockbox-info.txt`. No es un bug introducido por
Metro-Aura ni algo que `apps/metro/` pueda arreglar -- es cómo
`tools/version.sh` fue escrito, asumiendo la estructura de un checkout
de Rockbox suelto.

**Fix, sin tocar ningún archivo de Rockbox**: `firmware.make` ya
respeta una variable `VERSION` pasada por línea de comandos con
prioridad sobre el cálculo de git (`SVNVERSION:=$(shell VERSION='$(VERSION)'
...)`, y `version.sh` mismo: `if [ -z $VERSION ]; then ... fi`).
`firmware/tools/build_target.sh` ahora propaga una `VERSION` opcional
del entorno a cada `make`; `firmware/tools/package_dist.sh` la calcula
correctamente ANTES de invocar `build_target.sh` -- hash corto real +
fecha, con la "M" solo si `git status --porcelain` (corrido desde la
raíz real del repo, sin el `GIT_WORK_TREE` mal alineado) dice de
verdad que hay cambios sin commitear. Mismo patrón que
Aura-Firmware's D-296/D-297 documentó como aceptado sin diagnosticar
la causa raíz (`package_dist.sh` de ese repositorio solo bloquea el
release si el árbol está sucio, nunca corrige el string en sí) -- acá
se corrigió la causa en vez de solo evitarla.

## M-051 — R2-F1: todo texto de `apps/metro/` dibuja bajo `DRMODE_FG`, nunca el `DRMODE_SOLID` por defecto de Rockbox

`INVESTIGACION-metro-r2.md` E.1 confirmó la causa raíz de las "placas
negras" que tapaban el cover art en Now Playing: `metro_draw.c` nunca
llamaba a `lcd_set_drawmode()`, así que cada `lcd_putsxy()` heredaba el
`DRMODE_SOLID` por defecto de Rockbox -- cada glifo se dibuja con una
caja de fondo opaca (`fg_pattern`/`bg_pattern` del viewport), tapando
lo que sea que estuviera detrás en vez de leerlo como transparente.

**Fix**: `metro_draw_text()` fija `lcd_set_drawmode(DRMODE_FG)` justo
antes de `lcd_putsxy()`; `metro_draw_text_cut_right()` fija
`vp.drawmode = DRMODE_FG` en el viewport temporal que ya arma para el
recorte de texto (M-027); `metro_draw_tile()` (letra inicial de un
tile) fija `DRMODE_FG` inline antes de su `lcd_putsxy()` propio. Las
tres llamadas directas a `lcd_putsxy()` que quedaban fuera de
`metro_draw_text()` (`metro_draw_header()` x2, `metro_draw_pivots()` y
el dígito del ícono de repetir en `metro_widgets.c`) ahora se canalizan
por `metro_draw_text()` en vez de duplicar la lógica de drawmode --
simplifica el llamador y garantiza el mismo comportamiento en un solo
lugar. `metro_main()` fija el mismo baseline una vez, justo después de
`metro_fonts_init()` (el LCD arranca en `DRMODE_SOLID` y nada dibujó
texto todavía en ese punto).

**Desviación del plan** (`PLAN-metro-r2-maestro.md` DD-1): el plan
asumía que las llamadas a `plugin_load()` vivían dentro de
`metro_main.c` y pedía restaurar `DRMODE_FG` ahí "al volver de
cualquier `plugin_load()`". En el código real esas llamadas están en
`metro_photos.c` (`imageviewer.rock`) y `metro_video.c`
(`mpegplayer.rock`), no en `metro_main.c` -- cada una restaura
`DRMODE_FG` inline justo después de su propio `plugin_load()`, antes
de que `metro_transitions_fade()` vuelva a dibujar la lista. Ver
`docs/DESVIACIONES.md` R2-F1-1 para el detalle completo.

**Por qué no restaurar `DRMODE_SOLID` en algún punto**: ningún dibujo
de `apps/metro/` quiere una caja de fondo opaca a propósito -- el
fondo ya se pinta explícitamente donde hace falta (`lcd_fillrect()` en
`metro_draw_tile()`, `metro_draw_clear_rows_area()`, etc.) antes de
dibujar el texto encima, así que no hay ningún caso real que necesite
volver a `SOLID`.

## M-052 — R2-F1/DD-2: `METRO_NP_BG_ALPHA256` se mantiene en 77 (30%) tras verificar contra una carátula casi blanca

`INVESTIGACION-metro-r2.md` E.4 estimaba, sin observación real, que el
peor caso de contraste para la línea de texto terciaria (álbum) de Now
Playing rondaría ~25/255 (~10%) contra una carátula casi blanca --
suficiente para preocupar por legibilidad. `PLAN-metro-r2-maestro.md`
DD-2 pedía generar esa carátula, capturar el tema oscuro real y decidir
entre mantener 77 o bajar a 51 (20%) según lo que se viera.

**Qué se hizo**: `gen_test_media.sh` ahora genera una carátula
`0xF2F2EC` (casi blanca) para el álbum "Wheel & Click/Analog Dreams"
(ver el comentario nuevo ahí). Se capturó Now Playing reproduciendo un
tema de ese álbum en el simulador, tema oscuro, `graphics=full`:
`docs/screenshots/R2-F1-np-worstcase.png`. Se comparó contra
`docs/screenshots/R2-F1-np-clean.png` (carátula naranja `0xCC7733`,
"First Light", caso típico).

**Resultado**: la línea terciaria ("Analog Dreams") queda legible en
ambas capturas. La razón, visible al mirar el propio código de
`metro_screen_nowplaying_show()`: `metro_fb_blend_over_color()` mezcla
la carátula sobre `metro_color_bg()` (un azul-marino oscuro) al 30% --
incluso con una carátula casi blanca (~242 de luminosidad), el
compuesto queda dominado por el 70% del fondo oscuro
(`0.3*242 + 0.7*25 ≈ 90`, un gris medio-oscuro, no un blanco real). La
estimación de E.4 asumía implícitamente comparar el texto contra la
carátula a color pleno, no contra su versión ya atenuada al 30% -- el
mecanismo de blend en sí ya actúa como salvaguarda de contraste antes
de llegar al texto.

**Decisión**: se mantiene `METRO_NP_BG_ALPHA256 = 77`, sin bajar a 51.
Ver `docs/DESVIACIONES.md` no aplica aquí -- no hubo desviación del
plan, DD-2 preveía explícitamente ambos desenlaces ("si es legible,
mantener 77; si no, bajar a 51") y este es el desenlace "mantener".

**Nota lateral, fuera de alcance de R2-F1**: verificando estas capturas
se encontró que el índice de fila seleccionado en pantalla NO siempre
coincide con la pista que efectivamente empieza a sonar al reproducir
desde "Canciones" (todas) o desde una lista de canciones de álbum --
confirmado con trazas temporales (`songs_on_select` recibe el índice
correcto, pero `metro_music_play_all_songs()`/
`metro_music_play_songs_of_album()` terminan reproduciendo una pista
distinta a la de esa fila). Es un bug real y reproducible en
`metro_music.c` (`insert_matching_tracks()` y su interacción con
`playlist_start()`), preexistente a esta ronda (F4/F6), no introducido
por DD-1/DD-2 ni relacionado con ellos. No se investigó ni se corrigió
aquí -- fuera del alcance declarado de R2-F1. Reportado al dueño en el
resumen de esta fase para decidir cuándo abrirlo como su propia fase o
ítem de trabajo.

## M-053 — R2-F1/DD-3: `metro_fsutil_list_by_ext()` descarta directorios vía `dir_get_info()`/`ATTR_DIRECTORY`

`INVESTIGACION-metro-r2.md` B.3 confirmó empíricamente que
`metro_fsutil_list_by_ext()` no distinguía directorios de archivos --
un directorio literalmente llamado `Folder.jpg` (herramientas de
escritorio de terceros lo crean) aparecía en la lista de fotos como si
fuera una imagen.

**Fix**: mismo patrón que `apps/filetree.c` (línea ~201) ya usa para su
propio escaneo filtrado por extensión -- `dir_get_info(d, entry)` por
cada entrada, y se descarta si `info.attribute & ATTR_DIRECTORY`.
`fs_attr.h` (que define `ATTR_DIRECTORY`) llega transitivo vía
`dir.h`, ya incluido en `metro_fsutil.c` -- sin includes nuevos.

**Verificado**: se recreó el caso de repro exacto de B.3 --
`Photos/Folder.jpg/` (directorio) junto a los fixtures reales de fotos
en el simulador -- y se capturó la lista "todos" de Fotos
(`docs/screenshots/R2-F1-photos-dirfilter.png`): el directorio no
aparece, solo los 6 archivos de imagen reales quedan listados.

## M-054 — R2-F1/DD-4: `metro_ensure_media_dirs()` crea `/Music`, `/Videos`, `/Photos`, `/Playlists` si faltan

Un disco que nunca pasó por un sync real de Aura Studio (música
copiada a mano por USB, o un iPod recién formateado) puede no tener
ninguna de las cuatro carpetas de medios de nivel superior que
`docs/contracts/library-layout-v1.md` (Aura-Firmware, consultado en
solo lectura) define -- `/Music/`, `/Videos/`, `/Photos/`,
`/Playlists/`. Sin ellas, las pantallas de Videos/Fotos muestran listas
vacías sin explicación y guardar una lista de reproducción nueva
fallaría en silencio.

**Qué se hizo**: `metro_ensure_media_dirs()` (`metro_settings.c`, único
módulo autorizado por `CLAUDE.md` a construir rutas del contrato) crea
las cuatro carpetas con `mkdir()` si `dir_exists()` dice que falta cada
una -- mismo patrón que `metro_settings_save()` ya usa para
`METRO_DIR`. Se llama desde `metro_disk_handoff()` en `metro_main.c`,
el único punto (arranque + cada vuelta de USB, comentario ya existente
ahí) donde el firmware "recupera" el disco.

**Verificado**: se apartaron las cuatro carpetas del simulador
(simulando un disco recién montado sin ellas), se arrancó el
simulador, y las cuatro reaparecieron vacías tras el boot antes de
restaurar los fixtures reales.

## M-055 — R2-F1/DD-5: `metro_manifest_t` parsea las 13 claves reales de `sync_summary.cfg`, no solo 3

Aura Studio escribe el mismo `sync_summary.cfg` de 13 claves para
cualquier dispositivo, sea Aura o Metro (`CatalogSummaryWriter`, no
distingue por `firmware_family`) -- pero `metro_manifest_t` solo leía
`music_count`/`video_count`/`photo_count`, ignorando las otras 10
claves reales que Studio ya escribe hoy (`music_bytes`, `video_bytes`,
`photo_bytes`, `playlist_count`, y el desglose por categoría de
video/foto).

**Qué se hizo**: `metro_manifest_t`/`metro_manifest_load()`
(`metro_manifest.c/.h`) ahora replican el mismo conjunto de campos que
`aura_manifest_t` de Aura-Firmware (consultado en solo lectura) --
incluyendo el parser `parse_i64()` propio (mismo motivo que el de Aura:
`atoll()` no está disponible en todos los targets de este árbol) y las
banderas `has_video_categories`/`has_photo_categories`, derivadas de
si las claves de desglose aparecieron en el archivo (no son claves en
sí mismas) -- un manifiesto escrito antes de que Studio agregara el
desglose no las tiene, y el firmware debe distinguir eso de "el conteo
real es cero".

`metro_screen_about.c` (`about_count()`/`about_get_row()`) agrega filas
para `playlist_count` (siempre que haya sync) y, condicionalmente, las
6 filas de desglose (películas/series/videoclips si
`has_video_categories`; imágenes/fotografías/IA si
`has_photo_categories`) -- mismo orden que la pantalla Acerca de real
de Aura-Firmware (`aura_screens.c`, consultada en solo lectura),
adaptado al motor de filas genérico de Metro (Aura dibuja esa pantalla
a mano; Metro reutiliza su lista de filas existente, sin copiar el
motor de dibujo de Aura). 7 strings nuevos ES/EN en `metro_lang.c`
(`LANG_ABOUT_PLAYLISTS`/`MOVIES`/`SERIES`/`CLIPS`/`IMAGES`/
`PHOTOS_TAKEN`/`AI`), minúsculas sin acentos -- mismo estilo que el
resto de `metro_lang.c` (ninguna cadena existente usa tildes).

**Verificado**: `sync_summary.cfg` de 13 claves colocado a mano en el
simulador (conteos: 21 canciones, 5 videos, 8 fotos, 1 lista, desglose
1/1/3 de video y 1/6/1 de foto) -- capturas
`docs/screenshots/R2-F1-about-full.png`,
`R2-F1-about-full-scrolled.png`, `R2-F1-about-full-end.png` muestran
las 13 filas de conteo más "basado en rockbox" al final, en el orden
correcto.

## M-056 — R2-F1/DD-6: `package_dist.sh --release-tag` escribe `.rockbox/aura/version.txt`, mismo contrato que Aura-Firmware

**Nota de numeración**: `PLAN-metro-r2-maestro.md` DD-6 sugería "Decisión
M-052" para este ítem, escrito en Fase 3 antes de saber el orden real
de ejecución de Fase 4. `DECISIONS.md` es una bitácora cronológica, no
una spec (encabezado del archivo) -- M-052 ya quedó asignado a la
decisión de DD-2 (la primera que necesitó número nuevo tras M-051 en
el orden real de ejecución). Este ítem es M-056, siguiente libre.

`package_dist.sh` acepta `--release-tag vX.Y.Z` como primer/segundo
argumento -- mismo contrato exacto que el `package_dist.sh` de
Aura-Firmware (consultado en solo lectura, D-297): exige árbol git
limpio (`ERROR` + `exit 1` si `--release-tag` se combina con cambios
sin commitear, ANTES de compilar nada), y solo con la bandera escribe
`$RELEASE_TAG` literal en `.rockbox/aura/version.txt` dentro del
`$STAGE` que arma `rockbox.zip`, justo antes del `zip -qr` (mismo punto
que Aura). Sin la bandera (build de desarrollo, uso normal) el archivo
no se escribe -- su ausencia sigue siendo una señal válida en sí misma.

**Verificado**:
- Guardia de árbol sucio: `package_dist.sh --release-tag v0.2.0-test`
  corrido con los 25 archivos de esta misma fase sin commitear ->
  `ERROR: hay cambios sin commitear -- un --release-tag necesita el
  árbol limpio`, `exit 1`, sin compilar nada -- confirmado en vivo.
- Build de desarrollo (sin la bandera): corrida completa real
  (`build_target.sh` + bootloader + `make zip` + `mks5lboot` +
  `rockbox.zip` armado + centinelas + checksums, terminó en
  `firmware/dist/`) -- confirmado que `.rockbox/aura/version.txt`
  **no** aparece en `rockbox.zip` (`unzip -l` sin esa entrada).
- Escritura de `version.txt` en sí (el bloque `mkdir -p`/`echo >`
  dentro de `$STAGE`): verificado en aislamiento contra un directorio
  de staging de prueba -- contenido exacto `v0.2.0-test` escrito en
  `$STAGE/.rockbox/aura/version.txt`. **No verificado end-to-end**
  dentro de una corrida real de `package_dist.sh --release-tag` sobre
  árbol limpio: probar eso exige que `package_dist.sh` mismo (el
  archivo que esta misma decisión modifica) ya esté comiteado --
  imposible antes del commit de cierre de R2-F1. Queda como
  verificación pendiente natural la primera vez que R2-F5 corra un
  release real (`v0.2.0`, con el árbol ya limpio en ese punto).

## M-057 — R2-F2/DD-7,8,9: cuadrícula de fotos (modelo declarativo + geometría 4×80 + caché propia de miniaturas)

**Nota de numeración**: `PLAN-metro-r2-maestro.md` sugería "M-053" para
DD-9 -- ese número ya quedó tomado por DD-3 (R2-F1) en el orden real de
ejecución, mismo caso que M-056 ya documentó. Bitácora cronológica, no
spec (encabezado de este archivo).

**DD-7 -- modelo extendido, no pantalla centinela**: `struct metro_pivot`
(`metro_page.h`) gana dos campos **al final** -- `int tile_cols;` (0 =
lista de filas, el comportamiento de siempre) y
`const fb_data *(*get_tile)(void *ctx, int index);` (NULL = tile de
acento con inicial, `metro_draw_tile()`). Todo inicializador posicional
existente en el árbol sigue compilando -- los dos campos nuevos caen en
0/NULL por defecto (warnings `-Wmissing-field-initializers` esperados y
aceptados, no tratados como error). `metro_screen_list_show()` llama
`metro_draw_tiles()` (nuevo, `metro_draw.c`) cuando `tile_cols > 0`;
`metro_screen_list_handle()` usa `metro_nav_move_sel_grid()` (nuevo,
`metro_nav.c`) en vez de `metro_nav_move_sel()` para ese mismo caso.
FEATHER (F12) y el índice flotante (F10) se omiten para pivots de
cuadrícula -- ninguno de los dos tiene sentido en un grid, ver los
comentarios en `metro_screen_list.c`.

**DD-8 -- geometría (320×240, header+pivots hasta y=84)**: 4 columnas ×
tiles de 80×80, sin separación, al ras de los bordes
(`METRO_TILE_SIZE`/`METRO_TILE_COLS`/`METRO_TILE_ROWS_VISIBLE`,
`metro_draw.h`). Fila 1 en y=84, fila 2 en y=164 -- se corta sola en
y=240 (asoma 76 de 80px), sin necesitar una fila extra de "peek" como
`metro_draw_rows_ex()`. Selección: borde de 3px `metro_color_accent()`
**dentro** del tile (no lo agranda). Verificado visualmente:
`docs/screenshots/R2-F2-photos-grid.png` (8 tiles, borde de selección,
segunda fila asomando cortada).

**DD-9 -- caché de miniaturas (`metro_photo_thumbs.c/.h`, nuevo)**:
- Directorio propio `.../aura/metrocache/photos/` (NO el `photocache/`
  de Aura -- formato/tamaño distintos, y la limpieza de convivencia lo
  borraría de todas formas). Ruta armada solo en
  `metro_settings_metro_cache_dir()` (`metro_settings.c`), regla de
  rutas de contrato del `CLAUDE.md`.
- Formato: `fb_data` crudo 80×80 (12 800 B/foto), nombre
  `<archivo>.<mtime>.mth` -- la mtime en el nombre ES la invalidación.
  `metro_photo_item_t` (`metro_photos.h`) gana un campo `mtime`, ya
  disponible gratis: `metro_fsutil_list_by_ext()` ya llamaba
  `dir_get_info()` por cada entrada (M-053, R2-F1) para el chequeo de
  `ATTR_DIRECTORY` -- se agregó `metro_fsutil_list_by_ext_mtime()`
  (variante con un `out_mtimes[]` opcional) en vez de un segundo scan.
- Generación bajo demanda, presupuestada: `metro_photo_thumbs_get()`
  devuelve de inmediato lo que ya esté en la ventana RAM (16
  miniaturas, ~205KB) o en caché de disco (lectura cruda, no
  decodificación -- barata, se intenta síncrona); si no hay nada,
  encola el nombre y devuelve `NULL` (el llamador cae al tile de acento
  con inicial). `metro_photo_thumbs_tick()`, llamado una vez por
  iteración ociosa de `metro_main()` (mismo poll que el redibujado del
  índice flotante de F10), decodifica **como máximo una** miniatura
  pendiente por llamada y pide redibujo solo si de verdad decodificó
  algo. Verificado: `docs/screenshots/R2-F2-photos-grid-loading.png`
  (8 placeholders con inicial, caché de disco vacía a propósito) vs.
  `R2-F2-photos-grid.png` (mismos 8, ya decodificados) tras más ticks.
- **"Cubrir" sin probe de dimensiones JPEG**: en vez de portar el
  `probe_jpeg_dimensions()` propio de Aura (`aura_photos.c`, parser de
  cabecera JPEG a mano, sin equivalente en Rockbox stock) para calcular
  el recorte de cobertura de antemano, se decodifica UNA vez con
  `read_jpeg_file(FORMAT_NATIVE|FORMAT_RESIZE|FORMAT_KEEP_ASPECT)` a un
  scratch de `80*80*2*2` bytes (margen M-033) -- el resultado siempre
  trae una dimensión exactamente en 80 y la otra ≤80 -- y luego
  `cover_crop()` (`metro_photo_thumbs.c`) hace un remuestreo nearest-
  neighbor puramente entero desde ESE bitmap ya chico hasta el recorte
  centrado 80×80 final, sin una segunda decodificación de mayor
  resolución. Sacrifica algo de nitidez en el eje recortado a cambio de
  quedarse en una sola decodificación barata -- aceptable a 80px; el
  visor propio de fotos (R2-F3, DD-10) sí necesita precisión real ahí
  y por eso ese sí lee el algoritmo Q16.16 de `aura_photos.c` como
  referencia, esta miniatura no.
- Limpieza de huérfanos: `remove_stale()` corre solo en el momento de
  una decodificación real (ya es el camino lento) -- escanea el
  directorio de caché una vez y borra cualquier otro `.mth` con el
  mismo nombre base que el que se acaba de escribir. Verificado en
  vivo: se tocó `beach.jpg` (mtime nuevo, simulando un re-sync),
  se re-entró a la cuadrícula, y el `.mth` viejo desapareció al
  terminar de decodificar el nuevo -- solo queda un archivo por foto.
- Persistencia entre arranques verificada: mismos mtimes de archivo
  exactos en los 14 `.mth` antes/después de un segundo arranque
  completo del simulador entrando a la cuadrícula -- ninguno se
  regeneró.

**Fixtures**: `gen_test_media.sh` pasa de 5 a 14 fotos `.jpg` reales
(sigue habiendo `.bmp`/`.png` no soportados, y una sin categoría a
propósito) -- "todos" abarca casi 2 pantallas completas de la
cuadrícula (14 > 8 tiles/pantalla), demostrando el asoma de la segunda
fila. `photo_categories.cfg` reparte las nuevas entre las 3 categorías.

**`test_nav.c`**: 5 casos nuevos para `metro_nav_move_sel_grid()` --
alineación de `first_visible` a filas, clamp en ambos extremos, `count`
no múltiplo de `cols` (21/4, caso real de biblioteca), lista vacía. 89
checks totales, 0 fallos (`metro_nav_move_sel()` intacto, cero
regresión en los casos ya existentes).

## M-058 — R2-F3/DD-10: visor propio de fotos (`metro_screen_photo_viewer.c`), reemplaza `imageviewer.rock`

**Motivación real, no solo la del plan**: verificando R2-F1/R2-F2 en
sesión interactiva, el dueño reportó que el simulador "se traba" al
abrir una foto desde la cuadrícula, sin poder salir. Investigado antes
de escribir código nuevo: no es un cuelgue -- `imageviewer.rock`
(plugin nativo, sin tocar) usa un esquema de botones propio para el
pad clickwheel del iPod (`apps/plugins/imageviewer/imageviewer_button.h`,
`IPOD_4G_PAD`/`IPOD_3G_PAD`/`IPOD_1G2G_PAD`): `IMGVIEW_QUIT = (BUTTON_SELECT
| BUTTON_REL)`, no MENU -- `MENU` está remapeado dentro del plugin a
`IMGVIEW_UP` (paneo). El plugin seguía vivo y esperando el botón
correcto; solo era indistinguible de un cuelgue real para quien espera
la convención de Metro (MENU = volver). El cuadro "Cargando..." que
también se reportó es UI nativa de Rockbox dentro de ese mismo plugin,
no de Metro. R2-F3 ya estaba planeado como la fase siguiente -- resulta
ser también el fix.

**Qué se hizo** (patrón centinela, igual que Now Playing F5):
- `metro_screen_photo_viewer.c/.h` (nuevos): página centinela en la
  pila (`metro_screen_photo_viewer_is_current()`), dibujo/input
  propios. `metro_screen_hub.c`'s `photo_pivot_on_select()` empuja el
  visor pasándole el MISMO array/`count` que ese pivot ya usa para
  `get_row()`/`get_tile()` -- SCROLL_FWD/BACK dentro del visor navegan
  la categoría exacta desde la que se entró, no "todos" sin importar
  el pivot de origen.
- `MCTX_VIEWER` nuevo en `metro_keymap.c/.h`: SCROLL_FWD/BACK reutilizan
  `MACT_PREV`/`MACT_NEXT` (mismo significado que en cualquier lista);
  SELECT es la única acción nueva, `MACT_TOGGLE_VIEW_MODE` (ajustar
  ↔ cubrir); MENU/MENU-sostenido/PLAY reutilizan `MACT_BACK`/`MACT_HOME`/
  `MACT_PLAYPAUSE` sin cambios.
- `metro_main.c`: `at_viewer` se suma a `at_root`/`at_player` en el
  bucle principal y en el diff de `metro_nav_t` que elige la transición
  (M-043) -- entrar/salir del visor siempre usa FADE, igual que Now
  Playing; ambos centinelas son mutuamente excluyentes así que
  `player_x || viewer_x` nunca es ambiguo. El motor de miniaturas
  (R2-F2/DD-9) y el redibujado del índice flotante (F10) se excluyen
  también mientras `at_viewer` -- ninguno tiene nada que hacer sobre
  una foto a pantalla completa.
- **Ajustar**: `FORMAT_NATIVE|FORMAT_RESIZE|FORMAT_KEEP_ASPECT` directo
  a una caja de 320×240, centrado, franjas del `metro_color_bg()` del
  tema activo.
- **Cubrir**: se leyó `aura_photos.c:943-1063` de Aura-Firmware como
  referencia de algoritmo (solo lectura, código propio escrito para
  Metro) -- `probe_jpeg_dimensions()` (parseo de marcadores JPEG hasta
  SOFn, sin decodificar un solo píxel, portado con nombres/comentarios
  de Metro) da el tamaño real de origen; `compute_decode_and_display_size()`
  calcula en Q16.16 (sin FPU) el factor `max(320/w, 240/h)` -- cubrir,
  no ajustar -- y decide si decodificar directo al tamaño final o al
  tamaño de origen (según si haría falta agrandar); `draw_scaled_centered()`
  hace el recorte centrado final por muestreo nearest-neighbor cuando
  el tamaño decodificado no coincide exactamente con el de pantalla.
  Al alternar modo se re-decodifica siempre (`s_loaded_index = -1`),
  nunca se re-muestrea un buffer viejo.
- Buffer propio `METRO_PHOTO_VIEW_SCRATCH_SIZE = LCD_WIDTH*LCD_HEIGHT*2*2`
  (~300KB, margen ×2 de M-033), no compartido con el de Now Playing
  (`metro_albumart.c`) -- ambos pueden ser el buffer más grande de su
  propia pantalla, pero Now Playing y el visor son mutuamente
  excluyentes, nunca compiten al mismo tiempo.
- `imageviewer.rock` sigue en el árbol sin tocar (`apps/plugins/imageviewer.make`
  sigue compilando el `.rock`) -- Metro solo dejó de lanzarlo.
  `metro_photos_view()` (la función que lo hacía, `metro_photos.c`) se
  eliminó por completo, sin llamadores.

**Bug real encontrado y corregido verificando esto mismo, no un puerto
directo de Aura**: `compute_decode_and_display_size()` (el cálculo de
memoria de Aura, portado primero tal cual) solo comparaba
`decode_w*decode_h*sizeof(fb_data)` contra el tamaño del scratch --
probado en vivo contra un fixture 640×300 en modo cubrir,
`read_jpeg_file()` devolvió error (`ret <= 0`, "formato no soportado"
en pantalla -- fallo correctamente detectado, no corrupción de memoria,
la lección de M-033 sí se sostuvo). Causa real: el tamaño de
decodificación pedido (512×240) caía **entre** dos pasos de escala DCT
del propio decodificador JPEG (1/1 = 640×300, 1/2 = 320×150) -- para
llegar a un tamaño intermedio, el decodificador necesita el buffer de
resolución NATIVA completa como paso intermedio antes de su propio
re-escalado, no solo el tamaño final pedido. Fix: el chequeo de
memoria ahora presupuesta contra `max(decode_w*decode_h, src_w*src_h)`
en vez de solo `decode_w*decode_h`, y resta `JPEG_DECODE_OVERHEAD`
(`apps/recorder/jpeg_load.h`, ~38-39KB) del presupuesto en vez de
asumir que sobra espacio para el overhead del decodificador. Verificado
tras el fix: mismo fixture 640×300 en cubrir decodifica y se ve
correctamente recortado (`docs/screenshots/R2-F3-viewer-cover.png`).
Este hallazgo no se reportó de vuelta a Aura-Firmware (regla de
contención de la carpeta padre, y ese repo está fuera de alcance de
esta ronda) -- queda documentado aquí por si aplica también allá.

**`docs/DESVIACIONES.md` R2-1**: qué se pierde de `imageviewer.rock`
(zoom, paneo, slideshow, soporte PNG/GIF/BMP) -- misma pérdida que
Aura-Firmware ya aceptó para su propio visor (C.3).

**Verificado** (simulador): `docs/screenshots/R2-F3-viewer-fit.png`
(640×300, franjas arriba/abajo), `R2-F3-viewer-cover.png` (mismo
fixture, cubierto sin franjas), `R2-F3-viewer-small-cover.png`
(200×200, más chica que la pantalla, agrandada por muestreo sin
franjas), `R2-F3-viewer-next.png` (SCROLL_FWD a la siguiente foto del
mismo pivot), `R2-F3-menu-back-to-grid.png` (MENU vuelve a la
cuadrícula con la misma selección -- el fix real del reporte del
dueño), `R2-F3-music-keeps-playing.png` (hub sigue mostrando una pista
en reproducción después de pasar por el visor), `R2-F3-fade-mid.png`
(fundido de entrada a mitad de transición). Builds limpios (sim +
target ipod6g), 271 checks host-side, 0 fallos.

## M-059 — R2-F4/DD-11: `mpegplayer` (video) con estilo Metro -- port mecánico + restilado, sin menú de inicio

**Metodología**: se generó primero el `diff -u` de los 7 archivos
(Metro upstream, intacto, vs. Aura-Firmware) en un directorio de
scratch, como pide DD-11 -- nunca se copió un archivo entero de Aura.
El diff mismo separa con claridad "mecanismo" (se porta) de "diseño
Aura" (se reemplaza por el de Metro): `video_out_rockbox.c` resultó
ser 100% mecanismo puro, cero branding, portado casi verbatim; los dos
archivos con UI propia (`mpegplayer.c`, `mpeg_settings.c`) sí
necesitaron restilado real. Ver `MODIFICATIONS.md` R2-F4 para el
listado archivo por archivo.

**Lo que se portó tal cual (mecanismo)**: modo "cubrir" en
`video_out_rockbox.c` (`vo_draw_frame_cover()`, recorte+escalado
nearest-neighbor sobre la memoria sobrante del arena de libmpeg2,
`stretch_image_plane()` ya existente reutilizada con un segundo
llamador real) -- incluye el guard `scale_mode_locked` contra
codificadores MPEG-2 de GOP corto que repiten la cabecera de secuencia
en reproducción normal, un bug real que Aura-Firmware encontró y
documentó primero (D-308, consultado en solo lectura). `SETTINGS_VERSION`
5→6, `MPEG_TOGGLE_SCALE = BUTTON_SELECT`, y la eliminación completa del
menú de inicio interactivo (`get_start_time()`/`show_start_menu()`/
`draw_slider()`/`display_thumb_image()`/`show_loading()`/
`increment_time()`/`resume_options()`, ~400 líneas de `#define
MPEG_START_TIME_*` por target) -- `mpeg_start_menu()` ahora resuelve
directo a `MPEG_START_SEEK`, ignorando `settings.resume_options` a
propósito (correcto incluso si un `.rockbox/mpegplayer.cfg` viejo
todavía tiene `MPEG_RESUME_MENU_ALWAYS` guardado).

**Lo que se restiló para Metro, no copiado de Aura**:
- Widget de menú propio (`metro_menu_draw()`/`metro_menu_pick()`,
  reemplaza `rb->do_menu()`/`rb->set_option()`/`rb->set_int_ex()`)
  -- geometría de `metro_draw_rows()` (pitch 28px, x=12, encabezado
  caption y=4, seleccionado en fg, resto en secundario), **sin** la
  píldora redondeada de selección que Aura dibuja (su propio lenguaje
  Apple2026) -- rectángulos planos, como pide DD-11.
- Barra de progreso del OSD: dos colores planos (pista terciaria +
  relleno acento), sin borde -- reemplaza tanto el rectángulo con
  borde de Rockbox stock como la píldora de Aura.
- `metro_load_personalization()` (`mpegplayer.c`, en `osd_init()`, una
  sola vez -- no en cada redibujado del menú, ver
  `metro_osd_colors()`/`metro_language()` como accesores baratos hacia
  ese estado ya cargado) lee el esquema de **Metro** (`theme:0/1`,
  `accent:0..9`, `language:0/1`) de `/.rockbox/aura/aura.cfg`, no el de
  Aura (`theme`/`theme_id`/`accent_rgb24` + un `theme.cfg` por estilo
  custom) -- Metro no tiene temas instalables (M-012), así que solo
  elige entre los 10 acentos compilados de `metro_palette.h` y los
  tonos base oscuro/claro, exactamente lo mismo que `metro_theme.c`
  usa para el resto de la app. Tabla de 10 colores de acento duplicada
  localmente (`metro_accent_colors[]`, mismo orden que
  `metro_theme.c`'s `accent_colors[]`) porque un plugin no puede
  enlazar contra `apps/metro/metro_theme.c`, solo incluir el header
  puro del que ambos leen.
- Tabla bilingüe ES/EN propia (`metro_str()`/`enum metro_str_id`,
  `mpeg_settings.h`) para los ~35 textos que este plugin muestra
  (menús + 12 splashes de error) -- no incluye `metro_lang.c` (build
  separado), elige idioma vía el mismo `aura.cfg` ya leído.
- Ícono de estado (play/pausa/stop): se mantuvo el bitmap compilado de
  Rockbox stock (`osd.icons`), solo recoloreado al acento del usuario
  -- DD-11 pedía un ícono 100% geométrico (triángulo/dos barras, como
  F10) pero reemplazar el bitmap por dibujo vectorial es trabajo de
  diseño adicional fuera del alcance razonable de esta fase; el
  recoloreo (mecanismo que Aura ya portó) es un compromiso fiel de
  bajo riesgo. Ver `docs/DESVIACIONES.md` R2-3.

**Bug real encontrado y corregido, no en el port mecánico de Aura**:
al toggle a "cubrir" con un fixture 640×300, `read_jpeg_file()` no
aplica aquí (ese es el visor de fotos) -- el equivalente de video es
puramente aritmético (`stretch_image_plane()`, sin decodificador de
por medio), así que este archivo no repite el bug de memoria de
`metro_screen_photo_viewer.c` (M-058). No se encontraron bugs nuevos
en el port de video -- el mecanismo de Aura para "cubrir" en video
(a diferencia del de fotos) no depende de un decodificador JPEG con
sus propios límites de DCT, por eso no hereda ese problema.

**Verificado en vivo** (simulador): `docs/screenshots/R2-F4-video-play.png`
(video entra directo, sin menú de inicio -- confirmado con los 3
fixtures de video existentes, todos con entradas de reanudación en un
`mpegplayer.cfg` **versión 5** preexistente en el simdisk que sigue
cargando sin error en cada corrida, D.8 verificado de forma natural,
sin necesidad de fabricar el escenario), `R2-F4-video-settings.png`
(menú "Reproductor de video" → Ajustes/Salir, estilo Metro),
`R2-F4-display-options.png` (submenú completo: Mostrar FPS/Limitar
FPS/Omitir fotogramas/Modo de ajuste/Brillo de la luz de fondo),
`R2-F4-scale-mode.png` (Ajustar/Cubrir), `R2-F4-light-red-accent.png`
(mismo menú con `aura.cfg` puesto a tema claro + acento rojo a mano --
confirma que el plugin lee el esquema en vivo, no algo hardcodeado).
Colores verificados por inspección directa de valores en tiempo de
ejecución (no solo visual): `metro_accent_colors[]`/`osd.accent`
calculan exactamente el RGB565 esperado para cada acento (verificado
con `DEBUGF` temporal, revertido antes del commit).

**No verificado visualmente**: el contenido del panel OSD (barra de
progreso + tiempos) no se pudo distinguir con claridad en las capturas
headless -- el área donde debería estar aparece negra en todas las
capturas probadas (40, 80, 150 ticks), independiente del tema/acento
usado, lo que descarta que sea el propio panel OSD mal coloreado
(confirmado por los valores de color correctos vía `DEBUGF`) y apunta
a un artefacto de decodificación MPEG-2 en progreso (franjas aún no
decodificadas en negro) o a una limitación de temporización similar a
las ya documentadas en este proyecto (`docs/DESVIACIONES.md` F2-2/F2-3,
D-307 de Aura-Firmware para el propio frame YUV). El código del OSD en
sí (`draw_scrollbar_draw()`, `osd_refresh_status()`) es una adaptación
mínima y fiel del mecanismo ya probado de Aura, y los valores de color
que usa están verificados correctos por inspección directa -- el
riesgo real aquí es bajo, pero la confirmación visual queda pendiente
para cuando el dueño lo revise en el simulador interactivo o en
hardware real.

## M-060 — R2-F4 (continuación): rediseño real "Zune HD" del OSD de video

**Contexto**: tras M-059, el dueño verificó en el simulador interactivo
y confirmó que el menú nativo de Rockbox en efecto ya no aparece, pero
que el OSD en sí "dista mucho de parecerse al reproductor que tenía el
Zune" -- pidió explícitamente ayuda de diseño real, "como Microsoft lo
hubiera hecho con el Zune", no solo un port mecánico recoloreado. Se
propuso una maqueta HTML (artefacto, 5 cambios concretos con su
justificación) que el dueño aprobó sin cambios ("sí, me gusta esa
propuesta procede").

**Los 5 cambios aprobados y cómo se implementaron** (todo en
`apps/plugins/mpegplayer/mpegplayer.c`, sin tocar `video_out_rockbox.c`
ni `mpeg_settings.c` de M-059):

1. **Panel flotando sobre el video, sin fondo opaco** -- este punto
   resultó **no alcanzable tal cual se maquetó**, por una razón
   arquitectónica descubierta al leer `osd_show()` con cuidado antes de
   tocar código: Rockbox no compone el OSD sobre el video en vivo, lo
   **recorta**. `osd_show(OSD_SHOW)` llama
   `stream_vo_set_clip(&rc)` con `rc = {0, 0, SCREEN_WIDTH, osd.y}` --
   el hilo de video literalmente deja de dibujar en el área del OSD
   mientras está visible; al ocultarse, `stream_draw_frame(false)`
   redibuja esa zona desde cero. No es alpha-blending con "stale" video
   detrás -- es una zona muerta que el video nunca toca. Lograr un
   panel de verdad transparente exigiría leer el framebuffer de vuelta
   para componer manualmente, una reescritura mucho más grande y con
   riesgo de rendimiento real en hardware iPod 6G que no se puede medir
   esta ronda (Barrera B3). **Resolución aplicada**: no un panel
   "flotante" literal, sino uno mucho más chico y completamente plano
   -- `osd_refresh_background()` pasó de dibujar un bisel "elevado" de
   4 líneas de brillo/sombra (M-059 heredó esto de Rockbox stock) a un
   solo relleno plano sin borde, y la fila completa del OSD (ícono +
   tiempos + barra) se redujo de dos niveles a **una sola fila**, del
   tamaño del propio texto -- de un panel de ~28-34px de alto a uno de
   ~18-20px. El efecto visual apuntado ("ligero, no un cajón de chrome
   sobre el video") se logra por tamaño y planitud, no por
   transparencia real. Este ajuste de alcance no estaba en la maqueta
   aprobada -- se documenta aquí como corrección factual encontrada
   durante la ejecución (ver también `docs/DESVIACIONES.md` R2-4).
2. **Ícono de estado geométrico, sin sombra** -- `draw_status_icon()`
   (nueva, junto a `draw_tri_stepped()`) dibuja triángulos/barras con
   `draw_fillrect()` en pasos de 1px, igual que la barra de progreso ya
   hacía -- reemplaza `osd_refresh_status()`'s bitmap de 3 tamaños
   (`mpegplayer_status_icons_8/12/16x...`) con sombra de 1px offset en
   negro-blend. Los 5 estados (parado/pausado/reproduciendo/
   avance/retroceso) tienen su propio glifo; el color es siempre el
   acento del usuario.
3. **Tiempos en los extremos de la barra, no en una fila separada** --
   `osd_text_init()` se reescribió por completo: antes eran dos filas
   (ícono+tiempos arriba, barra abajo, con el ícono centrado sobre la
   barra); ahora es una sola fila, izquierda a derecha: ícono, tiempo
   transcurrido, la barra, duración total -- exactamente el layout de
   un transport bar real (Zune HD incluido). Se usó `vo_rect_set_ext()`
   (posición absoluta) en vez del truco original de "ancho como .r,
   luego offset" -- más fácil de verificar a mano sin arrastrar bugs de
   desplazamiento.
4. **Tipografía Metro real, no `FONT_SYSFIXED`** -- `metro-caption-14.fnt`
   (el mismo archivo que `apps/metro/metro_fonts.c` carga para el rol
   `MFONT_CAPTION`) se carga una vez en `osd_init()` vía
   `rb->font_load()` (un plugin no tiene `font_load_ex` con presupuesto
   de glifos, solo el `rb->` genérico) y se expone a través de
   `draw_setfont_osd()`, con reserva a `FONT_UI` si la carga falla. El
   riesgo de rectángulo de redibujado que tenía pendiente el plan
   anterior no aplicó: `osd_text_init()` mide el texto con la MISMA
   fuente que `osd_refresh()` usa para dibujar (ambos llaman
   `draw_setfont_osd()`), así que `osd.time_rect`/`osd.dur_rect` ya
   encierran exactamente lo que se dibuja -- no se necesitó ampliar el
   área de limpieza a mano.
5. **Barra delgada de 2px con punta redondeada, pista al 28% blanco** --
   `draw_scrollbar_draw()` ahora dibuja una línea de
   `METRO_OSD_BAR_LINE=2`px (antes ocupaba toda la altura del rect,
   ~12px) centrada dentro de un rect de `METRO_OSD_BAR_H=4`px, más un
   "thumb" cuadrado de esos mismos 4px en el borde de lo reproducido
   (recortado para no salirse nunca del rect que `osd.prog_rect` le
   asignó, ya que ese rect es lo que se limpia antes de cada redibujado
   y lo que se une al rectángulo sucio del blit final). La pista sin
   reproducir pasó de `osd.prog_trackcolor = s_metro_tertiary` (bloque
   sólido) a `draw_blendcolor(osd.bgcolor, MYLCD_WHITE, 71)` -- ~28% de
   blanco mezclado sobre el fondo del panel (`draw_blendcolor` interpola
   linealmente 0→255 entre sus dos colores, así que 71/255≈0.28).

**Limpieza asociada**: `osd.icons` y las tres externs
`mpegplayer_status_icons_8/12/16x8x1` (bitmaps que ya no se referencian
en código) se quitaron del struct/decls -- los `.bmp` fuente en
`apps/plugins/bitmaps/mono/` se dejaron intactos (recurso genérico de
Rockbox, tocar su pipeline de generación de bitmaps es una superficie
sin relación con este cambio). La variante de
`draw_oriented_mono_bitmap_part()` no-portrait quedó sin llamadores
tras quitar el ícono de bitmap -- se eliminó (la otra copia, la
`#ifdef LCD_PORTRAIT`, sigue viva vía `draw_putsxy_oriented()` para el
render de glifos). `draw_hline()` quedó igual de huérfana al aplanar
`osd_refresh_background()` -- eliminada también. Build limpio, cero
warnings, en sim (`build-sim`) y target (`build-ipod6g`).

**No verificado visualmente (igual que M-059, mismo límite, más
investigado esta vez)**: se intentó activamente encontrar una ventana
de ticks donde la captura headless mostrara el panel del OSD con
contenido legible. Con `firmware/tools/sim_shot.sh` variando ticks
finos (40/44/46/48/50/55/60/65/70/75/80/90) sobre la secuencia
`SCROLL_FWD,SELECT,SELECT`: en ticks ~40-50 aparece una franja negra
grande (bastante más alta que los ~18-20px que calcula
`osd_text_init()`) que por tamaño no puede ser solo el panel -- es
consistente con la conclusión que ya dejó `docs/DESVIACIONES.md` R2-3
para M-059: un artefacto de decodificación MPEG-2 en progreso (parte
del frame aún sin decodificar, no el panel). Para ticks ~55 en
adelante el video ya se ve a color completo y el panel ya no es
visible en absoluto -- la ventana en la que el panel está mostrado
*y* el video ya terminó de decodificar parece ser más angosta que la
granularidad de captura de un solo proceso permite aislar, igual que
"Committing database" en `docs/SUPERFICIES.md`. Se intentó además
`DEBUGF` temporal en `osd_text_init()` para confirmar los valores de
`osd.height`/rects en tiempo de ejecución sin depender de leer
píxeles -- resultó ser un callejón sin salida distinto: `apps/plugin.h`
compila `DEBUGF` como no-operación para plugins en este build (a
diferencia de `apps/metro/`, que sí imprime -- de ahí que
`metro_fonts: ... loaded as font id N` apareciera en el log pero nunca
una línea propia de `mpegplayer`); no se persiguió más allá porque
forzar una build de depuración del plugin es una superficie
nueva y no crítica para este cambio. La geometría se verificó en
cambio a mano (aritmética de `osd_text_init()` trazada término a
término contra `vo_rect_set_ext()`) y por la propia compilación limpia
en sim y target. Confirmación visual real queda, como en M-059, para
el simulador interactivo (lanzado al cierre de esta fase) o hardware.

## M-061 — R2-F4 (cierre): el rediseño Zune sí era invisible -- causa raíz `make` sin `make install`; menús reconstruidos con la anatomía real de página Metro

**El reporte del dueño que destapó todo**: tras M-060, el dueño verificó
en el simulador interactivo y vio el OSD *viejo* (dos niveles, volumen
en "-34dB", bisel) y los menús sin estilo Metro -- "¿en qué parte
supuestamente hiciste los ajustes?". Tenía razón: **ninguno de los
cambios de M-060 había llegado al simulador**.

**Causa raíz (metodológica, no de código)**: en este árbol, `make` a
secas compila y enlaza `mpegplayer.rock` en
`build-sim/apps/plugins/mpegplayer/` pero **no lo copia a
`simdisk/.rockbox/rocks/viewers/`** -- eso lo hace `make install`
(`tools/root.make`, `buildzip.pl` con `RBPREFIX=simdisk`). El
`.rock` del simdisk llevaba horas desactualizado (16:47 vs 18:09):
toda la verificación de M-060 -- capturas headless *y* el simulador
interactivo lanzado al cierre -- corrió contra el binario de M-059.

**Consecuencia que hay que corregir por escrito**: la sección "No
verificado visualmente" de M-059/M-060 y la conclusión de
`docs/DESVIACIONES.md` R2-3 ("el panel del OSD no se distingue en
capturas headless, artefacto de decodificación o límite de
temporización") quedan **desmentidas en su parte central**: con el
binario correcto instalado, el panel del OSD se captura perfectamente
(`docs/screenshots/R2-F4-zune-osd.png`, pausa a 40 ticks). La franja
negra gigante de ticks 44-48 sí era decodificación en progreso (eso
se sostiene), pero "el panel no aparece nunca" era simplemente el
panel viejo de un binario viejo. Ver `docs/DESVIACIONES.md` R2-5.

**Lo que M-060 dejó bien y esta pasada confirmó visualmente** (ya
instalado): fila única de transporte (ícono geométrico en acento +
tiempos en los extremos + línea de 2px al ~28% blanco), sin bisel,
panel de ~20px. `docs/screenshots/R2-F4-zune-osd.png`.

**Lo que faltaba de verdad y esta pasada agregó** (la crítica del dueño
sobre los menús seguía vigente incluso con el binario correcto: M-059
copió la *geometría* de filas pero no la *anatomía de página* de Metro):

1. **Anatomía completa de página** (`metro_page_chrome()`,
   `mpeg_settings.c`): réplica 1:1 de lo que toda pantalla real de la
   app dibuja -- ceja en caption minúsculas a (12,4)
   ("reproductor de video"), reloj `%02d:%02d` a la derecha, glifo de
   batería 18x9+nub (copiado de `metro_draw_battery()`, F10), y el
   elemento firma que faltaba por completo: **título de página gigante
   en minúsculas con la fuente display de 48px a (12,28)** -- la misma
   posición y fuente con que el hub dibuja sus pivotes
   (`metro_draw_pivots()`). Filas desde y=84 (antes 32), pitch 28 --
   los offsets exactos de `metro_draw.c`.
2. **Fuentes reales en los menús**: la pasada anterior de esta misma
   sesión ya había cambiado FONT_UI por `metro-list-20`/`metro-listsel-20`/
   `metro-caption-14`; ahora se suman `metro-display-48` (título) y
   `metro-title-28` (valor de la pantalla de brillo). Las cinco cargas
   via `rb->font_load()` **deduplican por ruta** contra las que
   `apps/metro/metro_fonts.c` ya dejó residentes al arrancar
   (`firmware/font.c`, `find_font_index()` → "already loaded") -- costo
   de RAM cero, nada que descargar al salir del plugin.
3. **Interacción real de Metro -- ciclar en el lugar**: los selectores
   de dos filas por valor (patrón Rockbox) se eliminaron; seleccionar
   una fila de valor **cicla el valor en su lugar**, con el valor
   actual siempre visible como subtítulo caption/terciario alineado a
   la derecha -- exactamente `general_on_select()` de
   `metro_screen_settings.c` (idioma/animaciones/gráficos ciclan así).
   La fila "Borrar todas las reanudaciones" muestra el conteo actual
   como valor (baja a 0 en el acto, confirmación visual gratis).
4. **Títulos de página**: strings nuevos en minúsculas y cortos
   (`video`, `ajustes`, `pantalla`, `audio`, `brillo` -- ES y EN) para
   que quepan en la fuente de 48px; la ceja reutiliza
   MSTR_VIDEO_PLAYER, ahora en minúsculas como toda ceja de la app.
5. **Volumen sin decibeles** (pedido explícito del dueño): el "-34dB"
   se reemplazó por una barra de nivel de 28px con el mismo lenguaje
   de la barra de avance (pista al 28% blanco + relleno acento),
   normalizando `(volume-min)/(max-min)` del rango real de
   SOUND_VOLUME. `osd_refresh_volume()` reescrita; ya no se mide texto.
6. **Pantalla de brillo**: misma anatomía de página (ceja + "brillo"
   en display-48) con el valor en title-28 y color acento.

**Verificado en vivo** (simulador, binario instalado con `make
install`): `R2-F4-zune-menu-video.png` (menú raíz), 
`R2-F4-zune-menu-ajustes.png` (valores "Uno"/"3" en línea),
`R2-F4-zune-menu-pantalla.png` (5 filas con valores en vivo),
`R2-F4-zune-menu-audio.png`, `R2-F4-zune-brillo.png` (valor en acento),
`R2-F4-zune-osd.png` (OSD pausado: ícono acento + 0:00/0:01 en los
extremos + línea 2px + barra de volumen). Build limpio sin warnings en
sim y target; 271 checks de los 4 suites en verde.

**Regla operativa nueva para el repo** (guardada también en memoria de
sesión): toda verificación en simulador -- headless o interactiva --
va precedida de `make install`, no solo `make`. `firmware/tools/sim_shot.sh`
no lo hace por sí mismo.

## M-062 — R3-F1: motor de miniaturas genérico (`metro_thumbs.c`), base para fotos de artista y quickplay

**Contexto**: primera fase de la ronda 3 (`PLAN-metro-r3-maestro.md` DD-1).
Fotos de artista (R3-F3) y quickplay (R3-F4) necesitan, cada uno por su
lado, una caché de N miniaturas pequeñas con ventana en RAM +
persistencia a disco -- el mismo problema que `metro_photo_thumbs.c`
(R2-F2/M-057) ya resolvió para `/Photos/`. En vez de triplicarlo, esta
fase lo generaliza primero, sin ningún cambio de cara al usuario.

**Qué cambió**: `metro_photo_thumbs.c/.h` → `metro_thumbs.c/.h`. La API
pasó de estar keyed por `(filename, mtime)` -- shape específico de
fotos -- a `metro_thumbs_get(source, ctx, index)`/`metro_thumbs_tick()`/
`metro_thumbs_reset()`, con un `struct metro_thumb_source` de dos
callbacks (`cache_key(ctx, index, ...)`, `decode(ctx, index, dst)`) que
cada fuente implementa. Se eligió indexar por `(source, ctx, index)` en
la cola pendiente -- no solo por una clave de texto, como tenía R2-F2
-- porque `metro_thumbs_tick()` necesita poder volver a llamar al
`decode()` correcto de la fuente correcta más tarde; una clave de texto
sola no alcanza para eso.

**Ventana de RAM única y compartida**: los 16 slots (~205 KB) siguen
siendo los mismos de R2-F2, pero ahora una sola instancia sirve a
cualquier fuente -- nunca hay más de una cuadrícula en pantalla a la
vez, así que no hay razón para pagar tres ventanas. Cambiar de fuente
(dejar Fotos por Artistas, por ejemplo) es un `metro_thumbs_reset()`,
igual que antes al cambiar de pivot dentro de Fotos.

**Caché en disco por subdirectorio**: `metro_settings_metro_cache_dir()`
(único punto autorizado a armar esta ruta, regla del `CLAUDE.md`) ganó
un parámetro `subdir` -- de `.../aura/metrocache/photos` fijo a
`.../aura/metrocache/<subdir>`, uno por fuente. El esquema de clave
`<nombre>.<mtime>.mth` se conserva tal cual: `remove_stale()` (que
antes recibía el nombre de archivo como prefijo explícito) ahora deriva
el prefijo genéricamente cortando la clave en el **último** punto --
funciona igual para las tres fuentes porque las tres siguen esa misma
convención `<estable>.<variable>` en su propio `cache_key()`.

**Decodificación JPEG-a-tile compartida, no parte del motor**: el
motor en sí (`metro_thumbs.c`) no sabe qué es un JPEG -- `cover_crop()`
y el `read_jpeg_file()` de R2-F2 se expusieron como
`metro_thumbs_decode_jpeg_cover(path, out)`, un helper público que
cualquier fuente cuyo `decode()` sea "leer un JPEG y recortarlo a
cuadrado" puede llamar (Fotos y, en R3-F3, Artistas -- Quickplay en
R3-F4 no, porque primero necesita resolver qué track de un álbum tiene
la carátula antes de decodificar nada). El scratch buffer de decode
sigue siendo uno solo, estático, compartido -- válido porque el
presupuesto de una decodificación por tick sigue vigente sin cambios.

**Fotos migradas en la misma fase**: `metro_screen_hub.c` gana un
`photo_thumb_source` chico (cache_key = `"<filename>.<mtime>"`, decode
= construir `/Photos/<filename>` y llamar al helper compartido) y
`photo_pivot_get_tile()` pasa a delegar en `metro_thumbs_get()` con esa
fuente -- es la única consumidora del motor en esta fase, y sirve de
prueba de regresión: el comportamiento tiene que ser bit a bit el
mismo que R2-F2.

**Verificado**: sim y target compilan limpio (los warnings de
`tile_cols` que aparecen en el build de target son preexistentes de
R2-F2 -- confirmado comparando contra el árbol sin estos cambios via
`git stash`, no algo que esta fase introdujo). 4 suites de test de host
en verde (271 checks, sin relación directa pero corridos como parte
del criterio de "nunca dejar el build roto"). Verificación visual en
el simulador (`make install` primero, regla de M-061): con el
`metrocache/photos/` previo borrado a mano para forzar el camino
"nada en caché", la cuadrícula muestra los 8 tiles de acento con
inicial de "todos" (idéntico a `R2-F2-photos-grid-loading.png`); tras
dejar correr el ciclo ocioso, los 8 tiles reales aparecen (idéntico a
`R2-F2-photos-grid.png`); `SCROLL_FWD`×5 mueve el borde de selección a
la fila 2 (idéntico a `R2-F2-photos-grid-scrolled.png`); y
`ls metrocache/photos/` vuelve a mostrar 8 `.mth` con el mismo esquema
`<archivo>.<mtime>.mth`. Al ser una regresión exacta (nada cambia de
cara al usuario), no se generaron capturas `R3-F1-*.png` nuevas -- las
tres de R2-F2 siguen siendo la documentación visual vigente de esta
superficie.

## M-063 — R3-F2: letras `.lrc` sincronizadas, modo de pantalla completa en Now Playing

**Contexto**: segunda fase de la ronda 3 (`PLAN-metro-r3-maestro.md`
DD-2/DD-3). Primer ítem grande del backlog de v1. A diferencia de casi
todo el resto de Metro, no es un port de mecanismo de Aura-Firmware:
solo la IDEA (letras sincronizadas) viene de ahí, el parser y el
render son diseño propio -- ver `docs/DESVIACIONES.md` R3-1 para el
porqué del modelo de memoria distinto.

**Parser (`metro_lrc.c/.h`, nuevo, C99 puro, host-testeable)**: acepta
`[mm:ss]`/`[mm:ss.f]`/`[mm:ss.ff]`/`[mm:ss.fff]` (normalizado a
milisegundos: 1 dígito ×100, 2×10, 3+ trunca a milisegundo), múltiples
timestamps por línea (comparten el mismo `offset` de texto, sin
duplicar), descarta silenciosamente tags de metadata (`[ar:...]`) y
líneas sin marca válida (mismo criterio que el parser de Aura,
`INVESTIGACION-metro-r3.md` A.1). `metro_lrc_parse(lrc, len)` opera
**en el propio `lrc->buf`** que el llamador ya llenó (lectura de
archivo real, o un fixture de test escrito directo) -- corta cada
línea en su lugar con un `\0`, sin buffer intermedio. `metro_lrc_find_active()`
es búsqueda binaria sobre las entradas (asume orden cronológico, no
reordena -- mismo supuesto que cualquier reproductor de `.lrc` real).
`metro_lrc_sibling_path()` deriva la ruta hermana (mismo directorio,
mismo nombre base, `.lrc`) con la misma convención que
`derive_sibling_path()` de Aura (consultado read-only) y que
`library-layout-v1.md` §3 ya documenta. 45 checks en
`apps/metro/test/test_lrc.c`, cero fallas a la primera corrida.

**Integración (`metro_screen_nowplaying.c`)**: `ensure_lyrics_loaded(id3)`
es una "caché de 1" keyed por `id3->path`, mismo patrón exacto que
`load_art()` de `metro_albumart.c` -- recarga solo cuando la pista
cambió, nunca por redibujo. La fila "letra" de Options (nueva, entre
Repeat y la cola) muestra "no disponible" si la pista actual no tiene
`.lrc` (o está vacío/sin líneas válidas) y en ese caso `SELECT` sobre
ella es un no-op -- la única forma de dejar el modo "no alcanzable sin
letra" sin agregar un tipo de fila deshabilitada nueva al widget
compartido de listas (que habría tocado `metro_screen_list.c`/`metro_draw.c`,
usado por toda la app, fuera del alcance de esta fase).

**Diseño visual (DD-2)**: modo de **pantalla completa**, no un panel
en el hueco de ~160×56px que `INVESTIGACION-metro-r3.md` A.6 encontró
libre -- reemplaza TODO el contenido normal de Now Playing (carátula,
título/artista/álbum, tiempos, barra, íconos de modo), deja el header
y el overlay de volumen (retroalimentación transitoria de una acción,
no parte del layout que este modo reemplaza). Línea activa en
`MFONT_TITLE` (28px) en `metro_color_fg()`, hasta 2 líneas de contexto
cada lado en `MFONT_LIST` (20px), atenuadas por distancia
(secundario/terciario). Alineado a la izquierda (x=12) -- Metro no
centra texto. **No se porta el "vidrio" translúcido de Aura**
(`INVESTIGACION-metro-r3.md` A.4): es gradiente + translucidez,
lenguaje Apple2026 ajeno al diseño plano de Metro; el atenuado al 30%
que Now Playing ya aplica sobre la carátula (F12, `METRO_NP_BG_ALPHA256`)
es justo lo que hace innecesario ese mecanismo -- Aura compone sobre
carátula a color pleno, Metro no.

**Desviación real encontrada** (no solo el modelo de memoria, que ya
estaba en el plan): DD-2/DA-3 pedían que el modo también se conmutara
con `SELECT` directo en Now Playing -- pero `player_mapping[]` ya tiene
`SELECT|REL`→Options y `SELECT|REPEAT`→shuffle, sin gesto libre. Se
resolvió a favor de la alternativa que la propia DA-3 ya ofrecía: **solo
desde la fila de Options**. Ver `docs/DESVIACIONES.md` R3-2.

**Verificado en vivo** (simulador, `make install` primero): con
`metro-test.lrc` (3 líneas reales + 3 tags de metadata) reproduciendo
la pista de prueba, Options muestra "letra: desactivado" → tras
activarla y volver, Now Playing dibuja el modo de pantalla completa
con la línea activa en grande y el contexto atenuado por distancia,
confirmando que el parser descartó correctamente los tres tags de
metadata sin que aparecieran como líneas. Con una pista sin `.lrc`
("Wheel & Click/Analog Dreams"), Options muestra "letra: no disponible"
y `SELECT` sobre esa fila no cambia nada (verificado repitiendo la
captura). Build limpio en sim y target (warnings de `tile_cols`
presentes son preexistentes, no de esta fase). 5 suites de test de
host en verde (316 checks).

## M-064 — R3-F3: fotos de artista como tiles cuadrados

**Contexto**: tercera fase de la ronda 3 (`PLAN-metro-r3-maestro.md`
DD-6), sobre el motor de miniaturas genérico de R3-F1. El pivot
Artistas de Música pasa de lista de texto a cuadrícula de tiles, con
la foto real cuando `artist_images.cfg` la mapea.

**Parser (`metro_artist_images_parse.c/.h`, nuevo, C99 puro,
host-testeable)**: formato **invertido** a propósito
(`CONTRATO-firmware-studio.md` §D.3) -- `<archivo>.jpg: <tag de
artista>`. El archivo es la clave porque es FAT-segura (nunca trae
`:`); el artista, que sí puede traerlo, es el valor -- por eso cortar
en el PRIMER `:` funciona sin ambigüedad. Descarta comentarios (`#`),
líneas en blanco, líneas sin `:`, y cualquier campo que exceda su
tope (128 B archivo / 64 B artista, los límites del propio contrato --
el de artista coincide con `METRO_MUSIC_ITEM_LEN`, el mismo tope que
`metro_music_artists()` ya usa para `tag_artist`).

**Índice (`metro_artist_images.c/.h`, nuevo, también host-testeable)**:
separado del parser de línea -- mismo split que Aura-Firmware usa de
verdad (`aura_artist_images_parse.c`, consultado read-only,
`INVESTIGACION-metro-r3.md` B.2) -- para poder probar las dos reglas
del contrato sin ninguna dependencia de Rockbox: valor duplicado →
gana la primera línea (variantes de archivo para el mismo tag de
artista, la primera mapeada se queda), y tope de 300 entradas
(coincide con `METRO_MUSIC_MAX_ITEMS`). 358 checks en
`apps/metro/test/test_artist_images.c`, incluido un caso que agrega
320 líneas y verifica que el índice se detiene exactamente en 300 sin
perder ninguna de las primeras 300.

**Integración (`metro_music.c`, `metro_settings.c/.h`,
`metro_screen_hub.c`)**: dos rutas nuevas y separadas, ambas armadas
solo en `metro_settings.c` (regla de rutas del `CLAUDE.md`) --
`.../aura/artist_images.cfg` (el índice de Studio) y `.../aura/artists/`
(las fotos fuente de Studio, distinto de `metro_thumbs.c`'s propio
`.../aura/metrocache/artists/`, la caché derivada de 80×80 de Metro).
`metro_music_reload_artist_images()` lee el `.cfg` línea por línea
(`read_line()`, mismo patrón de streaming que `metro_media_categories.c`,
sin cargar el archivo completo a RAM) y escanea `artists/` con
`metro_fsutil_list_by_ext_mtime()` (ya probado por `metro_photos.c`)
para el mtime de cada foto -- la clave de caché de `metro_thumbs.c`
necesita ese mtime, no solo el nombre. Se llama junto al resto de
`music_lists_refresh()`, mismo "refresca al entrar" que ya seguían
Artistas/Álbumes/Canciones/Géneros. El pivot Artistas gana
`tile_cols`/`get_tile` (`artist_thumb_source`, una fuente más del motor
de R3-F1) -- artista sin foto mapeada cae al tile de acento con
inicial ya existente, sin código nuevo para ese caso.

**Bug real encontrado y corregido en el camino** (no en el plan): las
tres fotos de artista mapeadas nunca decodificaban -- ver
`docs/DESVIACIONES.md` R3-3. Causa: `metro_thumbs_decode_jpeg_cover()`
(heredado de R2-F2) presupuestaba su scratch contra el tamaño del
tile (80px), asumiendo -- válido para `/Photos/`, no para fotos de
artista -- que la fuente siempre sería mucho más grande. Fotos de
artista están limitadas a ≤128px por contrato, justo en el hueco entre
los escalones de potencia de 2 del decodificador JPEG (128×1/2=64<80),
donde el decodificador cae a resolución nativa completa -- el mismo
`JPEG_DECODE_OVERHEAD` que R2-F3 ya documentó para el visor de fotos.
Corregido presupuestando el scratch contra el límite del contrato
(128px) en vez del tamaño del tile.

**Verificado en vivo** (simulador, `make install` primero, tagcache
reconstruido para que el nuevo track de prueba entrara): cuadrícula de
7 artistas -- 3 con foto real (incluida "DJ Twist: Remix Unit", el
caso con `:` en el tag, resuelto por su valor completo) y 4 con tile
de acento e inicial (uno de ellos, "artista desconocido", el caso sin
tag en absoluto). Seleccionar un artista con foto sigue entrando a sus
álbumes sin cambios. Fixtures nuevos en `gen_test_media.sh`: 3 JPEG de
128×128 vía el mismo pipeline ffmpeg→sips ya probado
(`gen_cover_jpg()`), `artist_images.cfg` de muestra, y una pista más
("Colon Artist Test") con artista `DJ Twist: Remix Unit` para ejercitar
el caso del `:`. Build limpio en sim y target (mismos warnings
preexistentes de `tile_cols`, nada nuevo). 6 suites de test de host en
verde (674 checks).

## M-065 — R3-F4: Quickplay (álbumes recientes) + runtime DB en la higiene

**Contexto**: cuarta fase de la ronda 3 (`PLAN-metro-r3-maestro.md`
DD-4/DD-5), sobre el motor de miniaturas de R3-F1. Música gana un
pivot nuevo, primero de la lista (DA-1, la opción recomendada por el
plan): los álbumes reproducidos más recientemente, al estilo "landing
surface" de Zune HD.

**`global_settings.runtimedb = true`** (`metro_apply_hygiene()`,
`metro_main.c`): Rockbox nunca escribe `tag_lastplayed` a menos que
este flag esté encendido -- default `false`, y Metro no tiene menú
propio para exponerlo (`INVESTIGACION-metro-r3.md` D.1 ya había
ubicado esto: los escritores, `tagtree_buffer_event()`/
`tagtree_track_finish_event()`, ya se registran incondicionalmente
desde el propio `tagtree_init()` de `apps/main.c` -- solo faltaba la
bandera que los habilita). Puramente local (historial de reproducción
del propio dispositivo, en su propio disco, nunca sale a ningún lado)
-- misma clase de "decidirlo por el usuario" que el resto de la
higiene ya fuerza.

**`METRO_NAV_MAX_PIVOTS` 6→8** (`metro_nav.h`): Música pasa de 5 a 6
pivots (Quickplay + Artistas/Álbumes/Canciones/Géneros/Playlists); se
dejó un pivot extra de margen sobre el mínimo exacto para que el
siguiente pivot nuevo no necesite otro cambio de una línea de
inmediato.

**Consulta (`metro_music_recent_albums()`, `metro_music.c`)**: no
existe una query agrupada para "los álbumes con el `tag_lastplayed`
más reciente" -- ese tag es por-PISTA, no algo que tagcache pueda
agrupar/ordenar por álbum directamente (`INVESTIGACION-metro-r3.md`
D.2). Se resolvió escaneando **todas** las pistas por `tag_filename`
(no `tag_title`, a propósito -- ver más abajo), agregando el
`max(tag_lastplayed)` por nombre de álbum, y resolviendo cada pick de
vuelta a un `tag_album` real de `metro_music_albums()` por coincidencia
de nombre (necesario para que `on_select()` pueda filtrar canciones
por ese álbum). `metro_music_track_path()` (nueva, junto a la anterior)
resuelve la ruta real de un `idx_id` vía `tagcache_retrieve()`, para
que la resolución de carátula tenga un path real que leer.

**Por qué `tag_filename` y no `tag_title`** (como sí hace el patrón
"reproducir todo" ya existente en este archivo): `tagcache_search_set_uniqbuf()`
deduplica por VALOR -- dos pistas con el mismo título en álbumes
distintos colapsarían en una sola bajo `tag_title`, subcontando
reproducciones reales. `tag_filename` es inherentemente único por
archivo físico, así que no hay ese riesgo (confirmado leyendo
`add_uniqbuf()` en `apps/tagcache.c`: un buffer de uniqbuf lleno deja
de deduplicar, nunca excluye resultados de forma incorrecta -- seguro
igual para un escaneo de biblioteca completa con el `s_uniqbuf` de
2048 entradas que ya existía).

**Carátula por track arbitrario (`metro_albumart_decode_track_cover()`,
`metro_albumart.c/.h`)**: Quickplay necesita la carátula de un track
representativo del álbum, no la del track que esté sonando ahora mismo
(`audio_current_track()` puede no aplicar -- quizás no está sonando
nada). Se usa `get_metadata()` (`lib/rbcodec/metadata/metadata.h`),
NO `get_temp_mp3entry()` (`playback.h`): ese es scratch memory del
motor de reproducción, con su propio locking, pensado para otra cosa
(espiar el siguiente track) -- usarlo aquí competiría con el hilo de
audio por algo que no tiene relación. `get_metadata()` es un utility
independiente; `tagcache.c` mismo lo usa igual, para leer tags de un
archivo arbitrario sin relación con lo que esté sonando. El
`mp3entry` de trabajo es `static` (no en el stack): la struct pesa
~1.5-2.4KB (`ID3V2_BUF_SIZE` hasta 1800B + `id3v1buf[4][92]`=368B +
`toc[100]`), demasiado para un stack frame cómodo en los hilos
pequeños de Rockbox.

**Downscale, no un segundo decode JPEG** (`downscale_to_tile()`,
mismo archivo): decodifica una vez a 136×136 (el tamaño ya probado que
usa Now Playing) y reescala los píxeles YA decodificados a 80×80 por
nearest-neighbour -- deliberadamente NO un segundo decode JPEG a 80px,
que arriesgaría el mismo hueco `JPEG_DECODE_OVERHEAD` que R3-F3
encontró para fotos de artista (`docs/DESVIACIONES.md` R3-3) para
cualquier carátula que caiga cerca de ese tamaño. Ambos tamaños son
cuadrados, así que es un escalado simple, sin recorte "cover".

**Bug real encontrado y corregido en el camino (no en el plan)**: `metro_albumart_decode_track_cover()`
comparte `s_scratch`/`METRO_ALBUMART_SCRATCH_SIZE` con la caché-de-1 de
`metro_albumart_load_current()` (`s_loaded_path`/`s_loaded`, la de Now
Playing). Sin invalidación, decodificar la carátula de un álbum de
Quickplay sobrescribiría el contenido de ese buffer sin que la
caché-de-1 de Now Playing se enterara -- volver a Now Playing sobre la
MISMA pista que ya estaba cacheada antes de visitar Quickplay serviría
la carátula equivocada (la última que Quickplay decodificó), no la
real. Corregido con `s_loaded = false;` al entrar a la función nueva,
forzando un redecode real la próxima vez que se llame a
`metro_albumart_load_current()`.

**Segundo bug real, más grave, encontrado verificando el criterio de
"el orden se conserva tras reiniciar el simulador"**: ver
`docs/DESVIACIONES.md` R3-4 -- Metro nunca llama `tagcache_shutdown()`
en ningún apagado, así que las escrituras encoladas (`playcount`/
`playtime`/`lastplayed`, la cola async de `tagcache.c`) se perdían en
cualquier apagado normal salvo que la cola se llenara sola (32
entradas). Corregido en `metro_main.c`, en el propio manejo de
`SYS_POWEROFF`/`SYS_REBOOT` que este archivo ya tenía.

**Verificado en vivo** (simulador, `make install` primero): biblioteca
sin historial → `LANG_QUICKPLAY_EMPTY` ("sin historial todavía --
reproduce algo primero"), no un pivot vacío genérico ni tiles falsos.
Tras reproducir tres álbumes reales en orden (First Light, Night
Drive, Analog Dreams -- lo suficiente para que la cola de tagcache se
desbordara sola y se volcara a disco), Quickplay los muestra en orden
inverso de reproducción (más reciente primero) con su carátula real
-- "Night Drive" (deliberadamente sin `cover.jpg` en los fixtures de
prueba) cae al tile de acento con inicial "N", el fallback correcto,
no un fallo. Seleccionar un tile entra a las canciones de ese álbum
sin cambios respecto al pivot Álbumes existente. El mismo grid,
byte-idéntico, aparece tras reiniciar el simulador de cero (sin tocar
ningún botón salvo entrar a Música) -- confirma persistencia real en
disco, no solo en RAM de ese proceso. Build limpio en sim y target
(mismos warnings preexistentes de `tile_cols`/`empty_message`, nada
nuevo). 6 suites de test de host en verde (678 checks), incluido un
caso nuevo para el tope de pivots (`test_max_pivots_is_respected`).

## M-066 — R3-F5: calificación (import de una vía) + fila de estrellas en Now Playing

**Contexto**: quinta fase de la ronda 3 (`PLAN-metro-r3-maestro.md`
DD-7), sobre el `runtimedb` que R3-F4 ya encendió. Metro importa
`ratings.cfg` de Studio hacia `tag_rating` (nativo de Rockbox, no una
tabla propia) y agrega una fila de calificación en Options de Now
Playing, mismo patrón "ciclar en el lugar" que Shuffle/Repeat.

**Primer paso real, antes de escribir el import**: el propio DD-7
pedía resolver, EN VIVO, una contradicción entre dos hallazgos de la
Fase 2 (`INVESTIGACION-metro-r3.md` C.2 vs D.1) -- ¿necesita Metro su
propio listener de `PLAYBACK_EVENT_TRACK_BUFFER` para restaurar
`tag_rating` en `id3->rating` al cambiar de pista (como sí necesita
Aura-Firmware), o el listener de stock que `apps/tagtree.c` ya
registra (`tagtree_buffer_event()`, vía el `tagtree_init()`
incondicional de `apps/main.c`, jamás tocado por Metro) alcanza solo?
**Verificado en vivo: alcanza solo.** Aura necesita su propio callback
porque nunca llama `tagtree_init()` en absoluto (árbol de navegación
propio) -- Metro sí lo hereda, así que el mismo listener que ya existía
antes de esta fase ya hacía el trabajo. **Ningún callback nuevo se
registró.** Ver `docs/DESVIACIONES.md` R3-5 para el detalle de la
verificación y por qué casi produce un falso negativo.

**Import (`metro_sync.c`, `import_ratings()`)**: calcado del propio
`import_ratings_from_studio()` de Aura (`aura_music.c`, consultado
read-only) salvo la ruta -- `metro_settings_ratings_cfg_path()` nueva,
mismo patrón `.../aura/ratings.cfg` que `artist_images_cfg_path()`
(regla de rutas del `CLAUDE.md`). Mismo parseo línea por línea que
`metro_media_categories.c` (`read_line()` + `settings_parseline()`,
sin cargar el archivo completo a RAM). Formato `<ruta absoluta>:
<rating 0-10>`; `tagcache_find_index()` resuelve la ruta, `rating`
clampeado a `[0,10]`, `tagcache_update_numeric()` escribe. Llamado
desde `job_ended()`'s camino de éxito real (`ok == true`), no desde el
atajo `!s_marker.music` de `start_job()` -- ese atajo no tocó tagcache
en absoluto esta vez, así que "al terminar bien [el import de
música]" (`library-layout-v1.md`) no aplica ahí.

**Fila de calificación (`metro_screen_nowplaying.c`)**: 5 estrellas,
mapeadas ×2 al 0-10 nativo -- mismo convenio de números pares que
`commit_rating()` de Aura (consultado read-only), así que una
calificación puesta de cualquiera de los dos lados significa lo mismo
en disco. `cycle_rating()` escribe `id3->rating` en memoria de
inmediato (visible sin esperar ningún evento) y encola el mismo valor
a tagcache vía `tagcache_update_numeric(id3->tagcache_idx - 1,
tag_rating, ...)` -- mismo patrón `-1` que `apps/tagtree.c` usa
internamente (`tagcache_idx` se guarda `+1`, 0 significa "sin
asignar").

**Bug real encontrado verificando el criterio "persiste tras
reiniciar"** (no en el plan): `tagcache_update_numeric()` solo ENCOLA
la escritura (cola async de 32 entradas de `apps/tagcache.c`) -- sin
forzar el volcado, ni el import ni una calificación puesta a mano
llegaban a disco (ni siquiera a la caché de RAM que
`tagcache_get_numeric()` lee) hasta que la cola se llenara sola o el
dispositivo apagara de verdad. Es la MISMA cola que R3-4
(`docs/DESVIACIONES.md`) ya encontró para `lastplayed`, pero un gatillo
DISTINTO: R3-4 arregló el apagado (`metro_main.c`, `SYS_POWEROFF`/
`SYS_REBOOT`) -- útil para cualquier escritura pendiente en general,
pero no ayuda a que una calificación recién importada o puesta a mano
se vea DURANTE la misma sesión, potencialmente nunca si el usuario no
apaga el dispositivo pronto. Corregido con `tagcache_shutdown()`
explícito al final de `import_ratings()` y al final de
`cycle_rating()` -- en este target (`ipod6g`, sin
`HAVE_EEPROM_SETTINGS`) esa función es literalmente
`run_command_queue(true)`, un volcado síncrono sin más efectos
secundarios, seguro de llamar a media sesión, no solo al apagar de
verdad. Encontrado con `DEBUGF` temporal en `import_ratings()` (path,
línea, `idx_id`, rating escrito -- confirmó que el import en sí
funcionaba perfecto) y en `rating_subtitle()` (`id3->path`/`rating`/
`tagcache_idx` en cada redibujo -- confirmó que la LECTURA veía un
valor viejo), ambos revertidos antes del commit.

**Verificado en vivo** (simulador, `make install` primero, con un
`ratings.cfg` de muestra en el simdisk -- 3 pistas reales + 1 ruta que
ya no existe, para ejercitar el "ignorado, no error" de
`tagcache_find_index()`): tras "sincronizar ahora" (Ajustes →
Biblioteca), "Sunrise" (calificada 10 en el archivo) muestra "5/5" en
Options sin haber tocado la fila nunca (`docs/screenshots/R3-F5-rating-imported.png`)
-- la lectura corre por el listener de stock, confirmando la
verificación de arriba. Calificar "Slow Turn" (sin calificación
previa) desde Options, en el lugar, hasta "3/5"
(`docs/screenshots/R3-F5-rating-set.png`) sobrevive cambiar de pista
(siguiente/anterior) y volver. **Ambos casos** -- la importada y la
puesta a mano -- se ven idénticos tras reiniciar el simulador de cero
(dos procesos nuevos, sin ningún botón salvo navegar a la pista),
confirmando que el fix de `tagcache_shutdown()` alcanza para el
criterio de "hecho" de la fase sin depender de un apagado real. Build
limpio en sim y target (mismos warnings preexistentes de `tile_cols`,
nada nuevo). 6 suites de test de host en verde (678 checks, sin
cambios -- `metro_sync.c`/`metro_screen_nowplaying.c` dependen de
tagcache real, no host-testeables, mismo patrón que el resto de este
archivo).

## M-067 — R3-F6: temporizador de sueño y presets de EQ como filas de Ajustes (cero pantallas nuevas)

**Contexto**: sexta fase de la ronda 3 (`PLAN-metro-r3-maestro.md`
DD-10), la más barata del backlog chico -- dos filas nuevas en el
pivot General de Ajustes, cero pantallas propias, cero cambios a
`apps/settings.h` ni a ningún otro archivo fuera de `apps/metro/`
(`git diff --stat` verificado como criterio explícito de la fase).

**Temporizador de sueño**: `set_sleeptimer_duration(int minutes)`/
`get_sleep_timer()`/`get_sleep_timer_active()`
(`firmware/powermgmt.c`) son API de core sin ninguna pantalla nativa
detrás -- ni `do_menu()` ni nada que las reglas duras de Metro
prohíban (`INVESTIGACION-metro-r3.md` F.1: Aura tampoco implementa
esta feature, no hay puerto que hacer). La fila cicla
`desactivado→15→30→60→90 min` -- el índice de qué paso sigue
(`s_sleep_step`) se guarda aparte de `get_sleep_timer()`, que cuenta
hacia ATRÁS en tiempo real (segundos restantes, no una posición
estable en la lista de pasos) y por eso no sirve para decidir el
próximo valor a ciclar, solo para el subtítulo. Puramente de sesión, a
propósito: un temporizador de sueño reinicia en cada boot en el propio
Rockbox stock también, no hay nada que persistir.

**Presets de EQ**: tabla **propia** de Metro (`eq_shapes[]` +
`eq_preset_gains[][]`, `metro_screen_settings.c`) -- NO el `extern
eq_defaults[EQ_NUM_BANDS]` de 4 líneas que Aura sí necesitó en
`apps/settings.h` para reusar los valores default de stock
(`INVESTIGACION-metro-r3.md` F.2). Metro no reusa esos defaults, así
que no necesita ese `extern` ni ningún cambio a `settings.h` -- API de
aplicación pura (`dsp_eq_enable()`/`dsp_set_eq_coefs()`,
`lib/rbcodec/dsp/eq.h`), ya accesible sin ese cambio. La FORMA de cada
banda (tipo/frecuencia de corte/Q) es igual en los 4 presets -- el
mismo layout de 10 bandas que el menú EQ de stock usa por default
(`apps/settings_list.c`, valores genéricos de ingeniería de audio, no
código de Aura) -- solo la GANANCIA por banda cambia entre "plano"
(todo en 0, EQ deshabilitado por completo vía `dsp_eq_enable(false)` --
bypass real, más barato y más correcto que 10 bandas midiendo cero),
"graves" (boost en 32/64/125 Hz), "voz" (corte leve en 32/64 Hz, boost
en 500/1000/2000 Hz) y "brillante" (boost en 4000/8000/16000 Hz).
Ganancia en décimas de dB (`apps/menus/eq_menu.h`:
EQ_GAIN_MIN/MAX = -240/240, o sea ±24.0 dB) -- confirmado leyendo el
formateo `"%2d.%d"` del propio menú de EQ de stock, no asumido.
También de sesión, no persiste en `metro_settings`/`aura.cfg` -- ni el
plan ni el criterio de "hecho" de la fase piden sobrevivir un reinicio,
y evita tocar el archivo de contrato con Aura Studio
(`aura.cfg`/`AuraDeviceProbe`) para algo que no lo necesita.

**Ambas filas al pivot General** (no uno de "Sonido" -- ese pivot no
existe en Metro hoy; los tres reales son General/Pantalla/Acerca de,
`INVESTIGACION-metro-r3.md` F.4 tenía un nombre equivocado ahí. DD-10
ya cubría este caso: "si no existe uno de sonido, ambas al general").
`general_count()` pasa de 5 a 7 filas; Biblioteca y Restablecer
ajustes se corren de 3/4 a 5/6. "Restablecer ajustes" también reinicia
sueño (a desactivado) y EQ (a plano) -- no forma parte del criterio de
"hecho" de la fase, pero es la consistencia obvia con el resto de esa
fila ya existente.

**Verificado en vivo** (simulador, `make install` primero):
`docs/screenshots/R3-F6-settings-sleep.png` (fila ciclada a "15 min",
subtítulo real) y `R3-F6-settings-eq.png` (fila ciclada a "graves").
El temporizador real arma -- confirmado con `DEBUGF` temporal en
`cycle_sleep()` (revertido antes del commit): tras ciclar a 15 min,
`get_sleep_timer()` devuelve 900 (segundos) y `get_sleep_timer_active()`
es verdadero; a 30 min, 1800. Cambiar de preset de EQ altera las
bandas -- verificado por inspección de estado (mismo `DEBUGF` temporal
en `cycle_eq()`, no por oído, tal como pedía el criterio de "hecho" de
la fase): preset "graves" con banda0=+6.0dB (60), "voz" con
banda0=-2.0dB (-20) y banda5=+4.0dB (40), coincidiendo exactamente con
la tabla. `git diff --stat` confirma **cero archivos fuera de
`apps/metro/`** en el diff completo de la fase. Build limpio en sim y
target (mismos warnings preexistentes de `tile_cols`, nada nuevo). 6
suites de test de host en verde (678 checks, sin cambios --
`metro_screen_settings.c` depende de `firmware/powermgmt.c`/
`lib/rbcodec/dsp/eq.c` reales, no host-testeable, mismo patrón que
`metro_sync.c`/`metro_screen_nowplaying.c` en R3-F5).

## M-068 — R3-F7: candado de interfaz por PIN, sin parche de core y con salida de emergencia probada

**Contexto**: séptima fase de la ronda 3 (`PLAN-metro-r3-maestro.md`
DD-8), la única que intercepta el bucle principal antes del despacho de
pantallas -- por eso el plan la marcaba para escalar de modelo. Clave de
4 dígitos marcada con la rueda, tres estados, persistencia en
`aura.cfg`, y la salida de emergencia como parte del criterio de
"Hecho", no como extra.

**Lo que NO se hizo, y es la decisión central**: Aura-Firmware (D-238)
parcha `apps/misc.c` -- un archivo de **core** -- para diferir el
montaje USB mientras el candado está activo. Metro no. El volumen es
FAT sin cifrar: diferir el montaje protege la interfaz, no los datos, y
la interfaz ya la protege el candado. Detalle completo del costo evitado
(incluido el caso borde del `seqnum`, que **deja de existir** en vez de
quedar sin resolver) en `docs/DESVIACIONES.md` R3-6. Consecuencia
deliberada y declarada en la ayuda: **Metro no ofrece protección de
datos**.

**Tres estados** (`metro_screen_lock.h`): NONE (sin clave), ARMED (con
clave, desbloqueado en esta sesión) y ACTIVE (bloqueando ahora).
Configurar la clave deja ARMED, **no** ACTIVE -- el dueño acaba de
entrar a Ajustes, no tiene sentido bloquearlo en ese instante; el
candado se cobra en el siguiente arranque. Aura llegó al mismo diseño
pero por el camino largo: con un solo booleano recibió reportes del
dueño en las dos direcciones (configurar "no servía de nada" / bloqueaba
de inmediato). **Rearmar en cada arranque, incondicionalmente**, es la
única parte del diseño de Aura que se copia tal cual, porque su
razonamiento es correcto y no obvio: todo apagado real termina sin
retorno, así que forzar la condición al arrancar cierra de golpe todos
los caminos de apagado (`SYS_POWEROFF`, batería crítica, el hold de PLAY
del driver) sin interceptar cada uno ni arriesgar que un camino nuevo se
olvide de rearmar.

**Interceptación** (`metro_main.c`): dos llamadas, cada una con su
razón. Una en el arranque, después de `metro_screen_list_init()` y
**antes** de `metro_disk_handoff()` -- si fuera después, la pantalla de
"actualizando biblioteca" (`metro_run_sync_screen_if_needed()`) se
dibujaría por encima del candado. Otra al principio del `while(1)`,
antes de calcular el contexto y de cualquier despacho: es **esa** línea,
y no el orden de las llamadas de arranque, la que vuelve
*estructuralmente* cierto que ninguna otra pantalla es alcanzable con
el candado puesto. La pantalla corre su propio bucle de entrada, el
mismo patrón modal que `metro_run_sync_screen_if_needed()` y
`metro_widgets_confirm()` ya usaban -- no hubo que reestructurar el
despacho.

**Dos mejoras deliberadas sobre Aura**, ambas verificadas en vivo:

1. **Retroceso con MENU.** Aura no tiene: equivocarse a media clave
   obliga allá a completar los cuatro dígitos y fallar a propósito. En
   Metro MENU borra el dígito anterior; en el primero, cancela al
   configurar y no hace nada al desbloquear (no existe un "cancelar"
   legítimo ahí).
2. **Falla ABIERTO, nunca cerrado.** La clave se guarda como **cadena**
   de 4 dígitos, no como entero. Con entero -- como Aura -- `"0000"` y
   "clave ausente" son el mismo valor 0, así que un `aura.cfg` con
   `screen_lock_enabled: 1` pero sin línea de clave deja el aparato
   bloqueado con una clave que nadie configuró. Metro valida que sean
   exactamente 4 dígitos y trata cualquier otra cosa (ausente, truncada,
   con letras) como "sin candado". Verificado con los tres casos
   corruptos: ninguno bloquea.

**Persistencia**: `screen_lock` y `screen_lock_pin` en `aura.cfg`, texto
plano y dicho así en la documentación -- coherente con que el candado
sea de interfaz. Se escriben **solo cuando hay candado**, no como un
`screen_lock: 0` permanente: así la salida de emergencia ("borra estas
dos líneas") deja un archivo que no las vuelve a hacer crecer solo.

**Salida de emergencia** (`docs/ESTADO_FINAL.md` + `docs/GUIA_FLASHEO.md`):
conectar por USB -- posible **porque** Metro no difiere el USB, ver
arriba -- y borrar las dos líneas. Surte efecto sin reiniciar: la
pantalla de candado relee los ajustes del disco al terminar cada sesión
USB. Ese `metro_settings_load()` cubre además el reverso, que era el
riesgo real de no hacerlo: sin releer, un guardado posterior habría
regenerado el `aura.cfg` desde la copia vieja en RAM y **resucitado la
clave recién borrada**. Aura-Firmware no tiene esta salida ni la
documenta.

**Teclas**: contexto `MCTX_LOCK` propio. Rueda marca el dígito, SELECT
confirma y avanza, MENU retrocede. LEFT/RIGHT/PLAY y MENU sostenido
quedan **sin mapear a propósito** -- con el candado puesto el aparato no
debe ofrecer ningún control, ni siquiera play/pausa, y no mapearlos es
lo que garantiza que resuelvan a `ACTION_NONE`. `metro_input_next()` se
llama con timeout (HZ/2), no bloqueando: el encabezado trae reloj y
batería y tiene que refrescarse solo (Aura bloquea indefinidamente y su
pantalla de candado no tiene reloj).

**Verificado en vivo** (simulador, `make install` primero), los ocho
criterios de la fase: `docs/screenshots/R3-F7-lock-setup.png`
(configurar), `R3-F7-lock-confirm.png` (segunda captura) y
`R3-F7-locked.png` (desbloqueo al arrancar, tras reiniciar el simulador
con la clave ya guardada). La clave marcada llegó a `aura.cfg` como
`screen_lock_pin: 1234` (dígitos correctos, doble captura funcionando).
Clave incorrecta (9999) rechazada con el aviso correcto y sin
desbloquear; clave correcta desbloquea al hub. **Ninguna otra pantalla
alcanzable**: 14 pulsaciones de escape (MENU, MENU sostenido, LEFT,
RIGHT, PLAY en toda combinación) dejan la pantalla de candado
intacta. USB con el candado puesto se atiende normal (token
`USB_INSERT` del inyector: aparece la pantalla de conexión). MENU
cancela la configuración y no escribe nada al `aura.cfg`. La salida de
emergencia **probada de verdad**: borrar las dos líneas y arrancar deja
el aparato en el hub, sin candado. Los tres casos de archivo corrupto
fallan abiertos. **Cero archivos de core en el diff** (`git status`
como criterio explícito: solo `apps/metro/` y la línea de `apps/SOURCES`
que lista el `.c` nuevo, la misma modificación ya documentada en
`MODIFICATIONS.md` desde F1 y que cada fase con archivo nuevo amplía).
Builds limpios en sim y target. 6 suites de test de host en verde (678
checks, sin cambios -- este módulo depende del LCD y del bucle de
entrada reales, no host-testeable).

## M-069 — R3-F8: CONTINUUM, el título de la fila vuela a la ceja de la página nueva

**Contexto**: octava fase de la ronda 3 (`PLAN-metro-r3-maestro.md`
DD-9), puro pulido visual sobre el motor de transiciones de F11/F12.
Lo que el usuario acaba de elegir es lo único que **no** gira durante
el PUSH: se queda plano y legible mientras el resto de la página hace
su turnstile detrás, y viaja hasta su lugar nuevo. Es el continuum real
de WP7, no un efecto inventado.

**El destino resultó ser la ceja, no el título grande** — y eso cambia
la forma de la animación (escalera de un escalón en vez de tres, texto
que encoge en vez de crecer). Detalle completo, con por qué la puerta
de igualdad que DD-9 define habría dejado la función como código muerto
si se comparaba contra el título grande, en `docs/DESVIACIONES.md` R3-7.

**Las tres puertas de DD-9, tal cual**: (1) nivel de FX —
`animations=all` **y** `graphics=full`, el mismo `if` que ya elegía
turnstile sobre slide; (2) solo PUSH hacia adentro — `direction > 0`,
porque al volver no hay fila de origen de la cual volar; (3) solo
cuando hay continuidad real que mostrar — el título de la fila elegida
tiene que ser igual a la ceja de la página que se abrió
(`title_dynamic`). Se suma una cuarta, propia: las cuadrículas de
tiles quedan fuera (`tile_cols == 0`), porque un tile no tiene una fila
de texto desde la cual volar.

**Dónde vive cada parte**: `metro_screen_list.c` **decide** (captura el
título y la y de la fila ANTES de `on_select()` —que puede empujar una
página y dejar la selección apuntando a otra cosa— y compara después
contra la página resultante); `metro_transitions.c` **anima**, sin
saber qué es una página ni un pivot, igual que el resto de ese módulo.
El armado es de un solo uso y se consume en el siguiente
`metro_transitions_push()` se llegue o no a animarlo — si no, un PUSH
sin continuidad heredaría el volador del anterior.

**Curva propia (OUT_QUAD), no la del turnstile (OUT_EXPO)**: hallazgo
real, encontrado al intentar capturar el vuelo a mitad y descubrir que
a mitad ya no había vuelo. Out-expo gasta el 82 % del recorrido en los
dos primeros cuadros de ocho -- perfecto para una rotación que debe
sentirse instantánea, pésimo para un texto que tiene que **leerse**
viajando: el título aterrizaba casi de inmediato y se quedaba quieto
los seis cuadros restantes, que es exactamente lo contrario de lo que
CONTINUUM cuenta. Out-quad (curva que Metro ya tenía, sin matemática
nueva) reparte el viaje a lo largo de toda la animación y sigue
frenando al llegar.

**Dos primitivas, una nueva y una que no hizo falta**: la variante de
`metro_fb_present_slide()` sin auto-`lcd_update()` que DD-9 pedía
resultó innecesaria — al nivel de FX de CONTINUUM el PUSH hace
turnstile, que ya establece ese mismo contrato (F12). Sí hizo falta
`metro_fb_fill_rect()`, para borrar la ceja del destino dentro del
buffer off-screen y que no se viera dos veces a la vez. Ver R3-7 para
el bug que ese borrado destapó (la ceja quedaba ausente de forma
permanente tras el asentado del turnstile) y su corrección.

**Tres defines de `metro_draw.c` pasaron a `metro_draw.h`**
(`METRO_DRAW_LEFT_X`, `METRO_DRAW_ROWS_FIRST_Y`, `METRO_DRAW_ROW_PITCH`):
`metro_screen_list.c` necesita saber en qué y está dibujada la fila
seleccionada, y `metro_transitions.c` la misma x del margen. Se
exportan en vez de re-declarar los números por tercera vez — el
`metro_draw.c` sigue usando sus nombres viejos, ahora definidos contra
los públicos.

**Un cambio fuera de `apps/metro/`, en herramienta de pruebas**: el
sondeo del hilo del simulador pasó de `HZ/10` a `HZ/50`
(`uisimulator/common/sim_tasks.c`, ya modificado por Metro y ampliado
antes por esta misma clase de razón). Con 10 ticks de resolución una
animación de 24 no se puede muestrear más que dos veces, y la primera
cae pasado el tercer cuadro — el criterio de "hecho" de esta fase pide
capturar justo el arranque. Registrado en `MODIFICATIONS.md`; solo
compila en el simulador, cero impacto en el binario de hardware.

**Verificado en vivo** (simulador, `make install` primero):
`docs/screenshots/R3-F8-continuum-early.png` (cuadro 1 — "Analog
Dreams" en 20 px blanco, a media pantalla, con la página visiblemente
girando detrás), `-mid.png` (ya en 14 px, más arriba, turnstile todavía
en curso) y `-late.png` (casi aterrizado, página casi asentada): tres
posiciones y los dos tamaños. `R3-F8-continuum-minimal-off.png`: el
mismo PUSH con `animations=minimal` muestra el slide a mitad **sin
volador**. Un PUSH sin continuidad real (hub → Ajustes, cuya página no
tiene `title_dynamic`) hace turnstile normal, también sin volador.
Presupuesto: `push-turnstile 8 frames in 30-32 ticks (budget 24)` con
volador contra `29-31` sin él — ~1 tick de costo, y **cero**
`auto-degrade` en tandas de cuatro PUSH seguidos (el umbral de M-015 es
2× presupuesto = 48). Builds limpios en sim y target; 6 suites de test
de host en verde (678 checks).

## M-070 — R3-F9: release v0.3.0 y la lista de verificación en hardware que sigue vacía

**Contexto**: novena y última fase de la ronda 3
(`PLAN-metro-r3-maestro.md` DD-11). Cierra la ronda: documentación al
día, artefactos regenerados, tag anotado, y —tras las dos barreras que
solo el dueño puede levantar— el Release publicado y la primera sesión
con hardware real.

**`docs/ESTADO_FINAL.md` reescrito a v0.3.0**: venía diciendo "v0.1.0"
todavía (la ronda 2 nunca lo actualizó, deuda que esta fase salda de
paso). Ahora acumula las tres rondas, con secciones propias para lo que
agregó cada una, y el conteo real de tests (678 en 6 suites, contra los
251 que declaraba).

**La lista de verificación en hardware, reescrita como lista de verdad**:
antes eran cinco viñetas en prosa; ahora son **17 puntos numerados
(H1-H17) en tablas con una columna de resultado sin responder**, para
llenarse punto por punto en el aparato real. H1-H9 cubren la base de
las rondas 1-2; H10-H17 lo nuevo de la ronda 3. Se agregó además una
sección corta que nombra las **tres cosas verificadas solo por vía
indirecta** en el simulador, que son las que más merecen atención en la
primera sesión real: el volcado de la cola de tagcache al apagar (en el
simulador la cola se desbordó sola por volumen, nunca se probó un
apagado limpio), el disparo real del temporizador de sueño (nunca se
dejaron correr 15 minutos), y el EQ por oído (el simulador no tiene
salida de audio representativa). Decir explícitamente qué NO se probó,
y por qué, vale más que una lista que parezca completa.

**`README.md`**: sección "Qué trae (v0.3.0)", aviso claro de que nada
se ha probado en hardware, enlace a la guía de flasheo, y limpieza de
dos referencias obsoletas ("a escribir en F0", "cuando exista").

**Las dos barreras**, tal cual las definió el plan y por qué existen:
**B1** es el `git push` del tag y la publicación del Release — sale de
la máquina y es irreversible de cara a terceros, así que se prepara
todo y se pide confirmación literal. **B2** es el flasheo: solo el
dueño tiene el iPod. Lo que reporte se anota en `ESTADO_FINAL.md`, y si
algo hay que ajustar (tiempos de transición, presupuesto de miniaturas,
la curva de CONTINUUM) sale en un commit de ajuste dentro de esta misma
fase.

## M-071 — R4/FA-8: PLAY controla la reproducción desde cualquier pantalla

**Contexto**: primera fase de la ronda 4. El botón físico de Play/Pausa
solo funcionaba dentro de Now Playing y del visor de fotos; desde el hub
o cualquier lista no hacía nada.

**Causa raíz — dos capas, ninguna era "interceptación"**: el botón nunca
se enrutaba. (1) `BUTTON_PLAY` solo estaba mapeado en `player_mapping[]`
y `viewer_mapping[]` de `metro_keymap.c`; ausente de `hub_mapping[]`,
`list_mapping[]`, `dialog_mapping[]` y `lock_mapping[]`, y como toda
tabla termina en `LAST_ITEM_IN_LIST` (no `__NEXTLIST`), un botón sin
mapear resuelve a `ACTION_NONE` en vez de encadenar a `CONTEXT_STD`.
(2) `MACT_PLAYPAUSE` solo se manejaba en dos pantallas. Cerrar una sola
capa no habría hecho nada.

**Qué se hizo**: `BUTTON_PLAY` agregado a hub y listas, más el caso
correspondiente en sus manejadores. **No** se agregó a `dialog_mapping[]`
(un diálogo modal de sí/no debe seguir enfocado en su propia pregunta) ni
a `lock_mapping[]` — esa exclusión es deliberada de M-068 y se conserva:
con el candado puesto el aparato no ofrece ningún control.

**Reuso en vez de una cuarta copia**: la pareja
`audio_status()`/`audio_pause()`/`audio_resume()` ya estaba duplicada
palabra por palabra en Now Playing y en el visor; esta fase habría hecho
cuatro copias. Se extrajo `metro_music_playpause()` a `metro_music.c` y
las cuatro pantallas la llaman. Se conserva el orden original de las
preguntas (PAUSE antes que PLAY) porque **una pista pausada tiene ambos
bits** en Rockbox: preguntar por PLAY primero nunca reanudaría nada.

**No-op si no hay nada sonando**, a propósito: pulsar PLAY con la
biblioteca detenida no debe arrancar algo por sorpresa, y ese caso solo
existe ahora que el botón vive en pantallas que no presuponen una pista.

**Verificado en vivo** con `DEBUGF` temporal (revertido antes del
commit): tres pulsaciones, tres transiciones reales de estado —
`0x1`→pausa, `0x3`→reanuda desde el hub, y `0x1`→pausa desde dentro de
una lista.

## M-072 — R4/FA-9: la búsqueda deja de saltar de 5 en 5 segundos

**Causa raíz**: no era refresco de LCD ni la lógica de "hold". El paso
era **fijo**: `METRO_SEEK_STEP_MS 5000` por cada evento de repetición
(`metro_screen_nowplaying.c`), así que sostener LEFT/RIGHT producía
brincos de 5 s por definición.

**Corrección a la suposición del plan de Fase 1**: ese plan proponía
"reutilizar `button_apply_acceleration()`, ya presente". **Leyendo su
implementación resultó inservible aquí**: es específica de la rueda —
lee la velocidad del wheel del bit 31 de `get_action_data()`
(`firmware/drivers/button.c:632-659`). Un `BUTTON_LEFT|BUTTON_REPEAT` no
trae ese dato, así que habría devuelto **0**: peor que no acelerar. Se
descartó y se escribió una rampa propia.

**Dónde vive, y por qué ahí**: la política (`metro_seek_step_ms(run)`)
está en `metro_motion.c`, no en la pantalla. Dos razones: es la misma
familia que `metro_ease()` (una curva de "cuánto avanzo por paso"), y
ese módulo es C99 puro sin dependencias de Rockbox, así que la rampa
queda cubierta por el arnés de host. Eso importa más de lo normal aquí:
**el inyector de botones del simulador no puede probar esto** — hace
press-release corto y nunca llega a `BUTTON_REPEAT`. Sin la función
pura, la única verificación posible habría sido sostener un botón a
mano.

La pantalla conserva solo el estado de sesión (la racha y su expiración
por `current_tick`), que sí depende de Rockbox.

**Forma**: arranca en 1000 ms y duplica cada 4 eventos consecutivos,
con tope en 10000 ms; la racha se reinicia tras medio segundo sin
eventos (el usuario soltó). Toque corto = ajuste fino; sostener =
atravesar una pista larga sin eternizarse.

**Verificado**: `test_motion.c` cubre el **contrato**, no los números
exactos — arranca en el mínimo, no crece dentro del primer tramo, sube
al completarlo, es monótona no decreciente en 500 iteraciones, satura
en el tope, y un `run` negativo se trata como el primero. La suite pasó
de 116 a 1625 checks.

**Pendiente de hardware**: si el paso concreto se siente bien es
afinación, y el simulador es mal juez (`INVESTIGACION.md` B.11).
Además `audio_ff_rewind()` fuerza un re-seek del búfer que en el iPod
real puede añadir su propio tirón — hipótesis no confirmada.

## M-073 — R4/FA-6: indicador de reproducción/pausa en Now Playing

**Causa raíz**: `metro_screen_nowplaying_show()` **nunca consultaba**
`AUDIO_STATUS_PAUSE`; el flag solo aparecía en el manejador de entrada.
Al pausar, la pantalla quedaba visualmente idéntica salvo que el tiempo
dejaba de avanzar — sin ninguna forma de saberlo a simple vista.

**Geometría portada, no inventada**: `metro_widgets_draw_play_icon()` y
`..._pause_icon()` reproducen **exactamente** `draw_status_icon()` y
`draw_tri_stepped()` del OSD del reproductor de video
(`apps/plugins/mpegplayer/mpegplayer.c:796-845`), cambiando solo la
primitiva de relleno. La razón de portarla en vez de dibujar otra es que
el mismo estado se vea igual en música y en video. El triángulo se traza
columna por columna (Rockbox no tiene primitiva de polígono) y queda
escalonado a propósito: a 16 px se lee como triángulo y encaja con el
lenguaje anguloso de Metro.

**Dos decisiones de colocación:**

- **Extremo izquierdo de la fila de glifos de estado** (la de
  aleatorio/repetir, en `y=176`, que se llena desde la derecha). No
  desplaza nada de lo que ya había.
- **Asimetría de color deliberada: pausa en acento, reproducción en
  secundario.** Reproducir es el estado normal y no necesita gritar;
  pausa es el que explica por qué no se oye nada, y es el que uno viene
  a buscar con la mirada.

**Verificado**: `docs/screenshots/R4-FA6-playing.png` y
`R4-FA6-paused.png`, comprobadas además de forma **mecánica** (sus
SHA-256 diferentes: la pantalla ya no es idéntica entre ambos estados).

## M-074 — R4/FA-5b: el pivot Álbumes pasa a cuadrícula con carátula real

**Estado previo**: el pivot Álbumes era una lista de texto — la única
entrada de `music_pivots[]` sin `tile_cols`/`get_tile`.

**Reuso, no construcción**: Quickplay (R3-F4/M-065) ya renderizaba
exactamente esto. En vez de duplicar el par `cache_key`/`decode`, la
fuente de miniaturas dejó de cablear `s_quickplay[]` y **lee ahora la
lista desde el `ctx` del pivot** — el campo que `struct metro_pivot` ya
tenía y que estos proveedores ignoraban. Una sola implementación
(`album_thumb_source`) sirve a los dos pivots y a cualquier cuadrícula
de álbumes futura.

**Efecto lateral bueno**: como la clave de caché es el **seek del
álbum** y no la cuadrícula que lo muestra, Quickplay y Álbumes
**comparten la caché en disco** — una carátula ya decodificada por uno
la reusa el otro sin volver a decodificar.

**Verificado en vivo** (`docs/screenshots/R4-FA5b-albums-grid.png`), con
los tres caminos cubiertos por los fixtures: dos álbumes con carátula
propia distinta (crema y naranja), cuatro heredando `/Music/cover.jpg`
del directorio padre vía `find_albumart()` — azul sólido `0x3366CC`, que
**es decodificación real, no un placeholder** (se comprobó contra el
color del fixture) — y dos sin arte alguno cayendo al tile de acento con
inicial ("A", "N").

**Consecuencia a tener presente, no un defecto**:
`metro_draw_tiles()` solo usa el título para la inicial dentro del tile;
**no dibuja ninguna etiqueta de texto**. Al pasar de lista a cuadrícula,
Álbumes pierde de pantalla el nombre del álbum y su artista. Es el mismo
comportamiento que Artistas y Fotos ya tenían, y es lo que se pidió,
pero con carátulas parecidas entre sí los álbumes dejan de ser
distinguibles. Poner nombre bajo los tiles sería trabajo aparte y
afectaría a las tres cuadrículas por igual.

## M-075 — R4/FA-2: los residuales de macOS dejan de contaminar las listas

**Contexto**: el dueño reportó que el firmware indexaba archivos que
empiezan con `._`. Observado en vivo en su iPod: `._rockbox.ipod`,
`._version.txt`, `._sync-pending.json`, más `.Spotlight-V100`,
`.Trashes` y `.fseventsd`.

**Causa raíz**: `matches_any_ext()` (`metro_fsutil.c`) compara **solo el
sufijo**. Un sidecar AppleDouble se llama `._IMG_1234.jpg` — **conserva
la extensión** — así que pasaba el filtro, y como tampoco es un
directorio, el chequeo de `ATTR_DIRECTORY` no lo atrapaba. No existía
ningún filtro de archivos ocultos en todo `apps/metro/`.

**Un segundo sitio, no reportado, encontrado leyendo**: el escaneo de
playlists de `metro_music.c` tiene la misma comparación por sufijo, así
que un `._Mi Lista.m3u8` aparecía como una **playlist fantasma** en la
lista. Se corrigió igual.

Un tercer sitio (`metro_thumbs.c`) escanea la caché propia comparando
contra un prefijo que el firmware mismo genera (`album-<seek>`,
`<archivo>.<mtime>`); un `._…` nunca coincide, así que no necesita el
filtro y no se tocó.

**La regla es el punto inicial, no el `._` específico**: en FAT un
nombre que empieza con punto es oculto por convención, ninguna fuente de
contenido legítimo lo genera (Studio sanea nombres, una copia manual
tampoco), y de paso cubre `.DS_Store`, los directorios de servicio de
macOS y `.`/`..`. Un punto en cualquier **otra** posición
(`mi.foto.jpg`) no se ve afectado — esa es la guarda de regresión que
importa.

**Solo del lado del firmware** (decisión del dueño). Es la elección
correcta además de la pedida: el usuario puede copiar archivos a mano
sin que Studio intervenga, así que filtrar al **leer** es lo único que
cubre todos los caminos.

**Dónde vive**: `static inline` en `metro_fsutil.h`, no en el `.c`. Ese
header no tiene dependencias de Rockbox, así que el predicado queda
cubierto por el arnés de host (`test_fsutil.c`, 20 checks) sin necesidad
de compilar el módulo entero — mismo criterio que M-072 con la rampa de
búsqueda.

**Verificado en vivo, antes y después** con residuales reales colocados
en el simdisk (`docs/screenshots/R4-FA2-antes.png` /
`R4-FA2-despues.png`): antes, la cuadrícula de Fotos abría con un tile
de acento cuya inicial era **un punto literal** (`._diagram.jpg`, que no
decodifica) seguido de un duplicado de `sunset.jpg`; después ambos
desaparecen y entran dos fotos reales que estaban desplazadas fuera de
la ventana. Las playlists muestran solo "QA Favorites", sin su fantasma.

**Fixtures permanentes**: los AppleDouble se agregaron a
`gen_test_media.sh` (con su magic real `0x00051607`) en vez de dejarlos
como un experimento de esta fase, para que cualquier captura futura de
las cuadrículas siga demostrando que el filtro aguanta.

## M-076 — R4/FA-5a: el catálogo español se acentúa, y sale a la luz un bug de UTF-8 que ya existía

**Encargo y su ambigüedad**: el dueño pidió cambiar "Albumes". La
Fase 1 lo marcó como contradicción a confirmar (¿inglés "Albums" o
español "álbumes"?) porque el principio del proyecto es "Español
impecable". Confirmó: **"álbumes", con acento**.

**El problema resultó ser mucho más grande que una palabra**: **ninguna**
cadena del catálogo español llevaba acentos. Un grep de `[áéíóúñ¿¡]`
sobre el bloque `strings_es[]` devolvía **cero** resultados. Se
corrigieron 30 cadenas, entre ellas:

- `"temporizador de sueno"` → `"sueño"`. La vieja no era "sueño" mal
  escrito: **"sueno" es otra palabra** (1.ª persona de *sonar*), así que
  la fila decía literalmente otra cosa.
- `"si"` → `"sí"`. Sin tilde es la conjunción condicional, no la
  afirmación — en un diálogo de sí/no eso importa.
- Las tres preguntas ganan su `¿` de apertura, que en español no es
  opcional.
- `"espanol"` → `"español"`, `"peliculas"` → `"películas"`,
  `"imagenes"` → `"imágenes"`, `"calificación"`, `"animación"`,
  `"gráficos"`, `"retroiluminación"`, etc.

**"videos" se dejó SIN acento a propósito**: el proyecto escribe en
español de México (`CLAUDE.md`), donde "video" es la forma estándar;
"vídeo" es de España.

**Precondición verificada antes de tocar nada**: se probó una sola
cadena con `"música ñ ¿á"` y se capturó el hub. Las fuentes renderizan
acentos, `ñ` y `¿` perfectamente — el rango generado (`0x20`-`0x17F`,
`gen_fonts.sh`) los cubre. Sin esa comprobación previa, acentuar 30
cadenas a ciegas podía haber llenado la interfaz de `?`.

### El hallazgo real: cortar la inicial por BYTE, no por carácter

Acentuar `LANG_UNKNOWN_ALBUM` destapó un bug **preexistente**. Dos
sitios sacaban la inicial de una etiqueta con `label[0]` — **un solo
byte**:

- `metro_draw_tile()` (`metro_draw.c`), el tile de acento con inicial.
- La letra flotante de índice (`metro_screen_list.c`).

Para cualquier texto que empiece con letra acentuada, eso parte la
secuencia UTF-8 a la mitad y le entrega a `lcd_putsxy()` un byte guía
suelto sin continuación: glifo basura.

**No es un bug que introduzca esta fase.** Una biblioteca real en
español con un artista "Ángela" o un álbum "Éxitos" ya lo disparaba
hoy; simplemente ninguna cadena compilada del firmware empezaba con
acento, así que nunca se había visto. Acentuar el catálogo lo volvió
alcanzable también desde el propio firmware — y por eso salió.

**Corrección**: `metro_lang_initial()` (nueva, en `metro_lang.c` porque
ese módulo no tiene dependencias de Rockbox y así queda host-testeable)
copia el **carácter** completo, de 1 a 4 bytes, y lo pasa a mayúscula.
Mayúsculas cubiertas: ASCII `a-z` **y** las acentuadas de Latin-1 en
UTF-8 (`á`→`Á`, `ñ`→`Ñ`), excluyendo `÷` (0xC3 0xB7, no es letra) y `ÿ`
(cuya mayúscula `Ÿ` no vive en Latin-1). `metro_widgets_draw_index_letter()`
pasa de recibir un `char` a recibir una cadena.

**24 checks nuevos** en `test_lang.c`, verdes al primer intento:
acentos, mayúsculas ya hechas, no-letras de Latin-1, multibyte de 3 y 4
bytes (CJK y emoji), secuencia truncada, byte de continuación suelto,
`NULL`, buffer insuficiente y `outsz == 0`.

**Fixture permanente en español**: `gen_test_media.sh` gana una pista de
"Ángela Ñu" / "Éxitos". Sin ella ninguna etiqueta del simulador empieza
con un carácter multibyte y el bug volvería a ser invisible.
`docs/screenshots/R4-FA5a-inicial-acentuada.png` muestra el tile con
una **Á** correcta.

**Efecto lateral observado, no corregido**: `label_cmp()`
(`metro_music.c`) compara bytes con mayusculización ASCII, así que una
inicial acentuada (`0xC3…`) ordena **después de la Z**. "álbum
desconocido" pasó de primero a último en la lista de álbumes. Afecta
igual a contenido real de biblioteca ("Ángela" cae tras "Zoé"), es
independiente de esta fase, y ordenar con plegado de acentos es trabajo
aparte — queda anotado, no resuelto.

## M-077 — R4/FA-1: iconografía Fluent, y el pipeline de assets que no existía

**Corrección a la premisa del encargo**: se pidió "sustituir los iconos
actuales por el set elegido". **No había un set que sustituir.** Los
cuatro iconos del firmware (batería, aleatorio, repetir, punto del PIN)
eran trazos geométricos escritos a mano con `lcd_drawline`/`lcd_fillrect`,
y en `apps/metro/` no existía **ni un solo archivo de icono**. El
trabajo real de esta fase fue construir el pipeline, no elegir el set.

**Set: Fluent System Icons** (decisión del dueño, Q2). MIT — verificado
contra el repositorio, no de memoria — y compatible con redistribución
embebida en GPL v2. Es además el descendiente directo de Metro.

### Por qué una tabla C generada, y no las dos alternativas obvias

- **Una fuente de iconos no cabe.** `gen_fonts.sh` genera el rango
  `0x20`-`0x17F` (Latin-1 + Extended-A). No hay zona de uso privado,
  que es donde vive el glifo de cualquier icon font.
- **Un `.bmp` por icono en `.rockbox/` obligaría a leer disco** para
  dibujarlos, y `CLAUDE.md` prohíbe lectura de disco dentro de un bucle
  de animación — los iconos de modo se dibujan en cada cuadro de Now
  Playing.

Así que se sigue el patrón que el proyecto ya tenía para datos
precalculados: `firmware/tools/gen_icons.py` → `metro_icons_table.c`,
generado offline y **commiteado**, con su generador versionado al lado
— idéntico a `gen_turnstile_table.py` → `metro_turnstile_table.c`. Los
`.svg` originales también se commitean (`firmware/assets/icons/`), así
que regenerar **no necesita red**.

**Formato**: un `unsigned short` por fila, bit 15 = píxel izquierdo. 32
bytes por icono, 5 iconos = 160 bytes en el binario. **Monocromo a
propósito**: el color lo pone quien dibuja, que es lo único compatible
con la regla de cero RGB fuera de `metro_palette.h` — y de paso permite
que el mismo glifo sirva en acento o secundario según el estado.

**Dibujo por corridas horizontales**, no píxel por píxel: un glifo de
16×16 serían hasta 256 `lcd_drawpixel()` sueltos, y estas siluetas
suelen ser una o dos corridas por fila.

### El icono que no entró, y por qué

`arrow_repeat_1` **se descartó tras probarlo**. Su insignia del "1" se
apelmaza en una mancha ilegible a 16 px, y se comprobaron tres
variantes antes de rendirse: 16 filled con umbral alto rompe el lazo en
píxeles sueltos; 20 filled reescalado y 16 regular quedan igual de
densos. Es un límite del icono a ese tamaño, no del pipeline.

Solución: se usa el lazo de `arrow_repeat_all` y el dígito se dibuja
**al lado**, que es el mecanismo que Metro ya usaba (y sí se lee) — solo
que antes el "1" iba *encima* de un lazo dibujado a mano y ahora va
junto a un glifo de Fluent.

**Lo que sigue siendo geométrico, a propósito**: la batería. No es un
símbolo fijo sino un indicador con relleno proporcional al nivel;
ningún glifo estático la resuelve.

**Ganancia colateral**: el overlay de volumen era texto puro
("volumen 42%"). Con el glifo de altavoz, el porcentaje solo ya dice
qué es — la palabra sobra, la línea se acorta, y `LANG_NP_VOLUME` se
eliminó del catálogo en vez de dejarlo muerto.

**Cumplimiento MIT**: `package_dist.sh` agrega el aviso y el texto
completo de la licencia de Fluent a `THIRD-PARTY-NOTICES.txt`, junto al
de Selawik que ya estaba. La licencia también se commitea en
`firmware/assets/icons/`.

**Verificado**: el generador es **reproducible** — dos corridas
producen bytes idénticos (SHA-256 comprobado), criterio que la Fase 1
había propuesto. Cada glifo se inspeccionó como arte ASCII **antes** de
integrarlo (`gen_icons.py --preview`), que es lo que permitió detectar
el problema de `repeat_1` sin llegar a compilarlo.
`docs/screenshots/R4-FA1-iconos-fluent.png` muestra pausa, aleatorio y
repetir-uno reales en pantalla; `R4-FA1-volumen.png`, el altavoz.
Builds limpios en sim y target; 2231 checks de host.

## M-078 — R4/FA-7: el fondo del reproductor deja de ser la misma imagen que el tile

**Estado previo**: `metro_albumart_load_background()` cargaba el arte de
`audio_current_track()` escalado a pantalla completa — **la misma imagen
que el tile**, solo que estirada y atenuada al 30%.

**Tabla acordada con el dueño** (Q7 de la Fase 1, confirmada por él):

| artista | álbum | fondo | tile |
|---|---|---|---|
| sí | sí | foto del artista | carátula real |
| sí | no | foto del artista | acento + inicial |
| no | sí | carátula | carátula real |
| no | no | plano (tema) | acento + inicial |

**La columna del TILE no necesitó ningún cambio**: el respaldo de acento
+ inicial existe desde F5 y ya se comportaba exactamente así. Solo
cambia la columna del FONDO.

**Y esa columna se reduce a una cascada de dos preguntas**: foto de
artista si la hay → si no, la carátula → si no, nada. Las cuatro filas
salen de eso en ese orden; no hizo falta escribir la tabla como tal.

**Dónde vive cada mitad**: `metro_albumart.c` gana
`metro_albumart_load_background_file()`, que decodifica un archivo
arbitrario al búfer de fondo — **decodifica, no decide**. La política
(cuál usar) vive en `metro_screen_nowplaying.c`, que es donde ya vivía
la decisión de dibujar fondo o no. Mantiene el reparto que la propia
cabecera de `metro_albumart.h` declara.

**Caché-de-1 clavada a la RUTA de origen**, no al track: así una foto de
artista y una carátula nunca se confunden entre sí, y volver a la misma
pista con la misma fuente no vuelve a decodificar. Ambas funciones
comparten búfer a propósito — hay un solo fondo en pantalla a la vez por
construcción.

**Caso borde cubierto**: si el índice mapea una foto de artista pero el
archivo resulta ilegible (corrupto, borrado entre el índice y el
dibujo), se cae a la **carátula**, no a fondo plano — un fallo de la
primera opción no debe costar también la segunda.

**Sobre el tamaño**: una foto de artista viene a lo mucho de 128 px
(tope del contrato) y aquí se agranda a 320×240. Se ve suave, y detrás
del texto al 30% de opacidad eso no es un defecto sino lo deseable.

**Las cuatro filas verificadas en vivo**, una captura por fila
(`docs/screenshots/R4-FA7-fila{1,2,3,4}.png`). Los fixtures hacen los
casos inequívocos por color: la foto de "Aura Test Combo" es verde mar y
la carátula de "First Light" naranja, así que la fila 1 muestra de un
vistazo que fondo y tile **ya no son la misma imagen**.

**Corrección a lo que se había dicho**: la Fase 1 anotó que dos de las
cuatro filas no eran verificables sin el iPod. Era incorrecto — los
fixtures de fotos de artista de R3-F3 viven en el simdisk, así que las
cuatro se verifican en el simulador.

**Fixture nuevo, permanente**: la fila 4 (ninguna de las dos imágenes)
no era alcanzable con los fixtures existentes — todo track bajo
`/Music/` hereda `/Music/cover.jpg` por el barrido de directorio padre
de `find_albumart()`. Se agregó a `gen_test_media.sh` un álbum "Sin
Portada" bajo un artista que a propósito no tiene foto, en una carpeta
cuyo padre tampoco tiene `cover.jpg`.

## M-079 — R4: ordenar con acentos plegados (una artista "Ángela" ya no cae tras la Z)

**Anotado como efecto lateral de M-076, ahora resuelto.** `label_cmp()`
(`metro_music.c`) recorría **bytes** y solo pasaba a mayúscula el ASCII.
Una inicial acentuada (`Á` = 0xC3 0x81) quedaba por encima de cualquier
letra, así que ordenaba **después de la Z**.

**No era un problema teórico**: afecta a cualquier biblioteca en
español. Verificado en vivo con el fixture "Ángela Ñu" — antes salía
último, tras "Wheel & Click".

**Reglas** (`metro_lang_collate()`, en `metro_lang.c` porque ese módulo
es puro y host-testeable, a diferencia de `metro_music.c`):

- Las vocales acentuadas pliegan a su base: `á`==`a`, `ü`==`u`. Es lo
  que espera cualquier hispanohablante al recorrer una lista.
- **`ñ` NO pliega a `n`**: es letra propia y va entre la N y la O, como
  manda la RAE. Por eso las claves son enteros escalados ×4 y no bytes
  — entre `'N'` y `'O'` no cabe nada.
- Empate en ese nivel → desempate por bytes crudos. Eso es lo que hace
  el resultado **determinista**: "Ángela"/"Angela" y "abba"/"ABBA" son
  pares que el plegado vuelve indistinguibles, y sin desempate su orden
  relativo quedaría a merced del algoritmo de ordenamiento.

**El contrato se corrigió por culpa de un test.** La primera versión de
la documentación decía "mayúsculas y minúsculas se ignoran (`a` ==
`A`)" y el test lo afirmaba como `collate("abba","ABBA") == 0`. Falló:
el desempate por bytes hace que no sea 0. La implementación era la
correcta y la **descripción** la imprecisa — se reescribió para
distinguir *dónde cae* una etiqueta (la caja no influye) de *cómo
desempata* (sí influye, a propósito).

**Verificado**: 23 checks nuevos en `test_lang.c` (Á antes que B y que
Z, "And" < "Áng" < "Ant", ñ entre N y O, dígitos antes que letras,
prefijos, degenerados) más la comprobación mecánica end-to-end: un
`DEBUGF` temporal sobre la lista real de artistas confirmó
`Ángela Ñu` en el índice **0**, delante de "artista desconocido" y
"Aura Test Combo".

## M-080 — R4: rótulo del tile seleccionado en las cuadrículas

**Anotado como consecuencia de FA-5b, ahora resuelto.** Una cuadrícula
no tenía **ningún** texto: `metro_draw_tiles()` solo usaba el título
para la inicial dentro del tile de respaldo. Con carátulas parecidas
entre sí los tiles dejaban de ser distinguibles — y no es hipotético:
cuatro álbumes de los fixtures heredan el mismo `/Music/cover.jpg` por
el barrido de directorio padre de `find_albumart()` y se ven idénticos.

**Un solo rótulo, para lo seleccionado**, no uno por tile: no hay
espacio vertical para etiquetas individuales (dos filas de 80 px
arrancando en y=84 ya se salen de los 240 de alto) y además sería ruido
— lo que hace falta saber es qué está elegido.

**Franja al pie sobre fondo sólido pintado con `lcd_fillrect()`**, para
que se lea encima de cualquier carátula. M-051 prohíbe conseguir ese
fondo vía `DRMODE_SOLID`; pintarlo aparte es justo la salida que esa
regla contempla. Cuesta los 22 px de abajo de la segunda fila, que ya
venía cortada por el borde de la pantalla: su función es asomar para
decir "hay más", y con 54 px la sigue cumpliendo.

**Título a la izquierda, subtítulo a la derecha en terciario** — el
mismo reparto que `metro_draw_rows_ex()` ya usa para las filas de
texto, de modo que un álbum se lee igual esté en lista o en cuadrícula
("Analog Dreams" / "Wheel & Click"). Beneficia a las cuatro cuadrículas
por igual: Álbumes, Artistas, Quickplay y Fotos.

**Verificado**: `docs/screenshots/R4-tiles-rotulo.png` (álbum con
subtítulo de artista) y `R4-orden-acentos.png` (artista sin subtítulo —
y de paso muestra la "Á" en primer lugar, la comprobación visual de
M-079).

## M-081 — R5-F1: "Acerca de" dejaba de responder en el iPod — leía disco en cada fila, en cada cuadro

**Síntoma reportado por el dueño (hardware real, v0.4.0):** al pasar al
pivot "acerca de" desde "pantalla", el iPod se queda en "pantalla". En el
simulador jamás ocurrió.

**Causa.** `metro_screen_about.c` llamaba `metro_manifest_load()` —un
`open()` + parseo línea a línea + `close()` de `sync_summary.cfg`— dentro
de `about_count()` **y dentro de `about_get_row()`**, es decir una vez por
fila. Y ambos proveedores corren **por cuadro**: la transición de pivot
(SLIDE) redibuja todas las filas en cada tick. Con ~12 filas, eso son ~13
aperturas de archivo por cuadro durante toda la animación. "Pantalla", el
pivot de al lado, no toca disco en ninguna fila: ésa era la única
diferencia entre el que funcionaba y el que no.

En el simulador el `open()` va al sistema de archivos del host y cuesta
microsegundos; en el iPod pasa por la capa FAT de Rockbox hacia un disco
de 1.8" que además puede estar detenido. Es una violación literal de la
regla del `CLAUDE.md` ("ninguna lectura de disco dentro de un bucle de
animación por cuadro") que el simulador no tenía forma de revelar.

**Decisión.** `metro_manifest` gana una copia en RAM:
`metro_manifest_reload()` + `metro_manifest_cached()`. Se recarga en
`metro_disk_handoff()` (`metro_main.c`), junto a `metro_device_reload()`,
que ya seguía exactamente este patrón por la misma razón: son los **dos
únicos** momentos en que el firmware recupera el disco (arranque y regreso
de la pantalla USB), y `sync_summary.cfg` lo escribe exclusivamente Aura
Studio por USB — no puede cambiar en ningún otro momento. Los proveedores
de About leen la copia; `metro_manifest_load()` queda documentado como
"toca disco, no llamar por cuadro".

**Auditoría de paso.** Se revisaron todos los `count()`/`get_row()` de
`apps/metro/` buscando I/O: About era el único. Los tiles pasan por el
caché de miniaturas de R3-F1, diseñado para eso. También se separó el
`static char buf[16]` que compartían las filas de brillo y
retroiluminación en `display_get_row()` — hoy inocuo porque cada fila se
dibuja justo después de pedirla, pero es una trampa para cualquier
llamador que guarde dos `struct metro_row`.

**Verificado** en simulador con un `sync_summary.cfg` de 13 claves: el
pivot muestra los contadores reales desde el caché. La comprobación
definitiva (que el iPod ya no se trabe) requiere hardware: pendiente del
dueño con el Release siguiente.

**Pendiente de evidencia (mismo encargo):** los menús de Ajustes
"desacomodados" (fila rotulada "ecualizador" cuyos valores son minutos).
No se encontró una causa en el código: HEAD es el tag `v0.4.0`, el
catálogo de `metro_lang.c` usa inicializadores designados, y título y
subtítulo de una fila salen de la misma llamada a `get_row`. Falta
comparar el hash del `rockbox.ipod` instalado contra el del Release y ver
una foto de la pantalla antes de tocar nada.

## M-082 — R5-F2: la "ll" se fundía en un solo trazo ("pantalla" se leía "pantala")

**Encargo del dueño:** *"al tener doble L minúscula no hay mucho espacio
entre las dos l… 'Pantalla' se lee como 'Pantala' pero con una l gruesa
muy extraña. ¿Hay forma de ponerle un poco de espacio, revisando todos los
caracteres de la tipografía hasta conseguir el espacio ideal?"*

**Diagnóstico.** Reproducible en el simulador (no era cosa del iPod): a
48 px Light —el tamaño del encabezado de pivot, donde vive "pantalla"—
las dos "l" de Selawik se rasterizan sin ningún píxel entre ellas. Los
bearings laterales de la fuente, que a ese peso son fracciones de píxel,
redondean a cero al convertir. A 20 px pasa lo mismo en grado menor
("brillo" en semibold).

**Qué se puede y qué no.** Las fuentes de Rockbox **no tienen kerning
por pares**: no existe "ajustar solo la ll". Lo único ajustable es la
separación global entre glifos (`convttf -c N`), y se comprobó que `N`
fraccionario no sirve —el avance se redondea a píxel entero y `-c 0.5`
produce una fuente idéntica—. El hinting ligero (`-L`) adelgaza los
trazos pero **no** separa las "l": no era el problema.

**Decisión.** `-c 1` en display (48), title (28), list (20) y listsel
(20). **La caption de 14 px queda en 0**: no hay queja ahí y un píxel por
glifo a ese tamaño es proporcionalmente mucho (7% del cuerpo) para un
texto que ya es el más denso de la interfaz (subtítulos a la derecha,
horas). `gen_fonts.sh` gana una cuarta columna por rol con el valor, para
que la decisión quede en el generador y no en un comando suelto.

**Efecto colateral asumido:** todo texto en esos cuatro roles es ~1 px
por glifo más ancho. Se revisaron hub, ajustes (general y pantalla) y
listas: los recortes por viewport (`metro_draw_text_cut_right`) y los
subtítulos alineados a la derecha absorben la diferencia sin solaparse.
Captura antes/después en
`docs/screenshots/r5-f2-fuentes-ll-antes-despues.png` (arriba el estado
anterior; segunda fila `-c 1`, la elegida; las demás, los descartes
`-c 2`, `-L` y `-c 1 -L`).

## M-083 — R5-F3: rediseño del reproductor sobre la maqueta del dueño

**Encargo:** maqueta de 320×240 entregada por el dueño (carátula a la
izquierda; a la derecha estrella/aleatorio/repetir, tres anillos de
transporte, ARTISTA/álbum/título; barra de progreso con márgenes y
tiempos debajo; volumen "10" sobre la carátula), más reglas explícitas:
iconos de transporte "en un círculo perfecto y delgado, que cuides el
antialiasing"; el volumen deja de ser barra y pasa a ser numérico `00`
(silencio) a `15` (máximo), aparece al ajustar y se desvanece 3 s después
"con un desvanecimiento lento"; "limitar volumen" usa la misma escala,
"ya no dB"; estrella/aleatorio/repetir siempre visibles, tenues apagados y
en acento encendidos.

### Escala de volumen (`metro_volume.c`, puro, 204 checks)
16 niveles sobre `sound_min..sound_max` de `SOUND_VOLUME` (−60…+12 dB en
el iPod 6G, paso 1 dB; este target no tiene volumen perceptual). **El
reproductor no guarda ningún nivel propio**: siempre se deriva de
`global_status.volume`, que Rockbox ya persiste, mediante un mapeo con
garantía de ida y vuelta (`level(db(L)) == L` para todo `L` y cualquier
rango, y estrictamente creciente: dos niveles nunca comparten dB). Eso es
lo que permite que cada muesca de rueda suba exactamente un nivel aunque
el dB intermedio se redondee. Verificado en simulador: 05→06→07→08 con
tres muescas.

Nivel 00 = `sound_min` (−60 dB), no un mute del códec: a −60 dB no se oye
nada en la práctica y así "00" sigue siendo un punto de la misma escala
continua, no un estado aparte.

### Límite de volumen
Nueva fila en Ajustes › General, en la misma escala, sobre
`global_settings.volume_limit` (Rockbox ya la aplica en
`sound_set_volume()`, así que el límite se respeta también para cualquier
ruta que no pase por Metro). Cinco presets que se ciclan con SELECT
(15, 12, 10, 08, 06), mismo patrón que brillo y retroiluminación: los 16
valores uno a uno serían quince pulsaciones en el peor caso, y un límite
por debajo de 06 no tiene uso real. Al cambiarlo se reaplica el volumen
actual, para que bajar el límite recorte de inmediato y no en la próxima
muesca. "Restablecer ajustes" lo devuelve a 15.

### Overlay de volumen
`"%02d"` en MFONT_LIST sobre la carátula, en la esquina superior
izquierda de la maqueta. 3 s quieto desde el ÚLTIMO ajuste, luego 1 s de
fundido hacia el fondo en 8 pasos discretos de color
(`metro_fb_blend_color`, expuesto de `metro_fb.c` para esto y para el
hub de R5-F5) — un fundido de color del texto, no de frame buffer, así
que no cuesta memoria. El bucle principal redibuja el reproductor a
~8 Hz mientras el nivel está en pantalla (`metro_screen_nowplaying_volume_visible()`)
y vuelve a 1 Hz después. Con `animations=off` o LCD apagado no hay
fundido: se corta en seco a los 3 s. La barra de volumen de F5
(`metro_widgets_draw_volume_overlay`) se eliminó; nada más la usaba.

### Iconos
Fluent System Icons (M-077), tres glifos nuevos: **estrella en variante
Regular** (contorno — el estilo de línea de la maqueta; a 16 px binariza
limpia) y **anterior/siguiente Filled**. Se probó la variante Regular de
aleatorio y repetir: las puntas de flecha se fragmentan a 16 px (se
conservan las Filled de R4, que para flechas son el mismo trazo de línea
solo más grueso, no una silueta). Los SVG quedan commiteados en
`firmware/assets/icons/` como los demás.

Anillos: `metro_widgets_draw_circle()`, punto medio entero, 1 px, **sin
antialias a propósito** — a esta densidad un anillo "suave" es un anillo
borroso. r = 13 (27 px), glifo de 16 px centrado; con el padding interno
de Fluent la tinta visible queda en ~12 px, como en la maqueta. El del
centro muestra el ESTADO y conserva la asimetría de M-073: play en fg
mientras suena, pausa en acento.

### Texto
Orden ARTISTA (versalitas, semibold) / álbum / título — el de la maqueta
y el del Zune original: la línea fuerte es quién, luego de dónde, luego
qué. `metro_lang_upper()` (8 checks nuevos) mayusculiza UTF-8 Latin-1
encadenando `metro_lang_initial()`, trunca en frontera de carácter.
Estrella = calificación > 0: lo único que Studio exporta a `ratings.cfg`
es la calificación (1–5 ★ → 2–10); su bandera de favorito no viaja al
dispositivo, así que no hay otra fuente posible.

**Asumido, revisable:** la columna derecha mide 144 px y los tres textos
van en los roles de 20 px de la app; un artista largo ("CULTURA
PROFÉTICA" en semibold) se recorta por la derecha. La maqueta usa un
cuerpo menor (~16 px); reproducirlo exige dos roles de fuente nuevos
(`gen_fonts.sh`), que no se agregan en esta pasada. Tiempo de la derecha
= duración total (la maqueta muestra dos cifras fijas; "restante"
cambiaría cada segundo).

### Fixture
`gen_test_media.sh` gana una pista de 20 s ("Cultura Profética / M.O.T.A
/ Un deseo", con acentos): todo lo demás dura ≤ 3 s y la canción se
acababa antes que el fundido — la primera captura mostraba la pantalla
de "nada sonando", no el fundido.

**Hallazgo de proceso, anotado:** la verificación en simulador de M-081
se hizo con un binario viejo. Un `git stash`/`pop` dejó `.o` compilados
dentro de la ventana del stash con el mismo segundo de mtime que los
`.c` restaurados, y `make` no los recompiló; el enlace falló recién al
compilar R5-F3 (`metro_manifest_reload` indefinido). Se forzó la
recompilación y se repitió la captura de About con el binario correcto —
el resultado es el mismo, pero la evidencia anterior no valía. Regla
práctica: tras `git stash pop`, `touch` de los archivos restaurados.

Capturas: `docs/screenshots/R5-F3-reproductor.png`,
`R5-F3-volumen-fundido.png`, `R5-F3-limite-volumen.png`.

## M-084 — R5-F4: play/pausa en la barra de estado, y la barra alineada en un solo eje

**Encargo:** *"cuando salgamos del reproductor, necesitamos tener una
indicación de que hay música en pausa o reproduciéndose… el icono de Play
o Pausa en la barra de estado a un lado del reloj. Todos los elementos de
la barra de estado deben estar alineados en su eje horizontal."*

**Qué había.** Título de página y reloj (caption 14 px) en `y=4`, batería
(9 px) también en `y=4`. Ampliada la captura: los dígitos del reloj
ocupan las filas 7–15 (centro 11) y la batería las 4–12 (centro 8.5):
flotaba ~2.5 px por encima del texto. No se notaba a simple vista, pero
es exactamente la falta de armonía que el dueño describe.

**Decisión.** Un solo eje: el centro vertical de los dígitos (fila 11).
Batería en `y=7` (7–15, idéntico a los dígitos). Glifo de transporte
(Fluent 16 px, tinta en filas 2–13 de su celda) en `y=3` → tinta en 5–16,
centro 10.5. Las tres constantes viven juntas en `metro_draw.c`
(`METRO_HEADER_*_Y`) para que el próximo elemento de la barra no vuelva
a inventarse su propia altura.

El glifo va a la **izquierda del reloj** (6 px de aire), solo cuando hay
audio (sonando o en pausa); sin audio no se dibuja nada. Misma asimetría
de color de M-073: play en secundario, pausa en acento. Se dibuja en
`metro_draw_header()`, así que aparece en todas las pantallas —
incluido el reproductor, donde es redundante con el anillo central pero
no estorba; hacer que el encabezado supiera "estoy en Now Playing" sería
acoplar `metro_draw.c` a una pantalla.

Captura: `docs/screenshots/R5-F4-barra-estado.png` (arriba sonando,
abajo en pausa).

## M-085 — R5-F5: la fila "reproduciendo" del hub se mueve (marquesina) o respira (pausa)

**Encargo:** *"mientras se esté reproduciendo, el texto (solo el nombre de
la canción) tendrá un movimiento en loop, de derecha a izquierda,
respetando el margen ya existente del lado izquierdo (del lado derecho se
puede cortar). Cuando esté en pausa, permanecerá quieto pero parpadeando
suavemente (no parpadeo real, un fade que lleve el texto a negros, durando
más tiempo en la fase donde se vea claramente el texto que en la que no)."*

**Marquesina.** 25 px/s (1 px cada 4 ticks), bucle sin costura: se
dibujan dos copias del título a `ancho + 60 px` de distancia, recortadas
a la ventana `[12, 320)` con el helper nuevo `metro_draw_text_clipped()`
(la forma general de `cut_right`: posición absoluta, recorte
independiente — el texto sale por DEBAJO del margen izquierdo, no lo
invade). Corre siempre que suena, sea corto o largo el título: es el
indicador de "hay música", no una solución a títulos largos.

**Respiración.** Ciclo de 3 s: 1.7 s a color pleno, 0.4 s de fundido al
fondo, 0.5 s invisible, 0.4 s de vuelta. La proporción visible/invisible
(≈2.5:1) es la que pidió el dueño; el fundido es `metro_fb_blend_color`
sobre el color del texto, como en M-083.

**Sin estado acumulado.** Ambas animaciones se calculan desde
`current_tick`: si el tick se salta cuadros (disco ocupado, LCD dormido)
se reanudan donde les toca, no donde se quedaron, y no hay nada que
reiniciar al entrar o salir del hub.

**Costo.** `metro_screen_hub_tick()` repinta SOLO la franja de esa fila
(`lcd_update_rect` de 320×52) a ~20 Hz; un `show()` completo a esa
cadencia redibujaría cuatro textos de 48 px y subiría 150 KB al LCD por
cuadro. El bucle principal acorta su espera de entrada a `HZ/20` solo
mientras `metro_screen_hub_wants_ticks()` (hay audio, la fila está a la
vista, `animations != off`, `lcd_active()`); el resto del tiempo no
cambia nada. Con la pantalla apagada no se dibuja ni se despierta nada.

Captura: `docs/screenshots/R5-F5-hub-marquesina-respiracion.png` (dos
instantes sonando — el título en posiciones distintas —, tres en pausa —
quieto, y atenuado en el tercero).

## M-086 — Anillos del reproductor con antialiasing (corrige M-083)

**Reporte del dueño tras probar v0.5.0 en el iPod:** *"solo no me gustaron los círculos… se notan muy pixelizados."*

M-083 leyó "que cuides el antialiasing" como "sin antialiasing" y dibujó el anillo con el algoritmo de punto medio, a píxel entero. En el simulador ampliado se veía nítido; en el panel real de 320×240 se ve como una escalera. La lectura correcta era la obvia: suavizarlo.

**Decisión.** Anillo por **cobertura**: para cada píxel del cuadro del anillo, la distancia al centro (raíz entera en 8.8) decide cuánta tinta lleva, y esa tinta se mezcla contra el píxel que **ya está debajo** (`metro_fb_plot_alpha`, nuevo en `metro_fb.c`: lee, mezcla, escribe en el viewport actual — real u offscreen). Así funciona igual sobre el fondo plano y sobre la carátula atenuada, y dentro de un frame pre-renderizado de transición. Grosor ≈ 1.5 px (pleno a ±0.25 px del radio, cero a ±1.25): un anillo de exactamente 1 px suavizado se reparte en dos filas a media intensidad y se ve gris; así queda delgado pero sólido. Costo: (2r+1)² raíces enteras por anillo, ~730 para r=13, tres anillos, una vez por segundo.

Captura: `docs/screenshots/R5-anillos-antialias.png` (izquierda 1 px suavizado, derecha el elegido).

## M-087 — Las listas de Música se cortaban en 300 ("no hay canciones después de la E")

**Reporte del dueño:** con una biblioteca grande, Canciones muestra 300 y se detiene en la E, en Metro **y** en Aura.

**Causa.** `METRO_MUSIC_MAX_ITEMS = 300` (heredado tal cual de `AURA_MUSIC_MAX_ITEMS`), un solo tope para todas las listas: canciones, artistas, álbumes, géneros, listas y las sublistas. Tagcache entrega los títulos **ya ordenados**, así que las primeras 300 que caben son de la A hasta donde alcance — y no había ningún aviso en pantalla. Las canciones están en el disco y en la base; la pantalla no tenía dónde ponerlas.

**Decisión.** Un tope por clase de lista, dimensionado para una biblioteca real en un aparato de 64 MB:
- `METRO_MUSIC_MAX_SONGS = 5000` (lista plana, canciones de un género, y la lista de reproducción que se arma al elegir una canción).
- `METRO_MUSIC_MAX_GROUPS = 2000` (artistas, álbumes, géneros, listas, álbumes de un artista, canciones de un álbum). `METRO_ARTIST_IMAGES_MAX` sigue a este tope (era 300 por la misma herencia).
Todo estático (nunca en la pila de 8 KB, D-226): `.bss` pasa a 7.2 MB en ARM y el búfer de audio queda en ≈55 MB. Los recorridos de tagcache que llenan las listas corren contra la copia en RAM (`tagcache_ram`, ya activado en `metro_main.c`), y el ordenamiento por inserción sobre entrada casi ordenada es cercano a lineal.

**El tope ya no es silencioso:** las listas de filas (canciones, géneros, canciones de un género/álbum) agregan una fila final terciaria "…y más: la lista está llena" cuando llegan al tope (`LANG_LIST_TRUNCATED`); las cuadrículas (artistas, álbumes) no, porque su índice es el de un tile real.

**Pendiente:** (1) cronometrar en el iPod cuánto tarda entrar a Música con miles de canciones — si molesta, cargar cada pivot la primera vez que se abre en vez de las cinco listas al entrar; (2) la misma corrección en Aura-Firmware, en su propia sesión (regla de contención). Capturas: lista de canciones sin cambios visuales.

## M-088 — Pantalla USB propia: el logo de Rockbox desaparece

**Reporte del dueño:** *"Al conectarlo vía USB todavía aparece el gráfico feo de Rockbox. Ayúdame a cambiar esa pantalla por algo que Metro hubiera hecho como indicio de conexión USB/sincronización."*

**Qué pasaba.** `metro_main.c` dibujaba la pantalla de Metro un instante y luego `default_event_handler()` entregaba el LCD a `gui_usb_screen_run()` de Rockbox, que pintaba `bm_usblogo` durante toda la sesión (DESVIACIONES F9-1 lo documentaba como deuda).

**Restricción dura.** Mientras el Mac tiene el disco, `font_disable_all()` descarga las fuentes a propósito y no se puede leer nada del volumen. Todo lo que se dibuje tiene que estar embebido en el binario. Lo que sí lo está: los colores del tema (enum en RAM), la tabla de iconos (`metro_icons_table.c`) y el wordmark "metro" (`bm_rockboxlogo`, 320×98, generado por `gen_logo.py`, compilado por `bmp2rb`; `INITDATA_ATTR` es no-op en S5L8702, D-223 de Aura).

**Decisión.** `metro_screen_usb.c` se reescribe con solo eso: el wordmark dibujado **como máscara** (la luminancia de cada píxel es la mezcla fondo→texto, `metro_fb_plot_alpha`), con lo que sale bien en tema claro y oscuro — un `lcd_bitmap` plano pintaría una losa negra en el claro; el glifo `arrow_sync` de Fluent (nuevo en la tabla) a 2× en acento arriba; y debajo el **indicador indeterminado de WP7**: cinco puntos en acento que cruzan la pantalla rápido en los bordes y lento por el centro, escalonados, con pausa entre barridos — geometría pura. **Sin texto**: la única fuente disponible sería la sysfont de 8 px de Rockbox, que no tiene nada que ver con Metro.

`apps/gui/usb_screen.c` (MODIFICATIONS.md, `Metro (M-088)`): en iPod 6G la pantalla principal llama `metro_screen_usb_show()` en vez de `bm_usblogo`, y el bucle sondea a `HZ/10` llamando `metro_screen_usb_tick()` (repinta solo la franja de 6 px de los puntos). `metro_apply_hygiene()` apaga `usb_hid` — el "modo teclado" USB de Rockbox no tiene lugar en Metro y además su rama del bucle nunca llegaba al tick. Puertas: `lcd_active()` y `animations != off` (los puntos se congelan).

Verificado en simulador con `USB_INSERT`: `docs/screenshots/R5-usb-metro.png`.

## M-089 — El glifo de la pantalla USB, suavizado (máscaras de cobertura de 8 bits)

**Reporte del dueño (v0.5.2 en el iPod):** *"el icono de sincronización se ve un poco pixeleado… quizá utilizando alguno ya existente de alguna biblioteca de iconos gratuita."*

M-088 dibujó el glifo de 16 px de la tabla monocroma escalado 2×: cada píxel se vuelve un bloque de 2×2, y en el panel real se nota. El icono ya era de una biblioteca libre (Fluent, MIT); lo que faltaba era el **tamaño**. Fluent publica `arrow_sync` solo hasta 24 px, pero el SVG es vectorial: rasterizado a 40 px con antialiasing sale limpio y con un trazo fino que empareja con el wordmark Light.

**Decisión.** Segunda tabla generada, `metro_glyphs_table.c` (misma lista/pipeline de `gen_icons.py`, sección `GLYPHS`): máscaras de **cobertura de 8 bits** (un byte por píxel, el canal alfa tal cual), 1 600 bytes el de 40×40. `metro_widgets_draw_glyph()` las pinta mezclando el color elegido contra lo que haya debajo (`metro_fb_plot_alpha`), así que siguen sin RGB fijo y funcionan sobre cualquier fondo. La tabla monocroma de 16 px se queda para lo que se dibuja a 16 px; el glifo grande existe porque la pantalla USB no puede leer disco y necesita más de 16 px. Captura: `docs/screenshots/R5-usb-icono-antialias.png`.

## M-090 — "Cambiar a Aura" desde Ajustes (contrato v10: dos firmwares instalados, cambio por renombre)

**Encargo del dueño:** que cada firmware pueda cambiar al otro desde sus ajustes, "aunque nos obligue a resetear el iPod", y que la instalación previa no se borre.

**Contrato v10** (Aura-Firmware canónico, copia en Studio; Metro lo lee de allá): el árbol activo sigue siendo `/.rockbox` (lo único que arranca el bootloader); el de Aura duerme completo, con sus ajustes, como `/.firmware-aura`; el de Metro, cuando Aura está activa, como `/.firmware-metro`. Cambiar = renombrar, no copiar.

**Implementación.** `metro_firmware_aura_installed()` / `metro_firmware_switch_to_aura()` en `metro_settings.c` (es quien puede armar rutas del contrato). La secuencia, en orden y sin nada entre medio: (1) `metro_settings_save()` + `settings_save()` + `tagcache_shutdown()` + `call_storage_idle_notifys(true)` — **todo** lo de Metro a disco ahora, porque tras el renombre `/.rockbox` es el árbol de Aura y cualquier escritura tardía caería allí; (2) `/.rockbox` → `/.firmware-metro` (saliente primero: el peor corte deja un dormido entero, que Studio levanta al conectar); (3) `/.firmware-aura` → `/.rockbox`, y si falla se deshace (2) y se sigue siendo Metro; (4) `/rockbox.ipod` de raíz := el del árbol (respaldo del bootloader, siempre el activo; copia a trozos con buffer estático); (5) `metro_sync_write_music_pending_marker()` — la base de música vive dentro de cada árbol, la de Aura está desactualizada; (6) `system_reboot()`: **nunca** el apagado normal, que guardaría los ajustes de Metro en el árbol de Aura. Si ya existiera `/.firmware-metro` (Studio garantiza que no) se aborta sin borrar nada. La fila de Ajustes es "cambiar a Aura" con subtítulo "no instalado" e inerte cuando no hay árbol dormido; con él, pide confirmación ("¿cambiar a Aura y reiniciar?").

Verificado en simulador con un árbol dormido de prueba: tras confirmar, `.rockbox` es el de Aura, `.firmware-metro/aura/aura.cfg` conserva `firmware_family: metro` y los ajustes, `/rockbox.ipod` es el binario de Aura y el marcador trae `music: true`. (El `system_reboot()` del simulador no termina el proceso; en el iPod es un reinicio real.)

## M-091 — El cambio de firmware deja de reconstruir la base sin motivo (contrato v12, sello de biblioteca)

Mismo reporte y misma corrección que D-329 en Aura-Firmware (la causa era nuestra: el cambio de v10 dejaba `music: true` siempre). `/.aura/library-stamp` + `db_stamp.txt` por árbol; `finish_ok()` de `metro_sync.c` anota el sello al terminar bien cualquier (re)construcción; `metro_firmware_switch_to_aura()` solo escribe el marcador si los sellos difieren, con el arranque en frío que sella al saliente. Verificado en simulador: primer cambio → marcador + sello; ida y vuelta sin sync → sin marcador. Las rutas nuevas viven en `metro_sync.c` (dueño de `/.aura`) — regla de rutas del contrato.

## M-092 — El wordmark pasa a "metro / aura" (encargo del dueño)

*"Me gustaría que dijera 'metro' y abajo 'aura'."* Dos sitios, mismo par: el **bitmap embebido** (`gen_logo.py` → `rockboxlogo.320x98x16.bmp`: "metro" Selawik Light 56 + "aura" a 26 en gris 60% — al dibujarse como máscara queda atenuado) que usan el arranque y la pantalla USB (M-088), y el **splash de runtime** (`metro_screen_splash.c`: "metro" en display + "aura" en título/secundario, bloque centrado, barra de progreso debajo del bloque). Captura: `docs/screenshots/R5-splash-metro-aura.png`.

## M-093 — "Cambiar sistema": submenú con una fila por familia hermana (tres familias)

**Contexto.** Ahora hay tres familias de firmware bajo el contrato v10: Aura (`/.firmware-aura`), Metro (`/.firmware-metro`, esta) y moonlit.aura (`/.firmware-moonlit`, `firmware_family: moonlit`). M-090 solo sabía "cambiar a Aura": la ruta del dormido y el nombre estaban cableados en `metro_settings.c` y en la fila 8 de Ajustes › General.

**Decisión.** La fila "cambiar a Aura" se sustituye por un submenú **"cambiar sistema"** (`METRO_ROW_NAV`, sin subtítulo) con una fila por familia hermana. La lista de hermanos es una tabla pura, sin I/O, en `apps/metro/metro_firmware_families.c` (`{"/.firmware-aura", LANG_FAMILY_AURA}, {"/.firmware-moonlit", LANG_FAMILY_MOONLIT}`; `METRO_FW_OWN_DORMANT = "/.firmware-metro"` fuera de la tabla: nunca se cambia a uno mismo). Añadir una familia = una línea en la tabla + su `LANG_FAMILY_*` al final del catálogo (M-009).

- `metro_settings.c`: `metro_firmware_aura_installed()`/`metro_firmware_switch_to_aura()` pasan a `metro_firmware_sibling_installed(i)` / `metro_firmware_switch_to(i)` (`metro_settings.c:245-320`). La secuencia de seis pasos de M-090/M-091 es **exactamente la misma**: guardas (hermano no instalado → false; `/.firmware-metro` ya existe → false), vaciado a disco, `/.rockbox` → `/.firmware-metro`, `/.firmware-<hermano>` → `/.rockbox` con rollback, respaldo `/rockbox.ipod`, marcador condicional al sello (`metro_sync_switch_needs_rebuild`), `system_reboot()`. Las invariantes v10 quedan intactas; solo cambia de dónde sale la ruta del entrante.
- `metro_screen_settings.c`: página local `switch_page` (patrón `options_page` de Now Playing): título de fila = `metro_lang_str(sibling->name)`, subtítulo "no instalado" si no hay dormido (fila inerte), y con dormido confirmación `"¿cambiar a %s y reiniciar?"` (`LANG_DIALOG_SWITCH_FMT`) → `metro_firmware_switch_to(i)`.
- `metro_widgets_confirm()` envuelve la pregunta en dos líneas cuando no cabe en 320 px (rompe en el último espacio que quepa, centrado sobre la línea base original): "¿cambiar a moonlit.aura y reiniciar?" se cortaba a la derecha en una sola línea de 28 px.
- Test host `apps/metro/test/test_firmware_families.c`: dos hermanos, ninguno es el propio, todos bajo `/.firmware-`, nombres distintos, fuera de rango → NULL.

Capturas: `docs/screenshots/v0.6.0-cambiar-sistema-vacio.png` (sin dormidos), `…-instalados.png` (ambos dormidos presentes), `…-confirmar.png` (diálogo para moonlit.aura). Secuencia en el simulador: Hub `SCROLL_FWD×3, SELECT` (ajustes) → `SCROLL_FWD×8, SELECT` (fila "cambiar sistema") → `SCROLL_FWD, SELECT` (moonlit.aura). El `system_reboot()` del simulador no termina el proceso, igual que en M-090.

## M-094 — `__TIME__`/`__DATE__` fuera de los plugins SDL (delta espurio de ~2,2 MB en la actualización selectiva)

**Qué pasaba.** La actualización selectiva de Studio (contrato v11, C28) compara CRC32 por archivo entre releases. Entre v0.5.5 y v0.5.6 el delta traía ~2,2 MB que no correspondían a ningún cambio: `quake.rock` y `duke3d.rock` cambian en **cada** build porque incrustan la hora de compilación — `Con_Printf ("Exe: "__TIME__" "__DATE__"\n")` en `quake/host.c` y `quake/host_cmd.c`, y `printf("Compiled %s …", __DATE__)` en `duke3d/Engine/src/display.c`. Es justo lo que el CLAUDE.md prohíbe ("nada de `__DATE__`/`__TIME__` en lo que viaje dentro de `rockbox.zip`"), heredado de upstream.

**Decisión.** Cadena fija `"rockbox build"` en los tres sitios (comentario inline `Metro (M-094)`, registro en `MODIFICATIONS.md`); el `#ifndef __DATE__` de duke3d queda eliminado por muerto. `grep -rn '__TIME__\|__DATE__' apps/plugins/sdl/progs/{quake,duke3d}` solo devuelve los comentarios. Sin efecto visible: eran líneas de consola interna de los juegos.


## M-095 — Base de datos tagcache compartida en `/.aura/tagcache` y sello compartido (contrato v15)

**Qué pasaba.** Los `.tcd` de tagcache vivían dentro de cada árbol (`/.rockbox/database_*.tcd`), así que cada familia mantenía su propia base y cada cambio de firmware la reconstruía si el sello difería. Además el sello v12 (`db_stamp.txt`) solo se escribía en `finish_ok()` de `metro_sync.c`: la reconstrucción de bootstrap de `metro_music_db_ready()` (la que dispara Metro cuando no hay base, sin marcador de Studio) **no sellaba**, con lo que el primer cambio de familia reconstruía aunque nada hubiera cambiado.

**Hechos que lo permiten.** `apps/tagcache.c/.h` son byte-idénticos en los tres repos (mismo `TAGCACHE_MAGIC`); la ruta de los `.tcd` es de runtime — `global_settings.tagcache_db_path` (`apps/settings_list.c:1817`, default `ROCKBOX_DIR`) — y `tagcache_init()` la copia una sola vez a `tc_stat.db_path` (`apps/tagcache.c:5624`); `open_db_fd()` hace `mkdir` del directorio si falta (`apps/tagcache.c:590`).

**Decisión.**
- `AURA_SHARED_DB_DIR "/.aura/tagcache"` en `metro_settings.h:118` (dueño de rutas del contrato). `metro_force_shared_db_path()` (`metro_settings.c:271`) fija `global_settings.tagcache_db_path` con `strmemccpy` — no se escribe en `main.c`: `metro_apply_hygiene()` **ya corre dentro de `init()`** entre `settings_load()` e `init_tagcache()` en ambos cuerpos (`apps/main.c:454-469` simulador, `:724-778` target, M-019), que es exactamente la ventana necesaria (después de que `settings_load()` restaure lo que diga `config.cfg`, antes de que `tagcache_init()` copie el valor). La premisa del encargo ("hygiene corre después de `init()`") era incorrecta; se cita aquí para que no se vuelva a buscar. Sin cambios a archivos de Rockbox.
- **Migración sin rebuild** en la misma función: si no existe `/.aura/tagcache/database_idx.tcd` y sí `/.rockbox/database_idx.tcd`, `mkdir` + `rename()` de todos los `database_*.tcd` (misma partición FAT: reescritura de entrada de directorio, sin copia). Si la base compartida ya existe (la construyó otra familia), los `.tcd` del árbol se **borran**: la compartida manda y el sello decide si está vigente; una copia por árbol solo sería peso muerto.
- **Sello compartido** `/.aura/tagcache/db_stamp.txt` (`metro_sync.c:46`): `metro_sync_record_db_stamp()` escribe ahí; `metro_sync_switch_needs_rebuild()` (`metro_sync.c:192`) ya no recibe árbol — compara ese sello con `/.aura/library-stamp`; sin sello de biblioteca (arranque en frío) sella la base compartida, que está al día, y devuelve `false`. `metro_sync_migrate_db_stamp(db_was_migrated)` (`metro_sync.c:177`) renombra el `.rockbox/aura/db_stamp.txt` pre-v15 al compartido **solo si la base migró con él** y no hay sello compartido; en cualquier otro caso lo borra (un sello describe la base junto a la que se escribió, no la de otra familia).
- **Sellado tras bootstrap**: `metro_music_bootstrap_tick()` (`metro_music.c:212`), sondeado desde el bucle ocioso de `metro_main.c` (la reconstrucción es asíncrona y el usuario puede no volver a entrar a Música en esa sesión), sella una vez cuando `tagcache_is_usable() && tagcache_is_fully_initialized()` y el sync está `IDLE`, si hubo rebuild de bootstrap **o** la base no tiene sello (migrada de un árbol pre-v12): el `tagcache_start_scan()` de una vez por arranque es lo que la pone al día. Con marcador pendiente nunca llega aquí (`db_ready()` cede al job); con marcador en error el marcador sigue forzando su propia reconstrucción, el sello no le afecta.

**Verificado en simulador.** Con `simdisk/.rockbox/database_*.tcd` (11 archivos) y sin `/.aura/tagcache`: tras arrancar, `ls simdisk/.aura/tagcache/` muestra los 11 `.tcd` con mtime original (24 ago) y `simdisk/.rockbox/database_*` no existe; al entrar a Música aparece `db_stamp.txt` == `library-stamp` (`fw-20260826T104231-000000d8`); segundo arranque: `stat` de los `.tcd` idéntico. Borrando `/.aura/tagcache/*` y entrando a Música: rebuild de bootstrap + `db_stamp.txt` recreado con el mismo valor del sello de biblioteca. `config.cfg` no requiere cambios (el valor se fuerza en cada arranque). Sin test host: la lógica del sello son tres comparaciones de archivo inseparables del I/O.

## M-096 — Claves estables de carátula (crc32 de ruta + mtime) y thumbs compartidos en `/.aura/thumbs` (contrato v15)

**Qué pasaba.** `metrocache/albums/album-<seek>.mth` (`metro_screen_hub.c`, R4/M-074) usaba el `seek` de tagcache como clave, que **cambia en cada reconstrucción**: tras un sync la cuadrícula mostraba carátulas cruzadas o regeneraba todo, y los archivos viejos quedaban huérfanos porque `finish_ok()` no limpiaba nada. Fotos y artistas ya usaban `<archivo>.<mtime>`, derivado del archivo y no de la base.

**Decisión.**
- **Clave de álbum** `a-<crc32 de la ruta de la pista representativa, 8 hex>-<mtime>`: `metro_music_album_art_key()` (`metro_music.c:618`) resuelve la primera canción del álbum (`metro_music_songs_of_album`, mismo orden que ya usaba `album_thumb_decode`), su `tag_filename` y `tag_mtime` con **una** búsqueda (`tagcache_get_numeric(&tcs, tag_mtime)` con `tcs.idx_id` fijado — tagcache guarda el mtime del archivo por pista, `add_tagcache(path, info.mtime)`, así que no hace falta `opendir` del directorio padre), `crc_32()` de Rockbox. Memo en anillo de 64 entradas (la cuadrícula pide la clave en cada repintado de cada tile visible); se vacía en `metro_music_albums()` porque los seeks pueden haberse renumerado.
- **Ubicación**: `AURA_SHARED_THUMBS_DIR "/.aura/thumbs"` (`metro_settings.h:119`); `metro_settings_metro_cache_dir()` devuelve `/.aura/thumbs/<fuente>` (albums/artists/photos). `metro_settings_migrate_shared_thumbs()` (`metro_settings.c:329`, mismo gancho de arranque que M-095) renombra `.rockbox/aura/metrocache/` completo al compartido si no existe; si ya existe, borra el del árbol (datos derivados). `metrocache/` desaparece: no vivía nada más ahí (`metro_settings.c:214-219` pre-v15). `ensure_cache_dir()` de `metro_thumbs.c` crea ahora hasta tres niveles (`/.aura` puede no existir en un disco nuevo). Formato `.mth` sin cambios (80×80 `fb_data` crudo, el mismo de moonlit).
- **`remove_stale()`** (invalidación por mtime al decodificar) generalizada: el tallo es lo anterior al último `.`, o al último `-` cuando la clave no tiene punto (`key_stem_len()`, `metro_thumbs.c:159`) — así `a-<crc>-<mtime2>` sigue desalojando a `a-<crc>-<mtime1>`.
- **Huérfanos**: `metro_thumbs_gc(source, ctx, count)` (`metro_thumbs.c:429`) borra todo `.mth` de `albums/` cuya clave no sea la de ningún álbum actual (claves recordadas como crc32 en 8 KB estáticos + `qsort`/bsearch; una colisión solo **conserva** un archivo, nunca borra uno vivo; con más de 2 048 álbumes no borra nada). No corre con presupuesto ni en cada entrada: corre completo en `music_lists_refresh()` (`metro_screen_hub.c:400`, donde `s_albums` acaba de releerse) **solo si hay bandera "sucio"**, que es un archivo vacío `/.aura/thumbs/albums.dirty` (persiste al reinicio: un sync que termina al arrancar y un apagado antes de abrir Música la perderían en RAM). La levantan `finish_ok()` con `music: true` (`metro_sync.c`), el sellado de bootstrap (`metro_music.c`) y la migración de directorio (los `album-<seek>.mth` viejos llegan como huérfanos). Costo: una resolución de clave por álbum (dos búsquedas de tagcache) — una vez por sync, no por visita.

**Verificado en simulador.** Con 8 `album-<seek>.mth` en `.rockbox/aura/metrocache/albums/` y sin `/.aura/thumbs`: tras entrar a Música, `ls simdisk/.aura/thumbs/albums/` muestra `a-031b464b-1787198376.mth`, `a-e8c6c06d-1787198376.mth` (los dos álbumes visibles en Quickplay) y ningún `album-*`; `.rockbox/aura/` solo tiene `aura.cfg`. Borrando `simdisk/.aura/tagcache/*` y reiniciando (rebuild de bootstrap): `stat` de los `.mth` idéntico — no se regeneran — y `albums.dirty` queda pendiente para la siguiente entrada a Música. Build de simulador y target sin advertencias nuevas (las de `metro_screen_hub.c`/`metro_screen_nowplaying.c` que aparecen al recompilar son preexistentes; las tres de `metro_thumbs.c` desaparecen).

## M-097 — Caché maestra compartida de imágenes en `/.aura/art` (contrato v16), constructor en segundo plano

**Qué pasaba.** Cada familia (Aura, Metro, moonlit) decodifica el mismo JPEG por separado para su propia miniatura: `/.aura/thumbs/<fuente>/<clave>.mth` (M-096) es 80×80 y **propio de cada firmware** en formato (aunque compartido en ubicación) — Now Playing en Metro decodifica el JPEG completo *otra vez* a 136px para la carátula grande, y cambiar de familia (M-093) implicaba que la nueva reconstruyera desde cero cualquier decodificación ya hecha por la anterior. Un JPEG de portada nunca cambia entre visitas: decodificarlo más de una vez, o más de una vez *por familia*, es trabajo desechado.

**Decisión — formato y ubicación.**
- `/.aura/art/<albums|artists|photos>/<clave>.art`: cabecera de 16 bytes little-endian (`'MAST'`, `width`, `height`, `flags=0`, `reserved=0` — `metro_master_art_format.h:45`) + RGB565 LE fila-contigua, **cuadrado**, "llenar y recortar centrado" (nunca letterbox): 130×130 álbumes/artistas, 80×80 fotos (`METRO_MASTER_ART_*_PX`). `<clave>.none` (0 bytes) es el marcador negativo compartido — nadie reintenta un JPEG mientras exista. `AURA_SHARED_ART_DIR "/.aura/art"` (`metro_settings.h:123`), única función que arma la ruta: `metro_settings_master_art_dir()` (regla de rutas de contrato del CLAUDE.md).
- **Claves** — mismo esquema que M-096, generalizado a las tres fuentes: álbumes `a-<crc32 hex8 ruta pista representativa>.<mtime>` (= `metro_music_album_art_key()`, sin cambios); artistas `r-<crc32 de `<artists dir>/<archivo>`>.<mtime>` (`metro_music_artist_image_master_key()`); fotos `p-<crc32 de `/Photos/<archivo>`>.<mtime>` (`metro_photos_master_key()`). Formato puro en `metro_master_art_format.c` (sin dependencia de Rockbox, compila en host) — CRC-32/MPEG-2 pineado contra Rockbox `crc_32()` en `test/test_master_art.c` (34 checks: crc32, formato de clave, cabecera, filtro de caja 130→80, "cubrir" nearest-neighbour y bilineal).
- **Regla única, un solo lugar**: nunca decodificar un JPEG si la maestra existe; siempre escribir la maestra al decodificar. Todo consumidor pasa por `metro_master_art_probe()` → `read()` / decodificar + `write()`: el motor de tiles genérico (`metro_thumbs_tick()`, que ahora deriva su `.mth` de 80px de la maestra con `metro_master_art_box_down()` — filtro de caja, nunca de un segundo JPEG), la carátula/fondo de Ahora Suena (`metro_albumart.c`: `resolve_master_from_id3()` lee/escribe la maestra de 130px y la usa 1:1 como tile de Ahora Suena — el tile pasó de 136 a 130px, `METRO_ALBUMART_SIZE` ahora es `METRO_MASTER_ART_ALBUM_PX`; el fondo de pantalla completa se deriva de esa misma maestra con `metro_master_art_cover_bilinear()`, nunca de un segundo decode) y el constructor en segundo plano (abajo). `metro_master_art_gc()` barre huérfanos de `/.aura/art/<fuente>` con la misma tabla de claves vivas (crc32 ordenados, bsearch) que ya arma `metro_thumbs_gc()` — reutiliza la tabla cuando `master_key == cache_key` (álbumes) y la reconstruye cuando difieren (artistas/fotos, cuya clave `.mth` no lleva el prefijo `r-`/`p-`).

**Decisión — hilo vs. plan B: HILO real, no plan B.** `apps/recorder/jpeg_load.c` guarda todo el estado del decodificador JPEG en **un solo `static struct jpeg`** (`read_jpeg_file()`), y `metro_albumart.c`/`metro_thumbs.c` tienen sus propios búferes de scratch estáticos, igual que `metro_music.c` con `s_uniqbuf` para sus búsquedas de tagcache — dos decodificaciones o dos recorridos de tagcache entrelazados en un `yield()` se corromperían entre sí. La solución no es plan B (un elemento por vuelta ociosa): es un **hilo de baja prioridad real**, siguiendo el patrón de `apps/plugins/pictureflow/pictureflow.c`, con un **mutex recursivo único** (`metro_master_art_lock()`/`unlock()`, `metro_master_art.c:37`) que serializa *todo* decode JPEG y *todo* recorrido de tagcache del árbol — no solo los del constructor:
  - `metro_master_art_builder.c`: `create_thread(builder_thread, s_stack, 8 KB, PRIORITY_BACKGROUND, "metro_art")`, creado una vez desde `metro_main()` tras `metro_disk_handoff()` (`metro_main.c:294`). Camina álbumes → artistas → fotos, un elemento a la vez, bajo el lock, durmiendo entre elementos (`GAP_BETWEEN_ITEMS` = HZ/20 en reposo, `GAP_WHEN_PLAYING` = HZ/4 con audio activo).
  - **Se detiene** (`may_run()`, `metro_master_art_builder.c:104`) si: hay una transición/animación en curso (`metro_master_art_builder_pause(true)`, llamado alrededor de `metro_transitions_slide/push/fade` — `metro_transitions.c:177,377,424` —, que además toma y suelta el lock una vez como barrera: si un elemento está a medio decodificar, esperar el lock garantiza que termine *antes* de que la animación arranque, no una simple bandera comprobada entre iteraciones); si pasaron menos de 2s desde la última entrada de usuario (`metro_master_art_builder_note_input()`, llamado desde el bucle de `metro_main()` en cada `action != MACT_NONE`); si hay un `metro_sync_job_active()`; o si el audio está reproduciendo y el buffer de archivo está por debajo de la mitad (`audio_wants_the_disk()` — proxy de "cargando buffer": no existe una consulta directa de "buffering ahora mismo" en `playback.h`, así que se opta por el lado conservador, dejando pasar al audio).
  - **Termina sin gastar CPU**: sin pase pendiente ni bandera de reintento, el hilo hace `sleep(HZ/2)` y vuelve a dormir — no hay busy-loop. Un pase completo (álbumes+artistas+fotos) solo corre bajo demanda (`metro_master_art_builder_request_pass()`, llamada desde `metro_thumbs_mark_dirty()` — un sync con música, un rebuild de bootstrap, o la migración de M-096 — con la misma bandera "sucio" que ya dispara `metro_thumbs_gc()`) o al arrancar. Si la base de tagcache aún no está lista (`build_albums()` devuelve `false`), el hilo reintenta cada segundo hasta que lo esté, sin tocar artistas/fotos todavía.
  - `metro_music.c`, `metro_screen_photo_viewer.c` (su único `read_jpeg_file()` fuera del motor de tiles) y `metro_albumart.c`/`metro_thumbs.c` toman el mismo lock alrededor de cada decode/recorrido de tagcache — el mutex es recursivo (Rockbox cuenta reentradas del mismo hilo), así que el hilo del constructor puede llamar a `metro_music_album_art_key()` (que toma el lock de nuevo) sin interbloquearse consigo mismo.

**`.bss`**: +94 080 bytes (target ipod6g: 7 238 300 → 7 332 380). Desglose: `metro_master_art.c` `s_scratch` 130×130×2 = 33 800 B (el búfer compartido de decodificación/lectura bajo el lock); `metro_master_art_builder.c` pila del hilo 8 192 B + `s_seeks[2000]` 8 000 B + `s_live[2048]` 8 192 B; `metro_albumart.c` `s_tile` (nuevo, 130×130×2 = 33 800 B — antes la carátula de Ahora Suena vivía dentro de `s_scratch`, compartido con el decodificador; separarlos evita que un decode de Quickplay pise la carátula ya cacheada); `metro_thumbs.c` scratch +2 064 B (130px en vez de 128px de margen). Sin cambio en `.data`/`.text` relevante más allá del código nuevo (+7 620 B de `.text`).

**Verificado en simulador.** Con `simdisk/.aura/thumbs/` y `.aura/art/` inexistentes (tagcache compartido ya poblado): `firmware/tools/sim_shot.sh` con 3000 ticks y **sin** entrar a Música puebla `/.aura/art/{albums,artists,photos}/` completos (12 álbumes: 8 `.art` + 4 `.none`; 2 artistas `.art`; 15 fotos `.art`) — el constructor corre desde el arranque, sin pantalla. Cabeceras validadas con `python3`/`struct`: `MAST` LE, 130×130 en álbumes/artistas, 80×80 en fotos, tamaño de payload exacto (`width*height*2`); `.none` de 0 bytes. `/.aura/thumbs/` se pobló también, pero solo con lo que Quickplay/la cuadrícula visitada realmente pidió (7 álbumes, 2 artistas, 9 fotos) — confirma que el barrido completo es del constructor, no del motor de tiles bajo demanda. Segundo arranque (3000 ticks más, sin tocar nada): mismo conjunto de 47 archivos, mismo `stat` (tamaño y mtime) byte a byte — nada se re-decodifica ni se re-escribe. Navegado a mano (`SELECT,RIGHT,RIGHT` → Álbumes; `SELECT,RIGHT` → Artistas; `SCROLL_FWD,SCROLL_FWD,SELECT` → Fotos; `SELECT,RIGHT,RIGHT,RIGHT,SELECT` → reproduce y entra a Ahora Suena): las tres cuadrículas muestran carátulas reales (colores de las imágenes de prueba fijas, no el tile de acento con inicial salvo en los álbumes con `.none` genuino) y Ahora Suena muestra la carátula de 130px de la maestra — capturas regeneradas en `docs/screenshots/v0.6.3-{albums,artists,photos,nowplaying}-from-master.png` (la de `nowplaying` que había dejado el agente anterior mostraba en realidad la lista de canciones, no Ahora Suena — la secuencia de botones no llegaba a reproducir nada; corregida). `make -C firmware/rockbox/apps/metro/test test`: 11 suites, todas en 0 fallos, incluida `test_master_art` (34 checks). `firmware/tools/build_target.sh` (firmware + bootloader): ambos en 0, sin advertencias nuevas (las de inicializador faltante en `metro_screen_hub.c`/`metro_screen_nowplaying.c` son preexistentes, de antes de F3).

## M-098 — `metro_music_db_ready()` se sondea desde el arranque, no solo desde el menú Música

**Qué pasaba.** `metro_music_db_ready()` (`metro_music.c:182-220`) es la única función que puede disparar `tagcache_rebuild()`/`tagcache_start_scan()` para reparar una base que tagcache decidió que "no es usable". Su único llamador en todo `apps/metro/` era `metro_screen_hub.c:828` (`hub_on_select()`, caso "Música"). El constructor de caché maestra (`metro_master_art_builder.c`, M-097) es un hilo real que espera `tagcache_is_usable()` por su cuenta (consultas directas a `apps/tagcache.c`) pero nunca llamaba a `metro_music_db_ready()`, así que nunca disparaba la reparación si la base quedara marcada "no usable" sin que el usuario hubiera entrado a Música primero. Aura-Firmware ya resuelve esto: `aura_music_db_ready()` se sondea en cada vuelta del bucle principal desde el primer frame, sin gate de `lcd_active()` (`aura_main.c:487`, D-021).

**Decisión.** `metro_main()` sondea `metro_music_db_ready()` desde la rama ociosa del bucle principal (`MACT_NONE`, `metro_main.c:384-448`), autolimitado a una vez por segundo con un `last_dbready_tick` propio (`metro_main.c:248,412-416`) — el mismo patrón que ya usan `last_player_tick`/`last_hub_tick` en esa función, no hace falta acelerar el timeout del bucle (`HZ/10`–`HZ/20`) para esto. Sin gate de `lcd_active()`: el trabajo de tagcache tiene que seguir con la pantalla dormida, mismo criterio que Aura.

`metro_music_bootstrap_tick()` (M-095) queda **redundante** como llamada directa desde `metro_main.c`: `metro_music_db_ready()` ya la invoca internamente (`metro_music.c:220`) en cualquier camino que no sea el `return` temprano por `metro_sync_job_active()` — y `metro_music_bootstrap_tick()` comprueba esa misma condición al entrar (`metro_music.c:245`), así que ambas funciones eran no-op exactamente en los mismos casos. Se retira la llamada directa de `metro_main.c` y la función pasa de pública a `static` dentro de `metro_music.c` (`metro_music.c:80` declaración adelantada, `metro_music.c:238` definición) — ya no tiene ningún llamador fuera del archivo. Su doc-comment se actualiza para reflejar que ahora corre "gratis" con la misma cadencia de un segundo que `metro_music_db_ready()`.

**Verificado.** `make -C firmware/rockbox/apps/metro/test test`: 11 suites, 0 fallos (ninguna cubre `metro_music.c`/`metro_main.c` directamente — son módulos atados a Rockbox, no lógica pura host-testeable). `firmware/tools/build_target.sh --firmware`: 0. Simulador: captura de hub tras arranque limpio sin regresión visual; entrar a "Música" desde el hub sigue abriendo Quickplay normalmente (sin quedarse en "actualizando biblioteca").

## M-099 — No reiniciar con un commit de tagcache a mitad de escritura al cambiar de firmware

**Qué pasaba.** `metro_firmware_switch_to()` (`metro_settings.c:432-476`, M-090/M-093) llama `tagcache_shutdown()` (`apps/tagcache.c:5508-5517`, solo vacía la cola de escrituras numéricas, nunca espera un commit en curso) y a continuación `rename()` + `system_reboot()`. Si tagcache estuviera a mitad de un `commit()` en ese instante, el header maestro **compartido** (`/.aura/tagcache/database_idx.tcd`, M-095) queda con `dirty=1` (`apps/tagcache.c:3538-3540`; solo se limpia al final del commit, `apps/tagcache.c:3593`) — el siguiente arranque de **cualquier** familia ve `check_all_headers()` devolver `false`, `tc_stat.ready=false`, y reconstruye la base compartida entera aunque los datos estuvieran bien.

**Decisión.** Antes del primer `rename()`, `metro_firmware_switch_to()` espera con tope a que `tagcache_get_commit_step()` (API pública ya existente, `apps/tagcache.c:5669-5672`) vuelva a 0 (`metro_settings.c:451-454`, tope `METRO_SWITCH_COMMIT_WAIT_TICKS = HZ * 8` definido en `metro_settings.c:430`). Si se agota el tope, procede con el switch de todas formas — no bloquear indefinidamente por una carrera rara; se documenta la decisión en el propio comentario del código (`metro_settings.c:435-438,446-449`). 8 s se eligió por ser holgado frente al costo real de un commit (la fase lenta, construir los índices, es la que `commit_step` va contando, y normalmente ya terminó mucho antes de que un usuario llegue a esta pantalla) sin arriesgar una espera larga si tagcache quedara atascado. No existe evidencia de que haga falta otro valor; se ajusta si aparece.

Nota: existe `tagcache_prepare_shutdown()` (`apps/tagcache.c:5495-5503`) que también consulta `tagcache_get_commit_step()`, pero solo para devolver `false` sin esperar — no sirve para este caso (se necesita esperar, no solo detectar) y no se usa aquí para no acoplarse a una función pensada para el apagado normal de Rockbox, que Metro no usa (M-006, `root_menu()` reemplazado). Sin cambios a `apps/tagcache.c`: solo se consume su API pública ya expuesta en `tagcache.h`, por lo que no aplica registro en `MODIFICATIONS.md`.

**Verificado en simulador.** Con una familia hermana falsa (`/.firmware-moonlit` vacío, fuera del árbol real de `simdisk/`, en una copia descartable) y la secuencia de M-093 (Hub `SCROLL_FWD×3, SELECT` → Ajustes; `SCROLL_FWD×8, SELECT` → "cambiar sistema"; `SCROLL_FWD, SELECT` → moonlit.aura) más `SCROLL_FWD, SELECT` para confirmar "sí": el switch completo (guardado, espera de commit, `tagcache_shutdown()`, los dos `rename()`) tardó ~12,17 s de pared desde el arranque del proceso hasta que `.firmware-metro`/`.rockbox` aparecían intercambiados en disco, contra ~11,87 s de una corrida idéntica que se detiene un paso antes (llega al diálogo de confirmación sin confirmar) — la diferencia (~0,3 s) es el costo de los dos botones extra inyectados, no una espera de 8 s: confirma que con `commit_step == 0` (caso normal, sin sync en curso) el switch no se retrasa. Tras el `rename()`, `system_reboot()` deja el proceso del simulador colgado esperando el cierre de la ventana SDL (comportamiento preexistente y ya documentado en M-090/M-093, "el `system_reboot()` del simulador no termina el proceso") — no relacionado con este cambio. Captura de la fila "cambiar sistema" mostrando el diálogo "¿cambiar a moonlit.aura y reiniciar?" con "no" por defecto, confirmando que la fila del hub sigue funcionando. Captura del hub tras un arranque limpio (simdisk real del repo, sin tocar) sin regresión visual. `make -C firmware/rockbox/apps/metro/test test`: 11 suites, 0 fallos. `firmware/tools/build_target.sh --firmware`: 0. `git status --short`: limpio salvo los cuatro archivos de `apps/metro/` tocados por M-098/M-099.

## M-100 — "actualizar biblioteca" prepara también las carátulas; el constructor no podía correr durante su propia preparación

**Encargo del dueño (2026-08-27).** Que las tres familias tengan en Ajustes una opción **"Actualizar Biblioteca"** con aviso de que puede tardar varios minutos según los archivos y el estado del disco, y que la preparación ocurra solo tras actualizar el firmware, tras un sync de Studio, o a pedido. Motivo: al entrar a Álbumes o Artistas las imágenes tardan en aparecer.

**Lo que ya existía.** La caché maestra compartida `/.aura/art/` (M-097, contrato v16) y la base compartida `/.aura/tagcache/` (M-095) ya están fuera de `/.rockbox/`, con las mismas claves y lados que Aura y moonlit — verificado leyendo los tres. La fila de Ajustes también existía: `LANG_SETTING_LIBRARY` ("biblioteca") con su confirmación y `metro_sync_request_manual()`. El comentario de `metro_sync.h` que decía *"no manual 'rebuild' trigger yet"* estaba desactualizado. Lo que faltaba era que esa preparación **incluyera las imágenes** y terminara antes de devolver el control.

**Decisión.** Terminado el trabajo de tagcache, `metro_sync.c` entra en `METRO_SYNC_BUILDING_ART`: espera a que la base sea consultable, corre una pasada **completa** del constructor de arte y recién entonces cierra la pantalla, mostrando el avance en la misma pantalla de espera (S4.3: sigue habiendo una sola). Solo cuando la música estuvo en juego. MENU la pospone: el constructor no se cancela, vuelve a su cadencia de fondo. La fila pasa a llamarse **"actualizar biblioteca"** y su aviso lleva la advertencia de duración.

**El problema no obvio: bloqueo mutuo entre el constructor y su propia preparación.** `may_run()` de `metro_master_art_builder.c` se niega a trabajar en tres casos — pausa de transición, menos de 2 s desde la última entrada del usuario, y **`metro_sync_job_active()`**. Durante una preparación explícita los tres se cumplen a la vez: el usuario acaba de apretar SELECT y la máquina de sync es justamente quien pidió la pasada. Sin tratarlo, el hilo no avanzaría nunca y la pantalla esperaría para siempre. `set_foreground(true)` salta esas tres puertas; la del **audio no se salta**: si suena música, el disco es suyo.

**La otra trampa, heredada de D-344 (Aura).** La pasada no puede arrancar en el instante en que tagcache termina: la base recién comitida todavía no es consultable y el recorrido de álbumes se saltaría entero en silencio, dando la pasada por "completa" sin preparar una sola carátula. `METRO_SYNC_BUILDING_ART` espera a `tagcache_is_usable()` con tope de `HZ * 10`. Metro además ya tenía su propia red de seguridad (`albums_pending` reintenta álbumes), y `s_pass_done` solo se marca cuando **ni** hubo una petición nueva **ni** quedaron álbumes pendientes — una pasada incompleta nunca cierra la pantalla.

**Bug colateral corregido: el aviso no cabía.** `draw_question()` está topada en **dos** líneas a propósito (una tercera choca con "sí"/"no"), así que la advertencia de duración salía cortada a la mitad. En vez de mutilar el texto se agregó `metro_widgets_confirm_detail()`: la pregunta queda corta en `MFONT_TITLE` y la advertencia va debajo en `MFONT_CAPTION` — la jerarquía habitual de Metro. Su envoltorio tuvo a su vez un fallo propio, encontrado en la captura: cortaba también la **última** línea en su último espacio y perdía la palabra final ("…y cómo esté el" en vez de "…el disco."); ahora, si lo que queda entra entero, va entero.

**Verificado.**
- Build ARM (`firmware/build-ipod6g`): **0 errores, 0 warnings**.
- `make -C firmware/rockbox/apps/metro/test test`: **9 suites, 0 fallos** (93 + 51 + 15 + 1 625 + 45 + 2 058 + 20 + 51 + 204 comprobaciones).
- Simulador (build limpio + `make install`), recorrido real con inyección de botones: la fila **"actualizar biblioteca"** aparece en Ajustes › general; el aviso muestra la pregunta y **la advertencia completa** en dos líneas de caption (captura); al aceptar corre la base y sigue con **"preparando carátulas 6/12"** bajo "actualizando biblioteca…" (captura), y la pantalla **se cierra sola** devolviendo a Ajustes. Con la caché maestra vaciada antes de la corrida, quedaron **12 maestras de álbum** escritas.
- **Nota sobre la captura del progreso**: la biblioteca del simulador tiene 12 álbumes y la fase se resuelve en menos de un segundo. La captura con "6/12" se tomó con la espera entre elementos subida a `HZ/3` **solo para poder observarla**; se revirtió y la verificación final corrió con el código tal como queda.
- **Warnings preexistentes, no tocados**: dos `-Wformat-truncation` en `metro_master_art_builder.c:238/295` (`snprintf` de rutas, sin líneas nuevas mías — `git diff` no agrega ningún `snprintf`) y varios `-Wmissing-field-initializers` en `metro_screen_hub.c`. Solo aparecen en el build del simulador, que usa otros flags que el de ARM.
- **Límite documentado**: no se probó en hardware con una biblioteca real de miles de pistas, ni el disparo por marcador de un sync de Studio real (exige Mac + iPod); los dos entran por el mismo `finish_ok()`, que es el punto modificado.

## M-101 — Pila del hilo principal 8 KB → 12 KB, y una herramienta que lo vigila (`stack_report.py`)

**Ronda "homologación", Fase 1.** Plan maestro `docs/plans/PLAN-ronda-3-firmwares-maestro.md` §E, ejecutado igual en las tres familias.

**El problema es común a las tres familias, aunque el panic se lo comiera Aura.** `firmware/target/arm/s5l8702/app.lds` es el mismo archivo en Aura-Firmware, Metro-Aura y moonlit.aura, y daba **8 KB** de pila al hilo principal (`0x7d30..0x9d30`). Ese hilo es el de la UI, y en las tres familias carga marcos que Rockbox nunca tuvo ahí: decodificación de JPEG y recorridos de tagcache bajo un solo lock (M-097), derivación de la caché maestra, el motor de tiles. Aura-Firmware reportó un `Stkov main` real con `sp` **528 bytes por debajo** del inicio de la pila (plan maestro §0, hallazgo 1). Metro no ha panicado, pero corre el mismo código sobre la misma pila: esperar el panic propio para reaccionar sería administrar por accidente.

**Decisión.** `. += 0x2000` → **`. += 0x3000`** (12 KB). Cabe: la IRAM de core mide 48 KB (`IRAMSIZE`, `0xC000`); tras el aumento `_fiqstackend` queda en **`0xb530`** (verificado en `firmware/build-ipod6g/rockbox.map`), o sea **2 768 B libres**. Comentario inline `Metro (M-101)` en el punto exacto y registro en `MODIFICATIONS.md` (GPL v2 §2a: `app.lds` es un archivo de Rockbox fuera de `apps/metro/`).

**`firmware/tools/stack_report.py`, portado de Aura-Firmware (D-345), no reescrito.** El plan maestro §E.3 pedía compilar con `-fstack-usage` y leer los `.su`. La primera versión de esta sesión hacía exactamente eso, en un árbol de build aparte. La sesión A ya había encontrado por qué no sirve y lo dejó documentado en D-345: **tras el inlining los `.su` cuentan la misma memoria dos veces** (su caso medido: gcc integra `style_fonts_exist` dentro de `aura_style_scan`, y sumar los dos marcos infla el camino). La herramienta portada mide el **desensamblado del binario que de verdad se publica** (`arm-elf-eabi-objdump -d`, sumando `push`/`stmdb` + `sub sp, #imm` de cada prólogo) y saca el grafo de llamadas del mismo desensamblado, así que marcos y aristas siempre corresponden al mismo binario; además no exige un segundo árbol de compilación, así que `package_dist.sh` la corre en segundos sobre el `.elf` recién enlazado. Se copió el mecanismo tal cual; solo cambian las rutas (`apps/metro/`), las raíces del hilo de UI (`main`, `metro_main` — `root_menu()` no existe aquí, M-006) y `GUARDED_EDGES`. Se conserva `--su-dir` como contraste opcional. **Desviación respecto al plan maestro §E.3 registrada en `docs/DESVIACIONES.md`.**

**`GUARDED_EDGES` queda VACÍA en Metro, a propósito.** Aura corta ahí la arista `skin_get_gwps → skin_load` porque D-345 hizo que `settings_apply_skins()` no cargue skins. **Metro no porta ese cambio**, y la razón es una medición, no una preferencia: el camino de skins cuesta **5 136 B** desde `skin_get_gwps` (5 296 B desde `settings_apply_skins`) y **no es el peor camino del binario** — el peor nace en `metro_music.c`/tagcache. Tocar el motor de skins de Rockbox para ganar un camino que ya cabe es riesgo sin beneficio. Se reevalúa si el peor camino se acerca al tope.

**Fila oculta de marca de agua de pila en "acerca de"** (§E.4): es la única forma de que el dueño confirme en hardware sin esperar otro panic. Se apoya en `thread_get_debug_info()`, la misma API pública que usa el menú de depuración de Rockbox (`apps/debug_menu.c:184`) — no se copia su lectura del canario `DEADBEEF`, que vive en un header privado del kernel (`firmware/kernel/thread-internal.h`). El total sale de los símbolos `stackbegin`/`stackend` del enlazador, que son exactamente los que `__get_main_stack()` le entrega al hilo principal en `init_threads()`. **En el simulador la fila dice "n/d en el simulador"**: ahí el hilo principal es el de SDL, administrado por el sistema operativo, y Rockbox ni siquiera compila los campos (`struct thread_debug_info` los envuelve en `#ifndef HAVE_SDL_THREADS`). Por eso en el simulador la fila está **siempre visible** (para poder capturarla) y en hardware está **oculta**, revelada con SELECT sostenido sobre la fila de versión.

- **La fila de versión ahora dice la versión.** `LANG_ABOUT_BASED_ON_ROCKBOX` no llevaba subtítulo; ahora lleva `rbversion`. Sin eso, la marca de agua que reporte el dueño no se puede atribuir a un binario concreto — que es justo para lo que existe.
- **Gesto**: `MACT_SELECT_HOLD` nuevo en `MCTX_LIST` (`{ BUTTON_SELECT | BUTTON_REPEAT, BUTTON_NONE }`), y `struct metro_pivot` gana `on_select_hold` **al final** (todo inicializador posicional existente sigue válido, misma regla que `tile_cols`/`empty_message`). Es el mismo par corto/sostenido que el reproductor ya prueba con `MACT_OPTIONS`/`MACT_TOGGLE_SHUFFLE`: el `BUTTON_REL` que sigue a un `REPEAT` **no** vuelve a disparar la acción corta, porque el prebutton del mapeo corto exige que el último botón haya sido `BUTTON_SELECT` a secas.

**El bug latente que destapó la herramienta: un build limpio del target no compilaba.** Al crear un árbol de build desde cero (la primera versión de `stack_report.py` lo necesitaba), `make` moría con `No rule to make target '<build>/metro_palette.h', needed by mpegplayer.o`. Causa: la pasada de dependencias (`mkdepfile`, `tools/functions.make:57`) arma su línea de comandos con `PPCFLAGS` + `OTHER_INC`, **no** con `MPEGCFLAGS` — y M-059 había agregado `-I$(APPSDIR)/metro` solo a `MPEGCFLAGS`. Sin resolverlo, el `-MG` convierte `metro_palette.h` en un archivo fantasma dentro del build que ninguna regla sabe construir. Llevaba escondido desde el 2026-08-19 porque el `firmware/build-ipod6g/` del árbol de trabajo arrastra un `make.dep` **anterior** a esa fase: nadie había reconstruido desde cero. Corregido con `OTHER_INC += -I$(APPSDIR)/metro` (`mpegplayer.make`, marca inline `Metro (M-101)`, registro en `MODIFICATIONS.md`); sin efecto sobre el binario, solo sobre la pasada de dependencias.

**`+HOLD` en el inyector del simulador** (`uisimulator/common/sim_tasks.c`, `MODIFICATIONS.md`): el inyector solo sabía press-then-release, así que **ningún gesto sostenido de Metro** se podía verificar sin la ventana SDL interactiva. Con el sufijo (`"SELECT+HOLD"`), la secuencia posteada es `press → BUTTON_REPEAT → BUTTON_REL`, que es lo que produce el driver del 6G al pasar el umbral de repetición. Mismo carácter que los tokens `USB_INSERT` (M-039) y `POWEROFF` que ese archivo ya traía: postear lo que el driver hubiera posteado. Solo compila en el simulador.

**Auditoría del `s_uniqbuf` compartido (encargo de la supervisora).** Los cinco `tagcache_search_set_uniqbuf(&tcs, s_uniqbuf, …)` de `metro_music.c` (`run_search`, `metro_music_recent_albums`, `metro_music_album_key_for_track`, `metro_music_album_seeks`, `insert_matching_tracks`) están **los cinco** dentro de `LOCK()`/`UNLOCK()`, y el hilo del constructor (`metro_master_art_builder.c`) solo llega a tagcache a través de esas mismas funciones. No hay uso sin candado entre el hilo de UI y el hilo `metro_art`: es exactamente la invariante que M-097 fijó al crear el mutex recursivo único. Sin cambio.

**Verificado.**
- `firmware/tools/build_target.sh --firmware`: **0 errores, 0 warnings**. `_fiqstackend = 0xb530` ≤ `0xC000` en `rockbox.map`.
- `firmware/tools/build_sim.sh` (con `make install`): 0 errores.
- `make -C firmware/rockbox/apps/metro/test test`: 11 suites, 0 fallos.
- `stack_report.py` sobre el binario del target: pila **12 288 B**; peor camino estático **7 400 B desde `main` (60,2 %)** y 7 304 B desde `metro_main` — **debajo del tope del 75 %**. 274 funciones con salto indirecto cuyo subárbol no se sigue, y los manejadores de fallo (`panicf`, `thread_stkov`, `UIE`, `buflib_panic`, `__div0*`) cortados del grafo: solo corren con el firmware ya caído, y dejarlos dentro hace que el peor camino de cualquier función que ceda el CPU incluya el manejador del desbordamiento que la herramienta existe para evitar.
- Capturas: `docs/screenshots/ronda-homologacion/f1-acerca-de.png` (fila de pila visible, con la versión en la fila de arriba), `…/f1-acerca-de-oculta.png` (tras SELECT sostenido: la fila desaparece **y no se navega a ningún lado**), `…/f1-acerca-de-pila.png` (segundo SELECT sostenido: vuelve).

**Lo que queda ROJO a propósito, y por qué.** `stack_report.py` sale con **FALLA (a)**: nueve funciones de `apps/metro/` superan el tope de 1 024 B de marco — `run_search` (1 592), `metro_music_album_key_for_track` (1 312), `metro_music_album_seeks` (1 288), `import_ratings` (1 272), `metro_music_album_art_key` (1 120), `metro_music_recent_albums` (1 112), `insert_matching_tracks` (1 104), `metro_thumbs_tick` (1 064), `write_marker` (1 040). Todas son el mismo patrón: `struct tagcache_search` (~800 B por sí sola: `seeklist[32]`, `idxfd[TAG_COUNT]`, `clause[32]`) más un `char path[MAX_PATH]` o un `buf[TAGCACHE_BUFSZ]` en la pila. **No es un descubrimiento incidental: es el primer resultado real de la herramienta**, y esas mismas funciones son ~4 KB de los 7 400 B del peor camino. Bajarlas a estáticos es seguro (las cinco de `metro_music.c` ya corren bajo el mutex único de M-097) pero toca cinco archivos y merece su propia pasada de verificación: **el plan lo agenda en la Fase 6** ("`stack_report.py` en verde"). Consecuencia mientras tanto: `package_dist.sh` **aborta** en el reporte, que es el comportamiento correcto — un paquete que no pasa este reporte no se publica.

## M-102 — Versión de formato de caché (`/.aura/art/format.txt`) y clave de álbum con el mtime de `cover.jpg` (contrato v18)

**Ronda "homologación", Fase 1.** Plan maestro §A, ejecutado igual en las tres familias.

**El problema que cierran las dos cosas juntas.** El recorte cuadrado ya existía en la caché maestra (M-097, `cover_geometry()`), pero una entrada de caché **no lleva versión de formato**: si una versión del firmware derivó mal un tile, la clave solo cambia cuando cambia la **fuente**, así que el tile malo sobrevive para siempre aunque se corrija el código. Y la clave de álbum de v16 (`a-<crc32 de la ruta de la pista>.<mtime de la pista>`) no se movía cuando Studio reescribía una `cover.jpg` **sin tocar ninguna pista** — que es exactamente lo que hace al re-obtener una carátula. Esa es la hipótesis (a) de D-338/M-096/D-055, abierta desde entonces.

**`/.aura/art/format.txt`** (contrato v18): un entero decimal, hoy `2`. Al arrancar, cada familia lo lee; si falta o es menor que el suyo, borra **todo** lo derivado del disco (`/.aura/art/{albums,artists,photos}` y la caché L2 privada, que en Metro es `/.aura/thumbs/`) y escribe su número. Studio nunca lo toca.

- La ruta la arma **solo** `metro_settings_master_art_format_path()` (regla de rutas del CLAUDE.md); la purga es `metro_settings_purge_derived_caches()`, en el módulo dueño de las dos raíces; la lógica de versión es `metro_master_art_check_format_version()`, en el módulo dueño del formato.
- **Dónde corre y por qué ahí**: en `metro_main()`, **entre** `metro_disk_handoff()` y `metro_master_art_builder_init()`. Es el único punto del arranque donde la purga no compite con el hilo del constructor escribiendo maestras en el directorio que está por vaciarse — por eso no viaja con el resto del trabajo de disco de una sola vez que vive en `metro_apply_hygiene()`.
- Un archivo **ilegible** (ausente, vacío, no numérico) cuenta como versión 1. Purgar cuesta una reconstrucción; conservar un tile mal derivado cuesta para siempre.
- La purga NO borra `/.aura/art` en sí (el `format.txt` se escribe ahí enseguida); sí borra sus subdirectorios, que `ensure_dir()` recrea a demanda.

**Clave de álbum v18**: el `<mtime>` pasa a ser `max(mtime de la pista representativa, mtime del `cover.jpg` hermano si existe)`. El **crc no cambia** — lo que identifica al álbum sigue siendo la ruta de la pista; lo que cambia es cuándo deja de ser válido lo derivado.

- La regla es una función **pura**, `metro_master_art_album_mtime()` (`metro_master_art_format.c`, compila en host), con 7 comprobaciones nuevas en `test/test_master_art.c` (34 → 41).
- **Sin `cover.jpg` el valor es 0 y la clave queda idéntica a la de v16**: una biblioteca con solo carátula embebida no revuelve su caché entera al actualizar. Un mtime negativo (lectura fallida) tampoco gana nunca: una clave no puede depender de que un `read` falle.
- El mtime del hermano lo lee `metro_fsutil_file_mtime()` — **Rockbox no tiene `stat()`**, así que la única vía a la fecha de un archivo es `opendir` + `dir_get_info()` del directorio padre, que es lo que ya hacían los escaneos de ese módulo. Se consulta solo al **resolver** la clave, que ya estaba memoizada en un anillo de 64 entradas (M-096), no en cada repintado.
- No hace falta repetir la búsqueda completa de `find_albumart()`: el contrato (`library-layout-v1` §2) pone la carátula de álbum exactamente ahí y con exactamente ese nombre, y cualquier **otra** fuente de arte (una APIC embebida, un archivo con otro nombre) viaja dentro de la pista y por lo tanto ya mueve el mtime de la pista.

**El último camino de imagen que no pasaba por el recorte cuadrado.** `metro_albumart_load_background_file()` con `mtime <= 0` decodificaba directo a 320×240 con `FORMAT_NATIVE | FORMAT_RESIZE` y **sin** `FORMAT_KEEP_ASPECT`: eso **estira** la imagen a la caja, no la recorta. Con una foto 4:3 o 16:9 —justo las que el dueño reportó— el fondo salía deformado (hallazgo 2 del plan maestro). Ahora ese camino usa la misma geometría que el normal: decode con `KEEP_ASPECT` a 130 px, `metro_master_art_cover()` (llenar y recortar centrado) y bilineal a 320×240. Sin clave no hay maestra que leer ni escribir, pero la **geometría** es idéntica. El decode y el recorte quedaron factorizados en `decode_square_master()`, usada por los dos caminos.

**Verificado en el simulador.**
- **Purga, medida con 2 048 archivos** sembrados a mano en las seis subcarpetas (`/.aura/art/{albums,artists,photos}` + `/.aura/thumbs/{…}`) y `format.txt` borrado: **10 ticks (~100 ms)**, muy por debajo del tope de ~2 s que fija el plan maestro §A.3.2 para decidir si la purga tiene que irse al hilo del constructor. Quedó `format.txt` = `2` y un solo archivo en el árbol. **Segundo arranque: sin purga** (idempotente). Tercer arranque: el constructor repobló la caché (31 archivos). La medición se tomó con un `printf` temporal bajo `#ifdef SIMULATOR`, **revertido**; el build verificado es el que queda. **La cifra es una cota inferior**: el simulador escribe sobre el sistema de archivos del host (SSD), no sobre FAT en el disco del iPod — la lista de verificación en hardware lo confirma.
- **Clave v18**: `touch` de `Music/Wheel & Click/Analog Dreams/cover.jpg` **sin tocar ninguna pista** → la clave pasó de `a-031b464b.1787198376` a `a-031b464b.1788544800` y la maestra vieja quedó desalojada por `remove_siblings()`. Eso es, exactamente, la hipótesis (a) cerrada.
- Target y simulador en 0 errores; 11 suites de tests host, 0 fallos.
- `docs/COMPAT_STUDIO.md`: filas **C32** (format.txt) y **C33** (clave v18).

**Límite documentado.** `Aura-Firmware/CONTRATO-firmware-studio.md` todavía dice **"Versión 17"**: la sesión canónica no ha publicado v18. Las dos cosas se implementaron contra el texto literal del plan maestro §A.2, que es la especificación acordada para v18. Anotado también en la cabecera de `docs/COMPAT_STUDIO.md`, para borrarlo cuando el repo hermano publique.
