#!/usr/bin/env python3
"""Valida propriedades de O01..O10, sem alterar scripts/gabaritos oficiais.

Confere compilacao C real, equivalencia PY/C em O0/O1, execucao da TAC
com VM independente e predicados especificos para cada transformacao.
Inclui caso simbolico para demonstrar CSE sem mascaramento pelo folding.
"""
from __future__ import annotations
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

from validar_ir_independente import IRMachine

ROOT = Path(__file__).resolve().parent
BASE = ROOT / 'minic-ir-test-cases' / 'testes' / '03-otimizacoes'
OUTPUTS = {
 'O01': ['20'], 'O02': ['13'], 'O03': ['6'], 'O04': ['9'],
 'O05': ['22','23'], 'O06':['10'], 'O07':['11'], 'O08':['168'],
 'O09':['0','2','4','6','8'], 'O10':['4'],
}

def run(cmd):
    env = dict(os.environ, PYTHONIOENCODING='utf-8', PYTHONUTF8='1')
    p = subprocess.run(cmd,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=15,env=env)
    return p.returncode,p.stdout.replace(b'\r\n',b'\n').decode('utf-8'),p.stderr.replace(b'\r\n',b'\n').decode('utf-8')

def predicate(case, ir):
    lines=ir.splitlines()
    if case=='O01': return 'print 20' in ir and not any(' + ' in x or ' * ' in x for x in lines)
    if case=='O02': return 'print 13' in ir and ' + 0' not in ir and ' * 1' not in ir and not re.search(r'\b[ab] =',ir)
    if case=='O03': return not re.search(r'\b(copia|final) =', ir)
    if case=='O04': return 'temporario =' not in ir
    if case=='O05': return 'print 22' in ir and 'print 23' in ir # mais: CSE simbólica abaixo
    if case=='O06': return 'print 10' in ir
    if case=='O07': return 'print 11' in ir and 'r = 22' not in ir and 'ifFalse true' not in ir
    if case=='O08':
        header=next((i for i,line in enumerate(lines) if re.fullmatch(r'L\d+_while:',line)),None)
        return header is not None and any(('42' in line or 'x * y' in line) and '=' in line for line in lines[:header]) and not any('x * y' in line for line in lines[header:])
    if case=='O09': return 'i + i' in ir and 'i * 2' not in ir
    if case=='O10': return 'return 4\nendfunc' in ir and 'print 99' not in ir
    return False

def main():
    gcc=shutil.which('gcc')
    if not gcc:
        print('ERRO: GCC indisponível',file=sys.stderr);return 2
    with tempfile.TemporaryDirectory(prefix='minic-otimizacoes-') as temp:
        binary=Path(temp)/('minic-test.exe' if os.name=='nt' else 'minic-test')
        p=subprocess.run([gcc,'-Wall','-Wextra','-std=c11','minic.c','-o',str(binary)],cwd=ROOT,capture_output=True,text=True)
        if p.returncode:
            print(p.stderr,file=sys.stderr);return 2
        failures=[]
        for case,expected in OUTPUTS.items():
            source=BASE/case/(case+'.c')
            outputs={}
            reasons=[]
            for lang,exe in [('Python',[sys.executable,str(ROOT/'minic.py')]),('C',[str(binary)])]:
                for mode in ('-O0','-O1'):
                    try: rc,ir,err=run(exe+['--ir',mode,str(source)])
                    except Exception as exc:
                        reasons.append(f'{lang}/{mode}: {exc}');continue
                    outputs[lang,mode]=(rc,ir,err)
                    if rc or err: reasons.append(f'{lang}/{mode}: rc={rc}, stderr={err[:100]!r}')
                    if not rc:
                        try:
                            observed=IRMachine(ir).execute()
                            if observed!=expected: reasons.append(f'{lang}/{mode}: execução {observed}, esperado {expected}')
                        except Exception as exc: reasons.append(f'{lang}/{mode}: TAC inválida: {exc}')
                    if mode=='-O1' and not predicate(case,ir):
                        reasons.append(f'{lang}: transformação não observada em {case}')
            for mode in ('-O0','-O1'):
                if outputs.get('Python',mode)!=outputs.get('C',mode): reasons.append(f'saída Python/C divergente em {mode}')
            if reasons:
                failures.append(case)
                print(f'[FALHA] {case}: {"; ".join(reasons)}',flush=True)
            else: print(f'[OK] {case}: TAC -O0/-O1 e transformação-alvo verificadas (Python/C)',flush=True)
        # Teste simbólico prova CSE de verdade: a*b é desconhecido na função,
        # portanto folding sozinho não elimina a duplicação da expressão.
        case_source=Path(temp)/'cse_simbolica.c'
        case_source.write_text('int calcular(int a, int b){ int x = a * b; int y = a * b; print(x+y); return 0; }\nint main(){ calcular(3,7); return 0; }\n',encoding='utf-8')
        for lang,exe in [('Python',[sys.executable,str(ROOT/'minic.py')]),('C',[str(binary)])]:
            rc,ir,err=run(exe+['--ir','-O1',str(case_source)])
            if rc or len(re.findall(r'\b[ab] \* [ab]\b',ir))!=1 or IRMachine(ir).execute()!=['42']:
                failures.append('CSE-simbolica-'+lang)
                print(f'[FALHA] CSE simbólica {lang}: retorno={rc}\n{ir}\n{err}',flush=True)
            else: print(f'[OK] CSE simbólica {lang}: multiplicação reaproveitada',flush=True)
        print(f'RESULTADO OTIMIZACOES O01-O10: {10-len([x for x in failures if x in OUTPUTS])}/10; falhas={len(failures)}')
        return 1 if failures else 0

if __name__=='__main__': raise SystemExit(main())
