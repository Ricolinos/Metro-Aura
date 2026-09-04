/* Tests host-side de metro_lang_initial() (R4/FA-5a, M-076): sacar el
 * PRIMER CARÁCTER de una cadena UTF-8, no su primer byte.
 * Ejecutar con `make -C apps/metro/test`. */
#include <stdio.h>
#include <string.h>
#include "../metro_lang.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

#define CHECK_INITIAL(input, expected) do { \
    char buf[5]; \
    metro_lang_initial((input), buf, sizeof(buf)); \
    checks++; \
    if (strcmp(buf, (expected)) != 0) { \
        failures++; \
        printf("FALLO %s:%d: initial(\"%s\") = \"%s\", esperado \"%s\"\n", \
               __FILE__, __LINE__, (input), buf, (expected)); \
    } \
} while (0)

static void test_ascii(void)
{
    CHECK_INITIAL("canciones", "C");
    CHECK_INITIAL("Canciones", "C");
    CHECK_INITIAL("2 Unlimited", "2");
    CHECK_INITIAL("_borrador", "_");
}

/* El caso que motivó todo esto: una letra acentuada ocupa DOS bytes,
 * así que `label[0]` entregaba medio carácter. */
static void test_acentos(void)
{
    CHECK_INITIAL("álbum desconocido", "Á");
    CHECK_INITIAL("Ángela", "Á");
    CHECK_INITIAL("éxitos", "É");
    CHECK_INITIAL("índice", "Í");
    CHECK_INITIAL("ópera", "Ó");
    CHECK_INITIAL("último", "Ú");
    CHECK_INITIAL("ñu", "Ñ");
    /* Ya en mayúscula: se conserva tal cual. */
    CHECK_INITIAL("Éxitos", "É");
    CHECK_INITIAL("Ñandú", "Ñ");
}

/* No todo 0xC3 xx es una letra: hay que no "mayusculizar" lo que no lo
 * es, ni salirse de Latin-1. */
static void test_no_letras_latin1(void)
{
    CHECK_INITIAL("÷ dividir", "÷");   /* 0xC3 0xB7, signo, no letra */
    CHECK_INITIAL("ÿ rara", "ÿ");      /* su mayúscula Ÿ no está en Latin-1 */
    CHECK_INITIAL("© 2026", "©");      /* 0xC2 xx, otro bloque */
}

/* Multibyte de 3 y 4 bytes: se copia el carácter entero aunque no haya
 * regla de mayúscula que aplicarle. */
static void test_multibyte_largo(void)
{
    CHECK_INITIAL("東京", "東");        /* 3 bytes */
    CHECK_INITIAL("😀 emoji", "😀");    /* 4 bytes */
}

/* Entradas degeneradas: nunca leer de más ni desreferenciar NULL. */
static void test_degenerado(void)
{
    char buf[5];

    CHECK_INITIAL("", "");

    metro_lang_initial(NULL, buf, sizeof(buf));
    CHECK(buf[0] == '\0');

    /* Secuencia truncada: byte guía de 2 bytes sin su continuación. */
    metro_lang_initial("\xC3", buf, sizeof(buf));
    CHECK(buf[0] == '\xC3' && buf[1] == '\0');

    /* Byte de continuación suelto: se trata como 1 byte, sin leer más. */
    metro_lang_initial("\xA1x", buf, sizeof(buf));
    CHECK(buf[0] == '\xA1' && buf[1] == '\0');

    /* Buffer que no alcanza para el carácter completo: trunca, pero
     * siempre termina en NUL y nunca escribe fuera. */
    metro_lang_initial("álbum", buf, 2);
    CHECK(buf[1] == '\0');

    /* outsz 0 no debe escribir nada -- centinela alrededor. */
    buf[0] = 'Z';
    metro_lang_initial("hola", buf, 0);
    CHECK(buf[0] == 'Z');
}

/* R4 (M-079): ordenamiento con acentos plegados. */
#define LT(a, b) do { \
    checks++; \
    if (!(metro_lang_collate((a), (b)) < 0)) { \
        failures++; \
        printf("FALLO %s:%d: esperaba \"%s\" < \"%s\"\n", \
               __FILE__, __LINE__, (a), (b)); \
    } \
} while (0)

