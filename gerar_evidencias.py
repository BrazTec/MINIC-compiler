#!/usr/bin/env python3
"""Registra uma reexecução independente das suítes MINIC SEM alterar os .sh.

Uso (na raiz do projeto, com bash e gcc no PATH):
    python3 gerar_evidencias.py

As saídas e códigos são registrados em docs/evidencia/.
Os scripts oficiais e os gabaritos são somente lidos/executados.
"""
from __future__ import annotations

from dataclasses import dataclass
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

ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "testes-scanner-minic_codes"
EVIDENCE = ROOT / "docs" / "evidencia"
MANIFEST = ROOT / "docs" / "SHA256_TESTADORES_OFICIAIS.txt"

@dataclass
class Run:
    title: str
    filename: str
    args: tuple[str, ...]
    expect: str = ""
    needs_gcc: bool = False
    preflight: str | None = None

CASES = (
    Run("Fixtures JSONL", "01_check_fixtures.txt", (sys.executable, "check_fixtures.py"), "794 tokens"),
    Run("Scanner Python", "01_scanner_python.txt", ("bash", "test_scanner_python.sh"), "18 OK"),
    Run("Scanner C", "01_scanner_c.txt", ("bash", "test_scanner_c.sh"), "18 OK", True, "scanner.c"),
    Run("Parser Python", "02_parser_python.txt", ("bash", "testar_parser_python.sh", "./testes-parser-50", "./parser.py"), "50 CASOS PASSARAM"),
    Run("Parser C", "02_parser_c.txt", ("bash", "testar_parser_c.sh", "./testes-parser-50", "./parser.c"), "50 CASOS PASSARAM", True, "parser.c"),
    Run("Semântico Python", "03_semantico_python.txt", ("bash", "testes_semanticos_py.sh", "minic.py", "./minic-testes-semanticos"), "20/20 aprovados"),
    Run("Semântico C", "03_semantico_c.txt", ("bash", "testes_semanticos_c.sh", "minic.c", "./minic-testes-semanticos"), "20/20 aprovados", True, "minic.c"),
    Run("IR Python — script oficial", "04_ir_script_oficial_python.txt", ("bash", "minic-ir-test-cases/test_ir_python.sh", "minic.py", "./minic-ir-test-cases/testes")),
    Run("IR C — script oficial", "04_ir_script_oficial_c.txt", ("bash", "minic-ir-test-cases/test_ir_c.sh", "minic.c", "./minic-ir-test-cases/testes"), needs_gcc=True, preflight="minic.c"),
    Run("IR — validação complementar", "04_ir_validacao_independente.txt", (sys.executable, "validar_ir_independente.py"), "30/30", True, "minic.c"),
    Run("IR — equivalência Python/C -O0/-O1", "04_ir_equivalencia_c_python.txt", (sys.executable, "validar_equivalencia_ir.py"), "60/60 comparações", True, "minic.c"),
    Run("IR — casos isolados (README)", "04_ir_casos_isolados.txt", (sys.executable, "validar_ir_por_caso.py"), "RESULTADO IR ISOLADO: 30/30", True, "minic.c"),
    Run("IR — otimizacoes O01-O10", "04_ir_otimizacoes_o01_o10.txt", (sys.executable, "validar_otimizacoes_o01_o10.py"), "RESULTADO OTIMIZACOES O01-O10: 10/10", True, "minic.c"),
)

def exec_capture(args: tuple[str, ...], env: dict[str, str], *, timeout: int = 300):
    try:
        proc = subprocess.run(args, cwd=SOURCE, env=env, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, timeout=timeout)
        return proc.returncode, proc.stdout.decode('utf-8', errors='replace')
    except (OSError, subprocess.TimeoutExpired) as exc:
        return 127, f"ERRO AO EXECUTAR: {exc}\n"

def assert_sh_integrity():
    errors=[]
    total=0
    for line in MANIFEST.read_text(encoding='utf-8').splitlines():
        if not line or line.startswith('#'):
            continue
        digest, relative = line.split('  ',1)
        f=ROOT/relative
        total+=1
        if not f.is_file() or sha256(f.read_bytes()).hexdigest()!=digest:
            errors.append(relative)
    return total, errors

