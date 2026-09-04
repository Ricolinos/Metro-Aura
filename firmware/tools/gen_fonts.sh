#!/usr/bin/env bash
# Convierte las fuentes Selawik vendoreadas a .fnt de Rockbox (formato
# RB12) con tools/convttf, un rol/tamaño a la vez -- ver DECISIONS.md
# M-010.
#
# Requiere tools/convttf compilado (requiere freetype2 vía pkg-config,
# `brew install freetype pkg-config`); se compila aquí mismo si falta.
#
# Uso:
#   firmware/tools/gen_fonts.sh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC_DIR="$ROOT_DIR/firmware/rockbox"
FONTS_SRC="$ROOT_DIR/firmware/assets/fonts-src"
FONTS_OUT="$ROOT_DIR/firmware/assets/fonts"
CONVTTF="$SRC_DIR/tools/convttf"

if [[ ! -x "$CONVTTF" ]]; then
  echo "==> Compilando tools/convttf"
  ( cd "$SRC_DIR/tools" && \
    cc -lm -std=c99 -O2 -Wall -g convttf.c -o convttf \
      $(pkg-config --cflags --libs freetype2) )
fi

mkdir -p "$FONTS_OUT"

# rol:archivo-fuente:tamaño-px -- ver DECISIONS.md M-010
ROLES=(
  "display:Selawik-Light.ttf:48:1"
  "title:Selawik-Light.ttf:28:1"
  "list:Selawik-Regular.ttf:20:1"
  "listsel:Selawik-Semibold.ttf:20:1"
  "caption:Selawik-Regular.ttf:14:0"
)

# Rango de caracteres: latin basico + Latin-1 Supplement + Latin
# Extended-A (0x20-0x17F, es/en/fr/de/it -- acentos, eñe, la Eszett
# alemana ß y la ligadura œ francesa ya caen dentro de ese rango) +
# Cyrillic (0x400-0x4FF, ruso) -- M-111 (ronda "ajustes 2", Fase 3,
# plan maestro SS D.3), sin cargar el conjunto completo de simbolos de
# Selawik.
#
# Sin "-x" (trim horizontal): convttf recorta hasta 2px por lado de
# TODO glifo "casi vacio", incluido el espacio (0x20), que a 20px pasa
# de ~5px a ~1px de ancho -- el texto se ve sin espacios. Aura-Firmware
# tampoco lo usa (design-system/generate.py: solo "-p <size>"). Ver
# DECISIONS.md M-028.
# M-111: DECIMAL, no hex -- tools/convttf.c parsea -s/-l/-D con
# atoi()/atol(), que no entienden el prefijo "0x" (se detienen en la
# 'x' y devuelven 0). Con `-l 0` convttf cae en su propio default
# ("ultimo glifo del font", codigo `if (limit_char == 0) limit_char =
# max_char`) y con `-s 0` arranca en el primer glifo que Selawik
# define (U+000D, no el espacio) -- asi que este script SIEMPRE genero
# fuentes con el rango COMPLETO nativo de Selawik (hasta U+2122, ~8500
# entradas de tabla) sin que "0x20"/"0x17F" tuvieran ningun efecto,
# desde M-010. Se encontro al verificar por que el vector Cyrillic no
# cambiaba el tamano del .fnt un byte -- ver DECISIONS.md M-111.
START=32   # 0x20, espacio
LIMIT=1279 # 0x4FF, cierra el bloque Cyrillic (ruso)
DEFAULT=63 # 0x3F, '?'

# Cuarto campo = "-c N", separación extra entre glifos (R5-F2, M-082).
# Rockbox no tiene kerning por pares, así que es lo único ajustable: a
# 48px Light la "ll" de "pantalla" se fundía en un solo trazo (bearings
# laterales de Selawik que redondean a 0 al rasterizar), y 1px lo separa
# sin abrir el texto de más. Fracciones como 0.5 no sirven: el avance se
# redondea a píxel entero y el medio píxel se pierde. La caption de 14px
# se deja en 0 -- no hay queja ahí y +1px por glifo es proporcionalmente
# mucho a ese tamaño.
for entry in "${ROLES[@]}"; do
  IFS=':' read -r role srcfont size spacing <<< "$entry"
  in="$FONTS_SRC/$srcfont"
  out="$FONTS_OUT/metro-$role-$size.fnt"
  echo "==> $role: $srcfont @ ${size}px, -c ${spacing} -> $(basename "$out")"
  "$CONVTTF" -p "$size" -s "$START" -l "$LIMIT" -D "$DEFAULT" \
    -c "$spacing" -o "$out" "$in"
done

echo "==> Listo: $FONTS_OUT"
ls -la "$FONTS_OUT"
