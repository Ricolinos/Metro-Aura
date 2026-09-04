/* Tests host del calculo de desplazamiento de la marquesina (M-106).
 * Solo la parte PURA: metro_marquee_offset_px(). El dibujo necesita
 * LCD y no se prueba aqui. `make -C apps/metro/test`.
 *
 * metro_marquee.c entero no compila en host (arrastra lcd.h, kernel.h,
 * metro_settings.h), asi que el reloj del ciclo vive en su propio
 * modulo puro, metro_marquee_cycle.c, que es lo unico que este test
 * enlaza -- mismo patron que metro_master_art_format.c (M-097).
 *
 * PORTADO de moonlit.aura (D-067, solo lectura) junto con el modulo:
 * si las dos familias comparten la aritmetica, comparten tambien lo que
 * la fija. */
#include <stdio.h>

int metro_marquee_offset_px(long elapsed_ms, int span_px,
                              int static_ms, int scroll_ms);

#define STATIC_MS 2000
#define SCROLL_MS 5000
#define SPAN      300

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

#define CHECK_EQ(got, want) do { \
    int g_ = (got), w_ = (want); \
    checks++; \
    if (g_ != w_) { \
        failures++; \
        printf("FALLO %s:%d: %d != %d\n", __FILE__, __LINE__, g_, w_); \
    } \
} while (0)

/* El tramo quieto es quieto: 2 000 ms sin moverse ni un pixel, para que
 * el texto se pueda leer antes de empezar a desfilar. */
static void test_tramo_quieto(void)
{
    CHECK_EQ(metro_marquee_offset_px(0, SPAN, STATIC_MS, SCROLL_MS), 0);
    CHECK_EQ(metro_marquee_offset_px(1, SPAN, STATIC_MS, SCROLL_MS), 0);
    CHECK_EQ(metro_marquee_offset_px(1999, SPAN, STATIC_MS, SCROLL_MS), 0);
    CHECK_EQ(metro_marquee_offset_px(2000, SPAN, STATIC_MS, SCROLL_MS), 0);
}

/* El barrido es LINEAL y llega exactamente a `span` al final: ahi la
 * segunda copia esta justo donde arranco la primera, que es lo que hace
 * que el bucle no tenga costura. */
static void test_barrido_lineal(void)
{
    CHECK_EQ(metro_marquee_offset_px(2000 + 2500, SPAN, STATIC_MS, SCROLL_MS),
             SPAN / 2);
    CHECK_EQ(metro_marquee_offset_px(2000 + 1250, SPAN, STATIC_MS, SCROLL_MS),
             SPAN / 4);
    CHECK_EQ(metro_marquee_offset_px(2000 + 4999, SPAN, STATIC_MS, SCROLL_MS),
             (SPAN * 4999) / SCROLL_MS);
    /* En el instante 2000+5000 el ciclo ya reinicio: vuelve a 0, que es
     * la MISMA imagen que span. */
    CHECK_EQ(metro_marquee_offset_px(2000 + 5000, SPAN, STATIC_MS, SCROLL_MS), 0);
}

/* El ciclo se repite cada static+scroll, indefinidamente. */
static void test_bucle(void)
{
    long cycle = STATIC_MS + SCROLL_MS;
    int i;

    for (i = 1; i < 5; i++)
    {
        CHECK_EQ(metro_marquee_offset_px(i * cycle, SPAN, STATIC_MS, SCROLL_MS), 0);
        CHECK_EQ(metro_marquee_offset_px(i * cycle + 2000 + 2500, SPAN,
                                            STATIC_MS, SCROLL_MS), SPAN / 2);
    }
    /* Un tiempo grande no desborda ni cambia de signo. */
    CHECK(metro_marquee_offset_px(1000L * 60 * 60 * 24, SPAN,
                                     STATIC_MS, SCROLL_MS) >= 0);
    CHECK(metro_marquee_offset_px(1000L * 60 * 60 * 24, SPAN,
                                     STATIC_MS, SCROLL_MS) <= SPAN);
}

/* Nunca sale del rango [0, span]: un desplazamiento mayor dejaria un
 * hueco visible entre las dos copias. */
static void test_rango(void)
{
    long t;

    for (t = 0; t < 3L * (STATIC_MS + SCROLL_MS); t += 37)
    {
        int o = metro_marquee_offset_px(t, SPAN, STATIC_MS, SCROLL_MS);
        CHECK(o >= 0 && o <= SPAN);
    }
}

/* Degenerados: nada que desplazar, o duraciones invalidas -> 0, nunca
 * una division por cero. */
static void test_degenerados(void)
{
    CHECK_EQ(metro_marquee_offset_px(5000, 0, STATIC_MS, SCROLL_MS), 0);
    CHECK_EQ(metro_marquee_offset_px(5000, -10, STATIC_MS, SCROLL_MS), 0);
    CHECK_EQ(metro_marquee_offset_px(5000, SPAN, STATIC_MS, 0), 0);
    CHECK_EQ(metro_marquee_offset_px(5000, SPAN, -1, SCROLL_MS), 0);
    /* Un reloj que retrocede (elapsed negativo) no invierte el barrido. */
    CHECK(metro_marquee_offset_px(-1, SPAN, STATIC_MS, SCROLL_MS) >= 0);
}

int main(void)
{
    test_tramo_quieto();
    test_barrido_lineal();
    test_bucle();
    test_rango();
    test_degenerados();

    printf("test_marquee: %d/%d checks OK\n", checks - failures, checks);
    if (failures)
    {
        printf("%d FALLO(S)\n", failures);
        return 1;
    }
    return 0;
}
