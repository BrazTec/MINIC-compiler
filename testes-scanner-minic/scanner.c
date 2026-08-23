#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


/* ============================================================
 * ESTRUTURAS
 * ============================================================ */

typedef struct {
    char *source;
    size_t length;
    size_t pos;
    int line;
    int column;
} Scanner;


typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} Buffer;


typedef struct {
    const char *word;
    const char *token;
} ReservedWord;


/* ============================================================
 * PALAVRAS RESERVADAS
 * ============================================================ */

static const ReservedWord RESERVED_WORDS[] = {
    {"int", "INT"},
    {"float", "FLOAT"},
    {"bool", "BOOL"},
    {"char", "CHAR"},
    {"void", "VOID"},

    {"if", "IF"},
    {"else", "ELSE"},

    {"while", "WHILE"},
    {"for", "FOR"},

    {"return", "RETURN"},
    {"break", "BREAK"},
    {"continue", "CONTINUE"},

    {"true", "TRUE"},
    {"false", "FALSE"},

    {"print", "PRINT"},
    {"read", "READ"}
};


static const size_t RESERVED_WORDS_COUNT =
    sizeof(RESERVED_WORDS) / sizeof(RESERVED_WORDS[0]);


/* ============================================================
 * CONTROLE DO SCANNER
 * ============================================================ */

static bool at_end(const Scanner *s)
{
    return s->pos >= s->length;
}


static unsigned char current_char(const Scanner *s)
{
    if (at_end(s)) {
        return '\0';
    }

    return (unsigned char)s->source[s->pos];
}


static unsigned char peek_char(const Scanner *s, size_t offset)
{
    size_t index = s->pos + offset;

    if (index >= s->length) {
        return '\0';
    }

    return (unsigned char)s->source[index];
}


/*
 * Bytes UTF-8 de continuação possuem o padrão 10xxxxxx.
 *
 * Isso ajuda a manter a coluna semelhante à versão Python
 * mesmo quando comentários/strings contêm caracteres acentuados.
 */
static bool is_utf8_continuation(unsigned char ch)
{
    return (ch & 0xC0u) == 0x80u;
}


static unsigned char advance_char(Scanner *s)
{
    if (at_end(s)) {
        return '\0';
    }

    unsigned char ch =
        (unsigned char)s->source[s->pos++];

    if (ch == '\n') {
        s->line++;
        s->column = 1;
    }
    else if (!is_utf8_continuation(ch)) {
        s->column++;
    }

    return ch;
}


/* ============================================================
 * CLASSIFICAÇÃO DE CARACTERES
 * ============================================================ */

static bool is_ascii_letter(unsigned char ch)
{
    return
        (ch >= 'a' && ch <= 'z') ||
        (ch >= 'A' && ch <= 'Z');
}


static bool is_digit_ascii(unsigned char ch)
{
    return ch >= '0' && ch <= '9';
}


static bool is_identifier_start(unsigned char ch)
{
    return is_ascii_letter(ch) || ch == '_';
}


static bool is_identifier_part(unsigned char ch)
{
    return
        is_identifier_start(ch) ||
        is_digit_ascii(ch);
}


/* ============================================================
 * SUBSTRING
 * ============================================================ */

static char *substring(
    const char *source,
    size_t start,
    size_t end
)
{
    if (end < start) {
        end = start;
    }

    size_t len = end - start;

    char *result =
        (char *)malloc(len + 1);

    if (result == NULL) {
        fprintf(
            stderr,
            "ERRO: memoria insuficiente.\n"
        );

        exit(2);
    }

    memcpy(
        result,
        source + start,
        len
    );

    result[len] = '\0';

    return result;
}


/* ============================================================
 * BUFFER DINÂMICO
 *
 * Usado para o conteúdo interpretado de STRING_LIT e CHAR_LIT.
 * ============================================================ */

