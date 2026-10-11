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

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char **argv)
{
    char *source;

#ifdef _WIN32
    /*
     * Os gabaritos semânticos usam CRLF entre as linhas. semantic.c grava
     * esses bytes explicitamente. Em executáveis nativos do Windows, stdout
     * em modo texto converteria novamente cada \n para CRLF, produzindo
     * \r\r\n. O modo binário preserva exatamente os bytes emitidos e
     * mantém o mesmo resultado observado em Linux/MSYS2.
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

    if (argc != 2) {
        fprintf(stderr, "Uso: ./minic codigo.c\n");
        return 2;
    }

    source = read_file_normalized(argv[1]);
    if (source == NULL) {
        fprintf(stderr, "ERRO: não foi possível ler o arquivo: %s\n", argv[1]);
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
        return 1;
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
        print_syntax_rejection(&parser);
        arena_free(&parser.arena);
        lexer_free(&lexer);
        free(source);
        return 1;
    }

    sem_analyzer_init(&analyzer);
    sem_analyze_program(&analyzer, ast);
    sem_write_result(&analyzer.diagnostics);
    status = analyzer.diagnostics.count == 0 ? 0 : 1;

    sem_analyzer_free(&analyzer);
    arena_free(&parser.arena);
    lexer_free(&lexer);
    free(source);

    return status;
}
