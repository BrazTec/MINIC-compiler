# RELATÓRIO – ETAPA 1: ANALISADOR LÉXICO

## 1. Introdução

Este relatório apresenta o desenvolvimento da primeira etapa do projeto da linguagem educacional **MINIC**, correspondente à implementação do **analisador léxico (lexer)**.

A linguagem MINIC é inspirada em um subconjunto da linguagem C e foi definida para permitir a construção incremental de um compilador. Nesta primeira etapa, o objetivo consiste em converter o código-fonte de entrada em uma sequência de tokens, preservando informações de **lexema, atributo, linha e coluna**, além de identificar determinados erros léxicos.

Conforme definido no projeto, o analisador foi implementado inicialmente em **Python** e, após sua validação, a mesma lógica foi traduzida para **C**.

Além da implementação dos dois scanners, foram utilizados casos de teste válidos, inválidos e programas completos em C, permitindo verificar o comportamento do analisador diante das principais construções da linguagem MINIC.

---

## 2. Objetivo

O objetivo desta etapa foi desenvolver um analisador léxico capaz de:

* ler arquivos-fonte com extensão `.minic` ou `.c`;
* reconhecer palavras reservadas da linguagem MINIC;
* reconhecer identificadores;
* reconhecer literais inteiros e reais;
* reconhecer literais de caractere e cadeia;
* reconhecer operadores aritméticos, relacionais e lógicos;
* reconhecer operadores de atribuição;
* reconhecer delimitadores;
* ignorar espaços em branco e comentários;
* preservar a posição de cada token por meio de linha e coluna;
* detectar erros léxicos definidos nos casos de teste;
* produzir a saída no formato JSONL;
* realizar recuperação após determinados erros, permitindo a continuação da análise;
* apresentar comportamento equivalente nas implementações em Python e C.

---

## 3. Estrutura do projeto

Após a preparação dos arquivos necessários, o projeto passou a possuir a seguinte organização geral:

```text
testes-scanner-minic_codes/
│
├── scanner.py
├── scanner.c
│
├── check_fixtures.py
├── gerar_expected.py
│
├── test_scanner_python.sh
├── test_scanner_c.sh
├── test_scanner_python.py
├── test_scanner_c.py
│
├── README.md
├── MANIFESTO.md
│
├── casos-validos/
│   ├── v01_declaracoes.minic
│   ├── v01_declaracoes.expected.jsonl
│   ├── v02_expressao_precedencia.minic
│   ├── v02_expressao_precedencia.expected.jsonl
│   ├── v03_operadores_compostos.minic
│   ├── v03_operadores_compostos.expected.jsonl
│   ├── v04_comentarios_e_posicoes.minic
│   ├── v04_comentarios_e_posicoes.expected.jsonl
│   ├── v05_funcoes_e_vetores.minic
│   ├── v05_funcoes_e_vetores.expected.jsonl
│   ├── v06_reservadas_vs_identificadores.minic
│   ├── v06_reservadas_vs_identificadores.expected.jsonl
│   ├── v07_literais_opcionais.minic
│   └── v07_literais_opcionais.expected.jsonl
│
├── casos-invalidos/
│   ├── i01 ...
│   ├── i02 ...
│   ├── i03 ...
│   ├── i04 ...
│   ├── i05 ...
│   └── i06 ...
│
└── casos-programas-c/
    ├── c01_fibonacci.c
    ├── c02_primos.c
    ├── c03_media_vetor.c
    ├── c04_menu_interativo.c
    └── c05_controle_temperatura.c
```

Nos casos válidos, cada entrada `.minic` possui um arquivo `.expected.jsonl`, contendo a sequência de tokens esperada.

Nos casos inválidos, além do arquivo de entrada e do `.expected.jsonl`, são utilizados arquivos `.errors.jsonl`, responsáveis por especificar os diagnósticos léxicos esperados.

---

## 4. Adequações realizadas nos casos de teste

Durante o desenvolvimento foram identificados alguns arquivos mencionados na documentação do projeto que não estavam presentes no conjunto disponibilizado inicialmente.

### 4.1 Casos válidos

O arquivo `MANIFESTO.md` descrevia sete casos válidos, porém a pasta correspondente não estava presente. Dessa forma, foi criada a pasta:

```text
casos-validos/
```

Foram implementados os sete casos definidos no manifesto:

| Caso                                      | Objetivo                                                                   |
| ----------------------------------------- | -------------------------------------------------------------------------- |
| `v01_declaracoes.minic`                   | Palavras reservadas, identificadores, literais, atribuição e delimitadores |
| `v02_expressao_precedencia.minic`         | Operadores aritméticos, relacionais e parênteses                           |
| `v03_operadores_compostos.minic`          | Operadores compostos e estratégia maximal munch                            |
| `v04_comentarios_e_posicoes.minic`        | Comentários, espaços, linhas e colunas                                     |
| `v05_funcoes_e_vetores.minic`             | Funções, parâmetros, vetores, chamadas e blocos                            |
| `v06_reservadas_vs_identificadores.minic` | Diferenciação entre palavras reservadas e identificadores semelhantes      |
| `v07_literais_opcionais.minic`            | Literais de caractere, cadeia e sequências de escape                       |

Após a implementação e validação do scanner em Python, foram gerados os respectivos arquivos `.expected.jsonl`.

Os sete casos válidos resultaram em **303 tokens esperados**.

### 4.2 Caso inválido i06

Na pasta `casos-invalidos`, os arquivos de resultado do caso `i06_identificador_iniciado_por_digito` estavam disponíveis, porém o arquivo de entrada `.minic` não estava presente.

Foi criado o arquivo:

```text
i06_identificador_iniciado_por_digito.minic
```

com o seguinte conteúdo:

```c
int 123abc = 4;
```

O objetivo desse teste é verificar a identificação do lexema inválido `123abc`.

O scanner registra o diagnóstico:

```json
{"error":"INVALID_IDENTIFIER","lexeme":"123abc","line":1,"column":5}
```

e mantém a recuperação léxica, produzindo separadamente os tokens correspondentes a `123` e `abc`.

---

## 5. Especificação dos principais tokens

O analisador léxico foi construído utilizando os nomes de tokens definidos pelos arquivos de teste fornecidos.

### 5.1 Palavras reservadas

| Lexema     | Token      |
| ---------- | ---------- |
| `int`      | `INT`      |
| `float`    | `FLOAT`    |
| `bool`     | `BOOL`     |
| `char`     | `CHAR`     |
| `void`     | `VOID`     |
| `if`       | `IF`       |
| `else`     | `ELSE`     |
| `while`    | `WHILE`    |
| `for`      | `FOR`      |
| `return`   | `RETURN`   |
| `break`    | `BREAK`    |
| `continue` | `CONTINUE` |
| `true`     | `TRUE`     |
| `false`    | `FALSE`    |
| `print`    | `PRINT`    |
| `read`     | `READ`     |

### 5.2 Identificadores e literais

| Token        | Regra                         | Atributo               |
| ------------ | ----------------------------- | ---------------------- |
| `IDENT`      | `[A-Za-z_][A-Za-z0-9_]*`      | Próprio lexema         |
| `INT_LIT`    | `[0-9]+`                      | Valor inteiro          |
| `FLOAT_LIT`  | `[0-9]+\.[0-9]+`              | Valor real             |
| `CHAR_LIT`   | Caractere entre aspas simples | Caractere interpretado |
| `STRING_LIT` | Cadeia entre aspas duplas     | Conteúdo da cadeia     |

Por exemplo:

```c
int valor = 10;
```

produz tokens equivalentes a:

```json
{"token":"INT","lexeme":"int","attribute":null,"line":1,"column":1}
{"token":"IDENT","lexeme":"valor","attribute":"valor","line":1,"column":5}
{"token":"ASSIGN","lexeme":"=","attribute":null,"line":1,"column":11}
{"token":"INT_LIT","lexeme":"10","attribute":10,"line":1,"column":13}
{"token":"SEMICOLON","lexeme":";","attribute":null,"line":1,"column":15}
```

### 5.3 Operadores

Os principais operadores reconhecidos são:

| Categoria   | Tokens                                      |
| ----------- | ------------------------------------------- |
| Aritméticos | `PLUS`, `MINUS`, `STAR`, `SLASH`, `PERCENT` |
| Atribuição  | `ASSIGN`                                    |
| Relacionais | `EQ`, `NE`, `LT`, `GT`, `LE`, `GE`          |
| Lógicos     | `AND`, `OR`, `NOT`                          |

Para operadores compostos foi utilizada a estratégia de **maximal munch**, verificando inicialmente os operadores de maior comprimento:

```text
==
!=
<=
>=
&&
||
```

Dessa forma, por exemplo, `<=` é reconhecido como um único token `LE`, em vez de dois tokens separados.

### 5.4 Delimitadores

Foram implementados:

```text
( ) [ ] { } ; ,
```

correspondendo aos tokens:

```text
LPAREN
RPAREN
LBRACKET
RBRACKET
LBRACE
RBRACE
SEMICOLON
COMMA
```

O token `DOT` também foi implementado para reproduzir o comportamento especificado no caso de teste do número real malformado.

---

## 6. Implementação em Python

A primeira implementação foi desenvolvida no arquivo:

```text
scanner.py
```

O scanner percorre o código-fonte caractere por caractere, mantendo:

* posição absoluta no arquivo;
* linha atual;
* coluna atual.

A estrutura foi organizada em funções específicas para diferentes categorias léxicas, incluindo:

```text
scan_identifier()
scan_number()
scan_string()
scan_char()
skip_line_comment()
skip_block_comment()
```

Além dessas funções, foram implementadas operações auxiliares equivalentes a:

```text
current()
peek()
advance()
```

A escolha de um scanner baseado na leitura sequencial dos caracteres, em vez de uma única expressão regular, facilitou o controle de linha e coluna, o tratamento de erros e principalmente a posterior tradução da lógica para a linguagem C.

---

## 7. Comentários

Comentários não geram tokens.

Foram implementados os dois formatos definidos pela linguagem:

### Comentário de linha

```c
// comentário
```

Seu conteúdo é ignorado até a próxima quebra de linha.

### Comentário de bloco

```c
/*
    comentário
*/
```

Seu conteúdo é ignorado até a sequência `*/`.

Caso o final do arquivo seja encontrado antes do fechamento do comentário, o scanner gera:

```text
UNTERMINATED_BLOCK_COMMENT
```

Mesmo sendo descartados, os caracteres e quebras de linha presentes em comentários continuam sendo considerados no cálculo das posições dos tokens seguintes.

---

## 8. Tratamento de erros léxicos

Os casos inválidos fornecidos foram utilizados para definir o comportamento de recuperação do scanner.

Foram tratados os seguintes diagnósticos:

| Erro                          | Situação                                         |
| ----------------------------- | ------------------------------------------------ |
| `UNKNOWN_SYMBOL`              | Símbolo não reconhecido                          |
| `UNTERMINATED_BLOCK_COMMENT`  | Comentário de bloco sem fechamento               |
| `UNTERMINATED_CHAR_LITERAL`   | Literal de caractere sem fechamento              |
| `UNTERMINATED_STRING_LITERAL` | Cadeia sem aspas de fechamento                   |
| `MALFORMED_REAL_LITERAL`      | Número como `12.`                                |
| `INVALID_IDENTIFIER`          | Identificador iniciado por dígito, como `123abc` |

O analisador foi desenvolvido de modo a continuar a análise quando existe possibilidade de recuperação.

Por exemplo:

```c
x = 1 @ 2;
```

gera o diagnóstico referente ao símbolo `@`, porém os tokens correspondentes a `2` e `;` continuam sendo produzidos.

Outro exemplo é:

```c
int 123abc = 4;
```

O lexema completo `123abc` gera `INVALID_IDENTIFIER`, mas os tokens recuperáveis continuam sendo reconhecidos.

---

## 9. Formato de saída

A saída do scanner é realizada no formato **JSON Lines (JSONL)**, contendo um objeto JSON por linha.

Cada token possui obrigatoriamente:

```text
token
lexeme
attribute
line
column
```

Exemplo:

```json
{"token":"IDENT","lexeme":"resultado","attribute":"resultado","line":1,"column":5}
```

O último token de toda entrada é:

```text
EOF
```

Os diagnósticos possuem:

```text
error
lexeme
line
column
```

Exemplo:

```json
{"error":"UNKNOWN_SYMBOL","lexeme":"@","line":1,"column":7}
```

---

## 10. Validação dos fixtures

O script disponibilizado:

```text
check_fixtures.py
```

foi utilizado para verificar a estrutura dos arquivos `.expected.jsonl` e `.errors.jsonl`.

Após a criação dos casos válidos e dos respectivos resultados esperados, foi executado:

```text
python3 check_fixtures.py
```

O resultado obtido foi:

```text
Fixtures válidas: 794 tokens esperados verificados.
```

Esse resultado confirma que os arquivos de fixture apresentam os campos necessários, linhas e colunas em formato inteiro e presença do token `EOF`.

**Figura 1 – Validação dos fixtures do projeto**

> Inserir neste ponto uma captura de tela do terminal mostrando o comando `python3 check_fixtures.py` e a mensagem `Fixtures válidas: 794 tokens esperados verificados.`

Fonte: elaboração própria.

---

## 11. Testes automatizados da versão Python

