# COMO TESTAR — MINIC

**Regra de integridade:** nenhum `.sh` oficial e nenhum gabarito deve ser modificado. Todos os comandos abaixo são executados externamente aos scripts. Pasta canônica: `testes-scanner-minic_codes/`.

## 1. Preparação

Abra MSYS2 UCRT64 ou Git Bash. No Windows, se necessário:

```bash
export PATH="/c/msys64/ucrt64/bin:$PATH"
export PYTHONIOENCODING=utf-8
export PYTHONUTF8=1
export LC_ALL=C.UTF-8
export LANG=C.UTF-8
command -v gcc
gcc --version
```

Esse caminho corresponde à instalação MSYS2 utilizada na validação; adapte se o GCC estiver instalado em outro local. **GCC encontrado no PATH** não dispensa conferir a compilação: binários antigos de Linux ou de outras máquinas podem produzir resultados enganosos. Os testes C precisam recompilar localmente os fontes.

Entre na pasta principal de fontes:

```bash
cd testes-scanner-minic_codes
```

## 2. Etapa 1 — Scanner

```bash
python3 check_fixtures.py
python3 test_scanner_python.py
python3 test_scanner_c.py

PASS=0 FAIL=0 WARN=0 bash test_scanner_python.sh
PASS=0 FAIL=0 WARN=0 bash test_scanner_c.sh
```

**Por que inicializar os contadores?** Os scripts oficiais utilizam as variáveis `PASS`, `FAIL` e `WARN` sem inicializá-las consistentemente. Passar valores por ambiente evita avisos `printf: : invalid number` e saída não nula em testes cujas 18 comparações estão corretas. O procedimento **não altera os scripts**. Um executável incompatível (mensagem `Exec format error`) deve ser removido e recompilado no Windows, sem alterar o `.sh`.

## 3. Etapa 2 — Parser descendente recursivo

```bash
chmod +x parser.py
bash testar_parser_python.sh ./testes-parser-50 ./parser.py
bash testar_parser_c.sh ./testes-parser-50 ./parser.c
```

São 50 casos em cada linguagem, incluindo 25 erros sintáticos intencionais. O parser **não** é ascendente.

## 4. Etapa 3 — Semântico

```bash
bash testes_semanticos_py.sh minic.py ./minic-testes-semanticos
bash testes_semanticos_c.sh minic.c ./minic-testes-semanticos
```

São 20 casos em cada linguagem; o driver sem `--ir` mantém o contrato semântico anterior.

## 5. Etapa 4 — IR sem otimização / com otimização

```bash
# Python (-O0 e -O1)
python3 minic.py --ir -O0 minic-ir-test-cases/testes/03-otimizacoes/O01/O01.c
python3 minic.py --ir -O1 minic-ir-test-cases/testes/03-otimizacoes/O01/O01.c

# C (-O0 e -O1)
gcc -Wall -Wextra -std=c11 minic.c -o minic
./minic --ir -O0 minic-ir-test-cases/testes/03-otimizacoes/O01/O01.c
./minic --ir -O1 minic-ir-test-cases/testes/03-otimizacoes/O01/O01.c
```

### Validação complementar conforme o README da suíte

```bash
python3 validar_ir_independente.py             # 30 casos, execução de TAC
python3 validar_equivalencia_ir.py             # 60 comparações Python/C
python3 validar_ir_por_caso.py                # P01–P10, E01–E10 e O01–O10
python3 validar_ir_por_caso.py --case O01     # um caso isolado
python3 validar_otimizacoes_o01_o10.py        # 10 transformações + CSE simbólica
```

Os resultados por caso são armazenados em `docs/evidencia/04_ir_por_caso/` a partir da raiz. A igualdade de resultados observáveis não comprova automaticamente otimização; por isso existe também `validar_otimizacoes_o01_o10.py`.

### Testadores `.sh` oficiais de IR: executar e registrar, sem modificar

```bash
bash minic-ir-test-cases/test_ir_python.sh minic.py ./minic-ir-test-cases/testes
bash minic-ir-test-cases/test_ir_c.sh minic.c ./minic-ir-test-cases/testes
```

**Resultado observado nesta versão:** `0/30` em cada script (`exit 1`). Os scripts não passam `--ir`/`-O1` e comparam texto literal de gabaritos descritivos, enquanto o README da suíte orienta comparação estrutural e semântica. O resultado não pode ser reclassificado como aprovado. Os verificadores complementares usam o contrato descrito no README e são registrados **separadamente**.

## 6. Integridade e geração das evidências completas

Na **raiz `Compilador MINIC/`** (se estiver em `testes-scanner-minic_codes/`, execute `cd ..`):

```bash
python3 verificar_scripts_oficiais.py
python3 gerar_evidencias.py
```

`gerar_evidencias.py` registra compilação, comandos, ambiente, stdout/stderr e códigos de saída em `docs/evidencia/`. Inclui as quatro etapas, testes IR por caso, equivalência entre linguagens e as otimizações O01–O10. **A saída agregada pode indicar duas ações não aprovadas**, pois os `.sh` oficiais de IR têm contrato diferente do README. Consultar sempre `docs/evidencia/00_RESUMO_DE_VALIDACAO.txt`, os relatórios individuais e `docs/08_SINTESE_FINAL_E_CRITERIOS_DE_AVALIACAO.md`.
