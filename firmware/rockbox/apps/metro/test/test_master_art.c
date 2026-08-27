/* Tests host-side de metro_master_art_format.c (M-097, contrato v16):
 * claves, cabecera 'MAST', filtro de caja 130->80 y recorte "cubrir".
 * Sin dependencias de Rockbox. Ejecutar con `make -C apps/metro/test`. */
#include <stdio.h>
#include <string.h>
#include "../metro_master_art_format.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

/* Vectores CRC-32/MPEG-2 calculados aparte (python, bit a bit):
 * es el mismo algoritmo que crc_32(buf, len, 0xffffffff) de Rockbox,
 * el que metro_music_album_art_key() ya usa desde M-096. */
static void test_crc32_matches_rockbox(void)
{
    const char *p = "/Music/Artist/Album/01 Song.mp3";
    CHECK(metro_master_art_crc32(p, strlen(p)) == 0x15ec085cu);
    p = "/Photos/beach.jpg";
    CHECK(metro_master_art_crc32(p, strlen(p)) == 0xbe6c7e93u);
    CHECK(metro_master_art_crc32("", 0) == 0xffffffffu);
    CHECK(metro_master_art_crc32("a", 1) == 0xe66c6494u);
}

static void test_key_format(void)
{
    char key[METRO_MASTER_ART_KEY_LEN];
    metro_master_art_format_key('a', 0x031b464bu, 1787198376L, key, sizeof(key));
    CHECK(!strcmp(key, "a-031b464b.1787198376"));
    metro_master_art_format_key('p', 0xbe6c7e93u, 42L, key, sizeof(key));
    CHECK(!strcmp(key, "p-be6c7e93.42"));
    metro_master_art_format_key('r', 0u, 7L, key, sizeof(key));
    CHECK(!strcmp(key, "r-00000000.7"));
}

static void test_px_for_subdir(void)
{
    CHECK(metro_master_art_px_for_subdir("albums") == 130);
    CHECK(metro_master_art_px_for_subdir("artists") == 130);
    CHECK(metro_master_art_px_for_subdir("photos") == 80);
    CHECK(metro_master_art_px_for_subdir("videos") == 0);
}

static void test_header_roundtrip(void)
{
    uint8_t hdr[METRO_MASTER_ART_HEADER_SIZE];
    uint16_t w = 0, h = 0;

    metro_master_art_header_pack(hdr, 130, 130);
    /* 'MAST' en little-endian: bytes M A S T */
    CHECK(hdr[0] == 'M' && hdr[1] == 'A' && hdr[2] == 'S' && hdr[3] == 'T');
    CHECK(hdr[4] == 130 && hdr[5] == 0 && hdr[6] == 130 && hdr[7] == 0);
    CHECK(memcmp(hdr + 8, "\0\0\0\0\0\0\0\0", 8) == 0);
    CHECK(metro_master_art_header_parse(hdr, &w, &h));
    CHECK(w == 130 && h == 130);

    metro_master_art_header_pack(hdr, 80, 80);
    CHECK(metro_master_art_header_parse(hdr, &w, &h) && w == 80 && h == 80);

    hdr[0] = 'X';
    CHECK(!metro_master_art_header_parse(hdr, &w, &h));

    metro_master_art_header_pack(hdr, 0, 80);
    CHECK(!metro_master_art_header_parse(hdr, &w, &h));

    /* flags/reserved distintos de 0 no invalidan (compatibilidad hacia
     * adelante: otra familia podria estrenar una bandera). */
    metro_master_art_header_pack(hdr, 130, 130);
    hdr[8] = 1;
    CHECK(metro_master_art_header_parse(hdr, &w, &h));
}

static uint16_t src[130 * 130];
static uint16_t dst[320 * 240];

