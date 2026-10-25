# MINIC — Compilador educacional (Python e C)

**Disciplina:** Compiladores  
**Escopo:** análise léxica, análise sintática, análise semântica e produção/otimização de código intermediário (IR/TAC).  
**Referência para o status:** evidências do ambiente Windows em **25/10/2026, 02:45:20 UTC**, em `docs/evidencia/00_RESUMO_DE_VALIDACAO.txt`.

## 1. Pipeline único da implementação

```text
Arquivo-fonte .c / .minic
  ↓
Scanner / analisador léxico → tokens + posições
  ↓
Parser DESCENDENTE RECURSIVO → AST
  ↓
Analisador semântico → tipos, símbolos, escopos e diagnósticos
  ├─ erro: rejeição; não gera TAC
  └─ válido
       ↓
Gerador de IR → TAC de três endereços (-O0)
       ↓
Otimizador conservador → TAC otimizada (-O1)
```

O projeto **não possui parser ascendente (LR/SLR/LALR)**. A SDD está descrita com regras gramaticais e atributos herdados/sintetizados, realizados por visitas à AST; isso não exige parser ascendente.

**Python:** `scanner.py` → `parser.py` → `semantic.py` → `ir.py`; entrada integrada em `minic.py`.  
**C:** scanner incorporado ao `parser.c`, análise em `semantic.c`, TAC em `ir.c` e passes adicionais em `ir_passes.c`; entrada integrada em `minic.c`. O driver C reúne os módulos por inclusão de arquivos `.c` para manter a compilação exigida pelos testadores. Trata-se de **separação lógica**, não de linkagem independente de módulos C.

## 2. Estrutura principal

```text
Compilador MINIC/
├── README.md                         ← visão geral e status
├── COMO_TESTAR.md                    ← comandos completos
├── gerar_evidencias.py               ← bateria e logs por etapa
├── verificar_scripts_oficiais.py    ← SHA-256 dos 16 .sh
├── docs/
│   ├── relatorio/                  ← relatórios das quatro etapas
│   ├── evidencia/                  ← logs e TACs por caso (Windows)
│   ├── historico/                  ← auditorias e versões históricas
│   ├── 01_ARQUITETURA_E_INTEGRACAO.md
│   ├── 02_SDD_ANALISE_SEMANTICA.md
│   ├── 03_MAPEAMENTO_IR.md
│   ├── 05_IR_IMPLEMENTACAO_E_LIMITACOES.md
│   ├── 06_SDD_TRADUCAO_TAC.md
│   ├── 07_VALIDACAO_IR_CASOS_ISOLADOS.md
│   └── 08_SINTESE_FINAL_E_CRITERIOS_DE_AVALIACAO.md
├── testes-scanner-minic_codes/     ← implementação principal, casos e scripts
└── testes-scanner-minic/           ← cópia histórica de compatibilidade
```

## 3. Estado documentado das etapas

| Etapa | Resultado dos testes correspondentes | Status |
|---|---|---|
| **1 — Léxico** | Scanner Python **18/18**, scanner C **18/18** | Implementada e validada na suíte |
| **2 — Sintático** | Parser Python **50/50**, parser C **50/50**; 25 rejeições esperadas por versão | Implementada e validada na suíte |
| **3 — Semântico** | Python **20/20**, C **20/20** | Implementada e validada na suíte |
| **4 — IR/TAC** | Validação funcional **30/30**; casos isolados **30/30**; equivalência Python/C `-O0`/`-O1` **60/60**; otimizações-alvo O01–O10 **10/10** | Implementada e validada nos verificadores complementares, com ressalva na avaliação por `.sh` |

**Ressalva obrigatória da Etapa 4:** `test_ir_python.sh` e `test_ir_c.sh` registraram **0/30 cada (`exit 1`)**. Os resultados não foram reclassificados como aprovação. A incompatibilidade metodológica entre o README da suíte e os scripts é detalhada na seção 5 e no relatório da Etapa 4.

Esses números atestam **os casos verificados**, não uma prova formal para todos os possíveis programas MINIC. A saída equivalente entre Python e C também não é, por si só, prova de correção semântica universal.

