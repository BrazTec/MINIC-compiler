/*
 * ir.c - gerador de código intermediário de três endereços (TAC)
 * A entrada é a AST já validada pelo analisador semântico.
 * Incluído por minic.c após parser.c e semantic.c, sem invocar subprocessos.
 */
#ifndef MINIC_IR_C_INCLUDED
#define MINIC_IR_C_INCLUDED

typedef struct IRName {
    const char *source;
    char *target;
    int depth;
    struct IRName *next;
} IRName;

typedef struct IRCount {
    const char *name;
    int count;
    struct IRCount *next;
} IRCount;

typedef struct {
    StringBuffer result;
    int temp_count;
    int label_count;
    int depth;
    IRName *names;
    IRCount *counts;
    const char *break_stack[64];
    int breaks;
    char **owned;
    size_t owned_count;
    size_t owned_capacity;
    int optimization;
} IRWriter;

static void ir_keep(IRWriter *w, char *owned)
{
    if (w->owned_count == w->owned_capacity) {
        w->owned_capacity = w->owned_capacity ? 2 * w->owned_capacity : 32;
        w->owned = xrealloc(w->owned, w->owned_capacity * sizeof(*w->owned));
    }
    w->owned[w->owned_count++] = owned;
}

static char *ir_format(IRWriter *w, const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int length;
    char *value;
    va_start(args, fmt);
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (length < 0) { va_end(copy); return ""; }
    value = xmalloc((size_t)length + 1);
    (void)vsnprintf(value, (size_t)length + 1, fmt, copy);
    va_end(copy);
    ir_keep(w, value);
    return value;
}

static void ir_line(IRWriter *w, bool indented, const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int length;
    char *line;
    va_start(args, fmt);
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (length < 0) { va_end(copy); return; }
    line = xmalloc((size_t)length + 1);
    (void)vsnprintf(line, (size_t)length + 1, fmt, copy);
    va_end(copy);
    if (indented) sb_append(&w->result, "  ");
    sb_append(&w->result, line);
    sb_append_char(&w->result, '\n');
    free(line);
}

static char *ir_temp(IRWriter *w)
{
    return ir_format(w, "t%d", ++w->temp_count);
}

static char *ir_label(IRWriter *w, const char *kind)
{
    return ir_format(w, "L%d_%s", ++w->label_count, kind);
}

static void ir_push(IRWriter *w) { w->depth++; }

static void ir_pop(IRWriter *w)
{
    IRName **entry = &w->names;
    while (*entry != NULL) {
        IRName *current = *entry;
        if (current->depth == w->depth) {
            *entry = current->next;
            free(current);
        } else {
            entry = &current->next;
        }
    }
    w->depth--;
}

static const char *ir_declare(IRWriter *w, const char *source)
{
    IRCount *count = w->counts;
    IRName *new_name;
    while (count != NULL && strcmp(count->name, source) != 0) {
        count = count->next;
    }
    if (count == NULL) {
        count = xcalloc(1, sizeof(*count));
        count->name = source;
        count->next = w->counts;
        w->counts = count;
    }
    count->count++;
    new_name = xmalloc(sizeof(*new_name));
    new_name->source = source;
    new_name->target = (count->count == 1)
        ? (char *)source : ir_format(w, "%s#%d", source, count->count);
    new_name->depth = w->depth;
    new_name->next = w->names;
    w->names = new_name;
    return new_name->target;
}

static const char *ir_resolve(IRWriter *w, const char *source)
{
    IRName *name;
    for (name = w->names; name != NULL; name = name->next) {
        if (strcmp(name->source, source) == 0) return name->target;
    }
    return source;
}

static const char *ir_expression(IRWriter *w, ASTNode *node, bool value_needed);
static void ir_branch_false(IRWriter *w, ASTNode *condition, const char *target);
static void ir_branch_true(IRWriter *w, ASTNode *condition, const char *target);
static void ir_statement(IRWriter *w, ASTNode *node, bool function_body);

