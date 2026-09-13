/*
 * Analisador sintático (parser) da linguagem MINIC - versão C.
 *
 * Uso:
 *     ./parser caminho/para/codigo.c
 *
 * Le o arquivo-fonte, faz analise lexica e sintatica (recursive descent)
 * e, se aceito, imprime a AST em formato de S-expressao (stdout).
 * Se houver erro sintatico, imprime a mensagem em stderr e termina com
 * codigo de saida != 0, sem imprimir AST.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

/* ============================================================
 * LEXER
 * ============================================================ */

typedef enum {
    T_INT, T_FLOAT, T_BOOL, T_CHAR, T_VOID,
    T_IF, T_ELSE, T_WHILE, T_FOR, T_RETURN, T_BREAK, T_CONTINUE,
    T_TRUE, T_FALSE, T_PRINT, T_READ,
    T_IDENT, T_INT_LIT, T_FLOAT_LIT, T_CHAR_LIT, T_STRING_LIT,
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT,
    T_LT, T_GT, T_LE, T_GE, T_EQ, T_NE,
    T_AND, T_OR, T_NOT, T_ASSIGN,
    T_LPAREN, T_RPAREN, T_LBRACKET, T_RBRACKET, T_LBRACE, T_RBRACE,
    T_SEMICOLON, T_COMMA, T_DOT,
    T_EOF
} TokType;

typedef struct {
    TokType type;
    char lexeme[256];
    char svalue[256]; /* para strings/char decodificados */
    double numvalue;
    int line;
    int col;
} Token;

typedef struct {
    Token *items;
    int count;
    int cap;
} TokenList;

static void tl_init(TokenList *tl) {
    tl->cap = 256;
    tl->count = 0;
    tl->items = malloc(sizeof(Token) * tl->cap);
}

static void tl_push(TokenList *tl, Token t) {
    if (tl->count >= tl->cap) {
        tl->cap *= 2;
        tl->items = realloc(tl->items, sizeof(Token) * tl->cap);
    }
    tl->items[tl->count++] = t;
}

static const char *tok_type_name(TokType t) {
    switch (t) {
        case T_INT: return "INT"; case T_FLOAT: return "FLOAT";
        case T_BOOL: return "BOOL"; case T_CHAR: return "CHAR";
        case T_VOID: return "VOID"; case T_IF: return "IF";
        case T_ELSE: return "ELSE"; case T_WHILE: return "WHILE";
        case T_FOR: return "FOR"; case T_RETURN: return "RETURN";
        case T_BREAK: return "BREAK"; case T_CONTINUE: return "CONTINUE";
        case T_TRUE: return "TRUE"; case T_FALSE: return "FALSE";
        case T_PRINT: return "PRINT"; case T_READ: return "READ";
        case T_IDENT: return "IDENT"; case T_INT_LIT: return "INT_LIT";
        case T_FLOAT_LIT: return "FLOAT_LIT"; case T_CHAR_LIT: return "CHAR_LIT";
        case T_STRING_LIT: return "STRING_LIT";
        case T_PLUS: return "PLUS"; case T_MINUS: return "MINUS";
        case T_STAR: return "STAR"; case T_SLASH: return "SLASH";
        case T_PERCENT: return "PERCENT"; case T_LT: return "LT";
        case T_GT: return "GT"; case T_LE: return "LE"; case T_GE: return "GE";
        case T_EQ: return "EQ"; case T_NE: return "NE";
        case T_AND: return "AND"; case T_OR: return "OR"; case T_NOT: return "NOT";
        case T_ASSIGN: return "ASSIGN"; case T_LPAREN: return "LPAREN";
        case T_RPAREN: return "RPAREN"; case T_LBRACKET: return "LBRACKET";
        case T_RBRACKET: return "RBRACKET"; case T_LBRACE: return "LBRACE";
        case T_RBRACE: return "RBRACE"; case T_SEMICOLON: return "SEMICOLON";
        case T_COMMA: return "COMMA"; case T_DOT: return "DOT";
        case T_EOF: return "EOF";
    }
    return "?";
}

