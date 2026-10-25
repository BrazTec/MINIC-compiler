/*
 * semantic.c - analisador semantico MINIC
 *
 * Esta unidade opera somente sobre a AST produzida pelo parser.
 * Quando usada pelo front-end minic.c, parser.c e semantic.c sao
 * compilados na mesma unidade por #include para atender ao script
 * de testes, que invoca gcc apenas sobre minic.c.
 */

#ifndef MINIC_SEMANTIC_C_INCLUDED
#define MINIC_SEMANTIC_C_INCLUDED

typedef struct {
    const char *base;
    bool is_array;
} SemType;

static const SemType SEM_ERROR_TYPE = {"<erro>", false};

typedef enum {
    SEM_SYMBOL_VARIABLE,
    SEM_SYMBOL_PARAMETER,
    SEM_SYMBOL_FUNCTION
} SemSymbolKind;

typedef struct SemSymbol {
    const char *name;
    SemSymbolKind kind;
    SemType type;
    int line;
    int column;
    SemType *parameters;
    size_t parameter_count;
    struct SemSymbol *next;
} SemSymbol;

typedef struct SemScope {
    struct SemScope *parent;
    SemSymbol *symbols;
} SemScope;

typedef struct {
    char code[8];
    int line;
    int column;
    char *message;
} SemDiagnostic;

typedef struct {
    SemDiagnostic *items;
    size_t count;
    size_t capacity;
} SemDiagnosticList;

typedef struct {
    SemDiagnosticList diagnostics;
    SemScope global_scope;
    ASTNode *current_function;
    int loop_depth;
} SemanticAnalyzerC;

static SemType sem_type(const char *base, bool is_array)
{
    SemType type;
    type.base = base;
    type.is_array = is_array;
    return type;
}

static bool sem_type_equal(SemType a, SemType b)
{
    return a.is_array == b.is_array && strcmp(a.base, b.base) == 0;
}

static bool sem_is_error(SemType type)
{
    return strcmp(type.base, "<erro>") == 0;
}

static const char *sem_type_label(SemType type, char buffer[64])
{
    if (type.is_array) {
        snprintf(buffer, 64, "%s[]", type.base);
        return buffer;
    }
    return type.base;
}

static bool sem_is_numeric(SemType type)
{
    return !type.is_array &&
           (strcmp(type.base, "int") == 0 || strcmp(type.base, "float") == 0);
}

static bool sem_compatible(SemType expected, SemType actual)
{
    if (sem_is_error(expected) || sem_is_error(actual)) {
        return true;
    }
    if (expected.is_array || actual.is_array) {
        return sem_type_equal(expected, actual);
    }
    if (strcmp(expected.base, actual.base) == 0) {
        return true;
    }
    return strcmp(expected.base, "float") == 0 && strcmp(actual.base, "int") == 0;
}

static bool sem_comparable(SemType left, SemType right)
{
    if (left.is_array || right.is_array) {
        return false;
    }
    if (strcmp(left.base, right.base) == 0) {
        return true;
    }
    return sem_is_numeric(left) && sem_is_numeric(right);
}

static void sem_scope_init(SemScope *scope, SemScope *parent)
{
    scope->parent = parent;
    scope->symbols = NULL;
}

static void sem_scope_free(SemScope *scope)
{
    SemSymbol *symbol = scope->symbols;
    while (symbol != NULL) {
        SemSymbol *next = symbol->next;
        free(symbol->parameters);
        free(symbol);
        symbol = next;
    }
    scope->symbols = NULL;
}

static SemSymbol *sem_scope_local(SemScope *scope, const char *name)
{
    SemSymbol *symbol;
    for (symbol = scope->symbols; symbol != NULL; symbol = symbol->next) {
        if (strcmp(symbol->name, name) == 0) {
            return symbol;
        }
    }
    return NULL;
}

static SemSymbol *sem_scope_resolve(SemScope *scope, const char *name)
{
    SemScope *current = scope;
    while (current != NULL) {
        SemSymbol *found = sem_scope_local(current, name);
        if (found != NULL) {
            return found;
        }
        current = current->parent;
    }
    return NULL;
}