static void ir_location(IRWriter *w, ASTNode *node,
                        const char **base, const char **offset)
{
    const char *index;
    *base = ir_expression(w, node->as.index.base, true);
    index = ir_expression(w, node->as.index.index, true);
    *offset = ir_temp(w);
    ir_line(w, true, "%s = %s * 1", *offset, index);
}

static const char *ir_expression(IRWriter *w, ASTNode *node, bool value_needed)
{
    const char *left, *right, *dest;
    size_t i;
    if (node == NULL) return "";
    switch (node->kind) {
        case NODE_LIT:
            return node->as.literal.lexeme;
        case NODE_ID:
            return ir_resolve(w, node->as.identifier.name);
        case NODE_ASSIGN:
            right = ir_expression(w, node->as.assign.value, true);
            if (node->as.assign.target->kind == NODE_ID) {
                dest = ir_resolve(w, node->as.assign.target->as.identifier.name);
                ir_line(w, true, "%s = %s", dest, right);
                return dest;
            }
            if (node->as.assign.target->kind == NODE_INDEX) {
                const char *base, *offset;
                ir_location(w, node->as.assign.target, &base, &offset);
                ir_line(w, true, "store %s[%s], %s", base, offset, right);
                return right;
            }
            return "<lvalue-invalido>";
        case NODE_BINARY:
            if (strcmp(node->as.binary.operator_lexeme, "&&") == 0 ||
                strcmp(node->as.binary.operator_lexeme, "||") == 0) {
                const char *f = ir_label(w, "bool_false");
                const char *end = ir_label(w, "bool_end");
                dest = ir_temp(w);
                ir_branch_false(w, node, f);
                ir_line(w, true, "%s = true", dest);
                ir_line(w, true, "goto %s", end);
                ir_line(w, false, "%s:", f);
                ir_line(w, true, "%s = false", dest);
                ir_line(w, false, "%s:", end);
                return dest;
            }
            left = ir_expression(w, node->as.binary.left, true);
            right = ir_expression(w, node->as.binary.right, true);
            dest = ir_temp(w);
            ir_line(w, true, "%s = %s %s %s", dest, left,
                    node->as.binary.operator_lexeme, right);
            return dest;
        case NODE_UNARY:
            left = ir_expression(w, node->as.unary.operand, true);
            dest = ir_temp(w);
            ir_line(w, true, "%s = %s %s", dest,
                    strcmp(node->as.unary.operator_lexeme, "!") == 0
                        ? "not" : node->as.unary.operator_lexeme, left);
            return dest;
        case NODE_CALL: {
            ASTNode *callee = node->as.call.callee;
            const char *name = callee && callee->kind == NODE_ID
                ? callee->as.identifier.name : "<chamada>";
            size_t count = node->as.call.arguments.count;
            const char **arguments = xcalloc(count ? count : 1, sizeof(*arguments));
            for (i = 0; i < count; ++i) {
                arguments[i] = ir_expression(w, node->as.call.arguments.items[i], true);
            }
            if (strcmp(name, "print") == 0 || strcmp(name, "read") == 0) {
                for (i = 0; i < count; ++i)
                    ir_line(w, true, "%s %s", name, arguments[i]);
                free(arguments);
                return "void";
            }
            for (i = 0; i < count; ++i) ir_line(w, true, "param %s", arguments[i]);
            free(arguments);
            if (value_needed) {
                dest = ir_temp(w);
                ir_line(w, true, "%s = call %s, %zu", dest, name, count);
                return dest;
            }
            ir_line(w, true, "call %s, %zu", name, count);
            return "void";
        }
        case NODE_INDEX: {
            const char *base, *offset;
            ir_location(w, node, &base, &offset);
            dest = ir_temp(w);
            ir_line(w, true, "%s = load %s[%s]", dest, base, offset);
            return dest;
        }
        default: return "<expressao-nao-tratada>";
    }
}

