#!/usr/bin/env python3
"""Verificacion mecanica de fuentes bitmap RB12 (M-010, M-111).

Portado de moonlit.aura (D-066), leido read-only como referencia --
ver DECISIONS.md M-111. Dos modos:

  check_fonts.py <archivo.fnt>
      Lee la cabecera RB12 (firmware/rockbox/firmware/font.c, 36 bytes,
      little-endian) e imprime firstchar/defaultchar/size/height/
      maxwidth. No falla por si solo -- es un reporte; quien genera la
      fuente (firmware/tools/gen_fonts.sh) es quien decide si un valor
      es aceptable.

  check_fonts.py --coverage [--fonts DIR] [--lang RUTA]
      Lee la tabla de glifos de cada .fnt y la compara con lo que
      metro_lang.c va a pedirle -- las 137 cadenas x 6 idiomas
      (M-111). Reporta los faltantes por rol y por idioma. Falla si
      falta algo: la UI propia no puede tener huecos.

Este es el chequeo que hubiera detectado en el acto que Selawik (los
tres .ttf vendoreados en firmware/assets/fonts-src/) no trae NINGUN
glifo cirilico -- ver DECISIONS.md M-111, "ajustes 2" Fase 3.
"""
import argparse
import struct
import sys

RB12_HEADER = struct.Struct("<4sHHHHiiiiii")


def die(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def read_rb12_header(path):
    with open(path, "rb") as f:
        data = f.read(RB12_HEADER.size)
    if len(data) < RB12_HEADER.size:
        die(f"{path}: archivo mas chico que la cabecera RB12 ({len(data)} bytes)")
    (magic, maxwidth, height, ascent, depth, firstchar, defaultchar,
     size, bits_size, noffset, nwidth) = RB12_HEADER.unpack(data)
    if magic != b"RB12":
        die(f"{path}: cabecera invalida (magic={magic!r}, se esperaba RB12)")
    return {
        "maxwidth": maxwidth, "height": height, "ascent": ascent, "depth": depth,
        "firstchar": firstchar, "defaultchar": defaultchar, "size": size,
        "bits_size": bits_size, "noffset": noffset, "nwidth": nwidth,
    }


def cmd_header(path):
    h = read_rb12_header(path)
    print(f"{path}: firstchar={h['firstchar']} defaultchar={h['defaultchar']} "
          f"size={h['size']} height={h['height']} maxwidth={h['maxwidth']}")


# --- coverage (M-111, portado de moonlit D-066) ----------------------------

# font.c. Por debajo de este bits_size la tabla de offsets es de 16
# bits; por encima, de 32. Hace falta para saltar hasta la tabla de
# anchos, que es la que dice si un codigo tiene glifo de verdad.
MAX_FONTSIZE_FOR_16_BIT_OFFSETS = 0xFFDB


def read_glyph_table(path):
    """-> (header, set de codepoints con glifo real).

    Un codigo dentro de [firstchar, firstchar+size) puede seguir sin
    glifo: convttf emite una entrada de ancho 0 para el hueco. Por eso
    no basta con el rango de la cabecera -- hay que leer la tabla de
    anchos."""
    h = read_rb12_header(path)
    with open(path, "rb") as f:
        data = f.read()

    # font.c: tras el bitmap se ALINEA (16 o 32 bits segun bits_size)
    # antes de la tabla de offsets. Sin ese relleno la tabla de anchos
    # sale corrida y todo el reporte miente.
    off = RB12_HEADER.size + h["bits_size"]
    if h["bits_size"] < MAX_FONTSIZE_FOR_16_BIT_OFFSETS:
        off = (off + 1) & ~1
        off += h["noffset"] * 2
    else:
        off = (off + 3) & ~3
        off += h["noffset"] * 4

    # Cruce que impide leer la tabla corrida en silencio: con el
    # relleno bien puesto, la tabla de anchos termina EXACTAMENTE donde
    # termina el archivo.
    if off + h["nwidth"] != len(data):
        die(f"{path}: la tabla de anchos termina en {off + h['nwidth']} "
            f"pero el archivo mide {len(data)} -- el calculo de "
            f"desplazamiento no coincide con font.c")

    widths = data[off:off + h["nwidth"]]
    covered = set()
    if h["nwidth"] == 0:
        # sin tabla de anchos: fuente de ancho fijo, todo el rango cuenta
        covered = set(range(h["firstchar"], h["firstchar"] + h["size"]))
    else:
        for i, w in enumerate(widths):
            if w > 0:
                covered.add(h["firstchar"] + i)
    return h, covered


def lang_codepoints(lang_path):
    """Codepoints de todas las cadenas literales de metro_lang.c, con
    el idioma que las trae (para el reporte por idioma de M-111)."""
    import re
    text = open(lang_path, encoding="utf-8").read()
    # Quita comentarios de bloque y de linea antes de buscar literales,
    # para no recoger acentos que solo viven en la prosa del comentario.
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)

    # Las seis tablas strings_es/en/fr/de/ru/it -- se detectan por su
    # propia declaracion en el archivo, no una lista aparte que se
    # pueda desincronizar si metro_lang.c gana un idioma.
    per_lang = {}
    for m in re.finditer(
            r'static const char \*const strings_(\w+)\[LANG_COUNT\] = \{(.*?)\n\};',
            text, re.S):
        lang, body = m.group(1), m.group(2)
        cps = set()
        for lit in re.findall(r'"((?:[^"\\]|\\.)*)"', body):
            lit = lit.replace("\\n", "\n").replace("\\t", "\t").replace('\\"', '"')
            for ch in lit:
                cps.add(ord(ch))
        per_lang[lang] = cps
    if not per_lang:
        die(f"{lang_path}: no se encontro ninguna tabla strings_<idioma>[] -- "
            "el patron de este script quedo desactualizado")
    return per_lang


