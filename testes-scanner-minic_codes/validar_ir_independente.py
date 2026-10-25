#!/usr/bin/env python3
"""Validação adicional de IR: sem modificar gabaritos/testadores oficiais.

Compara saída IR de Python/C, códigos de rejeição, e executa a TAC para
comparar efeitos observáveis entre -O0, -O1 e execução GCC de referência.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


class IRMachine:
    def __init__(self, text: str, step_limit: int = 200000):
        self.functions: dict[str, tuple[list[str], list[str], dict[str, int]]] = {}
        self.output: list[str] = []
        self.steps = 0
        self.step_limit = step_limit
        name, params, body = None, [], []
        for raw in text.splitlines():
            line = raw.strip()
            if not line: continue
            if line.startswith('func '):
                m = re.fullmatch(r'func\s+(\w+)(?:\((.*)\))?', line)
                if not m: raise ValueError('Função inválida: '+line)
                name = m.group(1)
                params = [x.strip() for x in m.group(2).split(',')] if m.group(2) else []
                body = []
            elif line == 'endfunc':
                if name is None: raise ValueError('endfunc sem função')
                labels = {cmd[:-1]: i for i, cmd in enumerate(body) if cmd.endswith(':')}
                self.functions[name] = (params, body, labels)
                name = None
            elif name is not None:
                body.append(line)
            else:
                raise ValueError('Instrução fora de função: '+line)
        if 'main' not in self.functions: raise ValueError('função main ausente')

    @staticmethod
    def show(value):
        if isinstance(value, bool): return 'true' if value else 'false'
        return str(value)

    @staticmethod
    def truthy(value):
        return bool(value)

    def eval(self, text, env):
        if text == 'true': return True
        if text == 'false': return False
        if re.fullmatch(r'-?\d+', text): return int(text)
        if re.fullmatch(r'-?\d+\.\d+', text): return float(text)
        if text in env: return env[text]
        raise RuntimeError(f'uso de variável não inicializada: {text}')

    def binary(self, a, op, b):
        if op == '+': return a+b
        if op == '-': return a-b
        if op == '*': return a*b
        if op == '/': return int(a/b) if isinstance(a,int) and isinstance(b,int) else a/b
        if op == '%': return a%b
        if op == '<': return a<b
        if op == '<=': return a<=b
        if op == '>': return a>b
        if op == '>=': return a>=b
        if op == '==': return a==b
        if op == '!=': return a!=b
        if op == 'and': return bool(a) and bool(b)
        if op == 'or': return bool(a) or bool(b)
        raise ValueError(f'operador não suportado: {op}')

    def call(self, name, args, depth=0):
        if depth > 64: raise RuntimeError('possível recursão infinita')
        params, lines, labels = self.functions[name]
        if len(params) != len(args): raise RuntimeError('aridade inconsistente')
        env = dict(zip(params,args))
        arrays = {}
        pending = []
        pc = 0
        while pc < len(lines):
            self.steps+=1
            if self.steps > self.step_limit: raise RuntimeError('limite de passos atingido')
            line = lines[pc]
            pc+=1
            if line.endswith(':'): continue
            m = re.fullmatch(r'goto (\S+)',line)
            if m: pc = labels[m.group(1)]; continue
            m = re.fullmatch(r'if(False|True) (\S+) goto (\S+)',line)
            if m:
                cond = bool(self.eval(m.group(2),env))
                if cond == (m.group(1)=='True'): pc=labels[m.group(3)]
                continue
            m = re.fullmatch(r'param (\S+)',line)
            if m: pending.append(self.eval(m.group(1),env)); continue
            m = re.fullmatch(r'print (\S+)',line)
            if m:
                value = self.eval(m.group(1),env)
                self.output.append(self.show(value))
                continue
            if line.startswith('read '): raise RuntimeError('read não exercitado na suíte')
            if line == 'return': return None
            m = re.fullmatch(r'return (\S+)',line)
            if m: return self.eval(m.group(1),env)
            m = re.fullmatch(r'alloc (\S+), (\S+)',line)
            if m:
                arrays[m.group(1)]=[0] * int(self.eval(m.group(2),env))
                continue
            m = re.fullmatch(r'store (\S+)\[(\S+)\], (\S+)',line)
            if m:
                arrays[m.group(1)][int(self.eval(m.group(2),env))]=self.eval(m.group(3),env)
                continue
            m = re.fullmatch(r'call (\w+), (\d+)',line)
            if m:
                count = int(m.group(2))
                self.call(m.group(1),pending[-count:] if count else [],depth+1)
                if count: del pending[-count:]
                continue
            m = re.fullmatch(r'(\S+) = call (\w+), (\d+)',line)
            if m:
                count=int(m.group(3))
                env[m.group(1)]=self.call(m.group(2),pending[-count:] if count else [],depth+1)
                if count:del pending[-count:]
                continue
            m = re.fullmatch(r'(\S+) = load (\S+)\[(\S+)\]',line)
            if m:
                env[m.group(1)]=arrays[m.group(2)][int(self.eval(m.group(3),env))]
                continue
            m = re.fullmatch(r'(\S+) = (\S+) (\S+) (\S+)',line)
            if m:
                env[m.group(1)] = self.binary(self.eval(m.group(2),env),m.group(3),self.eval(m.group(4),env))
                continue
            m = re.fullmatch(r'(\S+) = (not|\-|\+) (\S+)',line)
            if m:
                val=self.eval(m.group(3),env)
                env[m.group(1)]=not val if m.group(2)=='not' else (-val if m.group(2)=='-' else +val)
                continue
            m = re.fullmatch(r'(\S+) = (\S+)',line)
            if m:
                env[m.group(1)]=self.eval(m.group(2),env)
                continue
            raise ValueError(f'instrução desconhecida: {line}')
        return None

    def execute(self):
        self.call('main',[])
        return self.output


def run(command, timeout=6):
    env = dict(os.environ)
    env.update(PYTHONUTF8='1',PYTHONIOENCODING='utf-8')
    return subprocess.run(command, capture_output=True, env=env, timeout=timeout)


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--root',type=Path,default=Path(__file__).resolve().parent)
    ap.add_argument('--gcc',default='gcc')
    args=ap.parse_args()
    root=args.root.resolve()
    suite=root/'minic-ir-test-cases'/'testes'
    source=root/'minic.c'
    if not shutil.which(args.gcc):
        print(f'ERRO: GCC indisponível: {args.gcc}')
        return 2
    cases=sorted(suite.rglob('*.c'))
    failures=[]
    passed=0
    with tempfile.TemporaryDirectory(prefix='minic-ir-') as work:
        exe=Path(work)/'minic-c'
        compiled=run([args.gcc,'-Wall','-Wextra','-std=c11',str(source),'-o',str(exe)],timeout=20)
        if compiled.returncode:
            print(compiled.stderr.decode('utf-8','replace'))
            return 2
        helper = Path(work) / 'minic_stdlib.h'
        helper.write_text('#include <stdio.h>\n#include <stdbool.h>\n#define print(x) printf("%.15g\\n", (double)(x))\n', encoding='utf-8')
        for case in cases:
            group=case.parent.parent.name
            outputs={}
            statuses={}
            issue=[]
            for opt in ('-O0','-O1'):
                p=run([sys.executable,str(root/'minic.py'),'--ir',opt,str(case)])
                c=run([str(exe),'--ir',opt,str(case)])
                outputs[('Python',opt)]=p.stdout
                outputs[('C',opt)]=c.stdout
                statuses[('Python',opt)]=p.returncode
                statuses[('C',opt)]=c.returncode
                if p.returncode != c.returncode:issue.append(f'códigos distintos em {opt}: PY={p.returncode}, C={c.returncode}')
                if opt=='-O0' and p.stdout != c.stdout:issue.append('TAC -O0 diferentes')
                if group=='02-erros':
                    case_n=int(case.stem[1:])
                    expected=3 if case_n==9 else (2 if case_n==10 else 4)
                    if p.returncode!=expected or c.returncode!=expected: issue.append(f'rejeição esperada {expected}')
                    if p.stdout or c.stdout: issue.append('IR foi produzida para programa inválido')
                else:
                    if p.returncode or c.returncode:issue.append('programa válido foi rejeitado')
                    else:
                        for language, payload in [('Python',p.stdout),('C',c.stdout)]:
                            try:
                                vm=IRMachine(payload.decode('utf-8'))
                                outputs[(language,opt,'exec')]=vm.execute()
                            except Exception as exc:
                                issue.append(f'falha ao executar TAC {language} {opt}: {exc}')
            if group!='02-erros':
                # Referência independente: compila e executa o fonte original
                # como C nativo, com implementação mínima da intrínseca print.
                native_exe = Path(work) / ('reference_' + case.stem)
                reference_build = run([args.gcc, '-std=c11', '-include', str(helper),
                                       str(case), '-o', str(native_exe)], timeout=12)
                if reference_build.returncode:
                    issue.append('GCC referência falhou: '+reference_build.stderr.decode('utf-8','replace')[:140])
                else:
                    native = run([str(native_exe)],timeout=7)
                    if native.returncode:
                        issue.append('execução GCC referência falhou')
                    else:
                        reference = native.stdout.decode('utf-8','replace').splitlines()
                        actual = outputs.get(('Python','-O0','exec'))
                        if actual is not None and reference != actual:
                            issue.append(f'TAC -O0 difere do programa C nativo: {actual} != {reference}')
                for language in ['Python','C']:
                    x=outputs.get((language,'-O0','exec'))
                    y=outputs.get((language,'-O1','exec'))
                    if x is not None and y is not None and x!=y:
                        issue.append(f'{language}: efeitos diferentes entre -O0 e -O1: {x} != {y}')
                if outputs.get(('Python','-O0','exec'))!=outputs.get(('C','-O0','exec')):
                    issue.append('efeitos PY/C divergentes')
            if issue:
                failures.append((case.stem, issue))
                print(f'[FALHA] {case.stem}: {"; ".join(issue)}',flush=True)
            else:
                passed+=1
                print(f'[OK] {case.stem} ({group})',flush=True)
    print(f'RESULTADO VALIDACAO INDEPENDENTE: {passed}/{len(cases)}; reprovados={len(failures)}')
    return 1 if failures else 0


if __name__=='__main__':
    raise SystemExit(main())
