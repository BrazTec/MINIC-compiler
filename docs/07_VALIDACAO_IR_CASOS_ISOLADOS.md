# Etapa 4 — Evidências isoladas por caso

## Motivação

O `README.md` da suíte `minic-ir-test-cases/` orienta avaliar separadamente
programas válidos sem otimização (`-O0`), programas inválidos (rejeição
pela categoria) e programas de otimização em `-O0` e `-O1`. Os scripts oficiais
`test_ir_python.sh` e `test_ir_c.sh` são preservados **sem alterações**;
a comparação byte a byte deles não é equivalente à metodologia descrita
no README, que permite TACs equivalentes com temporários/rótulos diferentes.

## Executar

Na raiz `Compilador MINIC/`, com Python, bash e GCC disponíveis:

```bash
python3 gerar_evidencias.py
```

Dentro de `testes-scanner-minic_codes/` pode-se executar diretamente:

```bash
python3 validar_ir_por_caso.py
python3 validar_ir_por_caso.py --case O01
python3 validar_equivalencia_ir.py
python3 validar_ir_independente.py
```

**Não modificar os scripts `.sh` oficiais nem os arquivos `.txt` da suíte.**

## Arquivos produzidos

Em `docs/evidencia/04_ir_por_caso/`:

- `validos/P01.txt` a `P10.txt`: comandos e saídas Python/C em `-O0`,
  código de retorno, IR parseável e execução da TAC.
- `erros/E01.txt` a `E10.txt`: rejeição, categoria extraída do gabarito
  (`2` léxico, `3` sintático, `4` semântico), presença de diagnóstico e
  ausência de IR executável.
- `otimizacoes/O01.txt` a `O10.txt`: TAC Python/C em `-O0` e `-O1`,
  efeitos observáveis em cada modo, indicação de alteração textual e
  quantidade de instruções.
- `00_COMPILACAO_C.txt`: comprovação de recompilação independente.
- `00_RESUMO_IR_ISOLADO.txt`: total de casos aprovados nos critérios
  automatizados e ressalvas metodológicas.

O gerador principal ainda registra `04_ir_script_oficial_python.txt`,
`04_ir_script_oficial_c.txt`, `04_ir_validacao_independente.txt` e
`04_ir_equivalencia_c_python.txt`; resultados conflitantes não são ocultados.

## Alcance metodológico (leitura obrigatória)

**Aprovação nos testes isolados não significa conformidade estrutural total
com cada exemplo de TAC do gabarito, nem prova de todas as otimizações-alvo.**
Os critérios automatizados deste verificador abrangem geração, rejeição,
categoria de erro, parse/execução da TAC, equivalência de efeitos `-O0`/`-O1`
e comparação entre Python e C. Um otimizador pode preservar o comportamento
sem realizar, por exemplo, movimentação de código de laço (O08) ou eliminação
de subexpressão comum (O05). A existência/ausência de uma instrução ou a
simples mudança de comprimento da TAC não basta para certificar isso.

Para completar a avaliação acadêmica das otimizações, confrontar a TAC
por caso com a transformação indicada no `Oxx.txt`, respeitando a liberdade
de representação prevista no README. No caso O09, redução de força é opcional.

A validação complementar também inclui execução interpretada da TAC e
confronto de comportamento com C nativo (`validar_ir_independente.py`).

## Interpretação do `exit 1` no gerador principal

Se os `test_ir_*.sh` oficiais continuarem divergindo por comparação textual,
o gerador pode encerrar com `exit 1` mesmo com `30/30` isolados. Os resultados
devem ser comunicados **separadamente**, sem afirmar que os scripts oficiais
foram aprovados.

## Auditoria estrutural O01–O10 (nova)

```bash
python3 validar_otimizacoes_o01_o10.py
```

O teste verifica predicados específicos para as dez transformações, compara
as TACs Python/C em -O0 e -O1, interpreta a TAC e ainda roda um cenário CSE
simbólico (`a*b` em uma função com parâmetros) no qual a dobra de constantes
não é capaz de mascarar a reutilização. Os resultados são registrados pelo
gerador principal em `docs/evidencia/04_ir_otimizacoes_o01_o10.txt`.
O09 é realizado como `i*2 -> i+i`, não por atualização incremental.
A propagação geral interblocos/reaching definitions permanece fora do escopo.
