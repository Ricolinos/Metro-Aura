#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Reporte de uso de pila del hilo principal (M-101).

Mide el marco de cada funcion del binario del target y estima el peor
camino de pila desde las raices del hilo `main` (el de la UI). Falla --
codigo de salida 1 -- si:

  (a) una funcion de `apps/metro/` supera --max-frame bytes (1024 por
      defecto), o
  (b) el peor camino estimado supera --max-usage por ciento de la pila
      del hilo main (75 % por defecto), leida de rockbox.map.

Uso:
    firmware/tools/stack_report.py                    # build-ipod6g
    firmware/tools/stack_report.py --top 30
    firmware/tools/stack_report.py --path metro_screen_nowplaying_handle
    firmware/tools/stack_report.py --quiet            # solo el veredicto

PORTADO de Aura-Firmware (D-345), que es donde se escribio primero y
donde vive el binario cuyo panic real motivo la herramienta. Se copia el
mecanismo tal cual -- solo cambian las rutas (`apps/metro/`), las raices
del hilo de UI y la tabla GUARDED_EDGES, que es especifica de cada
familia. Registrado en DECISIONS.md M-101.

DESVIACION DOCUMENTADA respecto a PLAN-ronda-3-firmwares-maestro.md
SS E.3, que pedia compilar con `-fstack-usage` y leer los `.su`:

Esta herramienta saca los marcos del DESENSAMBLADO del binario que de
verdad se publica (`arm-elf-eabi-objdump -d`), sumando el prologo
(`push`/`stmdb` + `sub sp, #imm`) de cada funcion. Se prefirio a los
`.su` por tres razones:

  1. Mide el binario que se empaqueta, no una recompilacion con otras
     banderas. `-fstack-usage` reporta lo que el compilador RESERVA por
     funcion fuente; tras inlining y tail-calls el binario real puede
     diferir (caso concreto medido en D-345: `style_fonts_exist` no
     existe como marco propio -- gcc la integra en `aura_style_scan`, y
     sumar ambos `.su` cuenta la misma memoria dos veces).
  2. No exige un segundo arbol de compilacion. `package_dist.sh` puede
     llamarla en segundos sobre el `.elf` recien enlazado, en vez de
     rehacer un build completo con banderas distintas.
  3. El grafo de llamadas sale del MISMO desensamblado, asi que marcos y
     aristas siempre corresponden al mismo binario.

Se conserva `--su-dir` como contraste opcional: si existe un arbol
compilado con `-fstack-usage`, imprime las funciones donde el `.su` y el
desensamblado difieren mas.

LIMITES (se imprimen en el reporte, no se ocultan):
  - Solo se siguen llamadas DIRECTAS (`bl`/`blx <sym>`, y `b <sym>` a otra
    funcion como tail-call). Las llamadas por puntero se cuentan como
    "no seguidas" y su subarbol NO entra en la estimacion.
  - Es una sobreaproximacion del peor caso estatico: suma el peor hijo de
    cada nodo aunque dos ramas nunca ocurran en la misma corrida.
  - La recursion se corta y se reporta aparte (un ciclo no tiene peor
    camino finito).

