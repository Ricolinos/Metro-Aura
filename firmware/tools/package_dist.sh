#!/usr/bin/env bash
# Empaqueta los artefactos distribuibles del firmware en firmware/dist/.
# No se versionan (ver firmware/dist/README.md) -- este script los
# reproduce a partir del código fuente, para publicarlos como GitHub
# Release.
#
# Uso:
#   firmware/tools/package_dist.sh
#   firmware/tools/package_dist.sh --release-tag v0.2.0
#
# --release-tag <tag>: marca este empaquetado como un Release real --
# escribe el tag exacto en .rockbox/aura/version.txt (dentro de
# rockbox.zip), la única forma en que Aura Studio puede saber qué
# versión de Metro tiene instalada un dispositivo (mismo contrato que
# Aura-Firmware's package_dist.sh, D-297/M-004). Sin este flag (build
# de desarrollo) el archivo no se escribe -- su ausencia es una señal
# válida en sí misma. Requiere el árbol git limpio (aborta si no).
#
# Requiere: el toolchain ARM instalado en firmware/toolchain/ (ver
# firmware/tools/build_toolchain.sh) -- o RBDEV_TOOLCHAIN apuntando a
# uno externo, igual que build_target.sh (DECISIONS.md M-002).
#
# A diferencia de Aura-Firmware's package_dist.sh (que sirve de
# referencia para este script -- ver DECISIONS.md), Metro-Aura no
# tiene sistema de temas/iconos instalables ni pipeline design-system/:
# M-018 fija toda la iconografía compilada en el binario, nunca leída
# de disco. Nada de AuraPalette.swift/theme-format-v1.json/
# aura-theme-default.zip aquí -- no hay a qué apuntarían.
#
# Produce:
#   firmware/dist/rockbox.ipod             -- binario del firmware
#   firmware/dist/rockbox.zip              -- árbol .rockbox/ completo
#                                              ("make zip": códecs,
#                                              plugins/rocks,
#                                              viewers.config, codepages,
#                                              langs -- con las fuentes
#                                              de Metro encima)
#   firmware/dist/bootloader-ipod6g.ipod   -- bootloader dual-boot (si
#                                              ya está compilado --
#                                              build_target.sh lo hace
#                                              por defecto, a diferencia
#                                              de Aura-Firmware esto no
#                                              necesita un paso manual)
#   firmware/dist/mks5lboot                -- herramienta de flasheo DFU
#   firmware/dist/MODIFICATIONS.md         -- listado GPL §2a, para el Release
#   firmware/dist/THIRD-PARTY-NOTICES.txt  -- Selawik (SIL OFL 1.1) + Fluent Icons (MIT)
#   firmware/dist/checksums.txt            -- SHA-256 de todo lo de arriba

set -euo pipefail

RELEASE_TAG=""
if [[ "${1:-}" == "--release-tag" ]]; then
  RELEASE_TAG="${2:-}"
  [[ -n "$RELEASE_TAG" ]] || { echo "ERROR: --release-tag requiere un valor (ej. v0.2.0)" >&2; exit 1; }
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC_DIR="$ROOT_DIR/firmware/rockbox"
BUILD_DIR="$ROOT_DIR/firmware/build-ipod6g"
BOOT_BUILD_DIR="$ROOT_DIR/firmware/build-ipod6g-boot"
DIST_DIR="$ROOT_DIR/firmware/dist"
TC_BIN="${RBDEV_TOOLCHAIN:-$ROOT_DIR/firmware/toolchain/bin}"
# Metro (M-108): absoluto, por el mismo motivo que build_target.sh --
# este script tambien hace `cd` antes de usarlo (make zip, mks5lboot).
if [[ -d "$TC_BIN" ]]; then
  TC_BIN="$(cd "$TC_BIN" && pwd)"
fi

if [[ ! -d "$TC_BIN" ]]; then
  echo "ERROR: no se encontró el toolchain en $TC_BIN" >&2
  echo "       corre firmware/tools/build_toolchain.sh primero," >&2
  echo "       o exporta RBDEV_TOOLCHAIN=<ruta a un toolchain existente>." >&2
  exit 1
