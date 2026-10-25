# Histórico — Revisão de status, relatórios e evidências (24/10/2026)

Esta revisão partiu do ZIP **Compilador MINIC(2).zip** e manteve os
componentes do compilador e os gabaritos de teste sem alterações.

## Alterações documentais

- Etapa 1: status IMPLEMENTADA E VALIDADA; 18/18 em cada linguagem.
- Etapa 2: status IMPLEMENTADA E VALIDADA; 50/50 em cada linguagem.
- Etapa 3: status IMPLEMENTADA E VALIDADA; 20/20 em cada linguagem.
- Etapa 4: status IMPLEMENTADA COM LIMITAÇÕES; 30/30 complementar,
  0/30 nos testadores oficiais; -O1 parcial.
- Etapa 3: corrigida referência incorreta a CRLF; a suíte usa LF e
  não tem quebra de linha final.
- Etapa 1: substituídas solicitações de captura de tela por referências
  aos arquivos de evidência.
- Eliminada a recomendação de publicação por plataforma externa da
  auditoria histórica; o foco da documentação é o compilador.

## Scripts oficiais e referências

- Os 16 arquivos .sh seguem byte a byte idênticos aos do ZIP recebido.
- Os 315 arquivos de gabarito explicitamente identificados foram preservados.
- Os testadores de IR permanecem dentro de minic-ir-test-cases/.
- O testador complementar de IR não substitui os testadores oficiais.

## Evidências e ambiente

- Incorporado resumo de proveniência dos resultados fornecidos em 24/10/2026.
- Preservados registros anteriores de validação, identificados pelo ambiente.
- Nova execução do scanner C foi registrada com GCC verificado em Linux;
  não é evidência de compilação no Git Bash do aluno.
- Adicionado gerar_evidencias.py, que permite regenerar registros dos
  testadores oficiais e complementares sem editar qualquer .sh.
- Executáveis, __pycache__ e output.jsonl foram omitidos do ZIP, pois
  são artefatos regeneráveis e podem induzir falso positivo caso gcc falhe.

## Pendência técnica da Etapa 4

Os scripts oficiais de IR chamam minic sem a opção --ir e fazem comparação
literal com textos de referência descritivos. O gerador TAC tem validação
complementar de 30/30, mas permanece reprovado nos scripts originais
(0/30). Não houve ajuste artificial de saída, gabaritos ou testadores.