Copyright (C) 2026 Ricardo Gomez -- GPL v2 (ver LICENSE en la raiz).
"""

import argparse
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(ROOT)  # .../Metro-Aura

DEFAULT_BUILD = os.path.join(ROOT, "firmware", "build-ipod6g")
DEFAULT_OBJDUMP = os.path.join(ROOT, "firmware", "toolchain", "bin",
                               "arm-elf-eabi-objdump")
DEFAULT_NM = os.path.join(ROOT, "firmware", "toolchain", "bin",
                          "arm-elf-eabi-nm")

# Raices del hilo de UI. `main` es la de Rockbox; `metro_main` es la
# nuestra, y se lista aparte porque el camino que importa nace ahi
# (M-006: `root_menu()` de Rockbox no existe en este firmware).
ROOTS = ("main", "metro_main")

# Manejadores de FALLO: solo corren cuando el firmware ya se esta
# muriendo (canario de pila pisado, division por cero, buflib
# corrupto). Se cortan del grafo porque su subarbol -- panicf() ->
# vuprintf() -> UnwindStart() (656 B) y el desenrollador completo -- no
# es un camino de ejecucion normal, y dejarlo dentro hace que el peor
# camino de CUALQUIER funcion que ceda el CPU incluya el manejador del
# desbordamiento que esta herramienta existe para evitar. Se listan en
# el reporte para que la exclusion sea visible, no silenciosa.
FAULT_HANDLERS = (
    "thread_stkov", "thread_panicf", "panicf", "panicf_f",
    "buflib_panic", "UIE", "rb_backtrace", "UnwindStart",
    "__div0", "__div0_wrap", "__div0_wrap_s",
)

# Aristas que existen en el binario pero que una GUARDA DE EJECUCION
# vuelve inalcanzables en Aura. El desensamblado no puede verlas: la
# guarda es una variable, no una constante de compilacion. Se cortan
# aqui, de forma DECLARADA -- cada una se imprime en el reporte con su
# motivo, para que la exclusion sea auditable y no un numero que baja
# sin explicacion. Si alguna deja de ser cierta, el numero del reporte
# se vuelve optimista: revisar al tocar cualquiera de los dos extremos.
GUARDED_EDGES = {
    # M-101: VACIA a proposito en Metro-Aura. Aura-Firmware corta aqui
    # ("skin_get_gwps" -> "skin_load") porque D-345 hizo que
    # settings_apply_skins() no cargue skins. Metro NO porto ese cambio:
    # medido con esta misma herramienta, su camino de skins cuesta
    # 5 136 B desde skin_get_gwps (5 296 B desde settings_apply_skins) y
    # NO es el peor camino del binario -- el peor nace en
    # metro_music.c/tagcache. Tocar el motor de skins de Rockbox para
    # ganar un camino que ya cabe seria riesgo sin beneficio; se
    # reevalua si el peor camino se acerca al tope. Cualquier arista que
    # se agregue aqui se imprime en el reporte con su motivo, para que
    # la exclusion sea auditable y no un numero que baja sin
    # explicacion.
}

FN_RE = re.compile(r"^[0-9a-f]+ <(.+)>:")
PUSH_RE = re.compile(r"^\s*(?:push|stmdb)\b[^{]*\{([^}]*)\}")
SUB_SP_RE = re.compile(r"^\s*sub(?:s|\.w)?\s+sp,\s*(?:sp,\s*)?#(\d+)")
# `bl <addr> <sym>` / `b <addr> <sym>` / `blx <addr> <sym>`
CALL_RE = re.compile(r"^\s*(bl|blx|b)(?:\.w|\.n)?(?:eq|ne|cs|cc|mi|pl|vs|vc|"
                     r"hi|ls|ge|lt|gt|le|al)?\s+[0-9a-f]+\s+<([^>+]+)(?:\+0x[0-9a-f]+)?>")
# Salto por puntero: blx <reg>, bx <reg>, `ldr pc, ...`, `mov pc, ...`
INDIRECT_RE = re.compile(r"^\s*(?:blx\s+(?!0x|[0-9a-f]+\s)[a-z]|"
                         r"ldr\s+pc\b|mov\s+pc,\s*[a-z]|"
                         r"ldm[a-z.]*\s+[^,]+,\s*\{[^}]*pc[^}]*\}\s*$)")

REG_IDX = {"sl": 10, "fp": 11, "ip": 12, "sp": 13, "lr": 14, "pc": 15}


def reg_index(name):
    name = name.strip()
    if name.startswith("r") and name[1:].isdigit():
        return int(name[1:])
    return REG_IDX.get(name, 0)


def push_bytes(reglist):
    total = 0
    for item in reglist.split(","):
        item = item.strip()
        if not item:
            continue
        if "-" in item:
            lo, hi = item.split("-", 1)
            total += reg_index(hi) - reg_index(lo) + 1
        else:
            total += 1
    return total * 4


def disassemble(objdump, elf):
    out = subprocess.run([objdump, "-d", elf], capture_output=True, text=True)
    if out.returncode != 0:
        sys.exit("ERROR: objdump fallo sobre %s\n%s" % (elf, out.stderr))
    return out.stdout


def parse(disasm):
    """-> (frames, calls, indirect) por nombre de funcion."""
    frames, calls, indirect = {}, {}, set()
    cur = None
    for line in disasm.splitlines():
        m = FN_RE.match(line)
        if m:
            cur = m.group(1)
            frames.setdefault(cur, 0)
            calls.setdefault(cur, set())
            continue
        if cur is None:
            continue
        parts = line.split("\t")
        if len(parts) < 3:
            continue
        text = "\t".join(parts[2:]).rstrip()

        m = PUSH_RE.match(text)
        if m:
            frames[cur] += push_bytes(m.group(1))
            continue
        m = SUB_SP_RE.match(text)
        if m:
            frames[cur] += int(m.group(1))
            continue
        m = CALL_RE.match(text)
        if m:
            target = m.group(2)
            # `b` dentro de la propia funcion es un salto local, no una
            # llamada; `b` a otro simbolo es un tail-call.
            if target != cur:
                calls[cur].add(target)
            continue
        if INDIRECT_RE.match(text):
            indirect.add(cur)
    return frames, calls, indirect


def metro_symbols(nm, build_dir):
    """Simbolos definidos en los .o de apps/metro/ (para separar el reporte)."""
    metro_dir = os.path.join(build_dir, "apps", "metro")
    syms = set()
    if not os.path.isdir(metro_dir):
        return syms
    objs = [os.path.join(metro_dir, f) for f in sorted(os.listdir(metro_dir))
            if f.endswith(".o")]
    if not objs:
        return syms
    out = subprocess.run([nm, "--defined-only"] + objs,
                         capture_output=True, text=True)
    for line in out.stdout.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[1] in "TtWw":
            syms.add(parts[2])
    return syms


def stack_bounds(map_path):
    """-> (stackbegin, stackend) del hilo main, leidos del .map."""
    begin = end = None
    with open(map_path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            if begin is None and re.search(r"\bstackbegin = \.", line):
                begin = int(line.split()[0], 16)
            elif end is None and re.search(r"\bstackend = \.", line):
                end = int(line.split()[0], 16)
            if begin is not None and end is not None:
                break
    return begin, end


def base_name(sym):
    """`foo.constprop.0` / `foo.part.0` -> `foo` (solo para mostrar)."""
    return re.sub(r"\.(constprop|part|isra|cold|lto_priv)\.\d+$", "", sym)


class Worst:
    """Peor camino de pila, con memoizacion y corte de ciclos."""

    def __init__(self, frames, calls, cut=(), cut_edges=()):
        self.frames = frames
        self.calls = calls
        self.cut = set(cut)
        self.cut_edges = set(cut_edges)
        self.edges_seen = set()
        self.memo = {}
        self.best_child = {}
        self.recursive = set()
        self.cut_seen = set()

    def cost(self, fn, stack):
        if fn in self.memo:
            return self.memo[fn]
        if fn in stack:
            self.recursive.add(fn)
            return 0  # el ciclo se corta; se reporta aparte
        own = self.frames.get(fn, 0)
        best, best_fn = 0, None
        stack.add(fn)
        for callee in sorted(self.calls.get(fn, ())):
            if callee not in self.frames:
                continue
            if callee in self.cut:
                self.cut_seen.add(callee)
                continue
            if (fn, callee) in self.cut_edges:
                self.edges_seen.add((fn, callee))
                continue
            c = self.cost(callee, stack)
            if c > best:
                best, best_fn = c, callee
        stack.discard(fn)
        total = own + best
        # Se memoiza y se guarda el mejor hijo SIEMPRE, tambien en los
        # nodos cuyo subarbol tuvo un ciclo cortado: si no, el camino
        # impreso se queda mudo justo donde esta el ciclo y deja de
        # sumar el total reportado. El valor de un nodo con ciclo
        # depende del orden de visita (deterministico: hijos ordenados)
        # y queda por debajo del peor caso real -- por eso los ciclos se
        # listan aparte en "Limites de la medicion".
        self.memo[fn] = total
        self.best_child[fn] = best_fn
        return total

    def path(self, fn):
        out, seen = [], set()
        while fn is not None and fn not in seen:
            seen.add(fn)
            out.append((fn, self.frames.get(fn, 0)))
            fn = self.best_child.get(fn)
        return out

    def check_path(self, fn):
        """True si el camino impreso suma exactamente el costo reportado."""
        return sum(f for _n, f in self.path(fn)) == self.memo.get(fn, -1)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build-dir", default=DEFAULT_BUILD)
    ap.add_argument("--elf", default=None)
    ap.add_argument("--map", default=None)
    ap.add_argument("--objdump", default=DEFAULT_OBJDUMP)
    ap.add_argument("--nm", default=DEFAULT_NM)
    ap.add_argument("--top", type=int, default=30,
                    help="cuantas funciones listar por grupo (30)")
    ap.add_argument("--max-frame", type=int, default=1024,
                    help="marco maximo permitido en apps/metro/ (1024 B)")
    ap.add_argument("--max-usage", type=float, default=75.0,
                    help="porcentaje maximo del peor camino sobre la pila (75)")
    ap.add_argument("--path", metavar="FUNC", action="append", default=[],
                    help="imprime el peor camino desde FUNC (repetible)")
    ap.add_argument("--keep-guarded-edges", action="store_true",
                    help="no cortar las aristas con guarda de ejecucion "
                         "(ver GUARDED_EDGES)")
    ap.add_argument("--keep-fault-handlers", action="store_true",
                    help="no cortar panicf/UnwindStart y companiia del grafo")
    ap.add_argument("--su-dir", default=None,
                    help="arbol compilado con -fstack-usage, para contraste")
    ap.add_argument("--quiet", action="store_true",
                    help="solo el veredicto y los incumplimientos")
    args = ap.parse_args()

    elf = args.elf or os.path.join(args.build_dir, "rockbox.elf")
    mapf = args.map or os.path.join(args.build_dir, "rockbox.map")
    for f in (elf, mapf):
        if not os.path.exists(f):
            sys.exit("ERROR: no existe %s (compila el target primero)" % f)

    frames, calls, indirect = parse(disassemble(args.objdump, elf))
    metro = metro_symbols(args.nm, args.build_dir)
    begin, end = stack_bounds(mapf)
    if begin is None or end is None:
        sys.exit("ERROR: no se encontraron stackbegin/stackend en %s" % mapf)
    stack_size = end - begin

    cut = () if args.keep_fault_handlers else FAULT_HANDLERS
    edges = () if args.keep_guarded_edges else GUARDED_EDGES
    worst = Worst(frames, calls, cut, edges)
    root_costs = []
    for r in ROOTS:
        if r in frames:
            root_costs.append((r, worst.cost(r, set())))
    if not root_costs:
        sys.exit("ERROR: ninguna de las raices %s existe en el binario" % (ROOTS,))
    peak_fn, peak = max(root_costs, key=lambda x: x[1])
    pct = 100.0 * peak / stack_size if stack_size else 0.0

    if not args.quiet:
        print("== Pila del hilo main ==")
        print("  binario     %s" % os.path.relpath(elf, ROOT))
        print("  stackbegin  0x%05x" % begin)
        print("  stackend    0x%05x" % end)
        print("  tamano      %d B (%.1f KB)" % (stack_size, stack_size / 1024.0))
        print()

        for group, title in ((True, "apps/metro/"), (False, "Rockbox base")):
            rows = [(v, k) for k, v in frames.items()
                    if (k in metro) == group and v > 0]
            rows.sort(reverse=True)
            print("== %d marcos mayores -- %s ==" % (args.top, title))
            for v, k in rows[:args.top]:
                flag = "  <-- supera %d B" % args.max_frame \
                    if (group and v > args.max_frame) else ""
                print("  %7d  %s%s" % (v, base_name(k), flag))
            print()

        print("== Peor camino estatico ==")
        for r, c in sorted(root_costs, key=lambda x: -x[1]):
            print("  desde %-12s %7d B  (%.1f %% de la pila)"
                  % (r, c, 100.0 * c / stack_size))
        print()
        for fn, fsz in worst.path(peak_fn):
            print("  %7d  %s" % (fsz, base_name(fn)))
        if not worst.check_path(peak_fn):
            print("  (el camino impreso se corto en un ciclo: suma menos que")
            print("   el total de arriba -- ver 'recursion detectada')")
        print()

        for fn in args.path:
            if fn not in frames:
                print("  (sin simbolo: %s)" % fn)
                continue
            total = worst.cost(fn, set())
            print("== Peor camino desde %s: %d B ==" % (fn, total))
            for f2, fsz in worst.path(fn):
                print("  %7d  %s" % (fsz, base_name(f2)))
            print()

        print("== Limites de la medicion ==")
        print("  funciones con salto indirecto (subarbol NO seguido): %d"
              % len(indirect))
        for caller, callee in sorted(worst.edges_seen):
            print("  arista cortada por guarda: %s -> %s" % (caller, callee))
            print("    %s" % GUARDED_EDGES[(caller, callee)])
        if worst.cut_seen:
            print("  manejadores de fallo cortados del grafo: %s"
                  % ", ".join(sorted(worst.cut_seen)))
            print("  (solo corren con el firmware ya caido; --keep-fault-handlers")
            print("   los vuelve a incluir)")
        if worst.recursive:
            print("  recursion detectada (ciclo cortado): %s"
                  % ", ".join(sorted(base_name(f) for f in worst.recursive)[:8]))
        print("  sobreaproxima: suma el peor hijo de cada nodo aunque dos")
        print("  ramas nunca ocurran en la misma corrida.")
        print()

        if args.su_dir:
            report_su(args.su_dir, frames)

    over = sorted(((v, k) for k, v in frames.items()
                   if k in metro and v > args.max_frame), reverse=True)
    failed = False
    if over:
        failed = True
        print("FALLA (a): %d funcion(es) de apps/metro/ superan %d B:"
              % (len(over), args.max_frame))
        for v, k in over:
            print("  %7d  %s" % (v, base_name(k)))
    if pct > args.max_usage:
        failed = True
        print("FALLA (b): peor camino %d B = %.1f %% de la pila (%d B), "
              "tope %.0f %%" % (peak, pct, stack_size, args.max_usage))

    if failed:
        return 1
    print("stack_report: OK -- peor camino %d B (%.1f %% de %d B), "
          "ninguna funcion de apps/metro/ sobre %d B."
          % (peak, pct, stack_size, args.max_frame))
    return 0


def report_su(su_dir, frames):
    """Contraste opcional contra un arbol compilado con -fstack-usage."""
    su = {}
    for dirpath, _dirs, files in os.walk(su_dir):
        for f in files:
            if not f.endswith(".su"):
                continue
            with open(os.path.join(dirpath, f), encoding="utf-8",
                      errors="replace") as fh:
                for line in fh:
                    parts = line.rsplit("\t", 2)
                    if len(parts) != 3:
                        continue
                    name = parts[0].split(":")[-1]
                    try:
                        su[name] = max(su.get(name, 0), int(parts[1]))
                    except ValueError:
                        pass
    if not su:
        print("== Contraste con .su: no se encontro ningun .su en %s ==" % su_dir)
        return
    diffs = []
    for name, size in su.items():
        real = frames.get(name)
        if real is None:
            diffs.append((size, name, None))
        elif abs(real - size) >= 256:
            diffs.append((abs(real - size), name, real))
    diffs.sort(reverse=True)
    print("== Contraste con -fstack-usage (%d funciones) ==" % len(su))
    for delta, name, real in diffs[:15]:
        if real is None:
            print("  %7d  %-40s .su only (integrada o eliminada)" % (delta, name))
        else:
            print("  %7d  %-40s .su=%d  binario=%d"
                  % (delta, name, su[name], real))
    print()


if __name__ == "__main__":
    sys.exit(main())