static void buffer_init(Buffer *buffer)
{
    buffer->capacity = 32;
    buffer->length = 0;

    buffer->data =
        (char *)malloc(buffer->capacity);

    if (buffer->data == NULL) {
        fprintf(
            stderr,
            "ERRO: memoria insuficiente.\n"
        );

        exit(2);
    }

    buffer->data[0] = '\0';
}


static void buffer_append_byte(
    Buffer *buffer,
    char ch
)
{
    if (
        buffer->length + 2 >
        buffer->capacity
    ) {

        buffer->capacity *= 2;

        char *new_data =
            (char *)realloc(
                buffer->data,
                buffer->capacity
            );

        if (new_data == NULL) {
            free(buffer->data);

            fprintf(
                stderr,
                "ERRO: memoria insuficiente.\n"
            );

            exit(2);
        }

        buffer->data = new_data;
    }

    buffer->data[buffer->length++] = ch;
    buffer->data[buffer->length] = '\0';
}


static void buffer_free(Buffer *buffer)
{
    free(buffer->data);

    buffer->data = NULL;
    buffer->length = 0;
    buffer->capacity = 0;
}


/* ============================================================
 * JSON
 * ============================================================ */

static void print_json_string(
    const char *text
)
{
    putchar('"');

    for (
        const unsigned char *p =
            (const unsigned char *)text;
        *p != '\0';
        ++p
    ) {

        unsigned char ch = *p;

        switch (ch) {

            case '"':
                fputs("\\\"", stdout);
                break;

            case '\\':
                fputs("\\\\", stdout);
                break;

            case '\b':
                fputs("\\b", stdout);
                break;

            case '\f':
                fputs("\\f", stdout);
                break;

            case '\n':
                fputs("\\n", stdout);
                break;

            case '\r':
                fputs("\\r", stdout);
                break;

            case '\t':
                fputs("\\t", stdout);
                break;

            default:

                if (ch < 0x20u) {
                    printf(
                        "\\u%04x",
                        ch
                    );
                }
                else {
                    putchar((int)ch);
                }

                break;
        }
    }

    putchar('"');
}


/* ============================================================
 * EMISSÃO DE TOKENS
 * ============================================================ */

static void emit_token_null(
    const char *token,
    const char *lexeme,
    int line,
    int column
)
{
    fputs(
        "{\"token\":",
        stdout
    );

    print_json_string(token);

    fputs(
        ",\"lexeme\":",
        stdout
    );

    print_json_string(lexeme);

    printf(
        ",\"attribute\":null,"
        "\"line\":%d,"
        "\"column\":%d}\n",
        line,
        column
    );
}


static void emit_token_string(
    const char *token,
    const char *lexeme,
    const char *attribute,
    int line,
    int column
)
{
    fputs(
        "{\"token\":",
        stdout
    );

    print_json_string(token);

    fputs(
        ",\"lexeme\":",
        stdout
    );

    print_json_string(lexeme);

    fputs(
        ",\"attribute\":",
        stdout
    );

    print_json_string(attribute);

    printf(
        ",\"line\":%d,"
        "\"column\":%d}\n",
        line,
        column
    );
}


static void emit_token_number(
    const char *token,
    const char *lexeme,
    int line,
    int column
)
{
    fputs(
        "{\"token\":",
        stdout
    );

    print_json_string(token);

    fputs(
        ",\"lexeme\":",
        stdout
    );

    print_json_string(lexeme);

    /*
     * O lexema numérico pode ser usado diretamente
     * como valor JSON.
     *
     * Exemplos:
     *
     * "12"   -> 12
     * "23.5" -> 23.5
     * "0.0"  -> 0.0
     */
    fputs(
        ",\"attribute\":",
        stdout
    );

    fputs(
        lexeme,
        stdout
    );

    printf(
        ",\"line\":%d,"
        "\"column\":%d}\n",
        line,
        column
    );
}


/* ============================================================
 * EMISSÃO DE ERROS
 * ============================================================ */

