#!/usr/bin/env python3
"""Genera apps/metro/metro_icons_table.c -- los iconos de Fluent System
Icons (Microsoft, MIT) rasterizados a mapas de bits monocromos de
16x16, precalculados offline.

Por que una tabla generada y no assets en disco (R4/FA-1, M-077):

  * Metro no tenia NINGUN pipeline de iconos: sus cuatro iconos eran
    trazos geometricos escritos a mano con lcd_drawline/lcd_fillrect.
    Habia que construir el pipeline entero, no "cambiar de set".
  * Una FUENTE de iconos no cabe: gen_fonts.sh genera el rango
    0x20-0x17F (Latin-1 + Extended-A), sin zona de uso privado, que es
    donde viven los glifos de cualquier icon font.
  * Un .bmp por icono en .rockbox/ obligaria a leer disco para
    dibujarlos, y CLAUDE.md prohibe lectura de disco dentro de un bucle
    de animacion -- los iconos de modo se dibujan en cada cuadro de
    Now Playing.

Asi que se sigue el mismo patron que metro_turnstile_table.c: tabla C
generada offline y COMMITEADA, con su generador versionado al lado.
El .svg tambien se commitea (firmware/assets/icons/), de modo que
regenerar no necesita red.

Formato: un unsigned short por fila, bit 15 = pixel de la izquierda.
16 filas x 2 bytes = 32 bytes por icono. Monocromo a proposito: el
color lo pone quien dibuja (metro_color_accent() o el que toque), que
es la unica forma de respetar la regla de "cero RGB hardcodeado fuera
de metro_palette.h".

Requiere `rsvg-convert` (librsvg) y Pillow.

Uso:
    python3 firmware/tools/gen_icons.py
    python3 firmware/tools/gen_icons.py --preview   # arte ASCII, sin escribir
"""
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

ROOT_DIR = Path(__file__).resolve().parent.parent.parent
SVG_DIR = ROOT_DIR / "firmware/assets/icons"
OUT_PATH = ROOT_DIR / "firmware/rockbox/apps/metro/metro_icons_table.c"
GLYPHS_OUT_PATH = ROOT_DIR / "firmware/rockbox/apps/metro/metro_glyphs_table.c"

SIZE = 16

# R5 (M-089): glifos GRANDES con antialiasing, como mascaras de cobertura
# de 8 bits (un byte por pixel), para las pantallas que no pueden leer
# disco y necesitan algo mayor de 16px (la pantalla USB). La tabla
# monocroma de arriba no sirve para eso: escalarla 2x da bloques (el
# dueno lo vio en el iPod: "el icono se ve pixeleado"). Se rasteriza el
# SVG al tamano final con rsvg-convert y se guarda el canal alfa tal
# cual; en el firmware se dibuja con metro_fb_plot_alpha() mezclando el
# color elegido contra lo que haya debajo. 40x40 = 1,600 bytes por glifo.
#   (archivo svg sin extension, simbolo, tamano en px)
GLYPHS = [
    ("sync_large", "SYNC", 40),
    # M-104: candado grande para la pantalla de bloqueo EN REPOSO (la
    # que se ve con el interruptor Hold puesto). Mismo motivo que el
    # glifo de sync: es un simbolo SOLO en una pantalla vacia, y escalar
    # la mascara monocroma de 16px daria bloques. Se parte del SVG de
    # 24px (no del de 16) porque a 40px el trazo del de 16 se ve
    # engordado -- Fluent dibuja cada tamano por separado.
    ("lock_large", "LOCK", 40),
]

# El orden define enum metro_icon_id en metro_icons.h -- si se agrega
# uno, va AL FINAL y se agrega alli tambien, en el mismo orden.
#
# NO esta arrow_repeat_1, a proposito: su insignia del "1" se apelmaza
# en una mancha ilegible a 16px, y se comprobo que ninguna variante lo
# salva (16 filled con umbral alto rompe el lazo en pixeles sueltos; 20
# filled reescalado y 16 regular quedan igual de densos). Para
# REPEAT_ONE se dibuja este mismo lazo mas el caracter '1' al lado,
# que es el mecanismo que Metro ya usaba y si se lee. Ver DECISIONS.md
# M-077.
#
# R5-F3 (M-083): star es la variante REGULAR (contorno) de Fluent -- es
# el estilo de linea de la maqueta del dueno y a 16px binariza limpia.
# shuffle/repeat_all se quedan en FILLED a proposito: se probo su
# Regular y las puntas de flecha se fragmentan a esta densidad (el
# Filled de Fluent para flechas es el mismo trazo de linea, solo mas
# grueso -- no una silueta). previous/next son Filled por lo mismo.
ICONS = [
    ("play",       "PLAY"),
    ("pause",      "PAUSE"),
    ("shuffle",    "SHUFFLE"),
    ("repeat_all", "REPEAT_ALL"),
    ("speaker",    "SPEAKER"),
    ("star",       "STAR"),
    ("previous",   "PREVIOUS"),
    ("next",       "NEXT"),
    ("sync",       "SYNC"),  # R5/M-088: pantalla USB
    # M-104: candado de la barra de estado (Hold puesto). FILLED, no
    # Regular: a 16px el contorno del arco del grillete se parte en
    # pixeles sueltos al binarizar, igual que ya se documento arriba
    # para shuffle/repeat_all/previous/next. El Filled de Fluent para
    # el candado es una silueta solida con el ojo de la cerradura
    # calado, que a este tamano se lee de inmediato.
    ("lock",       "LOCK"),
]

