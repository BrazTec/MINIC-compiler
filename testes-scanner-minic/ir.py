#!/usr/bin/env python3
"""MINIC Etapa 4: geração real de TAC a partir da AST validada.

A saída usa IR de três endereços e não depende dos gabaritos de referência.
O modo -O1 faz otimizações conservadoras, preservando os efeitos colaterais.
"""
from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass
import re
from typing import Optional

from parser import (Program, Function, Block, VarDecl, If, While, Return,
                    ExprStmt, Assign, Binary, Unary, Call, Index, Id, Lit, Break)


@dataclass(frozen=True)
class Instruction:
    operation: str
    args: tuple[str, ...] = ()

    def render(self) -> str:
        a = self.args
        op = self.operation
        if op == "func": return f"func {a[0]}" + (f"({', '.join(a[1:])})" if len(a) > 1 else "")
        if op == "endfunc": return "endfunc"
        if op == "label": return f"{a[0]}:"
        if op == "goto": return f"goto {a[0]}"
        if op == "iffalse": return f"ifFalse {a[0]} goto {a[1]}"
        if op == "iftrue": return f"ifTrue {a[0]} goto {a[1]}"
        if op == "return": return f"return {a[0]}" if a else "return"
        if op == "print": return f"print {a[0]}"
        if op == "read": return f"read {a[0]}" if a else "read"
        if op == "param": return f"param {a[0]}"
        if op == "call": return f"{a[0]} = call {a[1]}, {a[2]}"
        if op == "call_void": return f"call {a[0]}, {a[1]}"
        if op == "move": return f"{a[0]} = {a[1]}"
        if op == "bin": return f"{a[0]} = {a[1]} {a[2]} {a[3]}"
        if op == "unary": return f"{a[0]} = {a[1]} {a[2]}"
        if op == "load": return f"{a[0]} = load {a[1]}[{a[2]}]"
        if op == "store": return f"store {a[0]}[{a[1]}], {a[2]}"
        if op == "alloc": return f"alloc {a[0]}, {a[1]}"
        raise ValueError(f"instrução desconhecida: {op}")