static void emit_error(
    const char *error,
    const char *lexeme,
    int line,
    int column
)
{
    fputs(
        "{\"error\":",
        stdout
    );

    print_json_string(error);

    fputs(
        ",\"lexeme\":",
        stdout
    );

    print_json_string(lexeme);

    printf(
        ",\"line\":%d,"
        "\"column\":%d}\n",
        line,
        column
    );
}


/* ============================================================
 * PALAVRAS RESERVADAS
 * ============================================================ */

static const char *reserved_token(
    const char *lexeme
)
{
    for (
        size_t i = 0;
        i < RESERVED_WORDS_COUNT;
        ++i
    ) {

        if (
            strcmp(
                lexeme,
                RESERVED_WORDS[i].word
            ) == 0
        ) {

            return
                RESERVED_WORDS[i].token;
        }
    }

    return NULL;
}


/* ============================================================
 * OPERADORES
 * ============================================================ */

static const char *two_char_token(
    unsigned char a,
    unsigned char b
)
{
    if (a == '=' && b == '=') {
        return "EQ";
    }

    if (a == '!' && b == '=') {
        return "NE";
    }

    if (a == '<' && b == '=') {
        return "LE";
    }

    if (a == '>' && b == '=') {
        return "GE";
    }

    if (a == '&' && b == '&') {
        return "AND";
    }

    if (a == '|' && b == '|') {
        return "OR";
    }

    return NULL;
}


static const char *one_char_token(
    unsigned char ch
)
{
    switch (ch) {

        case '+':
            return "PLUS";

        case '-':
            return "MINUS";

        case '*':
            return "STAR";

        case '/':
            return "SLASH";

        case '%':
            return "PERCENT";

        case '<':
            return "LT";

        case '>':
            return "GT";

        case '!':
            return "NOT";

        case '=':
            return "ASSIGN";

        case '(':
            return "LPAREN";

        case ')':
            return "RPAREN";

        case '[':
            return "LBRACKET";

        case ']':
            return "RBRACKET";

        case '{':
            return "LBRACE";

        case '}':
            return "RBRACE";

        case ';':
            return "SEMICOLON";

        case ',':
            return "COMMA";

        /*
         * Necessário por causa do fixture:
         *
         * valor = 12.;
         *
         * que espera:
         *
         * INT_LIT
         * DOT
         */
        case '.':
            return "DOT";

        default:
            return NULL;
    }
}


/* ============================================================
 * ESCAPES
 * ============================================================ */

static bool decode_escape(
    unsigned char escape_char,
    char *decoded
)
{
    switch (escape_char) {

        case 'n':
            *decoded = '\n';
            return true;

        case 't':
            *decoded = '\t';
            return true;

        case '\\':
            *decoded = '\\';
            return true;

        case '\'':
            *decoded = '\'';
            return true;

        case '"':
            *decoded = '"';
            return true;

        default:
            return false;
    }
}


/* ============================================================
 * COMENTÁRIO //
 * ============================================================ */

static void skip_line_comment(
    Scanner *s
)
{
    while (
        !at_end(s) &&
        current_char(s) != '\n'
    ) {

        advance_char(s);
    }
}


/* ============================================================
 * COMENTÁRIO DE BLOCO
 * ============================================================ */

static void skip_block_comment(
    Scanner *s
)
{
    size_t start_pos = s->pos;

    int start_line =
        s->line;

    int start_column =
        s->column;

    /* consome / */
    advance_char(s);

    /* consome * */
    advance_char(s);

    while (!at_end(s)) {

        if (
            current_char(s) == '*' &&
            peek_char(s, 1) == '/'
        ) {

            advance_char(s);
            advance_char(s);

            return;
        }

        advance_char(s);
    }

    /*
     * EOF antes de */
    char *lexeme =
        substring(
            s->source,
            start_pos,
            s->pos
        );

    emit_error(
        "UNTERMINATED_BLOCK_COMMENT",
        lexeme,
        start_line,
        start_column
    );

    free(lexeme);
}


