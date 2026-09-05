#!/usr/bin/env bash
# Genera archivos de audio sinteticos cortos en cada formato nativo que
# Metro debe reproducir (FLAC, MP3, AAC/m4a, ALAC, WAV, AIFF), con tags y
# una letra .lrc de muestra, para probar el reproductor en el simulador
# sin depender de musica real del usuario.
#
# Salida: firmware/test-media/ (no se versiona -- generado on-demand).
#
# Uso: firmware/tools/gen_test_media.sh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT_DIR="$ROOT_DIR/firmware/test-media"

mkdir -p "$OUT_DIR"

TITLE="Metro Test Tone"
ARTIST="Metro QA"
ALBUM="Fase 4 Fixtures"
DURATION=3
FREQ=440

gen() {
  local ext="$1"; shift
  local out="$OUT_DIR/metro-test.$ext"
  echo "==> Generando $out"
  ffmpeg -y -loglevel error \
    -f lavfi -i "sine=frequency=${FREQ}:duration=${DURATION}" \
    -metadata title="$TITLE" -metadata artist="$ARTIST" -metadata album="$ALBUM" \
    "$@" "$out"
}

gen flac -c:a flac
gen mp3  -c:a libmp3lame -b:a 128k
gen m4a  -c:a aac -b:a 128k
gen alac.m4a -c:a alac
gen wav  -c:a pcm_s16le
gen aiff -c:a pcm_s16be

# F5: solid-color cover art, via ffmpeg PNG -> sips JPEG, not ffmpeg's
# own mjpeg encoder straight to .jpg. -pix_fmt yuvj420p alone (D-030,
# DECISIONS.md) fixes the *chroma sampling factor* mismatch that used
# to corrupt these images outright; it does NOT fix a second, subtler
# ffmpeg-mjpeg-specific issue found verifying F5's Now Playing screen:
# Rockbox's JPEG decoder reads such a file "successfully" (ret>0,
# right dimensions) but drops chroma silently, rendering the album art
# tile as flat gray (RGB565 ~= the source color's luma) instead of the
# real color -- confirmed by decoding the exact same pixels through
# both PIL and sips instead of ffmpeg's mjpeg encoder, both correct.
# ffmpeg still generates the flat-color PNG (no chroma subsampling
# concern for a lossless format); sips does the actual JPEG encode.
gen_cover_jpg() {
  local color="$1" size="$2" out="$3"
  local tmp_dir tmp_png
  tmp_dir="$(mktemp -d)"
  tmp_png="$tmp_dir/cover.png"
  ffmpeg -y -loglevel error -f lavfi -i "color=c=${color}:s=${size}" \
    -frames:v 1 "$tmp_png"
  sips -s format jpeg "$tmp_png" --out "$out" > /dev/null
  rm -rf "$tmp_dir"
}

cat > "$OUT_DIR/metro-test.lrc" <<'EOF'
[ar:Metro QA]
[ti:Metro Test Tone]
[al:Fase 4 Fixtures]
[00:00.00]Instrumental
[00:01.00]Segundo uno
[00:02.00]Segundo dos
EOF

echo "==> Generando fixture SIN caratula (test-media/SinArte)"
# Carpeta propia sin cover.jpg: find_albumart no encuentra arte ni
# embebido ni de carpeta -> ejercita la imagen "Default" (nota gris
# sobre tile gris, D-112) en Cover Flow, el reproductor y Flip-and-Flow.
mkdir -p "$OUT_DIR/SinArte"
ffmpeg -y -loglevel error \
  -f lavfi -i "sine=frequency=330:duration=${DURATION}" \
  -metadata title="Pista sin arte" -metadata artist="Metro QA" \
  -metadata album="Album sin portada" \
  -c:a libmp3lame -b:a 128k "$OUT_DIR/SinArte/metro-test-noart.mp3"

echo "==> Generando $OUT_DIR/cover.jpg"
gen_cover_jpg "0x3366CC" "200x200" "$OUT_DIR/cover.jpg"

echo "==> Generando fixtures de Fotos (test-media/Photos)"
mkdir -p "$OUT_DIR/Photos"
ffmpeg -y -loglevel error -f lavfi -i "testsrc=size=320x240:rate=1" \
  -pix_fmt yuvj420p -frames:v 1 "$OUT_DIR/Photos/photo1.jpg"
