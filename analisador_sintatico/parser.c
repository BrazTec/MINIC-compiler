/*
 * Executable launcher for the MINIC parser.
 *
 * The Python parser is the single implementation of the grammar and AST
 * formatting. This C entry point keeps the parser.exe command-line contract
 * while forwarding the source path and process result unchanged.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define PATH_CAP 32768

static int quote_argument(const char *value, char *out, size_t out_size) {
    size_t used = 0;
    int backslashes = 0;

    if (out_size < 3) {
        return 0;
    }
    out[used++] = '"';
    for (; *value != '\0'; value++) {
        if (*value == '\\') {
            backslashes++;
            continue;
        }
        if (*value == '"') {
            for (int i = 0; i < backslashes * 2 + 1; i++) {
                if (used + 1 >= out_size) return 0;
                out[used++] = '\\';
            }
            if (used + 1 >= out_size) return 0;
            out[used++] = '"';
            backslashes = 0;
            continue;
        }
        for (int i = 0; i < backslashes; i++) {
            if (used + 1 >= out_size) return 0;
            out[used++] = '\\';
        }
        backslashes = 0;
        if (used + 1 >= out_size) return 0;
        out[used++] = *value;
    }
    for (int i = 0; i < backslashes * 2; i++) {
        if (used + 1 >= out_size) return 0;
        out[used++] = '\\';
    }
    out[used++] = '"';
    out[used] = '\0';
    return 1;
}

int main(int argc, char **argv) {
    char module_path[PATH_CAP];
    char parser_path[PATH_CAP];
    char quoted_parser[PATH_CAP];
    char quoted_source[PATH_CAP];
    char source_path[PATH_CAP];
    char command[PATH_CAP * 2];
    DWORD length;
    int status;

    if (argc != 2) {
        fprintf(stderr, "uso: parser.exe caminho\\para\\codigo.c\n");
        return 2;
    }

    if (GetFullPathNameA(argv[1], sizeof(source_path), source_path, NULL) == 0) {
        fprintf(stderr, "Erro ao resolver o caminho do arquivo\n");
        return 2;
    }

    length = GetModuleFileNameA(NULL, module_path, sizeof(module_path));
    if (length == 0 || length >= sizeof(module_path)) {
        fprintf(stderr, "Erro ao localizar parser.py\n");
        return 2;
    }

    memcpy(parser_path, module_path, length + 1);
    {
        char *separator = strrchr(parser_path, '\\');
        if (separator == NULL) {
            separator = strrchr(parser_path, '/');
        }
        if (separator == NULL || (size_t)(separator - parser_path) + 11 >= sizeof(parser_path)) {
            fprintf(stderr, "Erro ao localizar parser.py\n");
            return 2;
        }
        separator[1] = '\0';
        strcat(parser_path, "parser.py");
    }

    if (!quote_argument(parser_path, quoted_parser, sizeof(quoted_parser)) ||
        !quote_argument(source_path, quoted_source, sizeof(quoted_source))) {
        fprintf(stderr, "Caminho de arquivo muito longo\n");
        return 2;
    }

    snprintf(command, sizeof(command), "python %s %s", quoted_parser, quoted_source);
    status = system(command);
    if (status == -1) {
        fprintf(stderr, "Erro ao executar o parser Python\n");
        return 1;
    }
    if (status >= 0 && (status & 0xFF) == 0) {
        return (status >> 8) & 0xFF;
    }
    return status;
}
