# 01 — ARQUITETURA E INTEGRAÇÃO DO COMPILADOR MINIC

## 1. Contrato observado no código

**Linguagem de implementação:** Python e C11. **Linguagem compilada:** subconjunto educacional de C (MINIC).

```text
Arquivo de entrada
  ↓
[Scanner]   identifica tokens, lexemas e posições
  ↓ lista de tokens
[Parser]    analisa gramaticalmente por descida recursiva e cria AST
  ↓ AST
[Semântico] constrói/consulta escopos, atribui tipos e produz diagnósticos
  ├── erro: rejeita, não gera TAC
  └── sucesso
       ↓
[IR]        gera TAC, rótulos, temporários e instruções de controle
  └── opcional -O1 (otimizações locais/parciais)
```

### Python

- `scanner.py`: classe `Scanner` e tokens da Etapa 1.
- `parser.py`: `tokenize_source(source)`, `Parser(tokens, semantic_mode=True).parse()`; classes de AST.
- `semantic.py`: `SemanticAnalyzer(source).analyze(ast)`; classes `TypeRef`, `Symbol`, `Scope`, `Diagnostic`.
- `ir.py`: gerador TAC e otimizador conservador sobre instruções estruturadas.
- `minic.py`: driver. Sem flags, mantém o resultado da Etapa 3; com `--ir`, executa scanner → parser → semântico → IR.

O argumento `semantic_mode=True` flexibiliza especificamente a rejeição sintática de certos destinos de atribuição, preservando a detecção semântica de valor-L (SEM013). O modo padrão de `parser.py` permanece compatível com os casos da Etapa 2.

### C

- `scanner.c` serve à execução independente da Etapa 1.
- `parser.c` inclui o lexer necessário em memória e o parser descendente recursivo; pode executar sozinho na Etapa 2.
- `semantic.c` analisa a AST proveniente de `parser.c`.
- `ir.c`: gerador TAC, rótulos/temporários e otimização local.
- `minic.c` é o driver. Inclui `parser.c`, `semantic.c` e `ir.c` porque o script semântico fornecido compila exclusivamente `minic.c` com GCC. Esse fato deve ser explicitado como uma limitação de desacoplamento físico, ainda que a lógica esteja separada por componentes.

## 2. A técnica sintática é DESCENDENTE

O parser existente não utiliza pilha LR, estados LR, tabela ACTION/GOTO, shift/reduce nem algoritmo SLR/LALR. A classe Python `Parser` e as funções C equivalentes executam uma **análise descendente recursiva**. As funções de expressão (`parse_assignment`, `parse_logical_or`, `parse_multiplicative`, etc.) codificam precedência e associatividade.

Se um enunciado futuro exigir parser ascendente, isso será **trabalho novo de implementação**, não uma simples atualização de README. Não afirmar que o código já o implementa.

## 3. Responsabilidades e falhas

| Camada | Entrada | Saída | Rejeições |
|---|---|---|---|
| Scanner | texto | tokens e posição | lexemas inválidos |
| Parser | tokens | AST | gramática inválida |
| Semântico | AST + contexto | diagnósticos/aceite | símbolos, tipos, escopos e retornos |
| IR | AST sem erros semânticos | TAC de três endereços | não gerar IR após erro anterior |

## 4. Desacoplamento e módulos do repositório

A duplicação histórica de `testes-scanner-minic/` e `testes-scanner-minic_codes/` dificulta descobrir a implementação principal. Para preservar caminhos/fixtures, esta revisão **declara a segunda como canônica** e conserva a primeira como referência histórica. Os dois conjuntos de módulos centrais e scripts são byte a byte idênticos no ZIP recebido. Os casos léxicos, algumas evidências e saídas auxiliares apresentam diferenças.

Fase futura recomendada, dependente de validação de regressão: reestruturar `src/python`, `src/c`, `tests/*` e `docs`; no C, considerar headers e compilação separada, mantendo compatibilidade com os testadores oficiais por meio de arquivo driver.

## 5. Evidência de implementação x documentação

Uma tabela SDD documenta regras semânticas associadas à gramática. Essa formalização não implica que o parser seja ascendente, nem que o gerador IR use algoritmo ascendente. A SDD descritiva está em `02_SDD_ANALISE_SEMANTICA.md` e foi vinculada às rotinas reais.
