/* Tests host de metro_textseg.c (M-114). Modulo puro: enlaza solo.
 * `make -C apps/metro/test`. */
#include <stdio.h>
#include <string.h>
#include "../metro_textseg.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

#define BUFSZ 128

/* Sin fuente cirilica (MFONT_DISPLAY, M-113): siempre UN tramo
 * PRIMARY con la cadena entera, byte a byte. */
static void test_sin_fuente_cirilica(void)
{
    char buf[BUFSZ];
    struct metro_textseg segs[8];
    int n;

    n = metro_textseg_build("Don't Stop", false, buf, sizeof(buf), segs, 8);
    CHECK(n == 1);
    CHECK(segs[0].kind == METRO_TEXTSEG_PRIMARY);
    CHECK(!strcmp(segs[0].text, "Don't Stop"));
}

/* Sin fuente cirilica, un codepoint ruso de todos modos: NO se
 * clasifica CYRILLIC -- el atajo de un solo tramo aplica siempre que
 * has_cyrillic_font sea falso, sin excepciones. */
static void test_sin_fuente_cirilica_ignora_ruso(void)
{
    char buf[BUFSZ];
    struct metro_textseg segs[8];
    int n;

    /* "музыка" (musica). */
    n = metro_textseg_build("\xd0\xbc\xd1\x83\xd0\xb7\xd1\x8b\xd0\xba\xd0\xb0",
                            false, buf, sizeof(buf), segs, 8);
    CHECK(n == 1);
    CHECK(segs[0].kind == METRO_TEXTSEG_PRIMARY);
}

/* Texto ASCII puro, con fuente cirilica disponible: un solo tramo
 * PRIMARY, sin gastar tramos de mas. */
static void test_ascii_puro_un_tramo(void)
{
    char buf[BUFSZ];
    struct metro_textseg segs[8];
    int n;

    n = metro_textseg_build("Analog Dreams", true, buf, sizeof(buf), segs, 8);
    CHECK(n == 1);
    CHECK(segs[0].kind == METRO_TEXTSEG_PRIMARY);
    CHECK(!strcmp(segs[0].text, "Analog Dreams"));
}

/* Texto ruso puro, con fuente cirilica disponible: un solo tramo
 * CYRILLIC, sin gastar tramos de mas. */
static void test_cirilico_puro_un_tramo(void)
{
    char buf[BUFSZ];
    struct metro_textseg segs[8];
    int n;

    /* "музыка" -- las seis letras dentro de 1025-1105. */
    n = metro_textseg_build("\xd0\xbc\xd1\x83\xd0\xb7\xd1\x8b\xd0\xba\xd0\xb0",
                            true, buf, sizeof(buf), segs, 8);
    CHECK(n == 1);
    CHECK(segs[0].kind == METRO_TEXTSEG_CYRILLIC);
    CHECK(!strcmp(segs[0].text, "\xd0\xbc\xd1\x83\xd0\xb7\xd1\x8b\xd0\xba\xd0\xb0"));
}

/* Cirilico y latin alternando -- p.ej. un titulo con una palabra en
 * ingles adentro -- cada tramo con su tipo, contiguos del mismo tipo
 * fundidos en uno. */
static void test_cirilico_y_latin_alternan(void)
{
    char buf[BUFSZ];
    struct metro_textseg segs[8];
    int n;

    /* "рок - live" ("rock - live"): cirilico, luego PRIMARY -- " - "
     * y "live" son el MISMO tipo (PRIMARY) y se funden en un tramo. */
    n = metro_textseg_build("\xd1\x80\xd0\xbe\xd0\xba - live",
                            true, buf, sizeof(buf), segs, 8);
    CHECK(n == 2);
    CHECK(segs[0].kind == METRO_TEXTSEG_CYRILLIC);
    CHECK(!strcmp(segs[0].text, "\xd1\x80\xd0\xbe\xd0\xba"));
    CHECK(segs[1].kind == METRO_TEXTSEG_PRIMARY);
    CHECK(!strcmp(segs[1].text, " - live"));
}

/* Empieza y termina en un tramo CYRILLIC -- no hay primario que abrir
 * o cerrar alrededor. */
static void test_cirilico_al_borde(void)
{
    char buf[BUFSZ];
    struct metro_textseg segs[8];
    int n;

    /* "настройки" (ajustes) -- cirilico de punta a punta, como los
     * nombres de pivote del hub en ruso. */
    n = metro_textseg_build(
        "\xd0\xbd\xd0\xb0\xd1\x81\xd1\x82\xd1\x80\xd0\xbe\xd0\xb9\xd0\xba\xd0\xb8",
        true, buf, sizeof(buf), segs, 8);
    CHECK(n == 1);
    CHECK(segs[0].kind == METRO_TEXTSEG_CYRILLIC);
}

/* Un codepoint fuera de los dos rangos (emoji, CJK) no se clasifica
 * CYRILLIC ni revienta nada -- cae a PRIMARY, tal cual, para que el
 * defaultchar del rol resuelva lo que ninguna fuente cubre. */
static void test_fuera_de_rango_cae_a_primary(void)
{
    char buf[BUFSZ];
    struct metro_textseg segs[8];
    int n;

    n = metro_textseg_build("Wheel \xe2\x99\xaa in the Sky", true,
                            buf, sizeof(buf), segs, 8);
    CHECK(n == 1);
    CHECK(segs[0].kind == METRO_TEXTSEG_PRIMARY);
    CHECK(!strcmp(segs[0].text, "Wheel \xe2\x99\xaa in the Sky"));
}

/* Degenerados: nunca desreferencia NULL, nunca revienta con buffers o
 * limites de tramo en cero. */
