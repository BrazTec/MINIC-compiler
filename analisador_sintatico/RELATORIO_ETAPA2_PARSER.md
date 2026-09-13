# RELATÓRIO – ETAPA 2: ANALISADOR SINTÁTICO (PARSER)

## 1. Introdução

Este relatório apresenta o desenvolvimento da segunda etapa do projeto da linguagem educacional **MINIC**, correspondente à implementação do **analisador sintático (parser)**.

Na Etapa 1 foi construído o analisador léxico, responsável por converter o código-fonte em uma sequência de tokens. Nesta Etapa 2, o objetivo passou a ser consumir essa sequência de tokens e verificar se ela obedece à gramática da linguagem MINIC, construindo uma **árvore sintática abstrata (AST)** quando o programa é sintaticamente válido, ou reportando um **erro sintático** e interrompendo a análise quando não é.

Assim como na etapa anterior, o parser foi implementado inicialmente em **Python** e, em seguida, a mesma arquitetura foi traduzida para **C**, mantendo o mesmo comportamento nas duas linguagens.

---

## 2. Objetivo

O objetivo desta etapa foi desenvolver um analisador sintático capaz de:

* ler um arquivo-fonte `.c` (subconjunto MINIC) e tokenizá-lo internamente;
* reconhecer declarações globais de variáveis (com e sem inicializador, com e sem vetor);
* reconhecer declarações de funções, com lista de parâmetros tipados;
* reconhecer blocos, inclusive aninhados;
* reconhecer os comandos `if`/`else`, `while`, `return` e comandos de expressão;
* reconhecer expressões com a precedência correta entre atribuição, operadores lógicos, relacionais, aritméticos e unários;
* reconhecer chamadas de função e indexação de vetores;
* construir a AST no formato de S-expressão definido pelo conjunto de testes do professor;
* detectar e reportar erros sintáticos, encerrando a execução com código de saída diferente de zero e sem produzir AST;
* apresentar comportamento equivalente nas implementações em Python e em C.

---

## 3. Estrutura do projeto

```text
parser/
│
├── parser.py          # analisador sintático em Python (lexer + parser + AST)
├── parser.c            # analisador sintático em C (mesma arquitetura)
├── run_tests.py        # script de validação automática contra os 50 casos
│
└── testes-parser-50/
    ├── README.md
    ├── manifesto.json
    ├── INDICE.txt
    ├── EXECUCAO.txt
    └── casos/
        ├── 01_declaracao_inteira_simples/
        │   ├── codigo.c
        │   ├── ast.esperada.txt
        │   └── resultado.esperado.txt
        ├── 02_inicializacao_inteira/
        ├── ...
        └── 50_condicao_vazia/
```

O conjunto `testes-parser-50`, fornecido pelo professor, contém **50 programas completos em C**: os casos 01–25 devem ser **aceitos**, produzindo a AST indicada em `ast.esperada.txt`; os casos 26–50 devem ser **rejeitados**, sem gerar AST.

---

## 4. Arquitetura da solução

Diferentemente da Etapa 1, o parser não depende de um arquivo de tokens gerado previamente pelo scanner: cada implementação (`parser.py` e `parser.c`) é **autocontida**, incluindo seu próprio lexer interno, que reaproveita as mesmas regras léxicas definidas na Etapa 1 (palavras reservadas, operadores de um e dois caracteres, literais, comentários). Isso permite que o parser seja chamado diretamente sobre o código-fonte, conforme exigido pelo enunciado:

```bash
python parser.py codigo.c
./parser codigo.c
```

A arquitetura de cada implementação é dividida em três camadas:

1. **Lexer** – transforma o código-fonte em uma lista de tokens (tipo, lexema, valor, linha, coluna).
2. **Parser (recursive descent / descida recursiva)** – consome os tokens e constrói a AST, com uma função para cada símbolo não-terminal da gramática.
3. **Impressão da AST** – cada nó sabe se representar como uma S-expressão textual, seguindo o formato definido pelos testes.

---

