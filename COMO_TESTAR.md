# Como testar o compilador MINIC

Este guia mostra como executar os testes do compilador MINIC em cada pasta de testes do projeto.

## 1. Clonar o projeto

Escolha o diretório onde deseja salvar o projeto e rode:

```bash
git clone https://github.com/BrazTec/MINIC-compiler.git
cd MINIC-compiler
```

## 2. Preparação no Git Bash (Windows)

Se você estiver no Windows usando Git Bash, configure o alias do Python em cada nova sessão antes de executar os testes. Isso evita falhas causadas por diferenças de ambiente entre Linux e Windows.

```bash
python3() {
  python "$@"
}
export -f python3
```

> Importante: execute esse bloco toda vez que abrir uma nova sessão do Git Bash.

---

## 3. Testando a pasta `testes-scanner-minic`

Acesse a pasta do projeto:

```bash
cd <Caminho_da_sua_maquina>\MINIC-compiler\testes-scanner-minic
```

Substitua `<Caminho_da_sua_maquina>` pelo caminho real do seu ambiente.

### Analisador léxico

#### Python

```bash
./test_scanner_python.sh ./scanner.py ./casos-validos/
./test_scanner_python.sh ./scanner.py ./casos-invalidos/
```

#### C

```bash
./test_scanner_c.sh ./scanner.c ./casos-validos/
./test_scanner_c.sh ./scanner.c ./casos-invalidos/
```

### Analisador sintático (parser)

#### Python

```bash
./testar_parser_python.sh ./testes-parser-50/ ./parser.py
```

#### C

```bash
./testar_parser_c.sh ./testes-parser-50/ ./parser.c
```

### Analisador semântico

#### Python

```bash
./testes_semanticos_py.sh ./minic.py ./minic-testes-semanticos/
```

#### C

```bash
./testes_semanticos_c.sh ./minic.c ./minic-testes-semanticos/
```

---

## 4. Testando a pasta `testes-scanner-minic_codes`

Acesse a segunda pasta de testes:

```bash
cd <Caminho_da_sua_maquina>\MINIC-compiler\testes-scanner-minic_codes
```

### Analisador léxico

#### Python

```bash
./test_scanner_python.sh ./scanner.py ./casos-validos/
./test_scanner_python.sh ./scanner.py ./casos-invalidos/
```

#### C

```bash
./test_scanner_c.sh ./scanner.c ./casos-validos/
./test_scanner_c.sh ./scanner.c ./casos-invalidos/
```

### Analisador sintático (parser)

#### Python

```bash
./testar_parser_python.sh ./testes-parser-50/ ./parser.py
```

#### C

```bash
./testar_parser_c.sh ./testes-parser-50/ ./parser.c
```

### Analisador semântico

#### Python

```bash
./testes_semanticos_py.sh ./minic.py ./minic-testes-semanticos/
```

#### C

```bash
./testes_semanticos_c.sh ./minic.c ./minic-testes-semanticos/
```

---

## Dica final

- Sempre execute o alias `python3()` antes de rodar os testes em uma nova sessão do Git Bash.
- Se um comando falhar, confirme se o diretório atual está correto e se o nome do script/arquivo está exatamente como mostrado.
- Para facilitar, você pode manter o alias em um arquivo de configuração do shell e carregá-lo automaticamente quando abrir o Git Bash.