static void sem_diag_init(SemDiagnosticList *list)
{
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void sem_diag_free(SemDiagnosticList *list)
{
    size_t i;
    for (i = 0; i < list->count; ++i) {
        free(list->items[i].message);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static char *sem_vformat(const char *format, va_list args)
{
    va_list copy;
    int needed;
    char *buffer;

    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);

    if (needed < 0) {
        return xstrdup("erro semântico");
    }

    buffer = xmalloc((size_t)needed + 1);
    vsnprintf(buffer, (size_t)needed + 1, format, args);
    return buffer;
}

static void sem_error(
    SemanticAnalyzerC *analyzer,
    const char *code,
    int line,
    int column,
    const char *format,
    ...
)
{
    va_list args;
    SemDiagnostic *diag;

    if (analyzer->diagnostics.count == analyzer->diagnostics.capacity) {
        analyzer->diagnostics.capacity = analyzer->diagnostics.capacity == 0
            ? 8
            : analyzer->diagnostics.capacity * 2;
        analyzer->diagnostics.items = xrealloc(
            analyzer->diagnostics.items,
            analyzer->diagnostics.capacity * sizeof(*analyzer->diagnostics.items)
        );
    }

    diag = &analyzer->diagnostics.items[analyzer->diagnostics.count++];
    snprintf(diag->code, sizeof(diag->code), "%s", code);
    diag->line = line;
    diag->column = column;

    va_start(args, format);
    diag->message = sem_vformat(format, args);
    va_end(args);
}

static bool sem_declare(
    SemanticAnalyzerC *analyzer,
    SemScope *scope,
    const char *name,
    SemSymbolKind kind,
    SemType type,
    int line,
    int column,
    const SemType *parameters,
    size_t parameter_count
)
{
    SemSymbol *previous = sem_scope_local(scope, name);
    SemSymbol *symbol;

    if (previous != NULL) {
        char type_buffer[64];
        sem_error(
            analyzer,
            "SEM002",
            line,
            column,
            "“%s” já declarado neste escopo; declaração anterior na linha %d, coluna %d (tipo %s).",
            name,
            previous->line,
            previous->column,
            sem_type_label(previous->type, type_buffer)
        );
        return false;
    }

    symbol = xcalloc(1, sizeof(*symbol));
    symbol->name = name;
    symbol->kind = kind;
    symbol->type = type;
    symbol->line = line;
    symbol->column = column;
    symbol->parameter_count = parameter_count;

    if (parameter_count > 0) {
        symbol->parameters = xmalloc(parameter_count * sizeof(*symbol->parameters));
        memcpy(
            symbol->parameters,
            parameters,
            parameter_count * sizeof(*symbol->parameters)
        );
    }

    symbol->next = scope->symbols;
    scope->symbols = symbol;
    return true;
}

static void sem_position(ASTNode *node, int *line, int *column)
{
    if (node != NULL && node->line > 0 && node->column > 0) {
        *line = node->line;
        *column = node->column;
        return;
    }

    if (node != NULL) {
        switch (node->kind) {
            case NODE_BINARY:
                sem_position(node->as.binary.left, line, column);
                return;
            case NODE_UNARY:
                sem_position(node->as.unary.operand, line, column);
                return;
            case NODE_ASSIGN:
                sem_position(node->as.assign.target, line, column);
                return;
            case NODE_CALL:
                sem_position(node->as.call.callee, line, column);
                return;
            case NODE_INDEX:
                sem_position(node->as.index.base, line, column);
                return;
            default:
                break;
        }
    }

    *line = 1;
    *column = 1;
}

static void sem_expr_append(StringBuffer *buffer, ASTNode *node)
{
    size_t i;

    if (node == NULL) {
        sb_append(buffer, "expressão");
        return;
    }

    switch (node->kind) {
        case NODE_ID:
            sb_append(buffer, node->as.identifier.name);
            break;

        case NODE_LIT:
            sb_append(buffer, node->as.literal.lexeme);
            break;

        case NODE_BINARY:
            sem_expr_append(buffer, node->as.binary.left);
            sb_append_char(buffer, ' ');
            sb_append(buffer, node->as.binary.operator_lexeme);
            sb_append_char(buffer, ' ');
            sem_expr_append(buffer, node->as.binary.right);
            break;

        case NODE_UNARY:
            sb_append(buffer, node->as.unary.operator_lexeme);
            sem_expr_append(buffer, node->as.unary.operand);
            break;

        case NODE_CALL:
            sem_expr_append(buffer, node->as.call.callee);
            sb_append_char(buffer, '(');
            for (i = 0; i < node->as.call.arguments.count; ++i) {
                if (i > 0) {
                    sb_append(buffer, ", ");
                }
                sem_expr_append(buffer, node->as.call.arguments.items[i]);
            }
            sb_append_char(buffer, ')');
            break;

        case NODE_INDEX:
            sem_expr_append(buffer, node->as.index.base);
            sb_append_char(buffer, '[');
            sem_expr_append(buffer, node->as.index.index);
            sb_append_char(buffer, ']');
            break;

        case NODE_ASSIGN:
            sem_expr_append(buffer, node->as.assign.target);
            sb_append(buffer, " = ");
            sem_expr_append(buffer, node->as.assign.value);
            break;

        default:
            sb_append(buffer, "expressão");
            break;
    }
}

static char *sem_expr_text(ASTNode *node)
{
    StringBuffer buffer;
    sb_init(&buffer);
    sem_expr_append(&buffer, node);
    return sb_take(&buffer);
}

static SemSymbol *sem_resolve_id(
    SemanticAnalyzerC *analyzer,
    SemScope *scope,
    ASTNode *id_node
)
{
    SemSymbol *symbol = sem_scope_resolve(scope, id_node->as.identifier.name);
    if (symbol == NULL) {
        sem_error(
            analyzer,
            "SEM001",
            id_node->line,
            id_node->column,
            "Identificador “%s” não declarado neste escopo.",
            id_node->as.identifier.name
        );
    }
    return symbol;
}

static SemType sem_infer_expr(
    SemanticAnalyzerC *analyzer,
    ASTNode *node,
    SemScope *scope,
    const char *value_context
);

static SemType sem_infer_index(
    SemanticAnalyzerC *analyzer,
    ASTNode *node,
    SemScope *scope
)
{
    SemType base_type = sem_infer_expr(analyzer, node->as.index.base, scope, "expressão");
    SemType index_type = sem_infer_expr(analyzer, node->as.index.index, scope, "expressão");

    if (!sem_is_error(index_type) &&
        !(strcmp(index_type.base, "int") == 0 && !index_type.is_array)) {
        int line, column;
        char type_buffer[64];
        char *base_text = sem_expr_text(node->as.index.base);
        char *index_text = sem_expr_text(node->as.index.index);
        sem_position(node->as.index.index, &line, &column);
        sem_error(
            analyzer,
            "SEM006",
            line,
            column,
            "Índice do vetor “%s” deve ser int; recebeu %s (expressão “%s”).",
            base_text,
            sem_type_label(index_type, type_buffer),
            index_text
        );
        free(base_text);
        free(index_text);
    }

    if (sem_is_error(base_type) || !base_type.is_array) {
        return SEM_ERROR_TYPE;
    }

    return sem_type(base_type.base, false);
}

static SemType sem_infer_lvalue(
    SemanticAnalyzerC *analyzer,
    ASTNode *node,
    SemScope *scope
)
{
    if (node->kind == NODE_ID) {
        SemSymbol *symbol = sem_resolve_id(analyzer, scope, node);
        if (symbol == NULL || symbol->kind == SEM_SYMBOL_FUNCTION || symbol->type.is_array) {
            return SEM_ERROR_TYPE;
        }
        return symbol->type;
    }

    if (node->kind == NODE_INDEX) {
        return sem_infer_index(analyzer, node, scope);
    }

    return SEM_ERROR_TYPE;
}

static void sem_check_assignment(
    SemanticAnalyzerC *analyzer,
    SemType destination,
    SemType source_type,
    const char *destination_text,
    ASTNode *source_node
)
{
    int line, column;
    char source_buffer[64];
    char dest_buffer[64];
    char *source_text;

    if (sem_is_error(destination) || sem_is_error(source_type) ||
        sem_compatible(destination, source_type)) {
        return;
    }

    sem_position(source_node, &line, &column);
    source_text = sem_expr_text(source_node);

    sem_error(
        analyzer,
        "SEM003",
        line,
        column,
        "Não é possível atribuir %s a %s sem conversão permitida (destino “%s”; expressão “%s”).",
        sem_type_label(source_type, source_buffer),
        sem_type_label(destination, dest_buffer),
        destination_text,
        source_text
    );

    free(source_text);
}

static SemType sem_infer_call(
    SemanticAnalyzerC *analyzer,
    ASTNode *node,
    SemScope *scope,
    const char *value_context
)
{
    SemSymbol *symbol;
    size_t i;
    SemType *actual_types = NULL;

    if (node->as.call.callee == NULL || node->as.call.callee->kind != NODE_ID) {
        for (i = 0; i < node->as.call.arguments.count; ++i) {
            (void)sem_infer_expr(
                analyzer,
                node->as.call.arguments.items[i],
                scope,
                "expressão"
            );
        }
        return SEM_ERROR_TYPE;
    }

    symbol = sem_scope_resolve(scope, node->as.call.callee->as.identifier.name);
    if (symbol == NULL &&
        (strcmp(node->as.call.callee->as.identifier.name, "print") == 0 ||
         strcmp(node->as.call.callee->as.identifier.name, "read") == 0)) {
        for (i = 0; i < node->as.call.arguments.count; ++i) {
            (void)sem_infer_expr(analyzer, node->as.call.arguments.items[i], scope, "expressão");
        }
        return sem_type("void", false);
    }
    if (symbol == NULL) {
        sem_error(
            analyzer,
            "SEM001",
            node->as.call.callee->line,
            node->as.call.callee->column,
            "Identificador “%s” não declarado neste escopo.",
            node->as.call.callee->as.identifier.name
        );
        for (i = 0; i < node->as.call.arguments.count; ++i) {
            (void)sem_infer_expr(
                analyzer,
                node->as.call.arguments.items[i],
                scope,
                "expressão"
            );
        }
        return SEM_ERROR_TYPE;
    }

    if (symbol->kind != SEM_SYMBOL_FUNCTION) {
        return SEM_ERROR_TYPE;
    }

    if (node->as.call.arguments.count > 0) {
        actual_types = xmalloc(node->as.call.arguments.count * sizeof(*actual_types));
    }

    for (i = 0; i < node->as.call.arguments.count; ++i) {
        actual_types[i] = sem_infer_expr(
            analyzer,
            node->as.call.arguments.items[i],
            scope,
            "expressão"
        );
    }

    if (node->as.call.arguments.count != symbol->parameter_count) {
        sem_error(
            analyzer,
            "SEM007",
            node->as.call.callee->line,
            node->as.call.callee->column,
            "“%s” espera %zu argumentos, mas recebeu %zu.",
            symbol->name,
            symbol->parameter_count,
            node->as.call.arguments.count
        );
    } else {
        for (i = 0; i < symbol->parameter_count; ++i) {
            if (!sem_is_error(actual_types[i]) &&
                !sem_compatible(symbol->parameters[i], actual_types[i])) {
                int line, column;
                char expected_buffer[64];
                char actual_buffer[64];
                char *argument_text = sem_expr_text(node->as.call.arguments.items[i]);
                sem_position(node->as.call.arguments.items[i], &line, &column);
                sem_error(
                    analyzer,
                    "SEM008",
                    line,
                    column,
                    "Argumento %zu de “%s”: esperado %s, recebido %s (expressão “%s”).",
                    i + 1,
                    symbol->name,
                    sem_type_label(symbol->parameters[i], expected_buffer),
                    sem_type_label(actual_types[i], actual_buffer),
                    argument_text
                );
                free(argument_text);
            }
        }
    }

    free(actual_types);

    if (strcmp(symbol->type.base, "void") == 0 && strcmp(value_context, "comando") != 0) {
        sem_error(
            analyzer,
            "SEM012",
            node->as.call.callee->line,
            node->as.call.callee->column,
            "Função “%s” não produz valor (retorno void) e não pode ser usada como expressão de %s.",
            symbol->name,
            value_context
        );
        return SEM_ERROR_TYPE;
    }

    return symbol->type;
}

static const char *sem_non_assignable_description(ASTNode *node, char **owned)
{
    StringBuffer buffer;
    const char *kind = "literal";

    *owned = NULL;

    if (node->kind != NODE_LIT) {
        char *text = sem_expr_text(node);
        sb_init(&buffer);
        sb_append(&buffer, "a expressão “");
        sb_append(&buffer, text);
        sb_append(&buffer, "”");
        free(text);
        *owned = sb_take(&buffer);
        return *owned;
    }

    if (strcmp(node->as.literal.literal_type, "int") == 0) {
        kind = "literal inteiro";
    } else if (strcmp(node->as.literal.literal_type, "real") == 0) {
        kind = "literal real";
    } else if (strcmp(node->as.literal.literal_type, "bool") == 0) {
        kind = "literal booleano";
    } else if (strcmp(node->as.literal.literal_type, "char") == 0) {
        kind = "literal caractere";
    } else if (strcmp(node->as.literal.literal_type, "string") == 0) {
        kind = "literal string";
    }

    sb_init(&buffer);
    sb_append(&buffer, "o ");
    sb_append(&buffer, kind);
    sb_append(&buffer, " “");
    sb_append(&buffer, node->as.literal.lexeme);
    sb_append(&buffer, "”");
    *owned = sb_take(&buffer);
    return *owned;
}

static SemType sem_infer_assignment(
    SemanticAnalyzerC *analyzer,
    ASTNode *node,
    SemScope *scope
)
{
    SemType target_type;
    SemType value_type;
    char *target_text;

    if (node->as.assign.target->kind != NODE_ID &&
        node->as.assign.target->kind != NODE_INDEX) {
        int line, column;
        char *owned = NULL;
        const char *description;

        (void)sem_infer_expr(analyzer, node->as.assign.value, scope, "atribuição");
        sem_position(node->as.assign.target, &line, &column);
        description = sem_non_assignable_description(node->as.assign.target, &owned);
        sem_error(
            analyzer,
            "SEM013",
            line,
            column,
            "Destino de atribuição não é atribuível; %s não designa uma variável ou elemento de vetor.",
            description
        );
        free(owned);
        return SEM_ERROR_TYPE;
    }

    target_type = sem_infer_lvalue(analyzer, node->as.assign.target, scope);
    value_type = sem_infer_expr(analyzer, node->as.assign.value, scope, "atribuição");

    if (sem_is_error(target_type) || sem_is_error(value_type)) {
        return SEM_ERROR_TYPE;
    }

    target_text = sem_expr_text(node->as.assign.target);
    sem_check_assignment(
        analyzer,
        target_type,
        value_type,
        target_text,
        node->as.assign.value
    );
    free(target_text);
    return target_type;
}

static SemType sem_infer_expr(
    SemanticAnalyzerC *analyzer,
    ASTNode *node,
    SemScope *scope,
    const char *value_context
)
{
    if (node == NULL) {
        return SEM_ERROR_TYPE;
    }

    switch (node->kind) {
        case NODE_LIT:
            if (strcmp(node->as.literal.literal_type, "real") == 0) {
                return sem_type("float", false);
            }
            return sem_type(node->as.literal.literal_type, false);

        case NODE_ID: {
            SemSymbol *symbol = sem_resolve_id(analyzer, scope, node);
            return symbol != NULL ? symbol->type : SEM_ERROR_TYPE;
        }

        case NODE_INDEX:
            return sem_infer_index(analyzer, node, scope);

        case NODE_CALL:
            return sem_infer_call(analyzer, node, scope, value_context);

        case NODE_ASSIGN:
            return sem_infer_assignment(analyzer, node, scope);

        case NODE_UNARY: {
            SemType operand = sem_infer_expr(
                analyzer,
                node->as.unary.operand,
                scope,
                "expressão"
            );
            if (sem_is_error(operand)) {
                return SEM_ERROR_TYPE;
            }
            if (strcmp(node->as.unary.operator_lexeme, "!") == 0) {
                return sem_type_equal(operand, sem_type("bool", false))
                    ? sem_type("bool", false)
                    : SEM_ERROR_TYPE;
            }
            if ((strcmp(node->as.unary.operator_lexeme, "+") == 0 ||
                 strcmp(node->as.unary.operator_lexeme, "-") == 0) &&
                sem_is_numeric(operand)) {
                return operand;
            }
            return SEM_ERROR_TYPE;
        }

        case NODE_BINARY: {
            SemType left = sem_infer_expr(
                analyzer,
                node->as.binary.left,
                scope,
                "expressão"
            );
            SemType right = sem_infer_expr(
                analyzer,
                node->as.binary.right,
                scope,
                "expressão"
            );
            const char *op = node->as.binary.operator_lexeme;

            if (sem_is_error(left) || sem_is_error(right)) {
                return SEM_ERROR_TYPE;
            }

            if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 ||
                strcmp(op, "*") == 0 || strcmp(op, "/") == 0 ||
                strcmp(op, "%") == 0) {
                if (sem_is_numeric(left) && sem_is_numeric(right)) {
                    if (strcmp(left.base, "float") == 0 ||
                        strcmp(right.base, "float") == 0) {
                        return sem_type("float", false);
                    }
                    return sem_type("int", false);
                }
                {
                    char left_type[64], right_type[64];
                    sem_error(analyzer, "SEM015", node->as.binary.left->line,
                        node->as.binary.left->column,
                        "Operadores aritméticos exigem operandos numéricos (recebeu %s e %s).",
                        sem_type_label(left, left_type), sem_type_label(right, right_type));
                }
                return SEM_ERROR_TYPE;
            }

            if (strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
                strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) {
                return sem_is_numeric(left) && sem_is_numeric(right)
                    ? sem_type("bool", false)
                    : SEM_ERROR_TYPE;
            }

            if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) {
                return sem_comparable(left, right)
                    ? sem_type("bool", false)
                    : SEM_ERROR_TYPE;
            }

            if (strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) {
                return sem_type_equal(left, sem_type("bool", false)) &&
                       sem_type_equal(right, sem_type("bool", false))
                    ? sem_type("bool", false)
                    : SEM_ERROR_TYPE;
            }

            return SEM_ERROR_TYPE;
        }

        default:
            return SEM_ERROR_TYPE;
    }
}