## 5. Gramática implementada

A gramática foi derivada da análise dos 50 casos de teste fornecidos. Em notação simplificada:

```text
Program        → (GlobalDecl)* EOF
GlobalDecl     → Tipo IDENT ( VarDeclTail | FuncTail )
               | Stmt                      // comandos soltos também aceitos no escopo global

VarDeclTail    → '[' Expr ']' ';'          // declaração de vetor com tamanho
               | '=' Expr ';'              // declaração com inicializador
               | ';'                       // declaração simples

FuncTail       → '(' Params? ')' Block
Params         → Param (',' Param)*
Param          → Tipo IDENT

Block          → '{' Stmt* '}'
Stmt           → Block
               | Tipo IDENT VarDeclTail    // declaração local
               | 'if' '(' Expr ')' Stmt ('else' Stmt)?
               | 'while' '(' Expr ')' Stmt
               | 'return' Expr? ';'
               | Expr ';'                  // comando de expressão

Expr           → Assign
Assign         → Or ('=' Assign)?          // associativo à direita; alvo deve ser Id ou Index
Or             → And ('||' And)*
And            → Equality ('&&' Equality)*
Equality       → Relational (('=='|'!=') Relational)*
Relational     → Additive (('<'|'>'|'<='|'>=') Additive)*
Additive       → Multiplicative (('+'|'-') Multiplicative)*
Multiplicative → Unary (('*'|'/'|'%') Unary)*
Unary          → ('-'|'!'|'+') Unary | Postfix
Postfix        → Primary ( '[' Expr ']' | '(' Args? ')' )*
Primary        → IDENT | INT_LIT | FLOAT_LIT | CHAR_LIT | STRING_LIT
               | 'true' | 'false' | '(' Expr ')'
```

A ordem das regras de expressão reflete a precedência de operadores da linguagem, da menor (atribuição) para a maior (literais e parênteses), garantindo, por exemplo, que `2 + 3 * 4` seja interpretado como `Binary(+, 2, Binary(*, 3, 4))`.

---

## 6. Formato da AST

A AST é impressa como uma S-expressão, usando os seguintes construtores, na mesma notação especificada pelo README de `testes-parser-50`:

| Nó | Formato |
|---|---|
| Programa | `Program(decl1, decl2, ...)` |
| Declaração de variável | `VarDecl(tipo nome)`, `VarDecl(tipo nome=Expr)` ou `VarDecl(tipo nome size=Expr)` |
| Função | `Function(tipo nome(params) Block(...))` |
| Bloco | `Block(stmt1, stmt2, ...)` |
| If | `If(cond, then, else_ou_NULL)` |
| While | `While(cond, corpo)` |
| Return | `Return(expr_ou_NULL)` |
| Comando de expressão | `ExprStmt(expr)` |
| Atribuição | `Assign(alvo, expr)` |
| Binária | `Binary(op, esq, dir)` |
| Unária | `Unary(op, expr)` |
| Chamada | `Call(Id(nome), arg1, arg2, ...)` |
| Indexação | `Index(base, indice)` |
| Identificador | `Id(nome)` |
| Literal | `Lit(int, valor)`, `Lit(real, valor)`, `Lit(bool, valor)` |

`NULL` representa a ausência de um ramo (por exemplo, `if` sem `else`, ou `return` sem expressão).

---

## 7. Tratamento de erros sintáticos

Para os 25 casos que devem ser rejeitados (26–50), o parser identifica a falha no ponto exato em que o token encontrado não corresponde ao esperado pela gramática, produzindo uma mensagem do tipo:

```text
Erro sintático na linha 1, coluna 14: token ELSE inesperado ('else')
```

ou, quando o problema é a ausência de um token esperado:

```text
Erro sintático na linha 1, coluna 15: esperado PONTO_E_VIRGULA, encontrado 'return' (RETURN)
```

Nesses casos, o programa é encerrado imediatamente com **código de saída diferente de zero** e **nenhuma AST é impressa**, conforme especificado no `README.md` de `testes-parser-50` ("A mensagem exata pode variar entre implementações; as pistas indicam o ponto de falha esperado").

