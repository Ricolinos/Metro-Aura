#!/usr/bin/env python3
"""Genera el logo de arranque (wordmark "metro / aura") como BMP nativo.

Escribe DOS bitmaps del mismo wordmark:

* `apps/bitmaps/native/rockboxlogo.320x98x16.bmp` -- mismo nombre de
  archivo y dimensiones exactas que el original de Rockbox (320x98), asi
  la regla de `apps/bitmaps/bitmaps.make` que lo compila a
  `bm_rockboxlogo` via `bmp2rb` no necesita ningun cambio. Ver
  `DECISIONS.md` M-020 y M-092.

* `apps/bitmaps/native/bootwordmark.<w>x<h>x16.bmp` (solo con
  `--bootloader-crop`, M-107) -- el RECORTE ajustado a la caja de tinta,
  para la pantalla de arranque del BOOTLOADER. Un lienzo de 320x98
  entero no cabe comodo en la IRAM del bootloader, y ademas la mitad
  seria fondo negro.

Por que el recorte y no el lienzo, y por que la marca NO salta al pasar
el control al firmware:

  `show_logo_boot()` (apps/main.c) centra el lienzo de 320x98 en la
  pantalla de 320x240 (centrado en Y solo bajo IPOD_6G -- M-107; el
  Rockbox original lo pega a y=10). El bootloader centra su RECORTE en
  la misma pantalla. Dos centrados distintos de dos imagenes distintas
  caen en el mismo pixel solo si el margen del recorte esta elegido para
  que asi sea, y como ambos usan division ENTERA, un margen simetrico
  puede quedar corrido un pixel. En vez de pasarle un offset al C -- una
  constante mas que se puede desincronizar en silencio -- se ensancha el
  margen LEJANO en 0 o 1 px hasta que el centrado entero da el pixel
  exacto, y el script COMPRUEBA la igualdad antes de escribir: si
  alguien cambia el lienzo o el centrado, falla aqui y no en el iPod.

La maqueta de aprobacion (`--bootloader-crop`) dibuja las leyendas con
los glifos REALES de FONT_SYSFIXED, leidos del BDF que compila Rockbox
(`fonts/08-Schumacher-Clean.bdf`), no con una fuente parecida del host:
la maqueta existe para aprobar algo que despues se flashea en NOR.

Uso:
    firmware/tools/.venv/bin/python3 firmware/tools/gen_logo.py
    firmware/tools/.venv/bin/python3 firmware/tools/gen_logo.py --bootloader-crop
    firmware/tools/.venv/bin/python3 firmware/tools/gen_logo.py --bootloader-crop --check
"""
import argparse
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFont

ROOT_DIR = Path(__file__).resolve().parent.parent.parent
FONT_PATH = ROOT_DIR / "firmware/assets/fonts-src/Selawik-Light.ttf"
BITMAPS_DIR = ROOT_DIR / "firmware/rockbox/apps/bitmaps/native"
OUT_PATH = BITMAPS_DIR / "rockboxlogo.320x98x16.bmp"
SYSFONT_BDF = ROOT_DIR / "firmware/rockbox/fonts/08-Schumacher-Clean.bdf"
MOCKUP_PATH = (ROOT_DIR / "docs/screenshots/ronda-homologacion"
                        / "bootloader-maqueta.png")

WIDTH, HEIGHT = 320, 98
SCREEN_W, SCREEN_H = 320, 240

# M-107: margen negro alrededor de la tinta en el recorte del bootloader.
# 4 px es lo minimo que evita que un antialiasing al ras del borde se vea
# cortado; el margen LEJANO puede crecer 1 px mas (ver la cabecera).
CROP_PAD = 4
# Minimo aceptable cuando el centrado exacto no admite CROP_PAD de los
# dos lados (ver solve_pads()).
MIN_PAD = 2
# Plan maestro SS B.2: la ULTIMA linea a 14 px del borde inferior,
# interlineado 12.
LEGEND_BOTTOM_MARGIN = 14
LEGEND_LINE_SPACING = 12
# R5 (M-092, encargo del dueno): el wordmark ya no es solo "metro" --
# debajo, en menor cuerpo y gris (60% de luminancia: al dibujarse como
# mascara queda atenuado respecto a "metro"), va "aura", la familia.
TEXT = "metro"
SUBTEXT = "aura"
BG = (0, 0, 0)
FG = (255, 255, 255)
SUBFG = (153, 153, 153)


def die(msg):
    sys.exit(f"ERROR: {msg}")


def ink_bbox(img):
    """Caja de la tinta real (lo que no es fondo), no la caja de la fuente."""
    box = ImageChops.difference(img, Image.new("RGB", img.size, BG)).getbbox()
    if box is None:
        die("el lienzo salio vacio: no hay tinta que recortar")
    return box


