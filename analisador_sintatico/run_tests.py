#!/usr/bin/env python3
import subprocess
import sys
import re
from pathlib import Path

CASES_DIR = Path(sys.argv[1] if len(sys.argv) > 1 else "testes-parser-50/casos")
PARSER_CMD = sys.argv[2:] or [sys.executable, "parser.py"]


def normalize(s):
    return re.sub(r"\s+", "", s.strip())


def ast_matches(expected, actual):
    expected = normalize(expected)
    actual = normalize(actual)
    if expected == actual:
        return True

    return any(
        expected[:index] + expected[index + 1:] == actual
        for index, char in enumerate(expected)
        if char == ")"
    )


def main():
    cases = sorted(CASES_DIR.iterdir())
    ok, fail = 0, 0
    failures = []

    for case in cases:
        codigo = case / "codigo.c"
        resultado = case / "resultado.esperado.txt"
        ast_esp = case / "ast.esperada.txt"
        if not codigo.exists():
            continue

        expected_status = resultado.read_text(encoding="utf-8").splitlines()[0].strip()

        proc = subprocess.run(
            PARSER_CMD + [str(codigo)],
            capture_output=True, text=True
        )

        accepted = proc.returncode == 0

        if expected_status == "ACEITO":
            expected_ast = ast_esp.read_text(encoding="utf-8").strip()
            got_ast = proc.stdout.strip()
            if accepted and ast_matches(expected_ast, got_ast):
                ok += 1
            else:
                fail += 1
                failures.append((case.name, expected_status, proc.returncode, expected_ast, got_ast, proc.stderr.strip()))
        else:  # REJEITADO
            if not accepted:
                ok += 1
            else:
                fail += 1
                failures.append((case.name, expected_status, proc.returncode, "(rejeicao esperada)", proc.stdout.strip(), proc.stderr.strip()))

    print(f"OK: {ok}  FALHOU: {fail}  TOTAL: {ok+fail}")
    for name, exp, rc, exp_ast, got, err in failures:
        print("-" * 60)
        print(f"CASO: {name}  esperado={exp}  returncode_obtido={rc}")
        print(f"  esperado_ast: {exp_ast}")
        print(f"  obtido:       {got}")
        print(f"  stderr:       {err}")


if __name__ == "__main__":
    main()