Entre os erros cobertos pelos 25 casos inválidos estão:

* ausência de ponto e vírgula em declarações e comandos;
* ausência de parênteses/chaves de blocos, condições e chamadas;
* parâmetros de função sem tipo ou sem identificador;
* vírgulas extras ou ausentes em listas de parâmetros/argumentos;
* expressões incompletas (operador sem operando, dois operadores consecutivos);
* atribuição sem um alvo válido (identificador ou indexação);
* uso de `else` sem `if` correspondente;
* fechamento de chave extra (sem abertura correspondente).

---

## 8. Testes automatizados

Foi desenvolvido o script `run_tests.py`, que:

1. percorre todos os diretórios em `testes-parser-50/casos`;
2. executa o parser (Python ou C, conforme o comando informado) sobre cada `codigo.c`;
3. para os casos que devem ser **ACEITOS**, compara a AST produzida com `ast.esperada.txt` (ignorando diferenças de espaçamento, já que o próprio conjunto de gabaritos não é uniforme quanto a isso);
4. para os casos que devem ser **REJEITADOS**, verifica apenas se o código de saída é diferente de zero.

Execução:

```bash
python run_tests.py testes-parser-50/casos python parser.py
python run_tests.py testes-parser-50/casos ./parser
```

### Resultado obtido

```text
OK: 50  FALHOU: 0  TOTAL: 50
```

O mesmo resultado foi obtido para as duas implementações (Python e C), confirmando a equivalência de comportamento entre elas.

---

## 9. Observação sobre o gabarito do caso 24

Durante a validação, o caso `24_la_o_com_express_o_complexa` inicialmente não batia com o gabarito oficial, mesmo com a AST produzida sendo estruturalmente coerente com os demais casos. Uma inspeção da string esperada revelou uma **inconsistência no arquivo `ast.esperada.txt`**: o texto continha 23 parênteses de abertura contra 24 de fechamento (um parêntese de fechamento a mais), tornando a S-expressão do gabarito malformada.

Após a correção do parêntese excedente no arquivo de gabarito, a AST produzida pelo parser passou a coincidir integralmente com o esperado, e o conjunto completo de 50 casos passou a ser aprovado nas duas implementações.

---

## 10. Resultados consolidados

| Verificação | Resultado |
|---|---|
| Implementação Python | Aprovada |
| Implementação C | Aprovada |
| Casos válidos (01–25) | 25 aprovados |
| Casos inválidos (26–50) | 25 aprovados |
| Total de casos automatizados | 50 |
| Falhas na versão Python | 0 |
| Falhas na versão C | 0 |
| Inconsistências encontradas nos fixtures | 1 (corrigida – caso 24) |

---

## 11. Conclusão

A segunda etapa do projeto MINIC foi concluída com a implementação de analisadores sintáticos nas linguagens **Python e C**, ambos capazes de consumir código-fonte da linguagem MINIC diretamente (sem depender de uma etapa intermediária de tokens em arquivo), construir a AST correspondente e detectar erros sintáticos com recuperação adequada de posição (linha/coluna).

A validação contra o conjunto de 50 casos fornecido pelo professor resultou em **50 OK, 0 falhas** em ambas as implementações, após a identificação e correção de uma inconsistência pontual no arquivo de gabarito do caso 24.

Dessa forma, a etapa de análise sintática fornece uma base validada para a continuidade do desenvolvimento do compilador MINIC nas etapas posteriores (análise semântica e geração de código).

---

## 12. Referências

Material didático e especificação da linguagem MINIC disponibilizados na disciplina.

AHO, A. V.; LAM, M. S.; SETHI, R.; ULLMAN, J. D. **Compiladores: Princípios, Técnicas e Ferramentas**. 2. ed. São Paulo: Pearson, 2008.

FREE SOFTWARE FOUNDATION. **GCC – GNU Compiler Collection**. Documentação oficial do compilador GCC.
