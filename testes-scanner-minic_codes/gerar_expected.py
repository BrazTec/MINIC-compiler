import json
from pathlib import Path

from scanner import Scanner


ROOT = Path(__file__).resolve().parent
VALID_CASES = ROOT / "casos-validos"


def main():
    arquivos = sorted(VALID_CASES.glob("*.minic"))

    if not arquivos:
        print("ERRO: nenhum arquivo .minic encontrado em casos-validos.")
        return 1

    total = 0

    for arquivo in arquivos:
        source = arquivo.read_text(encoding="utf-8")

        scanner = Scanner(source)
        resultado = scanner.scan()

        erros = [
            item
            for item in resultado
            if "error" in item
        ]

        tokens = [
            item
            for item in resultado
            if "token" in item
        ]

        print("=" * 65)
        print(f"Caso: {arquivo.name}")

        # Caso válido não deveria produzir nenhum erro léxico.
        if erros:
            print("FALHA: foram encontrados erros léxicos:")

            for erro in erros:
                print(
                    json.dumps(
                        erro,
                        ensure_ascii=False
                    )
                )

            print(
                "O arquivo expected NÃO foi gerado "
                "para evitar criar um gabarito incorreto."
            )

            continue

        expected = arquivo.with_suffix(
            ".expected.jsonl"
        )

        with expected.open(
            "w",
            encoding="utf-8",
            newline="\n"
        ) as output:
            for token in tokens:
                output.write(
                    json.dumps(
                        token,
                        ensure_ascii=False,
                        separators=(",", ":")
                    )
                )
                output.write("\n")

        print(f"OK: {len(tokens)} token(s)")
        print(f"Gerado: {expected.name}")

        # Verificação básica do EOF
        if not tokens:
            print("AVISO: nenhum token produzido.")
        elif tokens[-1]["token"] != "EOF":
            print("AVISO: último token não é EOF.")
        else:
            eof = tokens[-1]

            print(
                f"EOF: linha {eof['line']}, "
                f"coluna {eof['column']}"
            )

        total += 1

    print("=" * 65)
    print(
        f"Concluído: {total} arquivo(s) expected "
        "gerado(s)."
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())