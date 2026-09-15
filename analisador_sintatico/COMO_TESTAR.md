# Como testar o analisador sintático

## Como testar o analisador sintático em Python

1. O parser Python em `parser.py` recebe um arquivo de entrada em C e, quando a entrada é válida, produz a AST esperada.
2. Para testar um caso isolado no Python, rode: `python parser.py caminho/para/codigo.c`.
3. Exemplo: `python .\parser.py .\testes-parser-50\casos\01_declara_o_inteira_simples\codigo.c`.
4. Se a entrada for válida, o programa imprime a AST no formato S-expression; se for inválida, ele rejeita a entrada e retorna erro.
5. Para verificar todos os casos do conjunto em Python, use o script auxiliar: `python .\executa_todos_casos.py`.
6. Esse script executa automaticamente cada caso da pasta `testes-parser-50\casos` e gera um arquivo chamado `executa_todos_casos_python.md` com os resultados.

## Como testar o analisador sintático em C

1. O parser em C segue a mesma ideia da versão Python: ele recebe um arquivo de entrada em C e valida a sintaxe.
2. Para testar um caso isolado em C, rode: `parser.exe caminho/para/codigo.c`.
3. Exemplo no Windows: `.\parser.exe .\testes-parser-50\casos\01_declara_o_inteira_simples\codigo.c`.
4. Se a entrada for válida, o programa imprime a AST no formato S-expression; se for inválida, ele rejeita a entrada e retorna erro.
5. Para facilitar o processo de testar todos os casos em C, use o executável auxiliar: `.\executa_todos_casos_c.exe`.
6. Esse executável percorre os testes do diretório `testes-parser-50\casos` e gera um arquivo chamado `executa_todos_casos_c.md` com a execução de cada caso.