fi

# DECISIONS.md M-050: tools/version.sh calcula mal el estado "sucio"
# en este repo (compara con GIT_WORK_TREE apuntando a firmware/rockbox/,
# no a la raíz real del repo git) -- lo calculamos bien aquí mismo y
# se lo pasamos a build_target.sh como VERSION= en vez de dejar que
# Rockbox lo intente por su cuenta.
GIT_HASH="$(git -C "$ROOT_DIR" rev-parse --short=10 HEAD)"
GIT_STATUS="$(git -C "$ROOT_DIR" status --porcelain)"

# R2-F1/DD-6 (M-056): mismo requisito que Aura-Firmware's package_dist.sh
# (D-297) -- un --release-tag construido sobre un árbol sucio produce
# un rockbox.ipod cuyo rockbox-info.txt queda marcado con una "M", no
# bit-provable contra el commit del tag. En desarrollo (sin el flag)
# no bloquea -- es normal estar iterando.
if [[ -n "$RELEASE_TAG" ]] && [[ -n "$GIT_STATUS" ]]; then
  echo "ERROR: hay cambios sin commitear -- un --release-tag necesita el árbol limpio" >&2
  echo "  (git status --short en $ROOT_DIR)" >&2
  exit 1
fi

if [[ -n "$GIT_STATUS" ]]; then
  echo "ADVERTENCIA: hay cambios sin commitear -- el release incluirá una 'M' real en la versión" >&2
  GIT_DIRTY="M"
else
  GIT_DIRTY=""
fi
export VERSION="${GIT_HASH}${GIT_DIRTY}-$(date -u +%y%m%d)"
echo "==> Versión: $VERSION"

echo "==> Compilando firmware + bootloader (build_target.sh)"
# Metro (M-108, D-348): con --release-tag, build_target.sh borra y
# reconfigura cada directorio de build antes de compilar
# (BUILD_TARGET_CLEAN=1) -- un release tiene que ser reproducible byte a
# byte (contrato v11, actualización selectiva por CRC32 de Studio), y un
# directorio de build reutilizado no lo garantiza aunque `make dep`
# (dentro de build_target.sh) regenere la base de dependencias. Sin el
# flag (build de desarrollo) solo corre `make dep`, que ya es lo que
# hace falta para que el binario no dependa de cuándo se creó el
# directorio.
if [[ -n "$RELEASE_TAG" ]]; then
  export BUILD_TARGET_CLEAN=1
fi
"$ROOT_DIR/firmware/tools/build_target.sh"

# M-101 (portado de Aura-Firmware, ver DECISIONS.md): la pila del hilo
# principal no da ningún aviso al compilar -- se manifiesta como un
# `Stkov main` en el iPod. stack_report.py mide el DESENSAMBLADO del
# binario recién enlazado (no recompila con -fstack-usage: eso duplica
# marcos que gcc integra por inlining) y falla si algún marco de
# apps/metro/ pasa de 1 KB o si el peor camino estático desde main() no
# cabe en el 75 % de la pila. Corre ANTES de empaquetar a propósito: un
# paquete que no pasa este reporte no se publica.
echo "==> Reporte estático de pila (stack_report.py)"
"$ROOT_DIR/firmware/tools/stack_report.py"

# M-111 (ronda "ajustes 2", Fase 3, portado de moonlit D-066): las
# fuentes .fnt son un rango DENSO -- una entrada de tabla por cada
# codepoint entre firstchar y firstchar+size, exista glifo o no. Un
# idioma cuyas cadenas piden un codepoint sin glifo real no falla al
# compilar (cae en `defaultchar`, silenciosamente) -- este chequeo es
# lo único que lo hubiera atrapado. Corre antes de empaquetar, mismo
# criterio que stack_report.py: un paquete que no pasa esto no se
# publica.
#
# M-114 (Fase 5): --known-incomplete ru se retira -- el hueco que
# M-111/M-112 documentaron (Selawik sin glifos cirilicos) se cerro con
# las fuentes Inter (M-113) y el dibujo por tramos (M-114,
# metro_textseg.c portado de moonlit D-074/D-081). Si "ru" vuelve a
# faltar cobertura de aqui en adelante, es una regresion real y este
# gate debe volver a bloquear el paquete, no tolerarla en silencio.
echo "==> Cobertura de glifos (check_fonts.py --coverage)"
python3 "$ROOT_DIR/firmware/tools/check_fonts.py" --coverage \
  --fonts "$ROOT_DIR/firmware/assets/fonts" \
  --lang "$ROOT_DIR/firmware/rockbox/apps/metro/metro_lang.c"

