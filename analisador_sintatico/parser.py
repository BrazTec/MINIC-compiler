#!/usr/bin/env python3
"""
Analisador sintático (parser) da linguagem MINIC.

Uso:
    python parser.py caminho/para/codigo.c

Lê o arquivo-fonte, faz a análise léxica (tokens) e a análise sintática
(recursive descent), construindo uma AST. Se o programa for aceito,
imprime a AST no formato de S-expressão (ver README de testes-parser-50).
Se houver erro sintático, imprime a mensagem de erro em stderr e termina
com código de saída diferente de zero (sem imprimir AST).
"""

import sys


# ============================================================
# LEXER (analisador léxico) - autocontido para uso pelo parser
# ============================================================

RESERVED_WORDS = {
    "int": "INT", "float": "FLOAT", "bool": "BOOL", "char": "CHAR", "void": "VOID",
    "if": "IF", "else": "ELSE",
    "while": "WHILE", "for": "FOR",
    "return": "RETURN", "break": "BREAK", "continue": "CONTINUE",
    "true": "TRUE", "false": "FALSE",
    "print": "PRINT", "read": "READ",
}

TWO_CHAR_TOKENS = {
    "==": "EQ", "!=": "NE", "<=": "LE", ">=": "GE", "&&": "AND", "||": "OR",
}

ONE_CHAR_TOKENS = {
    "+": "PLUS", "-": "MINUS", "*": "STAR", "/": "SLASH", "%": "PERCENT",
    "<": "LT", ">": "GT", "!": "NOT", "=": "ASSIGN",
    "(": "LPAREN", ")": "RPAREN", "[": "LBRACKET", "]": "RBRACKET",
    "{": "LBRACE", "}": "RBRACE", ";": "SEMICOLON", ",": "COMMA", ".": "DOT",
}

ESCAPES = {"n": "\n", "t": "\t", "\\": "\\", "'": "'", '"': '"'}


class Token:
    __slots__ = ("type", "lexeme", "value", "line", "column")

    def __init__(self, type_, lexeme, value, line, column):
        self.type = type_
        self.lexeme = lexeme
        self.value = value
        self.line = line
        self.column = column

    def __repr__(self):
        return f"Token({self.type},{self.lexeme!r},{self.line}:{self.column})"


class LexError(Exception):
    pass


def is_letter(ch):
    return bool(ch) and (("a" <= ch <= "z") or ("A" <= ch <= "Z"))


def is_ident_start(ch):
    return is_letter(ch) or ch == "_"


def is_ident_part(ch):
    return is_ident_start(ch) or (bool(ch) and "0" <= ch <= "9")


def is_digit(ch):
    return bool(ch) and "0" <= ch <= "9"


