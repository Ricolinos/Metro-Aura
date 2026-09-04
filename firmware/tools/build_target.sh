#!/usr/bin/env bash
# Compila el firmware y el bootloader para el target real ipod6g.
# Requiere el toolchain ARM ya compilado (firmware/tools/build_toolchain.sh).
#
# Uso:
#   firmware/tools/build_target.sh              # firmware + bootloader
#   firmware/tools/build_target.sh --firmware    # solo firmware
#   firmware/tools/build_target.sh --bootloader  # solo bootloader
#
# RBDEV_TOOLCHAIN=<ruta a bin/> usa un toolchain externo en vez del de
# este repo (atajo de desarrollo, nunca por defecto — DECISIONS.md M-002).
#
# VERSION=<string> fuerza la cadena de versión (rockbox-info.txt/
# rbversion.h) en vez de que Rockbox la calcule con tools/version.sh --
# ese script asume que la ruta de código fuente ES la raíz del repo
# git (compara con GIT_WORK_TREE apuntando ahí); en Metro-Aura el repo
# real es la carpeta padre de firmware/rockbox/, así que esa
# comparación falla estructuralmente y siempre marca "M" (árbol
# modificado), incluso con todo comiteado -- DECISIONS.md M-050.
# package_dist.sh calcula esta variable correctamente antes de
# invocar este script para un release; en desarrollo normal, se deja
# sin usar (la "M" de más no afecta nada funcional).

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC_DIR="$ROOT_DIR/firmware/rockbox"
TC_BIN="${RBDEV_TOOLCHAIN:-$ROOT_DIR/firmware/toolchain/bin}"

# Metro (M-108, aviso de moonlit D-062): TC_BIN se vuelve ABSOLUTO aqui.
# build_one() hace `cd` al directorio de build antes de usar
# PATH="$TC_BIN:$PATH", asi que un RBDEV_TOOLCHAIN relativo -- que es
# justo como lo escriben los planes de la ronda
# ("../Metro-Aura/firmware/toolchain/bin") -- se resolvia contra el
# directorio equivocado y configure/make no encontraban
# arm-elf-eabi-gcc. No se notaba en un build incremental porque el
# Makefile ya generado lleva las rutas absolutas del configure anterior;
# solo rompia el build LIMPIO, que es del que tiene que salir el
# paquete.
if [[ -d "$TC_BIN" ]]; then
  TC_BIN="$(cd "$TC_BIN" && pwd)"
fi

if [[ ! -d "$TC_BIN" ]]; then
  echo "ERROR: no se encontro el toolchain en $TC_BIN" >&2
  echo "       corre firmware/tools/build_toolchain.sh primero," >&2
  echo "       o exporta RBDEV_TOOLCHAIN=<ruta a un toolchain existente>." >&2
  exit 1
fi

WHAT="${1:---all}"

build_one() {
  local dir="$1" type="$2"
  local build_path="$ROOT_DIR/firmware/$dir"

  # Metro (M-108, D-348 de Aura-Firmware -- mecanismo COMPARTIDO por los
  # tres repos del fork): con BUILD_TARGET_CLEAN=1 se borra el directorio
  # entero antes de configurar. package_dist.sh lo pide con
  # --release-tag: un release tiene que ser reproducible byte a byte y un
  # directorio de build reutilizado no lo garantiza aunque `make dep`
  # (abajo) regenere la base de dependencias.
  if [[ "${BUILD_TARGET_CLEAN:-}" == "1" ]]; then
    echo "==> Limpiando $dir para una compilacion reproducible (D-348)"
    rm -rf "$build_path"
  fi

  mkdir -p "$build_path"
  cd "$build_path"
  if [[ ! -f Makefile ]]; then
    echo "==> Configurando $dir (type=$type)"
    PATH="$TC_BIN:$PATH" "$SRC_DIR/tools/configure" --target=ipod6g --type="$type"
  fi

  # Metro (M-108, D-348): la regla `$(DEPFILE) dep:` de tools/root.make
  # (heredada de Rockbox, compartida por los tres repos del fork) no
  # tiene prerrequisitos -- make.dep se genera UNA vez al crear el
  # directorio de build y nunca se refresca solo. Un #include agregado
  # despues queda invisible para make, y el .o que lo usa no se
  # recompila cuando esa cabecera cambia -- el binario terminaria
  # dependiendo de CUANDO se creo el directorio de build, no solo del
  # commit (encontrado con un rockbox.bin distinto en 7 bytes entre
  # incremental y limpio del mismo commit, en Aura-Firmware). `make dep`
  # solo escanea dependencias y se corre siempre, no solo para un
  # release.
  echo "==> Regenerando la base de dependencias ($dir, make dep, D-348)"
  PATH="$TC_BIN:$PATH" make dep

  echo "==> Compilando $dir"
  PATH="$TC_BIN:$PATH" make -j"$(sysctl -n hw.ncpu)" ${VERSION:+VERSION="$VERSION"}
}

if [[ "$WHAT" == "--all" || "$WHAT" == "--firmware" ]]; then
  build_one build-ipod6g N
  echo "==> Listo: firmware/build-ipod6g/rockbox.ipod"
fi

if [[ "$WHAT" == "--all" || "$WHAT" == "--bootloader" ]]; then
  build_one build-ipod6g-boot B
  echo "==> Listo: firmware/build-ipod6g-boot/bootloader-ipod6g.ipod"
fi