static void ir_branch_false(IRWriter *w, ASTNode *node, const char *target)
{
    if (node->kind == NODE_UNARY &&
        strcmp(node->as.unary.operator_lexeme, "!") == 0) {
        ir_branch_true(w, node->as.unary.operand, target);
    } else if (node->kind == NODE_BINARY &&
               strcmp(node->as.binary.operator_lexeme, "&&") == 0) {
        ir_branch_false(w, node->as.binary.left, target);
        ir_branch_false(w, node->as.binary.right, target);
    } else if (node->kind == NODE_BINARY &&
               strcmp(node->as.binary.operator_lexeme, "||") == 0) {
        const char *skip = ir_label(w, "or_true");
        ir_branch_true(w, node->as.binary.left, skip);
        ir_branch_false(w, node->as.binary.right, target);
        ir_line(w, false, "%s:", skip);
    } else {
        ir_line(w, true, "ifFalse %s goto %s", ir_expression(w, node, true), target);
    }
}

static void ir_branch_true(IRWriter *w, ASTNode *node, const char *target)
{
    if (node->kind == NODE_UNARY &&
        strcmp(node->as.unary.operator_lexeme, "!") == 0) {
        ir_branch_false(w, node->as.unary.operand, target);
    } else if (node->kind == NODE_BINARY &&
               strcmp(node->as.binary.operator_lexeme, "||") == 0) {
        ir_branch_true(w, node->as.binary.left, target);
        ir_branch_true(w, node->as.binary.right, target);
    } else if (node->kind == NODE_BINARY &&
               strcmp(node->as.binary.operator_lexeme, "&&") == 0) {
        const char *skip = ir_label(w, "and_false");
        ir_branch_false(w, node->as.binary.left, skip);
        ir_branch_true(w, node->as.binary.right, target);
        ir_line(w, false, "%s:", skip);
    } else {
        ir_line(w, true, "ifTrue %s goto %s", ir_expression(w, node, true), target);
    }
}

static void ir_statement(IRWriter *w, ASTNode *node, bool function_body)
{
    size_t i;
    const char *value;
    if (node == NULL) return;
    switch (node->kind) {
        case NODE_BLOCK:
            if (!function_body) ir_push(w);
            for (i = 0; i < node->as.block.items.count; ++i)
                ir_statement(w, node->as.block.items.items[i], false);
            if (!function_body) ir_pop(w);
            break;
        case NODE_VAR_DECL:
            value = ir_declare(w, node->as.var_decl.name);
            if (node->as.var_decl.size != NULL) {
                const char *size = ir_expression(w, node->as.var_decl.size, true);
                ir_line(w, true, "alloc %s, %s", value, size);
            }
            if (node->as.var_decl.initializer != NULL) {
                const char *init = ir_expression(w, node->as.var_decl.initializer, true);
                ir_line(w, true, "%s = %s", value, init);
            }
            break;
        case NODE_EXPR_STMT:
            (void)ir_expression(w, node->as.expr_stmt.expression, false);
            break;
        case NODE_RETURN:
            if (node->as.return_stmt.value)
                ir_line(w, true, "return %s",
                        ir_expression(w, node->as.return_stmt.value, true));
            else ir_line(w, true, "return");
            break;
        case NODE_IF: {
            const char *else_label = ir_label(w,
                node->as.if_stmt.else_branch ? "else" : "endif");
            const char *end = node->as.if_stmt.else_branch ?
                ir_label(w, "endif") : else_label;
            ir_branch_false(w, node->as.if_stmt.condition, else_label);
            ir_statement(w, node->as.if_stmt.then_branch, false);
            if (node->as.if_stmt.else_branch != NULL) {
                ir_line(w, true, "goto %s", end);
                ir_line(w, false, "%s:", else_label);
                ir_statement(w, node->as.if_stmt.else_branch, false);
            }
            ir_line(w, false, "%s:", end);
            break;
        }
        case NODE_WHILE: {
            const char *testing = ir_label(w, "while");
            const char *ending = ir_label(w, "endwhile");
            ir_line(w, false, "%s:", testing);
            ir_branch_false(w, node->as.while_stmt.condition, ending);
            if (w->breaks < 64) w->break_stack[w->breaks++] = ending;
            ir_statement(w, node->as.while_stmt.body, false);
            if (w->breaks > 0) --w->breaks;
            ir_line(w, true, "goto %s", testing);
            ir_line(w, false, "%s:", ending);
            break;
        }
        case NODE_BREAK:
            if (w->breaks > 0)
                ir_line(w, true, "goto %s", w->break_stack[w->breaks - 1]);
            break;
        default: break;
    }
}

