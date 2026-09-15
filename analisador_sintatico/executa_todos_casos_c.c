#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <direct.h>

#define MAX_PATH_LEN 4096

static void append_case_report(FILE *out, const char *case_dir, const char *code_path) {
    char command[MAX_PATH_LEN * 2];
    char output_buffer[8192];
    FILE *pipe;

    snprintf(command, sizeof(command), ".\\parser.exe \"%s\" 2>&1", code_path);

    fprintf(out, "## Caso: %s\n\n", case_dir);
    fprintf(out, "**Comando usado para rodar:**\n");
    fprintf(out, "```bash\n%s\n```\n\n", command);
    fprintf(out, "**Saída:**\n");
    fprintf(out, "```text\n");

    pipe = _popen(command, "r");
    if (pipe == NULL) {
        fprintf(out, "(erro ao executar o parser)\n");
        fprintf(out, "```\n\n");
        return;
    }

    while (fgets(output_buffer, sizeof(output_buffer), pipe) != NULL) {
        fputs(output_buffer, out);
    }

    fprintf(out, "```\n\n");
    _pclose(pipe);
}

int main(void) {
    char base_dir[MAX_PATH_LEN];
    char search_pattern[MAX_PATH_LEN];
    char output_path[MAX_PATH_LEN];
    struct _finddata_t entry;
    long handle;
    FILE *out;

    if (_getcwd(base_dir, sizeof(base_dir)) == NULL) {
        fprintf(stderr, "Erro ao obter o diretorio atual.\n");
        return 1;
    }

    snprintf(output_path, sizeof(output_path), "%s\\executa_todos_casos_c.md", base_dir);
    out = fopen(output_path, "w");
    if (out == NULL) {
        fprintf(stderr, "Erro ao criar o arquivo: %s\n", output_path);
        return 1;
    }

    fprintf(out, "# Resultados dos casos do parser\n\n");

    snprintf(search_pattern, sizeof(search_pattern), "%s\\testes-parser-50\\casos\\*", base_dir);
    handle = _findfirst(search_pattern, &entry);

    if (handle == -1L) {
        fprintf(stderr, "Diretório de casos nao encontrado: %s\\testes-parser-50\\casos\n", base_dir);
        fclose(out);
        return 1;
    }

    do {
        char case_dir_name[256];
        char case_dir_path[MAX_PATH_LEN];
        char code_path[MAX_PATH_LEN];

        if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
            continue;
        }

        if ((entry.attrib & _A_SUBDIR) == 0) {
            continue;
        }

        snprintf(case_dir_name, sizeof(case_dir_name), "%s", entry.name);
        snprintf(case_dir_path, sizeof(case_dir_path), "%s\\testes-parser-50\\casos\\%s", base_dir, case_dir_name);
        snprintf(code_path, sizeof(code_path), "%s\\codigo.c", case_dir_path);

        if (_access(code_path, 0) != 0) {
            continue;
        }

        append_case_report(out, case_dir_name, code_path);
    } while (_findnext(handle, &entry) == 0);

    _findclose(handle);
    fclose(out);

    printf("Relatorio salvo em: %s\n", output_path);
    return 0;
}
