#!/usr/bin/env python3

import json
import os
import shutil
import subprocess
import sys
from pathlib import Path


# ============================================================
# CONFIGURAÇÃO
# ============================================================

SCANNER_SOURCE = (
    Path(sys.argv[1])
    if len(sys.argv) > 1
    else Path("scanner.c")
)

TESTS_DIR = (
    Path(sys.argv[2])
    if len(sys.argv) > 2
    else Path(".")
)

SCANNER_BINARY = Path("scanner.exe")


TOTAL = 0
PASSED = 0
FAILED = 0
WARNINGS = 0


# ============================================================
# LOCALIZA O GCC
# ============================================================

def find_gcc():
    """
    Procura primeiro o gcc disponível no PATH.

    Caso não encontre, tenta o caminho padrão
    utilizado pelo MSYS2 UCRT64 no Windows.
    """

    gcc = shutil.which("gcc")

    if gcc:
        return gcc

    msys2_gcc = Path(
        r"C:\msys64\ucrt64\bin\gcc.exe"
    )

    if msys2_gcc.is_file():
        return str(msys2_gcc)

    return None


# ============================================================
# LEITURA JSONL
# ============================================================

def read_jsonl(path):
    path = Path(path)

    if not path.exists():
        return []

    values = []

    lines = path.read_text(
        encoding="utf-8"
    ).splitlines()

    for number, line in enumerate(lines, 1):

        if not line.strip():
            continue

        try:
            values.append(
                json.loads(line)
            )

        except json.JSONDecodeError as exc:
            print(
                f"FALHA: JSON inválido em "
                f"{path}:{number}: {exc}"
            )

            raise

    return values


# ============================================================
# PRIMEIRA DIFERENÇA
# ============================================================

def show_first_difference(got, want):

    maximum = max(
        len(got),
        len(want)
    )

    for index in range(maximum):

        actual = (
            got[index]
            if index < len(got)
            else "<ausente>"
        )

        expected = (
            want[index]
            if index < len(want)
            else "<a mais>"
        )

        if actual != expected:

            print(
                f"  primeira diferença "
                f"na posição {index + 1}:"
            )

            print(
                "    esperado: "
                + json.dumps(
                    expected,
                    ensure_ascii=False
                )
            )

            print(
                "    obtido:   "
                + json.dumps(
                    actual,
                    ensure_ascii=False
                )
            )

            return


# ============================================================
# COMPARAÇÃO
# ============================================================

def compare_jsonl(produced, expected_path):

    try:
        expected_tokens = read_jsonl(
            expected_path
        )

    except json.JSONDecodeError:
        return False

    # Mesmo comportamento dos scripts fornecidos
    # pelo professor:
    #
    # objetos de erro não entram na comparação
    # dos tokens.

    produced_tokens = [
        item
        for item in produced
        if "token" in item
    ]

    if produced_tokens != expected_tokens:

        print(
            "FALHA: sequência de tokens "
            "diferente."
        )

        show_first_difference(
            produced_tokens,
            expected_tokens
        )

        return False

    print(
        f"OK: {len(produced_tokens)} "
        f"token(s)."
    )

    return True


# ============================================================
# COMPILAÇÃO
# ============================================================

def compile_scanner():

    gcc = find_gcc()

    if gcc is None:
        print("ERRO: GCC não encontrado.")
        print(
            "Verifique se o MSYS2 UCRT64 "
            "está instalado."
        )
        return False

    print(
        "== Compilando o analisador léxico C =="
    )

    print(
        f"Compilador: {gcc}"
    )

    command = [
        gcc,
        "-Wall",
        "-Wextra",
        "-std=c11",
        str(SCANNER_SOURCE),
        "-o",
        str(SCANNER_BINARY),
    ]

    print(
        "Comando:",
        " ".join(command)
    )

    print("-" * 68)

    # Não captura stdout/stderr.
    # Assim qualquer erro do GCC aparece
    # diretamente no terminal.
    # --------------------------------------------------------
    # Ambiente do MSYS2 / MinGW
    # --------------------------------------------------------

    env = os.environ.copy()

    gcc_dir = str(
        Path(gcc).resolve().parent
    )

    env["PATH"] = (
        gcc_dir
        + os.pathsep
        + env.get("PATH", "")
    )

    try:
        process = subprocess.run(
            command,
            env=env
        )

    except OSError as exc:
        print(
            f"ERRO ao executar GCC: {exc}"
        )
        return False

    print("-" * 68)

    print(
        f"Código de retorno do GCC: "
        f"{process.returncode}"
    )

    if process.returncode != 0:
        print(
            "FALHA: a compilação falhou."
        )
        return False

    if not SCANNER_BINARY.is_file():
        print(
            "FALHA: o GCC retornou sucesso, "
            "mas scanner.exe não foi encontrado."
        )
        return False

    print(
        f"Executável gerado: "
        f"{SCANNER_BINARY.resolve()}"
    )

    print()

    return True


# ============================================================
# LOCALIZA ENTRADA DO FIXTURE
# ============================================================