ffmpeg -y -loglevel error -f lavfi -i "color=c=0x2244AA:s=320x240" \
  -pix_fmt yuvj420p -frames:v 1 "$OUT_DIR/Photos/photo2.jpg"
ffmpeg -y -loglevel error -f lavfi -i "smptebars=size=160x120:rate=1" \
  -frames:v 1 "$OUT_DIR/Photos/photo3.bmp"
ffmpeg -y -loglevel error -f lavfi -i "color=c=0xAA6622:s=100x100" \
  -frames:v 1 "$OUT_DIR/Photos/photo4_unsupported.png"

# F7: 4 fotos mas para ejercitar photo_categories.cfg (photo/image/ai)
# -- "sin categoria.jpg" queda deliberadamente fuera del indice: debe
# aparecer en "todas" pero en ninguna de las 3 filas filtradas
# (contrato D.2: sin entrada = sin categoria, no un error).
gen_cover_jpg "0x88AA44" "320x240" "$OUT_DIR/Photos/sunset.jpg"
gen_cover_jpg "0xCCCCCC" "320x240" "$OUT_DIR/Photos/diagram.jpg"
gen_cover_jpg "0xFF66CC" "320x240" "$OUT_DIR/Photos/dreamscape.jpg"
gen_cover_jpg "0x336699" "320x240" "$OUT_DIR/Photos/sin categoria.jpg"

# R2-F2/DD-9 "hecho": >= 12 fotos totales para que el pivot "todos" de
# la cuadricula (4 cols) abarque mas de una pantalla y se vea el asomo
# de la fila siguiente (misma logica que la lista, "asoma cortado").
# Repartidas entre las 3 categorias (no solo colores parejos, para que
# las capturas de la cuadricula muestren miniaturas distinguibles).
gen_cover_jpg "0xE07A5F" "320x240" "$OUT_DIR/Photos/mountain.jpg"
gen_cover_jpg "0x3D5A80" "320x240" "$OUT_DIR/Photos/ocean.jpg"
gen_cover_jpg "0x81B29A" "320x240" "$OUT_DIR/Photos/receipt.jpg"
gen_cover_jpg "0xF2CC8F" "320x240" "$OUT_DIR/Photos/whiteboard.jpg"
gen_cover_jpg "0x9B5DE5" "320x240" "$OUT_DIR/Photos/portrait-ai.jpg"
gen_cover_jpg "0xF15BB5" "320x240" "$OUT_DIR/Photos/landscape-ai.jpg"
gen_cover_jpg "0x00BBF9" "320x240" "$OUT_DIR/Photos/beach.jpg"
gen_cover_jpg "0xFEE440" "320x240" "$OUT_DIR/Photos/forest.jpg"

# R2-F3 "hecho": el visor propio (ajustar/cubrir) necesita fixtures con
# proporcion DISTINTA de 4:3 (320x240 y 640x480 son ambas 4:3 -- ajustar
# no dejaria franjas visibles en ninguna). 640x300 (mas ancha que 4:3)
# para ajustar con franjas arriba/abajo + cubrir recortando los lados;
# 200x200 (cuadrada, mas chica que la pantalla en las dos dimensiones)
# para ajustar con franjas a los lados + cubrir agrandando sin franjas.
# Con patron real (testsrc), no color plano, para que el recorte de
# "cubrir" sea visible a simple vista.
echo "==> Generando fixtures de proporcion para el visor (fit/cover, R2-F3)"
ffmpeg -y -loglevel error -f lavfi -i "testsrc=size=640x300:rate=1" \
  -pix_fmt yuvj420p -frames:v 1 "$OUT_DIR/Photos/landscape-large.jpg"
ffmpeg -y -loglevel error -f lavfi -i "testsrc=size=200x200:rate=1" \
  -pix_fmt yuvj420p -frames:v 1 "$OUT_DIR/Photos/small-photo.jpg"

