#!/usr/bin/env python3

import json
import sys
from pathlib import Path


# ============================================================
# PALAVRAS RESERVADAS DA MINIC
# ============================================================

RESERVED_WORDS = {
    "int": "INT",
    "float": "FLOAT",
    "bool": "BOOL",
    "char": "CHAR",
    "void": "VOID",

    "if": "IF",
    "else": "ELSE",

    "while": "WHILE",
    "for": "FOR",

    "return": "RETURN",
    "break": "BREAK",
    "continue": "CONTINUE",

    "true": "TRUE",
    "false": "FALSE",

    "print": "PRINT",
    "read": "READ",
}


# ============================================================
# OPERADORES COMPOSTOS
#
# Devem ser testados antes dos operadores simples.
# Regra de "maximal munch".
# ============================================================

TWO_CHAR_TOKENS = {
    "==": "EQ",
    "!=": "NE",
    "<=": "LE",
    ">=": "GE",
    "&&": "AND",
    "||": "OR",
}


# ============================================================
# OPERADORES E DELIMITADORES DE UM CARACTERE
# ============================================================

ONE_CHAR_TOKENS = {
    # Aritméticos
    "+": "PLUS",
    "-": "MINUS",
    "*": "STAR",
    "/": "SLASH",
    "%": "PERCENT",

    # Relacionais
    "<": "LT",
    ">": "GT",

    # Lógico
    "!": "NOT",

    # Atribuição
    "=": "ASSIGN",

    # Delimitadores
    "(": "LPAREN",
    ")": "RPAREN",
    "[": "LBRACKET",
    "]": "RBRACKET",
    "{": "LBRACE",
    "}": "RBRACE",
    ";": "SEMICOLON",
    ",": "COMMA",

    # Necessário para o fixture do real malformado 12.
    ".": "DOT",
}


# ============================================================
# ESCAPES PERMITIDOS
# ============================================================

ESCAPES = {
    "n": "\n",
    "t": "\t",
    "\\": "\\",
    "'": "'",
    '"': '"',
}


# ============================================================
# FUNÇÕES AUXILIARES
# ============================================================

def is_ascii_letter(ch):
    """
    Retorna True somente para letras ASCII A-Z ou a-z.
    """
    return bool(ch) and (
        ("a" <= ch <= "z")
        or ("A" <= ch <= "Z")
    )


def is_identifier_start(ch):
    """
    Primeiro caractere de um identificador:
    letra ou sublinhado.
    """
    return is_ascii_letter(ch) or ch == "_"


def is_identifier_part(ch):
    """
    Demais caracteres de um identificador:
    letras, dígitos ou sublinhado.
    """
    return (
        is_identifier_start(ch)
        or (bool(ch) and "0" <= ch <= "9")
    )


def is_digit(ch):
    """
    Dígito ASCII.
    """
    return bool(ch) and "0" <= ch <= "9"


# ============================================================
# SCANNER / ANALISADOR LÉXICO
# ============================================================