typedef struct {
    const char *word;
    TokType type;
} Reserved;

static Reserved RESERVED[] = {
    {"int", T_INT}, {"float", T_FLOAT}, {"bool", T_BOOL}, {"char", T_CHAR}, {"void", T_VOID},
    {"if", T_IF}, {"else", T_ELSE}, {"while", T_WHILE}, {"for", T_FOR},
    {"return", T_RETURN}, {"break", T_BREAK}, {"continue", T_CONTINUE},
    {"true", T_TRUE}, {"false", T_FALSE}, {"print", T_PRINT}, {"read", T_READ},
    {NULL, T_EOF}
};

typedef struct {
    const char *src;
    long len;
    long pos;
    int line;
    int col;
} Lexer;

static char lx_cur(Lexer *lx) {
    if (lx->pos >= lx->len) return '\0';
    return lx->src[lx->pos];
}

static char lx_peek(Lexer *lx, int off) {
    long idx = lx->pos + off;
    if (idx >= lx->len) return '\0';
    return lx->src[idx];
}

static char lx_advance(Lexer *lx) {
    if (lx->pos >= lx->len) return '\0';
    char ch = lx->src[lx->pos++];
    if (ch == '\n') { lx->line++; lx->col = 1; }
    else { lx->col++; }
    return ch;
}

static int is_ident_start(char c) { return isalpha((unsigned char)c) || c == '_'; }
static int is_ident_part(char c) { return isalnum((unsigned char)c) || c == '_'; }

static void lex_error(int line, int col, const char *msg) {
    fprintf(stderr, "Erro lexico na linha %d, coluna %d: %s\n", line, col, msg);
    exit(1);
}

static char decode_escape(char c) {
    switch (c) {
        case 'n': return '\n';
        case 't': return '\t';
        case '\\': return '\\';
        case '\'': return '\'';
        case '"': return '"';
        default: return c;
    }
}

static void skip_trivia(Lexer *lx) {
    for (;;) {
        char ch = lx_cur(lx);
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            lx_advance(lx);
            continue;
        }
        if (ch == '/' && lx_peek(lx, 1) == '/') {
            while (lx_cur(lx) != '\0' && lx_cur(lx) != '\n') lx_advance(lx);
            continue;
        }
        if (ch == '/' && lx_peek(lx, 1) == '*') {
            int sl = lx->line, sc = lx->col;
            lx_advance(lx); lx_advance(lx);
            int closed = 0;
            while (lx_cur(lx) != '\0') {
                if (lx_cur(lx) == '*' && lx_peek(lx, 1) == '/') {
                    lx_advance(lx); lx_advance(lx);
                    closed = 1;
                    break;
                }
                lx_advance(lx);
            }
            if (!closed) lex_error(sl, sc, "comentario de bloco nao terminado");
            continue;
        }
        break;
    }
}