echo "==> Generando fixtures de Video (test-media/Videos)"
mkdir -p "$OUT_DIR/Videos"
gen_video() {
  local name="$1" freq="$2"
  ffmpeg -y -loglevel error -f lavfi -i "testsrc=size=320x240:rate=15:duration=2" \
    -f lavfi -i "sine=frequency=${freq}:duration=2" \
    -c:v mpeg2video -q:v 5 -c:a mp2 "$OUT_DIR/Videos/$name.mpg"
}
gen_video "test" 440
gen_video "Feature Presentation" 300
gen_video "Season Premiere" 350
gen_video "Behind the Scenes" 500
# "Sin categoria" no entra al indice a proposito -- mismo criterio que
# "sin categoria.jpg" arriba.
gen_video "Sin categoria" 600

echo "==> Generando indices de categoria (test-media/aura)"
mkdir -p "$OUT_DIR/aura"
cat > "$OUT_DIR/aura/video_categories.cfg" <<'EOF'
# aura-video-categories v1
Feature Presentation.mpg: movie
Season Premiere.mpg: series
Behind the Scenes.mpg: clip
test.mpg: clip
EOF
cat > "$OUT_DIR/aura/photo_categories.cfg" <<'EOF'
# aura-photo-categories v1
sunset.jpg: photo
diagram.jpg: image
dreamscape.jpg: ai
photo1.jpg: photo
mountain.jpg: photo
ocean.jpg: photo
receipt.jpg: image
whiteboard.jpg: image
portrait-ai.jpg: ai
landscape-ai.jpg: ai
EOF

# F4: biblioteca musical de prueba para metro_music.c (proveedores sobre
# tagcache_search*) -- 2 artistas, 3 albumes, 12 pistas, 3 generos en el
# layout Artista/Album/archivo (mas dos pistas sueltas mas abajo para
# los otros dos layouts del contrato, ver el comentario junto a ellas).
# Cada pista lleva ALBUMARTIST igual a ARTIST (ejercita la ruta
# principal de metro_music_album_artist_label(): preferir
# tag_albumartist, con tag_artist como respaldo si una pista no lo
# trae -- ningun fixture aqui ejercita ese respaldo, ver el comentario
# [ESTIMADO] junto a esa funcion en metro_music.c). TRACKNUMBER fija el
# orden de disco de cada album (metro_music_songs_of_album() ordena por
# el, no alfabetico).
echo "==> Generando biblioteca musical de prueba (test-media/Music)"
MUSIC_DIR="$OUT_DIR/Music"
rm -rf "$MUSIC_DIR"

gen_track() {
  local artist="$1" album="$2" genre="$3" track="$4" title="$5" freq="$6"
  local dir="$MUSIC_DIR/$artist/$album"
  local out
  mkdir -p "$dir"
  out="$dir/$(printf '%02d' "$track") $title.mp3"
  ffmpeg -y -loglevel error \
    -f lavfi -i "sine=frequency=${freq}:duration=2" \
    -metadata title="$title" -metadata artist="$artist" \
    -metadata album_artist="$artist" -metadata album="$album" \
    -metadata genre="$genre" -metadata track="$track" \
    -c:a libmp3lame -b:a 96k "$out"
}

gen_track "Aura Test Combo" "First Light" "Electronic" 1 "Sunrise" 220
gen_track "Aura Test Combo" "First Light" "Electronic" 2 "Horizon" 247
gen_track "Aura Test Combo" "First Light" "Electronic" 3 "Glow" 262
gen_track "Aura Test Combo" "First Light" "Electronic" 4 "Daybreak" 294

# F5: cover.jpg de carpeta para el album que SI debe mostrar caratula
# real en Now Playing (find_albumart()); "Night Drive" se deja sin
# arte a proposito -- ejercita metro_draw_tile() (F5-nowplaying-noart.png).
gen_cover_jpg "0xCC7733" "300x300" "$MUSIC_DIR/Aura Test Combo/First Light/cover.jpg"

gen_track "Aura Test Combo" "Night Drive" "Synthwave" 1 "Overpass" 330
gen_track "Aura Test Combo" "Night Drive" "Synthwave" 2 "Neon Mile" 349
gen_track "Aura Test Combo" "Night Drive" "Synthwave" 3 "Rearview" 392
gen_track "Aura Test Combo" "Night Drive" "Synthwave" 4 "Static Coast" 440

gen_track "Wheel & Click" "Analog Dreams" "Ambient" 1 "Slow Turn" 165
gen_track "Wheel & Click" "Analog Dreams" "Ambient" 2 "Ferrite" 175
gen_track "Wheel & Click" "Analog Dreams" "Ambient" 3 "Idle Hum" 185
gen_track "Wheel & Click" "Analog Dreams" "Ambient" 4 "Wind Down" 196