class Scanner:

    def __init__(self, source):
        self.source = source
        self.length = len(source)

        # Posição absoluta dentro da string
        self.pos = 0

        # Linha e coluna começam em 1
        self.line = 1
        self.column = 1

        # Tokens e diagnósticos produzidos
        self.output = []

    # --------------------------------------------------------
    # CONTROLE DA ENTRADA
    # --------------------------------------------------------

    def at_end(self):
        return self.pos >= self.length

    def current(self):
        """
        Retorna o caractere atual.
        """
        if self.at_end():
            return ""

        return self.source[self.pos]

    def peek(self, offset=1):
        """
        Olha caracteres à frente sem avançar.
        """
        index = self.pos + offset

        if index >= self.length:
            return ""

        return self.source[index]

    def advance(self):
        """
        Consome um caractere e atualiza linha/coluna.
        """
        if self.at_end():
            return ""

        ch = self.source[self.pos]
        self.pos += 1

        if ch == "\n":
            self.line += 1
            self.column = 1
        else:
            self.column += 1

        return ch

    # --------------------------------------------------------
    # EMISSÃO DE TOKEN
    # --------------------------------------------------------

    def emit_token(
        self,
        token,
        lexeme,
        attribute,
        line,
        column
    ):
        self.output.append(
            {
                "token": token,
                "lexeme": lexeme,
                "attribute": attribute,
                "line": line,
                "column": column,
            }
        )

    # --------------------------------------------------------
    # EMISSÃO DE ERRO
    # --------------------------------------------------------

    def emit_error(
        self,
        error,
        lexeme,
        line,
        column
    ):
        self.output.append(
            {
                "error": error,
                "lexeme": lexeme,
                "line": line,
                "column": column,
            }
        )

    # ========================================================
    # COMENTÁRIO DE LINHA
    # ========================================================

    def skip_line_comment(self):
        """
        Ignora:
            // comentário

        A quebra de linha não é consumida aqui.
        """
        while (
            not self.at_end()
            and self.current() != "\n"
        ):
            self.advance()

    # ========================================================
    # COMENTÁRIO DE BLOCO
    # ========================================================

    def skip_block_comment(self):
        """
        Ignora:
            /* comentário */

        Se chegar ao EOF sem encontrar */,
        gera UNTERMINATED_BLOCK_COMMENT.
        """

        start_pos = self.pos
        start_line = self.line
        start_column = self.column

        # Consome /
        self.advance()

        # Consome *
        self.advance()

        while not self.at_end():

            if (
                self.current() == "*"
                and self.peek() == "/"
            ):
                self.advance()
                self.advance()
                return

            self.advance()

        # EOF sem */
        lexeme = self.source[start_pos:self.pos]

        self.emit_error(
            "UNTERMINATED_BLOCK_COMMENT",
            lexeme,
            start_line,
            start_column,
        )

    # ========================================================
    # IDENTIFICADORES E PALAVRAS RESERVADAS
    # ========================================================

    def scan_identifier(self):

        start_pos = self.pos
        start_line = self.line
        start_column = self.column

        while is_identifier_part(self.current()):
            self.advance()

        lexeme = self.source[start_pos:self.pos]

        token = RESERVED_WORDS.get(lexeme)

        # Palavra comum → IDENT
        if token is None:

            self.emit_token(
                "IDENT",
                lexeme,
                lexeme,
                start_line,
                start_column,
            )

        # Palavra reservada
        else:

            self.emit_token(
                token,
                lexeme,
                None,
                start_line,
                start_column,
            )

    # ========================================================
    # NÚMEROS
    # ========================================================

    def scan_number(self):

        start_pos = self.pos
        start_line = self.line
        start_column = self.column

        # Parte inteira
        while is_digit(self.current()):
            self.advance()

        integer_end = self.pos

        integer_lexeme = self.source[
            start_pos:integer_end
        ]

        # ----------------------------------------------------
        # REAL VÁLIDO
        #
        # [0-9]+\.[0-9]+
        # ----------------------------------------------------

        if (
            self.current() == "."
            and is_digit(self.peek())
        ):

            # Consome .
            self.advance()

            # Parte decimal
            while is_digit(self.current()):
                self.advance()

            lexeme = self.source[
                start_pos:self.pos
            ]

            self.emit_token(
                "FLOAT_LIT",
                lexeme,
                float(lexeme),
                start_line,
                start_column,
            )

            # Caso adicional:
            #
            # 12.5abc
            #
            # Não faz parte dos fixtures apresentados,
            # mas é tratado como identificador inválido.
            if is_identifier_start(self.current()):

                end = self.pos

                while (
                    end < self.length
                    and is_identifier_part(
                        self.source[end]
                    )
                ):
                    end += 1

                self.emit_error(
                    "INVALID_IDENTIFIER",
                    self.source[start_pos:end],
                    start_line,
                    start_column,
                )

            return

        # ----------------------------------------------------
        # REAL MALFORMADO
        #
        # Exemplo oficial:
        #
        # 12.
        #
        # Fixture esperado:
        #
        # INT_LIT 12
        # DOT .
        #
        # além do diagnóstico:
        #
        # MALFORMED_REAL_LITERAL "12."
        # ----------------------------------------------------

        if (
            self.current() == "."
            and not is_digit(self.peek())
        ):

            self.emit_token(
                "INT_LIT",
                integer_lexeme,
                int(integer_lexeme),
                start_line,
                start_column,
            )

            self.emit_error(
                "MALFORMED_REAL_LITERAL",
                integer_lexeme + ".",
                start_line,
                start_column,
            )

            # Importante:
            #
            # NÃO consumimos o ponto.
            #
            # Na próxima iteração ele será
            # reconhecido como DOT.
            return

        # ----------------------------------------------------
        # INTEIRO NORMAL
        # ----------------------------------------------------

        self.emit_token(
            "INT_LIT",
            integer_lexeme,
            int(integer_lexeme),
            start_line,
            start_column,
        )

        # ----------------------------------------------------
        # IDENTIFICADOR INICIADO POR DÍGITO
        #
        # Exemplo oficial:
        #
        # 123abc
        #
        # O fixture espera:
        #
        # INT_LIT 123
        # IDENT abc
        #
        # e também:
        #
        # INVALID_IDENTIFIER 123abc
        # ----------------------------------------------------

        if is_identifier_start(self.current()):

            end = self.pos

            while (
                end < self.length
                and is_identifier_part(
                    self.source[end]
                )
            ):
                end += 1

            self.emit_error(
                "INVALID_IDENTIFIER",
                self.source[start_pos:end],
                start_line,
                start_column,
            )

    # ========================================================
    # STRINGS
    # ========================================================

    def scan_string(self):

        start_pos = self.pos
        start_line = self.line
        start_column = self.column

        # Consome "
        self.advance()

        decoded = []

        while (
            not self.at_end()
            and self.current() != "\n"
        ):

            ch = self.current()

            # ----------------------------------------------
            # Fechamento normal
            # ----------------------------------------------

            if ch == '"':

                self.advance()

                lexeme = self.source[
                    start_pos:self.pos
                ]

                self.emit_token(
                    "STRING_LIT",
                    lexeme,
                    "".join(decoded),
                    start_line,
                    start_column,
                )

                return

            # ----------------------------------------------
            # Escape
            # ----------------------------------------------

            if ch == "\\":

                if self.peek() == "":
                    break

                escape_char = self.peek()

                if escape_char in ESCAPES:

                    # Consome \
                    self.advance()

                    # Consome caractere do escape
                    self.advance()

                    decoded.append(
                        ESCAPES[escape_char]
                    )

                    continue

                # Escape não reconhecido
                escape_line = self.line
                escape_column = self.column

                invalid_escape = self.source[
                    self.pos:self.pos + 2
                ]

                self.emit_error(
                    "INVALID_ESCAPE_SEQUENCE",
                    invalid_escape,
                    escape_line,
                    escape_column,
                )

                self.advance()

                if (
                    not self.at_end()
                    and self.current() != "\n"
                ):
                    decoded.append(
                        self.advance()
                    )

                continue

            # ----------------------------------------------
            # Caractere normal da string
            # ----------------------------------------------

            decoded.append(ch)
            self.advance()

        # --------------------------------------------------
        # STRING NÃO TERMINADA
        # --------------------------------------------------

        end_pos = self.pos

        lexeme = self.source[
            start_pos:end_pos
        ]

        self.emit_error(
            "UNTERMINATED_STRING_LITERAL",
            lexeme,
            start_line,
            start_column,
        )

        # Recuperação necessária para reproduzir:
        #
        # print("texto sem fim);
        #
        # O fixture oficial ainda espera:
        #
        # RPAREN
        # SEMICOLON

        self.recover_trailing_delimiters_from_string(
            start_pos + 1,
            end_pos,
            start_line,
            start_column + 1,
        )

    # ========================================================
    # RECUPERAÇÃO DE STRING INVÁLIDA
    # ========================================================

    def recover_trailing_delimiters_from_string(
        self,
        content_start,
        content_end,
        line,
        content_column,
    ):

        if content_start >= content_end:
            return

        index = content_end - 1

        # Ignora espaços finais
        while (
            index >= content_start
            and self.source[index] in " \t"
        ):
            index -= 1

        suffix_end = index + 1

        # Procura delimitadores no final
        while (
            index >= content_start
            and self.source[index]
            in "()[]{};,"
        ):
            index -= 1

        suffix_start = index + 1

        if suffix_start >= suffix_end:
            return

        # Emite os delimitadores preservando
        # suas posições originais.
        for absolute_index in range(
            suffix_start,
            suffix_end
        ):

            ch = self.source[absolute_index]

            token = ONE_CHAR_TOKENS.get(ch)

            if token is None:
                continue

            token_column = (
                content_column
                + absolute_index
                - content_start
            )

            self.emit_token(
                token,
                ch,
                None,
                line,
                token_column,
            )

    # ========================================================
    # CARACTERES
    # ========================================================

    def scan_char(self):

        start_pos = self.pos
        start_line = self.line
        start_column = self.column

        # Consome '
        self.advance()

        # --------------------------------------------------
        # EOF ou quebra imediatamente após '
        # --------------------------------------------------

        if (
            self.at_end()
            or self.current() == "\n"
        ):

            self.emit_error(
                "UNTERMINATED_CHAR_LITERAL",
                self.source[
                    start_pos:self.pos
                ],
                start_line,
                start_column,
            )

            return

        decoded = None

        # --------------------------------------------------
        # Escape
        # --------------------------------------------------

        if self.current() == "\\":

            if self.peek() in ESCAPES:

                # Consome \
                self.advance()

                escape_char = self.advance()

                decoded = ESCAPES[
                    escape_char
                ]

            else:

                # Escape inválido
                while (
                    not self.at_end()
                    and self.current()
                    not in ("'", "\n")
                ):
                    self.advance()

                if self.current() == "'":
                    self.advance()

                self.emit_error(
                    "INVALID_CHAR_LITERAL",
                    self.source[
                        start_pos:self.pos
                    ],
                    start_line,
                    start_column,
                )

                return

        else:

            # Um caractere simples
            decoded = self.advance()

        # --------------------------------------------------
        # Fechamento normal
        # --------------------------------------------------

        if self.current() == "'":

            self.advance()

            lexeme = self.source[
                start_pos:self.pos
            ]

            self.emit_token(
                "CHAR_LIT",
                lexeme,
                decoded,
                start_line,
                start_column,
            )

            return

        # --------------------------------------------------
        # CARACTERE NÃO TERMINADO
        #
        # Fixture:
        #
        # 'a;
        #
        # erro esperado:
        #
        # lexeme = "'a"
        #
        # sem incluir ;
        # --------------------------------------------------

        error_end = self.pos

        while (
            not self.at_end()
            and self.current() != "\n"
        ):

            if self.current() in ";,)}]":
                break

            self.advance()
            error_end = self.pos

        self.emit_error(
            "UNTERMINATED_CHAR_LITERAL",
            self.source[
                start_pos:error_end
            ],
            start_line,
            start_column,
        )

        # O fixture i03 não produz o ;
        # da linha defeituosa.
        #
        # Portanto descartamos o restante
        # dessa linha.
        while (
            not self.at_end()
            and self.current() != "\n"
        ):
            self.advance()

    # ========================================================
    # PROCESSAMENTO PRINCIPAL
    # ========================================================

    def scan(self):

        while not self.at_end():

            ch = self.current()

            # ------------------------------------------------
            # ESPAÇOS
            # ------------------------------------------------

            if ch in (" ", "\t", "\r"):
                self.advance()
                continue

            # ------------------------------------------------
            # QUEBRA DE LINHA
            # ------------------------------------------------

            if ch == "\n":
                self.advance()
                continue

            # ------------------------------------------------
            # COMENTÁRIOS
            #
            # Deve ser verificado antes de tratar /
            # como operador SLASH.
            # ------------------------------------------------

            if ch == "/":

                # //
                if self.peek() == "/":
                    self.skip_line_comment()
                    continue

                # /*
                if self.peek() == "*":
                    self.skip_block_comment()
                    continue

            # ------------------------------------------------
            # IDENTIFICADORES / RESERVADAS
            # ------------------------------------------------

            if is_identifier_start(ch):
                self.scan_identifier()
                continue

            # ------------------------------------------------
            # NÚMEROS
            # ------------------------------------------------

            if is_digit(ch):
                self.scan_number()
                continue

            # ------------------------------------------------
            # STRING
            # ------------------------------------------------

            if ch == '"':
                self.scan_string()
                continue

            # ------------------------------------------------
            # CHAR
            # ------------------------------------------------

            if ch == "'":
                self.scan_char()
                continue

            # ------------------------------------------------
            # OPERADORES COMPOSTOS
            #
            # Maximal munch
            # ------------------------------------------------

            pair = ch + self.peek()

            if pair in TWO_CHAR_TOKENS:

                start_line = self.line
                start_column = self.column

                self.advance()
                self.advance()

                self.emit_token(
                    TWO_CHAR_TOKENS[pair],
                    pair,
                    None,
                    start_line,
                    start_column,
                )

                continue

            # ------------------------------------------------
            # & ou | isolados
            # ------------------------------------------------

            if ch in ("&", "|"):

                start_line = self.line
                start_column = self.column

                self.advance()

                self.emit_error(
                    "INCOMPLETE_LOGICAL_OPERATOR",
                    ch,
                    start_line,
                    start_column,
                )

                continue

            # ------------------------------------------------
            # OPERADORES E DELIMITADORES SIMPLES
            # ------------------------------------------------

            token = ONE_CHAR_TOKENS.get(ch)

            if token is not None:

                start_line = self.line
                start_column = self.column

                self.advance()

                self.emit_token(
                    token,
                    ch,
                    None,
                    start_line,
                    start_column,
                )

                continue

            # ------------------------------------------------
            # SÍMBOLO DESCONHECIDO
            #
            # Exemplo:
            #
            # @
            # ------------------------------------------------

            start_line = self.line
            start_column = self.column

            lexeme = self.advance()

            self.emit_error(
                "UNKNOWN_SYMBOL",
                lexeme,
                start_line,
                start_column,
            )

        # ====================================================
        # EOF
        # ====================================================

        self.emit_token(
            "EOF",
            "",
            None,
            self.line,
            self.column,
        )

        return self.output