class IRGenerator:
    def __init__(self):
        self.code: list[Instruction] = []
        self.temps = 0
        self.labels = 0
        self.counts: dict[str, int] = defaultdict(int)
        self.scopes: list[dict[str, str]] = []
        self.break_targets: list[str] = []

    def emit(self, operation: str, *args: str) -> None:
        self.code.append(Instruction(operation, tuple(str(a) for a in args)))

    def temporary(self) -> str:
        self.temps += 1
        return f"t{self.temps}"

    def label(self, hint: str) -> str:
        self.labels += 1
        return f"L{self.labels}_{hint}"

    def push_scope(self) -> None:
        self.scopes.append({})

    def pop_scope(self) -> None:
        self.scopes.pop()

    def declare(self, name: str) -> str:
        self.counts[name] += 1
        new_name = name if self.counts[name] == 1 else f"{name}#{self.counts[name]}"
        self.scopes[-1][name] = new_name
        return new_name

    def resolve(self, name: str) -> str:
        for scope in reversed(self.scopes):
            if name in scope:
                return scope[name]
        return name

    def generate(self, program: Program, optimization: int = 0) -> list[Instruction]:
        self.push_scope()
        # Registra globais antes das funções para permitir referências futuras.
        for item in program.items:
            if isinstance(item, VarDecl):
                self.declare(item.name)
        for item in program.items:
            if isinstance(item, Function):
                self.function(item)
        self.pop_scope()
        return optimize(self.code) if optimization == 1 else list(self.code)

    def function(self, node: Function) -> None:
        self.temps = 0
        self.labels = 0
        self.push_scope()
        params = [self.declare(p.name) for p in node.parameters]
        self.emit("func", node.name, *params)
        self.statement(node.body, function_body=True)
        self.emit("endfunc")
        self.pop_scope()

    def statement(self, node, *, function_body=False) -> None:
        if node is None:
            return
        if isinstance(node, Block):
            if not function_body:
                self.push_scope()
            for item in node.items:
                self.statement(item)
            if not function_body:
                self.pop_scope()
        elif isinstance(node, VarDecl):
            name = self.declare(node.name)
            if node.size is not None:
                self.emit("alloc", name, self.expression(node.size))
            if node.initializer is not None:
                self.emit("move", name, self.expression(node.initializer))
        elif isinstance(node, ExprStmt):
            self.expression(node.expression, value_needed=False)
        elif isinstance(node, Return):
            self.emit("return", self.expression(node.value)) if node.value is not None else self.emit("return")
        elif isinstance(node, If):
            alternative = self.label("else" if node.else_branch else "endif")
            ending = self.label("endif") if node.else_branch else alternative
            self.branch_false(node.condition, alternative)
            self.statement(node.then_branch)
            if node.else_branch is not None:
                self.emit("goto", ending)
                self.emit("label", alternative)
                self.statement(node.else_branch)
            self.emit("label", ending)
        elif isinstance(node, While):
            testing = self.label("while")
            ending = self.label("endwhile")
            self.emit("label", testing)
            self.branch_false(node.condition, ending)
            self.break_targets.append(ending)
            try:
                self.statement(node.body)
            finally:
                self.break_targets.pop()
            self.emit("goto", testing)
            self.emit("label", ending)
        elif isinstance(node, Break):
            if not self.break_targets:
                raise ValueError("break fora de um laço foi aceito indevidamente")
            self.emit("goto", self.break_targets[-1])
        else:
            raise ValueError(f"nó de comando não tratado na IR: {type(node).__name__}")

    def branch_false(self, condition, target: str) -> None:
        # Curto-circuito em condições, preservando execução condicional de chamadas.
        if isinstance(condition, Unary) and condition.operator == "!":
            self.branch_true(condition.operand, target)
        elif isinstance(condition, Binary) and condition.operator == "&&":
            self.branch_false(condition.left, target)
            self.branch_false(condition.right, target)
        elif isinstance(condition, Binary) and condition.operator == "||":
            skip = self.label("or_true")
            self.branch_true(condition.left, skip)
            self.branch_false(condition.right, target)
            self.emit("label", skip)
        else:
            self.emit("iffalse", self.expression(condition), target)

    def branch_true(self, condition, target: str) -> None:
        if isinstance(condition, Unary) and condition.operator == "!":
            self.branch_false(condition.operand, target)
        elif isinstance(condition, Binary) and condition.operator == "||":
            self.branch_true(condition.left, target)
            self.branch_true(condition.right, target)
        elif isinstance(condition, Binary) and condition.operator == "&&":
            skip = self.label("and_false")
            self.branch_false(condition.left, skip)
            self.branch_true(condition.right, target)
            self.emit("label", skip)
        else:
            self.emit("iftrue", self.expression(condition), target)

    def expression(self, node, *, value_needed=True) -> str:
        if isinstance(node, Lit):
            return node.lexeme
        if isinstance(node, Id):
            return self.resolve(node.name)
        if isinstance(node, Assign):
            rhs = self.expression(node.value)
            if isinstance(node.target, Id):
                dest = self.resolve(node.target.name)
                self.emit("move", dest, rhs)
                return dest
            if isinstance(node.target, Index):
                base, index = self.array_location(node.target)
                self.emit("store", base, index, rhs)
                return rhs
            raise ValueError("destino de atribuição semântico inválido")
        if isinstance(node, Binary):
            if node.operator in {"&&", "||"}:
                result = self.temporary()
                when_false = self.label("bool_false")
                after = self.label("bool_end")
                self.branch_false(node, when_false)
                self.emit("move", result, "true")
                self.emit("goto", after)
                self.emit("label", when_false)
                self.emit("move", result, "false")
                self.emit("label", after)
                return result
            left = self.expression(node.left)
            right = self.expression(node.right)
            dest = self.temporary()
            self.emit("bin", dest, left, node.operator, right)
            return dest
        if isinstance(node, Unary):
            value = self.expression(node.operand)
            dest = self.temporary()
            self.emit("unary", dest, "not" if node.operator == "!" else node.operator, value)
            return dest
        if isinstance(node, Call):
            if not isinstance(node.callee, Id):
                raise ValueError("chamada dinâmica fora do MINIC")
            name = node.callee.name
            if name == "print":
                for argument in node.arguments:
                    self.emit("print", self.expression(argument))
                return "void"
            if name == "read":
                values = [self.expression(argument) for argument in node.arguments]
                for value in values:
                    self.emit("read", value)
                return "void"
            # Avalia parâmetros da esquerda para a direita.
            args = [self.expression(a) for a in node.arguments]
            for arg in args:
                self.emit("param", arg)
            if value_needed:
                dest = self.temporary()
                self.emit("call", dest, name, str(len(args)))
                return dest
            self.emit("call_void", name, str(len(args)))
            return "void"
        if isinstance(node, Index):
            base, index = self.array_location(node)
            dest = self.temporary()
            self.emit("load", dest, base, index)
            return dest
        raise ValueError(f"nó de expressão não tratado na IR: {type(node).__name__}")

    def array_location(self, node: Index) -> tuple[str, str]:
        base = self.expression(node.base)
        index = self.expression(node.index)
        address = self.temporary()
        self.emit("bin", address, index, "*", "1")
        return base, address


