/* Tests host-side de la tabla de familias hermanas (M-093,
 * metro_firmware_families.c): la lista pura de la que sale el submenu
 * "cambiar sistema". Ejecutar con `make -C apps/metro/test`. */
#include <stdio.h>
#include <string.h>
#include "../metro_firmware_families.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

int main(void)
{
    int i, j;
    int n = metro_fw_sibling_count();

    /* Hoy: Aura y moonlit.aura. */
    CHECK(n == 2);

    for (i = 0; i < n; i++)
    {
        const struct metro_fw_family *f = metro_fw_sibling(i);

        CHECK(f != NULL);
        if (f == NULL)
            continue;
        CHECK(f->dormant_dir != NULL);
        /* Nunca uno mismo: cambiar "a Metro" desde Metro no existe. */
        CHECK(strcmp(f->dormant_dir, METRO_FW_OWN_DORMANT) != 0);
        /* Contrato v10: todo dormido vive como /.firmware-<familia>. */
        CHECK(strncmp(f->dormant_dir, "/.firmware-", 11) == 0);
        CHECK(strlen(f->dormant_dir) > 11);
        CHECK(f->name >= 0 && f->name < LANG_COUNT);

        for (j = 0; j < i; j++)
        {
            const struct metro_fw_family *g = metro_fw_sibling(j);
            CHECK(strcmp(f->dormant_dir, g->dormant_dir) != 0);
            CHECK(f->name != g->name);
        }
    }

    /* Fuera de rango: NULL, nunca basura. */
    CHECK(metro_fw_sibling(n) == NULL);
    CHECK(metro_fw_sibling(-1) == NULL);

    printf("%d checks, %d fallos\n", checks, failures);
    return failures ? 1 : 0;
}