static void sem_check_condition(
    SemanticAnalyzerC *analyzer,
    const char *command,
    ASTNode *expression,
    SemType actual
)
{
    int line, column;
    char type_buffer[64];
    char *expression_text;

    if (sem_is_error(actual) || sem_type_equal(actual, sem_type("bool", false))) {
        return;
    }

    sem_position(expression, &line, &column);
    expression_text = sem_expr_text(expression);
    sem_error(
        analyzer,
        "SEM005",
        line,
        column,
        "Condição de %s deve ter tipo bool; recebeu %s (expressão “%s”).",
        command,
        sem_type_label(actual, type_buffer),
        expression_text
    );
    free(expression_text);
}

static bool sem_always_returns(ASTNode *node)
{
    size_t i;

    if (node == NULL) {
        return false;
    }

    if (node->kind == NODE_RETURN) {
        return true;
    }

    if (node->kind == NODE_BLOCK) {
        for (i = 0; i < node->as.block.items.count; ++i) {
            if (sem_always_returns(node->as.block.items.items[i])) {
                return true;
            }
        }
        return false;
    }

    if (node->kind == NODE_IF) {
        return node->as.if_stmt.else_branch != NULL &&
               sem_always_returns(node->as.if_stmt.then_branch) &&
               sem_always_returns(node->as.if_stmt.else_branch);
    }

    return false;
}