static TokenList tokenize(const char *source) {
    Lexer lx = { source, (long)strlen(source), 0, 1, 1 };
    TokenList tl;
    tl_init(&tl);

    for (;;) {
        skip_trivia(&lx);
        if (lx_cur(&lx) == '\0') {
            Token t = {0};
            t.type = T_EOF;
            t.line = lx.line; t.col = lx.col;
            tl_push(&tl, t);
            break;
        }

        int line = lx.line, col = lx.col;
        char ch = lx_cur(&lx);

        if (is_ident_start(ch)) {
            long start = lx.pos;
            while (is_ident_part(lx_cur(&lx))) lx_advance(&lx);
            long len = lx.pos - start;
            Token t = {0};
            t.line = line; t.col = col;
            if (len >= (long)sizeof(t.lexeme)) len = sizeof(t.lexeme) - 1;
            memcpy(t.lexeme, source + start, len);
            t.lexeme[len] = '\0';
            t.type = T_IDENT;
            for (int i = 0; RESERVED[i].word; i++) {
                if (strcmp(RESERVED[i].word, t.lexeme) == 0) { t.type = RESERVED[i].type; break; }
            }
            tl_push(&tl, t);
            continue;
        }

        if (isdigit((unsigned char)ch)) {
            long start = lx.pos;
            while (isdigit((unsigned char)lx_cur(&lx))) lx_advance(&lx);
            if (lx_cur(&lx) == '.' && isdigit((unsigned char)lx_peek(&lx, 1))) {
                lx_advance(&lx);
                while (isdigit((unsigned char)lx_cur(&lx))) lx_advance(&lx);
                long len = lx.pos - start;
                Token t = {0};
                t.line = line; t.col = col; t.type = T_FLOAT_LIT;
                if (len >= (long)sizeof(t.lexeme)) len = sizeof(t.lexeme) - 1;
                memcpy(t.lexeme, source + start, len);
                t.lexeme[len] = '\0';
                t.numvalue = atof(t.lexeme);
                tl_push(&tl, t);
                continue;
            }
            long len = lx.pos - start;
            Token t = {0};
            t.line = line; t.col = col; t.type = T_INT_LIT;
            if (len >= (long)sizeof(t.lexeme)) len = sizeof(t.lexeme) - 1;
            memcpy(t.lexeme, source + start, len);
            t.lexeme[len] = '\0';
            t.numvalue = atof(t.lexeme);
            tl_push(&tl, t);
            continue;
        }

        if (ch == '"') {
            long start = lx.pos;
            lx_advance(&lx);
            char buf[256]; int bi = 0;
            int closed = 0;
            while (lx_cur(&lx) != '\0' && lx_cur(&lx) != '\n') {
                char c = lx_cur(&lx);
                if (c == '"') { lx_advance(&lx); closed = 1; break; }
                if (c == '\\' && strchr("ntr\\'\"", lx_peek(&lx, 1))) {
                    lx_advance(&lx);
                    char e = lx_advance(&lx);
                    if (bi < (int)sizeof(buf) - 1) buf[bi++] = decode_escape(e);
                    continue;
                }
                if (bi < (int)sizeof(buf) - 1) buf[bi++] = c;
                lx_advance(&lx);
            }
            buf[bi] = '\0';
            if (!closed) lex_error(line, col, "cadeia de caracteres nao terminada");
            long len = lx.pos - start;
            Token t = {0};
            t.line = line; t.col = col; t.type = T_STRING_LIT;
            if (len >= (long)sizeof(t.lexeme)) len = sizeof(t.lexeme) - 1;
            memcpy(t.lexeme, source + start, len);
            t.lexeme[len] = '\0';
            strncpy(t.svalue, buf, sizeof(t.svalue) - 1);
            tl_push(&tl, t);
            continue;
        }

        if (ch == '\'') {
            long start = lx.pos;
            lx_advance(&lx);
            if (lx_cur(&lx) == '\0' || lx_cur(&lx) == '\n') lex_error(line, col, "caractere nao terminado");
            char value;
            if (lx_cur(&lx) == '\\' && strchr("ntr\\'\"", lx_peek(&lx, 1))) {
                lx_advance(&lx);
                value = decode_escape(lx_advance(&lx));
            } else {
                value = lx_advance(&lx);
            }
            if (lx_cur(&lx) != '\'') lex_error(line, col, "caractere nao terminado");
            lx_advance(&lx);
            long len = lx.pos - start;
            Token t = {0};
            t.line = line; t.col = col; t.type = T_CHAR_LIT;
            if (len >= (long)sizeof(t.lexeme)) len = sizeof(t.lexeme) - 1;
            memcpy(t.lexeme, source + start, len);
            t.lexeme[len] = '\0';
            t.svalue[0] = value; t.svalue[1] = '\0';
            tl_push(&tl, t);
            continue;
        }

        char two[3] = { ch, lx_peek(&lx, 1), '\0' };
        TokType tt = T_EOF; int matched2 = 1;
        if (strcmp(two, "==") == 0) tt = T_EQ;
        else if (strcmp(two, "!=") == 0) tt = T_NE;
        else if (strcmp(two, "<=") == 0) tt = T_LE;
        else if (strcmp(two, ">=") == 0) tt = T_GE;
        else if (strcmp(two, "&&") == 0) tt = T_AND;
        else if (strcmp(two, "||") == 0) tt = T_OR;
        else matched2 = 0;

        if (matched2) {
            lx_advance(&lx); lx_advance(&lx);
            Token t = {0};
            t.line = line; t.col = col; t.type = tt;
            strcpy(t.lexeme, two);
            tl_push(&tl, t);
            continue;
        }

        TokType one = T_EOF; int matched1 = 1;
        switch (ch) {
            case '+': one = T_PLUS; break;
            case '-': one = T_MINUS; break;
            case '*': one = T_STAR; break;
            case '/': one = T_SLASH; break;
            case '%': one = T_PERCENT; break;
            case '<': one = T_LT; break;
            case '>': one = T_GT; break;
            case '!': one = T_NOT; break;
            case '=': one = T_ASSIGN; break;
            case '(': one = T_LPAREN; break;
            case ')': one = T_RPAREN; break;
            case '[': one = T_LBRACKET; break;
            case ']': one = T_RBRACKET; break;
            case '{': one = T_LBRACE; break;
            case '}': one = T_RBRACE; break;
            case ';': one = T_SEMICOLON; break;
            case ',': one = T_COMMA; break;
            case '.': one = T_DOT; break;
            default: matched1 = 0; break;
        }

        if (matched1) {
            lx_advance(&lx);
            Token t = {0};
            t.line = line; t.col = col; t.type = one;
            t.lexeme[0] = ch; t.lexeme[1] = '\0';
            tl_push(&tl, t);
            continue;
        }

        char msg[64];
        snprintf(msg, sizeof(msg), "simbolo desconhecido '%c'", ch);
        lex_error(line, col, msg);
    }

    return tl;
}