static char *ir_generate(ASTNode *program, int optimization)
{
    IRWriter writer;
    size_t i, j;
    IRCount *count;
    memset(&writer, 0, sizeof(writer));
    sb_init(&writer.result);
    writer.optimization = optimization;
    ir_push(&writer);
    for (i = 0; i < program->as.program.items.count; ++i) {
        ASTNode *item = program->as.program.items.items[i];
        if (item->kind == NODE_VAR_DECL) (void)ir_declare(&writer, item->as.var_decl.name);
    }
    for (i = 0; i < program->as.program.items.count; ++i) {
        ASTNode *item = program->as.program.items.items[i];
        if (item->kind != NODE_FUNCTION) continue;
        writer.temp_count = 0;
        writer.label_count = 0;
        ir_push(&writer);
        if (item->as.function.parameters.count == 0) {
            ir_line(&writer, false, "func %s", item->as.function.name);
        } else {
            StringBuffer params;
            sb_init(&params);
            for (j = 0; j < item->as.function.parameters.count; ++j) {
                const char *name = ir_declare(&writer,
                    item->as.function.parameters.items[j].name);
                if (j > 0) sb_append(&params, ", ");
                sb_append(&params, name);
            }
            ir_line(&writer, false, "func %s(%s)", item->as.function.name, params.data);
            free(params.data);
        }
        ir_statement(&writer, item->as.function.body, true);
        ir_line(&writer, false, "endfunc");
        ir_pop(&writer);
    }
    ir_pop(&writer);
    for (i = 0; i < writer.owned_count; ++i) free(writer.owned[i]);
    free(writer.owned);
    for (count = writer.counts; count != NULL;) {
        IRCount *next = count->next;
        free(count);
        count = next;
    }
    return sb_take(&writer.result);
}

/*
 * -O1: otimização local e conservadora de TAC.
 * Dobra de constantes, propagação de cópias e identidades algébricas.
 * Zera fatos quando ocorre mudança de bloco/efeitos que podem invalidá-los.
 */
typedef struct IRFact {
    char name[128];
    char value[128];
    struct IRFact *next;
} IRFact;

static void ir_fact_clear(IRFact **facts)
{
    while (*facts != NULL) {
        IRFact *next = (*facts)->next;
        free(*facts);
        *facts = next;
    }
}

static void ir_fact_kill(IRFact **facts, const char *name)
{
    IRFact **it = facts;
    while (*it != NULL) {
        if (strcmp((*it)->name, name) == 0 ||
            strcmp((*it)->value, name) == 0) {
            IRFact *old = *it;
            *it = old->next;
            free(old);
        } else it = &(*it)->next;
    }
}

static void ir_fact_set(IRFact **facts, const char *name, const char *value)
{
    IRFact *fact;
    if (strlen(name) > 127 || strlen(value) > 127) return;
    ir_fact_kill(facts, name);
    fact = xcalloc(1, sizeof(*fact));
    strcpy(fact->name, name);
    strcpy(fact->value, value);
    fact->next = *facts;
    *facts = fact;
}

static const char *ir_fact_get(IRFact *facts, const char *name)
{
    int steps = 0;
    while (steps++ < 64) {
        IRFact *it;
        bool changed = false;
        for (it = facts; it != NULL; it = it->next) {
            if (strcmp(it->name, name) == 0 && strcmp(it->value, name) != 0) {
                name = it->value;
                changed = true;
                break;
            }
        }
        if (!changed) break;
    }
    return name;
}

static bool ir_parse_number(const char *s, double *out, bool *integral)
{
    char *end;
    if (strcmp(s, "true") == 0) { *out = 1; *integral = false; return true; }
    if (strcmp(s, "false") == 0) { *out = 0; *integral = false; return true; }
    *out = strtod(s, &end);
    if (end == s || *end != '\0') return false;
    *integral = strchr(s, '.') == NULL && strchr(s, 'e') == NULL && strchr(s, 'E') == NULL;
    return true;
}

