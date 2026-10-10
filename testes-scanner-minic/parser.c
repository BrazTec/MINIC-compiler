/*
 * parser.c - analisador sintatico MINIC
 *
 * Implementacao C11 autocontida: le codigo-fonte C/MINIC diretamente,
 * realiza analise lexica em memoria, aplica parser descendente recursivo,
 * constroi AST e imprime a mesma representacao textual de parser.py.
 *
 * Compilacao:
 *   gcc -Wall -Wextra -std=c11 parser.c -o parser
 *
 * Execucao:
 *   ./parser codigo.c
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REJECTION_HEADER "NÃO HÁ AST: o parser deve rejeitar a entrada."

/* ============================================================
 * UTILITARIOS DE MEMORIA
 * ============================================================ */

static void *xmalloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "ERRO: memoria insuficiente.\n");
        exit(2);
    }
    return ptr;
}

static void *xcalloc(size_t count, size_t size)
{
    void *ptr = calloc(count, size);
    if (ptr == NULL) {
        fprintf(stderr, "ERRO: memoria insuficiente.\n");
        exit(2);
    }
    return ptr;
}

static void *xrealloc(void *ptr, size_t size)
{
    void *new_ptr = realloc(ptr, size);
    if (new_ptr == NULL) {
        fprintf(stderr, "ERRO: memoria insuficiente.\n");
        exit(2);
    }
    return new_ptr;
}

static char *xstrdup(const char *text)
{
    size_t length = strlen(text);
    char *copy = xmalloc(length + 1);
    memcpy(copy, text, length + 1);
    return copy;
}

static char *xstrndup_local(const char *text, size_t length)
{
    char *copy = xmalloc(length + 1);
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

/* ============================================================
 * BUFFER DE TEXTO DINAMICO
 * ============================================================ */

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} StringBuffer;

static void sb_init(StringBuffer *buffer)
{
    buffer->capacity = 128;
    buffer->length = 0;
    buffer->data = xmalloc(buffer->capacity);
    buffer->data[0] = '\0';
}

static void sb_reserve(StringBuffer *buffer, size_t extra)
{
    size_t needed = buffer->length + extra + 1;
    if (needed <= buffer->capacity) {
        return;
    }

    while (buffer->capacity < needed) {
        buffer->capacity *= 2;
    }

    buffer->data = xrealloc(buffer->data, buffer->capacity);
}

static void sb_append_n(StringBuffer *buffer, const char *text, size_t length)
{
    sb_reserve(buffer, length);
    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
}

static void sb_append(StringBuffer *buffer, const char *text)
{
    sb_append_n(buffer, text, strlen(text));
}

static void sb_append_char(StringBuffer *buffer, char ch)
{
    sb_reserve(buffer, 1);
    buffer->data[buffer->length++] = ch;
    buffer->data[buffer->length] = '\0';
}

static char *sb_take(StringBuffer *buffer)
{
    char *data = buffer->data;
    buffer->data = NULL;
    buffer->length = 0;
    buffer->capacity = 0;
    return data;
}

/* ============================================================
 * TOKENS
 * ============================================================ */

typedef enum {
    TOK_INT,
    TOK_FLOAT,
    TOK_BOOL,
    TOK_CHAR,
    TOK_VOID,
    TOK_IF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_FOR,
    TOK_RETURN,
    TOK_BREAK,
    TOK_CONTINUE,
    TOK_TRUE,
    TOK_FALSE,
    TOK_PRINT,
    TOK_READ,

    TOK_IDENT,
    TOK_INT_LIT,
    TOK_FLOAT_LIT,
    TOK_CHAR_LIT,
    TOK_STRING_LIT,

    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,

    TOK_EQ,
    TOK_NE,
    TOK_LT,
    TOK_GT,
    TOK_LE,
    TOK_GE,

    TOK_AND,
    TOK_OR,
    TOK_NOT,
    TOK_ASSIGN,

    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_SEMICOLON,
    TOK_COMMA,
    TOK_DOT,

    TOK_EOF
} TokenType;

typedef struct {
    TokenType type;
    char *lexeme;
    int line;
    int column;
} Token;

typedef struct {
    Token *items;
    size_t count;
    size_t capacity;
} TokenVector;

static void token_vector_init(TokenVector *vector)
{
    vector->items = NULL;
    vector->count = 0;
    vector->capacity = 0;
}

static void token_vector_push(
    TokenVector *vector,
    TokenType type,
    char *lexeme,
    int line,
    int column
)
{
    if (vector->count == vector->capacity) {
        vector->capacity = vector->capacity == 0 ? 64 : vector->capacity * 2;
        vector->items = xrealloc(
            vector->items,
            vector->capacity * sizeof(*vector->items)
        );
    }

    vector->items[vector->count++] = (Token) {
        .type = type,
        .lexeme = lexeme,
        .line = line,
        .column = column
    };
}

static void token_vector_free(TokenVector *vector)
{
    size_t i;
    for (i = 0; i < vector->count; ++i) {
        free(vector->items[i].lexeme);
    }
    free(vector->items);
    vector->items = NULL;
    vector->count = 0;
    vector->capacity = 0;
}

/* ============================================================
 * ANALISADOR LEXICO INTERNO
 * ============================================================ */

typedef struct {
    const char *name;
    TokenType type;
} ReservedWord;

static const ReservedWord RESERVED_WORDS[] = {
    {"int", TOK_INT},
    {"float", TOK_FLOAT},
    {"bool", TOK_BOOL},
    {"char", TOK_CHAR},
    {"void", TOK_VOID},
    {"if", TOK_IF},
    {"else", TOK_ELSE},
    {"while", TOK_WHILE},
    {"for", TOK_FOR},
    {"return", TOK_RETURN},
    {"break", TOK_BREAK},
    {"continue", TOK_CONTINUE},
    {"true", TOK_TRUE},
    {"false", TOK_FALSE},
    {"print", TOK_PRINT},
    {"read", TOK_READ}
};

static const size_t RESERVED_WORDS_COUNT =
    sizeof(RESERVED_WORDS) / sizeof(RESERVED_WORDS[0]);

typedef struct {
    const char *source;
    size_t length;
    size_t pos;
    int line;
    int column;

    TokenVector tokens;

    bool has_error;
    char error_name[64];
    char *error_lexeme;
    int error_line;
    int error_column;
} Lexer;

static bool lexer_at_end(const Lexer *lexer)
{
    return lexer->pos >= lexer->length;
}

static unsigned char lexer_current(const Lexer *lexer)
{
    if (lexer_at_end(lexer)) {
        return '\0';
    }
    return (unsigned char) lexer->source[lexer->pos];
}

static unsigned char lexer_peek(const Lexer *lexer, size_t offset)
{
    size_t index = lexer->pos + offset;
    if (index >= lexer->length) {
        return '\0';
    }
    return (unsigned char) lexer->source[index];
}

static bool is_utf8_continuation(unsigned char ch)
{
    return (ch & 0xC0u) == 0x80u;
}

static unsigned char lexer_advance(Lexer *lexer)
{
    unsigned char ch;

    if (lexer_at_end(lexer)) {
        return '\0';
    }

    ch = (unsigned char) lexer->source[lexer->pos++];

    if (ch == '\n') {
        lexer->line += 1;
        lexer->column = 1;
    } else if (!is_utf8_continuation(ch)) {
        lexer->column += 1;
    }

    return ch;
}