def number(value: str):
    if value in {"true", "false"}: return value == "true"
    if re.fullmatch(r"-?\d+", value): return int(value)
    if re.fullmatch(r"-?\d+\.\d+", value): return float(value)
    return None


def literal(value) -> str:
    if isinstance(value, bool): return "true" if value else "false"
    return str(value)


def fold(left: str, op: str, right: str):
    l, r = number(left), number(right)
    if l is None or r is None:
        return None
    try:
        if op == "+": result = l + r
        elif op == "-": result = l - r
        elif op == "*": result = l * r
        elif op == "/":
            if r == 0: return None
            result = int(l / r) if isinstance(l, int) and isinstance(r, int) else l / r
        elif op == "%":
            if r == 0: return None
            result = l % r
        elif op == "<": result = l < r
        elif op == "<=": result = l <= r
        elif op == ">": result = l > r
        elif op == ">=": result = l >= r
        elif op == "==": result = l == r
        elif op == "!=": result = l != r
        elif op == "&&": result = bool(l) and bool(r)
        elif op == "||": result = bool(l) or bool(r)
        else: return None
        return literal(result)
    except (ArithmeticError, ValueError, OverflowError):
        return None



# Otimizações -O1 em passes distintos, sobre TAC estruturada, sem alterar -O0.
# As transformações com risco de efeitos colaterais não são especuladas.
_PURE = {"move", "bin", "unary"}
_TERMINATORS = {"goto", "return"}
_CONDITIONAL = {"iffalse", "iftrue"}
_COMMUTATIVE = {"+", "*", "==", "!="}


def _defs(ins: Instruction) -> set[str]:
    if ins.operation in {"move", "bin", "unary", "load", "call", "alloc", "read"} and ins.args:
        return {ins.args[0]}
    return set()


def _uses(ins: Instruction) -> set[str]:
    op, a = ins.operation, ins.args
    if op == "move": return {a[1]}
    if op == "bin": return {a[1], a[3]}
    if op == "unary": return {a[2]}
    if op == "load": return {a[1], a[2]}
    if op == "store": return set(a)
    if op in {"print", "param", "return", "iffalse", "iftrue"} and a: return {a[0]}
    return set()


def _successors(code: list[Instruction]) -> list[set[int]]:
    labels = {ins.args[0]: i for i, ins in enumerate(code) if ins.operation == "label"}
    successors = []
    for i, ins in enumerate(code):
        op = ins.operation
        next_i = i + 1 if i + 1 < len(code) else None
        if op in {"return", "endfunc"}:
            edges = set()
        elif op == "goto":
            edges = {labels[ins.args[0]]} if ins.args[0] in labels else set()
        elif op in _CONDITIONAL:
            edges = ({next_i} if next_i is not None else set())
            if ins.args[1] in labels: edges.add(labels[ins.args[1]])
        else:
            edges = {next_i} if next_i is not None else set()
        successors.append(edges)
    return successors


def _per_function(code: list[Instruction], fn):
    result = []
    i = 0
    while i < len(code):
        if code[i].operation != "func":
            result.append(code[i]); i += 1; continue
        j = i + 1
        while j < len(code) and code[j].operation != "endfunc": j += 1
        result.extend(fn(code[i:j + 1])); i = j + 1
    return result


