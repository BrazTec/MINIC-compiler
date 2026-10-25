/*
 * minic.c - front-end MINIC
 *
 * Fluxo: scanner -> parser -> analisador semantico.
 *
 * O script da disciplina compila somente este arquivo. Para preservar
 * os componentes em arquivos separados e, ao mesmo tempo, atender ao
 * contrato de compilacao, parser.c e semantic.c sao incorporados nesta
 * unidade de traducao. O main independente de parser.c fica desabilitado.
 */

#define MINIC_EMBEDDED
#include "parser.c"
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"
#include "semantic.c"
#include "ir.c"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char **argv)
{
    char *source;

#ifdef _WIN32
    /*
     * Os gabaritos semânticos usam LF (\n) entre as linhas e não possuem
     * quebra de linha ao final. Em executáveis nativos do Windows, stdout em
     * modo texto pode converter LF para CRLF. O modo binário impede essa
     * tradução e preserva exatamente os bytes exigidos pelos gabaritos.
     */
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    /* parser.c mantém o serializador para a execução independente da Etapa 2.
       A referência evita warning de função estática não utilizada nesta unidade. */
    (void)&serialize_ast;
    Lexer lexer;
    Parser parser;
    ASTNode *ast;
    SemanticAnalyzerC analyzer;
    int status;

    bool emit_ir = false;
    int optimization = 0;
    const char *filename = NULL;
    int i;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--ir") == 0) emit_ir = true;
        else if (strcmp(argv[i], "-O0") == 0) optimization = 0;
        else if (strcmp(argv[i], "-O1") == 0) optimization = 1;
        else if (argv[i][0] == '-' || filename != NULL) {
            fprintf(stderr, "Uso: ./minic [--ir] [-O0|-O1] codigo.c\n");
            return 2;
        } else filename = argv[i];
    }
    if (filename == NULL) {
        fprintf(stderr, "Uso: ./minic [--ir] [-O0|-O1] codigo.c\n");
        return 2;
    }
    source = read_file_normalized(filename);
    if (source == NULL) {
        fprintf(stderr, "ERRO: não foi possível ler o arquivo: %s\n", filename);
        return 2;
    }

    memset(&lexer, 0, sizeof(lexer));
    lexer.source = source;
    lexer.length = strlen(source);
    lexer.pos = 0;
    lexer.line = 1;
    lexer.column = 1;

    if (!lexer_scan_all(&lexer)) {
        print_lexical_error(&lexer);
        lexer_free(&lexer);
        free(source);
        return emit_ir ? 2 : 1;
    }

    memset(&parser, 0, sizeof(parser));
    parser.tokens = &lexer.tokens;
    parser.current_index = 0;
    parser.has_error = false;
    parser.error_token = NULL;
    parser.semantic_mode = true;
    arena_init(&parser.arena);

    ast = parser_parse(&parser);

    if (parser.has_error) {
        if (emit_ir) {
            fprintf(stderr, "Erro sintático na linha %d, coluna %d: %s. Encontrado '%s'.\n",
                    parser.error_token ? parser.error_token->line : 0,
                    parser.error_token ? parser.error_token->column : 0,
                    parser.error_message,
                    parser.error_token ? (parser.error_token->type == TOK_EOF
                        ? "EOF" : parser.error_token->lexeme) : "EOF");
        } else print_syntax_rejection(&parser);
        arena_free(&parser.arena);
        lexer_free(&lexer);
        free(source);
        return emit_ir ? 3 : 1;
    }

    sem_analyzer_init(&analyzer);
    sem_analyze_program(&analyzer, ast);
    if (emit_ir) {
        if (analyzer.diagnostics.count) {
            size_t j;
            for (j = 0; j < analyzer.diagnostics.count; ++j) {
                SemDiagnostic *d = &analyzer.diagnostics.items[j];
                fprintf(stderr, "%s — linha %d, coluna %d: %s\n",
                        d->code, d->line, d->column, d->message);
            }
            status = 4;
        } else {
            char *tac = ir_generate(ast, optimization);
            if (optimization == 1) {
                char *optimized = ir_optimize_advanced(tac);
                free(tac);
                tac = optimized;
            }
            fwrite(tac, 1, strlen(tac), stdout);
            free(tac);
            status = 0;
        }
    } else {
        sem_write_result(&analyzer.diagnostics);
        status = analyzer.diagnostics.count == 0 ? 0 : 1;
    }

    sem_analyzer_free(&analyzer);
    arena_free(&parser.arena);
    lexer_free(&lexer);
    free(source);

    return status;
}