## 4. Produção de TAC e otimizações implementadas

A geração de código intermediário só ocorre depois da aceitação semântica. Em `-O0`, a TAC representa atribuições, aritmética, testes, rótulos/desvios, laços (`while` e `for`), vetores, funções, recursão, `print` e `return`. `-O1` acrescenta passes conservadores.

| Casos da suíte | Transformação demonstrada nos testes |
|---|---|
| O01 | propagação e dobra de constantes |
| O02–O03 | simplificação algébrica, propagação e remoção de cópias redundantes |
| O04 | eliminação de atribuições mortas |
| O05 | eliminação local de subexpressões comuns (**CSE**) — também testada com operandos simbólicos |
| O06 | propagação e dobra de constantes no caso-alvo, **sem** análise global geral de *reaching definitions* |
| O07 | condição constante e remoção de ramo inalcançável |
| O08 | movimentação conservadora de expressão invariante de laço (**LICM**) |
| O09 | redução de força `i * 2 → i + i` (opcional na orientação do professor) |
| O10 | eliminação de instruções inalcançáveis após `return` |

**Limites:** CSE é local/conservadora; LICM não é geral nem especulativa; não há análise global completa de *reaching definitions*, SSA, otimização interprocedural universal nem geração de assembly. Consultar `docs/05_IR_IMPLEMENTACAO_E_LIMITACOES.md`.

## 5. Testadores originais e divergência da suíte IR

**Os 16 arquivos `.sh` foram preservados byte a byte.** O verificador `verificar_scripts_oficiais.py` reportou `16/16 — divergências: 0` no ambiente Windows. Entradas e arquivos de referência oficiais não foram alterados nesta revisão documental.

O README em `minic-ir-test-cases/` instrui avaliar IR pela **estrutura, fluxo, operandos, efeitos observáveis e categorias de erro**, permitindo diferenças na representação textual, e sugere `--ir`, `-O0` e `-O1`. Já os testadores `.sh` de IR invocam o driver **sem essas opções** e comparam texto via `cmp -s` contra arquivos que contêm descrições e referências de TAC. Sem `--ir`, o driver mantém a saída histórica da Etapa 3.

Por isso, o `0/30` dos `.sh` **é uma reprovação real nesses testadores**, mas não equivale, isoladamente, a 30 defeitos de geração de TAC. De modo recíproco, os `30/30`, `60/60` e `10/10` complementares **não substituem** a aprovação nos `.sh` nem a avaliação do docente. A divergência requer esclarecimento sobre o método formal de avaliação. Não modificar os testadores/gabaritos nem fazer o compilador reproduzir artificialmente os textos esperados.

## 6. Execução rápida

Abra Git Bash com GCC acessível (por exemplo, MSYS2 UCRT64 instalado no caminho abaixo):

```bash
export PATH="/c/msys64/ucrt64/bin:$PATH"
cd testes-scanner-minic_codes

python3 minic.py --ir -O0 minic-ir-test-cases/testes/03-otimizacoes/O01/O01.c
python3 minic.py --ir -O1 minic-ir-test-cases/testes/03-otimizacoes/O01/O01.c

gcc -Wall -Wextra -std=c11 minic.c -o minic
./minic --ir -O0 minic-ir-test-cases/testes/03-otimizacoes/O01/O01.c
./minic --ir -O1 minic-ir-test-cases/testes/03-otimizacoes/O01/O01.c

python3 validar_ir_independente.py
python3 validar_equivalencia_ir.py
python3 validar_ir_por_caso.py
python3 validar_otimizacoes_o01_o10.py
```

Para reproduzir os resultados dos testadores e salvar **todas** as evidências, na **raiz** do projeto:

```bash
python3 verificar_scripts_oficiais.py
python3 gerar_evidencias.py
```

`gerar_evidencias.py` pode terminar com **código 1** por causa dos dois scripts oficiais de IR; os logs identificam os resultados de cada teste separadamente. Comandos completos e cuidados com contadores dos scanners: `COMO_TESTAR.md`. A evidência final existente foi gerada no Windows; não se devem apresentar novos resultados como se tivessem sido executados nesse mesmo ambiente sem revalidar.