static void test_box_down_uniform_and_average(void)
{
    int i;
    /* Uniforme: el promedio de un color es ese color. */
    for (i = 0; i < 130 * 130; i++) src[i] = 0xF800; /* rojo puro */
    metro_master_art_box_down(src, 130, dst, 80);
    for (i = 0; i < 80 * 80; i++) if (dst[i] != 0xF800) break;
    CHECK(i == 80 * 80);

    /* 2x2 -> 1x1: promedio real por canal. rojo 31 y rojo 0 -> 16 (redondeo). */
    src[0] = 0xF800; src[1] = 0x0000; src[2] = 0xF800; src[3] = 0x0000;
    metro_master_art_box_down(src, 2, dst, 1);
    CHECK(((dst[0] >> 11) & 31) == 16 && (dst[0] & 0x07ff) == 0);

    /* Identidad cuando dpx == spx. */
    for (i = 0; i < 80 * 80; i++) src[i] = (uint16_t)(i * 7);
    metro_master_art_box_down(src, 80, dst, 80);
    CHECK(memcmp(src, dst, 80 * 80 * 2) == 0);
}

static void test_cover_square_identity_and_crop(void)
{
    int x, y;
    /* Cuadrado 130 -> 130: identidad. */
    for (y = 0; y < 130; y++) for (x = 0; x < 130; x++) src[y * 130 + x] = (uint16_t)(y * 131 + x);
    metro_master_art_cover(src, 130, 130, dst, 130, 130);
    CHECK(memcmp(src, dst, 130 * 130 * 2) == 0);

    /* Apaisado 130x100 -> 130x130: se escala la altura (100->130) y se
     * recorta el ancho centrado: la primera columna del destino cae
     * a 15 px del borde izquierdo de la fuente, la ultima a 15 del derecho. */
    for (y = 0; y < 100; y++) for (x = 0; x < 130; x++) src[y * 130 + x] = (uint16_t)x;
    metro_master_art_cover(src, 130, 100, dst, 130, 130);
    CHECK(dst[0] == 15);
    CHECK(dst[129] == 114);
    CHECK(dst[129 * 130 + 0] == 15);

    /* Vertical 100x130 -> 130x130: se recorta la altura. */
    for (y = 0; y < 130; y++) for (x = 0; x < 100; x++) src[y * 100 + x] = (uint16_t)y;
    metro_master_art_cover(src, 100, 130, dst, 130, 130);
    CHECK(dst[0] == 15);
    CHECK(dst[129 * 130] == 114);
    CHECK(dst[129] == 15);
}

static void test_cover_bilinear_flat_and_bounds(void)
{
    int i;
    for (i = 0; i < 130 * 130; i++) src[i] = 0x07E0; /* verde puro */
    metro_master_art_cover_bilinear(src, 130, 130, dst, 320, 240);
    for (i = 0; i < 320 * 240; i++) if (dst[i] != 0x07E0) break;
    CHECK(i == 320 * 240);

    /* Gradiente: la interpolacion queda entre los extremos y es monotona
     * en x en la fila del medio. */
    for (i = 0; i < 130 * 130; i++) src[i] = (uint16_t)(((i % 130) * 31 / 129) << 11);
    metro_master_art_cover_bilinear(src, 130, 130, dst, 320, 240);
    {
        int prev = -1, ok = 1, x;
        for (x = 0; x < 320; x++)
        {
            int r = (dst[120 * 320 + x] >> 11) & 31;
            if (r < prev) ok = 0;
            prev = r;
        }
        CHECK(ok);
        CHECK(((dst[120 * 320 + 319] >> 11) & 31) >= 28);
        CHECK(((dst[120 * 320 + 0] >> 11) & 31) <= 3);
    }
}

int main(void)
{
    test_crc32_matches_rockbox();
    test_key_format();
    test_px_for_subdir();
    test_header_roundtrip();
    test_box_down_uniform_and_average();
    test_cover_square_identity_and_crop();
    test_cover_bilinear_flat_and_bounds();
    printf("%d checks, %d fallos\n", checks, failures);
    return failures ? 1 : 0;
}
