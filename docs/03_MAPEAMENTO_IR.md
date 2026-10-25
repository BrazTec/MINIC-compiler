# 03 — ETAPA 4: MAPEAMENTO DE CÓDIGO INTERMEDIÁRIO (IR)

**Status de 25/10/2026:** geração TAC em `-O0` e otimizações em `-O1` implementadas em Python e C; os dez casos O01–O10 passaram no verificador específico, com limites de generalização detalhados em `05_IR_IMPLEMENTACAO_E_LIMITACOES.md`. **Todos os scripts `.sh` e arquivos de gabarito permanecem intactos.**

## 1. Material encontrado

Em ambas as pastas históricas existe `minic-ir-test-cases/` com **30 casos**:

- `01-validos-sem-otimizacao/P01–P10`: 10 programas válidos com referência TAC `-O0`.
- `02-erros/E01–E10`: 10 programas rejeitados, categorias e critérios de diagnóstico.
- `03-otimizacoes/O01–O10`: 10 programas válidos com objetivos de otimização `-O1`.

Também existem `test_ir_python.sh`, `test_ir_c.sh` e um README na própria suíte, fornecidos com o pacote.

## 2. Contradição constatada entre README e `.sh`

O README da suíte afirma que os gabaritos válidos são referências **estruturais**, e não uma exigência de equivalência textual byte a byte; nomes de temporários/rótulos e formas de indexação podem variar. Mas os DOIS scripts oficiais **utilizam `cmp -s`** sobre a saída filtrada e o **texto completo** de cada `.txt` (incluindo explicações do gabarito). Os scripts **não passam** `--ir`, `-O0` ou `-O1`: invocam apenas `python minic.py arquivo.c` e o executável C com um argumento fonte. Para as entradas inválidas, os `.txt` trazem categorias e critérios descritivos, não apenas mensagens a serem emitidas.

Logo, produzir um TAC semanticamente correto **não é suficiente** para passar nesses `.sh` sem que se defina um protocolo de saída que os atenda. Não adaptar os gabaritos nem os `.sh` para esconder a discrepância. Não alegar aprovação de IR enquanto não houver solução técnica verificável ou esclarecimento acadêmico sobre o contrato.

## 3. Interface implementada — compatibilidade da Etapa 3 preservada

```text
minic.py arquivo.c          # comportamento histórico da Etapa 3
minic.py --ir -O0 arquivo.c # TAC não otimizada
minic.py --ir -O1 arquivo.c # TAC otimizada
./minic --ir -O0 arquivo.c   # mesmo contrato na versão C
./minic --ir -O1 arquivo.c
```

Essas opções estão implementadas **nas duas linguagens**. Os `.sh` oficiais de IR continuam chamando o driver sem as flags e não podem servir como verificação isolada da TAC `-O0`/`-O1`. Os scripts permanecem intactos; as execuções por caso e por modo são feitas em validadores complementares independentes.

## 4. Modelo TAC efetivamente utilizado

```text
func nome
  t1 = a + b
  t2 = a < b
  ifFalse t2 goto Lfim
  param x
  t3 = call f, 1
  t4 = load vetor[i]
  store vetor[i], t4
  print t3
  return t3
Lfim:
endfunc
```

A implementação representa atribuição, aritmética, comparações, desvios, rótulos, parâmetros, chamadas, retornos, `print` e operações de vetores (`alloc`, `load`, `store`) no subconjunto coberto pelos testes. A forma de serialização não é exigida byte a byte pelo README da suíte. Temporários e rótulos determinísticos simplificam comparação C ↔ Python, mas não são critérios textuais universais.

## 5. Matriz dos 30 casos

### Válidos `-O0`

| Caso | Cobertura |
|---|---|
| P01 | inicialização, soma e `print` |
| P02 | precedência aritmética (multiplicação antes da subtração) |
| P03 | função com parâmetros, `call` e `return` |
| P04 | `if`/`else` e saltos por rótulo |
| P05 | `while`, incremento e laço |
| P06 | **`for`** e fluxo de repetição; reconhecido no pipeline integrado |
| P07 | indexação, acesso e atribuição em vetor (`load`/`store`) |
| P08 | booleanos e `&&`, curto-circuito ou operação equivalente |
| P09 | sombreamento de variáveis e nomes internos sem colisão |
| P10 | recursão, chamada e retorno |

### Erros

| Caso | Exigência adicional |
|---|---|
| E01 | símbolo não declarado |
| E02 | declaração duplicada no mesmo escopo |
| E03 | `bool → int` implícito não permitido |
| E04 | operação `bool + int` ilegal |
| E05 | chamada com aridade incorreta |
| E06 | retorno incompatível |
| E07 | **`break` fora de laço**; reconhecimento e rejeição contextual implementados |
| E08 | índice não inteiro |
| E09 | erro sintático, declaração sem `;` |
| E10 | erro léxico, símbolo desconhecido `@` |

Os códigos de rejeição foram implementados em modo `--ir`: **4** para rejeição semântica (E01–E08), **3** para sintática (E09) e **2** para léxica (E10), mantendo separado o contrato semântico anterior. Os `.sh` de IR não avaliam diretamente essa classificação.

### Otimizações `-O1`

| Caso | Transformação-alvo |
|---|---|
| O01 | propagação e dobra de constantes |
| O02 | simplificação algébrica, propagação de cópias |
| O03 | eliminação de cópias |
| O04 | eliminação de atribuição morta |
| O05 | eliminação local de subexpressões comuns |
| O06 | propagação global de constantes / reaching definitions |
| O07 | condição constante e remoção de ramo inalcançável |
| O08 | movimento de expressão invariável de laço |
| O09 | redução de força, **opcional segundo README**; exigir equivalência |
| O10 | eliminação de código morto/inalcançável |

## 6. Integrações e validação realizadas

O pipeline foi estendido com `for` e rejeição de `break` fora de laços no modo integrado. O gerador emite TAC para programas aceitos e diagnósticos sem TAC para os inválidos. A versão `-O1` executa propagação e dobra de constantes, simplificações algébricas, DCE, CSE local, remoção de blocos inalcançáveis, LICM conservadora e redução de força simples.

| Evidência de 25/10/2026 (Windows) | Resultado |
|---|---|
| `validar_ir_independente.py` | 30/30 |
| `validar_equivalencia_ir.py` | 60/60 (30 em `-O0`, 30 em `-O1`) |
| `validar_ir_por_caso.py` | 30/30, com logs individuais |
| `validar_otimizacoes_o01_o10.py` | 10/10, mais CSE simbólica Python/C |
| `test_ir_python.sh` e `test_ir_c.sh` | 0/30 **cada**, devido ao contrato textual descrito na seção 2 |

**Limitação fundamental do O06:** o caso demonstra propagação e dobra, **não** um resolvedor geral de *reaching definitions* interblocos. O09 foi implementado em forma simples, embora seja opcional no README da suíte. Outras limitações são detalhadas em `05_IR_IMPLEMENTACAO_E_LIMITACOES.md`.

## 7. Critério de conclusão

A geração de TAC e as transformações esperadas nos casos de referência foram verificadas por procedimentos complementares. Esses resultados **não equivalem** à aprovação dos dois `.sh` oficiais. A divergência entre comparação estrutural prevista no README e comparação literal aplicada pelos scripts precisa ser explicitada na avaliação docente. Os arquivos oficiais permanecem intactos.