/* ============================================================
 * IDENTIFICADORES / PALAVRAS RESERVADAS
 * ============================================================ */

static void scan_identifier(
    Scanner *s
)
{
    size_t start_pos =
        s->pos;

    int start_line =
        s->line;

    int start_column =
        s->column;

    while (
        is_identifier_part(
            current_char(s)
        )
    ) {

        advance_char(s);
    }

    char *lexeme =
        substring(
            s->source,
            start_pos,
            s->pos
        );

    const char *token =
        reserved_token(lexeme);

    if (token != NULL) {

        emit_token_null(
            token,
            lexeme,
            start_line,
            start_column
        );
    }
    else {

        emit_token_string(
            "IDENT",
            lexeme,
            lexeme,
            start_line,
            start_column
        );
    }

    free(lexeme);
}


/* ============================================================
 * NÚMEROS
 * ============================================================ */

static void scan_number(
    Scanner *s
)
{
    size_t start_pos =
        s->pos;

    int start_line =
        s->line;

    int start_column =
        s->column;


    /*
     * Parte inteira.
     */
    while (
        is_digit_ascii(
            current_char(s)
        )
    ) {

        advance_char(s);
    }


    size_t integer_end =
        s->pos;


    char *integer_lexeme =
        substring(
            s->source,
            start_pos,
            integer_end
        );


    /* ========================================================
     * REAL VÁLIDO
     *
     * [0-9]+\.[0-9]+
     * ======================================================== */

    if (
        current_char(s) == '.' &&
        is_digit_ascii(
            peek_char(s, 1)
        )
    ) {

        /* consome . */
        advance_char(s);


        while (
            is_digit_ascii(
                current_char(s)
            )
        ) {

            advance_char(s);
        }


        char *lexeme =
            substring(
                s->source,
                start_pos,
                s->pos
            );


        emit_token_number(
            "FLOAT_LIT",
            lexeme,
            start_line,
            start_column
        );


        /*
         * Caso adicional:
         *
         * 12.5abc
         */
        if (
            is_identifier_start(
                current_char(s)
            )
        ) {

            size_t end =
                s->pos;


            while (
                end < s->length &&
                is_identifier_part(
                    (unsigned char)
                    s->source[end]
                )
            ) {

                end++;
            }


            char *invalid =
                substring(
                    s->source,
                    start_pos,
                    end
                );


            emit_error(
                "INVALID_IDENTIFIER",
                invalid,
                start_line,
                start_column
            );


            free(invalid);
        }


        free(lexeme);
        free(integer_lexeme);

        return;
    }


    /* ========================================================
     * REAL MALFORMADO
     *
     * Exemplo:
     *
     * 12.
     *
     * Fixture:
     *
     * INT_LIT 12
     * erro MALFORMED_REAL_LITERAL
     * DOT .
     * ======================================================== */

    if (
        current_char(s) == '.' &&
        !is_digit_ascii(
            peek_char(s, 1)
        )
    ) {

        emit_token_number(
            "INT_LIT",
            integer_lexeme,
            start_line,
            start_column
        );


        char *malformed =
            (char *)malloc(
                strlen(integer_lexeme) + 2
            );


        if (malformed == NULL) {

            free(integer_lexeme);

            fprintf(
                stderr,
                "ERRO: memoria insuficiente.\n"
            );

            exit(2);
        }


        sprintf(
            malformed,
            "%s.",
            integer_lexeme
        );


        emit_error(
            "MALFORMED_REAL_LITERAL",
            malformed,
            start_line,
            start_column
        );


        free(malformed);
        free(integer_lexeme);


        /*
         * O ponto NÃO é consumido.
         *
         * Na próxima iteração ele será DOT.
         */
        return;
    }


    /* ========================================================
     * INTEIRO NORMAL
     * ======================================================== */

    emit_token_number(
        "INT_LIT",
        integer_lexeme,
        start_line,
        start_column
    );


    /* ========================================================
     * IDENTIFICADOR COMEÇANDO POR NÚMERO
     *
     * 123abc
     *
     * produz:
     *
     * INT_LIT 123
     * erro INVALID_IDENTIFIER
     * IDENT abc
     * ======================================================== */

    if (
        is_identifier_start(
            current_char(s)
        )
    ) {

        size_t end =
            s->pos;


        while (
            end < s->length &&
            is_identifier_part(
                (unsigned char)
                s->source[end]
            )
        ) {

            end++;
        }


        char *invalid =
            substring(
                s->source,
                start_pos,
                end
            );


        emit_error(
            "INVALID_IDENTIFIER",
            invalid,
            start_line,
            start_column
        );


        free(invalid);
    }


    free(integer_lexeme);
}