/* ============================================================
 * AST
 *
 * Representada como arvore de strings ja formatadas: cada no
 * eh construido e impresso diretamente como texto (abordagem
 * simples e suficiente para o formato de S-expressao exigido).
 * ============================================================ */

typedef struct Node {
    char *text; /* representacao textual final do (sub)no */
} Node;

static char *xstrdup(const char *s) {
    char *r = malloc(strlen(s) + 1);
    strcpy(r, s);
    return r;
}

static char *xsprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list args2;
    va_copy(args2, args);
    int needed = vsnprintf(NULL, 0, fmt, args);
    char *buf = malloc(needed + 1);
    vsnprintf(buf, needed + 1, fmt, args2);
    va_end(args);
    va_end(args2);
    return buf;
}

static Node node_new(char *text) {
    Node n; n.text = text; return n;
}

/* junta uma lista de nos com virgula */
static char *join_nodes(Node *nodes, int count) {
    if (count == 0) return xstrdup("");
    size_t total = 1;
    for (int i = 0; i < count; i++) total += strlen(nodes[i].text) + 1;
    char *buf = malloc(total);
    buf[0] = '\0';
    for (int i = 0; i < count; i++) {
        if (i > 0) strcat(buf, ",");
        strcat(buf, nodes[i].text);
    }
    return buf;
}

/* ============================================================
 * PARSER
 * ============================================================ */

typedef struct {
    TokenList tl;
    int pos;
} Parser;

static Token *p_cur(Parser *p) { return &p->tl.items[p->pos]; }

static int p_check1(Parser *p, TokType t) { return p_cur(p)->type == t; }

static int p_is_type(TokType t) {
    return t == T_INT || t == T_FLOAT || t == T_BOOL || t == T_CHAR || t == T_VOID;
}

