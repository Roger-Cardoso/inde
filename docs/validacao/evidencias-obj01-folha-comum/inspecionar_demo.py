#!/usr/bin/env python3
"""Inspeciona a demo fechada deste corte, somente de leitura; JSON em stdout."""
import hashlib
import json
import sqlite3
import sys
from pathlib import Path

project = Path(sys.argv[1]).resolve()


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory():
    return {str(f.relative_to(project)): digest(f)
            for f in project.rglob('*') if f.is_file()}


before = inventory()
files = [project / 'data/project.sqlite3',
         *project.glob('documents/composed/*/obj01-piloto.sqlite3'),
         *project.glob('recovery/editor-v1/*.sqlite3')]
report = {'project': str(project), 'readonly': True, 'databases': {}}
owner = {}
for path in files:
    connection = sqlite3.connect(path.as_uri() + '?mode=ro', uri=True)
    connection.execute('PRAGMA query_only=ON')
    integrity = [row[0] for row in connection.execute('PRAGMA integrity_check')]
    foreign_keys = connection.execute('PRAGMA foreign_key_check').fetchall()
    assert integrity == ['ok'] and not foreign_keys, path
    row = {'sha256': digest(path), 'integrity_check': integrity,
           'foreign_key_check': foreign_keys,
           'user_version': connection.execute('PRAGMA user_version').fetchone()[0]}
    if path.name == 'project.sqlite3':
        row['schema'] = connection.execute('SELECT max(version) FROM schema_migrations').fetchone()[0]
        assert row['schema'] == 37
        owner = {i: (title, body) for i, title, body in connection.execute(
            'SELECT id,title,content FROM documents')}
        row['documents'] = len(owner)
    if path.name == 'obj01-piloto.sqlite3':
        assert row['user_version'] == 5
        revision, saved_hash, saved = connection.execute(
            'SELECT revision,sha256,snapshot FROM saved').fetchone()
        base_revision, base_hash, working_hash, working = connection.execute(
            'SELECT base_revision,base_sha256,sha256,snapshot FROM working').fetchone()
        assert hashlib.sha256(saved.encode()).hexdigest() == saved_hash
        assert hashlib.sha256(working.encode()).hexdigest() == working_hash
        assert (base_revision, base_hash) == (revision, saved_hash)
        state = json.loads(working)
        document = json.loads(state['working_text'])['document']
        assert (document['title'], document['content']) == owner[document['id']]
        assert hashlib.sha256(state['source'].encode()).hexdigest() == state['source_sha256']
        assert (path.parent / 'origem-documento.json').read_bytes() == state['source'].encode()
        paragraphs = json.loads(document['paragraphs'])
        row.update(format=state['format'], revision=revision,
                   saved_equals_working=saved == working,
                   document_title_content_match_owner=True,
                   origin_sha256=state['source_sha256'],
                   origin_matches_archived_file=True,
                   paragraphs=len(paragraphs['blocks']),
                   paragraph_ids=paragraphs['order'],
                   equations=len(state['composition']['equations']),
                   tables=len(state['tables']), formulas=len(state['formulas']))
    report['databases'][str(path.relative_to(project))] = row
    connection.close()
assert inventory() == before, 'A árvore do projeto mudou durante a conferência'
report.update(project_files_checked=len(before), unchanged_after_checks=True)
print(json.dumps(report, ensure_ascii=False, indent=2))
