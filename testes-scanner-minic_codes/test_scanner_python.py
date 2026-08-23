#!/usr/bin/env python3

import json
import subprocess
import sys
from pathlib import Path


# ============================================================
# CONFIGURAÇÃO
# ============================================================

# Uso:
#
#   python3 test_scanner_python.py
#
# ou:
#
#   python3 test_scanner_python.py scanner.py .
#
# Primeiro argumento:
#   arquivo do scanner
#
# Segundo argumento:
#   diretório onde estão os testes

SCANNER = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("scanner.py")
TESTS_DIR = Path(sys.argv[2]) if len(sys.argv) > 2 else Path(".")

TOTAL = 0
PASSED = 0
FAILED = 0
WARNINGS = 0


# ============================================================
# LEITURA DE JSONL
# ============================================================

def read_jsonl(path):
    """
    Lê um arquivo JSONL.

    Linhas vazias são ignoradas.

    Retorna uma lista de objetos Python.
    """

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
    """
    Exibe a primeira diferença entre duas
    sequências de tokens.
    """

    maximum = max(
        len(got),
        len(want)
    )

    for index in range(maximum):

        if index < len(got):
            actual = got[index]
        else:
            actual = "<ausente>"

        if index < len(want):
            expected = want[index]
        else:
            expected = "<a mais>"

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
    """
    Compara os tokens produzidos pelo scanner
    com o arquivo .expected.jsonl.

    Objetos contendo "error" são ignorados,
    reproduzindo o comportamento do script
    test_scanner_python.sh fornecido.
    """

    try:

        expected_tokens = read_jsonl(
            expected_path
        )

    except json.JSONDecodeError:
        return False

    # O scanner pode produzir:
    #
    # {"token": ...}
    #
    # e:
    #
    # {"error": ...}
    #
    # O script original compara apenas tokens.

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
# EXECUÇÃO DO SCANNER
# ============================================================

def run_case(input_path, expected_path):
    """
    Executa um único caso de teste.
    """

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

    print()
    print("=" * 64)
    print(f"Caso: {label}")

    print(
        f"Comando: "
        f"{sys.executable} "
        f"{SCANNER} "
        f"{input_path}"
    )

    print(
        f"Resultado esperado: "
        f"{expected_path}"
    )

    # --------------------------------------------------------
    # Executa o scanner
    # --------------------------------------------------------

    try:

        process = subprocess.run(
            [
                sys.executable,
                str(SCANNER),
                str(input_path),
            ],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )

    except OSError as exc:

        print(
            f"FALHA: não foi possível "
            f"executar o scanner: {exc}"
        )

        FAILED += 1
        return

    # --------------------------------------------------------
    # STDERR
    # --------------------------------------------------------

    if process.stderr.strip():

        print("Saída de erro do scanner:")

        print(
            process.stderr.rstrip()
        )

    # --------------------------------------------------------
    # Interpreta stdout como JSONL
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
    # Compara
    # --------------------------------------------------------

    if valid_json and compare_jsonl(
        produced,
        expected_path
    ):

        print("RESULTADO: OK")
        PASSED += 1

    else:

        print(
            "RESULTADO: FALHOU — "
            "tokens diferentes do esperado"
        )

        FAILED += 1

    # --------------------------------------------------------
    # Código de retorno diferente de zero
    # --------------------------------------------------------

    if process.returncode != 0:

        WARNINGS += 1

        print(
            f"Aviso: o scanner terminou "
            f"com código "
            f"{process.returncode}."
        )


# ============================================================
# DESCOBERTA DA ENTRADA
# ============================================================

def find_inputs(expected):
    """
    Descobre qual entrada corresponde ao
    arquivo .expected.jsonl.

    Exemplos:

    v01_declaracoes.expected.jsonl
        ->
    v01_declaracoes.minic

    c01_fibonacci.c.expected.jsonl
        ->
    c01_fibonacci.c
    """

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
# MAIN
# ============================================================

def main():

    global WARNINGS

    # --------------------------------------------------------
    # Verificações iniciais
    # --------------------------------------------------------

    if not SCANNER.is_file():

        print(
            f"ERRO: scanner não encontrado: "
            f"{SCANNER}",
            file=sys.stderr,
        )

        return 2

    if not TESTS_DIR.is_dir():

        print(
            f"ERRO: diretório de testes "
            f"não encontrado: "
            f"{TESTS_DIR}",
            file=sys.stderr,
        )

        return 2

    # --------------------------------------------------------
    # Localiza todos os expected
    # --------------------------------------------------------

    expected_files = sorted(
        TESTS_DIR.rglob(
            "*.expected.jsonl"
        )
    )

    if not expected_files:

        print(
            "ERRO: nenhum resultado esperado "
            "para .c ou .minic foi encontrado.",
            file=sys.stderr,
        )

        return 2

    print("== Testando o analisador léxico Python ==")

    print(
        f"Scanner: {SCANNER}"
    )

    print(
        f"Diretório de testes: "
        f"{TESTS_DIR.resolve()}"
    )

    print(
        f"Fixtures encontrados: "
        f"{len(expected_files)}"
    )

    # --------------------------------------------------------
    # Executa cada fixture
    # --------------------------------------------------------

    for expected in expected_files:

        inputs = find_inputs(expected)

        if not inputs:

            print()
            print("=" * 64)

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
    print("=" * 64)

    print(
        f"Resumo: "
        f"{PASSED} OK, "
        f"{FAILED} falharam, "
        f"{WARNINGS} avisos, "
        f"{TOTAL} casos verificados."
    )

    # Mesmo critério do script Bash:
    #
    # sucesso somente se:
    # - nenhuma falha
    # - nenhum aviso

    if FAILED == 0 and WARNINGS == 0:

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