static char *sem_fallthrough_detail(ASTNode *body)
{
    if (body != NULL && body->kind == NODE_BLOCK && body->as.block.items.count > 0) {
        ASTNode *last = body->as.block.items.items[body->as.block.items.count - 1];
        if (last->kind == NODE_IF && last->as.if_stmt.else_branch == NULL) {
            char *condition = sem_expr_text(last->as.if_stmt.condition);
            StringBuffer buffer;
            sb_init(&buffer);
            sb_append(&buffer, "o ramo em que “");
            sb_append(&buffer, condition);
            sb_append(&buffer, "” é falso alcança o fim do corpo.");
            free(condition);
            return sb_take(&buffer);
        }
    }

    return xstrdup("há um caminho de execução que alcança o fim do corpo.");
}

static void sem_analyze_statement(
    SemanticAnalyzerC *analyzer,
    ASTNode *node,
    SemScope *scope
);

static void sem_analyze_var_decl(
    SemanticAnalyzerC *analyzer,
    ASTNode *declaration,
    SemScope *scope
)
{
    SemType declared_type = sem_type(
        declaration->as.var_decl.type_name,
        declaration->as.var_decl.size != NULL
    );

    (void)sem_declare(
        analyzer,
        scope,
        declaration->as.var_decl.name,
        SEM_SYMBOL_VARIABLE,
        declared_type,
        declaration->line,
        declaration->column,
        NULL,
        0
    );

    if (declaration->as.var_decl.size != NULL) {
        SemType dimension = sem_infer_expr(
            analyzer,
            declaration->as.var_decl.size,
            scope,
            "expressão"
        );
        if (!sem_is_error(dimension) &&
            !sem_type_equal(dimension, sem_type("int", false))) {
            sem_error(analyzer, "SEM016", declaration->as.var_decl.size->line,
                      declaration->as.var_decl.size->column,
                      "Dimensão do vetor deve ser int.");
        }
    }

    if (declaration->as.var_decl.initializer != NULL) {
        SemType source_type = sem_infer_expr(
            analyzer,
            declaration->as.var_decl.initializer,
            scope,
            "inicialização"
        );
        sem_check_assignment(
            analyzer,
            sem_type(declaration->as.var_decl.type_name, false),
            source_type,
            declaration->as.var_decl.name,
            declaration->as.var_decl.initializer
        );
    }
}

