/* Tests host-side de /.aura/settings.cfg (M-110, plan maestro SS A):
 * los tres casos del vector A.3, literales. Ejecutar con
 * `make -C apps/metro/test`. Parte lineas "clave: valor" a mano (no
 * settings_parseline(), que es del target) -- el contrato de
 * metro_shared_settings_parse_field() es sobre nombre/valor ya
 * partidos, sea cual sea quien los partio. */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "../metro_shared_settings.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

/* Imita el recorte de settings_parseline(): "clave: valor" -> dos
 * cadenas sin el ':' ni espacios alrededor. `line` se modifica in
 * place, igual que la real. */
static bool split(char *line, char **name, char **value)
{
    char *colon = strchr(line, ':');
    char *v;

    if (!colon)
        return false;
    *colon = '\0';
    *name = line;
    v = colon + 1;
    while (*v == ' ')
        v++;
    *value = v;
    return true;
}

static const char *const vector_a3[] = {
    "# aura-shared-settings v1",
    "rev: 7",
    "updated_by: metro",
    "screen_lock_enabled: 1",
    "screen_lock_pin: 0427",
    "screen_lock_require: 1min",
    "brightness: 32",
    "backlight_timeout: 10",
    "idle_poweroff: 20",
    "keyclick: 1",
    "volume_limit: -6",
    "replaygain: album",
    "language: fr",
    "appearance: light",
    "clave_futura: lo que sea",
};
#define VECTOR_A3_N (int)(sizeof(vector_a3) / sizeof(vector_a3[0]))

/* Caso 1: el vector A.3 completo, tal cual. */
static void test_vector_valid(void)
{
    metro_shared_settings_t s;
    char line[64];
    int i;
    int unknown = 0;

    metro_shared_settings_defaults(&s);
    CHECK(metro_shared_settings_is_header(vector_a3[0]));

    for (i = 1; i < VECTOR_A3_N; i++)
    {
        char *name, *value;
        strcpy(line, vector_a3[i]);
        CHECK(split(line, &name, &value));
        if (!metro_shared_settings_parse_field(&s, name, value))
            unknown++;
    }

    /* 13 claves conocidas, 1 desconocida (clave_futura) -- A.3. */
    CHECK(unknown == 1);
    CHECK(s.rev == 7);
    CHECK(s.updated_by == METRO_SHARED_BY_METRO);
    CHECK(s.screen_lock_enabled == true);
    CHECK(!strcmp(s.screen_lock_pin, "0427"));
    CHECK(s.screen_lock_require == METRO_LOCK_REQUIRE_1MIN);
    CHECK(s.brightness == 32);
    CHECK(s.backlight_timeout == 10);
    CHECK(s.idle_poweroff == 20);
    CHECK(s.keyclick == true);
    CHECK(s.volume_limit == -6);
    CHECK(s.replaygain == METRO_SHARED_RG_ALBUM);
    CHECK(!strcmp(s.language, "fr"));
    CHECK(s.appearance == METRO_THEME_LIGHT);

    /* Reescritura: las 13 claves conocidas vuelven a salir (el
     * llamador real es quien concatena la linea cruda de
     * clave_futura aparte -- este modulo no la guarda, esa es tarea
     * de metro_settings.c). rev/appearance/screen_lock_pin
     * redondean exactos. */
    {
        char buf[64];
        size_t n;

        n = metro_shared_settings_format_field(&s, 0, buf, sizeof(buf));
        CHECK(n > 0 && !strcmp(buf, "rev: 7\n"));

        n = metro_shared_settings_format_field(&s, 3, buf, sizeof(buf));
        CHECK(n > 0 && !strcmp(buf, "screen_lock_pin: 0427\n"));

        n = metro_shared_settings_format_field(&s, 12, buf, sizeof(buf));
        CHECK(n > 0 && !strcmp(buf, "appearance: light\n"));
    }
}

/* Caso 2: sin cabecera -> el llamador debe rechazar el archivo
 * ENTERO (A.2.5) antes de parsear nada. is_header() es lo que decide
 * eso; aqui solo se prueba que devuelve false para cualquier primera
 * linea que no sea la cabecera exacta -- incluida una que YA es una
 * clave valida (rev: 7), que es justo el caso "el archivo empieza
 * directo con datos, sin cabecera". */
static void test_vector_no_header(void)
{
    CHECK(!metro_shared_settings_is_header("rev: 7"));
    CHECK(!metro_shared_settings_is_header(""));
    CHECK(!metro_shared_settings_is_header("# aura-shared-settings v2"));
}

/* Caso 3: brightness: 999 -> solo esa clave se ignora, el resto del
 * archivo sigue vivo (A.2.2). */
static void test_vector_bad_brightness(void)
{
    metro_shared_settings_t s;
    char line[64];
    char *name, *value;

    metro_shared_settings_defaults(&s);

    strcpy(line, "rev: 3");
    split(line, &name, &value);
    CHECK(metro_shared_settings_parse_field(&s, name, value));

    strcpy(line, "brightness: 999");
    split(line, &name, &value);
    /* La clave SE RECONOCE (devuelve true -- no es una linea
     * desconocida que haya que preservar cruda), pero el valor no
     * entra en el rango de sanidad y se ignora: el campo se queda en
     * su default (0), nunca en 999. */
    CHECK(metro_shared_settings_parse_field(&s, name, value));
    CHECK(s.brightness == 0);

    strcpy(line, "idle_poweroff: 45");
    split(line, &name, &value);
    CHECK(metro_shared_settings_parse_field(&s, name, value));

    /* rev e idle_poweroff, las claves buenas antes y despues de la
     * mala, se aplicaron -- el archivo no se abortó entero. */
    CHECK(s.rev == 3);
    CHECK(s.idle_poweroff == 45);
}

/* El candado sin activar no debe escribir su PIN ni su "cuando
 * pedirlo" -- mismo criterio que aura.cfg (M-068), ahora en A.1. */
static void test_lock_fields_omitted_when_off(void)
{
    metro_shared_settings_t s;
    char buf[64];

    metro_shared_settings_defaults(&s);
    s.screen_lock_enabled = false;

    CHECK(metro_shared_settings_format_field(&s, 3, buf, sizeof(buf)) == 0);
    CHECK(metro_shared_settings_format_field(&s, 4, buf, sizeof(buf)) == 0);
}

int main(void)
{
    test_vector_valid();
    test_vector_no_header();
    test_vector_bad_brightness();
    test_lock_fields_omitted_when_off();

    printf("%d checks, %d fallos\n", checks, failures);
    return failures ? 1 : 0;
}
