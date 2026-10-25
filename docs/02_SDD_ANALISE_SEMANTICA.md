# 02 — SDD (DEFINIÇÃO DIRIGIDA PELA SINTAXE) DA MINIC

**Escopo:** formalização explicativa das regras semânticas da Etapa 3 já existentes em `semantic.py` e `semantic.c`. O parser de origem é **descendente recursivo**, e a aplicação das regras ocorre por **percursos sobre a AST**, não por reduções de um parser LR.

## 1. Modelo de atributos

- `E.tipo` (sintetizado): tipo de uma expressão, `int`, `float`, `bool`, `char`, `void`, vetor ou tipo de erro.
- `N.ambiente` (herdado): ambiente léxico em que nós de declaração/comando/expressão são analisados.
- `F.tipo_retorno` (herdado nos `return`): tipo declarado pela função que contém o comando.
- `B.retorna_sempre` (sintetizado): predicado de cobertura de retorno para blocos e condicionais.
- `P.diagnosticos` (coleção): erros semânticos acumulados; não é um token ou atributo léxico.

Estruturas de implementação: `TypeRef`, `Symbol`, `Scope`, `Diagnostic`, `SemanticAnalyzer`; na versão C, `SemType`, `SemSymbol`, `SemScope`, `SemDiagnosticList`.

## 2. Produções e regras semânticas (notação resumida)

| Produção abstrata | Atributos / ação dirigida pela sintaxe | Rotina Python relacionada |
|---|---|---|
| `Programa → Item*` | coletar primeiro símbolos globais e assinaturas de funções; analisar corpos na segunda passagem | `analyze`, `_declare` |
| `Bloco → { Decl/Comando* }` | `novoAmbiente = Scope(ambientePai)` para blocos aninhados; resolver sombra léxica | `_analyze_block`, `Scope.resolve` |
| `Decl → Tipo id [= E]` | inserir `id` no ambiente local; rejeitar duplicação no mesmo ambiente; verificar compatibilidade de `E.tipo` | `_analyze_var_decl`, `_declare`, `_compatible` |
| `E → id` | `E.tipo = lookup(E.ambiente,id).tipo`; SEM001 se ausente | `_resolve`, `_infer_expr` |
| `E → literal` | `E.tipo = tipoDoLiteral` | `_infer_expr` |
| `E → E1 op E2` | sintetizar `E.tipo` conforme operador e tipos de operandos; operadores aritméticos exigem numéricos | `_infer_expr` |
| `E → E1 = E2` | verificar valor-L de `E1`, depois compatibilidade entre destino e `E2.tipo` | `_infer_assignment`, `_infer_lvalue` |
| `E → id(args)` | resolver assinatura; validar aridade, tipos e retorno | `_infer_call` |
| `E → base[idx]` | verificar indexação e `idx.tipo = int`; sintetizar tipo do elemento | `_infer_index` |
| `Comando → if (E) S [else S]` | exigir `E.tipo = bool`; verificar cobertura de retorno por ambos os ramos | `_analyze_statement`, `_check_condition`, `_always_returns` |
| `Comando → while (E) S` | exigir `E.tipo = bool`; analisar corpo no ambiente apropriado | `_analyze_statement`, `_check_condition` |
| `Comando → return E?` | comparar tipo da expressão com `F.tipo_retorno`; verificar retorno em funções não-void | `_analyze_return`, `_always_returns` |

**Nota de fidelidade:** a tabela representa regras presentes ou pretendidas pelo desenho da implementação; não substitui cobertura de testes independentes, nem afirma que todas as restrições possíveis de MINIC já estão implementadas. Diagnósticos SEM001, SEM002, SEM003, SEM005, SEM006, SEM007, SEM008, SEM009, SEM011, SEM012 e SEM013 aparecem nos gabaritos da Etapa 3.

## 3. Exemplo de derivação de tipo

Fonte:

```c
float soma(float a, int b) { return a + b; }
```

Produção resumida: `E → E1 + E2`, com `E1.tipo=float` e `E2.tipo=int`. A regra de promoção permite `int → float`; portanto `E.tipo=float`, compatível com o `return` da função.

Um estreitamento implícito `float → int` não é permitido pelas regras desta etapa. A regra produz diagnóstico, não IR para execução.

## 4. Escopo léxico e sombreamento

`Scope(parent)` constrói um ambiente filho. `_declare` rejeita duplicação **no mesmo** ambiente (SEM002); `Scope.resolve` procura no ambiente corrente e sobe pelos ancestrais. Um identificador local pode, assim, sombrear o global sem ser tratado como duplicação do mesmo escopo.

## 5. Relação entre SDD e IR implementada

A SDD sintetiza/verifica tipos antes de gerar código intermediário. A tradução para TAC utiliza os conceitos `E.lugar` (temporário/valor), `B.codigo` (lista de instruções) e destinos de desvio/rótulos para fluxo de controle. Tais valores são gerados pelas rotinas de `IRGenerator`/`IRWriter` durante a visita à AST, e **não são campos persistidos na AST**. O vínculo formal entre produções gramaticais e tradução é descrito em `docs/06_SDD_TRADUCAO_TAC.md`.

## 6. Conclusão teórica

Uma SDD é uma associação explícita entre produções gramaticais e atributos/regras. Ela pode ser aplicada por visitas à AST gerada por parser descendente; a técnica ascendente não é requisito intrínseco de toda SDD. Se a disciplina exigir parser ascendente especificamente, será necessário projeto de implementação separado.
