#!/usr/bin/env python3


# ============================================================
# CONFIGURAÇÃO
# ============================================================

# Testador do parser Python para o conjunto testes-parser-50.

# Uso padrão:
#     python3 test_parser_python.py

# Uso explícito:
#     python3 test_parser_python.py ./testes-parser-50 ./parser.py

# Contrato verificado:
# - casos ACEITO:
#     * parser retorna código 0;
#     * produz AST em stdout;
#     * AST é comparada com ast.esperada.txt ignorando apenas diferenças
#       irrelevantes de espaços em branco;
# - casos REJEITADO:
#     * parser retorna código diferente de 0;
#     * não produz AST em stdout;
#     * produz diagnóstico identificável como erro sintático;
# - timeout por caso para evitar travamentos.

# O testador também detecta inconsistências simples no próprio gabarito.
# No material atualmente fornecido, o caso 24 possui um ')' excedente em
# ast.esperada.txt. Quando a única diferença for exatamente esse fechamento
# excedente, o caso é validado com ressalva e contabilizado como aviso, não
# como falha do parser.


from __future__ import annotations

import argparse
import difflib
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


SYNTAX_ERROR_RE = re.compile(
    # "erro sint" também reconhece saídas Windows com acentuação
    # eventualmente degradada (ex.: "Erro sint�tico").
    r"(erro\s+sint|erro\s+de\s+sintaxe|syntax\s*error|parse\s*error)",
    re.IGNORECASE,
)

EXPECTED_ACCEPTED = "ACEITO"
EXPECTED_REJECTED = "REJEITADO"

DEFAULT_TIMEOUT_SECONDS = 5.0


@dataclass
class CaseResult:
    case_id: str
    name: str
    expected_status: str
    passed: bool
    warning: bool = False
    detail: str = ""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Testa parser.py contra os 50 casos do parser MINIC."
    )
    parser.add_argument(
        "test_dir",
        nargs="?",
        default="./testes-parser-50",
        help="Diretório testes-parser-50 (padrão: ./testes-parser-50)",
    )
    parser.add_argument(
        "parser_path",
        nargs="?",
        default="./parser.py",
        help="Arquivo parser.py (padrão: ./parser.py)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=DEFAULT_TIMEOUT_SECONDS,
        help=f"Timeout por caso em segundos (padrão: {DEFAULT_TIMEOUT_SECONDS:g})",
    )
    parser.add_argument(
        "--show-diff",
        action="store_true",
        help="Mostra diff completo das ASTs que divergirem.",
    )
    return parser.parse_args()


def normalize_newlines(text: str) -> str:
    return text.replace("\r\n", "\n").replace("\r", "\n")


def normalize_ast(text: str) -> str:
    """
    Normalização propositalmente conservadora para AST.

    Os fixtures apresentam diferenças de espaços internos, por exemplo:
        VarDecl(int x = Lit(int,1))
        VarDecl(int x=Lit(int,1))

    Como a representação é uma S-expressão compacta e os exemplos não usam
    strings nos 25 casos válidos, remover espaços em branco é suficiente para
    comparar a estrutura prevista sem penalizar essas inconsistências.
    """
    return re.sub(r"\s+", "", normalize_newlines(text)).strip()


def parenthesis_balance(text: str) -> tuple[int, int]:
    """
    Retorna:
        (saldo_final, menor_saldo_observado)

    Uma AST textual balanceada deve resultar em (0, >= 0).
    """
    balance = 0
    minimum = 0

    for char in text:
        if char == "(":
            balance += 1
        elif char == ")":
            balance -= 1
            minimum = min(minimum, balance)

    return balance, minimum


def remove_one_closing_paren_variants(text: str):
    """
    Gera variantes removendo exatamente um ')'.

    Usado apenas para diagnosticar um gabarito que possua um único
    parêntese de fechamento excedente. O arquivo original nunca é alterado.
    """
    for index, char in enumerate(text):
        if char == ")":
            yield text[:index] + text[index + 1 :]


def matches_expected_with_one_extra_closing_paren(
    actual: str,
    expected: str,
) -> bool:
    """
    Retorna True somente quando:
    - a AST real está balanceada;
    - a AST esperada possui exatamente um ')' a mais;
    - remover um único ')' do esperado torna as duas ASTs idênticas
      após a normalização de espaços.
    """
    actual_norm = normalize_ast(actual)
    expected_norm = normalize_ast(expected)

    actual_balance, actual_min = parenthesis_balance(actual_norm)
    expected_balance, _ = parenthesis_balance(expected_norm)

    if actual_balance != 0 or actual_min < 0:
        return False

    if expected_balance != -1:
        return False

    return any(
        candidate == actual_norm
        for candidate in remove_one_closing_paren_variants(expected_norm)
    )