def _hoist_loop_invariants(code: list[Instruction]) -> list[Instruction]:
    """LICM restrita a loops naturais estruturados, operações sem exceções e
    operandos definidos exclusivamente fora do laço. Não move acessos à memória.
    """
    code = code[:]
    i = 0
    while i < len(code):
        head = code[i]
        if head.operation != "label" or not head.args[0].endswith("_while"):
            i += 1; continue
        tail = next((j for j in range(i+1, len(code)) if code[j] == Instruction("goto", (head.args[0],))), None)
        if tail is None or not any(ins.operation == "iffalse" for ins in code[i:tail]):
            i += 1; continue
        loop = code[i:tail+1]
        # Sem saltos extraordinários, chamadas ou alterações observáveis na
        # expressão hoisted. Não atravessa loops aninhados.
        if any(ins.operation in {"call", "call_void", "read", "store"} for ins in loop):
            i += 1; continue
        if sum(ins.operation == "label" and ins.args[0].endswith("_while") for ins in loop) != 1:
            i += 1; continue
        written = set().union(*(_defs(ins) for ins in loop))
        candidate = None
        for j in range(i+1, tail):
            ins = code[j]
            if ins.operation != "bin" or ins.args[2] not in {"+", "-", "*", "<", "<=", ">", ">=", "==", "!="}:
                continue
            dest, left, _, right = ins.args
            if (dest not in _uses(ins) and left not in written and right not in written
                and re.fullmatch(r"t\d+", dest)
                and all(dest not in _uses(other) for other in code[i:j])):
                candidate = j; break
        if candidate is None:
            i += 1; continue
        instr = code.pop(candidate)
        code.insert(i, instr)
        i += 2
    return code


def _cse_local(code: list[Instruction]) -> list[Instruction]:
    """CSE local em blocos básicos com invalidação de dependências."""
    expressions: dict[tuple[str, str, str], str] = {}
    result = []
    for ins in code:
        op, a = ins.operation, ins.args
        if op == "bin" and a[2] not in {"/", "%"}:
            dest, left, operator, right = a
            operands = tuple(sorted((left, right))) if operator in _COMMUTATIVE else (left, right)
            key = (operator, *operands)
            previous = expressions.get(key)
            if previous and previous != dest:
                ins = Instruction("move", (dest, previous))
        for dest in _defs(ins):
            expressions = {k:v for k,v in expressions.items() if dest not in k[1:] and v != dest}
        if ins.operation == "bin" and ins.args[2] not in {"/", "%"}:
            dest, left, operator, right = ins.args
            operands = tuple(sorted((left, right))) if operator in _COMMUTATIVE else (left, right)
            expressions[(operator, *operands)] = dest
        result.append(ins)
        if op in {"label", "goto", "iffalse", "iftrue", "func", "endfunc", "call", "call_void", "store", "read", "return"}:
            expressions.clear()
    return result


def _propagate_fold(code: list[Instruction]) -> list[Instruction]:
    """Propagação local de constantes/cópias, sem fatos especulativos em joins."""
    values: dict[str, str] = {}
    result = []
    def resolve(x):
        visited = set()
        while x in values and x not in visited:
            visited.add(x); x = values[x]
        return x
    def kill(dest):
        values.pop(dest, None)
        for name in list(values):
            if values[name] == dest: values.pop(name)
    for ins in code:
        op, a = ins.operation, ins.args
        if op == "move":
            dest, src = a
            src = resolve(src); kill(dest)
            if dest != src:
                ins = Instruction("move", (dest, src))
                values[dest] = src
            else: continue
        elif op == "bin":
            dest, left, operator, right = a
            left, right = resolve(left), resolve(right)
            folded = fold(left, operator, right)
            if folded is None:
                if operator == "+" and right == "0": folded = left
                elif operator == "+" and left == "0": folded = right
                elif operator == "-" and right == "0": folded = left
                elif operator == "*" and right == "1": folded = left
                elif operator == "*" and left == "1": folded = right
                elif operator == "*" and (right == "0" or left == "0"): folded = "0"
            kill(dest)
            if folded is not None:
                ins = Instruction("move", (dest, folded)); values[dest] = folded
            elif operator == "*" and right == "2":
                ins = Instruction("bin", (dest, left, "+", left))  # força reduzida (O09)
            elif operator == "*" and left == "2":
                ins = Instruction("bin", (dest, right, "+", right))
            else:
                ins = Instruction("bin", (dest, left, operator, right))
        elif op == "unary":
            dest, operator, operand = a
            operand = resolve(operand)
            num = number(operand)
            folded = None
            if num is not None and operator == "not": folded = literal(not bool(num))
            elif num is not None and operator == "-": folded = literal(-num)
            elif num is not None and operator == "+": folded = literal(+num)
            kill(dest)
            if folded is not None:
                ins = Instruction("move", (dest, folded)); values[dest] = folded
            else: ins = Instruction("unary", (dest, operator, operand))
        elif op in {"print", "param"}:
            ins = Instruction(op, (resolve(a[0]),))
        elif op == "load":
            kill(a[0]); ins = Instruction(op, (a[0], a[1], resolve(a[2])))
        elif op in _CONDITIONAL:
            ins = Instruction(op, (resolve(a[0]), a[1]))
        elif op == "return" and a:
            ins = Instruction(op, (resolve(a[0]),))
        elif op == "store":
            ins = Instruction(op, (a[0], resolve(a[1]), resolve(a[2])))
        elif op == "alloc":
            kill(a[0]); ins = Instruction(op, (a[0], resolve(a[1])))
        elif op == "read" and a:
            kill(a[0])
        result.append(ins)
        if op in {"label", "goto", "iffalse", "iftrue", "func", "endfunc", "call", "call_void", "return", "store", "read"}:
            values.clear()
    return result