class Lexer:
    def __init__(self, source):
        self.src = source
        self.n = len(source)
        self.pos = 0
        self.line = 1
        self.col = 1

    def at_end(self):
        return self.pos >= self.n

    def cur(self):
        return "" if self.at_end() else self.src[self.pos]

    def peek(self, off=1):
        idx = self.pos + off
        return "" if idx >= self.n else self.src[idx]

    def advance(self):
        if self.at_end():
            return ""
        ch = self.src[self.pos]
        self.pos += 1
        if ch == "\n":
            self.line += 1
            self.col = 1
        else:
            self.col += 1
        return ch

    def tokenize(self):
        tokens = []
        while True:
            self.skip_trivia()
            if self.at_end():
                tokens.append(Token("EOF", "", None, self.line, self.col))
                break
            ch = self.cur()
            line, col = self.line, self.col

            if is_ident_start(ch):
                tokens.append(self.scan_ident(line, col))
                continue
            if is_digit(ch):
                tokens.append(self.scan_number(line, col))
                continue
            if ch == '"':
                tokens.append(self.scan_string(line, col))
                continue
            if ch == "'":
                tokens.append(self.scan_char(line, col))
                continue

            two = ch + self.peek()
            if two in TWO_CHAR_TOKENS:
                self.advance()
                self.advance()
                tokens.append(Token(TWO_CHAR_TOKENS[two], two, None, line, col))
                continue

            if ch in ONE_CHAR_TOKENS:
                self.advance()
                tokens.append(Token(ONE_CHAR_TOKENS[ch], ch, None, line, col))
                continue

            raise LexError(f"símbolo desconhecido '{ch}' na linha {line}, coluna {col}")
        return tokens

    def skip_trivia(self):
        while not self.at_end():
            ch = self.cur()
            if ch in " \t\r\n":
                self.advance()
                continue
            if ch == "/" and self.peek() == "/":
                while not self.at_end() and self.cur() != "\n":
                    self.advance()
                continue
            if ch == "/" and self.peek() == "*":
                start_line, start_col = self.line, self.col
                self.advance()
                self.advance()
                closed = False
                while not self.at_end():
                    if self.cur() == "*" and self.peek() == "/":
                        self.advance()
                        self.advance()
                        closed = True
                        break
                    self.advance()
                if not closed:
                    raise LexError(
                        f"comentário de bloco não terminado (linha {start_line}, coluna {start_col})"
                    )
                continue
            break

    def scan_ident(self, line, col):
        start = self.pos
        while is_ident_part(self.cur()):
            self.advance()
        lexeme = self.src[start:self.pos]
        ttype = RESERVED_WORDS.get(lexeme, "IDENT")
        value = lexeme if ttype == "IDENT" else None
        return Token(ttype, lexeme, value, line, col)

    def scan_number(self, line, col):
        start = self.pos
        while is_digit(self.cur()):
            self.advance()
        if self.cur() == "." and is_digit(self.peek()):
            self.advance()
            while is_digit(self.cur()):
                self.advance()
            lexeme = self.src[start:self.pos]
            return Token("FLOAT_LIT", lexeme, float(lexeme), line, col)
        lexeme = self.src[start:self.pos]
        return Token("INT_LIT", lexeme, int(lexeme), line, col)

    def scan_string(self, line, col):
        start = self.pos
        self.advance()  # consome "
        chars = []
        while not self.at_end() and self.cur() != "\n":
            ch = self.cur()
            if ch == '"':
                self.advance()
                lexeme = self.src[start:self.pos]
                return Token("STRING_LIT", lexeme, "".join(chars), line, col)
            if ch == "\\" and self.peek() in ESCAPES:
                self.advance()
                esc = self.advance()
                chars.append(ESCAPES[esc])
                continue
            chars.append(ch)
            self.advance()
        raise LexError(f"cadeia de caracteres não terminada (linha {line}, coluna {col})")

    def scan_char(self, line, col):
        start = self.pos
        self.advance()  # consome '
        if self.at_end() or self.cur() == "\n":
            raise LexError(f"caractere não terminado (linha {line}, coluna {col})")
        ch = self.cur()
        if ch == "\\" and self.peek() in ESCAPES:
            self.advance()
            value = ESCAPES[self.advance()]
        else:
            value = self.advance()
        if self.cur() != "'":
            raise LexError(f"caractere não terminado (linha {line}, coluna {col})")
        self.advance()
        lexeme = self.src[start:self.pos]
        return Token("CHAR_LIT", lexeme, value, line, col)


# ============================================================
# AST
# ============================================================

def join_nodes(nodes):
    return ",".join(str(n) for n in nodes)


class Program:
    def __init__(self, decls):
        self.decls = decls

    def __str__(self):
        return f"Program({join_nodes(self.decls)})"


class VarDecl:
    def __init__(self, type_, name, size=None, init=None):
        self.type = type_
        self.name = name
        self.size = size
        self.init = init

    def __str__(self):
        s = f"{self.type} {self.name}"
        if self.size is not None:
            s += f" size={self.size}"
        if self.init is not None:
            s += f"={self.init}"
        return f"VarDecl({s})"


class Param:
    def __init__(self, type_, name):
        self.type = type_
        self.name = name

    def __str__(self):
        return f"{self.type} {self.name}"


