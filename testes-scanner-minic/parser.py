#!/usr/bin/env python3

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Optional
import sys

try:
    from scanner import Scanner
except ImportError:
    Scanner = None


# ============================================================
# TOKEN
# ============================================================

@dataclass
class Token:
    type: str
    lexeme: str
    attribute: Any
    line: int
    column: int


# ============================================================
# AST
# ============================================================

class ASTNode:
    pass


@dataclass
class Program(ASTNode):
    items: list[ASTNode]


@dataclass
class Parameter:
    type_name: str
    name: str
    is_array: bool = False
    line: int = 0
    column: int = 0


@dataclass
class Function(ASTNode):
    return_type: str
    name: str
    parameters: list[Parameter]
    body: ASTNode
    line: int = 0
    column: int = 0


@dataclass
class Block(ASTNode):
    items: list[ASTNode]


@dataclass
class VarDecl(ASTNode):
    type_name: str
    name: str
    size: Optional[ASTNode] = None
    initializer: Optional[ASTNode] = None
    line: int = 0
    column: int = 0


@dataclass
class If(ASTNode):
    condition: ASTNode
    then_branch: ASTNode
    else_branch: Optional[ASTNode]


@dataclass
class While(ASTNode):
    condition: ASTNode
    body: ASTNode


@dataclass
class Break(ASTNode):
    line: int = 0
    column: int = 0


@dataclass
class Return(ASTNode):
    value: Optional[ASTNode]


@dataclass
class ExprStmt(ASTNode):
    expression: ASTNode


@dataclass
class Assign(ASTNode):
    target: ASTNode
    value: ASTNode


@dataclass
class Binary(ASTNode):
    operator: str
    left: ASTNode
    right: ASTNode


@dataclass
class Unary(ASTNode):
    operator: str
    operand: ASTNode


@dataclass
class Call(ASTNode):
    callee: ASTNode
    arguments: list[ASTNode]


@dataclass
class Index(ASTNode):
    base: ASTNode
    index: ASTNode


@dataclass
class Id(ASTNode):
    name: str
    line: int = 0
    column: int = 0


@dataclass
class Lit(ASTNode):
    literal_type: str
    value: Any
    lexeme: str
    line: int = 0
    column: int = 0


# ============================================================
# ERROS
# ============================================================

class ParserError(Exception):

    def __init__(
        self,
        token: Token,
        message: str
    ):
        self.token = token
        self.message = message

        found = (
            "EOF"
            if token.type == "EOF"
            else token.lexeme
        )

        super().__init__(
            f"Erro sintático na linha "
            f"{token.line}, "
            f"coluna {token.column}: "
            f"{message}. "
            f"Encontrado '{found}'."
        )


class LexicalError(Exception):
    pass


# ============================================================
# TIPOS
# ============================================================

VARIABLE_TYPES = {
    "INT": "int",
    "FLOAT": "float",
    "BOOL": "bool",
    "CHAR": "char",
}

RETURN_TYPES = {
    **VARIABLE_TYPES,
    "VOID": "void",
}


# ============================================================
# PARSER
# ============================================================