static bool ir_fold_number(const char *left, const char *op, const char *right,
                           char output[128])
{
    double a, b, c;
    bool ai, bi, result_bool = false, is_bool = false;
    if (!ir_parse_number(left, &a, &ai) || !ir_parse_number(right, &b, &bi))
        return false;
    if (strcmp(op, "+") == 0) c = a + b;
    else if (strcmp(op, "-") == 0) c = a - b;
    else if (strcmp(op, "*") == 0) c = a * b;
    else if (strcmp(op, "/") == 0) {
        if (b == 0) return false;
        c = (ai && bi) ? (double)((long long)(a / b)) : a / b;
    } else if (strcmp(op, "%") == 0) {
        long long x, y;
        if (!ai || !bi || b == 0) return false;
        x = (long long)a; y = (long long)b;
        c = (double)(x % y);
    } else if (strcmp(op, "<") == 0) { is_bool = true; result_bool = a < b; c = 0; }
    else if (strcmp(op, "<=") == 0) { is_bool = true; result_bool = a <= b; c = 0; }
    else if (strcmp(op, ">") == 0) { is_bool = true; result_bool = a > b; c = 0; }
    else if (strcmp(op, ">=") == 0) { is_bool = true; result_bool = a >= b; c = 0; }
    else if (strcmp(op, "==") == 0) { is_bool = true; result_bool = a == b; c = 0; }
    else if (strcmp(op, "!=") == 0) { is_bool = true; result_bool = a != b; c = 0; }
    else return false;
    if (is_bool) snprintf(output, 128, "%s", result_bool ? "true" : "false");
    else if (ai && bi) snprintf(output, 128, "%lld", (long long)c);
    else snprintf(output, 128, "%.15g", c);
    return true;
}

/*
 * Elimina somente atribuicoes puras a temporarios sem consumidores.
 * Usa analise de uso de identificadores (nao comparacao textual solta) e
 * repete ate atingir ponto fixo. Instrucoes com efeitos (call, store,
 * print, read, param, saltos) nunca sao removidas.
 *
 * Esta etapa corresponde a DCE conservadora do otimizador Python.
 */
static bool ir_temp_definition(const char *line, char name[128])
{
    const char *p = line;
    size_t n;
    while (*p == ' ') ++p;
    if (*p++ != 't' || !isdigit((unsigned char)*p)) return false;
    while (isdigit((unsigned char)*p)) ++p;
    n = (size_t)(p - line);
    /* O nome nao inclui os espacos de indentacao. */
    {
        const char *start = line;
        while (*start == ' ') ++start;
        n = (size_t)(p - start);
        if (n >= 128) return false;
        memcpy(name, start, n);
        name[n] = '\0';
    }
    while (*p == ' ') ++p;
    if (*p++ != '=') return false;
    while (*p == ' ') ++p;
    if (strncmp(p, "call ", 5) == 0 ||
        strncmp(p, "read", 4) == 0) return false;
    return true;
}

static bool ir_token_boundary(char ch)
{
    return !isalnum((unsigned char)ch) && ch != '_' && ch != '#';
}

static bool ir_line_uses_temp(const char *line, const char *temp)
{
    const char *search = line;
    const char *start = line;
    size_t n = strlen(temp);
    char defined[128];
    /* Ignore o identificador antes de '=': uma definicao nao e uso. */
    if (ir_temp_definition(line, defined)) {
        const char *equal = strchr(line, '=');
        if (equal != NULL) start = equal + 1;
    }
    search = start;
    while ((search = strstr(search, temp)) != NULL) {
        char before = search == start ? ' ' : search[-1];
        char after = search[n];
        if (ir_token_boundary(before) &&
            (after == '\0' || ir_token_boundary(after))) return true;
        search += n;
    }
    return false;
}