# ============================================================
# MAIN
# ============================================================

def main():

    # --------------------------------------------------------
    # O scanner recebe exatamente um arquivo.
    #
    # python scanner.py programa.c
    # python scanner.py programa.minic
    # --------------------------------------------------------

    if len(sys.argv) != 2:

        print(
            f"Uso: {Path(sys.argv[0]).name} "
            "<arquivo.c|arquivo.minic>",
            file=sys.stderr,
        )

        return 1

    file_path = Path(sys.argv[1])

    # --------------------------------------------------------
    # LEITURA DO ARQUIVO
    # --------------------------------------------------------

    try:

        source = file_path.read_text(
            encoding="utf-8"
        )

    except FileNotFoundError:

        print(
            f"ERRO: arquivo não encontrado: "
            f"{file_path}",
            file=sys.stderr,
        )

        return 1

    except OSError as exc:

        print(
            f"ERRO: não foi possível ler "
            f"{file_path}: {exc}",
            file=sys.stderr,
        )

        return 1

    # --------------------------------------------------------
    # EXECUÇÃO DO LEXER
    # --------------------------------------------------------

    scanner = Scanner(source)

    result = scanner.scan()

    # --------------------------------------------------------
    # JSONL
    #
    # Um objeto JSON por linha.
    # --------------------------------------------------------

    for item in result:

        print(
            json.dumps(
                item,
                ensure_ascii=False,
                separators=(",", ":"),
            )
        )

    # Erros léxicos pertencem ao programa analisado.
    # O scanner em si executou corretamente.
    #
    # Por isso retornamos 0 mesmo quando foram
    # encontrados erros léxicos.
    return 0


if __name__ == "__main__":
    raise SystemExit(main())