static Token p_advance(Parser *p) {
    Token t = *p_cur(p);
    if (p_cur(p)->type != T_EOF) p->pos++;
    return t;
}

static void p_fail(Parser *p, const char *expected) {
    Token *t = p_cur(p);
    fprintf(stderr,
            "Erro sintatico na linha %d, coluna %d: esperado %s, encontrado '%s' (%s)\n",
            t->line, t->col, expected, t->lexeme, tok_type_name(t->type));
    exit(1);
}

static void p_unexpected(Parser *p) {
    Token *t = p_cur(p);
    fprintf(stderr,
            "Erro sintatico na linha %d, coluna %d: token %s inesperado ('%s')\n",
            t->line, t->col, tok_type_name(t->type), t->lexeme);
    exit(1);
}

static Token p_expect(Parser *p, TokType t, const char *expected) {
    if (p_check1(p, t)) return p_advance(p);
    p_fail(p, expected);
    Token dummy = {0};
    return dummy; /* nao alcancado */
}

/* forward decls */
static Node parse_program(Parser *p);
static Node parse_global_decl(Parser *p);
static Node parse_var_decl_tail(Parser *p, const char *type_name, const char *name);
static Node parse_function_tail(Parser *p, const char *ret_type, const char *name);
static Node parse_block(Parser *p);
static Node parse_stmt(Parser *p);
static Node parse_expr(Parser *p);
static Node parse_assignment(Parser *p);
static Node parse_or(Parser *p);
static Node parse_and(Parser *p);
static Node parse_equality(Parser *p);
static Node parse_relational(Parser *p);
static Node parse_additive(Parser *p);
static Node parse_multiplicative(Parser *p);
static Node parse_unary(Parser *p);
static Node parse_postfix(Parser *p);
static Node parse_primary(Parser *p);
static int is_lvalue_text(const char *text); /* Id(..) ou Index(..) */

static Node parse_program(Parser *p) {
    Node list[4096];
    int count = 0;
    while (!p_check1(p, T_EOF)) {
        list[count++] = parse_global_decl(p);
    }
    char *joined = join_nodes(list, count);
    char *out = xsprintf("Program(%s)", joined);
    free(joined);
    return node_new(out);
}

static Node parse_global_decl(Parser *p) {
    if (!p_is_type(p_cur(p)->type)) {
        return parse_stmt(p);
    }
    Token type_tok = p_advance(p);
    Token name_tok = p_expect(p, T_IDENT, "IDENT");

    if (p_check1(p, T_LPAREN)) {
        return parse_function_tail(p, type_tok.lexeme, name_tok.lexeme);
    }
    return parse_var_decl_tail(p, type_tok.lexeme, name_tok.lexeme);
}

static Node parse_var_decl_tail(Parser *p, const char *type_name, const char *name) {
    char *size_text = NULL;
    char *init_text = NULL;

    if (p_check1(p, T_LBRACKET)) {
        p_advance(p);
        Node sz = parse_expr(p);
        p_expect(p, T_RBRACKET, "FECHA_COLCHETE");
        size_text = sz.text;
    } else if (p_check1(p, T_ASSIGN)) {
        p_advance(p);
        Node ex = parse_expr(p);
        init_text = ex.text;
    }

    p_expect(p, T_SEMICOLON, "PONTO_E_VIRGULA");

    char *inner;
    if (size_text) {
        inner = xsprintf("%s %s size=%s", type_name, name, size_text);
        free(size_text);
    } else if (init_text) {
        inner = xsprintf("%s %s=%s", type_name, name, init_text);
        free(init_text);
    } else {
        inner = xsprintf("%s %s", type_name, name);
    }
    char *out = xsprintf("VarDecl(%s)", inner);
    free(inner);
    return node_new(out);
}

