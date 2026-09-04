# MODIFICATIONS.md — Aviso de modificaciones (GPL v2 §2a)

Este archivo cumple el requisito de la GPL v2 §2(a) de hacer constar,
de forma destacada, que este software fue modificado y la fecha de la
modificación.

## Origen

Este repositorio es un fork de [Rockbox](https://www.rockbox.org/)
(espejo [github.com/Rockbox/rockbox](https://github.com/Rockbox/rockbox)),
software libre bajo GPL v2. Rockbox se importó a `firmware/rockbox/`
en el commit base **`0726ec93517a61f602679ab052b083217ec9c96d`**
(2026-08-09), clonado sin el `.git` interno de Rockbox (se versiona
dentro del `.git` de este repositorio; ver `DECISIONS.md`, M-001).

Es el **mismo commit base** que usa el proyecto hermano
[Aura-Firmware](https://github.com/Ricolinos/Aura-Firmware) — ver
`DECISIONS.md` M-001 para la justificación.

## Los 10 archivos de Rockbox modificados fuera de `apps/metro/`

Todos son **fixes de hardware/build/compatibilidad heredados de
Aura-Firmware**, portados byte-idénticos (ver `DECISIONS.md` M-005,
`docs/DESVIACIONES.md` F0-2 sobre por qué no se reescribió su
atribución inline). Cada uno conserva su propio comentario de
modificación GPL original, autoría de Aura-Firmware, con referencia a
sus decisiones `D-NNN` (bitácora en el `DECISIONS.md`/`DECISIONS-ARCHIVE.md`
de ese repositorio, no en este). Este archivo documenta que además
fueron **portados a Metro-Aura** el 2026-08-20, sin más cambios que la
copia:

- `apps/plugin.c` (D-298: `plugin_set_silent_open_errors()`)
- `apps/plugin.h` (D-298: declaración)
- `apps/tagcache.c` (D-021/D-244/D-293: contador de trabajos de
  reconstrucción, descarte de temporal huérfano, fix de fuga de
  buffer en simulador, fix de `load_ramcache()` con base creciente,
  `commit()` con buffer temporal general)
- `apps/tagcache.h` (D-293: `tagcache_get_build_jobs_done()`,
  `tagcache_has_pending_temp()`, `tagcache_discard_pending_temp()`)
- `bootloader/ipod-s5l87xx.c` (D-064: arranque silencioso bajo
  `#ifdef IPOD_6G`, sin afectar `IPOD_NANO3G`)
- `firmware/export/config/ipod6g.h` (D-06x: `CONFIG_BACKLIGHT_FADING
  BACKLIGHT_FADING_SW_HW_REG`; D-061: fix de `USBPOWER_BTN_IGNORE`
  fuera de `#ifdef BOOTLOADER`)
- `lib/rbcodec/codecs/aiff.c` (tolerancia a variaciones de AIFF
  exportado por Music/iTunes)
- `tools/configure` (D-007: detección del GCC de Homebrew más
  reciente disponible en macOS, con fallback 16→15→14→13 — **no
  estaba documentado en la lista de 27 de Aura-Firmware**, hallado y
  añadido en la Fase Cero de este repositorio; ver
  `docs/DESVIACIONES.md` F0-1)
- `uisimulator/common/sim_tasks.c` (automatización de entrada +
  captura de pantalla headless para el simulador SDL; variables de
  entorno renombradas `METRO_SIM_*` en vez de `AURA_SIM_*`, ver más
  abajo)
- `utils/mks5lboot/Makefile` (backend libusb opcional en macOS)

(Rutas relativas a `firmware/rockbox/`.)

### Excepción: `uisimulator/common/sim_tasks.c` sí se modificó al portar

A diferencia de los otros 9, este archivo **no** se portó byte-idéntico:
las variables de entorno y funciones que Aura-Firmware nombró con el
prefijo `AURA_SIM_*` se renombraron a `METRO_SIM_*`
(`METRO_SIM_AUTODUMP_TICKS`, `METRO_SIM_AUTODUMP_QUIT`,
`METRO_SIM_BUTTONS`), porque son identificadores de proyecto, no un
aviso de modificación GPL. El mecanismo (inyección de botones +
autodump headless) es idéntico.

**F9 (2026-08-20, M-039):** agrega el token `USB_INSERT` a
`METRO_SIM_BUTTONS` — llama `sim_trigger_usb(true)` (la misma función
que ya dispara el menú interactivo del simulador) en vez de postear un
botón, para poder capturar `metro_screen_usb.c` de forma headless. No
existía en el mecanismo original de Aura-Firmware.

**R3-F8 (2026-08-20, M-069):** el sondeo del hilo del simulador pasa de
`HZ/10` a `HZ/50` mientras hay un volcado o botones pendientes. Con 10
ticks de resolución, una animación de 24 ticks (los 8 cuadros × 3 del
PUSH bajo `animations=all`) solo se puede muestrear dos veces y la
primera cae ya pasado el tercer cuadro — imposible capturar el arranque
de CONTINUUM, que es justo el criterio de "hecho" de esa fase. A 2
ticks hay ~12 muestras dentro de la misma animación. Mismo carácter que
la ampliación de `METRO_MAX_INJECT_BUTTONS` que este archivo ya traía:
herramienta de pruebas, solo compila en el simulador, sin ningún
impacto en el binario de hardware.

**M-101 (2026-09-04):** sufijo `+HOLD` en cualquier nombre de botón de
`METRO_SIM_BUTTONS` (p. ej. `SELECT+HOLD`). El inyector solo sabía hacer
press-then-release, así que **ningún gesto de botón sostenido de Metro**
(MENU mantenido = ir al hub, SELECT mantenido en el reproductor =
aleatorio, SELECT mantenido en "acerca de" = revelar la marca de agua de
pila) se podía verificar sin la ventana SDL interactiva. Con el sufijo la
secuencia posteada es `press → BUTTON_REPEAT → BUTTON_REL`, que es lo que
el driver del 6G produce cuando la pulsación pasa el umbral de repetición
— y por eso el `BUTTON_REL` final ya no casa con el mapeo corto (su
prebutton exige que el último botón haya sido el código a secas). Mismo
carácter que los tokens `USB_INSERT` (M-039) y `POWEROFF` que este archivo
ya traía: postear lo que el driver hubiera posteado. Herramienta de
pruebas, solo compila en el simulador.

**M-104 (2026-09-04):** token `HOLD` en `METRO_SIM_BUTTONS` — **conmuta**
el interruptor Hold simulado, la misma variable que la tecla `h` de la
ventana SDL (`firmware/target/hosted/sdl/button-sdl.c`,
`hold_button_state`). El Hold del 6G **no es un botón**: no se puede
postear, se lee por sondeo (`pmu_holdswitch_locked()` en el aparato, esa
variable en el simulador), así que sin este token la máquina de estados
del bloqueo por Hold y el ícono de candado de la barra solo se podían
verificar a mano en la ventana interactiva. Mismo carácter que
`USB_INSERT` (M-039) y `POWEROFF`: tocar directo lo que el driver
tocaría. Solo compila en el simulador.

## `apps/metro/` — código nuevo, no una modificación

Todo el árbol `firmware/rockbox/apps/metro/` es código **nuevo**,
escrito para este proyecto — no es una modificación de un archivo
preexistente de Rockbox. Cada archivo lleva su propia cabecera de
copyright GPL v2. Es la UI "Metro" (menús, navegación twist,
reproducción) que reemplaza `root_menu()`/la UI de menús estándar de
Rockbox — ver `DECISIONS.md` M-006.

## Otros archivos de Rockbox modificados

Esta sección se actualiza en cada fase de `PLAN_MAESTRO.md` §5 que
toque un archivo de Rockbox fuera de `apps/metro/` — ver el registro
vivo en `DECISIONS.md`.

### F1 (2026-08-20)

- `apps/main.c`: `root_menu()` → `metro_main()` como único punto de
  entrada de la UI (M-006, no retorna). `metro_apply_hygiene()`
  (M-019) se llama en los dos cuerpos de `init()` (simulador y target
  real), justo después de `settings_load()` y antes de
  `settings_apply(true)`/`settings_apply_skins()` — es el único punto
  donde puede correr sin que el backdrop Cabbie v2 stock ya se haya
  pintado sobre el LCD (ver `docs/DESVIACIONES.md` F1-1).
- `apps/SOURCES`: bloque agregado al final listando
  `metro/metro_main.c` y `metro/metro_screen_splash.c`.
- `apps/bitmaps/native/rockboxlogo.320x98x16.bmp`: reemplazado por el
  wordmark "metro" (Selawik Light, blanco sobre negro), mismo nombre
  de archivo y dimensiones exactas (320×98) que el original — la regla
  de `apps/bitmaps/bitmaps.make` que lo compila a `bm_rockboxlogo` vía
  `bmp2rb` no necesitó ningún cambio. Generado por
  `firmware/tools/gen_logo.py` desde
  `firmware/assets/fonts-src/Selawik-Light.ttf` (M-020).

### F9 (2026-08-20)

- `apps/gui/usb_screen.c` (M-088): tres bloques `#ifdef IPOD_6G`, marcados
  `Metro (M-088)` — `#include "metro/metro_screen_usb.h"`; en
  `usb_screens_draw()` la pantalla principal llama
  `metro_screen_usb_show()` en vez de pintar `bm_usblogo` (el logo
  genérico "USB"); y en el bucle de `handle_usb_events()` el sondeo pasa
  de `HZ/2` a `HZ/10` y llama `metro_screen_usb_tick()` en cada vuelta,
  para animar los puntos del indicador indeterminado. Sin cambio en la
  lógica USB/HID ni en el resto de targets. Todo lo que Metro dibuja ahí
  está embebido en el binario (`font_disable_all()` sigue corriendo
  antes, sin tocarse).
- `apps/gui/splash.c` (M-037): un solo gancho de una línea —
  `metro_splash_translate(splash_buf, sizeof(splash_buf))`, corre
  justo después de que `vsnprintf()` resuelve el mensaje (con
  cualquier argumento dinámico ya sustituido) y antes del ajuste de
  línea, que no se toca. Reescribe al wording de Metro (ES/EN) los
  mensajes conocidos que vienen del árbol de Rockbox que Metro no
  controla (tagcache, carga de playlist/plugin, apagado por batería
  baja) — mismo mecanismo que `aura_splash_translate()` de
  Aura-Firmware (D-055 en ese repo), sin copiar su código: la tabla de
  mensajes vive en `apps/metro/metro_splash_lang.c`, código nuevo de
  Metro. Un mensaje sin regla conocida se muestra tal cual, nunca se
  reemplaza por un genérico que esconda información de diagnóstico.

### R2-F4 (2026-08-19, M-059)

Puerto + restilado del plugin `mpegplayer` (video) al estilo Metro,
siguiendo el mismo mecanismo que Aura-Firmware ya probó primero
(D-304..D-309 de ese repositorio, consultado en solo lectura como guía
de mecanismo — nunca copiado archivo por archivo; el diseño propio de
Aura, como su barra "píldora" redondeada, no se portó, ver
`docs/DESVIACIONES.md` R2-3). Los 7 archivos, cada uno con su propio
comentario inline `Metro (M-059)` en el punto exacto del cambio:

- `apps/plugins/mpegplayer/mpeg_settings.h`: `SETTINGS_VERSION` 5→6;
  `enum mpeg_scale_mode_id` (ajustar/cubrir) y el campo
  `settings.scale_mode` nuevos; `MPEG_SETTING_ENABLE_START_MENU`/
  `MPEG_MENU_RESUME` eliminados (el menú de inicio interactivo ya no
  existe, ver `mpeg_settings.c`).
- `apps/plugins/mpegplayer/mpeg_settings.c`: reescritura completa del
  menú de ajustes — el bloque de `#define MPEG_START_TIME_*` por
  target (~400 líneas) y `get_start_time()`/`show_start_menu()`/
  `draw_slider()`/`display_thumb_image()`/`show_loading()`/
  `increment_time()`/`resume_options()` se eliminan (código muerto una
  vez removido el menú de inicio); `rb->do_menu()`/`rb->set_option()`/
  `rb->set_int_ex()` (widgets 100% nativos de Rockbox) reemplazados por
  `metro_menu_draw()`/`metro_menu_pick()`/`metro_menu_adjust_int()`
  (widget propio, geometría de `metro_draw_rows()`: pitch 28px, x=12,
  seleccionado en fg, resto en secundario, sin píldora); tabla
  bilingüe ES/EN propia del plugin (`metro_str()`, no puede incluir
  `metro_lang.c`); `mpeg_start_menu()` ahora resuelve directo
  (`MPEG_START_SEEK`), sin mostrar nunca el menú "Play from
  beginning/Resume/Set time/Settings/Quit".
- `apps/plugins/mpegplayer/mpegplayer.c`: `MPEG_TOGGLE_SCALE` (SELECT)
  nuevo en el keypad del iPod; `struct osd` gana
  `prog_trackcolor`/`accent`; `metro_load_personalization()` (nuevo)
  lee `/.rockbox/aura/aura.cfg` una vez en `osd_init()` (esquema de
  Metro: `theme`/`accent`/`language`, colores solo vía
  `metro_palette.h`); `draw_scrollbar_draw()` reescrita a barra plana
  de dos colores (pista terciaria + relleno acento), sin borde;
  `osd_refresh_status()` recolorea el ícono de estado al acento; 4
  strings de splash traducidos vía `metro_str()`.
- `apps/plugins/mpegplayer/stream_mgr.c`: 8 strings de splash de error
  (fallos de inicialización de hilos/memoria) traducidos vía
  `metro_str()` en vez de texto en inglés fijo.
- `apps/plugins/mpegplayer/video_out.h`: 2 declaraciones nuevas,
  `vo_update_scale_mode()`/`vo_toggle_scale_mode()`.
- `apps/plugins/mpegplayer/video_out_rockbox.c`: modo "cubrir" —
  `vo_draw_frame_cover()` (recorta+escala por muestreo nearest-neighbor
  sobre la memoria sobrante del arena de libmpeg2, reutilizando
  `stretch_image_plane()` ya existente) y `vo_recalc_rect()`; guarda
  `scale_mode_locked` en `vo_setup()` contra codificadores MPEG-2 de
  GOP corto que repiten la cabecera de secuencia durante la
  reproducción normal (bug real encontrado y documentado primero por
  Aura-Firmware, D-308).
- `apps/plugins/mpegplayer/mpegplayer.make`: agrega
  `-I$(APPSDIR)/metro` a `MPEGCFLAGS` para incluir `metro_palette.h`
  directo (header puro, sin `.c`, nada que enlazar).

Ver `DECISIONS.md` M-059 para el detalle completo de cada decisión de
diseño y el bug de memoria encontrado y corregido durante la
verificación (no presente en el port mecánico inicial de Aura).

### R2-F4, continuación (2026-08-19, M-060)

Rediseño real "Zune HD" del OSD de video, pedido explícitamente por el
dueño tras verificar M-059 en el simulador interactivo (el port
mecánico ya quitaba el menú nativo de Rockbox, pero el OSD en sí no se
parecía al reproductor del Zune). Todo el cambio vive en
`apps/plugins/mpegplayer/mpegplayer.c`, comentario inline `Metro
(M-060)`/`R2-F4 Zune redesign (M-060)` en cada punto:

- `struct osd` pierde el campo `icons` (ya no hay ícono bitmap); las 3
  externs `mpegplayer_status_icons_8/12/16x8x1` se quitan (los `.bmp`
  fuente en `apps/plugins/bitmaps/mono/` se dejan intactos, fuera de
  alcance).
- `osd_text_init()`: reescritura completa, de layout de dos filas
  (ícono+tiempos arriba, barra abajo) a una sola fila (ícono, tiempo
  transcurrido, barra, duración), usando `vo_rect_set_ext()` en vez del
  truco original de ancho-como-`.r`-luego-offset.
- `osd_refresh_background()`: el bisel elevado de 4 líneas de
  brillo/sombra se quita, un solo relleno plano.
- `draw_status_icon()`/`draw_tri_stepped()` (nuevas): ícono geométrico
  por `draw_fillrect()`, reemplaza el bitmap+sombra de
  `osd_refresh_status()`.
- `draw_scrollbar_draw()`: línea de 2px con "thumb" cuadrado de 4px en
  el borde de lo reproducido, en vez del bloque de altura completa
  sin punta.
- `osd_init()`: `osd.prog_trackcolor` pasa de `s_metro_tertiary` sólido
  a `draw_blendcolor(osd.bgcolor, MYLCD_WHITE, 71)` (~28% blanco);
  carga `metro-caption-14.fnt` vía `rb->font_load()`
  (`draw_setfont_osd()`, con reserva a `FONT_UI` si falla).
- `draw_oriented_mono_bitmap_part()` (la variante no-portrait) y
  `draw_hline()` se eliminan por quedar sin llamadores tras lo
  anterior.

Ver `DECISIONS.md` M-060 para el detalle de cada uno de los 5 cambios
de la maqueta aprobada por el dueño, incluida la razón por la que el
panel no puede ser realmente transparente sobre el video en vivo
(`docs/DESVIACIONES.md` R2-4).

### R2-F4, cierre (2026-08-19, M-061)

Menús del plugin reconstruidos con la anatomía real de página Metro y
volumen del OSD como barra de nivel, tras verificación del dueño en el
simulador interactivo (que además destapó que M-060 nunca había
llegado al simdisk -- `make` no instala plugins, ver
`docs/DESVIACIONES.md` R2-5). Comentarios inline `Metro (M-060 cont.)`/
`M-061`:

- `apps/plugins/mpegplayer/mpeg_settings.h`: 5 IDs de string nuevos
  (títulos de página en minúsculas: `MSTR_TITLE_VIDEO`/`_SETTINGS`/
  `_DISPLAY`/`_AUDIO`/`_BRIGHTNESS`); declaraciones
  `metro_font_caption()`/`metro_font_list()`/`metro_font_list_sel()`/
  `metro_font_display()`/`metro_font_title()`.
- `apps/plugins/mpegplayer/mpeg_settings.c`: `metro_page_chrome()`
  (nueva -- ceja caption + reloj + batería 18x9 replicando
  `metro_draw_header()`/`metro_draw_battery()` de `apps/metro/`, más el
  título de página en display-48 a (12,28));
  `metro_menu_draw()` ahora dibuja esa anatomía completa con filas
  desde y=84; `display_options()`/`audio_options()`/`mpeg_settings()`
  reescritas al patrón ciclar-en-el-lugar con valores visibles
  (los selectores de dos filas por valor se eliminaron);
  `metro_adjust_draw()` (brillo) con la misma anatomía y el valor en
  title-28/acento; strings de ceja en minúsculas.
- `apps/plugins/mpegplayer/mpegplayer.c`: carga de
  `metro-display-48.fnt`/`metro-title-28.fnt` (deduplicadas por ruta
  contra las de la app, `firmware/font.c`); `osd_refresh_volume()`
  reescrita a barra de nivel de 28px (pista 28% blanco + relleno
  acento, rango real de SOUND_VOLUME normalizado) en vez del texto
  "-NdB"; `osd_text_init()` reserva ancho fijo para esa barra.

Ver `DECISIONS.md` M-061.

### M-094 (2026-08-26): `__TIME__`/`__DATE__` fuera de los plugins SDL

Los juegos SDL incrustaban la hora de compilación, con lo que
`quake.rock` y `duke3d.rock` cambiaban en cada build y la
actualización selectiva de Aura Studio (contrato v11) arrastraba
~2,2 MB espurios entre releases. Comentarios inline `Metro (M-094)`:

- `apps/plugins/sdl/progs/quake/host.c`: `Con_Printf ("Exe: "__TIME__"
  "__DATE__"\n")` → `"Exe: rockbox build\n"`.
- `apps/plugins/sdl/progs/quake/host_cmd.c`: ídem.
- `apps/plugins/sdl/progs/duke3d/Engine/src/display.c`: el `%s` de
  "Compiled %s against SDL version…" recibe `"rockbox build"` en vez de
  `__DATE__`; el `#if (!defined __DATE__)` de respaldo se elimina por
  muerto.

Ver `DECISIONS.md` M-094.


### M-095/M-096 (2026-08-26): sin cambios a archivos de Rockbox

Contrato v15 (`/.aura/tagcache`, `/.aura/thumbs`) se implementa por
completo dentro de `apps/metro/`: la ruta de la base de datos se fija
en `global_settings.tagcache_db_path` desde `metro_apply_hygiene()`,
que `apps/main.c` ya llama (F1, M-019) entre `settings_load()` e
`init_tagcache()` — la ventana exacta que necesita. No se tocó
`apps/main.c` ni `apps/tagcache.c`. Ver `DECISIONS.md` M-095.

### M-107 (2026-09-04): pantalla de arranque del bootloader

- `bootloader/ipod-s5l87xx.c` (M-107): bloque `#ifdef IPOD_6G` nuevo con
  `draw_boot_screen()` — wordmark centrado más dos leyendas en
  `FONT_SYSFIXED` y gris `#999999` (`metro · arranque <rbversion>` y
  `Basado en Rockbox · GPL v2 · rockbox.org`) —, llamado desde `main()`
  justo después de `lcd_setfont(FONT_SYSFIXED)` y **antes** de
  `backlight_init()`, para que la luz encienda con la marca ya dibujada.
  Además, el encabezado del modo USB del bootloader pasa al mismo gris
  (las líneas de acción siguen en blanco: son instrucciones de
  recuperación). Comentarios inline `Metro (M-107)` en cada punto.
  **El literal RGB es una excepción documentada**: el bootloader no
  enlaza `apps/metro/`, que no existe en ese build, así que
  `metro_palette.h` no está disponible.
- `apps/bitmaps/native/bootwordmark.140x68x16.bmp` (M-107): asset nuevo,
  generado por `firmware/tools/gen_logo.py --bootloader-crop` — el
  recorte del MISMO wordmark que ya vive en
  `rockboxlogo.320x98x16.bmp`, ajustado a su caja de tinta para no meter
  un lienzo de 320×98 en la IRAM del bootloader.
- `apps/bitmaps/native/SOURCES` (M-107): lo lista bajo
  `#if defined(BOOTLOADER)` + `#if defined(IPOD_6G)`, para no cambiar
  ningún otro target de Rockbox que comparta ese archivo.
- `apps/main.c` (M-107): `show_logo_boot()` centra el wordmark también
  en **Y** y deja de escribir la línea "Ver. \<rbversion\>", ambas cosas
  bajo `#elif defined(IPOD_6G)`. El eje X ya se centraba; el Y
  estaba fijo en 10 px (valor original de Rockbox, pensado para
  pantallas más chicas y logos más angostos). No es estética: es lo que
  hace que el paso **bootloader → firmware no tenga salto**, porque la
  pantalla del bootloader centra el recorte del mismo wordmark en la
  misma pantalla. Y sin la línea de versión, la transición es la que
  describe el plan maestro §B.2: la marca se queda quieta y la pantalla
  se limpia (la versión del firmware vive en "acerca de", M-101; la del
  bootloader en su propia pantalla). Mismo criterio que D-051 de
  Aura-Firmware y D-050 de moonlit.aura. `version`/`ver_w` quedan sin
  lector bajo `IPOD_6G` y se marcan con `(void)` para no mover el
  cálculo: **el resto de los targets conservan su `y = 10`, su línea de
  versión y su código byte a byte**.

Ver `DECISIONS.md` M-107.

### M-101 (2026-09-04): pila del hilo principal 8 KB -> 12 KB

- `firmware/target/arm/s5l8702/app.lds` (M-101): la sección `.stack`
  pasa de `. += 0x2000` a `. += 0x3000`. Comentario inline
  `Metro (M-101)` en el punto exacto. La IRAM de core mide 48 KB
  (`IRAMSIZE`, `0xC000`); con el aumento `_fiqstackend` queda en
  `0xb530` — verificado en `firmware/build-ipod6g/rockbox.map` —, o sea
  2 768 B libres. El archivo es idéntico en las tres familias
  (Aura-Firmware, Metro-Aura, moonlit.aura) y el aumento se aplica a
  las tres en la misma ronda: la causa es común (el hilo de UI carga
  marcos que Rockbox nunca tuvo — decode JPEG y recorridos de tagcache
  bajo un solo lock) y Aura-Firmware ya se comió un `Stkov main` real
  con la cifra de 8 KB.
- `apps/plugins/mpegplayer/mpegplayer.make` (M-101): `OTHER_INC +=
  -I$(APPSDIR)/metro`, junto al `MPEGCFLAGS +=` que ya había puesto
  M-059. Comentario inline `Metro (M-101)`. La pasada de dependencias
  (`mkdepfile`, `tools/functions.make:57`) arma su línea de comandos con
  `PPCFLAGS` + `OTHER_INC`, **no** con `MPEGCFLAGS`: sin esta línea
  `metro_palette.h` no se resuelve ahí y el `-MG` lo convierte en un
  `$(BUILDDIR)/metro_palette.h` fantasma que ninguna regla sabe
  construir. Solo se manifiesta en un directorio de build cuyo
  `make.dep` se genere DESPUÉS de M-059 — por eso llevaba desde el
  2026-08-19 escondido: el `firmware/build-ipod6g/` del árbol de trabajo
  arrastra un `make.dep` anterior a esa fase. Encontrado al crear
  `firmware/build-ipod6g-stack/` desde cero para
  `firmware/tools/stack_report.py`. Sin efecto sobre el binario: solo
  cambia la pasada de dependencias.

- `apps/gui/skin_engine/skin_engine.c` (M-101, segundo addendum):
  `settings_apply_skins()` deja de cargar los skins — se elimina el
  `skins_initialised = true` y el bucle `skin_get_gwps()` que lo seguía;
  todo lo demás de la función (init de backdrops, recarga del ajuste de
  backdrop, aviso `THEME_STATUSBAR`) queda intacto. Comentario inline
  `Metro (M-101)` en el punto exacto. **Metro no usa el motor de skins**:
  dibuja su propia barra de estado (`metro_draw_header()`) y su propio
  "Ahora suena", ningún tema de Metro es un `.wps`/`.sbs`, y el
  `CLAUDE.md` de este repo lo prohíbe explícitamente para cualquier
  pantalla propia (M-006). Con `skins_initialised` en false,
  `skin_get_gwps()` sale de inmediato para `CUSTOM_STATUSBAR` — la única
  pantalla skinneable a la que Metro puede llegar — y con eso desaparece
  del hilo de UI el subárbol `skin_data_load` → `font_load_ex` →
  `glyph_cache_load` → apertura de archivo → ATA, que medido con
  `firmware/tools/stack_report.py` costaba **5 136 B** y era la cola del
  peor camino de pila. Mismo cambio, mismo archivo y misma razón que
  D-345 en Aura-Firmware y su equivalente en moonlit.aura: los tres
  árboles divergen de Rockbox base en los mismos puntos, para que una
  auditoría GPL o un merge futuro sea uno solo.
- `uisimulator/common/sim_tasks.c` (M-101): sufijo `+HOLD` en
  `METRO_SIM_BUTTONS` — ver la sección "Excepción" de este archivo, donde
  ya vive el registro de los cambios de este archivo de automatización.

Ver `DECISIONS.md` M-101.

### M-114 (2026-09-04): MAXUSERFONTS 12 -> 16 (cirílico por tramos)

- `firmware/export/font.h` (M-114): `MAXUSERFONTS` pasa de `12` a `16`.
  Comentario inline `Metro (M-114)` en el punto exacto. Cinco roles
  primarios (Selawik) + cuatro fuentes cirílicas (Inter, M-113 --
  `MFONT_DISPLAY` se queda sin la suya, cae a `MFONT_TITLE`) = 9
  fuentes que `metro_fonts_init()` carga ahora. Medido, no supuesto:
  con `MAXUSERFONTS` todavía en 12, las 9 cargaron las 9 sin un solo
  "failed to load" (verificado con el DEBUGF real en el simulador,
  ids 1-9, dos ranuras libres) -- a diferencia del hallazgo real que
  moonlit.aura documentó para su propio D-081 (ahí sí agotaba el
  presupuesto de 16 con sus 20 fuentes). Se sube de todos modos, a 16,
  por el mismo motivo preventivo que moonlit usó (dejar margen para el
  próximo rol o la próxima fuente aparte sin volver a tocar este
  número), no para arreglar una falla que esta medición no encontró.
  `.bss` del target (`firmware/build-ipod6g/rockbox.elf`, aislado --
  mismo commit, solo esta constante): **7 352 988 -> 7 353 052 B
  (+64 B)**, el costo esperado de 4 ranuras más en las tablas internas
  de `MAXFONTS` de `firmware/font.c` (`buflib_allocations[MAXFONTS]`,
  enteros de 4 B). Ver `DECISIONS.md` M-114.

### M-097 (2026-08-26)

- `apps/SOURCES`: bloque de F1 extendido con `metro/metro_master_art_format.c`,
  `metro/metro_master_art.c`, `metro/metro_master_art_builder.c` (la
  caché maestra compartida de imágenes en `/.aura/art`, contrato v16).
  Sin cambios a ningún otro archivo de Rockbox fuera de `apps/metro/`.
  Ver `DECISIONS.md` M-097.