static void test_collate(void)
{
    /* El bug que motivó esto: una inicial acentuada caía tras la Z. */
    LT("Ángela", "Beto");
    LT("Ángela", "Zoé");
    LT("Andrés", "Ángela");     /* And < Áng: la 'd' pliega antes que 'g' */
    LT("Ángela", "Antonio");    /* Áng < Ant */
    LT("Éxitos", "Fuego");
    LT("Último", "Vals");

    /* La caja no cambia DÓNDE cae una etiqueta: ambas formas aterrizan
     * en el mismo sitio respecto al resto. */
    LT("abba", "Beto");
    LT("ABBA", "Beto");
    LT("abba", "Zzz");
    /* ...pero sí desempatan entre ellas, de forma determinista: 0 solo
     * para cadenas idénticas byte a byte. */
    checks++;
    if (metro_lang_collate("abba", "ABBA") == 0)
    {
        failures++;
        printf("FALLO %s:%d: abba/ABBA deberían desempatar\n",
               __FILE__, __LINE__);
    }
    checks++;
    if (metro_lang_collate("abba", "abba") != 0)
    {
        failures++;
        printf("FALLO %s:%d: cadenas idénticas deben dar 0\n",
               __FILE__, __LINE__);
    }

    /* La ñ es letra propia: va DESPUÉS de toda la N y ANTES de la O. */
    LT("Nuevo", "Ñu");
    LT("Ñu", "Oasis");
    LT("Nz", "Ñu");             /* incluso tras la última n+consonante */

    /* Los dígitos siguen antes que las letras. */
    LT("2 Unlimited", "Abba");

    /* Empate al plegar: desempata determinista, sin quedar "igual". */
    checks++;
    if (metro_lang_collate("Angela", "Ángela") == 0)
    {
        failures++;
        printf("FALLO %s:%d: Angela/Ángela deberían desempatar\n",
               __FILE__, __LINE__);
    }

    /* Prefijo: lo más corto va primero. */
    LT("Sol", "Solar");

    /* Degenerados: no debe reventar. */
    checks++;
    if (metro_lang_collate("", "") != 0) { failures++; printf("FALLO vacias\n"); }
    LT("", "a");
}

/* R5-F3 (M-083): metro_lang_upper -- la línea de artista del
 * reproductor va en mayúsculas. */
static void test_upper(void)
{
    char out[64];

    metro_lang_upper("cultura profética", out, sizeof(out));
    CHECK(strcmp(out, "CULTURA PROFÉTICA") == 0);

    metro_lang_upper("m.o.t.a", out, sizeof(out));
    CHECK(strcmp(out, "M.O.T.A") == 0);

    /* ñ y ü suben; dígitos, signos y ya-mayúsculas quedan igual. */
    metro_lang_upper("Año 2 - ñandú/ü", out, sizeof(out));
    CHECK(strcmp(out, "AÑO 2 - ÑANDÚ/Ü") == 0);

    /* Un carácter fuera de Latin-1 (€, 3 bytes) se copia intacto. */
    metro_lang_upper("a€b", out, sizeof(out));
    CHECK(strcmp(out, "A€B") == 0);

    /* Truncado en frontera de carácter: "áb" no cabe entero en 3 bytes
     * (á son 2 + NUL), así que sale "Á" y nunca medio "b" ni media á. */
    metro_lang_upper("áb", out, 3);
    CHECK(strcmp(out, "Á") == 0);
    metro_lang_upper("xá", out, 3);
    CHECK(strcmp(out, "X") == 0);

    metro_lang_upper("", out, sizeof(out));
    CHECK(out[0] == '\0');
    metro_lang_upper(NULL, out, sizeof(out));
    CHECK(out[0] == '\0');
}

/* M-110/M-111 (contrato v19 SS A.1): el codigo de dos letras que
 * /.aura/settings.cfg usa para `language`. */
static void test_code(void)
{
    enum metro_language lang;

    CHECK(metro_lang_from_code("es", &lang) && lang == METRO_LANG_ES);
    CHECK(metro_lang_from_code("en", &lang) && lang == METRO_LANG_EN);
    /* M-111: los seis del contrato ya estan implementados. */
    CHECK(metro_lang_from_code("fr", &lang) && lang == METRO_LANG_FR);
    CHECK(metro_lang_from_code("de", &lang) && lang == METRO_LANG_DE);
    CHECK(metro_lang_from_code("ru", &lang) && lang == METRO_LANG_RU);
    CHECK(metro_lang_from_code("it", &lang) && lang == METRO_LANG_IT);
    CHECK(!metro_lang_from_code("xx", &lang));

    CHECK(!strcmp(metro_lang_code(METRO_LANG_ES), "es"));
    CHECK(!strcmp(metro_lang_code(METRO_LANG_EN), "en"));
    CHECK(!strcmp(metro_lang_code(METRO_LANG_FR), "fr"));
    CHECK(!strcmp(metro_lang_code(METRO_LANG_DE), "de"));
    CHECK(!strcmp(metro_lang_code(METRO_LANG_RU), "ru"));
    CHECK(!strcmp(metro_lang_code(METRO_LANG_IT), "it"));
}