static Node parse_function_tail(Parser *p, const char *ret_type, const char *name) {
    p_expect(p, T_LPAREN, "ABRE_PAREN");
    char params_buf[2048];
    params_buf[0] = '\0';
    int first = 1;

    if (!p_check1(p, T_RPAREN)) {
        for (;;) {
            if (!p_is_type(p_cur(p)->type)) p_fail(p, "KW_TIPO");
            Token type_tok = p_advance(p);
            Token name_tok = p_expect(p, T_IDENT, "IDENT");
            if (!first) strcat(params_buf, ",");
            char tmp[300];
            snprintf(tmp, sizeof(tmp), "%s %s", type_tok.lexeme, name_tok.lexeme);
            strcat(params_buf, tmp);
            first = 0;
            if (p_check1(p, T_COMMA)) { p_advance(p); continue; }
            break;
        }
    }
    p_expect(p, T_RPAREN, "IDENT ou FECHA_PAREN");
    Node body = parse_block(p);
    char *out = xsprintf("Function(%s %s(%s) %s)", ret_type, name, params_buf, body.text);
    free(body.text);
    return node_new(out);
}

static Node parse_block(Parser *p) {
    p_expect(p, T_LBRACE, "ABRE_CHAVE");
    Node list[4096];
    int count = 0;
    while (!p_check1(p, T_RBRACE) && !p_check1(p, T_EOF)) {
        list[count++] = parse_stmt(p);
    }
    p_expect(p, T_RBRACE, "FECHA_CHAVE");
    char *joined = join_nodes(list, count);
    char *out = xsprintf("Block(%s)", joined);
    free(joined);
    return node_new(out);
}

static Node parse_stmt(Parser *p) {
    if (p_check1(p, T_LBRACE)) return parse_block(p);

    if (p_is_type(p_cur(p)->type)) {
        Token type_tok = p_advance(p);
        Token name_tok = p_expect(p, T_IDENT, "IDENT");
        return parse_var_decl_tail(p, type_tok.lexeme, name_tok.lexeme);
    }

    if (p_check1(p, T_IF)) {
        p_advance(p);
        p_expect(p, T_LPAREN, "ABRE_PAREN");
        Node cond = parse_expr(p);
        p_expect(p, T_RPAREN, "FECHA_PAREN");
        Node then_s = parse_stmt(p);
        char *els_text;
        int has_else = 0;
        if (p_check1(p, T_ELSE)) {
            p_advance(p);
            Node els = parse_stmt(p);
            els_text = els.text;
            has_else = 1;
        } else {
            els_text = xstrdup("NULL");
        }
        char *out = xsprintf("If(%s,%s,%s)", cond.text, then_s.text, els_text);
        free(cond.text); free(then_s.text); free(els_text);
        (void)has_else;
        return node_new(out);
    }

    if (p_check1(p, T_WHILE)) {
        p_advance(p);
        p_expect(p, T_LPAREN, "ABRE_PAREN");
        Node cond = parse_expr(p);
        p_expect(p, T_RPAREN, "FECHA_PAREN");
        Node body = parse_stmt(p);
        char *out = xsprintf("While(%s,%s)", cond.text, body.text);
        free(cond.text); free(body.text);
        return node_new(out);
    }

    if (p_check1(p, T_RETURN)) {
        p_advance(p);
        char *expr_text;
        if (!p_check1(p, T_SEMICOLON)) {
            Node ex = parse_expr(p);
            expr_text = ex.text;
        } else {
            expr_text = xstrdup("NULL");
        }
        p_expect(p, T_SEMICOLON, "PONTO_E_VIRGULA");
        char *out = xsprintf("Return(%s)", expr_text);
        free(expr_text);
        return node_new(out);
    }

    if (p_check1(p, T_ELSE)) {
        p_unexpected(p);
    }

    if (p_check1(p, T_RBRACE) || p_check1(p, T_EOF)) {
        p_fail(p, "inicio de statement");
    }

    Node ex = parse_expr(p);
    p_expect(p, T_SEMICOLON, "PONTO_E_VIRGULA");
    char *out = xsprintf("ExprStmt(%s)", ex.text);
    free(ex.text);
    return node_new(out);
}