/* ============================================================
 * CÁLCULO DE COLUNA EM RECUPERAÇÃO
 * ============================================================ */

static int column_at_index(
    const Scanner *s,
    size_t start_index,
    int start_column,
    size_t target_index
)
{
    int column =
        start_column;


    for (
        size_t i = start_index;
        i < target_index;
        ++i
    ) {

        unsigned char ch =
            (unsigned char)
            s->source[i];


        if (
            !is_utf8_continuation(ch)
        ) {

            column++;
        }
    }


    return column;
}


/* ============================================================
 * RECUPERAÇÃO DE STRING NÃO TERMINADA
 * ============================================================ */

static void recover_trailing_delimiters_from_string(
    Scanner *s,
    size_t content_start,
    size_t content_end,
    int line,
    int content_column
)
{
    if (
        content_start >=
        content_end
    ) {

        return;
    }


    size_t index =
        content_end;


    /*
     * Ignora espaços finais.
     */
    while (
        index > content_start
    ) {

        unsigned char ch =
            (unsigned char)
            s->source[index - 1];


        if (
            ch == ' ' ||
            ch == '\t'
        ) {

            index--;
        }
        else {

            break;
        }
    }


    size_t suffix_end =
        index;


    /*
     * Procura delimitadores finais.
     */
    while (
        index > content_start
    ) {

        unsigned char ch =
            (unsigned char)
            s->source[index - 1];


        if (
            ch == '(' ||
            ch == ')' ||
            ch == '[' ||
            ch == ']' ||
            ch == '{' ||
            ch == '}' ||
            ch == ';' ||
            ch == ','
        ) {

            index--;
        }
        else {

            break;
        }
    }


    size_t suffix_start =
        index;


    if (
        suffix_start >=
        suffix_end
    ) {

        return;
    }


    for (
        size_t absolute_index =
            suffix_start;

        absolute_index <
            suffix_end;

        ++absolute_index
    ) {

        unsigned char ch =
            (unsigned char)
            s->source[
                absolute_index
            ];


        const char *token =
            one_char_token(ch);


        if (token == NULL) {
            continue;
        }


        int token_column =
            column_at_index(
                s,
                content_start,
                content_column,
                absolute_index
            );


        char lexeme[2] = {
            (char)ch,
            '\0'
        };


        emit_token_null(
            token,
            lexeme,
            line,
            token_column
        );
    }
}


/* ============================================================
 * STRING
 * ============================================================ */