static bool ascii_letter(unsigned char ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

static bool ascii_digit(unsigned char ch)
{
    return ch >= '0' && ch <= '9';
}

static bool identifier_start(unsigned char ch)
{
    return ascii_letter(ch) || ch == '_';
}

static bool identifier_part(unsigned char ch)
{
    return identifier_start(ch) || ascii_digit(ch);
}

static char *lexer_substring(const Lexer *lexer, size_t start, size_t end)
{
    return xstrndup_local(lexer->source + start, end - start);
}

static void lexer_set_error(
    Lexer *lexer,
    const char *name,
    char *lexeme,
    int line,
    int column
)
{
    if (lexer->has_error) {
        free(lexeme);
        return;
    }

    lexer->has_error = true;
    snprintf(lexer->error_name, sizeof(lexer->error_name), "%s", name);
    lexer->error_lexeme = lexeme;
    lexer->error_line = line;
    lexer->error_column = column;
}

static TokenType reserved_type(const char *lexeme, bool *found)
{
    size_t i;

    for (i = 0; i < RESERVED_WORDS_COUNT; ++i) {
        if (strcmp(RESERVED_WORDS[i].name, lexeme) == 0) {
            *found = true;
            return RESERVED_WORDS[i].type;
        }
    }

    *found = false;
    return TOK_IDENT;
}

static void lexer_scan_identifier(Lexer *lexer)
{
    size_t start = lexer->pos;
    int line = lexer->line;
    int column = lexer->column;
    char *lexeme;
    bool found;
    TokenType type;

    while (identifier_part(lexer_current(lexer))) {
        lexer_advance(lexer);
    }

    lexeme = lexer_substring(lexer, start, lexer->pos);
    type = reserved_type(lexeme, &found);

    token_vector_push(
        &lexer->tokens,
        found ? type : TOK_IDENT,
        lexeme,
        line,
        column
    );
}

static void lexer_scan_number(Lexer *lexer)
{
    size_t start = lexer->pos;
    int line = lexer->line;
    int column = lexer->column;
    bool is_float = false;

    while (ascii_digit(lexer_current(lexer))) {
        lexer_advance(lexer);
    }

    if (lexer_current(lexer) == '.') {
        if (ascii_digit(lexer_peek(lexer, 1))) {
            is_float = true;
            lexer_advance(lexer);
            while (ascii_digit(lexer_current(lexer))) {
                lexer_advance(lexer);
            }
        } else {
            char *number_lexeme = lexer_substring(lexer, start, lexer->pos);
            token_vector_push(
                &lexer->tokens,
                TOK_INT_LIT,
                number_lexeme,
                line,
                column
            );
            return;
        }
    }

    if (identifier_start(lexer_current(lexer))) {
        size_t end = lexer->pos;
        while (identifier_part(lexer_current(lexer))) {
            lexer_advance(lexer);
        }
        end = lexer->pos;
        lexer_set_error(
            lexer,
            "INVALID_IDENTIFIER",
            lexer_substring(lexer, start, end),
            line,
            column
        );
        return;
    }

    token_vector_push(
        &lexer->tokens,
        is_float ? TOK_FLOAT_LIT : TOK_INT_LIT,
        lexer_substring(lexer, start, lexer->pos),
        line,
        column
    );
}

static void lexer_skip_line_comment(Lexer *lexer)
{
    lexer_advance(lexer); /* / */
    lexer_advance(lexer); /* / */

    while (!lexer_at_end(lexer) && lexer_current(lexer) != '\n') {
        lexer_advance(lexer);
    }
}

static void lexer_skip_block_comment(Lexer *lexer)
{
    size_t start = lexer->pos;
    int line = lexer->line;
    int column = lexer->column;

    lexer_advance(lexer); /* / */
    lexer_advance(lexer); /* * */

    while (!lexer_at_end(lexer)) {
        if (lexer_current(lexer) == '*' && lexer_peek(lexer, 1) == '/') {
            lexer_advance(lexer);
            lexer_advance(lexer);
            return;
        }
        lexer_advance(lexer);
    }

    lexer_set_error(
        lexer,
        "UNTERMINATED_BLOCK_COMMENT",
        lexer_substring(lexer, start, lexer->pos),
        line,
        column
    );
}

static void lexer_scan_string(Lexer *lexer)
{
    size_t start = lexer->pos;
    int line = lexer->line;
    int column = lexer->column;
    bool escaped = false;

    lexer_advance(lexer); /* abertura */

    while (!lexer_at_end(lexer)) {
        unsigned char ch = lexer_current(lexer);

        if (ch == '\n') {
            lexer_set_error(
                lexer,
                "UNTERMINATED_STRING_LITERAL",
                lexer_substring(lexer, start, lexer->pos),
                line,
                column
            );
            return;
        }

        if (!escaped && ch == '"') {
            lexer_advance(lexer);
            token_vector_push(
                &lexer->tokens,
                TOK_STRING_LIT,
                lexer_substring(lexer, start, lexer->pos),
                line,
                column
            );
            return;
        }

        if (!escaped && ch == '\\') {
            escaped = true;
            lexer_advance(lexer);
            continue;
        }

        escaped = false;
        lexer_advance(lexer);
    }

    lexer_set_error(
        lexer,
        "UNTERMINATED_STRING_LITERAL",
        lexer_substring(lexer, start, lexer->pos),
        line,
        column
    );
}

static void lexer_scan_char(Lexer *lexer)
{
    size_t start = lexer->pos;
    int line = lexer->line;
    int column = lexer->column;
    bool escaped = false;

    lexer_advance(lexer); /* abertura */

    while (!lexer_at_end(lexer)) {
        unsigned char ch = lexer_current(lexer);

        if (ch == '\n') {
            lexer_set_error(
                lexer,
                "UNTERMINATED_CHAR_LITERAL",
                lexer_substring(lexer, start, lexer->pos),
                line,
                column
            );
            return;
        }

        if (!escaped && ch == '\'') {
            lexer_advance(lexer);
            token_vector_push(
                &lexer->tokens,
                TOK_CHAR_LIT,
                lexer_substring(lexer, start, lexer->pos),
                line,
                column
            );
            return;
        }

        if (!escaped && ch == '\\') {
            escaped = true;
            lexer_advance(lexer);
            continue;
        }

        escaped = false;
        lexer_advance(lexer);
    }

    lexer_set_error(
        lexer,
        "UNTERMINATED_CHAR_LITERAL",
        lexer_substring(lexer, start, lexer->pos),
        line,
        column
    );
}

static void lexer_push_single(
    Lexer *lexer,
    TokenType type,
    int line,
    int column
)
{
    size_t start = lexer->pos;
    lexer_advance(lexer);
    token_vector_push(
        &lexer->tokens,
        type,
        lexer_substring(lexer, start, lexer->pos),
        line,
        column
    );
}

static void lexer_push_double(
    Lexer *lexer,
    TokenType type,
    int line,
    int column
)
{
    size_t start = lexer->pos;
    lexer_advance(lexer);
    lexer_advance(lexer);
    token_vector_push(
        &lexer->tokens,
        type,
        lexer_substring(lexer, start, lexer->pos),
        line,
        column
    );
}

static bool lexer_scan_all(Lexer *lexer)
{
    token_vector_init(&lexer->tokens);
    lexer->has_error = false;
    lexer->error_lexeme = NULL;

    while (!lexer_at_end(lexer) && !lexer->has_error) {
        unsigned char ch = lexer_current(lexer);
        int line = lexer->line;
        int column = lexer->column;

        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            lexer_advance(lexer);
            continue;
        }

        if (ch == '/' && lexer_peek(lexer, 1) == '/') {
            lexer_skip_line_comment(lexer);
            continue;
        }

        if (ch == '/' && lexer_peek(lexer, 1) == '*') {
            lexer_skip_block_comment(lexer);
            continue;
        }

        if (identifier_start(ch)) {
            lexer_scan_identifier(lexer);
            continue;
        }

        if (ascii_digit(ch)) {
            lexer_scan_number(lexer);
            continue;
        }

        if (ch == '"') {
            lexer_scan_string(lexer);
            continue;
        }

        if (ch == '\'') {
            lexer_scan_char(lexer);
            continue;
        }

        if (ch == '=' && lexer_peek(lexer, 1) == '=') {
            lexer_push_double(lexer, TOK_EQ, line, column);
        } else if (ch == '!' && lexer_peek(lexer, 1) == '=') {
            lexer_push_double(lexer, TOK_NE, line, column);
        } else if (ch == '<' && lexer_peek(lexer, 1) == '=') {
            lexer_push_double(lexer, TOK_LE, line, column);
        } else if (ch == '>' && lexer_peek(lexer, 1) == '=') {
            lexer_push_double(lexer, TOK_GE, line, column);
        } else if (ch == '&' && lexer_peek(lexer, 1) == '&') {
            lexer_push_double(lexer, TOK_AND, line, column);
        } else if (ch == '|' && lexer_peek(lexer, 1) == '|') {
            lexer_push_double(lexer, TOK_OR, line, column);
        } else {
            TokenType type;
            bool known = true;

            switch (ch) {
                case '+': type = TOK_PLUS; break;
                case '-': type = TOK_MINUS; break;
                case '*': type = TOK_STAR; break;
                case '/': type = TOK_SLASH; break;
                case '%': type = TOK_PERCENT; break;
                case '<': type = TOK_LT; break;
                case '>': type = TOK_GT; break;
                case '!': type = TOK_NOT; break;
                case '=': type = TOK_ASSIGN; break;
                case '(': type = TOK_LPAREN; break;
                case ')': type = TOK_RPAREN; break;
                case '[': type = TOK_LBRACKET; break;
                case ']': type = TOK_RBRACKET; break;
                case '{': type = TOK_LBRACE; break;
                case '}': type = TOK_RBRACE; break;
                case ';': type = TOK_SEMICOLON; break;
                case ',': type = TOK_COMMA; break;
                case '.': type = TOK_DOT; break;
                default: known = false; type = TOK_EOF; break;
            }

            if (known) {
                lexer_push_single(lexer, type, line, column);
            } else {
                size_t start = lexer->pos;
                lexer_advance(lexer);
                lexer_set_error(
                    lexer,
                    (ch == '&' || ch == '|')
                        ? "INCOMPLETE_LOGICAL_OPERATOR"
                        : "UNKNOWN_SYMBOL",
                    lexer_substring(lexer, start, lexer->pos),
                    line,
                    column
                );
            }
        }
    }

    if (lexer->has_error) {
        return false;
    }

    token_vector_push(
        &lexer->tokens,
        TOK_EOF,
        xstrdup(""),
        lexer->line,
        lexer->column
    );

    return true;
}

