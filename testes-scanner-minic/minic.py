#!/usr/bin/env python3
"""Front-end MINIC: scanner -> parser -> analisador semântico."""

from pathlib import Path
import sys

from parser import Parser, ParserError, LexicalError, tokenize_source, print_syntax_rejection
from semantic import SemanticAnalyzer, render_semantic_result
from ir import IRGenerator, render as render_ir


def main() -> int:
    # Sem --ir, preserva estritamente o contrato da suíte semântica.
    args = sys.argv[1:]
    emit_ir = "--ir" in args
    optimization = 1 if "-O1" in args else 0
    if "-O0" in args and optimization:
        print("ERRO: não combine -O0 e -O1", file=sys.stderr)
        return 2
    filenames = [x for x in args if x not in {"--ir", "-O0", "-O1"}]
    if len(filenames) != 1 or len(args) != len(filenames) + sum(x in args for x in ("--ir", "-O0", "-O1")):
        print("Uso: python minic.py [--ir] [-O0|-O1] codigo.c", file=sys.stderr)
        return 2

    path = Path(filenames[0])

    try:
        source = path.read_text(encoding="utf-8")
    except OSError as exc:
        print(f"ERRO: não foi possível ler o arquivo: {exc}", file=sys.stderr)
        return 2

    try:
        tokens = tokenize_source(source)
        # semantic_mode posterga a regra de valor-L para a camada semântica.
        ast = Parser(tokens, semantic_mode=True).parse()
    except ParserError as exc:
        if emit_ir:
            print(str(exc), file=sys.stderr)
            return 3
        print_syntax_rejection(exc)
        return 1
    except LexicalError as exc:
        print(str(exc), file=sys.stderr)
        return 2 if emit_ir else 1

    diagnostics = SemanticAnalyzer(source).analyze(ast)
    if emit_ir:
        if diagnostics:
            for diagnostic in diagnostics:
                print(diagnostic.render(), file=sys.stderr)
            return 4
        result = IRGenerator().generate(ast, optimization=optimization)
        text = render_ir(result)
        sys.stdout.buffer.write((text + "\n").encode("utf-8"))
        return 0
    sys.stdout.buffer.write(render_semantic_result(diagnostics).encode("utf-8"))
    return 0 if not diagnostics else 1


if __name__ == "__main__":
    raise SystemExit(main())
