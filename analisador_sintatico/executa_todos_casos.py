#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

BASE_DIR = Path(__file__).resolve().parent
CASES_DIR = BASE_DIR / "testes-parser-50" / "casos"
PARSER_PATH = BASE_DIR / "parser.py"


def main() -> int:
    if not PARSER_PATH.exists():
        print(f"Arquivo do parser não encontrado: {PARSER_PATH}")
        return 1

    if not CASES_DIR.exists():
        print(f"Diretório de casos não encontrado: {CASES_DIR}")
        return 1

    output_path = BASE_DIR / "executa_todos_casos_python.md"
    linhas = ["# Resultados dos casos do parser", ""]

    cases = sorted(p for p in CASES_DIR.iterdir() if p.is_dir())

    for case in cases:
        codigo = case / "codigo.c"
        if not codigo.exists():
            continue

        rel_codigo = Path(".") / "testes-parser-50" / "casos" / case.name / "codigo.c"
        comando_cli = f"python .\parser.py {rel_codigo.as_posix()}"
        proc = subprocess.run(
            [sys.executable, str(PARSER_PATH), str(codigo)],
            capture_output=True,
            text=True,
        )

        saida = proc.stdout.strip() or proc.stderr.strip() or "(sem saída)"

        linhas.append(f"## Caso: {case.name}")
        linhas.append("")
        linhas.append("**Comando usado para rodar:**")
        linhas.append(f"```bash\n{comando_cli}\n```")
        linhas.append("")
        linhas.append("**Saída:**")
        linhas.append(f"```text\n{saida}\n```")
        linhas.append("")

    output_path.write_text("\n".join(linhas), encoding="utf-8")
    print(f"Relatório salvo em: {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