class Parser:

    def __init__(
        self,
        tokens: list[Token],
        semantic_mode: bool = False
    ):
        self.tokens = tokens
        self.current_index = 0
        # Na Etapa 2, destinos inválidos de atribuição eram
        # rejeitados sintaticamente. A suíte semântica inclui
        # "3 = n;" para que essa regra seja validada em SEM013.
        # O modo semântico posterga apenas essa checagem.
        self.semantic_mode = semantic_mode


    # ========================================================
    # CONTROLE DOS TOKENS
    # ========================================================

    def current(self) -> Token:
        return self.tokens[
            self.current_index
        ]


    def previous(self) -> Token:
        return self.tokens[
            self.current_index - 1
        ]


    def at_end(self) -> bool:
        return (
            self.current().type
            == "EOF"
        )


    def check(
        self,
        *types: str
    ) -> bool:
        return (
            self.current().type
            in types
        )


    def advance(self) -> Token:

        token = self.current()

        if not self.at_end():
            self.current_index += 1

        return token


    def match(
        self,
        *types: str
    ) -> bool:

        if self.check(*types):
            self.advance()
            return True

        return False


    def consume(
        self,
        token_type: str,
        message: str
    ) -> Token:

        if self.check(token_type):
            return self.advance()

        raise ParserError(
            self.current(),
            message
        )


    # ========================================================
    # PROGRAMA
    # ========================================================

    def parse(self) -> Program:

        items = []

        while not self.at_end():

            items.append(
                self.parse_top_level()
            )

        return Program(items)


    def parse_top_level(
        self
    ) -> ASTNode:

        if self.check(
            *RETURN_TYPES.keys()
        ):

            return (
                self.parse_declaration_or_function()
            )

        return self.parse_statement()


    # ========================================================
    # TIPOS
    # ========================================================

    def parse_type(
        self,
        allow_void: bool = False
    ) -> str:

        mapping = (
            RETURN_TYPES
            if allow_void
            else VARIABLE_TYPES
        )

        token = self.current()

        if token.type in mapping:

            self.advance()

            return mapping[token.type]

        raise ParserError(
            token,
            "esperado tipo"
        )


    # ========================================================
    # DECLARAÇÃO OU FUNÇÃO
    # ========================================================

    def parse_declaration_or_function(
        self
    ) -> ASTNode:

        type_token = self.current()

        type_name = self.parse_type(
            allow_void=True
        )

        name = self.consume(
            "IDENT",
            "esperado identificador "
            "após o tipo"
        )

        # ----------------------------------------------
        # Função
        # ----------------------------------------------

        if self.match("LPAREN"):

            return self.finish_function(
                type_name,
                name,
                type_token
            )

        # ----------------------------------------------
        # void só pode ser retorno de função
        # ----------------------------------------------

        if type_name == "void":

            raise ParserError(
                name,
                "'void' só pode ser usado "
                "como tipo de retorno de função"
            )

        # ----------------------------------------------
        # Variável global
        # ----------------------------------------------

        return (
            self.finish_variable_declaration(
                type_name,
                name
            )
        )


    # ========================================================
    # FUNÇÕES
    # ========================================================

    def finish_function(
        self,
        return_type: str,
        name_token: Token,
        type_token: Optional[Token] = None
    ) -> Function:

        parameters = []

        # ----------------------------------------------
        # Lista de parâmetros
        # ----------------------------------------------

        if not self.check("RPAREN"):

            while True:

                if self.check(
                    "COMMA",
                    "RPAREN",
                    "LBRACE",
                    "SEMICOLON"
                ):

                    raise ParserError(
                        self.current(),
                        "esperado tipo de parâmetro"
                    )

                type_name = self.parse_type(
                    allow_void=False
                )

                param_name = self.consume(
                    "IDENT",
                    "esperado identificador "
                    "do parâmetro"
                )

                is_array = False

                # Parâmetro vetor MINIC: int dados[]
                if self.match("LBRACKET"):
                    self.consume(
                        "RBRACKET",
                        "esperado ']' após "
                        "parâmetro vetor"
                    )
                    is_array = True

                parameters.append(
                    Parameter(
                        type_name,
                        param_name.lexeme,
                        is_array,
                        param_name.line,
                        param_name.column
                    )
                )

                if not self.match("COMMA"):
                    break

                if self.check("RPAREN"):

                    raise ParserError(
                        self.current(),
                        "esperado tipo de parâmetro "
                        "após vírgula"
                    )

        self.consume(
            "RPAREN",
            "esperado ')' após "
            "os parâmetros"
        )

        # ----------------------------------------------
        # Corpo obrigatório
        # ----------------------------------------------

        if not self.check("LBRACE"):

            raise ParserError(
                self.current(),
                "esperado '{' para iniciar "
                "o corpo da função"
            )

        body = self.parse_block()

        start_token = (
            type_token
            if type_token is not None
            else name_token
        )

        return Function(
            return_type,
            name_token.lexeme,
            parameters,
            body,
            start_token.line,
            start_token.column
        )


    # ========================================================
    # VARIÁVEIS
    # ========================================================

    def finish_variable_declaration(
        self,
        type_name: str,
        name_token: Token
    ) -> VarDecl:

        size = None
        initializer = None

        # ----------------------------------------------
        # Vetor
        #
        # int dados[10];
        # ----------------------------------------------

        if self.match("LBRACKET"):

            if self.check("RBRACKET"):

                raise ParserError(
                    self.current(),
                    "esperado expressão para "
                    "o tamanho do vetor"
                )

            size = self.parse_expression()

            self.consume(
                "RBRACKET",
                "esperado ']' após "
                "o tamanho do vetor"
            )

        # ----------------------------------------------
        # Inicializador
        #
        # int x = 10;
        # ----------------------------------------------

        if self.match("ASSIGN"):

            if self.check("SEMICOLON"):

                raise ParserError(
                    self.current(),
                    "esperado expressão após ="
                )

            initializer = (
                self.parse_expression()
            )

        self.consume(
            "SEMICOLON",
            "esperado ';' após "
            "a declaração"
        )

        return VarDecl(
            type_name,
            name_token.lexeme,
            size,
            initializer,
            name_token.line,
            name_token.column
        )


    # ========================================================
    # DECLARAÇÃO LOCAL
    # ========================================================

    def parse_local_declaration(
        self
    ) -> VarDecl:

        type_name = self.parse_type(
            allow_void=False
        )

        name = self.consume(
            "IDENT",
            "esperado identificador "
            "após o tipo"
        )

        # Não permitimos declaração de função
        # dentro de bloco.

        if self.check("LPAREN"):

            raise ParserError(
                self.current(),
                "declaração de função "
                "não permitida dentro de bloco"
            )

        return (
            self.finish_variable_declaration(
                type_name,
                name
            )
        )


    # ========================================================
    # BLOCO
    # ========================================================

    def parse_block(self) -> Block:

        self.consume(
            "LBRACE",
            "esperado '{' para iniciar bloco"
        )

        items = []

        while (
            not self.check("RBRACE")
            and not self.at_end()
        ):

            # Declaração local

            if self.check(
                *VARIABLE_TYPES.keys()
            ):

                items.append(
                    self.parse_local_declaration()
                )

            # Statement

            else:

                items.append(
                    self.parse_statement()
                )

        self.consume(
            "RBRACE",
            "esperado '}' para fechar bloco"
        )

        return Block(items)


    # ========================================================
    # STATEMENTS
    # ========================================================

    def parse_statement(
        self
    ) -> ASTNode:

        # ----------------------------------------------
        # else isolado
        # ----------------------------------------------

        if self.check("ELSE"):

            raise ParserError(
                self.current(),
                "token 'else' inesperado"
            )

        # ----------------------------------------------
        # } onde deveria existir statement
        # ----------------------------------------------

        if self.check("RBRACE"):

            raise ParserError(
                self.current(),
                "esperado início de statement"
            )

        # ----------------------------------------------
        # Bloco
        # ----------------------------------------------

        if self.check("LBRACE"):

            return self.parse_block()

        # ----------------------------------------------
        # if
        # ----------------------------------------------

        if self.match("IF"):

            return (
                self.parse_if_statement()
            )

        # ----------------------------------------------
        # while
        # ----------------------------------------------

        if self.match("WHILE"):

            return (
                self.parse_while_statement()
            )

        # for/ break reconhecidos apenas no pipeline integrado;
        # o parser isolado da Etapa 2 mantém seu contrato original.
        if self.semantic_mode and self.match("FOR"):
            return self.parse_for_statement()

        if self.semantic_mode and self.match("BREAK"):
            token = self.previous()
            self.consume("SEMICOLON", "esperado ';' após break")
            return Break(token.line, token.column)

        # ----------------------------------------------
        # return
        # ----------------------------------------------

        if self.match("RETURN"):

            return (
                self.parse_return_statement()
            )

        # ----------------------------------------------
        # expressão ;
        # ----------------------------------------------

        return (
            self.parse_expression_statement()
        )


    # ========================================================
    # IF
    # ========================================================

    def parse_if_statement(
        self
    ) -> If:

        self.consume(
            "LPAREN",
            "esperado '(' após if"
        )

        if self.check("RPAREN"):

            raise ParserError(
                self.current(),
                "esperado expressão "
                "na condição do if"
            )

        condition = (
            self.parse_expression()
        )

        self.consume(
            "RPAREN",
            "esperado ')' após "
            "a condição do if"
        )

        then_branch = (
            self.parse_statement()
        )

        else_branch = None

        if self.match("ELSE"):

            else_branch = (
                self.parse_statement()
            )

        return If(
            condition,
            then_branch,
            else_branch
        )


    # ========================================================
    # WHILE
    # ========================================================

    def parse_while_statement(
        self
    ) -> While:

        self.consume(
            "LPAREN",
            "esperado '(' após while"
        )

        if self.check("RPAREN"):

            raise ParserError(
                self.current(),
                "esperado expressão "
                "na condição do while"
            )

        condition = (
            self.parse_expression()
        )

        self.consume(
            "RPAREN",
            "esperado ')' após "
            "a condição do while"
        )

        if self.check(
            "RBRACE",
            "EOF"
        ):

            raise ParserError(
                self.current(),
                "esperado início de statement "
                "como corpo do while"
            )

        body = self.parse_statement()

        return While(
            condition,
            body
        )


    # ========================================================
    # FOR: dessugar em bloco + while para compartilhar AST/SDD
    # ========================================================
    def parse_for_statement(self) -> Block:
        self.consume("LPAREN", "esperado '(' após for")
        init = None
        if not self.check("SEMICOLON"):
            init = ExprStmt(self.parse_expression())
        self.consume("SEMICOLON", "esperado ';' após inicializador do for")
        condition = Lit("bool", True, "true") if self.check("SEMICOLON") else self.parse_expression()
        self.consume("SEMICOLON", "esperado ';' após condição do for")
        step = None if self.check("RPAREN") else ExprStmt(self.parse_expression())
        self.consume("RPAREN", "esperado ')' após for")
        body = self.parse_statement()
        repeated = Block([body, step] if step is not None else [body])
        result = While(condition, repeated)
        return Block(([init] if init is not None else []) + [result])

    # ========================================================
    # RETURN
    # ========================================================

    def parse_return_statement(
        self
    ) -> Return:

        # return;

        if self.match("SEMICOLON"):

            return Return(None)

        # return expressão;

        value = (
            self.parse_expression()
        )

        self.consume(
            "SEMICOLON",
            "esperado ';' após return"
        )

        return Return(value)


    # ========================================================
    # EXPRESSION STATEMENT
    # ========================================================

    def parse_expression_statement(
        self
    ) -> ExprStmt:

        expression = (
            self.parse_expression()
        )

        self.consume(
            "SEMICOLON",
            "esperado ';' após "
            "a expressão"
        )

        return ExprStmt(
            expression
        )


    # ========================================================
    # EXPRESSÕES
    #
    # Precedência:
    #
    # =
    # ||
    # &&
    # == !=
    # < > <= >=
    # + -
    # * / %
    # ! - +
    # call / index
    # primary
    # ========================================================

    def parse_expression(
        self
    ) -> ASTNode:

        return self.parse_assignment()


    # ========================================================
    # ATRIBUIÇÃO
    #
    # Associativa à direita:
    #
    # a = b = 3
    #
    # a = (b = 3)
    # ========================================================

    def parse_assignment(
        self
    ) -> ASTNode:

        left = (
            self.parse_logical_or()
        )

        if self.match("ASSIGN"):

            if (
                not self.semantic_mode
                and not isinstance(
                    left,
                    (Id, Index)
                )
            ):

                raise ParserError(
                    self.previous(),
                    "destino inválido "
                    "para atribuição"
                )

            value = (
                self.parse_assignment()
            )

            return Assign(
                left,
                value
            )

        return left


    # ========================================================
    # ||
    # ========================================================

    def parse_logical_or(
        self
    ) -> ASTNode:

        expression = (
            self.parse_logical_and()
        )

        while self.match("OR"):

            operator = (
                self.previous().lexeme
            )

            right = (
                self.parse_logical_and()
            )

            expression = Binary(
                operator,
                expression,
                right
            )

        return expression


    # ========================================================
    # &&
    # ========================================================

    def parse_logical_and(
        self
    ) -> ASTNode:

        expression = (
            self.parse_equality()
        )

        while self.match("AND"):

            operator = (
                self.previous().lexeme
            )

            right = (
                self.parse_equality()
            )

            expression = Binary(
                operator,
                expression,
                right
            )

        return expression


    # ========================================================
    # == !=
    # ========================================================

    def parse_equality(
        self
    ) -> ASTNode:

        expression = (
            self.parse_relational()
        )

        while self.match(
            "EQ",
            "NE"
        ):

            operator = (
                self.previous().lexeme
            )

            right = (
                self.parse_relational()
            )

            expression = Binary(
                operator,
                expression,
                right
            )

        return expression


    # ========================================================
    # < > <= >=
    # ========================================================

    def parse_relational(
        self
    ) -> ASTNode:

        expression = (
            self.parse_additive()
        )

        while self.match(
            "LT",
            "GT",
            "LE",
            "GE"
        ):

            operator = (
                self.previous().lexeme
            )

            right = (
                self.parse_additive()
            )

            expression = Binary(
                operator,
                expression,
                right
            )

        return expression


    # ========================================================
    # + -
    # ========================================================

    def parse_additive(
        self
    ) -> ASTNode:

        expression = (
            self.parse_multiplicative()
        )

        while self.match(
            "PLUS",
            "MINUS"
        ):

            operator = (
                self.previous().lexeme
            )

            right = (
                self.parse_multiplicative()
            )

            expression = Binary(
                operator,
                expression,
                right
            )

        return expression


    # ========================================================
    # * / %
    # ========================================================

    def parse_multiplicative(
        self
    ) -> ASTNode:

        expression = (
            self.parse_unary()
        )

        while self.match(
            "STAR",
            "SLASH",
            "PERCENT"
        ):

            operator = (
                self.previous().lexeme
            )

            right = (
                self.parse_unary()
            )

            expression = Binary(
                operator,
                expression,
                right
            )

        return expression


    # ========================================================
    # UNÁRIO
    #
    # !x
    # -x
    # +x
    # ========================================================

    def parse_unary(
        self
    ) -> ASTNode:

        if self.match(
            "NOT",
            "MINUS",
            "PLUS"
        ):

            operator = (
                self.previous().lexeme
            )

            operand = (
                self.parse_unary()
            )

            return Unary(
                operator,
                operand
            )

        return self.parse_postfix()


    # ========================================================
    # POSTFIX
    #
    # chamada:
    # foo(...)
    #
    # índice:
    # vetor[...]
    # ========================================================

    def parse_postfix(
        self
    ) -> ASTNode:

        expression = (
            self.parse_primary()
        )

        while True:

            # ------------------------------------------
            # CHAMADA
            # ------------------------------------------

            if self.match("LPAREN"):

                arguments = []

                if not self.check(
                    "RPAREN"
                ):

                    arguments.append(
                        self.parse_expression()
                    )

                    while self.match(
                        "COMMA"
                    ):

                        if self.check(
                            "RPAREN"
                        ):

                            raise ParserError(
                                self.current(),
                                "esperado argumento "
                                "após vírgula"
                            )

                        arguments.append(
                            self.parse_expression()
                        )

                self.consume(
                    "RPAREN",
                    "esperado ')' após "
                    "os argumentos"
                )

                expression = Call(
                    expression,
                    arguments
                )

            # ------------------------------------------
            # ÍNDICE
            # ------------------------------------------

            elif self.match(
                "LBRACKET"
            ):

                if self.check(
                    "RBRACKET"
                ):

                    raise ParserError(
                        self.current(),
                        "esperado expressão "
                        "no índice"
                    )

                index = (
                    self.parse_expression()
                )

                self.consume(
                    "RBRACKET",
                    "esperado ']' após "
                    "o índice"
                )

                expression = Index(
                    expression,
                    index
                )

            else:
                break

        return expression


    # ========================================================
    # PRIMARY
    # ========================================================

    def parse_primary(
        self
    ) -> ASTNode:

        # ----------------------------------------------
        # Inteiro
        # ----------------------------------------------

        if self.match("INT_LIT"):

            token = self.previous()

            return Lit(
                "int",
                token.attribute,
                token.lexeme,
                token.line,
                token.column
            )

        # ----------------------------------------------
        # Real
        # ----------------------------------------------

        if self.match("FLOAT_LIT"):

            token = self.previous()

            return Lit(
                "real",
                token.attribute,
                token.lexeme,
                token.line,
                token.column
            )

        # ----------------------------------------------
        # Booleano
        # ----------------------------------------------

        if self.match(
            "TRUE",
            "FALSE"
        ):

            token = self.previous()

            return Lit(
                "bool",
                token.lexeme == "true",
                token.lexeme,
                token.line,
                token.column
            )

        # ----------------------------------------------
        # Caractere
        # ----------------------------------------------

        if self.match("CHAR_LIT"):

            token = self.previous()

            return Lit(
                "char",
                token.attribute,
                token.lexeme,
                token.line,
                token.column
            )

        # ----------------------------------------------
        # String
        # ----------------------------------------------

        if self.match("STRING_LIT"):

            token = self.previous()

            return Lit(
                "string",
                token.attribute,
                token.lexeme,
                token.line,
                token.column
            )

        # ----------------------------------------------
        # Identificador
        #
        # print/read também podem funcionar
        # como chamadas.
        # ----------------------------------------------

        if self.match(
            "IDENT",
            "PRINT",
            "READ"
        ):

            token = self.previous()

            return Id(
                token.lexeme,
                token.line,
                token.column
            )

        # ----------------------------------------------
        # ( expressão )
        # ----------------------------------------------

        if self.match("LPAREN"):

            if self.check("RPAREN"):

                raise ParserError(
                    self.current(),
                    "esperado expressão "
                    "entre parênteses"
                )

            expression = (
                self.parse_expression()
            )

            self.consume(
                "RPAREN",
                "esperado ')' após "
                "a expressão"
            )

            return expression

        raise ParserError(
            self.current(),
            "esperado identificador, "
            "literal ou expressão "
            "entre parênteses"
        )