static void sem_analyze_block(
    SemanticAnalyzerC *analyzer,
    ASTNode *block,
    SemScope *parent,
    bool create_scope
)
{
    SemScope child;
    SemScope *scope = parent;
    size_t i;

    if (create_scope) {
        sem_scope_init(&child, parent);
        scope = &child;
    }

    for (i = 0; i < block->as.block.items.count; ++i) {
        ASTNode *item = block->as.block.items.items[i];
        if (item->kind == NODE_VAR_DECL) {
            sem_analyze_var_decl(analyzer, item, scope);
        } else {
            sem_analyze_statement(analyzer, item, scope);
        }
    }

    if (create_scope) {
        sem_scope_free(&child);
    }
}

static void sem_analyze_return(
    SemanticAnalyzerC *analyzer,
    ASTNode *node,
    SemScope *scope
)
{
    SemType expected;
    SemType actual;
    int line, column;

    if (analyzer->current_function == NULL) {
        return;
    }

    expected = sem_type(analyzer->current_function->as.function.return_type, false);

    if (node->as.return_stmt.value == NULL) {
        if (strcmp(expected.base, "void") != 0) {
            sem_error(
                analyzer,
                "SEM009",
                analyzer->current_function->line,
                analyzer->current_function->column,
                "Retorno vazio incompatível com o tipo %s da função “%s”.",
                expected.base,
                analyzer->current_function->as.function.name
            );
        }
        return;
    }

    actual = sem_infer_expr(
        analyzer,
        node->as.return_stmt.value,
        scope,
        "retorno"
    );

    if (strcmp(expected.base, "void") == 0) {
        if (!sem_is_error(actual)) {
            sem_position(node->as.return_stmt.value, &line, &column);
            sem_error(
                analyzer,
                "SEM009",
                line,
                column,
                "Função “%s” possui retorno void e não pode retornar um valor.",
                analyzer->current_function->as.function.name
            );
        }
        return;
    }

    if (sem_is_error(actual) || sem_compatible(expected, actual)) {
        return;
    }

    sem_position(node->as.return_stmt.value, &line, &column);

    if (strcmp(expected.base, "int") == 0 && strcmp(actual.base, "float") == 0) {
        sem_error(
            analyzer,
            "SEM009",
            line,
            column,
            "Retorno float incompatível com o tipo int da função “%s”; conversão implícita de float para int não permitida.",
            analyzer->current_function->as.function.name
        );
    } else {
        char actual_buffer[64];
        char expected_buffer[64];
        sem_error(
            analyzer,
            "SEM009",
            line,
            column,
            "Retorno %s incompatível com o tipo %s da função “%s”.",
            sem_type_label(actual, actual_buffer),
            sem_type_label(expected, expected_buffer),
            analyzer->current_function->as.function.name
        );
    }
}

