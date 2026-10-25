# Revisão da Etapa 4 — 25/10/2026

## Escopo

Adicionados CFG + eliminação de instruções inalcançáveis (O07/O10),
vivacidade e DCE de variáveis/temporários (O02–O04), CSE por bloco básico
(O05), hoisting conservador de invariantes (O08), simplificação de cópias e
constantes (O01/O06), e redução de força `i * 2 -> i + i` (O09).

Python: `testes-scanner-minic_codes/ir.py`.
C: `testes-scanner-minic_codes/ir.c` inclui `ir_passes.c`.
Os fontes também foram sincronizados à pasta de compatibilidade.
Os arquivos `.sh` e os gabaritos do professor não foram alterados.

## Verificações executadas no ambiente de desenvolvimento

- `validar_otimizacoes_o01_o10.py`: 10/10 casos com predicados de
  otimização, mais CSE simbólica Python/C;
- `validar_equivalencia_ir.py`: 60/60 (Python/C, modos -O0/-O1);
- `validar_ir_independente.py`: 30/30 (execução e efeito observável).

Os .sh oficiais da Etapa 4 mantêm a divergência com seu README e continuam
registrados separadamente. Ver resultados completos em `docs/evidencia/`.

## Limites

O06: caso atendido sem implementar reaching definitions genérico.
O08: somente loops estruturados com expressões puras/operandos invariantes.
O09: `x * 2 -> x + x`, não atualização incremental de indução.
CSE: local por bloco; DCE: conservadora quanto à memória e efeitos.
A suíte não equivale a prova geral de correção de compiladores.