# ============================================================
# SERIALIZAÇÃO DA AST
# ============================================================

def serialize_ast(
    node: Optional[ASTNode]
) -> str:

    # --------------------------------------------------------
    # NULL
    # --------------------------------------------------------

    if node is None:
        return "NULL"

    # --------------------------------------------------------
    # PROGRAM
    # --------------------------------------------------------

    if isinstance(
        node,
        Program
    ):

        return (
            "Program("
            + ", ".join(
                serialize_ast(item)
                for item in node.items
            )
            + ")"
        )

    # --------------------------------------------------------
    # FUNCTION
    # --------------------------------------------------------

    if isinstance(
        node,
        Function
    ):

        parameters = ",".join(
            f"{parameter.type_name} "
            f"{parameter.name}"
            f"{'[]' if parameter.is_array else ''}"
            for parameter
            in node.parameters
        )

        return (
            f"Function("
            f"{node.return_type} "
            f"{node.name}"
            f"({parameters}) "
            f"{serialize_ast(node.body)}"
            f")"
        )

    # --------------------------------------------------------
    # BLOCK
    # --------------------------------------------------------

    if isinstance(
        node,
        Block
    ):

        return (
            "Block("
            + ", ".join(
                serialize_ast(item)
                for item in node.items
            )
            + ")"
        )

    # --------------------------------------------------------
    # VAR DECL
    # --------------------------------------------------------

    if isinstance(
        node,
        VarDecl
    ):

        text = (
            f"{node.type_name} "
            f"{node.name}"
        )

        if node.size is not None:

            text += (
                f" size="
                f"{serialize_ast(node.size)}"
            )

        if (
            node.initializer
            is not None
        ):

            text += (
                f"="
                f"{serialize_ast(node.initializer)}"
            )

        return (
            f"VarDecl({text})"
        )

    # --------------------------------------------------------
    # IF
    # --------------------------------------------------------

    if isinstance(
        node,
        If
    ):

        return (
            "If("
            f"{serialize_ast(node.condition)},"
            f"{serialize_ast(node.then_branch)},"
            f"{serialize_ast(node.else_branch)}"
            ")"
        )

    # --------------------------------------------------------
    # WHILE
    # --------------------------------------------------------

    if isinstance(
        node,
        While
    ):

        return (
            "While("
            f"{serialize_ast(node.condition)},"
            f"{serialize_ast(node.body)}"
            ")"
        )

    # --------------------------------------------------------
    # RETURN
    # --------------------------------------------------------

    if isinstance(
        node,
        Return
    ):

        return (
            "Return("
            f"{serialize_ast(node.value)}"
            ")"
        )

    # --------------------------------------------------------
    # EXPRESSION STATEMENT
    # --------------------------------------------------------

    if isinstance(
        node,
        ExprStmt
    ):

        return (
            "ExprStmt("
            f"{serialize_ast(node.expression)}"
            ")"
        )

    # --------------------------------------------------------
    # ASSIGN
    # --------------------------------------------------------

    if isinstance(
        node,
        Assign
    ):

        return (
            "Assign("
            f"{serialize_ast(node.target)},"
            f"{serialize_ast(node.value)}"
            ")"
        )

    # --------------------------------------------------------
    # BINARY
    # --------------------------------------------------------

    if isinstance(
        node,
        Binary
    ):

        return (
            "Binary("
            f"{node.operator},"
            f"{serialize_ast(node.left)},"
            f"{serialize_ast(node.right)}"
            ")"
        )

    # --------------------------------------------------------
    # UNARY
    # --------------------------------------------------------

    if isinstance(
        node,
        Unary
    ):

        return (
            "Unary("
            f"{node.operator},"
            f"{serialize_ast(node.operand)}"
            ")"
        )

    # --------------------------------------------------------
    # CALL
    # --------------------------------------------------------

    if isinstance(
        node,
        Call
    ):

        arguments = "".join(
            ","
            + serialize_ast(argument)
            for argument
            in node.arguments
        )

        return (
            "Call("
            f"{serialize_ast(node.callee)}"
            f"{arguments}"
            ")"
        )

    # --------------------------------------------------------
    # INDEX
    # --------------------------------------------------------

    if isinstance(
        node,
        Index
    ):

        return (
            "Index("
            f"{serialize_ast(node.base)},"
            f"{serialize_ast(node.index)}"
            ")"
        )

    # --------------------------------------------------------
    # ID
    # --------------------------------------------------------

    if isinstance(
        node,
        Id
    ):

        return (
            f"Id({node.name})"
        )

    # --------------------------------------------------------
    # LITERAL
    # --------------------------------------------------------

    if isinstance(
        node,
        Lit
    ):

        return (
            "Lit("
            f"{node.literal_type},"
            f"{node.lexeme}"
            ")"
        )

    raise TypeError(
        f"Nó AST desconhecido: "
        f"{type(node).__name__}"
    )