static void scan_string(
    Scanner *s
)
{
    size_t start_pos =
        s->pos;

    int start_line =
        s->line;

    int start_column =
        s->column;


    /* consome " */
    advance_char(s);


    Buffer decoded;

    buffer_init(
        &decoded
    );


    while (
        !at_end(s) &&
        current_char(s) != '\n'
    ) {

        unsigned char ch =
            current_char(s);


        /* ====================================================
         * FECHAMENTO NORMAL
         * ==================================================== */

        if (ch == '"') {

            advance_char(s);


            char *lexeme =
                substring(
                    s->source,
                    start_pos,
                    s->pos
                );


            emit_token_string(
                "STRING_LIT",
                lexeme,
                decoded.data,
                start_line,
                start_column
            );


            free(lexeme);

            buffer_free(
                &decoded
            );

            return;
        }


        /* ====================================================
         * ESCAPE
         * ==================================================== */

        if (ch == '\\') {

            if (
                peek_char(s, 1) ==
                '\0'
            ) {

                break;
            }


            unsigned char escape_char =
                peek_char(s, 1);


            char decoded_char =
                '\0';


            if (
                decode_escape(
                    escape_char,
                    &decoded_char
                )
            ) {

                /* consome \ */
                advance_char(s);

                /* consome caractere */
                advance_char(s);


                buffer_append_byte(
                    &decoded,
                    decoded_char
                );


                continue;
            }


            /*
             * Escape inválido.
             */
            int escape_line =
                s->line;

            int escape_column =
                s->column;

            size_t error_start =
                s->pos;


            advance_char(s);


            if (
                !at_end(s) &&
                current_char(s) != '\n'
            ) {

                advance_char(s);
            }


            char *invalid_escape =
                substring(
                    s->source,
                    error_start,
                    s->pos
                );


            emit_error(
                "INVALID_ESCAPE_SEQUENCE",
                invalid_escape,
                escape_line,
                escape_column
            );


            free(invalid_escape);


            if (
                escape_char != '\0' &&
                escape_char != '\n'
            ) {

                buffer_append_byte(
                    &decoded,
                    (char)escape_char
                );
            }


            continue;
        }


        /* ====================================================
         * CARACTERE NORMAL
         * ==================================================== */

        buffer_append_byte(
            &decoded,
            (char)ch
        );


        advance_char(s);
    }


    /* ========================================================
     * STRING NÃO TERMINADA
     * ======================================================== */

    size_t end_pos =
        s->pos;


    char *lexeme =
        substring(
            s->source,
            start_pos,
            end_pos
        );


    emit_error(
        "UNTERMINATED_STRING_LITERAL",
        lexeme,
        start_line,
        start_column
    );


    free(lexeme);


    /*
     * Necessário para reproduzir:
     *
     * print("texto sem fim);
     *
     * mantendo:
     *
     * RPAREN
     * SEMICOLON
     */
    recover_trailing_delimiters_from_string(
        s,
        start_pos + 1,
        end_pos,
        start_line,
        start_column + 1
    );


    buffer_free(
        &decoded
    );
}


/* ============================================================
 * CHAR
 * ============================================================ */

static void scan_char_literal(
    Scanner *s
)
{
    size_t start_pos =
        s->pos;

    int start_line =
        s->line;

    int start_column =
        s->column;


    /* consome ' */
    advance_char(s);


    if (
        at_end(s) ||
        current_char(s) == '\n'
    ) {

        char *lexeme =
            substring(
                s->source,
                start_pos,
                s->pos
            );


        emit_error(
            "UNTERMINATED_CHAR_LITERAL",
            lexeme,
            start_line,
            start_column
        );


        free(lexeme);

        return;
    }


    Buffer decoded;

    buffer_init(
        &decoded
    );


    /* ========================================================
     * ESCAPE
     * ======================================================== */

    if (
        current_char(s) == '\\'
    ) {

        unsigned char escape_char =
            peek_char(s, 1);


        char decoded_char =
            '\0';


        if (
            decode_escape(
                escape_char,
                &decoded_char
            )
        ) {

            advance_char(s);
            advance_char(s);


            buffer_append_byte(
                &decoded,
                decoded_char
            );
        }
        else {

            while (
                !at_end(s) &&
                current_char(s) != '\'' &&
                current_char(s) != '\n'
            ) {

                advance_char(s);
            }


            if (
                current_char(s) == '\''
            ) {

                advance_char(s);
            }


            char *lexeme =
                substring(
                    s->source,
                    start_pos,
                    s->pos
                );


            emit_error(
                "INVALID_CHAR_LITERAL",
                lexeme,
                start_line,
                start_column
            );


            free(lexeme);

            buffer_free(
                &decoded
            );

            return;
        }
    }
    else {

        unsigned char ch =
            advance_char(s);


        buffer_append_byte(
            &decoded,
            (char)ch
        );
    }


    /* ========================================================
     * FECHAMENTO NORMAL
     * ======================================================== */

    if (
        current_char(s) == '\''
    ) {

        advance_char(s);


        char *lexeme =
            substring(
                s->source,
                start_pos,
                s->pos
            );


        emit_token_string(
            "CHAR_LIT",
            lexeme,
            decoded.data,
            start_line,
            start_column
        );


        free(lexeme);

        buffer_free(
            &decoded
        );

        return;
    }


    /* ========================================================
     * CHAR NÃO TERMINADO
     * ======================================================== */

    size_t error_end =
        s->pos;


    while (
        !at_end(s) &&
        current_char(s) != '\n'
    ) {

        unsigned char ch =
            current_char(s);


        if (
            ch == ';' ||
            ch == ',' ||
            ch == ')' ||
            ch == '}' ||
            ch == ']'
        ) {

            break;
        }


        advance_char(s);

        error_end =
            s->pos;
    }


    char *lexeme =
        substring(
            s->source,
            start_pos,
            error_end
        );


    emit_error(
        "UNTERMINATED_CHAR_LITERAL",
        lexeme,
        start_line,
        start_column
    );


    free(lexeme);


    /*
     * O fixture i03 descarta o restante
     * da linha inválida.
     */
    while (
        !at_end(s) &&
        current_char(s) != '\n'
    ) {

        advance_char(s);
    }


    buffer_free(
        &decoded
    );
}