def first_difference(expected: str, actual: str) -> str:
    expected_norm = normalize_ast(expected)
    actual_norm = normalize_ast(actual)

    limit = min(len(expected_norm), len(actual_norm))

    for index in range(limit):
        if expected_norm[index] != actual_norm[index]:
            start = max(0, index - 35)
            end_expected = min(len(expected_norm), index + 35)
            end_actual = min(len(actual_norm), index + 35)

            return (
                f"primeira diferença no caractere {index + 1}\n"
                f"      esperado: ...{expected_norm[start:end_expected]}...\n"
                f"      obtido:   ...{actual_norm[start:end_actual]}..."
            )

    if len(expected_norm) != len(actual_norm):
        return (
            "as ASTs coincidem até o fim da menor saída, "
            f"mas possuem tamanhos diferentes "
            f"(esperado={len(expected_norm)}, obtido={len(actual_norm)})"
        )

    return "diferença não identificada"


def read_expected_status(result_file: Path) -> tuple[str, str]:
    text = normalize_newlines(
        result_file.read_text(encoding="utf-8")
    ).strip()

    lines = [line.strip() for line in text.splitlines() if line.strip()]

    if not lines:
        raise ValueError("resultado.esperado.txt vazio")

    status = lines[0].upper()

    if status not in {EXPECTED_ACCEPTED, EXPECTED_REJECTED}:
        raise ValueError(
            f"status inválido em resultado.esperado.txt: {lines[0]!r}"
        )

    hint = " ".join(lines[1:]).strip()
    return status, hint


def discover_cases(test_dir: Path) -> list[Path]:
    case_root = test_dir / "casos"

    if not case_root.is_dir():
        raise FileNotFoundError(
            f"pasta de casos não encontrada: {case_root}"
        )

    cases = [
        directory
        for directory in case_root.iterdir()
        if directory.is_dir() and (directory / "codigo.c").is_file()
    ]

    return sorted(cases, key=lambda path: path.name)


def validate_case_files(case_dir: Path) -> list[str]:
    required = [
        "codigo.c",
        "ast.esperada.txt",
        "resultado.esperado.txt",
    ]

    return [
        filename
        for filename in required
        if not (case_dir / filename).is_file()
    ]


def run_parser(
    parser_path: Path,
    source_path: Path,
    timeout: float,
) -> subprocess.CompletedProcess[str]:
    # No Windows, um processo Python com stdout/stderr redirecionados
    # pode escolher uma codificação local diferente de UTF-8. Como o
    # testador lê a saída como UTF-8, isso pode transformar "sintático"
    # em "sint�tico". Forçamos UTF-8 no processo filho para manter o
    # diagnóstico íntegro e independente da configuração do terminal.
    env = os.environ.copy()
    env["PYTHONIOENCODING"] = "utf-8"
    env["PYTHONUTF8"] = "1"

    return subprocess.run(
        [
            sys.executable,
            str(parser_path),
            str(source_path),
        ],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout,
        env=env,
    )