# R2-F1/DD-2: near-white cover art, worst-case contrast fixture for
# Now Playing's semi-transparent text plate (METRO_NP_BG_ALPHA256,
# metro_screen_nowplaying.c) -- the palest fixture that ships, on
# purpose, to check the tertiary (album) text line stays legible over
# the palest cover a real user could plausibly have.
gen_cover_jpg "0xF2F2EC" "300x300" "$MUSIC_DIR/Wheel & Click/Analog Dreams/cover.jpg"

# COMPAT_STUDIO.md C8: tagcache escanea todo el disco sin importar la
# profundidad de carpetas (docs/contracts/library-layout-v1.md SS "/Music/",
# Aura-Firmware, "no depende del layout") -- estas dos pistas ejercitan
# los otros dos layouts que Studio puede elegir ademas del por defecto
# (Artista/Album/archivo, ya cubierto arriba): Album/archivo (sin
# carpeta de artista) y Artista/archivo (sin carpeta de album).
mkdir -p "$MUSIC_DIR/Flat Album Test"
ffmpeg -y -loglevel error -f lavfi -i "sine=frequency=210:duration=2" \
  -metadata title="Flat Song" -metadata artist="Flat Artist" \
  -metadata album_artist="Flat Artist" -metadata album="Flat Album Test" \
  -metadata genre="Electronic" -metadata track=1 \
  -c:a libmp3lame -b:a 96k "$MUSIC_DIR/Flat Album Test/01 Flat Song.mp3"

mkdir -p "$MUSIC_DIR/Flat Artist Test"
ffmpeg -y -loglevel error -f lavfi -i "sine=frequency=280:duration=2" \
  -metadata title="Loose Track" -metadata artist="Flat Artist Test" \
  -metadata album_artist="Flat Artist Test" -metadata album="Singles" \
  -metadata genre="Ambient" -metadata track=1 \
  -c:a libmp3lame -b:a 96k "$MUSIC_DIR/Flat Artist Test/01 Loose Track.mp3"

# R3-F3/DD-6: un artista mas, con ':' en el propio tag -- justo el caso
# que motivo el formato invertido "<archivo>: <artista>" de
# artist_images.cfg (B.1: el valor SI puede traer ':', la clave nunca).
mkdir -p "$MUSIC_DIR/Colon Artist Test"
ffmpeg -y -loglevel error -f lavfi -i "sine=frequency=305:duration=2" \
  -metadata title="Remix Cut" -metadata artist="DJ Twist: Remix Unit" \
  -metadata album_artist="DJ Twist: Remix Unit" -metadata album="Colon Test" \
  -metadata genre="Electronic" -metadata track=1 \
  -c:a libmp3lame -b:a 96k "$MUSIC_DIR/Colon Artist Test/01 Remix Cut.mp3"

echo "==> Generando fotos de artista de prueba (test-media/aura/artists)"
mkdir -p "$OUT_DIR/aura/artists"
# Metro QA y Aura Test Combo SI tienen foto; Wheel & Click deliberadamente
# NO (ejercita el tile de acento con inicial, DD-6); DJ Twist: Remix Unit
# prueba que un tag con ':' se resuelve por su valor completo, no solo
# hasta el primer ':'.
gen_cover_jpg "0x6B4EFF" "128x128" "$OUT_DIR/aura/artists/metro-qa.jpg"
gen_cover_jpg "0x2E8B57" "128x128" "$OUT_DIR/aura/artists/aura-test-combo.jpg"
gen_cover_jpg "0xE0A030" "128x128" "$OUT_DIR/aura/artists/dj-twist.jpg"
cat > "$OUT_DIR/aura/artist_images.cfg" <<'EOF'
# artist_images.cfg v1 (CONTRATO-firmware-studio.md SS D.3)
metro-qa.jpg: Metro QA
aura-test-combo.jpg: Aura Test Combo
dj-twist.jpg: DJ Twist: Remix Unit
EOF