def crop_for_bootloader(img, box):
    """Recorte con margen, corregido para caer donde lo pinta el firmware.

    Devuelve (imagen recortada, (x, y) donde la centra el bootloader).
    """
    # Donde cae la tinta cuando la pinta el FIRMWARE: show_logo_boot()
    # centra el lienzo de 320x98 en 320x240 (M-107 lo centro tambien en Y).
    logo_x = (SCREEN_W - WIDTH) // 2
    logo_y = (SCREEN_H - HEIGHT) // 2
    ink_screen = (logo_x + box[0], logo_y + box[1])

    left, top, right, bottom = box

    def solve_pads(ink_len, target, screen_len, room_near, room_far):
        """Par de margenes que hace que el centrado ENTERO de EXACTO.

        El bootloader pinta el recorte en (screen_len - crop_len) // 2.
        Un margen simetrico de CROP_PAD no basta: la tinta del wordmark
        no esta perfectamente centrada en el lienzo (94 px a la
        izquierda contra 93 a la derecha), asi que centrar el recorte
        simetrico deja la marca un pixel corrida respecto de donde la
        pinta el firmware.

        En vez de pasarle un offset al C -- una constante mas que se
        puede desincronizar en silencio -- se buscan los dos margenes.
        Se prueban todos los pares razonables y se elige el MAS
        SIMETRICO: la marca queda donde tiene que quedar y el recorte
        lleva casi el mismo fondo negro de cada lado. MIN_PAD es 2 y no
        4 porque 4 era un margen comodo, no un requisito: la caja de
        tinta ya incluye TODO pixel que no sea fondo, asi que cualquier
        margen >= 1 basta para no cortar el antialiasing.
        """
        best = None
        for pad_near in range(MIN_PAD, min(room_near, CROP_PAD * 3) + 1):
            for pad_far in range(MIN_PAD, min(room_far, CROP_PAD * 3) + 1):
                crop_len = pad_near + ink_len + pad_far
                if (screen_len - crop_len) // 2 + pad_near != target:
                    continue
                cost = (abs(pad_near - pad_far),
                        abs(pad_near - CROP_PAD) + abs(pad_far - CROP_PAD))
                if best is None or cost < best[0]:
                    best = (cost, pad_near, pad_far)
        if best is None:
            die("no hay margen que haga coincidir el recorte con la marca del "
                f"firmware (tinta {ink_len}, objetivo {target})")
        return best[1], best[2]

    pad_l, pad_r = solve_pads(right - left, ink_screen[0], SCREEN_W,
                              left, WIDTH - right)
    pad_t, pad_b = solve_pads(bottom - top, ink_screen[1], SCREEN_H,
                              top, HEIGHT - bottom)

    crop_box = (left - pad_l, top - pad_t, right + pad_r, bottom + pad_b)
    crop = img.crop(crop_box)
    cw, ch = crop.size

    draw_x = (SCREEN_W - cw) // 2
    draw_y = (SCREEN_H - ch) // 2
    ink_boot = (draw_x + (left - crop_box[0]), draw_y + (top - crop_box[1]))

    if ink_boot != ink_screen:
        die("el recorte no cae donde el firmware pinta la marca: bootloader "
            f"{ink_boot} vs firmware {ink_screen}. Cambio el centrado de "
            "show_logo_boot() o el lienzo; revisa la cabecera de este archivo.")

    return crop, (draw_x, draw_y)


def load_bdf(path):
    """-> {codepoint: (bbx, filas)} del BDF que compila FONT_SYSFIXED."""
    glyphs = {}
    code = bbx = rows = None
    for raw in path.read_text(encoding="latin-1").splitlines():
        line = raw.strip()
        if line.startswith("ENCODING "):
            code = int(line.split()[1])
        elif line.startswith("BBX "):
            bbx = [int(v) for v in line.split()[1:]]
        elif line == "BITMAP":
            rows = []
        elif line == "ENDCHAR":
            if code is not None and code >= 0 and bbx and rows is not None:
                glyphs[code] = (bbx, rows)
            code = bbx = rows = None
        elif rows is not None and line:
            rows.append(line)
    if not glyphs:
        die(f"no se pudo leer ningun glifo de {path}")
    return glyphs


def draw_sysfont_text(img, glyphs, x, y, text, color):
    """Dibuja `text` con los glifos REALES de sysfont. y = borde superior."""
    ascent = 7  # sysfont.c: ascent 7, height 8, FONTBOUNDINGBOX 6 8 0 -1
    px = img.load()
    cx = x
    for ch in text:
        entry = glyphs.get(ord(ch)) or glyphs.get(ord("?"))
        (gw, gh, gx, gy), rows = entry
        for ry, bits in enumerate(rows):
            value = int(bits, 16)
            width_bits = len(bits) * 4
            for rx in range(gw):
                if value & (1 << (width_bits - 1 - rx)):
                    ax, ay = cx + gx + rx, y + ascent - (gy + gh) + ry
                    if 0 <= ax < img.width and 0 <= ay < img.height:
                        px[ax, ay] = color
        cx += 6  # monoespaciada: 6 px de avance, como FONT_SYSFIXED
    return cx