static char *ir_dce_temporaries(const char *input)
{
    char **lines = NULL;
    bool *removed = NULL;
    size_t n = 0, cap = 0, i, j;
    const char *cursor = input;
    bool changed;
    const size_t input_size = strlen(input);
    const bool had_trailing_newline = input_size > 0 && input[input_size - 1] == '\n';
    StringBuffer output;
    /* O texto ja esta renderizado; cada linha e uma instrucao TAC. */
    while (*cursor != '\0') {
        const char *end = strchr(cursor, '\n');
        size_t width = end ? (size_t)(end - cursor) : strlen(cursor);
        if (n == cap) {
            size_t next = cap ? cap * 2 : 32;
            char **more = realloc(lines, next * sizeof(*lines));
            bool *flags;
            if (more == NULL) { fputs("IR: sem memoria\n", stderr); exit(2); }
            lines = more;
            flags = realloc(removed, next * sizeof(*removed));
            if (flags == NULL) { fputs("IR: sem memoria\n", stderr); exit(2); }
            removed = flags;
            cap = next;
        }
        lines[n] = xstrndup_local(cursor, width);
        removed[n] = false;
        ++n;
        if (end == NULL) break;
        cursor = end + 1;
    }
    do {
        changed = false;
        for (i = 0; i < n; ++i) {
            char temp[128];
            bool used = false;
            if (removed[i] || !ir_temp_definition(lines[i], temp)) continue;
            for (j = 0; j < n; ++j) {
                if (!removed[j] && ir_line_uses_temp(lines[j], temp)) {
                    used = true;
                    break;
                }
            }
            if (!used) { removed[i] = true; changed = true; }
        }
    } while (changed);
    sb_init(&output);
    for (i = 0; i < n; ++i) {
        if (!removed[i]) {
            if (output.length) sb_append_char(&output, '\n');
            sb_append(&output, lines[i]);
        }
        free(lines[i]);
    }
    free(lines);
    free(removed);
    if (had_trailing_newline && output.length) sb_append_char(&output, '\n');
    return sb_take(&output);
}