# R4/FA-2 (M-075): residuales de macOS como fixtures PERMANENTES. Son
# sidecars AppleDouble reales (magic 0x00051607) que conservan la
# extension del archivo original -- exactamente el caso que se colaba
# por el filtro de sufijo. Se generan siempre para que cualquier
# captura futura de las cuadriculas siga demostrando que el filtro
# aguanta, no solo la de la fase que lo introdujo.
# R4/FA-7 (M-078): la CUARTA fila de la tabla de fondo -- artista SIN
# foto ("Wheel & Click" no esta en artist_images.cfg, a proposito desde
# R3-F3) y album SIN caratula, en una carpeta cuyo PADRE tampoco tiene
# cover.jpg (find_albumart mira el directorio del track y su padre). Sin
# esto no habia forma de llegar al caso "ninguna de las dos" y esa fila
# de la tabla quedaba sin verificar.
mkdir -p "$MUSIC_DIR/Wheel & Click/Sin Portada"
ffmpeg -y -loglevel error -f lavfi -i "sine=frequency=260:duration=2" \
  -metadata title="Sin Nada" -metadata artist="Wheel & Click" \
  -metadata album_artist="Wheel & Click" -metadata album="Sin Portada" \
  -metadata genre="Ambient" -metadata track=1 \
  -c:a libmp3lame -b:a 96k "$MUSIC_DIR/Wheel & Click/Sin Portada/01 Sin Nada.mp3"

# R4/FA-5a (M-076): contenido en espanol REAL, con inicial acentuada y
# enye. Cubre de forma permanente el camino de metro_lang_initial():
# sin esto, ninguna etiqueta del simulador empieza con un caracter
# multibyte y el bug de "cortar por el primer byte" vuelve a ser
# invisible. Sin carpeta de artista a proposito, para que el tile caiga
# al respaldo de acento + inicial y la "A" con acento se vea.
mkdir -p "$MUSIC_DIR/Angela Nu"
ffmpeg -y -loglevel error -f lavfi -i "sine=frequency=390:duration=2" \
  -metadata title="Canción Ñoña" -metadata artist="Ángela Ñu" \
  -metadata album_artist="Ángela Ñu" -metadata album="Éxitos" \
  -metadata genre="Latin" -metadata track=1 \
  -c:a libmp3lame -b:a 96k "$MUSIC_DIR/Angela Nu/01 Cancion Nona.mp3"

# M-111 (ronda "ajustes 2", Fase 3, plan maestro SS D.3): alemán real
# (Eszett + diéresis) y cirílico real (ruso), mismo criterio que el
# fixture de Ángela Ñu de arriba -- contenido REAL en la biblioteca,
# no solo cadenas de UI traducidas, para ejercitar
# metro_lang_collate()/metro_lang_initial()/metro_lang_upper() con
# datos que vienen de fuera del firmware (tags de Studio), no solo de
# metro_lang.c. El álbum cirílico también sirve como evidencia visual
# permanente del hueco de cobertura de glifos que documenta
# DECISIONS.md M-111 (Selawik no trae cirílico -- el título se ve como
# "??????" hasta que se resuelva con una fuente o una transliteración).
mkdir -p "$MUSIC_DIR/Gruen"
ffmpeg -y -loglevel error -f lavfi -i "sine=frequency=410:duration=2" \
  -metadata title="Straße der Größe" -metadata artist="Käthe Müller" \
  -metadata album_artist="Käthe Müller" -metadata album="Glückspilz" \
  -metadata genre="Schlager" -metadata track=1 \
  -c:a libmp3lame -b:a 96k "$MUSIC_DIR/Gruen/01 Strasse der Groesse.mp3"

# M-121 (aviso de Aura D-359): nombres INVENTADOS, no de un artista ni
# una obra reales -- estos fixtures salen en las capturas del README
# publico. Lo que este fixture prueba no es el nombre sino la ё
# (U+0451): su mayuscula Ё obliga al intercambio de byte lider en
# metro_lang_upper() (0xD1 -> 0xD0), que es el caso que ninguna otra
# cadena cirilica del catalogo ejercita. La carpeta va en ASCII por
# D-357, igual que la de abajo.
mkdir -p "$MUSIC_DIR/LangFixtureRU"
ffmpeg -y -loglevel error -f lavfi -i "sine=frequency=430:duration=2" \
  -metadata title="Утро в Москве" -metadata artist="Фёдор Тестов" \
  -metadata album_artist="Фёдор Тестов" -metadata album="Пробный Альбом" \
  -metadata genre="Classical" -metadata track=1 \
  -c:a libmp3lame -b:a 96k "$MUSIC_DIR/LangFixtureRU/01 Utro v Moskve.mp3"

