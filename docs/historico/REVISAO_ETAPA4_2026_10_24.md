# Histórico da revisão — Etapa 4 e organização

## Objetivos

1. Respeitar os ambientes Bash/MSYS2: GCC deve estar no PATH (o erro `gcc: command not found` não demanda mudança no script).
2. Preservar integralmente os 16 `.sh` oficiais do professor e todos os gabaritos IR.
3. Manter os módulos em Python e C equivalentes no pipeline integrado.
4. Implementar TAC `-O0`, um subconjunto conservador de `-O1` e testes independentes.
5. Consolidar relatórios, evidências e históricos em `docs/`.

## Código modificado

- `parser.py` / `parser.c`: `for` por dessugaring e `break` em `semantic_mode`.
- `semantic.py` / `semantic.c`: intrínseca `print`, checagem de `break`, expressão aritmética e dimensão de vetor.
- `minic.py` / `minic.c`: opção `--ir`, modos `-O0/-O1`, códigos 2/3/4 no modo IR, preservando modo semântico legado.
- NOVOS: `ir.py`, `ir.c`, `validar_ir_independente.py`.

## Documentação/organização

- READMEs principais atualizados para refletir o pipeline real, parser descendente e status parcial de otimização.
- Arquivos históricos `backup-gabaritos-invalidos/` concentrados em `docs/historico/gabaritos_etapa2_originais/`, não descartados.
- Binários e caches regeneráveis retirados da cópia de entrega, e `.gitignore` atualizado.
- Nenhum `.sh` oficial foi alterado. Nenhum gabarito da suíte IR foi alterado.
- O arquivo de teste Python removido pelo usuário não foi recriado.

## Evidências e limites

- Testes oficiais Etapas 1–3: resultados completos em `docs/evidencia/`.
- Etapa 4: `validar_ir_independente.py` com 30 casos aprovados nas verificações de rejeição, TAC e equivalência observável.
- Testadores oficiais IR: 0/30 em Python e 0/30 em C, pois não passam flags e comparam literal de gabaritos narrativos (contraditório com README da suíte). Divergência preservada e documentada; não há manipulação de entradas ou resultados.
- O1 não implementa todas as otimizações-alvo. Ver `docs/05_IR_IMPLEMENTACAO_E_LIMITACOES.md`.