# Umbral sobre el canal ALFA, no sobre el color: los SVG de Fluent son
# una silueta solida (fill="#212121") sobre fondo transparente, asi que
# la forma la define la cobertura, no el tono. 40% en vez de 50% para
# no adelgazar de mas los trazos finos al binarizar a 16px.
ALPHA_THRESHOLD = 102  # 0.4 * 255


def rasterize(svg_path: Path) -> Image.Image:
    """SVG -> PIL Image RGBA de SIZE x SIZE, via rsvg-convert."""
    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as tmp:
        tmp_path = Path(tmp.name)
    try:
        subprocess.run(
            ["rsvg-convert", "-w", str(SIZE), "-h", str(SIZE),
             "-o", str(tmp_path), str(svg_path)],
            check=True, capture_output=True,
        )
        return Image.open(tmp_path).convert("RGBA").copy()
    finally:
        tmp_path.unlink(missing_ok=True)


def to_rows(img: Image.Image) -> list[int]:
    """Imagen -> 16 mascaras de fila, bit 15 = pixel de la izquierda."""
    rows = []
    px = img.load()
    for y in range(SIZE):
        mask = 0
        for x in range(SIZE):
            if px[x, y][3] >= ALPHA_THRESHOLD:
                mask |= 1 << (SIZE - 1 - x)
        rows.append(mask)
    return rows


def ascii_art(rows: list[int]) -> str:
    out = []
    for mask in rows:
        out.append("".join("#" if mask & (1 << (SIZE - 1 - x)) else "."
                            for x in range(SIZE)))
    return "\n".join(out)


def main() -> int:
    preview = "--preview" in sys.argv

    if not SVG_DIR.is_dir():
        print(f"ERROR: no existe {SVG_DIR}", file=sys.stderr)
        return 1

    table = []
    for name, _sym in ICONS:
        svg = SVG_DIR / f"{name}.svg"
        if not svg.is_file():
            print(f"ERROR: falta {svg}", file=sys.stderr)
            return 1
        rows = to_rows(rasterize(svg))
        if not any(rows):
            print(f"ERROR: {name} rasterizo vacio -- revisa el SVG",
                  file=sys.stderr)
            return 1
        table.append((name, rows))
        if preview:
            print(f"--- {name} ---\n{ascii_art(rows)}\n")

    if preview:
        return 0

    lines = [
        "/* GENERATED by firmware/tools/gen_icons.py -- do not edit by hand.",
        " *",
        " * Fluent System Icons (Microsoft), MIT -- see",
        " * firmware/assets/icons/LICENSE-fluent-system-icons.txt for the",
        " * license text redistributed with the release, and",
        " * THIRD-PARTY-NOTICES.txt in firmware/dist/.",
        " *",
        " * One unsigned short per row, bit 15 = leftmost pixel.",
        " * Monochrome on purpose: the caller picks the colour, which is what",
        " * keeps this out of the way of the no-hardcoded-RGB rule.",
        " */",
        '#include "metro_icons.h"',
        "",
        "const struct metro_icon metro_icons[METRO_ICON_COUNT] = {",
    ]
    for name, rows in table:
        lines.append(f"    /* {name} */")
        lines.append("    { {")
        for i in range(0, SIZE, 4):
            chunk = ", ".join(f"0x{r:04X}" for r in rows[i:i + 4])
            lines.append(f"        {chunk},")
        lines.append("    } },")
    lines.append("};")
    lines.append("")

    OUT_PATH.write_text("\n".join(lines), encoding="utf-8")
    print(f"==> {OUT_PATH.relative_to(ROOT_DIR)} ({len(table)} iconos)")

    # --- glifos grandes con antialiasing (M-089) ---
    glines = [
        "/* GENERATED by firmware/tools/gen_icons.py -- do not edit by hand.",
        " *",
        " * Fluent System Icons (Microsoft), MIT -- see",
        " * firmware/assets/icons/LICENSE-fluent-system-icons.txt.",
        " *",
        " * 8-bit coverage masks (one byte per pixel, row-major), rasterised",
        " * with anti-aliasing at their final size. Drawn by blending a colour",
        " * over whatever is behind (metro_fb_plot_alpha), so they stay",
        " * theme-neutral and never hard-code an RGB.",
        " */",
        '#include "metro_glyphs.h"',
        "",
    ]
    for name, sym, size in GLYPHS:
        svg = SVG_DIR / f"{name}.svg"
        if not svg.is_file():
            print(f"ERROR: falta {svg}", file=sys.stderr)
            return 1
        with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as tmp:
            tmp_path = Path(tmp.name)
        try:
            subprocess.run(["rsvg-convert", "-w", str(size), "-h", str(size),
                            "-o", str(tmp_path), str(svg)],
                           check=True, capture_output=True)
            img = Image.open(tmp_path).convert("RGBA")
        finally:
            tmp_path.unlink(missing_ok=True)
        alpha = img.split()[3]
        data = list(alpha.get_flattened_data()) if hasattr(alpha, "get_flattened_data") else list(alpha.getdata())
        if not any(data):
            print(f"ERROR: {name} rasterizo vacio", file=sys.stderr)
            return 1
        glines.append(f"static const unsigned char glyph_{name}_alpha[{size * size}] = {{")
        for y in range(size):
            row = data[y * size:(y + 1) * size]
            glines.append("    " + ", ".join(f"{v:3d}" for v in row) + ",")
        glines.append("};")
        glines.append(f"const struct metro_glyph metro_glyph_{name} = {{ {size}, {size}, glyph_{name}_alpha }};")
        glines.append("")
    GLYPHS_OUT_PATH.write_text("\n".join(glines), encoding="utf-8")
    print(f"==> {GLYPHS_OUT_PATH.relative_to(ROOT_DIR)} ({len(GLYPHS)} glifos)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