static void sem_analyze_statement(
    SemanticAnalyzerC *analyzer,
    ASTNode *node,
    SemScope *scope
)
{
    if (node == NULL) {
        return;
    }

    switch (node->kind) {
        case NODE_BLOCK:
            sem_analyze_block(analyzer, node, scope, true);
            break;

        case NODE_IF: {
            SemType condition_type = sem_infer_expr(
                analyzer,
                node->as.if_stmt.condition,
                scope,
                "expressão"
            );
            sem_check_condition(
                analyzer,
                "if",
                node->as.if_stmt.condition,
                condition_type
            );
            sem_analyze_statement(analyzer, node->as.if_stmt.then_branch, scope);
            if (node->as.if_stmt.else_branch != NULL) {
                sem_analyze_statement(analyzer, node->as.if_stmt.else_branch, scope);
            }
            break;
        }

        case NODE_WHILE: {
            SemType condition_type = sem_infer_expr(
                analyzer,
                node->as.while_stmt.condition,
                scope,
                "expressão"
            );
            sem_check_condition(
                analyzer,
                "while",
                node->as.while_stmt.condition,
                condition_type
            );
            analyzer->loop_depth++;
            sem_analyze_statement(analyzer, node->as.while_stmt.body, scope);
            analyzer->loop_depth--;
            break;
        }

        case NODE_BREAK:
            if (analyzer->loop_depth == 0) {
                sem_error(analyzer, "SEM014", node->line, node->column,
                          "break fora de um laço.");
            }
            break;

        case NODE_RETURN:
            sem_analyze_return(analyzer, node, scope);
            break;

        case NODE_EXPR_STMT:
            (void)sem_infer_expr(
                analyzer,
                node->as.expr_stmt.expression,
                scope,
                "comando"
            );
            break;

        case NODE_VAR_DECL:
            sem_analyze_var_decl(analyzer, node, scope);
            break;

        default:
            break;
    }
}