def fmt_cp(cp):
    ch = chr(cp)
    shown = ch if ch.isprintable() and not ch.isspace() else " "
    return f"U+{cp:04X} '{shown}'"


# M-114 (Fase 5, plan maestro SS D.3): rango cirílico -- mismos límites
# que metro_textseg.h (METRO_TEXTSEG_CYRILLIC_START/_LIMIT), copiado a
# propósito (este script no enlaza C) en vez de compartido. Mismo rango
# que moonlit_textseg.h (D-081), leído read-only como referencia.
CYRILLIC_RANGE = range(1025, 1106)


def _font_category(name):
    """-> "cyrillic" | "primary", por el sufijo del archivo
    (gen_fonts.sh: metro-<rol>-<px>[-cyrillic].fnt). Cada categoría
    cubre un universo de codepoints DISTINTO a propósito (M-113/M-114)
    -- pedirle a la fuente cirílica que cubra es/en/fr/de/it (o a la
    primaria que cubra ruso) reportaría faltantes que nunca fueron su
    trabajo. Sin categoría "punct": Metro no tiene fuente de
    puntuación aparte (M-111 ya resolvió el único hueco real, el
    carácter de puntos suspensivos, cambiando la cadena fuente por
    "..." en vez de agregar una fuente)."""
    if name.endswith("-cyrillic.fnt"):
        return "cyrillic"
    return "primary"


