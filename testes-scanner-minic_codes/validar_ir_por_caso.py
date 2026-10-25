#!/usr/bin/env python3
"""Evidências isoladas da Etapa 4 de acordo com o README da suíte MINIC.

NÃO edita scripts .sh, gabaritos ou fontes. Examina separadamente:
  P01-P10: TAC válida em -O0;
  E01-E10: rejeição com categoria/código previsto no gabarito;
  O01-O10: TAC em -O0 e -O1, com execução e efeitos equivalentes.

Python e C são avaliados por caso. Diferença de rótulos/temporários não
constitui, por si só, erro: a comparação textual C/Python é complementar e
não é uma comparação textual com o gabarito do professor.

A comparação de efeitos e os sinais de alteração da IR NÃO demonstram a
implementação de toda otimização-alvo individual; ver relatório por caso.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
from hashlib import sha256
from pathlib import Path
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile

SOURCE = Path(__file__).resolve().parent
PROJECT = SOURCE.parent
TESTS = SOURCE / 'minic-ir-test-cases' / 'testes'
EVIDENCE = PROJECT / 'docs' / 'evidencia' / '04_ir_por_caso'
GROUPS = (
    ('P', '01-validos-sem-otimizacao', 'validos'),
    ('E', '02-erros', 'erros'),
    ('O', '03-otimizacoes', 'otimizacoes'),
)


def _run(cmd: list[str], *, timeout: int = 12) -> tuple[int, str, str]:
    env = os.environ.copy()
    env.update(PYTHONUTF8='1', PYTHONIOENCODING='utf-8')
    try:
        p = subprocess.run(cmd, cwd=SOURCE, env=env, stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE, timeout=timeout)
        return (p.returncode,
                p.stdout.decode('utf-8', 'replace').replace('\r\n', '\n'),
                p.stderr.decode('utf-8', 'replace').replace('\r\n', '\n'))
    except subprocess.TimeoutExpired as exc:
        return 124, '', f'ERRO: tempo limite de {timeout}s: {exc}\n'
    except OSError as exc:
        return 127, '', f'ERRO: não foi possível executar: {exc}\n'


def _reference_code(ref: str) -> int:
    match = re.search(r'c[oó]digo de sa[ií]da\s+(\d+)', ref, flags=re.IGNORECASE)
    if not match:
        raise ValueError('gabarito sem categoria/código de saída reconhecível')
    return int(match.group(1))


def _line_count(source: str) -> int:
    return sum(bool(line.strip()) for line in source.splitlines())


def _record(case_id: str, group: str, source: Path, expected: Path,
            observations: list[str], captures: list[tuple[str, list[str], tuple[int, str, str]]],
            failures: list[str]) -> None:
    lines = [f'CASO: {case_id}', f'CATEGORIA: {group}',
             f'ARQUIVO: {source.relative_to(PROJECT).as_posix()}',
             f'GABARITO: {expected.relative_to(PROJECT).as_posix()}',
             f'SHA256_GABARITO: {sha256(expected.read_bytes()).hexdigest()}',
             f'DATA_UTC: {datetime.now(timezone.utc).isoformat(timespec="seconds")}',
             f'PLATAFORMA: {platform.platform()}',
             f'RESULTADO: {"APROVADO NOS CRITÉRIOS AUTOMATIZADOS" if not failures else "EXIGE INVESTIGAÇÃO"}',
             '', 'VERIFICAÇÕES REALIZADAS:']
    lines.extend(f'  - {ob}' for ob in observations)
    if failures:
        lines.extend(['', 'DIVERGÊNCIAS:'] + [f'  - {f}' for f in failures])
    lines.extend(['', 'ATENÇÃO: este relatório não confunde equivalência de execução com',
                  'comprovação de cada transformação-alvo de otimização. A IR canônica',
                  'do professor é referência estrutural, não saída a reproduzir literalmente.'])
    for name, cmd, (rc, stdout, stderr) in captures:
        lines.extend(['', '=' * 75, f'EXECUÇÃO: {name}',
                      'COMANDO: ' + ' '.join(cmd), f'EXIT_STATUS: {rc}',
                      'STDOUT:', stdout.rstrip('\n') or '(vazio)',
                      'STDERR:', stderr.rstrip('\n') or '(vazio)'])
    outfile = EVIDENCE / group / f'{case_id}.txt'
    outfile.parent.mkdir(parents=True, exist_ok=True)
    outfile.write_text('\n'.join(lines).rstrip()+'\n', encoding='utf-8', newline='\n')


def main() -> int:
    global EVIDENCE
    ap = argparse.ArgumentParser(description='Avaliar casos isolados IR e salvar evidências por caso')
    ap.add_argument('--case', metavar='P01|E01|O01', help='executar somente um caso')
    ap.add_argument('--evidencias', type=Path, default=EVIDENCE,
                    help='pasta de saída de evidências (uso padrão: docs/evidencia/04_ir_por_caso)')
    args = ap.parse_args()
    EVIDENCE = args.evidencias.resolve()

    if args.case and not re.fullmatch(r'[PEO](?:0[1-9]|10)', args.case.upper()):
        ap.error('--case deve ser P01..P10, E01..E10 ou O01..O10')
    selected = args.case.upper() if args.case else None
    if not shutil.which('gcc'):
        print('ERRO: GCC indisponível no PATH. No Git Bash: export PATH="/c/msys64/ucrt64/bin:$PATH"')
        return 2
    try:
        from validar_ir_independente import IRMachine
    except ImportError as exc:
        print(f'ERRO: não foi possível importar a máquina de validação TAC: {exc}')
        return 2

    expected_cases = [(prefix + f'{i:02d}', group, dirname)
                      for prefix, dirname, group in GROUPS for i in range(1, 11)
                      if selected is None or selected == prefix + f'{i:02d}']
    for case_id, group, dirname in expected_cases:
        s = TESTS / dirname / case_id / (case_id+'.c')
        ref = s.with_suffix('.txt')
        if not s.is_file() or not ref.is_file():
            print(f'ERRO: arquivos ausentes em {s.parent}')
            return 2

    EVIDENCE.mkdir(parents=True,exist_ok=True)
    success = {group: 0 for _, _, group in GROUPS}
    counts = {group: 0 for _, _, group in GROUPS}
    changed = []
    all_issues = []
    with tempfile.TemporaryDirectory(prefix='minic-ir-casos-') as folder:
        binary = Path(folder) / ('minic_ir_casos.exe' if os.name == 'nt' else 'minic_ir_casos')
        compile_cmd = ['gcc', '-Wall','-Wextra','-std=c11','minic.c', '-o', str(binary)]
        compiled = _run(compile_cmd, timeout=40)
        (EVIDENCE / '00_COMPILACAO_C.txt').write_text(
            'COMANDO: '+' '.join(compile_cmd)+'\n'
            + f'EXIT_STATUS: {compiled[0]}\nSTDOUT:\n{compiled[1]}\nSTDERR:\n{compiled[2]}\n',
            encoding='utf-8', newline='\n')
        if compiled[0] != 0:
            print('ERRO: compilação C falhou. Consulte 00_COMPILACAO_C.txt')
            return 2

        for case_id, group, dirname in expected_cases:
            counts[group] += 1
            source = TESTS / dirname / case_id / (case_id + '.c')
            reference = source.with_suffix('.txt')
            reference_text = reference.read_text(encoding='utf-8')
            modes = ('-O0','-O1') if group=='otimizacoes' else ('-O0',)
            captures=[]
            observations=[]
            problems=[]
            vals={}
            for mode in modes:
                for language, entry in (('Python', [sys.executable, 'minic.py']),
                                        ('C', [str(binary)])):
                    cmd = entry + ['--ir', mode, str(source)]
                    res = _run(cmd)
                    captures.append((f'{language} {mode}', cmd, res))
                    vals[(language,mode)] = res
                    code, stdout, stderr = res
                    if group=='erros':
                        expected_exit = _reference_code(reference_text)
                        if code != expected_exit:
                            problems.append(f'{language} {mode}: esperado código de rejeição {expected_exit}, obtido {code}')
                        if not (stdout.strip() or stderr.strip()):
                            problems.append(f'{language} {mode}: ausência de diagnóstico')
                        if re.search(r'^\s*func\s+', stdout, flags=re.MULTILINE):
                            problems.append(f'{language} {mode}: gerou IR executável apesar do erro')
                    else:
                        if code != 0:
                            problems.append(f'{language} {mode}: programa válido rejeitado (exit {code})')
                        else:
                            try:
                                vm = IRMachine(stdout)
                                vals[(language,mode,'efeitos')] = vm.execute()
                                observations.append(f'{language} {mode}: TAC parseável/executável; '
                                                    f'{_line_count(stdout)} instruções/linhas; '
                                                    f'efeitos observáveis={vm.output!r}')
                            except Exception as exc:
                                problems.append(f'{language} {mode}: TAC inválida ou não executável: {exc}')
                if vals[('Python',mode)] != vals[('C',mode)]:
                    observations.append(f'{mode}: diferenças textuais/diagnósticas entre Python e C; '
                                        'verificar equivalência semântica')
                else:
                    observations.append(f'{mode}: saída e código de retorno iguais em Python e C')
                if group != 'erros':
                    a = vals.get(('Python', mode, 'efeitos'))
                    b = vals.get(('C', mode, 'efeitos'))
                    if a is not None and b is not None and a != b:
                        problems.append(f'{mode}: efeitos divergentes Python={a!r}, C={b!r}')
            if group=='erros':
                observations.append('Categoria exigida no gabarito: retorno '+str(_reference_code(reference_text)))
                observations.append('Diagnóstico conferido por presença/rejeição, não por redação idêntica')
            if group=='otimizacoes':
                for language in ('Python','C'):
                    z=vals.get((language,'-O0','efeitos'))
                    o=vals.get((language,'-O1','efeitos'))
                    if z is not None and o is not None and z!=o:
                        problems.append(f'{language}: efeitos diferentes entre -O0 {z!r} e -O1 {o!r}')
                    o0=vals[(language,'-O0')][1]
                    o1=vals[(language,'-O1')][1]
                    changed_txt='ALTERADA' if o0!=o1 else 'IGUAL'
                    observations.append(f'{language}: IR -O1 {changed_txt} em relação a -O0; '
                                        f'linhas -O0={_line_count(o0)}, -O1={_line_count(o1)}')
                    if o0!=o1:
                        changed.append((case_id,language))
                observations.append('Transformação-alvo específica: requer confronto estrutural/inspeção; '
                                    'saída alterada não garante aplicação da otimização nomeada no gabarito')
            _record(case_id,group,source,reference,observations,captures,problems)
            if problems:
                all_issues.append((case_id,problems))
                print(f'[FALHA][{group}] {case_id}: {"; ".join(problems)}',flush=True)
            else:
                success[group] += 1
                print(f'[OK][{group}] {case_id} — evidência individual gerada',flush=True)
    total_ok=sum(success.values())
    total=sum(counts.values())
    summary = ['EVIDÊNCIAS ISOLADAS ETAPA 4 — MINIC',
               f'Data/hora UTC: {datetime.now(timezone.utc).isoformat(timespec="seconds")}',
               f'Diretório: {EVIDENCE}',
               f'PLATAFORMA: {platform.platform()}',
               'Critérios: README minic-ir-test-cases/README.md',
               'Arquivos oficiais .sh e .txt NÃO modificados.', '']
    for prefix, dirname, group in GROUPS:
        summary.append(f'{group.upper()}: {success[group]}/{counts[group]} critérios automatizados aprovados')
    summary += ['', f'RESULTADO ISOLADO: {total_ok}/{total}; reprovados={total-total_ok}',
                '', 'OTIMIZAÇÕES — sinais observados (NÃO avaliação completa das transformações):']
    summary.extend(f'  {case_id} {lang}: IR -O1 difere de -O0' for case_id,lang in changed)
    summary += ['', 'LIMITAÇÕES:',
                '- Os casos P verificam TAC parseável e efeitos equivalentes entre Python e C.',
                '- Os casos E verificam rejeição/categoria e presença de diagnóstico.',
                '- Os casos O verificam execução equivalente nos dois níveis e registram alterações.',
                '- Não há comparação byte a byte com os gabaritos do professor.',
                '- Não é provada automaticamente cada otimização-alvo O01..O10.',
                '- A comparação com C nativo e a avaliação dos 30 exemplos seguem também',
                '  em validar_ir_independente.py; ver a evidência complementar separada.',
                '- A equivalência de saída Python/C semântica não substitui análise de fluxo',
                '  de controle e transformação estrutural, especialmente em loops.',
                '- Os testadores .sh oficiais de IR permanecem preservados e relatados à parte.',
                '']
    if all_issues:
        summary += ['DIVERGÊNCIAS:']
        for case_id, issues in all_issues:
            summary.extend(f'  {case_id}: {item}' for item in issues)
    file=EVIDENCE/'00_RESUMO_IR_ISOLADO.txt'
    file.write_text('\n'.join(summary), encoding='utf-8', newline='\n')
    print(f'RESULTADO IR ISOLADO: {total_ok}/{total}; reprovados={total-total_ok}')
    print('Evidências em:', EVIDENCE)
    return 0 if total_ok==total else 1


if __name__=='__main__':
    raise SystemExit(main())