static void sem_analyze_function(
    SemanticAnalyzerC *analyzer,
    ASTNode *function
)
{
    SemScope function_scope;
    ASTNode *previous_function = analyzer->current_function;
    size_t i;

    sem_scope_init(&function_scope, &analyzer->global_scope);
    analyzer->current_function = function;

    for (i = 0; i < function->as.function.parameters.count; ++i) {
        Parameter *parameter = &function->as.function.parameters.items[i];
        (void)sem_declare(
            analyzer,
            &function_scope,
            parameter->name,
            SEM_SYMBOL_PARAMETER,
            sem_type(parameter->type_name, parameter->is_array),
            parameter->line,
            parameter->column,
            NULL,
            0
        );
    }

    sem_analyze_block(
        analyzer,
        function->as.function.body,
        &function_scope,
        false
    );

    if (strcmp(function->as.function.return_type, "void") != 0 &&
        !sem_always_returns(function->as.function.body)) {
        char *detail = sem_fallthrough_detail(function->as.function.body);
        sem_error(
            analyzer,
            "SEM011",
            function->line,
            function->column,
            "A função “%s” pode terminar sem retornar %s; %s",
            function->as.function.name,
            function->as.function.return_type,
            detail
        );
        free(detail);
    }

    sem_scope_free(&function_scope);
    analyzer->current_function = previous_function;
}