static void lexer_free(Lexer *lexer)
{
    token_vector_free(&lexer->tokens);
    free(lexer->error_lexeme);
    lexer->error_lexeme = NULL;
}

/* ============================================================
 * AST
 * ============================================================ */

typedef enum {
    NODE_PROGRAM,
    NODE_FUNCTION,
    NODE_BLOCK,
    NODE_VAR_DECL,
    NODE_IF,
    NODE_WHILE,
    NODE_RETURN,
    NODE_EXPR_STMT,
    NODE_ASSIGN,
    NODE_BINARY,
    NODE_UNARY,
    NODE_CALL,
    NODE_INDEX,
    NODE_ID,
    NODE_LIT
} NodeKind;

typedef struct ASTNode ASTNode;

typedef struct {
    ASTNode **items;
    size_t count;
    size_t capacity;
} NodeList;

typedef struct {
    const char *type_name;
    const char *name;
    bool is_array;
    int line;
    int column;
} Parameter;

typedef struct {
    Parameter *items;
    size_t count;
    size_t capacity;
} ParameterList;

struct ASTNode {
    NodeKind kind;
    int line;
    int column;

    union {
        struct {
            NodeList items;
        } program;

        struct {
            const char *return_type;
            const char *name;
            ParameterList parameters;
            ASTNode *body;
        } function;

        struct {
            NodeList items;
        } block;

        struct {
            const char *type_name;
            const char *name;
            ASTNode *size;
            ASTNode *initializer;
        } var_decl;

        struct {
            ASTNode *condition;
            ASTNode *then_branch;
            ASTNode *else_branch;
        } if_stmt;

        struct {
            ASTNode *condition;
            ASTNode *body;
        } while_stmt;

        struct {
            ASTNode *value;
        } return_stmt;

        struct {
            ASTNode *expression;
        } expr_stmt;

        struct {
            ASTNode *target;
            ASTNode *value;
        } assign;

        struct {
            const char *operator_lexeme;
            ASTNode *left;
            ASTNode *right;
        } binary;

        struct {
            const char *operator_lexeme;
            ASTNode *operand;
        } unary;

        struct {
            ASTNode *callee;
            NodeList arguments;
        } call;

        struct {
            ASTNode *base;
            ASTNode *index;
        } index;

        struct {
            const char *name;
        } identifier;

        struct {
            const char *literal_type;
            const char *lexeme;
        } literal;
    } as;
};

typedef struct {
    ASTNode **nodes;
    size_t count;
    size_t capacity;
} NodeArena;

static void node_list_init(NodeList *list)
{
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void node_list_push(NodeList *list, ASTNode *node)
{
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->items = xrealloc(
            list->items,
            list->capacity * sizeof(*list->items)
        );
    }
    list->items[list->count++] = node;
}

static void parameter_list_init(ParameterList *list)
{
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void parameter_list_push(
    ParameterList *list,
    const char *type_name,
    const char *name,
    bool is_array,
    int line,
    int column
)
{
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = xrealloc(
            list->items,
            list->capacity * sizeof(*list->items)
        );
    }

    list->items[list->count++] = (Parameter) {
        .type_name = type_name,
        .name = name,
        .is_array = is_array,
        .line = line,
        .column = column
    };
}

static void arena_init(NodeArena *arena)
{
    arena->nodes = NULL;
    arena->count = 0;
    arena->capacity = 0;
}

static ASTNode *arena_new_node(NodeArena *arena, NodeKind kind)
{
    ASTNode *node = xcalloc(1, sizeof(*node));
    node->kind = kind;

    if (arena->count == arena->capacity) {
        arena->capacity = arena->capacity == 0 ? 64 : arena->capacity * 2;
        arena->nodes = xrealloc(
            arena->nodes,
            arena->capacity * sizeof(*arena->nodes)
        );
    }

    arena->nodes[arena->count++] = node;
    return node;
}