Os scripts de teste originalmente disponibilizados possuem extensão `.sh` e são destinados à execução através de um ambiente Bash.

O desenvolvimento e os testes deste projeto foram realizados no **Visual Studio Code em ambiente Windows**, utilizando o terminal integrado PowerShell.

Como o comando `bash` não estava disponível nesse ambiente, foi implementado um script equivalente em Python:

```text
test_scanner_python.py
```

Esse script preserva a lógica do `test_scanner_python.sh`, realizando:

1. localização automática dos arquivos `.expected.jsonl`;
2. identificação da entrada `.minic` ou `.c` correspondente;
3. execução do `scanner.py`;
4. leitura da saída JSONL;
5. separação dos objetos que representam tokens;
6. comparação com o resultado esperado;
7. apresentação da primeira diferença, caso exista;
8. geração de um resumo final.

A execução foi realizada através do comando:

```text
python3 test_scanner_python.py
```

O resultado final obtido foi:

```text
================================================================
Resumo: 18 OK, 0 falharam, 0 avisos, 18 casos verificados.
RESULTADO FINAL: TODOS OS TESTES PASSARAM.
```

Foram verificados:

* 7 casos válidos;
* 6 casos inválidos;
* 5 programas completos em C.

Totalizando:

```text
18 casos de teste
```

**Figura 2 – Resultado dos testes automatizados do scanner em Python**

> Inserir neste ponto a captura de tela do terminal contendo o resumo `18 OK, 0 falharam, 0 avisos`.

Fonte: elaboração própria.

---

## 12. Implementação em C

Após a validação integral da versão Python, foi realizada a tradução da mesma arquitetura para a linguagem C, no arquivo:

```text
scanner.c
```

A implementação em C manteve os mesmos princípios da versão Python:

* leitura caractere por caractere;
* controle de posição;
* controle de linha e coluna;
* maximal munch;
* reconhecimento de palavras reservadas;
* reconhecimento de identificadores e literais;
* tratamento de comentários;
* tratamento e recuperação de erros;
* geração de JSONL.

A versão em C utiliza uma estrutura `Scanner` para armazenar informações como:

```text
source
length
pos
line
column
```

Também foram implementadas funções equivalentes às utilizadas na versão Python, permitindo manter o comportamento entre as duas implementações.

---

## 13. Ambiente de desenvolvimento da versão C

Todo o desenvolvimento foi realizado no **Visual Studio Code**, utilizando:

* extensão **Python**, da Microsoft;
* extensão **C/C++**, da Microsoft;
* terminal integrado PowerShell.

Para compilar o código C no ambiente Windows foi instalado o **MSYS2**, utilizando o ambiente **UCRT64**.

O compilador GCC foi instalado através do comando:

```text
pacman -S mingw-w64-ucrt-x86_64-gcc
```

Como alternativa, a documentação do Visual Studio Code apresenta a instalação do toolchain completo:

```text
pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain
```

O compilador utilizado nos testes foi:

```text
gcc.exe (Rev3, Built by MSYS2 project) 16.2.0
```

A compilação do scanner foi realizada utilizando o padrão C11 e as opções de diagnóstico:

```text
gcc -Wall -Wextra -std=c11 scanner.c -o scanner.exe
```

Essas opções habilitam avisos adicionais durante a compilação e garantem a utilização do padrão C11.

---

## 14. Adaptação dos testes para C no Windows

Assim como ocorreu na versão Python, o script originalmente disponibilizado para o scanner C utilizava Bash:

```text
test_scanner_c.sh
```

Para permitir a execução automática diretamente no PowerShell, foi desenvolvido:

```text
test_scanner_c.py
```

O script realiza inicialmente a compilação de:

```text
scanner.c
```

utilizando GCC e, caso a compilação seja concluída com sucesso, executa automaticamente todos os casos de teste.

No ambiente utilizado, o compilador foi localizado em:

```text
C:\msys64\ucrt64\bin\gcc.exe
```

O testador foi configurado para incluir o diretório necessário no ambiente utilizado pelo processo, permitindo tanto a compilação quanto a execução do arquivo `scanner.exe`.

---

## 15. Resultado dos testes da versão C

Após a compilação, o scanner C foi submetido aos mesmos 18 casos utilizados para validar a implementação Python.

O resultado final foi:

```text
====================================================================
Resumo: 18 OK, 0 falharam, 0 avisos, 18 casos verificados.
RESULTADO FINAL: TODOS OS TESTES PASSARAM.
```