/* ============================================================
 * SCANNER PRINCIPAL
 * ============================================================ */

static void scan_all(
    Scanner *s
)
{
    while (!at_end(s)) {

        unsigned char ch =
            current_char(s);


        /* ====================================================
         * ESPAÇOS
         * ==================================================== */

        if (
            ch == ' ' ||
            ch == '\t'
        ) {

            advance_char(s);

            continue;
        }


        /* ====================================================
         * NOVA LINHA
         * ==================================================== */

        if (ch == '\n') {

            advance_char(s);

            continue;
        }


        /* ====================================================
         * COMENTÁRIOS
         * ==================================================== */

        if (ch == '/') {

            if (
                peek_char(s, 1) ==
                '/'
            ) {

                skip_line_comment(s);

                continue;
            }


            if (
                peek_char(s, 1) ==
                '*'
            ) {

                skip_block_comment(s);

                continue;
            }
        }


        /* ====================================================
         * IDENTIFICADOR / RESERVADA
         * ==================================================== */

        if (
            is_identifier_start(ch)
        ) {

            scan_identifier(s);

            continue;
        }


        /* ====================================================
         * NÚMERO
         * ==================================================== */

        if (
            is_digit_ascii(ch)
        ) {

            scan_number(s);

            continue;
        }


        /* ====================================================
         * STRING
         * ==================================================== */

        if (ch == '"') {

            scan_string(s);

            continue;
        }


        /* ====================================================
         * CHAR
         * ==================================================== */

        if (ch == '\'') {

            scan_char_literal(s);

            continue;
        }


        /* ====================================================
         * OPERADORES COMPOSTOS
         *
         * Maximal munch.
         * ==================================================== */

        const char *pair_token =
            two_char_token(
                ch,
                peek_char(s, 1)
            );


        if (pair_token != NULL) {

            int line =
                s->line;

            int column =
                s->column;


            char lexeme[3] = {
                (char)ch,
                (char)peek_char(s, 1),
                '\0'
            };


            advance_char(s);
            advance_char(s);


            emit_token_null(
                pair_token,
                lexeme,
                line,
                column
            );


            continue;
        }


        /* ====================================================
         * & ou | ISOLADOS
         * ==================================================== */

        if (
            ch == '&' ||
            ch == '|'
        ) {

            int line =
                s->line;

            int column =
                s->column;


            char lexeme[2] = {
                (char)ch,
                '\0'
            };


            advance_char(s);


            emit_error(
                "INCOMPLETE_LOGICAL_OPERATOR",
                lexeme,
                line,
                column
            );


            continue;
        }


        /* ====================================================
         * OPERADORES / DELIMITADORES SIMPLES
         * ==================================================== */

        const char *token =
            one_char_token(ch);


        if (token != NULL) {

            int line =
                s->line;

            int column =
                s->column;


            char lexeme[2] = {
                (char)ch,
                '\0'
            };


            advance_char(s);


            emit_token_null(
                token,
                lexeme,
                line,
                column
            );


            continue;
        }


        /* ====================================================
         * SÍMBOLO DESCONHECIDO
         * ==================================================== */

        int line =
            s->line;

        int column =
            s->column;


        size_t start =
            s->pos;


        advance_char(s);


        size_t end =
            s->pos;


        char *lexeme =
            substring(
                s->source,
                start,
                end
            );


        emit_error(
            "UNKNOWN_SYMBOL",
            lexeme,
            line,
            column
        );


        free(lexeme);
    }


    /* ========================================================
     * EOF
     * ======================================================== */

    emit_token_null(
        "EOF",
        "",
        s->line,
        s->column
    );
}