static void arena_free(NodeArena *arena)
{
    size_t i;

    for (i = 0; i < arena->count; ++i) {
        ASTNode *node = arena->nodes[i];

        switch (node->kind) {
            case NODE_PROGRAM:
                free(node->as.program.items.items);
                break;
            case NODE_FUNCTION:
                free(node->as.function.parameters.items);
                break;
            case NODE_BLOCK:
                free(node->as.block.items.items);
                break;
            case NODE_CALL:
                free(node->as.call.arguments.items);
                break;
            default:
                break;
        }

        free(node);
    }

    free(arena->nodes);
    arena->nodes = NULL;
    arena->count = 0;
    arena->capacity = 0;
}

/* ============================================================
 * PARSER DESCENDENTE RECURSIVO
 * ============================================================ */

typedef struct {
    TokenVector *tokens;
    size_t current_index;
    NodeArena arena;

    bool has_error;
    Token *error_token;
    char error_message[256];

    /* Quando true, a validade do destino de atribuição é
       postergada para o analisador semântico (SEM013). */
    bool semantic_mode;
} Parser;

static ASTNode *parse_expression(Parser *parser);
static ASTNode *parse_assignment(Parser *parser);
static ASTNode *parse_logical_or(Parser *parser);
static ASTNode *parse_logical_and(Parser *parser);
static ASTNode *parse_equality(Parser *parser);
static ASTNode *parse_relational(Parser *parser);
static ASTNode *parse_additive(Parser *parser);
static ASTNode *parse_multiplicative(Parser *parser);
static ASTNode *parse_unary(Parser *parser);
static ASTNode *parse_postfix(Parser *parser);
static ASTNode *parse_primary(Parser *parser);
static ASTNode *parse_statement(Parser *parser);
static ASTNode *parse_block(Parser *parser);

static Token *parser_current(Parser *parser)
{
    return &parser->tokens->items[parser->current_index];
}

static Token *parser_previous(Parser *parser)
{
    if (parser->current_index == 0) {
        return &parser->tokens->items[0];
    }
    return &parser->tokens->items[parser->current_index - 1];
}

static bool parser_at_end(Parser *parser)
{
    return parser_current(parser)->type == TOK_EOF;
}

static bool parser_check(Parser *parser, TokenType type)
{
    return parser_current(parser)->type == type;
}

static bool parser_check_any(
    Parser *parser,
    const TokenType *types,
    size_t count
)
{
    size_t i;
    TokenType current = parser_current(parser)->type;

    for (i = 0; i < count; ++i) {
        if (current == types[i]) {
            return true;
        }
    }
    return false;
}

static Token *parser_advance(Parser *parser)
{
    Token *token = parser_current(parser);

    if (!parser_at_end(parser)) {
        parser->current_index += 1;
    }

    return token;
}

static bool parser_match(Parser *parser, TokenType type)
{
    if (parser_check(parser, type)) {
        parser_advance(parser);
        return true;
    }
    return false;
}

static bool parser_match_any(
    Parser *parser,
    const TokenType *types,
    size_t count
)
{
    if (parser_check_any(parser, types, count)) {
        parser_advance(parser);
        return true;
    }
    return false;
}

static void parser_set_error(
    Parser *parser,
    Token *token,
    const char *message
)
{
    if (parser->has_error) {
        return;
    }

    parser->has_error = true;
    parser->error_token = token;
    snprintf(parser->error_message, sizeof(parser->error_message), "%s", message);
}

static Token *parser_consume(
    Parser *parser,
    TokenType type,
    const char *message
)
{
    if (parser->has_error) {
        return NULL;
    }

    if (parser_check(parser, type)) {
        return parser_advance(parser);
    }

    parser_set_error(parser, parser_current(parser), message);
    return NULL;
}

static bool is_variable_type(TokenType type)
{
    return type == TOK_INT || type == TOK_FLOAT ||
           type == TOK_BOOL || type == TOK_CHAR;
}

static bool is_return_type(TokenType type)
{
    return is_variable_type(type) || type == TOK_VOID;
}

static const char *type_name_from_token(TokenType type)
{
    switch (type) {
        case TOK_INT: return "int";
        case TOK_FLOAT: return "float";
        case TOK_BOOL: return "bool";
        case TOK_CHAR: return "char";
        case TOK_VOID: return "void";
        default: return NULL;
    }
}

static const char *parse_type(Parser *parser, bool allow_void)
{
    Token *token;

    if (parser->has_error) {
        return NULL;
    }

    token = parser_current(parser);

    if (is_variable_type(token->type) || (allow_void && token->type == TOK_VOID)) {
        parser_advance(parser);
        return type_name_from_token(token->type);
    }

    parser_set_error(parser, token, "esperado tipo");
    return NULL;
}

static ASTNode *new_program(Parser *parser)
{
    ASTNode *node = arena_new_node(&parser->arena, NODE_PROGRAM);
    node_list_init(&node->as.program.items);
    return node;
}

static ASTNode *new_block(Parser *parser)
{
    ASTNode *node = arena_new_node(&parser->arena, NODE_BLOCK);
    node_list_init(&node->as.block.items);
    return node;
}

static ASTNode *new_var_decl(
    Parser *parser,
    const char *type_name,
    Token *name_token,
    ASTNode *size,
    ASTNode *initializer
)
{
    ASTNode *node = arena_new_node(&parser->arena, NODE_VAR_DECL);
    node->line = name_token->line;
    node->column = name_token->column;
    node->as.var_decl.type_name = type_name;
    node->as.var_decl.name = name_token->lexeme;
    node->as.var_decl.size = size;
    node->as.var_decl.initializer = initializer;
    return node;
}

static ASTNode *new_id(Parser *parser, Token *token)
{
    ASTNode *node = arena_new_node(&parser->arena, NODE_ID);
    node->line = token->line;
    node->column = token->column;
    node->as.identifier.name = token->lexeme;
    return node;
}

static ASTNode *new_lit(
    Parser *parser,
    const char *literal_type,
    Token *token
)
{
    ASTNode *node = arena_new_node(&parser->arena, NODE_LIT);
    node->line = token->line;
    node->column = token->column;
    node->as.literal.literal_type = literal_type;
    node->as.literal.lexeme = token->lexeme;
    return node;
}

static ASTNode *new_binary(
    Parser *parser,
    const char *operator_lexeme,
    ASTNode *left,
    ASTNode *right
)
{
    ASTNode *node = arena_new_node(&parser->arena, NODE_BINARY);
    node->line = left != NULL ? left->line : 0;
    node->column = left != NULL ? left->column : 0;
    node->as.binary.operator_lexeme = operator_lexeme;
    node->as.binary.left = left;
    node->as.binary.right = right;
    return node;
}

static ASTNode *new_unary(
    Parser *parser,
    const char *operator_lexeme,
    ASTNode *operand
)
{
    ASTNode *node = arena_new_node(&parser->arena, NODE_UNARY);
    node->line = operand != NULL ? operand->line : 0;
    node->column = operand != NULL ? operand->column : 0;
    node->as.unary.operator_lexeme = operator_lexeme;
    node->as.unary.operand = operand;
    return node;
}