def test_accepted_case(
    case_id: str,
    case_name: str,
    process: subprocess.CompletedProcess[str],
    expected_ast: str,
    show_diff: bool,
) -> CaseResult:
    stdout = normalize_newlines(process.stdout).strip()
    stderr = normalize_newlines(process.stderr).strip()

    if process.returncode != 0:
        detail = (
            f"parser retornou código {process.returncode}, "
            "mas o caso deveria ser aceito"
        )
        if stderr:
            detail += f"\n      stderr: {stderr}"

        return CaseResult(
            case_id,
            case_name,
            EXPECTED_ACCEPTED,
            False,
            detail=detail,
        )

    if not stdout:
        return CaseResult(
            case_id,
            case_name,
            EXPECTED_ACCEPTED,
            False,
            detail="parser retornou sucesso, mas não produziu AST em stdout",
        )

    if stderr:
        return CaseResult(
            case_id,
            case_name,
            EXPECTED_ACCEPTED,
            False,
            detail=f"caso válido produziu conteúdo em stderr: {stderr}",
        )

    actual_balance, actual_min = parenthesis_balance(normalize_ast(stdout))

    if actual_balance != 0 or actual_min < 0:
        return CaseResult(
            case_id,
            case_name,
            EXPECTED_ACCEPTED,
            False,
            detail=(
                "AST produzida pelo parser possui parênteses desbalanceados "
                f"(saldo={actual_balance}, mínimo={actual_min})"
            ),
        )

    if normalize_ast(stdout) == normalize_ast(expected_ast):
        return CaseResult(
            case_id,
            case_name,
            EXPECTED_ACCEPTED,
            True,
        )

    # Detecta um problema simples no próprio gabarito sem modificar o fixture.
    if matches_expected_with_one_extra_closing_paren(stdout, expected_ast):
        return CaseResult(
            case_id,
            case_name,
            EXPECTED_ACCEPTED,
            True,
            warning=True,
            detail=(
                "AST do parser está balanceada e coincide com o gabarito "
                "após remover exatamente um ')' excedente do "
                "ast.esperada.txt"
            ),
        )

    detail = first_difference(expected_ast, stdout)

    if show_diff:
        expected_lines = [normalize_ast(expected_ast) + "\n"]
        actual_lines = [normalize_ast(stdout) + "\n"]

        diff = "".join(
            difflib.unified_diff(
                expected_lines,
                actual_lines,
                fromfile="esperado",
                tofile="obtido",
            )
        ).rstrip()

        if diff:
            detail += "\n" + "\n".join(
                f"      {line}"
                for line in diff.splitlines()
            )

    return CaseResult(
        case_id,
        case_name,
        EXPECTED_ACCEPTED,
        False,
        detail=detail,
    )


def test_rejected_case(
    case_id: str,
    case_name: str,
    process: subprocess.CompletedProcess[str],
    hint: str,
) -> CaseResult:
    stdout = normalize_newlines(process.stdout).strip()
    stderr = normalize_newlines(process.stderr).strip()
    combined = "\n".join(part for part in (stdout, stderr) if part)

    if process.returncode == 0:
        detail = "parser retornou código 0, mas o caso deveria ser rejeitado"
        if stdout:
            detail += f"\n      stdout: {stdout}"

        return CaseResult(
            case_id,
            case_name,
            EXPECTED_REJECTED,
            False,
            detail=detail,
        )

    # README: casos inválidos devem ser rejeitados sem AST.
    if stdout:
        return CaseResult(
            case_id,
            case_name,
            EXPECTED_REJECTED,
            False,
            detail=(
                "caso rejeitado produziu conteúdo em stdout; "
                "o contrato pede rejeição sem AST"
                f"\n      stdout: {stdout}"
                + (f"\n      stderr: {stderr}" if stderr else "")
            ),
        )

    if not combined:
        return CaseResult(
            case_id,
            case_name,
            EXPECTED_REJECTED,
            False,
            detail="parser rejeitou a entrada, mas não produziu diagnóstico",
        )

    if not SYNTAX_ERROR_RE.search(combined):
        detail = (
            "processo falhou, porém a saída não foi identificada "
            "como erro sintático"
            f"\n      diagnóstico: {combined}"
        )
        if hint:
            detail += f"\n      pista do fixture: {hint}"

        return CaseResult(
            case_id,
            case_name,
            EXPECTED_REJECTED,
            False,
            detail=detail,
        )

    return CaseResult(
        case_id,
        case_name,
        EXPECTED_REJECTED,
        True,
    )


def preflight_parser(parser_path: Path) -> tuple[bool, str]:
    if not parser_path.is_file():
        return False, f"parser não encontrado: {parser_path}"

    process = subprocess.run(
        [
            sys.executable,
            "-m",
            "py_compile",
            str(parser_path),
        ],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )

    if process.returncode != 0:
        diagnostic = (
            normalize_newlines(process.stderr).strip()
            or normalize_newlines(process.stdout).strip()
            or "erro de sintaxe não detalhado"
        )
        return False, diagnostic

    return True, ""