static void sem_analyzer_init(SemanticAnalyzerC *analyzer)
{
    sem_diag_init(&analyzer->diagnostics);
    sem_scope_init(&analyzer->global_scope, NULL);
    analyzer->current_function = NULL;
    analyzer->loop_depth = 0;
}

static void sem_analyzer_free(SemanticAnalyzerC *analyzer)
{
    sem_scope_free(&analyzer->global_scope);
    sem_diag_free(&analyzer->diagnostics);
}

static void sem_analyze_program(SemanticAnalyzerC *analyzer, ASTNode *program)
{
    size_t i;

    /* Primeira passagem: símbolos globais e assinaturas. */
    for (i = 0; i < program->as.program.items.count; ++i) {
        ASTNode *item = program->as.program.items.items[i];

        if (item->kind == NODE_FUNCTION) {
            size_t j;
            SemType *parameters = NULL;

            if (item->as.function.parameters.count > 0) {
                parameters = xmalloc(
                    item->as.function.parameters.count * sizeof(*parameters)
                );
            }

            for (j = 0; j < item->as.function.parameters.count; ++j) {
                Parameter *parameter = &item->as.function.parameters.items[j];
                parameters[j] = sem_type(
                    parameter->type_name,
                    parameter->is_array
                );
            }

            (void)sem_declare(
                analyzer,
                &analyzer->global_scope,
                item->as.function.name,
                SEM_SYMBOL_FUNCTION,
                sem_type(item->as.function.return_type, false),
                item->line,
                item->column,
                parameters,
                item->as.function.parameters.count
            );
            free(parameters);

        } else if (item->kind == NODE_VAR_DECL) {
            (void)sem_declare(
                analyzer,
                &analyzer->global_scope,
                item->as.var_decl.name,
                SEM_SYMBOL_VARIABLE,
                sem_type(
                    item->as.var_decl.type_name,
                    item->as.var_decl.size != NULL
                ),
                item->line,
                item->column,
                NULL,
                0
            );
        }
    }

    /* Segunda passagem: corpos e expressões. */
    for (i = 0; i < program->as.program.items.count; ++i) {
        ASTNode *item = program->as.program.items.items[i];
        if (item->kind == NODE_FUNCTION) {
            sem_analyze_function(analyzer, item);
        } else if (item->kind == NODE_VAR_DECL) {
            if (item->as.var_decl.size != NULL) {
                (void)sem_infer_expr(
                    analyzer,
                    item->as.var_decl.size,
                    &analyzer->global_scope,
                    "expressão"
                );
            }
            if (item->as.var_decl.initializer != NULL) {
                SemType source_type = sem_infer_expr(
                    analyzer,
                    item->as.var_decl.initializer,
                    &analyzer->global_scope,
                    "inicialização"
                );
                sem_check_assignment(
                    analyzer,
                    sem_type(item->as.var_decl.type_name, false),
                    source_type,
                    item->as.var_decl.name,
                    item->as.var_decl.initializer
                );
            }
        } else {
            sem_analyze_statement(analyzer, item, &analyzer->global_scope);
        }
    }
}

static void sem_write_result(const SemDiagnosticList *diagnostics)
{
    size_t i;
    char summary[160];

    for (i = 0; i < diagnostics->count; ++i) {
        const SemDiagnostic *diag = &diagnostics->items[i];
        if (i > 0) {
            fwrite("\n", 1, 1, stdout);
        }
        fprintf(
            stdout,
            "%s — linha %d, coluna %d: %s",
            diag->code,
            diag->line,
            diag->column,
            diag->message
        );
    }

    if (diagnostics->count > 0) {
        fwrite("\n", 1, 1, stdout);
        snprintf(
            summary,
            sizeof(summary),
            "Análise semântica concluída: %zu %s; programa rejeitado.",
            diagnostics->count,
            diagnostics->count == 1 ? "erro" : "erros"
        );
    } else {
        snprintf(
            summary,
            sizeof(summary),
            "Análise semântica concluída: 0 erros; programa aceito."
        );
    }

    fwrite(summary, 1, strlen(summary), stdout);
}

#endif /* MINIC_SEMANTIC_C_INCLUDED */