/* ============================================================
 * LEITURA DE ARQUIVO
 * ============================================================ */

static char *read_file_normalized(
    const char *path,
    size_t *out_length
)
{
    FILE *file =
        fopen(
            path,
            "rb"
        );


    if (file == NULL) {
        return NULL;
    }


    if (
        fseek(
            file,
            0,
            SEEK_END
        ) != 0
    ) {

        fclose(file);

        return NULL;
    }


    long file_size =
        ftell(file);


    if (file_size < 0) {

        fclose(file);

        return NULL;
    }


    rewind(file);


    char *raw =
        (char *)malloc(
            (size_t)file_size + 1
        );


    if (raw == NULL) {

        fclose(file);

        fprintf(
            stderr,
            "ERRO: memoria insuficiente.\n"
        );

        exit(2);
    }


    size_t bytes_read =
        fread(
            raw,
            1,
            (size_t)file_size,
            file
        );


    fclose(file);


    raw[bytes_read] =
        '\0';


    /*
     * O Python faz normalização de novas linhas:
     *
     * CRLF -> LF
     *
     * Precisamos fazer o mesmo para que linha,
     * coluna e lexemas sejam iguais aos fixtures.
     */
    char *normalized =
        (char *)malloc(
            bytes_read + 1
        );


    if (normalized == NULL) {

        free(raw);

        fprintf(
            stderr,
            "ERRO: memoria insuficiente.\n"
        );

        exit(2);
    }


    size_t out =
        0;


    for (
        size_t i = 0;
        i < bytes_read;
        ++i
    ) {

        if (raw[i] == '\r') {

            if (
                i + 1 < bytes_read &&
                raw[i + 1] == '\n'
            ) {

                i++;
            }


            normalized[out++] =
                '\n';
        }
        else {

            normalized[out++] =
                raw[i];
        }
    }


    normalized[out] =
        '\0';


    free(raw);


    *out_length =
        out;


    return normalized;
}


/* ============================================================
 * MAIN
 * ============================================================ */

int main(
    int argc,
    char **argv
)
{
    if (argc != 2) {

        fprintf(
            stderr,
            "Uso: %s <arquivo.c|arquivo.minic>\n",
            argv[0]
        );

        return 1;
    }


    size_t length =
        0;


    char *source =
        read_file_normalized(
            argv[1],
            &length
        );


    if (source == NULL) {

        fprintf(
            stderr,
            "ERRO: arquivo nao encontrado "
            "ou nao pode ser lido: %s\n",
            argv[1]
        );

        return 1;
    }


    Scanner scanner = {
        .source = source,
        .length = length,
        .pos = 0,
        .line = 1,
        .column = 1
    };


    scan_all(
        &scanner
    );


    free(source);


    return 0;
}