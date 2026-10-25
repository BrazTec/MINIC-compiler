# Revisão de equivalência -O1 — 24/10/2026

## Problema observado

Na amostra O01, as versões Python e C produziam IR sem otimização equivalente; no modo -O1, C conservava instruções temporárias `t1 = 5` e `t2 = 20` sem uso.

## Correção

- `ir.c`: acrescentada DCE conservadora de temporários, com análise de referências e repetição até ponto fixo. Foram preservadas instruções com efeitos e o final de linha do formato TAC.
- `ir.c`: alinhamento com Python da substituição de condições de desvio e índices de `load`/`store` após propagação de fatos.
- `semantic.c`: diagnóstico SEM015 detalha os tipos dos operandos, como o Python.
- `minic.c`: o diagnóstico sintático no modo `--ir` inclui o token encontrado, como o Python.
- `validar_equivalencia_ir.py`: novo testador complementar que compila o C e compara os 30 casos em -O0 e -O1. Não modifica nenhum testador oficial.

## Compatibilidade

Nenhum `.sh` ou gabarito oficial foi modificado. A interface sem `--ir` foi preservada. O pipeline C ainda compila `minic.c` como unidade única exigida pela suíte.

Aprovação na validação complementar não equivale a aprovação nos scripts oficiais de IR, que seguem uma comparação textual incompatível com o README da suíte.
