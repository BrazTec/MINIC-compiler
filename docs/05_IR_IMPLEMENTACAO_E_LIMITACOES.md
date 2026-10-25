# 05 — GERAÇÃO DE IR (ETAPA 4): IMPLEMENTAÇÃO E LIMITAÇÕES

## Implementação entregue

- `ir.py`: instruções TAC como registros estruturados (`Instruction`), geração a partir da AST, temporários e rótulos, escopos/nomes distintos, chamadas com parâmetros, `load`/`store`, saltos condicionais, laços, `return`, `print` e curto-circuito booleano. Otimização opcional `-O1` em passagem separada sobre as instruções estruturadas.
- `ir.c` e `ir_passes.c`: geração TAC C e passes -O1 sobre a TAC textual, incluindo CFG, DCE geral intraprocedural, CSE local, LICM conservadora e redução de força.
- `minic.py`, `minic.c`: controladores integrados scanner → parser → semântico → IR. O IR é gerado apenas na ausência de erros semânticos. Sem `--ir`, saída histórica da Etapa 3 foi preservada.
- `parser.py`, `parser.c`: em `semantic_mode`, suportam `for` (dessugaring para bloco + `while`) e `break`; o modo de parser isolado mantém o comportamento anterior.
- `semantic.py`, `semantic.c`: reconhecimento da intrínseca `print`, rejeição de `break` fora de laço, aritmética entre tipos incompatíveis e dimensão de vetor não inteira.

## Interface

```text
python3 minic.py --ir -O0 arquivo.c
python3 minic.py --ir -O1 arquivo.c
./minic --ir -O0 arquivo.c
./minic --ir -O1 arquivo.c
```

Saída válida: instruções TAC de três endereços e status 0. Rejeição em modo IR: diagnóstico sem TAC em stdout, retorno 2 (léxico), 3 (sintático), 4 (semântico). Chamadas recursivas e operações em vetores possuem instruções dedicadas. Nomes temporários e rótulos são determinísticos por função.

## Testes adicionais

`validar_ir_independente.py` é um **testador complementar**, não um substituto do professor. Ele executa 30 programas em ambas as linguagens, verifica os códigos de rejeição, compara o texto TAC `-O0`, interpreta TAC e valida efeitos observáveis entre `-O0`/`-O1`, e confronta os resultados com execução nativa do fonte C válido. Limita-se ao subconjunto de operações exercitado nos 30 casos.

## Otimizações `-O1`: situação revisada (25/10/2026)

O otimizador foi estendido com passes de controle de fluxo e análise de
vivacidade em ambas as linguagens. A implementação C agora inclui o arquivo
`ir_passes.c`, inserido por `ir.c` na compilação de `minic.c` (não requer
vinculação separada).

| Caso-alvo | Implementação observada |
|---|---|
| O01 constantes | Dobra e propagação; `print 20` |
| O02 identidades | Elimina `+ 0`/`* 1` e definições não utilizadas |
| O03 cópias | Propaga e remove cópias sem observadores |
| O04 definições mortas | DCE com vivacidade em CFG; remove `temporario = 27` |
| O05 CSE | CSE local com invalidação de operandos; teste simbólico prova reutilização de `a*b` |
| O06 constantes | Produz `print 10`; não há solver global geral de reaching definitions |
| O07 controle | Simplificação de desvio constante e remoção de ramo inalcançável |
| O08 LICM | Move expressão pura invariante antes do laço; no caso é dobrada para `42` |
| O09 força | Reescreve multiplicação por dois como soma `i + i`; o README a considera opcional |
| O10 inalcançável | Elimina `print 99` após `return 4` |

`validar_otimizacoes_o01_o10.py` checa cada propriedade em Python e C nos
modos `-O0`/`-O1`, incluindo efeitos observáveis por interpretador da TAC e
caso simbólico adicional para CSE. O resultado local de desenvolvimento foi
10/10 e CSE simbólica em ambas as linguagens.

**Limites**: o conjunto demonstra as transformações nos casos O01–O10; não
fornece uma solução completa de propagação interblocos/reaching definitions,
LICM especulativa ou interprocedural, DCE com análise de alias nem SSA.
A execução equivalente isolada não é suficiente para comprovar uma
transformação — por isso a suíte estrutural foi acrescentada.

### Equivalência textual verificada para a suíte

Após a sincronização da DCE de temporários e do tratamento de desvios e indexação, `validar_equivalencia_ir.py` compara **30 programas em cada modo** (`-O0` e `-O1`) entre C e Python, incluindo stdout, stderr e status (com normalização exclusiva de LF/CRLF). Esta comparação demonstra equivalência das saídas no conjunto verificado; não substitui os testadores oficiais nem prova a implementação de todas as otimizações-alvo.

## Divergência entre README da suíte e testadores oficiais

`minic-ir-test-cases/README.md` afirma que os gabaritos IR são representações canônicas **não byte a byte**, com temporários e rótulos livres. Os scripts oficiais `test_ir_python.sh` e `test_ir_c.sh` chamam os drivers sem `--ir`/`-O1` e usam `cmp` após filtragem de mensagens contra `.txt` que contêm descrições de sucesso/rejeição e critérios didáticos. Não foram alterados os `.sh`, os `.txt`, nem introduzido comportamento para emitir o próprio gabarito como se fosse TAC. É necessária definição institucional do protocolo de teste automatizado ou uma validação estrutural complementar.

## Limitações técnicas

O IR não é SSA, não possui registrador físico nem backend para assembly. Os scripts C incluem módulos para atender à compilação em uma unidade. Otimizadores realizam apenas transformações locais conservadoras. A representação `for` foi dessugarada e `break` fora de laço é rejeitado; semânticas mais amplas (`continue`, `for` com declaração em inicialização, efeitos de `read`) requerem testes específicos. Testes existentes não provam correção universal.
