#!/usr/bin/env python3
"""Compara os .sh da árvore de trabalho com o manifesto SHA-256 original do ZIP auditado.
Não modifica nenhum arquivo.
"""
from pathlib import Path
from hashlib import sha256
import sys

root = Path(__file__).resolve().parent
manifest = root / 'docs' / 'SHA256_TESTADORES_OFICIAIS.txt'
failures = []
checked = 0
for line in manifest.read_text(encoding='utf-8').splitlines():
    if not line.strip() or line.startswith('#'):
        continue
    digest, relative = line.split('  ', 1)
    target = root / relative
    if not target.is_file():
        failures.append(f'Arquivo ausente: {relative}')
        continue
    actual = sha256(target.read_bytes()).hexdigest()
    checked += 1
    if actual != digest:
        failures.append(f'Arquivo alterado: {relative}')
for error in failures:
    print('FALHA:', error)
print(f'Testadores .sh verificados: {checked} — divergências: {len(failures)}')
sys.exit(1 if failures else 0)
