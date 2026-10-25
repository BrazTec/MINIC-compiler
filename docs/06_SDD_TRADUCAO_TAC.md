# 06 — ESQUEMA DE TRADUÇÃO DIRIGIDO PELA SINTAXE: AST → TAC

## Escopo e correspondência

O parser MINIC **descendente recursivo** produz uma AST tipada/validada pela etapa semântica. O gerador de IR recebe essa AST, e implementa por visita regras equivalentes às ações de uma SDD. `E.lugar` é representado pelo valor retornado pelas funções `expression()` (Python) ou `ir_expression()` (C). `S.codigo` corresponde à sequência de `Instruction`/linhas TAC emitidas. Rótulos são produzidos por `label()` ou `ir_label()` e temporários por `temporary()` ou `ir_temp()`.

Essas são **regras de tradução por visita à AST**, não uma afirmação de implementação de parser LR ou um motor genérico de atributos em memória.

## Produções e regras

| Regra sintática | Regra de tradução (TAC) | Implementação |
|---|---|---|
| `E → id` | `E.lugar := lookup(nome, ambiente)` | `resolve` / `ir_resolve` |
| `E → literal` | `E.lugar := lexema(literal)` | `expression(Lit)` / `NODE_LIT` |
| `E → E1 op E2` | `t := novoTemp(); emit(t = E1.lugar op E2.lugar); E.lugar := t` | `Binary`/`NODE_BINARY` |
| `E → op E1` | `t := novoTemp(); emit(t = op E1.lugar); E.lugar := t` | `Unary`/`NODE_UNARY` |
| `S → id = E ;` | `emit(resolve(id) = E.lugar)` | `Assign`/`NODE_ASSIGN` |
| `E → f(args)` | avaliar args na ordem; `param a_i`; `t = call f, n` | `Call`/`NODE_CALL` |
| `E → vetor[idx]` | calcular offset; `t = load v[offset]` | `Index`/`NODE_INDEX` |
| `S → vetor[idx] = E` | calcular offset; `store v[offset], E.lugar` | `Assign(Index)` |
| `S → if (E) S1 else S2` | gerar `ifFalse`; bloco S1; `goto` fim; rótulo else; S2; rótulo fim | `If`/`NODE_IF` |
| `S → while (E) S1` | rótulo teste; avaliar E novamente; desviar se falso; S1; voltar ao teste | `While`/`NODE_WHILE` |
| `S → for(init;E;passo) S1` | reduzir sintaticamente a bloco com `init` e `while(E) { S1; passo; }` | `parse_for_statement` apenas em modo integrado |
| `S → break;` | `goto labelSaída` do laço mais interno, após verificação semântica | `Break`/`NODE_BREAK` |
| `S → return E;` | `emit(return E.lugar)` | `Return`/`NODE_RETURN` |

## Atributos herdados, escopo e tabela de símbolos

O ambiente lexical é herdado de bloco para comando/expressão. `push_scope()/pop_scope()` e `ir_push()/ir_pop()` criam e descartam associações entre identificadores de origem e nomes internos únicos, preservando sombreamento. Exemplos: duas declarações `x` em blocos aninhados geram nomes `x` e `x#2`.

## Controle de fluxo e avaliação

Condições de `if`/`while` geram saltos `ifFalse` e `goto`. As operações booleanas `&&`/`||` usam desvio de curto-circuito: o operando direito só é avaliado quando necessário. A condição do `while` é reavaliada em cada iteração. Chamadas preservam efeitos colaterais e a ordem esquerda→direita dos argumentos.

## Implementação dos otimizadores

O modo `-O0` gera TAC sem passes de otimização. `-O1` em Python opera sobre `Instruction` estruturadas; em C, sobre o texto TAC gerado. Ambos evitam propagação de fatos conhecida através de fronteiras de controle/efeitos laterais, com algumas simplificações locais. Não existe SDD de otimização global nem análise de fluxo de dados completa. Consulte `05_IR_IMPLEMENTACAO_E_LIMITACOES.md`.

## Limitações

Esse mapeamento explica as ações de tradução efetivamente existentes. Não constitui prova formal de preservação semântica para toda a linguagem; por isso são realizados testes adicionais de execução da TAC, comparação entre implementações e execução nativa de programas de referência.