def cmd_coverage(fonts_dir, lang_path, known_incomplete):
    import glob
    import os

    fonts = sorted(glob.glob(os.path.join(fonts_dir, "*.fnt")))
    if not fonts:
        die(f"no hay .fnt en {fonts_dir}")

    per_lang = lang_codepoints(lang_path)
    # M-114: el rango cirílico es responsabilidad EXCLUSIVA de las
    # fuentes -cyrillic.fnt -- las primarias (M-010/M-111) nunca lo
    # cubrieron ni deberían, así que separarlo aquí es lo que evita que
    # el ruso rompa el chequeo de las demás combinaciones fuente/idioma.
    per_lang_cyrillic = {lang: {c for c in cps if c in CYRILLIC_RANGE}
                          for lang, cps in per_lang.items()}
    per_lang_other = {lang: cps - per_lang_cyrillic[lang]
                       for lang, cps in per_lang.items()}
    ui_cyrillic = set()
    for cps in per_lang_cyrillic.values():
        ui_cyrillic |= cps
    total_ui = set()
    for cps in per_lang.values():
        total_ui |= cps

    print(f"== cobertura de glifos (M-111/M-114) ==  {len(per_lang)} idiomas, "
          f"{len(total_ui)} codepoints distintos en total (+{len(ui_cyrillic)} cirílicos)")
    if known_incomplete:
        print(f"   (tolerados como incompletos, ya conocido: {', '.join(sorted(known_incomplete))})")
    failures = 0
    warnings = 0

    for path in fonts:
        h, covered = read_glyph_table(path)
        name = os.path.basename(path)
        category = _font_category(name)
        print(f"\n{name} [{category}]: firstchar={h['firstchar']} size={h['size']} "
              f"defaultchar={h['defaultchar']} glifos_reales={len(covered)}")

        if category == "cyrillic":
            # M-114: su única responsabilidad es el alfabeto ruso --
            # nunca hay transliteración (no hay ASCII razonable para
            # "я"), así que un faltante aquí es siempre real.
            miss = sorted(c for c in ui_cyrillic if c not in covered)
            if miss:
                failures += 1
                print(f"   FALTA cirílico de la UI ({len(miss)}): "
                      + ", ".join(fmt_cp(c) for c in miss[:12])
                      + (" ..." if len(miss) > 12 else ""))
            else:
                print(f"   cirílico: completo ({len(ui_cyrillic)}/{len(ui_cyrillic)})")
            continue

        for lang in sorted(per_lang):
            # primary: contra per_lang_other -- el rango cirílico (M-114)
            # es trabajo de la fuente -cyrillic.fnt del mismo rol, no de
            # ésta (un rol sin fuente cirílica, hoy solo "display", cae
            # al de "title" en el dibujo por tramos -- metro_draw.c, no
            # a esta fuente primaria).
            miss = sorted(c for c in per_lang_other[lang] if c not in covered and c >= 32)
            if miss and lang in known_incomplete:
                warnings += 1
                print(f"   AVISO (tolerado) {lang} ({len(miss)}/{len(per_lang_other[lang])}): "
                      + ", ".join(fmt_cp(c) for c in miss[:12])
                      + (" ..." if len(miss) > 12 else ""))
            elif miss:
                failures += 1
                print(f"   FALTA {lang} ({len(miss)}/{len(per_lang_other[lang])}): "
                      + ", ".join(fmt_cp(c) for c in miss[:12])
                      + (" ..." if len(miss) > 12 else ""))
            else:
                print(f"   {lang}: completo")

    if failures:
        die(f"{failures} combinacion(es) fuente/idioma o fuente/cirílico sin cobertura -- "
            "ver arriba que codepoints faltan")
    if warnings:
        print(f"\ncheck_fonts: {warnings} combinacion(es) incompletas pero toleradas "
              "(--known-incomplete) -- no bloquean el paquete.")
    else:
        print("\ncheck_fonts: los seis idiomas (con cirílico incluido) estan cubiertos en todos los roles.")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("path", nargs="?", help=".fnt a inspeccionar")
    parser.add_argument("--coverage", action="store_true",
                         help="compara la tabla de glifos de cada .fnt con los seis idiomas de metro_lang.c")
    parser.add_argument("--fonts", default="firmware/assets/fonts",
                         help="directorio de .fnt para --coverage")
    parser.add_argument("--lang", default="firmware/rockbox/apps/metro/metro_lang.c",
                         help="fuente de las cadenas de UI para --coverage")
    parser.add_argument("--known-incomplete", default="",
                         help="idiomas (codigo de dos letras, separados por coma) que se "
                              "reportan pero no hacen fallar --coverage -- para un hueco "
                              "YA CONOCIDO y en camino de resolverse, nunca para silenciar "
                              "uno nuevo (M-111: ver DECISIONS.md, el ruso hasta la Fase 5 "
                              "de la ronda 'ajustes 2')")
    args = parser.parse_args()

    if args.coverage:
        known = {s.strip() for s in args.known_incomplete.split(",") if s.strip()}
        cmd_coverage(args.fonts, args.lang, known)
    elif args.path:
        cmd_header(args.path)
    else:
        parser.error("pasa una ruta .fnt o --coverage")


if __name__ == "__main__":
    main()