echo "==> Empaquetando el árbol .rockbox/ real (make zip: códecs, rocks, viewers.config, codepages, langs)"
(cd "$BUILD_DIR" && PATH="$TC_BIN:$PATH" make zip ${VERSION:+VERSION="$VERSION"})

echo "==> Compilando mks5lboot"
MKS5LBOOT_DIR="$SRC_DIR/utils/mks5lboot"
(cd "$MKS5LBOOT_DIR" && make)

mkdir -p "$DIST_DIR"

echo "==> Copiando rockbox.ipod"
cp "$BUILD_DIR/rockbox.ipod" "$DIST_DIR/rockbox.ipod"

echo "==> Copiando mks5lboot"
cp "$MKS5LBOOT_DIR/mks5lboot" "$DIST_DIR/mks5lboot"

if [[ -f "$BOOT_BUILD_DIR/bootloader-ipod6g.ipod" ]]; then
  echo "==> Copiando bootloader-ipod6g.ipod"
  cp "$BOOT_BUILD_DIR/bootloader-ipod6g.ipod" "$DIST_DIR/bootloader-ipod6g.ipod"
else
  echo "ADVERTENCIA: $BOOT_BUILD_DIR/bootloader-ipod6g.ipod no existe -- build_target.sh debería haberlo generado. checksums.txt saldrá sin él." >&2
fi