class Function:
    def __init__(self, ret_type, name, params, body):
        self.ret_type = ret_type
        self.name = name
        self.params = params
        self.body = body

    def __str__(self):
        params = ",".join(str(p) for p in self.params)
        return f"Function({self.ret_type} {self.name}({params}) {self.body})"


class Block:
    def __init__(self, stmts):
        self.stmts = stmts

    def __str__(self):
        return f"Block({join_nodes(self.stmts)})"


class If:
    def __init__(self, cond, then, els):
        self.cond = cond
        self.then = then
        self.els = els

    def __str__(self):
        els = self.els if self.els is not None else "NULL"
        return f"If({self.cond},{self.then},{els})"


class While:
    def __init__(self, cond, body):
        self.cond = cond
        self.body = body

    def __str__(self):
        return f"While({self.cond},{self.body})"


class Return:
    def __init__(self, expr):
        self.expr = expr

    def __str__(self):
        e = self.expr if self.expr is not None else "NULL"
        return f"Return({e})"


class ExprStmt:
    def __init__(self, expr):
        self.expr = expr

    def __str__(self):
        return f"ExprStmt({self.expr})"


class Assign:
    def __init__(self, target, expr):
        self.target = target
        self.expr = expr

    def __str__(self):
        return f"Assign({self.target},{self.expr})"


class Binary:
    def __init__(self, op, left, right):
        self.op = op
        self.left = left
        self.right = right

    def __str__(self):
        return f"Binary({self.op},{self.left},{self.right})"


class Unary:
    def __init__(self, op, expr):
        self.op = op
        self.expr = expr

    def __str__(self):
        return f"Unary({self.op},{self.expr})"


class Call:
    def __init__(self, callee, args):
        self.callee = callee
        self.args = args

    def __str__(self):
        parts = [str(self.callee)] + [str(a) for a in self.args]
        return f"Call({','.join(parts)})"


class Index:
    def __init__(self, base, index):
        self.base = base
        self.index = index

    def __str__(self):
        return f"Index({self.base},{self.index})"


class Id:
    def __init__(self, name):
        self.name = name

    def __str__(self):
        return f"Id({self.name})"


class Lit:
    def __init__(self, kind, value):
        self.kind = kind
        self.value = value

    def __str__(self):
        return f"Lit({self.kind},{self.value})"


# ============================================================
# PARSER
# ============================================================

TYPE_TOKENS = {"INT", "FLOAT", "BOOL", "CHAR", "VOID"}


class ParseError(Exception):
    pass


