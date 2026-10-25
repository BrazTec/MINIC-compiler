#!/usr/bin/env python3
"""Teste complementar de equivalência da IR (sem alterar .sh oficiais).

Compara código de saída e stdout/stderr para os 30 programas em -O0 e -O1.
Normaliza apenas a codificação de finais de linha para compatibilidade
Windows/MSYS2 (CRLF vs LF). Não modifica gabaritos nem código-fonte.
"""
from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent
CASES = ROOT / "minic-ir-test-cases" / "testes"


def run(command: list[str], *, timeout: float = 12) -> tuple[int, bytes, bytes]:
    env = dict(os.environ)
    env.update(PYTHONUTF8="1", PYTHONIOENCODING="utf-8")
    result = subprocess.run(
        command, cwd=ROOT, capture_output=True, timeout=timeout, env=env,
    )
    def normalize(data: bytes) -> bytes:
        return data.replace(b"\r\n", b"\n")
    return result.returncode, normalize(result.stdout), normalize(result.stderr)


def main() -> int:
    gcc = shutil.which("gcc")
    if not gcc:
        print("ERRO: GCC não encontrado. No Git Bash: export PATH='/c/msys64/ucrt64/bin:$PATH'", file=sys.stderr)
        return 2
    cases = sorted(CASES.rglob("*.c"))
    if len(cases) != 30:
        print(f"ERRO: esperados 30 casos, encontrados {len(cases)}", file=sys.stderr)
        return 2
    with tempfile.TemporaryDirectory(prefix="minic-ir-equivalencia-") as temp:
        binary = Path(temp) / ("minic_compare.exe" if os.name == "nt" else "minic_compare")
        compiled = subprocess.run(
            [gcc, "-Wall", "-Wextra", "-std=c11", "minic.c", "-o", str(binary)],
            cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace",
        )
        if compiled.returncode:
            print("ERRO: compilação C falhou.\n" + compiled.stderr, file=sys.stderr)
            return 2
        failures = 0
        for opt in ("-O0", "-O1"):
            approved = 0
            for source in cases:
                rel = source.relative_to(ROOT)
                try:
                    python_result = run([sys.executable, str(ROOT / "minic.py"), "--ir", opt, str(source)])
                    c_result = run([str(binary), "--ir", opt, str(source)])
                except subprocess.TimeoutExpired:
                    failures += 1
                    print(f"[FALHA][{opt}] {rel} — timeout", flush=True)
                    continue
                if python_result == c_result:
                    approved += 1
                    print(f"[OK][{opt}] {rel}", flush=True)
                else:
                    failures += 1
                    print(f"[FALHA][{opt}] {rel}", flush=True)
                    print(f"  Python: retorno={python_result[0]}, stdout={python_result[1]!r}, stderr={python_result[2]!r}")
                    print(f"  C:      retorno={c_result[0]}, stdout={c_result[1]!r}, stderr={c_result[2]!r}")
            print(f"Resumo {opt}: {approved}/{len(cases)} equivalentes.\n", flush=True)
        print(f"RESULTADO: {2 * len(cases)-failures}/{2 * len(cases)} comparações; divergências={failures}", flush=True)
        return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