static void test_degenerados(void)
{
    char buf[BUFSZ];
    struct metro_textseg segs[8];

    CHECK(metro_textseg_build(NULL, true, buf, sizeof(buf), segs, 8) == 0);
    CHECK(metro_textseg_build("", true, buf, sizeof(buf), segs, 8) == 0);
    CHECK(metro_textseg_build("hola", true, NULL, sizeof(buf), segs, 8) == 0);
    CHECK(metro_textseg_build("hola", true, buf, 0, segs, 8) == 0);
    CHECK(metro_textseg_build("hola", true, buf, sizeof(buf), segs, 0) == 0);
    /* Mismos cuatro con has_cyrillic_font en falso (el otro camino). */
    CHECK(metro_textseg_build(NULL, false, buf, sizeof(buf), segs, 8) == 0);
    CHECK(metro_textseg_build("", false, buf, sizeof(buf), segs, 8) == 0);
}

/* El buffer y el arreglo de tramos mandan: nunca se desborda ninguno
 * de los dos, aunque el texto de entrada de para mas. */
static void test_no_desborda(void)
{
    char buf[8];
    struct metro_textseg segs[8];
    int n, i;

    /* Buffer chico, con fuente cirilica: se trunca sin escribir fuera
     * de el. */
    n = metro_textseg_build("\xd1\x80\xd0\xbe\xd0\xba - live \xd0\xbc\xd1\x83\xd0\xb7\xd1\x8b\xd0\xba\xd0\xb0",
                            true, buf, sizeof(buf), segs, 8);
    CHECK(n >= 1);
    for (i = 0; i < n; i++)
        CHECK(segs[i].text >= buf && segs[i].text < buf + sizeof(buf));

    /* Buffer chico, sin fuente cirilica (el otro camino de arriba). */
    n = metro_textseg_build("Don't Stop Believing", false, buf, sizeof(buf), segs, 8);
    CHECK(n == 1);
    CHECK(segs[0].text >= buf && segs[0].text < buf + sizeof(buf));

    /* max_segs chico: nunca escribe mas de los que se le piden. */
    {
        char buf2[BUFSZ];
        struct metro_textseg segs2[2];

        n = metro_textseg_build(
            "\xd1\x80\xd0\xbe\xd0\xba - live \xd0\xbc\xd1\x83\xd0\xb7\xd1\x8b\xd0\xba\xd0\xb0 - more",
            true, buf2, sizeof(buf2), segs2, 2);
        CHECK(n <= 2);
    }
}

/* M-118 (hallazgo de moonlit, 7b89e2c6): un espacio ASCII es PRIMARY y
 * PARTE la corrida cirilica, asi que el ruso gasta ~2 tramos por
 * palabra. Este test fija que una frase rusa REAL (de metro_lang.c)
 * pasa holgadamente de los 12 tramos que M-114 daba de tope: con
 * aquel limite, metro_draw_text() la dibujaba truncada en silencio.
 *
 * No comprueba un numero exacto de tramos -- comprueba que el orden de
 * magnitud es el que obliga a dimensionar de verdad. Si alguien vuelve
 * a bajar METRO_TEXTSEG_MAX a una decena, esto lo caza. */
static void test_frase_rusa_pasa_de_doce_tramos(void)
{
    char buf[512];
    struct metro_textseg segs[64];
    int n, i, cyr = 0;

    /* "Полный исходный код и текст лицензии находятся здесь" */
    n = metro_textseg_build(
        "\xd0\x9f\xd0\xbe\xd0\xbb\xd0\xbd\xd1\x8b\xd0\xb9"
        "\x20\xd0\xb8\xd1\x81\xd1\x85\xd0\xbe\xd0\xb4\xd0"
        "\xbd\xd1\x8b\xd0\xb9\x20\xd0\xba\xd0\xbe\xd0\xb4"
        "\x20\xd0\xb8\x20\xd1\x82\xd0\xb5\xd0\xba\xd1\x81"
        "\xd1\x82\x20\xd0\xbb\xd0\xb8\xd1\x86\xd0\xb5\xd0"
        "\xbd\xd0\xb7\xd0\xb8\xd0\xb8\x20\xd0\xbd\xd0\xb0"
        "\xd1\x85\xd0\xbe\xd0\xb4\xd1\x8f\xd1\x82\xd1\x81"
        "\xd1\x8f\x20\xd0\xb7\xd0\xb4\xd0\xb5\xd1\x81\xd1"
        "\x8c",
        true, buf, sizeof(buf), segs, 64);

    CHECK(n > 12);

    /* Y que la particion es la esperada: palabras cirilicas separadas
     * por tramos PRIMARY de un espacio. */
    for (i = 0; i < n; i++)
        if (segs[i].kind == METRO_TEXTSEG_CYRILLIC)
            cyr++;
    CHECK(cyr == 8);                 /* ocho palabras rusas */
    CHECK(segs[0].kind == METRO_TEXTSEG_CYRILLIC);
    CHECK(segs[1].kind == METRO_TEXTSEG_PRIMARY);
    CHECK(!strcmp(segs[1].text, " "));
}

int main(void)
{
    test_sin_fuente_cirilica();
    test_sin_fuente_cirilica_ignora_ruso();
    test_ascii_puro_un_tramo();
    test_cirilico_puro_un_tramo();
    test_cirilico_y_latin_alternan();
    test_cirilico_al_borde();
    test_fuera_de_rango_cae_a_primary();
    test_degenerados();
    test_no_desborda();
    test_frase_rusa_pasa_de_doce_tramos();

    printf("test_textseg: %d/%d checks OK\n", checks - failures, checks);
    if (failures)
    {
        printf("%d FALLO(S)\n", failures);
        return 1;
    }
    return 0;
}