class Parser:
    def __init__(self, tokens):
        self.tokens = tokens
        self.pos = 0

    def cur(self):
        return self.tokens[self.pos]

    def check(self, *types):
        return self.cur().type in types

    def advance(self):
        tok = self.tokens[self.pos]
        if tok.type != "EOF":
            self.pos += 1
        return tok

    def expect(self, type_, expected_desc=None):
        if self.cur().type == type_:
            return self.advance()
        self.error(expected_desc or type_)

    def error(self, expected_desc):
        tok = self.cur()
        raise ParseError(
            f"Erro sintático na linha {tok.line}, coluna {tok.column}: "
            f"esperado {expected_desc}, encontrado '{tok.lexeme or tok.type}' ({tok.type})"
        )

    def unexpected(self, tok=None):
        tok = tok or self.cur()
        raise ParseError(
            f"Erro sintático na linha {tok.line}, coluna {tok.column}: "
            f"token {tok.type} inesperado ('{tok.lexeme}')"
        )

    # --------------------------------------------------------
    # Program
    # --------------------------------------------------------

    def parse_program(self):
        decls = []
        while not self.check("EOF"):
            decls.append(self.parse_global_decl())
        return Program(decls)

    def parse_global_decl(self):
        if not self.check(*TYPE_TOKENS):
            # Permite comandos soltos no escopo global (ex.: expressões),
            # reaproveitando a mesma gramática de statements.
            return self.parse_stmt()
        type_tok = self.advance()
        type_name = type_tok.lexeme

        name_tok = self.expect("IDENT", "IDENT")
        name = name_tok.lexeme

        if self.check("LPAREN"):
            return self.parse_function_tail(type_name, name)

        return self.parse_var_decl_tail(type_name, name)

    def parse_var_decl_tail(self, type_name, name):
        size = None
        init = None

        if self.check("LBRACKET"):
            self.advance()
            size = self.parse_expr()
            self.expect("RBRACKET", "FECHA_COLCHETE")
        elif self.check("ASSIGN"):
            self.advance()
            init = self.parse_expr()

        self.expect("SEMICOLON", "PONTO_E_VIRGULA")
        return VarDecl(type_name, name, size=size, init=init)

    def parse_function_tail(self, ret_type, name):
        self.expect("LPAREN", "ABRE_PAREN")
        params = []
        if not self.check("RPAREN"):
            params.append(self.parse_param())
            while self.check("COMMA"):
                self.advance()
                params.append(self.parse_param())
        self.expect("RPAREN", "IDENT ou FECHA_PAREN")
        body = self.parse_block()
        return Function(ret_type, name, params, body)

    def parse_param(self):
        if not self.check(*TYPE_TOKENS):
            self.error("KW_TIPO")
        type_tok = self.advance()
        name_tok = self.expect("IDENT", "IDENT")
        return Param(type_tok.lexeme, name_tok.lexeme)

    # --------------------------------------------------------
    # Statements
    # --------------------------------------------------------

    def parse_block(self):
        self.expect("LBRACE", "ABRE_CHAVE")
        stmts = []
        while not self.check("RBRACE", "EOF"):
            stmts.append(self.parse_stmt())
        self.expect("RBRACE", "FECHA_CHAVE")
        return Block(stmts)

    def parse_stmt(self):
        if self.check("LBRACE"):
            return self.parse_block()
        if self.check(*TYPE_TOKENS):
            return self.parse_local_var_decl()
        if self.check("IF"):
            return self.parse_if()
        if self.check("WHILE"):
            return self.parse_while()
        if self.check("RETURN"):
            return self.parse_return()
        if self.check("ELSE"):
            self.unexpected()
        if self.check("RBRACE", "EOF"):
            self.error("início de statement")
        return self.parse_expr_stmt()

    def parse_local_var_decl(self):
        type_tok = self.advance()
        name_tok = self.expect("IDENT", "IDENT")
        return self.parse_var_decl_tail(type_tok.lexeme, name_tok.lexeme)

    def parse_if(self):
        self.advance()
        self.expect("LPAREN", "ABRE_PAREN")
        cond = self.parse_expr()
        self.expect("RPAREN", "FECHA_PAREN")
        then = self.parse_stmt()
        els = None
        if self.check("ELSE"):
            self.advance()
            els = self.parse_stmt()
        return If(cond, then, els)

    def parse_while(self):
        self.advance()
        self.expect("LPAREN", "ABRE_PAREN")
        cond = self.parse_expr()
        self.expect("RPAREN", "FECHA_PAREN")
        body = self.parse_stmt()
        return While(cond, body)

    def parse_return(self):
        self.advance()
        expr = None
        if not self.check("SEMICOLON"):
            expr = self.parse_expr()
        self.expect("SEMICOLON", "PONTO_E_VIRGULA")
        return Return(expr)

    def parse_expr_stmt(self):
        expr = self.parse_expr()
        self.expect("SEMICOLON", "PONTO_E_VIRGULA")
        return ExprStmt(expr)

    # --------------------------------------------------------
    # Expressions (precedência crescente)
    # --------------------------------------------------------

    def parse_expr(self):
        return self.parse_assignment()

    def parse_assignment(self):
        left = self.parse_or()
        if self.check("ASSIGN"):
            if not isinstance(left, (Id, Index)):
                self.error("identificador ou índice de vetor à esquerda de '='")
            self.advance()
            right = self.parse_assignment()
            return Assign(left, right)
        return left

    def parse_or(self):
        left = self.parse_and()
        while self.check("OR"):
            op = self.advance().lexeme
            right = self.parse_and()
            left = Binary(op, left, right)
        return left

    def parse_and(self):
        left = self.parse_equality()
        while self.check("AND"):
            op = self.advance().lexeme
            right = self.parse_equality()
            left = Binary(op, left, right)
        return left

    def parse_equality(self):
        left = self.parse_relational()
        while self.check("EQ", "NE"):
            op = self.advance().lexeme
            right = self.parse_relational()
            left = Binary(op, left, right)
        return left

    def parse_relational(self):
        left = self.parse_additive()
        while self.check("LT", "GT", "LE", "GE"):
            op = self.advance().lexeme
            right = self.parse_additive()
            left = Binary(op, left, right)
        return left

    def parse_additive(self):
        left = self.parse_multiplicative()
        while self.check("PLUS", "MINUS"):
            op = self.advance().lexeme
            right = self.parse_multiplicative()
            left = Binary(op, left, right)
        return left

    def parse_multiplicative(self):
        left = self.parse_unary()
        while self.check("STAR", "SLASH", "PERCENT"):
            op = self.advance().lexeme
            right = self.parse_unary()
            left = Binary(op, left, right)
        return left

    def parse_unary(self):
        if self.check("MINUS", "NOT", "PLUS"):
            op = self.advance().lexeme
            expr = self.parse_unary()
            return Unary(op, expr)
        return self.parse_postfix()

    def parse_postfix(self):
        expr = self.parse_primary()
        while True:
            if self.check("LBRACKET"):
                self.advance()
                if self.check("RBRACKET"):
                    self.error("expressão")
                idx = self.parse_expr()
                self.expect("RBRACKET", "FECHA_COLCHETE")
                expr = Index(expr, idx)
            elif self.check("LPAREN"):
                self.advance()
                args = []
                if not self.check("RPAREN"):
                    args.append(self.parse_expr())
                    while self.check("COMMA"):
                        self.advance()
                        if self.check("RPAREN"):
                            self.error("FECHA_PAREN ou expressão")
                        args.append(self.parse_expr())
                self.expect("RPAREN", "FECHA_PAREN")
                expr = Call(expr, args)
            else:
                break
        return expr

    def parse_primary(self):
        tok = self.cur()

        if tok.type == "IDENT":
            self.advance()
            return Id(tok.lexeme)

        if tok.type == "INT_LIT":
            self.advance()
            return Lit("int", tok.value)

        if tok.type == "FLOAT_LIT":
            self.advance()
            return Lit("real", tok.value)

        if tok.type == "CHAR_LIT":
            self.advance()
            return Lit("char", repr(tok.value) if False else tok.value)

        if tok.type == "STRING_LIT":
            self.advance()
            return Lit("string", tok.value)

        if tok.type == "TRUE":
            self.advance()
            return Lit("bool", "true")

        if tok.type == "FALSE":
            self.advance()
            return Lit("bool", "false")

        if tok.type == "LPAREN":
            self.advance()
            expr = self.parse_expr()
            self.expect("RPAREN", "FECHA_PAREN")
            return expr

        self.error("identificador, literal ou '(' expressão ')'")


# ============================================================
# MAIN
# ============================================================

def main():
    if len(sys.argv) < 2:
        print("uso: python parser.py caminho/para/codigo.c", file=sys.stderr)
        sys.exit(2)

    path = sys.argv[1]
    try:
        with open(path, "r", encoding="utf-8") as f:
            source = f.read()
    except OSError as exc:
        print(f"Erro ao abrir arquivo: {exc}", file=sys.stderr)
        sys.exit(2)

    try:
        tokens = Lexer(source).tokenize()
    except LexError as exc:
        print(f"Erro léxico: {exc}", file=sys.stderr)
        sys.exit(1)

    try:
        ast = Parser(tokens).parse_program()
    except ParseError as exc:
        print(str(exc), file=sys.stderr)
        sys.exit(1)

    print(str(ast))
    sys.exit(0)


if __name__ == "__main__":
    main()