static Node parse_expr(Parser *p) { return parse_assignment(p); }

static int is_lvalue_text(const char *text) {
    return strncmp(text, "Id(", 3) == 0 || strncmp(text, "Index(", 6) == 0;
}

static Node parse_assignment(Parser *p) {
    Node left = parse_or(p);
    if (p_check1(p, T_ASSIGN)) {
        if (!is_lvalue_text(left.text)) {
            p_fail(p, "identificador ou indice de vetor a esquerda de '='");
        }
        p_advance(p);
        Node right = parse_assignment(p);
        char *out = xsprintf("Assign(%s,%s)", left.text, right.text);
        free(left.text); free(right.text);
        return node_new(out);
    }
    return left;
}

#define BIN_LOOP(FUNC_NAME, NEXT_FUNC, CHECK_COND, GET_OP) \
static Node FUNC_NAME(Parser *p) { \
    Node left = NEXT_FUNC(p); \
    while (CHECK_COND) { \
        char op[3]; strcpy(op, GET_OP); \
        p_advance(p); \
        Node right = NEXT_FUNC(p); \
        char *out = xsprintf("Binary(%s,%s,%s)", op, left.text, right.text); \
        free(left.text); free(right.text); \
        left = node_new(out); \
    } \
    return left; \
}

BIN_LOOP(parse_or, parse_and, p_check1(p, T_OR), "||")
BIN_LOOP(parse_and, parse_equality, p_check1(p, T_AND), "&&")

static Node parse_equality(Parser *p) {
    Node left = parse_relational(p);
    while (p_check1(p, T_EQ) || p_check1(p, T_NE)) {
        const char *op = p_check1(p, T_EQ) ? "==" : "!=";
        p_advance(p);
        Node right = parse_relational(p);
        char *out = xsprintf("Binary(%s,%s,%s)", op, left.text, right.text);
        free(left.text); free(right.text);
        left = node_new(out);
    }
    return left;
}

static Node parse_relational(Parser *p) {
    Node left = parse_additive(p);
    while (p_check1(p, T_LT) || p_check1(p, T_GT) || p_check1(p, T_LE) || p_check1(p, T_GE)) {
        const char *op = p_check1(p, T_LT) ? "<" : p_check1(p, T_GT) ? ">" : p_check1(p, T_LE) ? "<=" : ">=";
        p_advance(p);
        Node right = parse_additive(p);
        char *out = xsprintf("Binary(%s,%s,%s)", op, left.text, right.text);
        free(left.text); free(right.text);
        left = node_new(out);
    }
    return left;
}

static Node parse_additive(Parser *p) {
    Node left = parse_multiplicative(p);
    while (p_check1(p, T_PLUS) || p_check1(p, T_MINUS)) {
        const char *op = p_check1(p, T_PLUS) ? "+" : "-";
        p_advance(p);
        Node right = parse_multiplicative(p);
        char *out = xsprintf("Binary(%s,%s,%s)", op, left.text, right.text);
        free(left.text); free(right.text);
        left = node_new(out);
    }
    return left;
}

static Node parse_multiplicative(Parser *p) {
    Node left = parse_unary(p);
    while (p_check1(p, T_STAR) || p_check1(p, T_SLASH) || p_check1(p, T_PERCENT)) {
        const char *op = p_check1(p, T_STAR) ? "*" : p_check1(p, T_SLASH) ? "/" : "%";
        p_advance(p);
        Node right = parse_unary(p);
        char *out = xsprintf("Binary(%s,%s,%s)", op, left.text, right.text);
        free(left.text); free(right.text);
        left = node_new(out);
    }
    return left;
}

