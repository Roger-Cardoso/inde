#!/usr/bin/env python3
"""Loopback-only view/controller for the real P2-A C++ process; no document logic."""
import argparse
import base64
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import os
from pathlib import Path
import secrets
import select
import struct
import subprocess
import sys
import tempfile
from urllib.parse import urlsplit
import zlib

ASSETS = Path(__file__).resolve().parent
ROOT = ASSETS.parents[1]
MAX_BODY = 12 * 1024 * 1024


def sample_png():
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    width, height = 480, 280
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            if y > 170 + ((x - 240) ** 2 // 2200):
                color = (41, 89, 84)
            elif y > 120 + ((x - 50) ** 2 // 3200):
                color = (108, 148, 128)
            elif (x - 360) ** 2 + (y - 65) ** 2 < 24 ** 2:
                color = (237, 202, 131)
            else:
                color = (214, 228, 215)
            rows.extend(color)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


class Engine:
    def __init__(self, binary, workspace, profile='p2a', seed=None):
        args=[str(binary),'--workspace',str(workspace),'--profile',profile]
        if seed is not None: args += ['--seed-p2d' if profile=='p2d2' else '--seed-p2c' if profile=='p2d' else '--seed-p2b' if profile=='p2c' else '--seed-p2a',str(seed)]
        self.process = subprocess.Popen(args,
                                        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=sys.stderr)
        try:
            self.request({'op': 'view'})
        except Exception:
            self.close()
            raise

    def request(self, data):
        encoded = json.dumps(data, ensure_ascii=False, separators=(',', ':')).encode()
        if len(encoded) > MAX_BODY:
            raise ValueError('request_limit')
        self.process.stdin.write(encoded + b'\n')
        self.process.stdin.flush()
        if not select.select([self.process.stdout], [], [], 45)[0]:
            raise RuntimeError('engine_timeout')
        line = self.process.stdout.readline(16 * 1024 * 1024)
        if not line or not line.endswith(b'\n'):
            raise RuntimeError('engine_did_not_respond')
        return json.loads(line)

    def close(self):
        try:
            self.process.stdin.close()
        except BrokenPipeError:
            pass
        try:
            self.process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            self.process.terminate()
            self.process.wait(timeout=5)
        self.process.stdout.close()
        if self.process.returncode:
            raise RuntimeError('engine_failed: ' + str(self.process.returncode))


class Handler(BaseHTTPRequestHandler):
    def setup(self):
        super().setup()
        self.connection.settimeout(15)

    def reply(self, status, body, mime='application/json; charset=utf-8'):
        if not isinstance(body, bytes):
            body = json.dumps(body, ensure_ascii=False).encode()
        self.send_response(status)
        self.send_header('Content-Type', mime)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('Referrer-Policy', 'no-referrer')
        self.send_header('Content-Security-Policy', "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' blob:; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'")
        self.end_headers()
        self.wfile.write(body)

    def host_ok(self):
        origin = self.headers.get('Origin')
        return self.headers.get('Host') == self.server.authority and (origin is None or origin == 'http://' + self.server.authority)

    def authorized(self):
        token = self.headers.get('X-Obj01-Token', '')
        return self.host_ok() and secrets.compare_digest(token, self.server.token)

    def do_GET(self):
        path = urlsplit(self.path).path
        if not self.host_ok():
            return self.reply(403, {'accepted': False, 'error': 'origin_forbidden'})
        assets = {'/': ('index.html', 'text/html; charset=utf-8'),
                  '/app.js': ('app.js', 'text/javascript; charset=utf-8'),
                  '/style.css': ('style.css', 'text/css; charset=utf-8')}
        if path in assets:
            file, mime = assets[path]
            return self.reply(200, (ASSETS / file).read_bytes(), mime)
        if path == '/sample.png':
            return self.reply(200, self.server.sample, 'image/png')
        if not self.authorized():
            return self.reply(403, {'accepted': False, 'error': 'authorization_required'})
        try:
            if path == '/api/state':
                return self.reply(200, self.server.engine.request({'op': 'view'}))
            if path.startswith('/api/image/'):
                resource = path.removeprefix('/api/image/')
                if len(resource) != 36:
                    return self.reply(400, {'accepted': False, 'error': 'invalid_resource'})
                data = self.server.engine.request({'op': 'image', 'resource': resource})
                if data['accepted']:
                    return self.reply(200, base64.b64decode(data['data'], validate=True), 'image/png')
                return self.reply(422, data)
            self.reply(404, {'accepted': False, 'error': 'not_found'})
        except (RuntimeError, ValueError, OSError):
            self.reply(503, {'accepted': False, 'error': 'engine_unavailable'})

    def do_POST(self):
        if not self.authorized():
            return self.reply(403, {'accepted': False, 'error': 'authorization_required'})
        if self.path != '/api/request':
            return self.reply(404, {'accepted': False, 'error': 'not_found'})
        try:
            size = int(self.headers.get('Content-Length', '-1'))
            if size < 0 or size > MAX_BODY:
                return self.reply(413, {'accepted': False, 'error': 'request_limit'})
            if self.headers.get_content_type() != 'application/json':
                return self.reply(415, {'accepted': False, 'error': 'json_required'})
            raw = self.rfile.read(size)
            if len(raw) != size:
                return self.reply(400, {'accepted': False, 'error': 'incomplete_request'})
            data = json.loads(raw)
            if not isinstance(data, dict) or data.get('op') not in {'import', 'duplicate', 'move', 'arrange', 'arrange_objects', 'set_presentation', 'reset_presentation', 'edit_table_design', 'restore_removed', 'stack', 'restore', 'save', 'reopen', 'select', 'create_table', 'edit_cell', 'select_cell', 'remove', 'undo', 'redo', 'set_context', 'reorder_text', 'reopen_working', 'export_legacy', 'create_formula', 'edit_formula', 'execute_formula', 'reset_formula_variable', 'attach_formula', 'detach_formula', 'edit_formula_text', 'create_formula_cards', 'edit_formula_cards', 'preview_formula_cards', 'create_formula_hub', 'edit_formula_hub', 'preview_formula_hub'}:
                return self.reply(400, {'accepted': False, 'error': 'unsupported_operation'})
            self.reply(200, self.server.engine.request(data))
        except (ValueError, UnicodeError, RecursionError):
            self.reply(400, {'accepted': False, 'error': 'invalid_request'})
        except (RuntimeError, OSError):
            self.reply(503, {'accepted': False, 'error': 'engine_unavailable'})

    def log_message(self, fmt, *args):
        sys.stderr.write('P2-A: ' + fmt % args + '\n')


def main():
    parser = argparse.ArgumentParser(description='Abrir o experimento isolado INDE P2-A')
    parser.add_argument('--binary', type=Path, default=ROOT / 'build-obj01-p2a/inde_object_composite_pilot')
    parser.add_argument('--workspace', type=Path)
    parser.add_argument('--profile', choices=['p2a','p2b','p2c','p2d','p2d2'], default='p2a')
    parser.add_argument('--seed-p2a', type=Path, help='Copiar estado salvo de uma fixture P2-A para uma pasta P2-B nova')
    parser.add_argument('--seed-p2b', type=Path, help='Copiar estado salvo de uma fixture P2-B para P2-C novo')
    parser.add_argument('--seed-p2c', type=Path, help='Copiar estado salvo de P2-C para P2-D novo')
    parser.add_argument('--seed-p2d', type=Path, help='Copiar cálculo salvo de P2-D para P2-D2 novo; comandos mistos incompatíveis são recusados')
    parser.add_argument('--port', type=int, default=0)
    parser.add_argument('--ready-file', type=Path, help=argparse.SUPPRESS)
    args = parser.parse_args()
    if not args.binary.is_file():
        parser.error('Compile o piloto conforme o README antes de abrir a demonstração.')
    workspace = args.workspace.resolve() if args.workspace else Path(tempfile.mkdtemp(prefix='inde-obj01-'+args.profile+'-'))
    if sum(bool(s) for s in [args.seed_p2a,args.seed_p2b,args.seed_p2c,args.seed_p2d]) > 1: parser.error('Escolha somente uma origem experimental.')
    if (args.seed_p2a and args.profile != 'p2b') or (args.seed_p2b and args.profile != 'p2c') or (args.seed_p2c and args.profile != 'p2d') or (args.seed_p2d and args.profile != 'p2d2'):
        parser.error('A origem deve corresponder ao perfil experimental de destino.')
    engine = Engine(args.binary.resolve(), workspace, args.profile, args.seed_p2d or args.seed_p2c or args.seed_p2b or args.seed_p2a)
    try:
        with HTTPServer(('127.0.0.1', args.port), Handler) as server:
            server.engine = engine
            server.token = secrets.token_urlsafe(32)
            server.authority = '127.0.0.1:' + str(server.server_port)
            server.sample = sample_png()
            ready = {'url': 'http://' + server.authority + '/#' + server.token, 'workspace': str(workspace), 'profile': args.profile, 'pid': os.getpid()}
            if args.ready_file:
                # Publish readiness only after complete bytes exist. A polling
                # client must never observe an empty or partially written JSON.
                fd, pending = tempfile.mkstemp(prefix='.obj01-ready-', dir=args.ready_file.parent)
                try:
                    with os.fdopen(fd, 'w', encoding='utf-8') as file:
                        json.dump(ready, file)
                        file.flush()
                        os.fsync(file.fileno())
                    os.link(pending, args.ready_file)  # no overwrite
                finally:
                    os.unlink(pending)
            print('Demo '+args.profile.upper()+': ' + ready['url'], flush=True)
            print('Dados experimentais: ' + str(workspace), flush=True)
            print('Ctrl+C encerra. Operações confirmadas são recuperáveis neste perfil; Salvar confirma o estado autoral.' if args.profile in ['p2c','p2d','p2d2'] else 'Ctrl+C encerra. Alterações em memória precisam ser salvas antes de encerrar.', flush=True)
            try:
                server.serve_forever()
            except KeyboardInterrupt:
                pass
    finally:
        engine.close()


if __name__ == '__main__':
    main()
