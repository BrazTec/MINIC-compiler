#!/usr/bin/env python3
"""
Regulariza os gabaritos dos casos sintaticamente inválidos do parser MINIC.

Objetivo
--------
Os casos REJEITADO possuem, originalmente, um ast.esperada.txt com apenas:

    NÃO HÁ AST: o parser deve rejeitar a entrada.

Entretanto, o README também exige diagnóstico sintático e informa que a
mensagem exata pode variar. O parser deste projeto adota uma saída em duas
camadas, enviada para stderr:

    NÃO HÁ AST: o parser deve rejeitar a entrada.
    Erro sintático na linha X, coluna Y: ...

Este script executa o parser nos casos marcados como REJEITADO no
manifesto.json, valida o comportamento e, somente quando solicitado com
--apply, atualiza o ast.esperada.txt para refletir exatamente a saída
padronizada do parser.

Por segurança:
- por padrão, funciona em modo de pré-visualização e NÃO altera arquivos;
- valida todos os casos antes de escrever qualquer gabarito;
- exige código de saída diferente de zero;
- exige stdout vazio;
- exige a linha "NÃO HÁ AST: o parser deve rejeitar a entrada.";
- exige uma mensagem identificável como erro sintático;
- cria backup dos gabaritos originais antes de aplicar alterações.

Uso
---
Pré-visualizar:
    python3 regularizar_gabaritos_invalidos.py

Aplicar:
    python3 regularizar_gabaritos_invalidos.py --apply

Caminhos explícitos:
    python3 regularizar_gabaritos_invalidos.py \
        ./testes-parser-50 ./parser.py --apply
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path


REJECTION_HEADER = "NÃO HÁ AST: o parser deve rejeitar a entrada."

SYNTAX_ERROR_RE = re.compile(
    r"(erro\s+sint|erro\s+de\s+sintaxe|syntax\s*error|parse\s*error)",
    re.IGNORECASE,
)

DEFAULT_TIMEOUT_SECONDS = 5.0


@dataclass
class RejectedCase:
    case_id: int
    title: str
    directory: Path
    hint: str


@dataclass
class ProposedUpdate:
    case: RejectedCase
    expected_path: Path
    old_content: str
    new_content: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Regulariza ast.esperada.txt dos casos REJEITADO "
            "a partir da saída validada do parser.py."
        )
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
        "--apply",
        action="store_true",
        help="Aplica as alterações. Sem esta opção, apenas pré-visualiza.",
    )

    parser.add_argument(
        "--timeout",
        type=float,
        default=DEFAULT_TIMEOUT_SECONDS,
        help=f"Timeout por caso em segundos (padrão: {DEFAULT_TIMEOUT_SECONDS:g})",
    )

    parser.add_argument(
        "--show-content",
        action="store_true",
        help="Mostra o conteúdo completo que será gravado em cada gabarito.",
    )

    return parser.parse_args()


def normalize_text(text: str) -> str:
    """
    Normaliza apenas aspectos de arquivo:
    - CRLF/CR -> LF;
    - remove espaços em branco no fim de cada linha;
    - remove linhas vazias no início/fim;
    - garante exatamente um LF ao final.

    O conteúdo do diagnóstico não é reescrito.
    """
    text = text.replace("\r\n", "\n").replace("\r", "\n")

    lines = [line.rstrip() for line in text.split("\n")]

    while lines and not lines[0]:
        lines.pop(0)

    while lines and not lines[-1]:
        lines.pop()

    return "\n".join(lines) + "\n"


def load_rejected_cases(test_dir: Path) -> list[RejectedCase]:
    manifest_path = test_dir / "manifesto.json"

    if not manifest_path.is_file():
        raise FileNotFoundError(
            f"manifesto.json não encontrado: {manifest_path}"
        )

    with manifest_path.open("r", encoding="utf-8-sig") as file:
        manifest = json.load(file)

    cases = []

    for item in manifest.get("casos", []):
        if str(item.get("resultado", "")).upper() != "REJEITADO":
            continue

        case_id = int(item["id"])
        directory = test_dir / item["diretorio"]

        cases.append(
            RejectedCase(
                case_id=case_id,
                title=str(item.get("titulo", "")),
                directory=directory,
                hint=str(item.get("pista", "")),
            )
        )

    cases.sort(key=lambda case: case.case_id)
    return cases


def validate_project_structure(
    test_dir: Path,
    parser_path: Path,
    cases: list[RejectedCase],
) -> list[str]:
    errors = []

    if not test_dir.is_dir():
        errors.append(f"diretório de testes não encontrado: {test_dir}")

    if not parser_path.is_file():
        errors.append(f"parser.py não encontrado: {parser_path}")

    if not cases:
        errors.append("nenhum caso REJEITADO encontrado no manifesto.json")

    for case in cases:
        if not case.directory.is_dir():
            errors.append(
                f"caso {case.case_id:02d}: diretório não encontrado: "
                f"{case.directory}"
            )
            continue

        source = case.directory / "codigo.c"
        expected = case.directory / "ast.esperada.txt"
        result = case.directory / "resultado.esperado.txt"

        if not source.is_file():
            errors.append(
                f"caso {case.case_id:02d}: codigo.c ausente"
            )

        if not expected.is_file():
            errors.append(
                f"caso {case.case_id:02d}: ast.esperada.txt ausente"
            )

        if not result.is_file():
            errors.append(
                f"caso {case.case_id:02d}: resultado.esperado.txt ausente"
            )

    return errors


def preflight_parser(parser_path: Path) -> tuple[bool, str]:
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

    if process.returncode == 0:
        return True, ""

    diagnostic = (
        process.stderr.strip()
        or process.stdout.strip()
        or "erro não detalhado"
    )

    return False, diagnostic


def run_parser(
    parser_path: Path,
    source_path: Path,
    timeout: float,
) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()

    # Mantém acentos íntegros tanto no PowerShell quanto no MSYS2/Bash.
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
        errors="strict",
        timeout=timeout,
        env=env,
    )


def validate_rejection_output(
    process: subprocess.CompletedProcess[str],
) -> tuple[bool, str, str]:
    """
    Retorna:
        (ok, mensagem_de_validação, conteúdo_para_gabarito)
    """
    stdout = normalize_text(process.stdout) if process.stdout else ""
    stderr = normalize_text(process.stderr) if process.stderr else ""

    if process.returncode == 0:
        return (
            False,
            "parser retornou código 0 para caso que deveria ser rejeitado",
            "",
        )

    if stdout.strip():
        return (
            False,
            (
                "parser produziu conteúdo em stdout; "
                "casos rejeitados não devem produzir AST"
            ),
            "",
        )

    if not stderr.strip():
        return (
            False,
            "parser rejeitou a entrada, mas não produziu diagnóstico em stderr",
            "",
        )

    lines = [
        line.strip()
        for line in stderr.replace("\r\n", "\n").splitlines()
        if line.strip()
    ]

    if not lines:
        return (
            False,
            "stderr vazio após normalização",
            "",
        )

    if lines[0] != REJECTION_HEADER:
        return (
            False,
            (
                "primeira linha diferente do cabeçalho padronizado; "
                f"esperado: {REJECTION_HEADER!r}; "
                f"obtido: {lines[0]!r}"
            ),
            "",
        )

    diagnostic = "\n".join(lines[1:])

    if not diagnostic:
        return (
            False,
            "cabeçalho de rejeição presente, mas diagnóstico sintático ausente",
            "",
        )

    if not SYNTAX_ERROR_RE.search(diagnostic):
        return (
            False,
            (
                "a segunda camada não foi reconhecida como diagnóstico "
                "de erro sintático"
            ),
            "",
        )

    return (
        True,
        "rejeição e diagnóstico sintático validados",
        normalize_text(stderr),
    )


def collect_updates(
    parser_path: Path,
    cases: list[RejectedCase],
    timeout: float,
    show_content: bool,
) -> tuple[list[ProposedUpdate], list[str]]:
    updates = []
    errors = []

    for index, case in enumerate(cases, start=1):
        source_path = case.directory / "codigo.c"
        expected_path = case.directory / "ast.esperada.txt"

        try:
            process = run_parser(
                parser_path,
                source_path,
                timeout,
            )
        except subprocess.TimeoutExpired:
            message = (
                f"caso {case.case_id:02d}: "
                f"timeout após {timeout:g}s"
            )
            errors.append(message)
            print(
                f"[{index:02d}/{len(cases):02d}] "
                f"{case.case_id:02d} {case.title:<44} FALHOU (timeout)"
            )
            continue
        except UnicodeDecodeError as exc:
            message = (
                f"caso {case.case_id:02d}: "
                f"saída não pôde ser lida como UTF-8: {exc}"
            )
            errors.append(message)
            print(
                f"[{index:02d}/{len(cases):02d}] "
                f"{case.case_id:02d} {case.title:<44} FALHOU (encoding)"
            )
            continue

        ok, validation_message, new_content = validate_rejection_output(
            process
        )

        if not ok:
            errors.append(
                f"caso {case.case_id:02d}: {validation_message}"
            )

            print(
                f"[{index:02d}/{len(cases):02d}] "
                f"{case.case_id:02d} {case.title:<44} FALHOU"
            )
            print(f"      {validation_message}")

            if process.stdout.strip():
                print("      stdout:")
                for line in process.stdout.splitlines():
                    print(f"        {line}")

            if process.stderr.strip():
                print("      stderr:")
                for line in process.stderr.splitlines():
                    print(f"        {line}")

            continue

        old_content = normalize_text(
            expected_path.read_text(
                encoding="utf-8-sig"
            )
        )

        changed = old_content != new_content

        status = "ALTERAR" if changed else "JÁ REGULARIZADO"

        print(
            f"[{index:02d}/{len(cases):02d}] "
            f"{case.case_id:02d} {case.title:<44} {status}"
        )

        if case.hint:
            print(f"      pista: {case.hint}")

        if show_content and changed:
            print("      novo conteúdo:")
            for line in new_content.rstrip("\n").splitlines():
                print(f"        {line}")

        if changed:
            updates.append(
                ProposedUpdate(
                    case=case,
                    expected_path=expected_path,
                    old_content=old_content,
                    new_content=new_content,
                )
            )

    return updates, errors


def create_backup(
    updates: list[ProposedUpdate],
    project_root: Path,
) -> Path:
    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")

    backup_root = (
        project_root
        / "backup-gabaritos-invalidos"
        / timestamp
    )

    for update in updates:
        case_backup_dir = (
            backup_root
            / update.case.directory.name
        )

        case_backup_dir.mkdir(
            parents=True,
            exist_ok=True,
        )

        backup_path = (
            case_backup_dir
            / "ast.esperada.txt"
        )

        shutil.copy2(
            update.expected_path,
            backup_path,
        )

    return backup_root


def apply_updates(
    updates: list[ProposedUpdate],
) -> None:
    for update in updates:
        # Escrita simples, com LF e UTF-8, apenas após todos os casos
        # terem passado pela validação.
        update.expected_path.write_text(
            update.new_content,
            encoding="utf-8",
            newline="\n",
        )


def main() -> int:
    args = parse_args()

    test_dir = Path(args.test_dir).resolve()
    parser_path = Path(args.parser_path).resolve()
    project_root = Path.cwd().resolve()

    print("== Regularização dos gabaritos sintaticamente inválidos ==")
    print(f"Parser:       {parser_path}")
    print(f"Testes:       {test_dir}")
    print(f"Modo:         {'APLICAR' if args.apply else 'PRÉ-VISUALIZAÇÃO'}")
    print(f"Timeout:      {args.timeout:g}s por caso")
    print()

    try:
        cases = load_rejected_cases(test_dir)
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        print(f"ERRO ao carregar manifesto: {exc}", file=sys.stderr)
        return 2

    structure_errors = validate_project_structure(
        test_dir,
        parser_path,
        cases,
    )

    if structure_errors:
        print("ERRO: estrutura do projeto inválida.")
        for error in structure_errors:
            print(f"  - {error}")
        return 2

    parser_ok, parser_error = preflight_parser(parser_path)

    if not parser_ok:
        print("ERRO: parser.py não passou pela verificação sintática.")
        print(parser_error)
        return 2

    print(f"Casos REJEITADO encontrados no manifesto: {len(cases)}")
    print("Verificação sintática do parser.py: OK")
    print("-" * 92)

    updates, validation_errors = collect_updates(
        parser_path,
        cases,
        args.timeout,
        args.show_content,
    )

    print()
    print("=" * 92)
    print("Resumo")
    print("------")
    print(f"Casos REJEITADO encontrados:     {len(cases)}")
    print(f"Casos validados:                 {len(cases) - len(validation_errors)}")
    print(f"Gabaritos que precisam alteração:{len(updates):>5}")
    print(f"Falhas de validação:              {len(validation_errors)}")
    print("=" * 92)

    if validation_errors:
        print()
        print(
            "ABORTADO: nenhum gabarito foi alterado porque existem "
            "casos que não passaram pela validação."
        )
        return 1

    if not updates:
        print()
        print(
            "Nenhuma alteração necessária: todos os gabaritos inválidos "
            "já estão regularizados."
        )
        return 0

    if not args.apply:
        print()
        print(
            "PRÉ-VISUALIZAÇÃO CONCLUÍDA: nenhum arquivo foi alterado."
        )
        print()
        print("Para aplicar as alterações:")
        print(
            f"  python3 {Path(sys.argv[0]).name} "
            f"\"{args.test_dir}\" \"{args.parser_path}\" --apply"
        )
        return 0

    backup_root = create_backup(
        updates,
        project_root,
    )

    apply_updates(updates)

    print()
    print(f"Backup criado em: {backup_root}")
    print(f"Gabaritos atualizados: {len(updates)}")
    print()
    print("REGULARIZAÇÃO CONCLUÍDA COM SUCESSO.")
    print()
    print("Próxima validação sugerida:")
    print(
        "  KEEP_TMP=1 bash testar_parser_python.sh "
        "./testes-parser-50 ./parser.py"
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