def _simplify_branches(code: list[Instruction]) -> list[Instruction]:
    result = []
    for ins in code:
        if ins.operation in _CONDITIONAL and (value := number(ins.args[0])) is not None:
            jumps = bool(value) if ins.operation == "iftrue" else not bool(value)
            if jumps: result.append(Instruction("goto", (ins.args[1],)))
        else: result.append(ins)
    return result


def _remove_unreachable(code: list[Instruction]) -> list[Instruction]:
    def func(part):
        if not part: return part
        successors = _successors(part)
        seen, stack = set(), [0]
        while stack:
            i = stack.pop()
            if i in seen or i >= len(part): continue
            seen.add(i); stack.extend(successors[i] - seen)
        return [ins for i, ins in enumerate(part) if i in seen or ins.operation == "endfunc"]
    return _per_function(code, func)


def _clean_labels(code: list[Instruction]) -> list[Instruction]:
    """Remove rótulos sem referências e saltos que apenas pulam à próxima linha."""
    for _ in range(4):
        targets = {a[-1] for ins in code if ins.operation in {"goto", "iffalse", "iftrue"} for a in (ins.args,)}
        changed = False
        result = []
        for i, ins in enumerate(code):
            if ins.operation == "label" and ins.args[0] not in targets:
                changed = True; continue
            if ins.operation == "goto" and i + 1 < len(code) and code[i+1] == Instruction("label", (ins.args[0],)):
                changed = True; continue
            result.append(ins)
        code = result
        if not changed: break
    return code


def _dead_assignments(code: list[Instruction]) -> list[Instruction]:
    """DCE por vivacidade (CFG) com efeitos protegidos.

    Chamadas e memória permanecem; divisões e loads não são removidas porque
    podem sinalizar erro em tempo de execução. Nomes compartilhados por funções
    distintas são conservadoramente preservados.
    """
    shared = set()
    observed = {}
    for ins in code:
        if ins.operation == "func": observed[ins.args[0]] = set()
        elif observed:
            next(reversed(observed.values())).update(_uses(ins) | _defs(ins))
    for names in observed.values():
        for other in observed.values():
            if other is not names: shared.update(names & other)
    def func(part):
        part = part[:]
        for _ in range(len(part) + 1):
            succ = _successors(part)
            live_in = [set() for _ in part]
            live_out = [set() for _ in part]
            for _inner in range(max(4, len(part) * 2)):
                change = False
                for i in range(len(part)-1, -1, -1):
                    out = set().union(*(live_in[j] for j in succ[i])) if succ[i] else set()
                    incoming = (_uses(part[i]) | (out - _defs(part[i])))
                    if incoming != live_in[i] or out != live_out[i]:
                        live_in[i], live_out[i] = incoming, out; change = True
                if not change: break
            removable = []
            for i, ins in enumerate(part):
                if ins.operation not in _PURE: continue
                dest = ins.args[0]
                if dest in shared or dest in live_out[i]: continue
                if ins.operation == "bin" and ins.args[2] in {"/", "%"}: continue
                removable.append(i)
            if not removable: break
            doomed = set(removable)
            part = [ins for i, ins in enumerate(part) if i not in doomed]
        return part
    return _per_function(code, func)


def optimize(instructions: list[Instruction]) -> list[Instruction]:
    """O1: LICM segura, CSE local, cópias/constantes, redução de força,
    simplificação de CFG, DCE/liveness, preservando efeitos externos.
    """
    code = _hoist_loop_invariants(instructions)
    code = _cse_local(code)
    for _ in range(4):
        old = code
        code = _propagate_fold(code)
        code = _simplify_branches(code)
        code = _remove_unreachable(code)
        code = _clean_labels(code)
        code = _dead_assignments(code)
        if code == old: break
    return code

def render(ir: list[Instruction]) -> str:
    return "\n".join(("  " if i.operation not in {"func", "endfunc", "label"} else "") + i.render() for i in ir)