static Node parse_unary(Parser *p) {
    if (p_check1(p, T_MINUS) || p_check1(p, T_NOT) || p_check1(p, T_PLUS)) {
        const char *op = p_check1(p, T_MINUS) ? "-" : p_check1(p, T_NOT) ? "!" : "+";
        p_advance(p);
        Node ex = parse_unary(p);
        char *out = xsprintf("Unary(%s,%s)", op, ex.text);
        free(ex.text);
        return node_new(out);
    }
    return parse_postfix(p);
}

static Node parse_postfix(Parser *p) {
    Node expr = parse_primary(p);
    for (;;) {
        if (p_check1(p, T_LBRACKET)) {
            p_advance(p);
            if (p_check1(p, T_RBRACKET)) p_fail(p, "expressao");
            Node idx = parse_expr(p);
            p_expect(p, T_RBRACKET, "FECHA_COLCHETE");
            char *out = xsprintf("Index(%s,%s)", expr.text, idx.text);
            free(expr.text); free(idx.text);
            expr = node_new(out);
        } else if (p_check1(p, T_LPAREN)) {
            p_advance(p);
            Node args[256];
            int argc = 0;
            if (!p_check1(p, T_RPAREN)) {
                args[argc++] = parse_expr(p);
                while (p_check1(p, T_COMMA)) {
                    p_advance(p);
                    if (p_check1(p, T_RPAREN)) p_fail(p, "FECHA_PAREN ou expressao");
                    args[argc++] = parse_expr(p);
                }
            }
            p_expect(p, T_RPAREN, "FECHA_PAREN");
            char argbuf[4096]; argbuf[0] = '\0';
            for (int i = 0; i < argc; i++) {
                strcat(argbuf, ",");
                strcat(argbuf, args[i].text);
                free(args[i].text);
            }
            char *out = xsprintf("Call(%s%s)", expr.text, argbuf);
            free(expr.text);
            expr = node_new(out);
        } else {
            break;
        }
    }
    return expr;
}

static Node parse_primary(Parser *p) {
    Token *t = p_cur(p);

    if (t->type == T_IDENT) {
        p_advance(p);
        return node_new(xsprintf("Id(%s)", t->lexeme));
    }
    if (t->type == T_INT_LIT) {
        p_advance(p);
        long v = (long)t->numvalue;
        return node_new(xsprintf("Lit(int,%ld)", v));
    }
    if (t->type == T_FLOAT_LIT) {
        p_advance(p);
        /* remove zeros a direita desnecessarios mantendo ao menos 1 casa decimal,
           preservando o texto original do literal quando possivel */
        return node_new(xsprintf("Lit(real,%s)", t->lexeme));
    }
    if (t->type == T_CHAR_LIT) {
        p_advance(p);
        return node_new(xsprintf("Lit(char,%s)", t->svalue));
    }
    if (t->type == T_STRING_LIT) {
        p_advance(p);
        return node_new(xsprintf("Lit(string,%s)", t->svalue));
    }
    if (t->type == T_TRUE) {
        p_advance(p);
        return node_new(xstrdup("Lit(bool,true)"));
    }
    if (t->type == T_FALSE) {
        p_advance(p);
        return node_new(xstrdup("Lit(bool,false)"));
    }
    if (t->type == T_LPAREN) {
        p_advance(p);
        Node ex = parse_expr(p);
        p_expect(p, T_RPAREN, "FECHA_PAREN");
        return ex;
    }

    p_fail(p, "identificador, literal ou '(' expressao ')'");
    Node dummy = node_new(xstrdup(""));
    return dummy; /* nao alcancado */
}

/* ============================================================
 * MAIN
 * ============================================================ */

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Erro ao abrir arquivo: %s\n", path);
        exit(2);
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "uso: %s caminho/para/codigo.c\n", argv[0]);
        return 2;
    }

    char *source = read_file(argv[1]);

    TokenList tl = tokenize(source);

    Parser p;
    p.tl = tl;
    p.pos = 0;

    Node ast = parse_program(&p);

    printf("%s\n", ast.text);

    return 0;
}