# ============================================================
# INTEGRAÇÃO COM O SCANNER
# ============================================================

def tokenize_source(
    source: str
) -> list[Token]:

    if Scanner is None:

        raise RuntimeError(
            "scanner.py não foi encontrado. "
            "O parser.py deve estar no mesmo "
            "diretório do scanner.py."
        )

    scanner = Scanner(source)

    result = scanner.scan()

    tokens = []

    for item in result:

        # ----------------------------------------------------
        # Erro léxico
        # ----------------------------------------------------

        if "error" in item:

            raise LexicalError(
                f"Erro léxico na linha "
                f"{item['line']}, "
                f"coluna "
                f"{item['column']}: "
                f"{item['error']} "
                f"({item['lexeme']!r})"
            )

        # ----------------------------------------------------
        # Token
        # ----------------------------------------------------

        tokens.append(
            Token(
                type=item["token"],
                lexeme=item["lexeme"],
                attribute=item["attribute"],
                line=item["line"],
                column=item["column"],
            )
        )

    return tokens


def print_syntax_rejection(error: ParserError) -> None:

    print(
        "NÃO HÁ AST: o parser deve rejeitar a entrada.",
        file=sys.stderr
    )

    print(
        str(error),
        file=sys.stderr
    )


# ============================================================
# MAIN
# ============================================================