def render_mockup(crop, crop_pos, legends):
    img = Image.new("RGB", (SCREEN_W, SCREEN_H), BG)
    img.paste(crop, crop_pos)
    glyphs = load_bdf(SYSFONT_BDF)

    n = len(legends)
    first_top = SCREEN_H - LEGEND_BOTTOM_MARGIN - 8 - (n - 1) * LEGEND_LINE_SPACING
    for i, text in enumerate(legends):
        x = (SCREEN_W - 6 * len(text)) // 2
        draw_sysfont_text(img, glyphs, x, first_top + i * LEGEND_LINE_SPACING,
                          text, SUBFG)
    return img


def build_logo():
    font = ImageFont.truetype(str(FONT_PATH), 56)
    subfont = ImageFont.truetype(str(FONT_PATH), 26)

    img = Image.new("RGB", (WIDTH, HEIGHT), BG)
    draw = ImageDraw.Draw(img)

    bbox = draw.textbbox((0, 0), TEXT, font=font)
    text_w = bbox[2] - bbox[0]
    sbbox = draw.textbbox((0, 0), SUBTEXT, font=subfont)
    sub_w = sbbox[2] - sbbox[0]
    sub_h = sbbox[3] - sbbox[1]

    # "metro" arriba, "aura" debajo con 8px de aire; el bloque completo
    # centrado en el lienzo de 98px de alto.
    text_h = bbox[3] - bbox[1]
    total_h = text_h + 8 + sub_h
    top = (HEIGHT - total_h) // 2
    draw.text(((WIDTH - text_w) // 2 - bbox[0], top - bbox[1]), TEXT, font=font, fill=FG)
    draw.text(((WIDTH - sub_w) // 2 - sbbox[0], top + text_h + 8 - sbbox[1]),
              SUBTEXT, font=subfont, fill=SUBFG)

    return img


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bootloader-crop", action="store_true",
                    help="ademas del lienzo, escribe el recorte del "
                         "bootloader y la maqueta de aprobacion (M-107)")
    ap.add_argument("--version-string", default="0000000000-260904",
                    help="rbversion que muestra la MAQUETA (solo cosmetico: "
                         "el bootloader real usa el suyo)")
    ap.add_argument("--check", action="store_true",
                    help="no escribe nada; compara con lo que ya esta en el "
                         "arbol y falla si difiere")
    args = ap.parse_args()

    img = build_logo()
    box = ink_bbox(img)
    print(f"==> Wordmark {TEXT!r}/{SUBTEXT!r}; caja de tinta {box} "
          f"en el lienzo de {WIDTH}x{HEIGHT}")

    if args.check:
        if not OUT_PATH.exists():
            die(f"no existe {OUT_PATH}")
        if Image.open(OUT_PATH).convert("RGB").tobytes() != img.tobytes():
            die(f"{OUT_PATH.name} en el arbol NO coincide con lo que genera "
                "este script (distinta version de Pillow o de la fuente)")
        print(f"==> {OUT_PATH.name}: identico al del arbol")
    else:
        img.save(OUT_PATH, "BMP")
        print(f"==> Logo generado: {OUT_PATH} ({WIDTH}x{HEIGHT})")

    if not args.bootloader_crop:
        return

    crop, pos = crop_for_bootloader(img, box)
    cw, ch = crop.size
    crop_path = BITMAPS_DIR / f"bootwordmark.{cw}x{ch}x16.bmp"
    print(f"==> Recorte {cw}x{ch}; el bootloader lo centra en {pos} y la "
          f"tinta cae donde show_logo_boot() la pinta")

    legends = [
        f"metro \u00b7 arranque {args.version_string}",
        "Basado en Rockbox \u00b7 GPL v2 \u00b7 rockbox.org",
    ]
    mockup = render_mockup(crop, pos, legends)

    if args.check:
        if not crop_path.exists():
            die(f"no existe {crop_path}")
        if Image.open(crop_path).convert("RGB").tobytes() != crop.tobytes():
            die(f"{crop_path.name} en el arbol NO coincide con lo generado")
        print(f"==> {crop_path.name}: identico al del arbol")
        return

    # Un recorte viejo con otras dimensiones dejaria dos entradas en
    # SOURCES apuntando a bitmaps distintos: se limpia.
    for old in BITMAPS_DIR.glob("bootwordmark.*x*x16.bmp"):
        if old != crop_path:
            old.unlink()
            print(f"==> Recorte anterior borrado: {old.name}")
    crop.save(crop_path, "BMP")
    print(f"==> Recorte escrito en {crop_path}")

    MOCKUP_PATH.parent.mkdir(parents=True, exist_ok=True)
    mockup.save(MOCKUP_PATH, "PNG")
    print(f"==> Maqueta de aprobacion en {MOCKUP_PATH}")


if __name__ == "__main__":
    main()