Dessa forma, as duas implementações apresentaram o mesmo comportamento para o conjunto de testes utilizado.

**Figura 3 – Resultado dos testes automatizados do scanner em C**

> Inserir neste ponto a captura de tela do terminal contendo o resumo da execução de `python3 test_scanner_c.py`.

Fonte: elaboração própria.

---

## 16. Resultados consolidados

Os resultados obtidos podem ser resumidos da seguinte forma:

| Verificação                                | Resultado   |
| ------------------------------------------ | ----------- |
| Implementação Python                       | Aprovada    |
| Implementação C                            | Aprovada    |
| Casos válidos                              | 7 aprovados |
| Casos inválidos                            | 6 aprovados |
| Programas completos em C                   | 5 aprovados |
| Total de casos automatizados               | 18          |
| Falhas na versão Python                    | 0           |
| Falhas na versão C                         | 0           |
| Avisos nos testes                          | 0           |
| Tokens verificados por `check_fixtures.py` | 794         |

Os testes demonstraram que as implementações em Python e C reconhecem corretamente os elementos léxicos definidos para a linguagem MINIC e preservam o formato estabelecido pelos fixtures.

---

## 17. Ressalvas e decisões de implementação

Durante o desenvolvimento foram registradas algumas adequações importantes:

1. A pasta `casos-validos`, mencionada no `MANIFESTO.md`, não estava presente inicialmente e foi criada com os casos `v01` até `v07`.

2. Para os casos válidos foram criados arquivos `.minic` e `.expected.jsonl`. Arquivos `.errors.jsonl` não foram necessários, pois são destinados aos casos que devem produzir diagnósticos.

3. Na pasta `casos-invalidos`, foi incluído o arquivo:

```text
i06_identificador_iniciado_por_digito.minic
```

com o conteúdo:

```c
int 123abc = 4;
```

4. Foram criadas as duas implementações solicitadas:

```text
scanner.py
scanner.c
```

5. Os scripts `.sh` disponibilizados são destinados a ambientes Bash. Como o projeto foi desenvolvido no Windows através do PowerShell integrado ao VS Code, foram criadas versões equivalentes em Python:

```text
test_scanner_python.py
test_scanner_c.py
```

6. Para a compilação em C foi instalado o MSYS2 UCRT64 e o compilador GCC.

7. O `MANIFESTO.md` menciona o caso `i07_operador_logico_incompleto.minic`, relacionado ao uso isolado dos operadores `&` e `|`. Entretanto, esse conjunto de fixture não estava presente no material utilizado. O scanner implementado possui tratamento para esses símbolos isolados, porém não foi possível validar o nome exato do diagnóstico contra um arquivo `.errors.jsonl` oficial.

---

## 18. Conclusão

A primeira etapa do projeto MINIC foi concluída com a implementação de analisadores léxicos nas linguagens **Python e C**.

A estratégia adotada consistiu inicialmente na implementação do scanner em Python, permitindo validar o comportamento do analisador a partir dos casos de teste fornecidos e dos casos válidos adicionados ao projeto. Após a validação da primeira implementação, a mesma arquitetura foi traduzida para C.

O analisador desenvolvido é capaz de reconhecer palavras reservadas, identificadores, diferentes tipos de literais, operadores e delimitadores, além de ignorar comentários e manter informações de linha e coluna.

Também foram implementados mecanismos de diagnóstico e recuperação para os erros léxicos especificados pelos casos inválidos.

A validação estrutural dos fixtures resultou em **794 tokens esperados verificados**. Posteriormente, os scanners Python e C foram submetidos a um conjunto de **18 casos automatizados**, incluindo casos válidos, inválidos e programas completos.

Tanto a implementação Python quanto a implementação C obtiveram:

```text
18 OK
0 falharam
0 avisos
```

Os resultados demonstram que ambas as implementações atendem ao contrato léxico definido para esta etapa do projeto e produzem saídas compatíveis com os fixtures utilizados.

Dessa forma, a etapa de análise léxica fornece uma base validada para a continuidade do desenvolvimento do compilador MINIC nas etapas posteriores.

---

## 19. Referências

MICROSOFT. **Visual Studio Code – C/C++ Documentation**. Documentação oficial do Visual Studio Code.

MSYS2. **MSYS2 Documentation**. Documentação oficial do ambiente MSYS2 e do toolchain UCRT64.

FREE SOFTWARE FOUNDATION. **GCC – GNU Compiler Collection**. Documentação oficial do compilador GCC.

Material didático e especificação da linguagem MINIC disponibilizados na disciplina.