# R5-F3 (M-083): una pista LARGA (20 s). Todo lo demás dura <= 3 s, y eso
# no alcanza para verificar nada temporizado en el reproductor: el nivel
# de volumen se queda 3 s y se desvanece durante 1 s más -- con las
# pistas cortas la canción se acababa antes que el fundido y la captura
# mostraba la pantalla de "nada sonando", no el fundido. Artista/álbum
# con acentos a propósito (ejercitan metro_lang_upper en la línea de
# artista en versalitas).
#
# M-115 (ronda "ajustes 2"): dos bugs reales encontrados juntos al
# investigar por qué el pivot "canciones" del simulador aparecía vacío
# pese a que la base de tagcache sí se construía.
#
# (El nombre de artista que se cita abajo es el que este fixture tenía
# hasta M-121, cuando se cambió por uno inventado -- ver el comentario
# junto al ffmpeg. La historia del bug se deja tal cual porque las rutas
# viejas son parte de lo que explica.)
#
# 1. Esta pista vivía en "$OUT_DIR/Cultura Profética/..." -- ANTES de
#    que "$MUSIC_DIR" existiera como variable, y sobre todo ANTES del
#    "rm -rf $MUSIC_DIR" (unas líneas arriba de este comentario, al
#    definir $MUSIC_DIR) que limpia el árbol antes de regenerar la
#    biblioteca de prueba. Estaba fuera de Music/ por completo -- nunca
#    se copiaba a simdisk/Music/ ni se escaneaba, en ninguna ronda
#    desde R5-F3 (M-083). Moverla aquí, DESPUÉS del `rm -rf`, es lo que
#    la deja sobrevivir: moverla a cualquier punto ANTES de esa línea
#    (incluido "$MUSIC_DIR/Cultura Profetica/...") la habría dejado
#    borrada de nuevo por el propio `rm -rf`, que es exactamente lo que
#    pasó en el primer intento de este mismo hallazgo.
# 2. Hallazgo de Aura-Firmware (D-357), aplicado por si acaso: la
#    CARPETA va en ASCII puro ("LangFixtureES") aunque el ARTISTA en
#    las etiquetas ID3 sí lleve acento ("Café Ficticio") --
#    son dos pruebas distintas (nombre de archivo vs. metadata) que
#    compartían sin querer el mismo string. APFS puede normalizar a
#    NFD un nombre de directorio no-ASCII al crearlo, y el escaneo del
#    simulador no siempre reconcilia esa forma con la NFC que espera.
#    El acento de la carpeta nunca fue parte de lo que este fixture
#    pretendía probar (eso es la metadata, sin tocar).
# M-121 (aviso de Aura D-359): nombre INVENTADO. Antes usaba una banda
# real; lo que el fixture prueba es el acento en la metadata (la E con
# tilde bajo metro_lang_upper()) y la duracion de 20 s, no quien la
# firma.
mkdir -p "$MUSIC_DIR/LangFixtureES/Muestra Larga"
ffmpeg -y -loglevel error \
  -f lavfi -i "sine=frequency=220:duration=20" \
  -metadata title="Un deseo" -metadata artist="Café Ficticio" \
  -metadata album="Muestra Larga" \
  -c:a libmp3lame -b:a 128k "$MUSIC_DIR/LangFixtureES/Muestra Larga/01 Un deseo.mp3"

echo "==> Generando residuales de macOS de prueba (AppleDouble)"
printf '\x00\x05\x16\x07appledouble de prueba' > "$OUT_DIR/Photos/._diagram.jpg"
printf '\x00\x05\x16\x07appledouble de prueba' > "$OUT_DIR/Photos/._sunset.jpg"
printf '\x00\x05\x16\x07appledouble de prueba' > "$OUT_DIR/Videos/._clip1.mpg"