def main():
    sys.stderr.reconfigure(encoding="utf-8")

    if len(sys.argv) != 2:

        print(
            "Uso: "
            "python parser.py codigo.c",
            file=sys.stderr
        )

        return 2

    path = Path(
        sys.argv[1]
    )

    # --------------------------------------------------------
    # LEITURA DO FONTE
    # --------------------------------------------------------

    try:

        source = path.read_text(
            encoding="utf-8"
        )

    except OSError as exc:

        print(
            f"ERRO: não foi possível "
            f"ler o arquivo: {exc}",
            file=sys.stderr
        )

        return 2

    # --------------------------------------------------------
    # SCANNER + PARSER
    # --------------------------------------------------------

    try:

        tokens = tokenize_source(
            source
        )

        parser = Parser(
            tokens
        )

        ast = parser.parse()

        print(
            serialize_ast(ast)
        )

        return 0

    # --------------------------------------------------------
    # ERRO SINTÁTICO
    # --------------------------------------------------------

    except ParserError as exc:

        print_syntax_rejection(exc)

        return 1

    # --------------------------------------------------------
    # ERRO LÉXICO
    # --------------------------------------------------------

    except LexicalError as exc:

        print(
            str(exc),
            file=sys.stderr
        )

        return 1


if __name__ == "__main__":
    raise SystemExit(main())