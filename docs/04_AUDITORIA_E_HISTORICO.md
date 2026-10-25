> **Registro histórico (auditoria anterior à implementação de IR).** O texto abaixo descreve o estado do projeto antes da Etapa 4. O estado atual, implementação e resultados encontram-se em `README.md`, `docs/05_IR_IMPLEMENTACAO_E_LIMITACOES.md` e `docs/historico/REVISAO_ETAPA4_2026_10_24.md`.

# 04 — AUDITORIA E HISTÓRICO DE ALTERAÇÕES

## Escopo desta revisão

Fonte: ZIP `Compilador MINIC.zip` enviado pelo usuário, em outubro de 2026. Objetivo: alinhar documentos ao código real e mapear a Etapa 4, **sem alterar qualquer `.sh` fornecido pelo professor**.

### Alterações efetuadas nesta revisão

- Reescrita do README principal para identificar um único pipeline C/Python com entrada oficial em `minic.py`/`minic.c`.
- Declaração explícita de parser **descendente recursivo**. Não se encontrou afirmação textual de parser ascendente nos README/relatórios TXT/DOCX inspecionados; foi reforçada a nomenclatura correta, sem rebatizar a implementação.
- Documentação explicativa da SDD existente, relacionando produções e atributos a rotinas reais.
- Mapeamento dos 30 casos de IR e de divergências entre seu README e testadores.
- Novo `COMO_TESTAR.md` e verificação independente de integridade por SHA-256.
- Atualização apenas dos READMEs das duas pastas de compatibilidade, indicando qual é a canônica.

### Arquivos intocados

- **Todos os 16 arquivos `.sh`** (duas cópias de oito scripts), comprovados por SHA-256.
- Todo código-fonte `.py` e `.c`.
- Todos os arquivos de entrada `.c`, `.minic` e `.jsonl` usados nas suítes.
- Todos os gabaritos `.txt`, `.expected*` e `.gabarito`.
- Relatórios acadêmicos existentes `.txt` e `.docx`.
- Imagens de evidência, backups históricos e arquivos auxiliares já enviados.

### Estrutura duplicada

`testes-scanner-minic_codes` foi declarado diretório canônico. `testes-scanner-minic` permanece como cópia histórica compatível. No ZIP de entrada, 344 arquivos relativos eram comuns às duas árvores, dos quais 13 tinham conteúdo diferente (majoritariamente fixtures léxicos, prints e saída auxiliar). Os oito módulos centrais de Python e C e os scripts oficiais eram idênticos entre as duas árvores.

### Verificações realizadas na auditoria

- Semântico Python (20 casos, pasta canônica): **20/20**.
- Semântico C (20 casos, pasta canônica): **20/20**.
- Sintático C (50 casos): **50/50** aprovados; o contador de tipo de erro pode depender do locale.
- Scanner Python: **18/18** no primeiro testador executado.
- A verificação Python sintática não foi concluída nesta sessão de auditoria; executar localmente com `chmod +x parser.py` e ambiente UTF-8. Não confundir ausência de validação nesta sessão com reprovação do código.
- IR: **ainda não implementado**; nenhuma reivindicação de 30/30.

### Observações para a próxima entrega

A avaliação acadêmica exige equivalência de implementações e correção além dos fixtures. É necessário ampliar testes negativos semânticos (ex.: chamada de objeto não invocável), verificar funções/escopos, acrescentar casos de fronteira e relacionar o código às regras SDD. Esses testes complementares não devem substituir os oficiais.

**Recomendação de validação:** conferir `python3 verificar_scripts_oficiais.py`, executar todos os testadores com GCC disponível e manter os arquivos oficiais inalterados.
