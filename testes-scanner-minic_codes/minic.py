#!/usr/bin/env python3
"""Front-end MINIC: scanner -> parser -> analisador semântico."""

from pathlib import Path
import sys

from parser import Parser, ParserError, LexicalError, tokenize_source, print_syntax_rejection
from semantic import SemanticAnalyzer, render_semantic_result


def main() -> int:
    if len(sys.argv) != 2:
        print("Uso: python minic.py codigo.c", file=sys.stderr)
        return 2

    path = Path(sys.argv[1])

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
        print_syntax_rejection(exc)
        return 1
    except LexicalError as exc:
        print(str(exc), file=sys.stderr)
        return 1

    diagnostics = SemanticAnalyzer(source).analyze(ast)
    sys.stdout.buffer.write(render_semantic_result(diagnostics).encode("utf-8"))
    return 0 if not diagnostics else 1


if __name__ == "__main__":
    raise SystemExit(main())