static char *ir_optimize_text(const char *input)
{
    StringBuffer output;
    IRFact *facts = NULL;
    const char *cursor = input;
    sb_init(&output);
    while (*cursor != '\0') {
        const char *end = strchr(cursor, '\n');
        size_t width = end ? (size_t)(end - cursor) : strlen(cursor);
        char *line = xstrndup_local(cursor, width);
        char dest[128] = {0}, a[128] = {0}, op[32] = {0}, b[128] = {0};
        char rendered[512] = {0};
        bool changed = false;
        const char *trimmed = line;
        while (*trimmed == ' ') ++trimmed;
        if (sscanf(trimmed, "store %127[^[][%127[^]]], %127s", dest, a, b) == 3 &&
            strncmp(trimmed, "store ", 6) == 0) {
            /* Python: store resolve indice e valor antes de invalidar fatos. */
            char value[128] = {0};
            char base[128] = {0};
            char index[128] = {0};
            if (sscanf(trimmed, "store %127[^[][%127[^]]], %127s", base, index, value) == 3) {
                snprintf(rendered, sizeof(rendered), "  store %s[%s], %s",
                         base, ir_fact_get(facts, index), ir_fact_get(facts, value));
                changed = true;
            }
            ir_fact_clear(&facts);
        } else if (sscanf(trimmed, "%127s = load %127[^[][%127[^]]]", dest, a, b) == 3 &&
                   strstr(trimmed, " = load ") != NULL) {
            char resolved_index[128];
            snprintf(resolved_index, sizeof(resolved_index), "%s", ir_fact_get(facts, b));
            ir_fact_kill(&facts, dest);
            snprintf(rendered, sizeof(rendered), "  %s = load %s[%s]", dest, a, resolved_index);
            changed = true;
        } else if (strncmp(trimmed, "ifFalse ", 8) == 0 ||
            strncmp(trimmed, "ifTrue ", 7) == 0) {
            char condition[128] = {0}, target[128] = {0};
            const bool is_false = strncmp(trimmed, "ifFalse ", 8) == 0;
            if (sscanf(trimmed, is_false ? "ifFalse %127s goto %127s" :
                                      "ifTrue %127s goto %127s",
                       condition, target) == 2) {
                snprintf(rendered, sizeof(rendered), "  %s %s goto %s",
                         is_false ? "ifFalse" : "ifTrue",
                         ir_fact_get(facts, condition), target);
                changed = true;
            }
            ir_fact_clear(&facts);
        } else if (strncmp(trimmed, "func ", 5) == 0 ||
            strcmp(trimmed, "endfunc") == 0 ||
            strncmp(trimmed, "goto ", 5) == 0 ||
            strncmp(trimmed, "ifFalse ", 8) == 0 ||
            strncmp(trimmed, "ifTrue ", 7) == 0 ||
            (width && line[width - 1] == ':') ||
            strncmp(trimmed, "call ", 5) == 0 ||
            strncmp(trimmed, "store ", 6) == 0 ||
            strncmp(trimmed, "read ", 5) == 0) {
            ir_fact_clear(&facts);
        } else if (sscanf(trimmed, "%127s = %127s %31s %127s", dest, a, op, b) == 4 &&
                   strcmp(a, "call") != 0 && strcmp(a, "load") != 0) {
            const char *left = ir_fact_get(facts, a);
            const char *right = ir_fact_get(facts, b);
            char result[128];
            bool folded = ir_fold_number(left, op, right, result);
            if (!folded) {
                if ((strcmp(op, "+") == 0 || strcmp(op, "-") == 0) && strcmp(right, "0") == 0) { snprintf(result, sizeof(result), "%s", left); folded = true; }
                else if (strcmp(op, "+") == 0 && strcmp(left, "0") == 0) { snprintf(result, sizeof(result), "%s", right); folded = true; }
                else if (strcmp(op, "*") == 0 && strcmp(left, "1") == 0) { snprintf(result, sizeof(result), "%s", right); folded = true; }
                else if (strcmp(op, "*") == 0 && strcmp(right, "1") == 0) { snprintf(result, sizeof(result), "%s", left); folded = true; }
                else if (strcmp(op, "*") == 0 && (strcmp(right, "0") == 0 || strcmp(left, "0") == 0)) { snprintf(result, sizeof(result), "0"); folded = true; }
            }
            ir_fact_kill(&facts, dest);
            if (folded) {
                snprintf(rendered, sizeof(rendered), "  %s = %s", dest, result);
                ir_fact_set(&facts, dest, result);
            } else snprintf(rendered, sizeof(rendered), "  %s = %s %s %s", dest, left, op, right);
            changed = true;
        } else if (sscanf(trimmed, "%127s = %127s", dest, a) == 2 &&
                   !strstr(trimmed, " call ") && !strstr(trimmed, " load ")) {
            const char *source = ir_fact_get(facts, a);
            ir_fact_kill(&facts, dest);
            ir_fact_set(&facts, dest, source);
            snprintf(rendered, sizeof(rendered), "  %s = %s", dest, source);
            changed = true;
        } else if (sscanf(trimmed, "print %127s", a) == 1) {
            snprintf(rendered, sizeof(rendered), "  print %s", ir_fact_get(facts, a));
            changed = true;
        } else if (sscanf(trimmed, "return %127s", a) == 1) {
            snprintf(rendered, sizeof(rendered), "  return %s", ir_fact_get(facts, a));
            changed = true;
            ir_fact_clear(&facts);
        } else if (sscanf(trimmed, "param %127s", a) == 1) {
            snprintf(rendered, sizeof(rendered), "  param %s", ir_fact_get(facts, a));
            changed = true;
        }
        sb_append(&output, changed ? rendered : line);
        if (end != NULL) sb_append_char(&output, '\n');
        free(line);
        if (end == NULL) break;
        cursor = end + 1;
    }
    ir_fact_clear(&facts);
    {
        char *folded = sb_take(&output);
        char *cleaned = ir_dce_temporaries(folded);
        free(folded);
        return cleaned;
    }
}

#include "ir_passes.c"
#endif