def find_inputs(expected):

    suffix = ".expected.jsonl"

    expected_string = str(expected)

    base_string = expected_string[
        :-len(suffix)
    ]

    base = Path(base_string)

    inputs = []

    # Exemplo:
    #
    # c01_fibonacci.c.expected.jsonl
    # ->
    # c01_fibonacci.c

    if base.is_file():
        inputs.append(base)

    # Exemplo:
    #
    # v01_declaracoes.expected.jsonl
    # ->
    # v01_declaracoes.minic

    minic = Path(
        str(base) + ".minic"
    )

    if minic.is_file():
        inputs.append(minic)

    return inputs


# ============================================================
# EXECUTA UM CASO
# ============================================================

def run_case(input_path, expected_path):

    global TOTAL
    global PASSED
    global FAILED
    global WARNINGS

    TOTAL += 1

    try:
        label = input_path.relative_to(
            TESTS_DIR
        )
    except ValueError:
        label = input_path

    print("=" * 68)

    print(
        f"Caso: {label}"
    )

    print(
        f"Resultado esperado: "
        f"{expected_path}"
    )

    executable = (
        SCANNER_BINARY.resolve()
    )

    try:
        env = os.environ.copy()

        msys2_bin = r"C:\msys64\ucrt64\bin"

        env["PATH"] = (
            msys2_bin
            + os.pathsep
            + env.get("PATH", "")
        )

        process = subprocess.run(
            [
                str(executable),
                str(input_path),
            ],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            env=env,
        )

    except OSError as exc:

        print(
            f"FALHA: não foi possível "
            f"executar scanner.exe: {exc}"
        )

        FAILED += 1

        return

    # --------------------------------------------------------
    # STDERR
    # --------------------------------------------------------

    if process.stderr.strip():

        print(
            "Saída de erro do scanner:"
        )

        print(
            process.stderr.rstrip()
        )

    # --------------------------------------------------------
    # INTERPRETA STDOUT COMO JSONL
    # --------------------------------------------------------

    produced = []

    valid_json = True

    for number, line in enumerate(
        process.stdout.splitlines(),
        1
    ):

        if not line.strip():
            continue

        try:

            produced.append(
                json.loads(line)
            )

        except json.JSONDecodeError as exc:

            print(
                f"FALHA: JSON inválido "
                f"produzido pelo scanner "
                f"na linha {number}: {exc}"
            )

            print(
                f"  conteúdo: {line}"
            )

            valid_json = False

            break

    # --------------------------------------------------------
    # COMPARAÇÃO
    # --------------------------------------------------------

    if (
        valid_json
        and compare_jsonl(
            produced,
            expected_path
        )
    ):

        print(
            "RESULTADO: OK"
        )

        PASSED += 1

    else:

        print(
            "RESULTADO: FALHOU — "
            "tokens diferentes do esperado"
        )

        FAILED += 1

    # --------------------------------------------------------
    # CÓDIGO DE RETORNO
    # --------------------------------------------------------

    if process.returncode != 0:

        WARNINGS += 1

        print(
            f"Aviso: scanner.exe terminou "
            f"com código "
            f"{process.returncode}."
        )


# ============================================================
# MAIN
# ============================================================

def main():

    global WARNINGS

    if not SCANNER_SOURCE.is_file():

        print(
            f"ERRO: arquivo não encontrado: "
            f"{SCANNER_SOURCE}"
        )

        return 2

    if not TESTS_DIR.is_dir():

        print(
            f"ERRO: diretório de testes "
            f"não encontrado: "
            f"{TESTS_DIR}"
        )

        return 2

    # --------------------------------------------------------
    # COMPILA PRIMEIRO
    # --------------------------------------------------------

    if not compile_scanner():
        return 2

    # --------------------------------------------------------
    # LOCALIZA FIXTURES
    # --------------------------------------------------------

    expected_files = sorted(
        TESTS_DIR.rglob(
            "*.expected.jsonl"
        )
    )

    if not expected_files:

        print(
            "ERRO: nenhum resultado esperado "
            "foi encontrado."
        )

        return 2

    print(
        f"Fixtures encontrados: "
        f"{len(expected_files)}"
    )

    print()

    # --------------------------------------------------------
    # EXECUTA TODOS
    # --------------------------------------------------------

    for expected in expected_files:

        inputs = find_inputs(
            expected
        )

        if not inputs:

            print("=" * 68)

            print(
                "AVISO: entrada correspondente "
                "não encontrada para:"
            )

            print(
                f"  {expected}"
            )

            WARNINGS += 1

            continue

        for input_path in inputs:

            run_case(
                input_path,
                expected
            )

    # --------------------------------------------------------
    # RESUMO
    # --------------------------------------------------------

    print()
    print("=" * 68)

    print(
        f"Resumo: "
        f"{PASSED} OK, "
        f"{FAILED} falharam, "
        f"{WARNINGS} avisos, "
        f"{TOTAL} casos verificados."
    )

    if (
        FAILED == 0
        and WARNINGS == 0
    ):

        print(
            "RESULTADO FINAL: "
            "TODOS OS TESTES PASSARAM."
        )

        return 0

    print(
        "RESULTADO FINAL: "
        "EXISTEM FALHAS OU AVISOS."
    )

    return 1


if __name__ == "__main__":
    raise SystemExit(main())