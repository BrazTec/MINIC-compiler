# 08 — Síntese final de implementação e critérios de avaliação

## 1. Referência de avaliação

Os resultados desta síntese provêm do registro **Windows 11 / Python 3.14.7 / MSYS2 GCC UCRT 16.2.0**, capturado em **25/10/2026 às 02:45:20 UTC**, no arquivo `docs/evidencia/00_RESUMO_DE_VALIDACAO.txt`. Não representam testes de uma versão posterior nem aprovam automaticamente a correção acadêmica.

## 2. Estado por etapa

| Etapa | Técnica aplicada | Suíte / validação | Status documental |
|---|---|---|---|
| 1 — Scanner | Léxico em Python/C | 18/18 em cada | **Implementada e validada nos testes** |
| 2 — Parser | Descendente recursivo; geração da AST | 50/50 em cada; 25 rejeições previstas | **Implementada e validada nos testes** |
| 3 — Semântico | Escopos, símbolos, tipos, funções, vetores, retornos | 20/20 em cada | **Implementada e validada nos testes** |
| 4 — IR | TAC `-O0`; passes conservadores `-O1` | 30/30 funcional; 30/30 isolado; 60/60 equivalência Python/C; 10/10 otimizações-alvo | **Implementada e validada por testes complementares, com ressalva dos oficiais** |

A SDD foi explicitada nos documentos `02_SDD_ANALISE_SEMANTICA.md` e `06_SDD_TRADUCAO_TAC.md`, vinculando regras a funções de visita à AST. Não há parser ascendente.

## 3. Rastreabilidade das otimizações da Etapa 4

| Grupo | Comprovação dos casos | Limites |
|---|---|---|
| O01–O03 | constantes, identidades e cópias | regras locais/conservadoras |
| O04 | remoção de atribuição morta | preservação de possíveis efeitos colaterais |
| O05 | CSE local, inclusive prova simbólica extra | não é CSE global universal |
| O06 | propagação que alcança o resultado esperado | **não** prova *reaching definitions* geral |
| O07 e O10 | poda de ramo/código inalcançável | dependente do CFG reconhecido |
| O08 | LICM em laço estrutural demonstrado | não é LICM universal ou especulativa |
| O09 | transformação simples `i * 2` em `i + i` | otimização opcional na especificação |

Evidências: `04_ir_otimizacoes_o01_o10.txt`, `04_ir_equivalencia_c_python.txt`, `04_ir_validacao_independente.txt` e `04_ir_por_caso/`.

## 4. Divergência explícita: README da suíte de IR × testadores `.sh`

**Texto do README:** recomenda execução com `--ir`, `-O0`/`-O1`, comparação estrutural e de comportamento, normalizando temporários/rótulos, e rejeição pela categoria de erro. A IR de referência **não exige igualdade byte a byte**.

**Comportamento dos dois scripts:** invocação sem flags de IR e comparação via `cmp -s` com arquivos que incluem descrições de referência. Sem `--ir`, o MINIC mantém por compatibilidade a saída semântica da Etapa 3.

**Resultados observados:** script Python IR `0/30` (exit 1); script C IR `0/30` (exit 1). São falhas reais desses testadores e devem permanecer registradas. Não autorizam inferir que todos os 30 programas tenham TAC incorreta, nem permitem declarar aprovação oficial com base nas verificações complementares. O critério de avaliação final deve ser esclarecido pelo docente. Nenhum `.sh` ou gabarito foi alterado nesta revisão.

## 5. Integridade e reprodutibilidade

A execução registrada confirma `16/16` scripts preservados conforme SHA-256 e compilação GCC disponível. Os logs de `gerar_evidencias.py` apresentam o código de saída de cada avaliação. O gerador pode indicar **duas ações não aprovadas** pelo resultado dos `.sh` de IR, mesmo quando as validações complementares passam.

Na raiz da entrega:

```bash
python3 verificar_scripts_oficiais.py
python3 gerar_evidencias.py
```

Para comandos por etapa e exemplos `-O0`/`-O1`, consultar `COMO_TESTAR.md`.

## 6. Conclusão

As quatro etapas estão implementadas e possuem cobertura demonstrada nas suítes especificadas. A última etapa inclui passes reais de otimização em Python e C, validados nos cenários O01–O10. **A ressalva dos dois testadores oficiais de IR permanece aberta.** Não se afirma correção universal, otimização global completa ou aprovação formal dos scripts de IR.
