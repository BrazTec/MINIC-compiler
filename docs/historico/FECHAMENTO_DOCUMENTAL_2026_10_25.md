# Fechamento documental — 25/10/2026

Origem: ZIP recebido como `Compilador MINIC(6).zip`. Revisão **somente documental e de empacotamento**, sem alterações de código-fonte, testadores `.sh`, entradas ou gabaritos.

## Atualizações

- `README.md`: retiradas declarações obsoletas segundo as quais CSE, LICM, redução de força e código inalcançável não estavam implementados; status e evidências consolidados.
- `COMO_TESTAR.md`: comandos `-O0`/`-O1`, execução isolada, ressalva dos scripts oficiais, contadores de scanner por ambiente e prevenção de binários antigos.
- `docs/03_MAPEAMENTO_IR.md`: removidas instruções de implementação futura já realizadas; matriz preservada; resultados e limites atualizados.
- `docs/08_SINTESE_FINAL_E_CRITERIOS_DE_AVALIACAO.md`: novo índice dos resultados e dos critérios de avaliação, incluindo falha real dos dois scripts IR.
- `docs/relatorio/`: acrescentado registro da evidência final Windows aos quatro relatórios; sem modificar os resultados existentes.
- Remoção **apenas do pacote de entrega** dos executáveis `.exe`, `__pycache__`, `.pyc` e `output.jsonl`, que são artefatos regeneráveis. Registros em `docs/evidencia/` permaneceram preservados.

## Integridade

`16/16` scripts `.sh` originais sem modificação. Os gabaritos e os fontes permaneceram idênticos byte a byte aos recebidos. Os dois `.sh` IR seguem com `0/30` em cada, e os critérios técnicos do README da suíte não foram trocados por testes de igualdade textual forçada.

O conteúdo de `docs/historico/` descreve estados anteriores do projeto e deve ser lido como histórico, não como situação atual. O índice vigente de status é `README.md` e `docs/08_SINTESE_FINAL_E_CRITERIOS_DE_AVALIACAO.md`.