def main():
    EVIDENCE.mkdir(parents=True,exist_ok=True)
    # O testador oficial Python da Etapa 2 exige que parser.py seja executável.
    # Apenas a permissão do parser.py é ajustada; nenhum arquivo .sh é alterado.
    py_parser=SOURCE / "parser.py"
    if py_parser.is_file() and os.name != 'nt' and not os.access(py_parser, os.X_OK):
        py_parser.chmod(py_parser.stat().st_mode | 0o111)
    total, errors=assert_sh_integrity()
    if errors:
        print('ABORTADO: scripts oficiais diferentes do manifesto SHA-256:')
        print('\n'.join(errors))
        return 2
    env=os.environ.copy()
    env['PYTHONIOENCODING']='utf-8'
    env['PYTHONUTF8']='1'
    env.setdefault('LANG','C.UTF-8')
    env.setdefault('LC_ALL','C.UTF-8')
    timestamp=datetime.now(timezone.utc).isoformat(timespec='seconds')
    compiler=shutil.which('gcc',path=env.get('PATH'))
    bash=shutil.which('bash',path=env.get('PATH'))
    intro=(f"Data/hora UTC: {timestamp}\n"
           f"Sistema: {platform.platform()}\n"
           f"Python: {sys.version.splitlines()[0]}\n"
           f"Bash: {bash or 'NÃO DISPONÍVEL'}\n"
           f"GCC: {compiler or 'NÃO DISPONÍVEL'}\n"
           f"Diretório analisado: {SOURCE}\n"
           f"Testadores .sh íntegros (SHA-256): {total}/{total}\n")
    if compiler:
        code, out=exec_capture(('gcc','--version'),env,timeout=20)
        intro += f"gcc --version (status {code}): {out.splitlines()[0] if out else '(sem saída)'}\n"
    (EVIDENCE/'00_AMBIENTE_E_INTEGRIDADE.txt').write_text(intro,encoding='utf-8',newline='\n')
    print(intro)
    results=[]
    with tempfile.TemporaryDirectory(prefix='minic-preflight-') as tmp:
        checked={}
        def compile_check(filename):
            if filename in checked:return checked[filename]
            target=Path(tmp)/(Path(filename).stem + '.exe' if os.name=='nt' else Path(filename).stem)
            cmd=('gcc','-Wall','-Wextra','-std=c11',filename,'-o',str(target))
            code,out=exec_capture(cmd,env)
            checked[filename]=(code,cmd,out)
            return checked[filename]
        for case in CASES:
            detail=''
            precheck_ok=True
            if case.needs_gcc:
                if compiler is None:
                    precheck_ok=False
                    detail='GCC ausente no PATH; teste C NÃO executado para evitar falso positivo com binário antigo.\n'
                elif case.preflight:
                    code, cmd, out=compile_check(case.preflight)
                    detail=(f"PRÉ-COMPILAÇÃO: {' '.join(cmd)}\n"
                            f"STATUS PRÉ-COMPILAÇÃO: {code}\n{out}\n")
                    precheck_ok=(code==0)
            if not precheck_ok:
                status='NÃO EXECUTADO: falha ou ausência de GCC'
                result_code=125
                output=detail
            else:
                run_env = env.copy()
                # Os testadores oficiais de scanner usam PASS/FAIL/WARN na
                # contagem, mas inicializam PASSED/FAILED/SKIPPED. No Git Bash,
                # printf com FAIL/WARN vazios pode retornar 1 e interromper o
                # script por causa de set -e. Inicializamos apenas o ambiente
                # do processo filho, SEM modificar os arquivos .sh originais.
                scanner_env_fix = case.title in ('Scanner Python', 'Scanner C')
                if scanner_env_fix:
                    run_env.update(PASS='0', FAIL='0', WARN='0')
                    detail += ('AMBIENTE DO TESTADOR: PASS=0 FAIL=0 WARN=0 '
                               '(contadores inicializados externamente; .sh intacto)\n')
                result_code, output=exec_capture(case.args,run_env)
                ok=(result_code==0 and (case.expect.lower() in output.lower() if case.expect else True))
                status='APROVADO NO TESTADOR' if ok else 'REPROVADO / EXIGE INVESTIGAÇÃO'
            log=(f"ETAPA: {case.title}\nDATA_UTC: {timestamp}\n"
                 f"AMBIENTE: {platform.platform()}\n"
                 f"COMANDO: {' '.join(case.args)}\n"
                 f"RESULTADO DE EXECUÇÃO: {status}\n\n"
                 f"{detail if precheck_ok else ''}"
                 f"SAÍDA COMPLETA:\n{output.rstrip()}\n"
                 f"\nEXIT_STATUS: {result_code}\n")
            (EVIDENCE/case.filename).write_text(log,encoding='utf-8',newline='\n')
            results.append((case.title,status,result_code))
            print(f"{case.title}: {status} (exit {result_code})",flush=True)
    summary='RESUMO DA REVALIDAÇÃO DO MINIC\n'+'='*52+'\n'+intro+'\n'
    for title,status,exit_code in results:
        summary+=f"{title}: {status} (status={exit_code})\n"
    summary+=('''\nNOTAS DE INTERPRETAÇÃO
- As aprovações do scanner/parser/semântico decorrem de seus próprios testadores.
- O pré-teste GCC impede que binários C anteriores mascarem erros de compilação.
- O script complementar de IR não equivale aos testadores IR oficiais.
- Os resultados oficiais de IR são documentados mesmo quando falham.
- Os scripts .sh e os gabaritos foram apenas lidos/executados; não alterados.
''')
    (EVIDENCE/'00_RESUMO_DE_VALIDACAO.txt').write_text(summary,encoding='utf-8',newline='\n')
    failed=[t for t,s,c in results if s!='APROVADO NO TESTADOR']
    print(f"\nTestes/ações não aprovados: {len(failed)}. Consultar 00_RESUMO_DE_VALIDACAO.txt.")
    # Coleta concluída, mas manter status de erro se algum testador não aprovou.
    # Na Etapa 4, a falha oficial é uma pendência documental/técnica conhecida.
    return 1 if failed else 0

if __name__=='__main__':
    raise SystemExit(main())
