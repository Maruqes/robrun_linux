/* loadtest.c - verify a proxy DLL publishes every expected export.
 *
 * Needs no game: loads the DLL, resolves every EOS_* name listed in a .def
 * file, and reports the ones that are missing.
 *
 * usage: loadtest.exe <dll> <def>
 * exit : 0 all present, 1 missing names, 2 load failure or bad arguments
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

#define MAX_NAME 256

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: loadtest <dll> <def>\n");
        return 2;
    }

    HMODULE dll = LoadLibraryA(argv[1]);
    if (!dll) {
        fprintf(stderr, "LoadLibraryA(%s) failed: %lu\n", argv[1], GetLastError());
        return 2;
    }

    FILE* def = fopen(argv[2], "r");
    if (!def) {
        fprintf(stderr, "cannot open %s\n", argv[2]);
        return 2;
    }

    int checked = 0, missing = 0;
    char line[512];
    while (fgets(line, sizeof line, def)) {
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "EOS_", 4) != 0) continue;

        char* end = p;
        while (*end && *end != '\r' && *end != '\n' && *end != ' ' && *end != '=') end++;
        *end = '\0';
        if (end == p || (size_t)(end - p) >= MAX_NAME) continue;

        checked++;
        if (!GetProcAddress(dll, p)) {
            printf("MISSING %s\n", p);
            missing++;
        }
    }
    fclose(def);

    printf("checked=%d missing=%d\n", checked, missing);
    return missing ? 1 : 0;
}