static ASTNode *new_assign(
    Parser *parser,
    ASTNode *target,
    ASTNode *value
)
{
    ASTNode *node = arena_new_node(&parser->arena, NODE_ASSIGN);
    node->line = target != NULL ? target->line : 0;
    node->column = target != NULL ? target->column : 0;
    node->as.assign.target = target;
    node->as.assign.value = value;
    return node;
}

static ASTNode *finish_variable_declaration(
    Parser *parser,
    const char *type_name,
    Token *name_token
)
{
    ASTNode *size = NULL;
    ASTNode *initializer = NULL;

    if (parser->has_error) {
        return NULL;
    }

    if (parser_match(parser, TOK_LBRACKET)) {
        if (parser_check(parser, TOK_RBRACKET)) {
            parser_set_error(
                parser,
                parser_current(parser),
                "esperado expressão para o tamanho do vetor"
            );
            return NULL;
        }

        size = parse_expression(parser);
        if (parser->has_error) {
            return NULL;
        }

        parser_consume(
            parser,
            TOK_RBRACKET,
            "esperado ']' após o tamanho do vetor"
        );
        if (parser->has_error) {
            return NULL;
        }
    }

    if (parser_match(parser, TOK_ASSIGN)) {
        if (parser_check(parser, TOK_SEMICOLON)) {
            parser_set_error(
                parser,
                parser_current(parser),
                "esperado expressão após ="
            );
            return NULL;
        }

        initializer = parse_expression(parser);
        if (parser->has_error) {
            return NULL;
        }
    }

    parser_consume(
        parser,
        TOK_SEMICOLON,
        "esperado ';' após a declaração"
    );
    if (parser->has_error) {
        return NULL;
    }

    return new_var_decl(
        parser,
        type_name,
        name_token,
        size,
        initializer
    );
}

static ASTNode *finish_function(
    Parser *parser,
    const char *return_type,
    Token *name_token,
    Token *type_token
)
{
    ParameterList parameters;
    ASTNode *body;

    parameter_list_init(&parameters);

    if (!parser_check(parser, TOK_RPAREN)) {
        for (;;) {
            static const TokenType invalid_parameter_starts[] = {
                TOK_COMMA,
                TOK_RPAREN,
                TOK_LBRACE,
                TOK_SEMICOLON
            };
            const char *parameter_type;
            Token *parameter_name;
            bool parameter_is_array = false;

            if (parser_check_any(
                    parser,
                    invalid_parameter_starts,
                    sizeof(invalid_parameter_starts) /
                        sizeof(invalid_parameter_starts[0])
                )) {
                parser_set_error(
                    parser,
                    parser_current(parser),
                    "esperado tipo de parâmetro"
                );
                free(parameters.items);
                return NULL;
            }

            parameter_type = parse_type(parser, false);
            if (parser->has_error) {
                free(parameters.items);
                return NULL;
            }

            parameter_name = parser_consume(
                parser,
                TOK_IDENT,
                "esperado identificador do parâmetro"
            );
            if (parser->has_error) {
                free(parameters.items);
                return NULL;
            }

            if (parser_match(parser, TOK_LBRACKET)) {
                parser_consume(
                    parser,
                    TOK_RBRACKET,
                    "esperado ']' após parâmetro vetor"
                );
                if (parser->has_error) {
                    free(parameters.items);
                    return NULL;
                }
                parameter_is_array = true;
            }

            parameter_list_push(
                &parameters,
                parameter_type,
                parameter_name->lexeme,
                parameter_is_array,
                parameter_name->line,
                parameter_name->column
            );

            if (!parser_match(parser, TOK_COMMA)) {
                break;
            }

            if (parser_check(parser, TOK_RPAREN)) {
                parser_set_error(
                    parser,
                    parser_current(parser),
                    "esperado tipo de parâmetro após vírgula"
                );
                free(parameters.items);
                return NULL;
            }
        }
    }

    parser_consume(
        parser,
        TOK_RPAREN,
        "esperado ')' após os parâmetros"
    );
    if (parser->has_error) {
        free(parameters.items);
        return NULL;
    }

    if (!parser_check(parser, TOK_LBRACE)) {
        parser_set_error(
            parser,
            parser_current(parser),
            "esperado '{' para iniciar o corpo da função"
        );
        free(parameters.items);
        return NULL;
    }

    body = parse_block(parser);
    if (parser->has_error) {
        free(parameters.items);
        return NULL;
    }

    {
        ASTNode *node = arena_new_node(&parser->arena, NODE_FUNCTION);
        node->line = type_token != NULL ? type_token->line : name_token->line;
        node->column = type_token != NULL ? type_token->column : name_token->column;
        node->as.function.return_type = return_type;
        node->as.function.name = name_token->lexeme;
        node->as.function.parameters = parameters;
        node->as.function.body = body;
        return node;
    }
}

static ASTNode *parse_declaration_or_function(Parser *parser)
{
    const char *type_name;
    Token *name;
    Token *type_token = parser_current(parser);

    type_name = parse_type(parser, true);
    if (parser->has_error) {
        return NULL;
    }

    name = parser_consume(
        parser,
        TOK_IDENT,
        "esperado identificador após o tipo"
    );
    if (parser->has_error) {
        return NULL;
    }

    if (parser_match(parser, TOK_LPAREN)) {
        return finish_function(parser, type_name, name, type_token);
    }

    if (strcmp(type_name, "void") == 0) {
        parser_set_error(
            parser,
            name,
            "'void' só pode ser usado como tipo de retorno de função"
        );
        return NULL;
    }

    return finish_variable_declaration(parser, type_name, name);
}

static ASTNode *parse_local_declaration(Parser *parser)
{
    const char *type_name;
    Token *name;

    type_name = parse_type(parser, false);
    if (parser->has_error) {
        return NULL;
    }

    name = parser_consume(
        parser,
        TOK_IDENT,
        "esperado identificador após o tipo"
    );
    if (parser->has_error) {
        return NULL;
    }

    if (parser_check(parser, TOK_LPAREN)) {
        parser_set_error(
            parser,
            parser_current(parser),
            "declaração de função não permitida dentro de bloco"
        );
        return NULL;
    }

    return finish_variable_declaration(parser, type_name, name);
}

static ASTNode *parse_block(Parser *parser)
{
    ASTNode *block;

    parser_consume(
        parser,
        TOK_LBRACE,
        "esperado '{' para iniciar bloco"
    );
    if (parser->has_error) {
        return NULL;
    }

    block = new_block(parser);

    while (!parser_check(parser, TOK_RBRACE) && !parser_at_end(parser)) {
        ASTNode *item;

        if (is_variable_type(parser_current(parser)->type)) {
            item = parse_local_declaration(parser);
        } else {
            item = parse_statement(parser);
        }

        if (parser->has_error) {
            return NULL;
        }

        node_list_push(&block->as.block.items, item);
    }

    parser_consume(
        parser,
        TOK_RBRACE,
        "esperado '}' para fechar bloco"
    );
    if (parser->has_error) {
        return NULL;
    }

    return block;
}