/* M-111: el nombre nativo del selector -- fijo, nunca traducido. */
static void test_native_name(void)
{
    CHECK(!strcmp(metro_lang_native_name(METRO_LANG_ES), "Español"));
    CHECK(!strcmp(metro_lang_native_name(METRO_LANG_EN), "English"));
    CHECK(!strcmp(metro_lang_native_name(METRO_LANG_FR), "Français"));
    CHECK(!strcmp(metro_lang_native_name(METRO_LANG_DE), "Deutsch"));
    CHECK(!strcmp(metro_lang_native_name(METRO_LANG_RU), "Русский"));
    CHECK(!strcmp(metro_lang_native_name(METRO_LANG_IT), "Italiano"));
}

/* M-111: metro_lang_str() debe devolver texto real (no "") para las
 * 137 claves en los seis idiomas -- una tabla con un hueco (una clave
 * olvidada al traducir) se ve exactamente como una cadena vacia en la
 * UI, facil de no notar a simple vista. */
static void test_all_languages_complete(void)
{
    enum metro_language lang;

    for (lang = 0; lang < METRO_LANG_COUNT; lang++)
    {
        enum metro_lang_id id;

        metro_lang_set(lang);
        for (id = 0; id < LANG_COUNT; id++)
        {
            checks++;
            if (metro_lang_str(id)[0] == '\0')
            {
                failures++;
                printf("FALLO %s:%d: idioma %d, LANG id %d vacio\n",
                       __FILE__, __LINE__, (int)lang, (int)id);
            }
        }
    }
    metro_lang_set(METRO_LANG_ES);
}

/* M-111: mayusculas cirilicas (metro_lang_upper(), usada en la linea
 * de artista de Ahora Suena) -- а..п con el mismo desplazamiento que
 * Latin-1, р..я cruzando el guia UTF-8, y la excepcion real ё->Ё. */
static void test_cyrillic_upper(void)
{
    char out[64];

    metro_lang_upper("музыка", out, sizeof(out));
    CHECK(!strcmp(out, "МУЗЫКА"));

    metro_lang_upper("пётр чайковский", out, sizeof(out));
    CHECK(!strcmp(out, "ПЁТР ЧАЙКОВСКИЙ"));

    /* Toda la fila р..я, para no dejar sin cubrir ninguna carta al
     * otro lado del corte 0xD0/0xD1. */
    metro_lang_upper("рстуфхцчшщъыьэюя", out, sizeof(out));
    CHECK(!strcmp(out, "РСТУФХЦЧШЩЪЫЬЭЮЯ"));

    /* Ya en mayuscula: se conserva tal cual. */
    metro_lang_upper("МОСКВА", out, sizeof(out));
    CHECK(!strcmp(out, "МОСКВА"));
}

/* M-111: orden cirilico -- alfabeto correcto, у antes de ф antes de я,
 * y que no se cruce con el bloque Latin-1/ASCII. */
static void test_cyrillic_collate(void)
{
    LT("Андрей", "Борис");
    LT("Ёлка", "Жасмин");        /* Ё pliega justo antes de Ж */
    LT("Чайковский", "Шостакович");
    LT("Юрий", "Я");
    /* Mayuscula/minuscula no cambia el orden relativo frente a otra
     * palabra, igual que ya vale para Latin-1. */
    LT("андрей", "Борис");
    /* Cirilico ordena despues de Latin-1/ASCII -- un catalogo mixto
     * (ver gen_test_media.sh) no intercala mal las dos escrituras. */
    LT("Zoé", "Андрей");
    LT("2 Unlimited", "Андрей");
}

int main(void)
{
    test_ascii();
    test_acentos();
    test_no_letras_latin1();
    test_multibyte_largo();
    test_degenerado();
    test_collate();
    test_upper();
    test_code();
    test_native_name();
    test_all_languages_complete();
    test_cyrillic_upper();
    test_cyrillic_collate();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