echo "==> Armando rockbox.zip (.rockbox/ completo)"
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/.rockbox"
# Base real -- todo lo que "make zip" acaba de producir (codecs/,
# rocks/, viewers.config, codepages/, langs/, themes/, wps/, eqs/,
# recpresets/, debug/, backdrops/, rockbox-info.txt, rockbox.ipod) --
# y ENCIMA las fuentes de Metro (metro_fonts.c las carga por nombre
# fijo, así que tienen que existir en el disco exactamente con esos
# nombres, igual que hace build_sim.sh con el simdisk).
unzip -q "$BUILD_DIR/rockbox.zip" -d "$STAGE"
mkdir -p "$STAGE/.rockbox/fonts"
cp "$ROOT_DIR"/firmware/assets/fonts/*.fnt "$STAGE/.rockbox/fonts/"

# Centinelas -- si "make zip" alguna vez deja de producir alguno de
# estos (cambio de layout de Rockbox upstream, target mal configurado,
# etc.), mejor abortar aquí con un mensaje claro que publicar un
# rockbox.zip mudo (sin video, sin audio en una instalación desde
# cero) -- misma lección que Aura-Firmware documentó de la manera
# difícil (D-297 en ese repositorio) y que este script hereda a
# propósito en vez de repetir el mismo descuido.
SENTINELS=(
  ".rockbox/rocks/viewers/mpegplayer.rock"
  ".rockbox/codecs/mpa.codec"
  ".rockbox/codecs/flac.codec"
  ".rockbox/codecs/aac.codec"
  ".rockbox/codecs/alac.codec"
  ".rockbox/viewers.config"
  ".rockbox/fonts/metro-display-48.fnt"
  ".rockbox/fonts/metro-list-20.fnt"
  ".rockbox/rockbox.ipod"
)
for sentinel in "${SENTINELS[@]}"; do
  if [[ ! -e "$STAGE/$sentinel" ]]; then
    echo "ERROR: falta $sentinel en el árbol armado -- rockbox.zip saldría incompleto. Revisa 'make zip' en $BUILD_DIR." >&2
    exit 1
  fi
done
echo "==> Centinelas verificados: $(find "$STAGE/.rockbox" -type f | wc -l | tr -d ' ') archivos en el árbol"

# version.txt (R2-F1/DD-6, M-056): solo se escribe si este empaquetado
# corresponde a un Release real (--release-tag) -- mismo contrato que
# Aura-Firmware. Aura Studio's AuraUpdateChecker lo lee para saber la
# versión instalada sin depender del hash embebido en rockbox-info.txt.
if [[ -n "$RELEASE_TAG" ]]; then
  echo "==> Escribiendo .rockbox/aura/version.txt ($RELEASE_TAG)"
  mkdir -p "$STAGE/.rockbox/aura"
  echo "$RELEASE_TAG" > "$STAGE/.rockbox/aura/version.txt"
fi

# Metro (M-108, D-075 de moonlit): `zip -r` AGREGA sobre un zip
# existente, no lo reemplaza -- un rockbox.zip viejo de un
# --release-tag anterior en este mismo firmware/dist/ dejaba
# sobrevivir su .rockbox/aura/version.txt (u otro archivo que ya no
# está en STAGE) en el paquete de una corrida sin el flag. `rm -f`
# antes de armar el zip para que el archivo salga completo y solo con
# lo que STAGE tiene ahora.
rm -f "$DIST_DIR/rockbox.zip"
(cd "$STAGE" && zip -qr "$DIST_DIR/rockbox.zip" .rockbox)

echo "==> Copiando MODIFICATIONS.md (asset del Release, cumplimiento GPL §2a)"
cp "$ROOT_DIR/MODIFICATIONS.md" "$DIST_DIR/MODIFICATIONS.md"

echo "==> Generando THIRD-PARTY-NOTICES.txt (Selawik + Inter + Fluent Icons, asset del Release)"
{
  echo "Metro-Aura -- avisos de terceros"
  echo "================================="
  echo
  echo "El wordmark \"metro\" y la tipografía de la interfaz usan Selawik"
  echo "(Microsoft, SIL Open Font License 1.1) -- ver DECISIONS.md M-020:"
  echo "fuente de sistema publicada libremente, no un logotipo ni una"
  echo "marca de Zune/Windows Phone. El texto completo de la licencia"
  echo "sigue abajo."
  echo
  echo "Los glifos cirílicos (ruso) usan Inter (The Inter Project"
  echo "Authors, SIL Open Font License 1.1) -- ver DECISIONS.md M-112:"
  echo "Selawik no trae cirílico, Inter lo cubre y es del mismo estilo"
  echo "humanista. El texto completo de la licencia sigue abajo."
  echo
  echo "Los iconos de la interfaz provienen de Fluent System Icons"
  echo "(Microsoft, MIT) -- ver DECISIONS.md M-077. Se redistribuyen"
  echo "rasterizados a mapas de bits monocromos de 16x16 dentro del"
  echo "binario (apps/metro/metro_icons_table.c, generado por"
  echo "firmware/tools/gen_icons.py); los SVG originales estan en"
  echo "firmware/assets/icons/. La licencia MIT exige conservar este"
  echo "aviso, y sigue abajo completo."
  echo
  echo "-- Selawik (tipografía) ---------------------------------------------"
  echo
  cat "$ROOT_DIR/firmware/assets/fonts-src/LICENSE-Selawik.txt"
  echo
  echo "-- Inter (tipografía, glifos cirílicos) ------------------------------"
  echo
  cat "$ROOT_DIR/firmware/assets/fonts-src/LICENSE-Inter.txt"
  echo
  echo "-- Fluent System Icons (iconografía) --------------------------------"
  echo
  cat "$ROOT_DIR/firmware/assets/icons/LICENSE-fluent-system-icons.txt"
} > "$DIST_DIR/THIRD-PARTY-NOTICES.txt"

echo "==> Generando checksums.txt"
(
  cd "$DIST_DIR"
  files=(rockbox.zip rockbox.ipod mks5lboot)
  [[ -f bootloader-ipod6g.ipod ]] && files+=(bootloader-ipod6g.ipod)
  shasum -a 256 "${files[@]}" > checksums.txt
)

echo "==> Listo: $DIST_DIR"
ls -la "$DIST_DIR"