def main() -> int:
    args = parse_args()

    test_dir = Path(args.test_dir).resolve()
    parser_path = Path(args.parser_path).resolve()

    print("== Teste do analisador sintático Python ==")
    print(f"Parser:    {parser_path}")
    print(f"Testes:    {test_dir}")
    print(f"Python:    {sys.executable}")
    print(f"Timeout:   {args.timeout:g}s por caso")
    print()

    if not test_dir.is_dir():
        print(
            f"ERRO: diretório de testes não encontrado: {test_dir}",
            file=sys.stderr,
        )
        return 2

    parser_ok, parser_error = preflight_parser(parser_path)

    if not parser_ok:
        print("FALHA: parser.py não passou pela verificação sintática.")
        print(parser_error)
        return 2

    print("Verificação sintática do parser.py: OK")
    print()

    try:
        cases = discover_cases(test_dir)
    except FileNotFoundError as exc:
        print(f"ERRO: {exc}", file=sys.stderr)
        return 2

    print(f"Casos encontrados: {len(cases)}")
    print("-" * 78)

    results: list[CaseResult] = []
    fixture_errors = 0

    for index, case_dir in enumerate(cases, start=1):
        case_name = case_dir.name
        case_id = case_name.split("_", 1)[0]

        missing = validate_case_files(case_dir)

        if missing:
            fixture_errors += 1
            result = CaseResult(
                case_id,
                case_name,
                "DESCONHECIDO",
                False,
                detail="arquivos ausentes: " + ", ".join(missing),
            )
            results.append(result)

            print(
                f"[{index:02d}/{len(cases):02d}] "
                f"{case_name:<48} FALHOU (fixture incompleto)"
            )
            continue

        source_path = case_dir / "codigo.c"
        expected_ast = (case_dir / "ast.esperada.txt").read_text(
            encoding="utf-8"
        )

        try:
            expected_status, hint = read_expected_status(
                case_dir / "resultado.esperado.txt"
            )
        except (OSError, ValueError) as exc:
            fixture_errors += 1
            result = CaseResult(
                case_id,
                case_name,
                "DESCONHECIDO",
                False,
                detail=f"resultado esperado inválido: {exc}",
            )
            results.append(result)

            print(
                f"[{index:02d}/{len(cases):02d}] "
                f"{case_name:<48} FALHOU (resultado esperado inválido)"
            )
            continue

        try:
            process = run_parser(
                parser_path,
                source_path,
                args.timeout,
            )
        except subprocess.TimeoutExpired:
            result = CaseResult(
                case_id,
                case_name,
                expected_status,
                False,
                detail=f"timeout após {args.timeout:g}s",
            )
            results.append(result)

            print(
                f"[{index:02d}/{len(cases):02d}] "
                f"{case_name:<48} FALHOU (timeout)"
            )
            continue

        if expected_status == EXPECTED_ACCEPTED:
            result = test_accepted_case(
                case_id,
                case_name,
                process,
                expected_ast,
                args.show_diff,
            )
        else:
            result = test_rejected_case(
                case_id,
                case_name,
                process,
                hint,
            )

        results.append(result)

        if result.passed and result.warning:
            status_text = "OK (RESSALVA NO GABARITO)"
        elif result.passed:
            status_text = "OK"
        else:
            status_text = "FALHOU"

        print(
            f"[{index:02d}/{len(cases):02d}] "
            f"{case_name:<48} {status_text}"
        )

        if result.detail and (not result.passed or result.warning):
            for line in result.detail.splitlines():
                print(f"      {line}")

    accepted = [
        result
        for result in results
        if result.expected_status == EXPECTED_ACCEPTED
    ]
    rejected = [
        result
        for result in results
        if result.expected_status == EXPECTED_REJECTED
    ]

    passed = sum(result.passed for result in results)
    failed = len(results) - passed
    warnings = sum(result.warning for result in results)

    accepted_passed = sum(result.passed for result in accepted)
    rejected_passed = sum(result.passed for result in rejected)

    print()
    print("=" * 78)
    print("Resumo")
    print("------")
    print(f"Total de casos:                 {len(results)}")
    print(
        f"Casos ACEITO aprovados:         "
        f"{accepted_passed}/{len(accepted)}"
    )
    print(
        f"Casos REJEITADO aprovados:      "
        f"{rejected_passed}/{len(rejected)}"
    )
    print(f"Total aprovado:                 {passed}")
    print(f"Falhas:                         {failed}")
    print(f"Avisos/ressalvas de fixture:    {warnings}")
    print(f"Erros de estrutura dos fixtures:{fixture_errors:>5}")
    print("=" * 78)

    if len(results) != 50:
        print(
            f"RESULTADO FINAL: os casos encontrados foram validados, "
            f"mas o conjunto possui {len(results)} casos em vez de 50."
        )
        return 1 if failed else 0

    if failed == 0:
        if warnings:
            print(
                "RESULTADO FINAL: TODOS OS 50 CASOS PASSARAM "
                f"({warnings} ressalva(s) identificada(s) no gabarito)."
            )
        else:
            print(
                "RESULTADO FINAL: TODOS OS 50 CASOS PASSARAM."
            )
        return 0

    print(
        "RESULTADO FINAL: há casos que precisam ser isolados e corrigidos."
    )
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
