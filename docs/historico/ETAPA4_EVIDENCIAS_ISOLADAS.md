# Histórico — Ampliação das evidências de IR

- Adicionado `validar_ir_por_caso.py`, sem alterar scripts oficiais ou gabaritos.
- `gerar_evidencias.py` passou a executar o validador isolado e o comparador
  de equivalência Python/C, além da suíte independente preexistente.
- Foram criados relatórios separados P01-P10 (`-O0`), E01-E10 (categoria)
  e O01-O10 (`-O0`/`-O1`) em `docs/evidencia/04_ir_por_caso/`.
- Os resultados dos dois `.sh` de IR oficiais permanecem documentados, inclusive
  suas falhas por confronto literal, em vez de serem substituídos pelos testes complementares.
- Nenhuma mudança em `.sh`, `.c`, `minic.py`, `ir.py`, `semantic.py` ou gabaritos.