static ASTNode *parse_if_statement(Parser *parser)
{
    ASTNode *condition;
    ASTNode *then_branch;
    ASTNode *else_branch = NULL;
    ASTNode *node;

    parser_consume(parser, TOK_LPAREN, "esperado '(' após if");
    if (parser->has_error) {
        return NULL;
    }

    if (parser_check(parser, TOK_RPAREN)) {
        parser_set_error(
            parser,
            parser_current(parser),
            "esperado expressão na condição do if"
        );
        return NULL;
    }

    condition = parse_expression(parser);
    if (parser->has_error) {
        return NULL;
    }

    parser_consume(
        parser,
        TOK_RPAREN,
        "esperado ')' após a condição do if"
    );
    if (parser->has_error) {
        return NULL;
    }

    then_branch = parse_statement(parser);
    if (parser->has_error) {
        return NULL;
    }

    if (parser_match(parser, TOK_ELSE)) {
        else_branch = parse_statement(parser);
        if (parser->has_error) {
            return NULL;
        }
    }

    node = arena_new_node(&parser->arena, NODE_IF);
    node->as.if_stmt.condition = condition;
    node->as.if_stmt.then_branch = then_branch;
    node->as.if_stmt.else_branch = else_branch;
    return node;
}

static ASTNode *parse_while_statement(Parser *parser)
{
    ASTNode *condition;
    ASTNode *body;
    ASTNode *node;

    parser_consume(parser, TOK_LPAREN, "esperado '(' após while");
    if (parser->has_error) {
        return NULL;
    }

    if (parser_check(parser, TOK_RPAREN)) {
        parser_set_error(
            parser,
            parser_current(parser),
            "esperado expressão na condição do while"
        );
        return NULL;
    }

    condition = parse_expression(parser);
    if (parser->has_error) {
        return NULL;
    }

    parser_consume(
        parser,
        TOK_RPAREN,
        "esperado ')' após a condição do while"
    );
    if (parser->has_error) {
        return NULL;
    }

    if (parser_check(parser, TOK_RBRACE) || parser_check(parser, TOK_EOF)) {
        parser_set_error(
            parser,
            parser_current(parser),
            "esperado início de statement como corpo do while"
        );
        return NULL;
    }

    body = parse_statement(parser);
    if (parser->has_error) {
        return NULL;
    }

    node = arena_new_node(&parser->arena, NODE_WHILE);
    node->as.while_stmt.condition = condition;
    node->as.while_stmt.body = body;
    return node;
}

static ASTNode *parse_return_statement(Parser *parser)
{
    ASTNode *node;
    ASTNode *value = NULL;

    if (parser_match(parser, TOK_SEMICOLON)) {
        node = arena_new_node(&parser->arena, NODE_RETURN);
        node->as.return_stmt.value = NULL;
        return node;
    }

    value = parse_expression(parser);
    if (parser->has_error) {
        return NULL;
    }

    parser_consume(parser, TOK_SEMICOLON, "esperado ';' após return");
    if (parser->has_error) {
        return NULL;
    }

    node = arena_new_node(&parser->arena, NODE_RETURN);
    node->as.return_stmt.value = value;
    return node;
}

static ASTNode *parse_expression_statement(Parser *parser)
{
    ASTNode *expression = parse_expression(parser);
    ASTNode *node;

    if (parser->has_error) {
        return NULL;
    }

    parser_consume(
        parser,
        TOK_SEMICOLON,
        "esperado ';' após a expressão"
    );
    if (parser->has_error) {
        return NULL;
    }

    node = arena_new_node(&parser->arena, NODE_EXPR_STMT);
    node->as.expr_stmt.expression = expression;
    return node;
}

static ASTNode *parse_statement(Parser *parser)
{
    if (parser_check(parser, TOK_ELSE)) {
        parser_set_error(parser, parser_current(parser), "token 'else' inesperado");
        return NULL;
    }

    if (parser_check(parser, TOK_RBRACE)) {
        parser_set_error(parser, parser_current(parser), "esperado início de statement");
        return NULL;
    }

    if (parser_check(parser, TOK_LBRACE)) {
        return parse_block(parser);
    }

    if (parser_match(parser, TOK_IF)) {
        return parse_if_statement(parser);
    }

    if (parser_match(parser, TOK_WHILE)) {
        return parse_while_statement(parser);
    }

    if (parser_match(parser, TOK_RETURN)) {
        return parse_return_statement(parser);
    }

    return parse_expression_statement(parser);
}

/* ============================================================
 * EXPRESSOES
 * ============================================================ */

static ASTNode *parse_expression(Parser *parser)
{
    return parse_assignment(parser);
}

static ASTNode *parse_assignment(Parser *parser)
{
    ASTNode *left = parse_logical_or(parser);

    if (parser->has_error) {
        return NULL;
    }

    if (parser_match(parser, TOK_ASSIGN)) {
        ASTNode *value;

        if (!parser->semantic_mode &&
            left->kind != NODE_ID &&
            left->kind != NODE_INDEX) {
            parser_set_error(
                parser,
                parser_previous(parser),
                "destino inválido para atribuição"
            );
            return NULL;
        }

        value = parse_assignment(parser);
        if (parser->has_error) {
            return NULL;
        }

        return new_assign(parser, left, value);
    }

    return left;
}

static ASTNode *parse_logical_or(Parser *parser)
{
    ASTNode *expression = parse_logical_and(parser);

    while (!parser->has_error && parser_match(parser, TOK_OR)) {
        Token *operator_token = parser_previous(parser);
        ASTNode *right = parse_logical_and(parser);
        if (parser->has_error) {
            return NULL;
        }
        expression = new_binary(
            parser,
            operator_token->lexeme,
            expression,
            right
        );
    }

    return expression;
}

static ASTNode *parse_logical_and(Parser *parser)
{
    ASTNode *expression = parse_equality(parser);

    while (!parser->has_error && parser_match(parser, TOK_AND)) {
        Token *operator_token = parser_previous(parser);
        ASTNode *right = parse_equality(parser);
        if (parser->has_error) {
            return NULL;
        }
        expression = new_binary(
            parser,
            operator_token->lexeme,
            expression,
            right
        );
    }

    return expression;
}

static ASTNode *parse_equality(Parser *parser)
{
    static const TokenType operators[] = {TOK_EQ, TOK_NE};
    ASTNode *expression = parse_relational(parser);

    while (!parser->has_error && parser_match_any(
            parser,
            operators,
            sizeof(operators) / sizeof(operators[0])
        )) {
        Token *operator_token = parser_previous(parser);
        ASTNode *right = parse_relational(parser);
        if (parser->has_error) {
            return NULL;
        }
        expression = new_binary(
            parser,
            operator_token->lexeme,
            expression,
            right
        );
    }

    return expression;
}

static ASTNode *parse_relational(Parser *parser)
{
    static const TokenType operators[] = {TOK_LT, TOK_GT, TOK_LE, TOK_GE};
    ASTNode *expression = parse_additive(parser);

    while (!parser->has_error && parser_match_any(
            parser,
            operators,
            sizeof(operators) / sizeof(operators[0])
        )) {
        Token *operator_token = parser_previous(parser);
        ASTNode *right = parse_additive(parser);
        if (parser->has_error) {
            return NULL;
        }
        expression = new_binary(
            parser,
            operator_token->lexeme,
            expression,
            right
        );
    }

    return expression;
}