echo "==> Generando ratings.cfg de prueba (test-media/aura/ratings.cfg)"
# R3-F5/DD-7: <ruta absoluta>: <rating 0-10>, calcado del formato real
# de Studio (CONTRATO-firmware-studio.md, import_ratings_from_studio()
# de Aura consultado read-only) -- tres pistas con distintos valores
# para ejercitar el mapeo x2 a estrellas (10->5, 6->3, 2->1), mas una
# ruta que ya no existe en la biblioteca (ejercita el "ignorado, no es
# error" de tagcache_find_index() devolviendo false).
cat > "$OUT_DIR/aura/ratings.cfg" <<'EOF'
# ratings.cfg v1 (CONTRATO-firmware-studio.md)
/Music/Aura Test Combo/First Light/01 Sunrise.mp3: 10
/Music/Aura Test Combo/Night Drive/01 Overpass.mp3: 6
/Music/Wheel & Click/Analog Dreams/02 Ferrite.mp3: 2
/Music/No Existe/archivo fantasma.mp3: 8
EOF

echo "==> Generando lista de prueba (test-media/Playlists)"
mkdir -p "$OUT_DIR/Playlists"
cat > "$OUT_DIR/Playlists/QA Favorites.m3u8" <<'EOF'
/Music/Aura Test Combo/First Light/01 Sunrise.mp3
/Music/Aura Test Combo/Night Drive/02 Neon Mile.mp3
/Music/Wheel & Click/Analog Dreams/03 Idle Hum.mp3
EOF

SIMDISK="$ROOT_DIR/firmware/build-sim/simdisk"
if [[ -d "$SIMDISK" ]]; then
  echo "==> Instalando fixtures en $SIMDISK"
  mkdir -p "$SIMDISK/Photos" "$SIMDISK/Videos" "$SIMDISK/.rockbox/aura"
  cp "$OUT_DIR"/Photos/* "$SIMDISK/Photos/"
  cp "$OUT_DIR"/Videos/*.mpg "$SIMDISK/Videos/"
  cp "$OUT_DIR"/aura/video_categories.cfg "$SIMDISK/.rockbox/aura/"
  cp "$OUT_DIR"/aura/photo_categories.cfg "$SIMDISK/.rockbox/aura/"
  # Fixtures de MUSICA: solo bajo peticion explicita
  # (METRO_INSTALL_MUSIC_FIXTURES=1), para no ensuciar el simulador con
  # una biblioteca de prueba cuando alguien solo necesita probar
  # Videos/Fotos, o cuando simdisk/Music ya apunta (symlink manual del
  # dueno del checkout) a una biblioteca real.
  if [[ "${METRO_INSTALL_MUSIC_FIXTURES:-0}" == "1" ]]; then
    mkdir -p "$SIMDISK/Music" "$SIMDISK/Playlists"
    cp -R "$MUSIC_DIR"/. "$SIMDISK/Music/"
    cp "$OUT_DIR/Playlists"/*.m3u8 "$SIMDISK/Playlists/"
    # R4/FA-2: el mismo caso, pero para playlists -- `._X.m3u8` termina
    # en .m3u8 y aparecia como una lista fantasma.
    printf '\x00\x05\x16\x07appledouble de prueba' > "$SIMDISK/Playlists/._QA Favorites.m3u8"
    cp "$OUT_DIR"/metro-test.* "$SIMDISK/Music/"
    cp "$OUT_DIR"/cover.jpg "$SIMDISK/Music/"
    # En la RAIZ del disco, no bajo Music/: find_albumart tambien busca
    # cover.jpg en el directorio padre, y Music/cover.jpg "vestiria" al
    # album que precisamente debe quedar sin arte.
    mkdir -p "$SIMDISK/SinArte"
    cp "$OUT_DIR"/SinArte/*.mp3 "$SIMDISK/SinArte/"
    # R3-F3/DD-6: solo tiene sentido junto con la biblioteca musical
    # (los tags de artista tienen que existir para que haya algo que
    # emparejar) -- mismo gate que Music/Playlists arriba.
    mkdir -p "$SIMDISK/.rockbox/aura/artists"
    cp "$OUT_DIR"/aura/artist_images.cfg "$SIMDISK/.rockbox/aura/"
    cp "$OUT_DIR"/aura/artists/*.jpg "$SIMDISK/.rockbox/aura/artists/"
    # R3-F5/DD-7: mismo gate -- las rutas de ratings.cfg apuntan a
    # pistas de esta misma biblioteca de prueba.
    cp "$OUT_DIR"/aura/ratings.cfg "$SIMDISK/.rockbox/aura/"
  fi
fi

echo "==> Listo: $OUT_DIR"