static ASTNode *parse_additive(Parser *parser)
{
    static const TokenType operators[] = {TOK_PLUS, TOK_MINUS};
    ASTNode *expression = parse_multiplicative(parser);

    while (!parser->has_error && parser_match_any(
            parser,
            operators,
            sizeof(operators) / sizeof(operators[0])
        )) {
        Token *operator_token = parser_previous(parser);
        ASTNode *right = parse_multiplicative(parser);
        if (parser->has_error) {
            return NULL;
        }
        expression = new_binary(
            parser,
            operator_token->lexeme,
            expression,
            right
        );
    }

    return expression;
}

static ASTNode *parse_multiplicative(Parser *parser)
{
    static const TokenType operators[] = {TOK_STAR, TOK_SLASH, TOK_PERCENT};
    ASTNode *expression = parse_unary(parser);

    while (!parser->has_error && parser_match_any(
            parser,
            operators,
            sizeof(operators) / sizeof(operators[0])
        )) {
        Token *operator_token = parser_previous(parser);
        ASTNode *right = parse_unary(parser);
        if (parser->has_error) {
            return NULL;
        }
        expression = new_binary(
            parser,
            operator_token->lexeme,
            expression,
            right
        );
    }

    return expression;
}

static ASTNode *parse_unary(Parser *parser)
{
    static const TokenType operators[] = {TOK_NOT, TOK_MINUS, TOK_PLUS};

    if (parser_match_any(
            parser,
            operators,
            sizeof(operators) / sizeof(operators[0])
        )) {
        Token *operator_token = parser_previous(parser);
        ASTNode *operand = parse_unary(parser);
        if (parser->has_error) {
            return NULL;
        }
        return new_unary(parser, operator_token->lexeme, operand);
    }

    return parse_postfix(parser);
}

static ASTNode *parse_postfix(Parser *parser)
{
    ASTNode *expression = parse_primary(parser);

    if (parser->has_error) {
        return NULL;
    }

    for (;;) {
        if (parser_match(parser, TOK_LPAREN)) {
            NodeList arguments;
            ASTNode *call;

            node_list_init(&arguments);

            if (!parser_check(parser, TOK_RPAREN)) {
                ASTNode *argument = parse_expression(parser);
                if (parser->has_error) {
                    free(arguments.items);
                    return NULL;
                }
                node_list_push(&arguments, argument);

                while (parser_match(parser, TOK_COMMA)) {
                    if (parser_check(parser, TOK_RPAREN)) {
                        parser_set_error(
                            parser,
                            parser_current(parser),
                            "esperado argumento após vírgula"
                        );
                        free(arguments.items);
                        return NULL;
                    }

                    argument = parse_expression(parser);
                    if (parser->has_error) {
                        free(arguments.items);
                        return NULL;
                    }
                    node_list_push(&arguments, argument);
                }
            }

            parser_consume(
                parser,
                TOK_RPAREN,
                "esperado ')' após os argumentos"
            );
            if (parser->has_error) {
                free(arguments.items);
                return NULL;
            }

            call = arena_new_node(&parser->arena, NODE_CALL);
            call->line = expression != NULL ? expression->line : 0;
            call->column = expression != NULL ? expression->column : 0;
            call->as.call.callee = expression;
            call->as.call.arguments = arguments;
            expression = call;

        } else if (parser_match(parser, TOK_LBRACKET)) {
            ASTNode *index;
            ASTNode *index_node;

            if (parser_check(parser, TOK_RBRACKET)) {
                parser_set_error(
                    parser,
                    parser_current(parser),
                    "esperado expressão no índice"
                );
                return NULL;
            }

            index = parse_expression(parser);
            if (parser->has_error) {
                return NULL;
            }

            parser_consume(
                parser,
                TOK_RBRACKET,
                "esperado ']' após o índice"
            );
            if (parser->has_error) {
                return NULL;
            }

            index_node = arena_new_node(&parser->arena, NODE_INDEX);
            index_node->line = expression != NULL ? expression->line : 0;
            index_node->column = expression != NULL ? expression->column : 0;
            index_node->as.index.base = expression;
            index_node->as.index.index = index;
            expression = index_node;

        } else {
            break;
        }
    }

    return expression;
}

static ASTNode *parse_primary(Parser *parser)
{
    Token *token;

    if (parser_match(parser, TOK_INT_LIT)) {
        token = parser_previous(parser);
        return new_lit(parser, "int", token);
    }

    if (parser_match(parser, TOK_FLOAT_LIT)) {
        token = parser_previous(parser);
        return new_lit(parser, "real", token);
    }

    if (parser_match(parser, TOK_TRUE) || parser_match(parser, TOK_FALSE)) {
        token = parser_previous(parser);
        return new_lit(parser, "bool", token);
    }

    if (parser_match(parser, TOK_CHAR_LIT)) {
        token = parser_previous(parser);
        return new_lit(parser, "char", token);
    }

    if (parser_match(parser, TOK_STRING_LIT)) {
        token = parser_previous(parser);
        return new_lit(parser, "string", token);
    }

    if (parser_match(parser, TOK_IDENT) ||
        parser_match(parser, TOK_PRINT) ||
        parser_match(parser, TOK_READ)) {
        token = parser_previous(parser);
        return new_id(parser, token);
    }

    if (parser_match(parser, TOK_LPAREN)) {
        ASTNode *expression;

        if (parser_check(parser, TOK_RPAREN)) {
            parser_set_error(
                parser,
                parser_current(parser),
                "esperado expressão entre parênteses"
            );
            return NULL;
        }

        expression = parse_expression(parser);
        if (parser->has_error) {
            return NULL;
        }

        parser_consume(
            parser,
            TOK_RPAREN,
            "esperado ')' após a expressão"
        );
        if (parser->has_error) {
            return NULL;
        }

        return expression;
    }

    parser_set_error(
        parser,
        parser_current(parser),
        "esperado identificador, literal ou expressão entre parênteses"
    );
    return NULL;
}

static ASTNode *parse_top_level(Parser *parser)
{
    if (is_return_type(parser_current(parser)->type)) {
        return parse_declaration_or_function(parser);
    }

    return parse_statement(parser);
}

static ASTNode *parser_parse(Parser *parser)
{
    ASTNode *program = new_program(parser);

    while (!parser_at_end(parser) && !parser->has_error) {
        ASTNode *item = parse_top_level(parser);
        if (parser->has_error) {
            return NULL;
        }
        node_list_push(&program->as.program.items, item);
    }

    return program;
}

/* ============================================================
 * SERIALIZACAO DA AST
 * ============================================================ */

static void serialize_node(StringBuffer *buffer, const ASTNode *node);

static void serialize_node_list(
    StringBuffer *buffer,
    const NodeList *list,
    const char *separator
)
{
    size_t i;
    for (i = 0; i < list->count; ++i) {
        if (i > 0) {
            sb_append(buffer, separator);
        }
        serialize_node(buffer, list->items[i]);
    }
}

static void serialize_node(StringBuffer *buffer, const ASTNode *node)
{
    size_t i;

    if (node == NULL) {
        sb_append(buffer, "NULL");
        return;
    }

    switch (node->kind) {
        case NODE_PROGRAM:
            sb_append(buffer, "Program(");
            serialize_node_list(buffer, &node->as.program.items, ", ");
            sb_append_char(buffer, ')');
            break;

        case NODE_FUNCTION:
            sb_append(buffer, "Function(");
            sb_append(buffer, node->as.function.return_type);
            sb_append_char(buffer, ' ');
            sb_append(buffer, node->as.function.name);
            sb_append_char(buffer, '(');

            for (i = 0; i < node->as.function.parameters.count; ++i) {
                Parameter *parameter = &node->as.function.parameters.items[i];
                if (i > 0) {
                    sb_append_char(buffer, ',');
                }
                sb_append(buffer, parameter->type_name);
                sb_append_char(buffer, ' ');
                sb_append(buffer, parameter->name);
                if (parameter->is_array) {
                    sb_append(buffer, "[]");
                }
            }

            sb_append(buffer, ") ");
            serialize_node(buffer, node->as.function.body);
            sb_append_char(buffer, ')');
            break;

        case NODE_BLOCK:
            sb_append(buffer, "Block(");
            serialize_node_list(buffer, &node->as.block.items, ", ");
            sb_append_char(buffer, ')');
            break;

        case NODE_VAR_DECL:
            sb_append(buffer, "VarDecl(");
            sb_append(buffer, node->as.var_decl.type_name);
            sb_append_char(buffer, ' ');
            sb_append(buffer, node->as.var_decl.name);

            if (node->as.var_decl.size != NULL) {
                sb_append(buffer, " size=");
                serialize_node(buffer, node->as.var_decl.size);
            }

            if (node->as.var_decl.initializer != NULL) {
                sb_append_char(buffer, '=');
                serialize_node(buffer, node->as.var_decl.initializer);
            }

            sb_append_char(buffer, ')');
            break;

        case NODE_IF:
            sb_append(buffer, "If(");
            serialize_node(buffer, node->as.if_stmt.condition);
            sb_append_char(buffer, ',');
            serialize_node(buffer, node->as.if_stmt.then_branch);
            sb_append_char(buffer, ',');
            serialize_node(buffer, node->as.if_stmt.else_branch);
            sb_append_char(buffer, ')');
            break;

        case NODE_WHILE:
            sb_append(buffer, "While(");
            serialize_node(buffer, node->as.while_stmt.condition);
            sb_append_char(buffer, ',');
            serialize_node(buffer, node->as.while_stmt.body);
            sb_append_char(buffer, ')');
            break;

        case NODE_RETURN:
            sb_append(buffer, "Return(");
            serialize_node(buffer, node->as.return_stmt.value);
            sb_append_char(buffer, ')');
            break;

        case NODE_EXPR_STMT:
            sb_append(buffer, "ExprStmt(");
            serialize_node(buffer, node->as.expr_stmt.expression);
            sb_append_char(buffer, ')');
            break;

        case NODE_ASSIGN:
            sb_append(buffer, "Assign(");
            serialize_node(buffer, node->as.assign.target);
            sb_append_char(buffer, ',');
            serialize_node(buffer, node->as.assign.value);
            sb_append_char(buffer, ')');
            break;

        case NODE_BINARY:
            sb_append(buffer, "Binary(");
            sb_append(buffer, node->as.binary.operator_lexeme);
            sb_append_char(buffer, ',');
            serialize_node(buffer, node->as.binary.left);
            sb_append_char(buffer, ',');
            serialize_node(buffer, node->as.binary.right);
            sb_append_char(buffer, ')');
            break;

        case NODE_UNARY:
            sb_append(buffer, "Unary(");
            sb_append(buffer, node->as.unary.operator_lexeme);
            sb_append_char(buffer, ',');
            serialize_node(buffer, node->as.unary.operand);
            sb_append_char(buffer, ')');
            break;

        case NODE_CALL:
            sb_append(buffer, "Call(");
            serialize_node(buffer, node->as.call.callee);
            for (i = 0; i < node->as.call.arguments.count; ++i) {
                sb_append_char(buffer, ',');
                serialize_node(buffer, node->as.call.arguments.items[i]);
            }
            sb_append_char(buffer, ')');
            break;

        case NODE_INDEX:
            sb_append(buffer, "Index(");
            serialize_node(buffer, node->as.index.base);
            sb_append_char(buffer, ',');
            serialize_node(buffer, node->as.index.index);
            sb_append_char(buffer, ')');
            break;

        case NODE_ID:
            sb_append(buffer, "Id(");
            sb_append(buffer, node->as.identifier.name);
            sb_append_char(buffer, ')');
            break;

        case NODE_LIT:
            sb_append(buffer, "Lit(");
            sb_append(buffer, node->as.literal.literal_type);
            sb_append_char(buffer, ',');
            sb_append(buffer, node->as.literal.lexeme);
            sb_append_char(buffer, ')');
            break;
    }
}

static char *serialize_ast(const ASTNode *root)
{
    StringBuffer buffer;
    sb_init(&buffer);
    serialize_node(&buffer, root);
    return sb_take(&buffer);
}

/* ============================================================
 * ARQUIVO
 * ============================================================ */

static char *read_file_normalized(const char *path)
{
    FILE *file;
    long raw_size;
    char *raw;
    char *normalized;
    size_t read_count;
    size_t i;
    size_t out = 0;

    file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    raw_size = ftell(file);
    if (raw_size < 0) {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    raw = xmalloc((size_t) raw_size + 1);
    read_count = fread(raw, 1, (size_t) raw_size, file);
    fclose(file);
    raw[read_count] = '\0';

    normalized = xmalloc(read_count + 1);

    for (i = 0; i < read_count; ++i) {
        if (raw[i] == '\r') {
            if (i + 1 < read_count && raw[i + 1] == '\n') {
                i += 1;
            }
            normalized[out++] = '\n';
        } else {
            normalized[out++] = raw[i];
        }
    }

    normalized[out] = '\0';
    free(raw);
    return normalized;
}

/* ============================================================
 * SAIDA DE ERROS
 * ============================================================ */

static void print_syntax_rejection(const Parser *parser)
{
    const Token *token = parser->error_token;
    const char *found = token->type == TOK_EOF ? "EOF" : token->lexeme;

    fprintf(stderr, "%s\n", REJECTION_HEADER);
    fprintf(
        stderr,
        "Erro sintático na linha %d, coluna %d: %s. Encontrado '%s'.\n",
        token->line,
        token->column,
        parser->error_message,
        found
    );
}

static void print_lexical_error(const Lexer *lexer)
{
    fprintf(
        stderr,
        "Erro léxico na linha %d, coluna %d: %s ('%s')\n",
        lexer->error_line,
        lexer->error_column,
        lexer->error_name,
        lexer->error_lexeme != NULL ? lexer->error_lexeme : ""
    );
}

/* ============================================================
 * MAIN
 * ============================================================ */

#ifndef MINIC_EMBEDDED
int main(int argc, char **argv)
{
    char *source;
    Lexer lexer;
    Parser parser;
    ASTNode *ast;
    char *ast_text;
    int status = 0;

    if (argc != 2) {
        fprintf(stderr, "Uso: ./parser codigo.c\n");
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
    parser.semantic_mode = false;
    arena_init(&parser.arena);

    ast = parser_parse(&parser);

    if (parser.has_error) {
        print_syntax_rejection(&parser);
        status = 1;
    } else {
        ast_text = serialize_ast(ast);
        printf("%s\n", ast_text);
        free(ast_text);
    }

    arena_free(&parser.arena);
    lexer_free(&lexer);
    free(source);

    return status;
}
#endif /* MINIC_EMBEDDED */
