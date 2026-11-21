"""Real HTTP -> thin controller -> C++ -> SQLite tests on loopback."""
import base64
import http.client
import json
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
from urllib.parse import urlsplit
import unittest

BINARY = Path(sys.argv.pop(1)).resolve() if __name__ == '__main__' else None
SERVER = Path(__file__).resolve().parents[2] / 'tools/obj01-pilot/serve.py'


class HttpPilot(unittest.TestCase):
    profile = 'p2a'
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='obj01-p2a-http-')
        self.ready = Path(self.tmp.name) / 'ready.json'
        self.log = tempfile.TemporaryFile()
        self.process = subprocess.Popen([sys.executable, str(SERVER), '--binary', str(BINARY),
                                         '--workspace', str(Path(self.tmp.name) / 'fixture'), '--profile', self.profile,
                                         '--ready-file', str(self.ready)], stdout=self.log, stderr=self.log)
        self.addCleanup(self.cleanup_server)
        for _ in range(300):
            if self.ready.is_file():
                break
            if self.process.poll() is not None:
                self.log.seek(0)
                self.fail(self.log.read().decode())
            time.sleep(.05)
        self.assertTrue(self.ready.is_file(), 'local server did not become ready')
        ready = json.loads(self.ready.read_text())
        parsed = urlsplit(ready['url'])
        self.authority = parsed.netloc
        self.token = parsed.fragment

    def cleanup_server(self):
        if self.process.poll() is None:
            self.process.send_signal(signal.SIGINT)
        self.process.wait(timeout=20)
        self.log.seek(0)
        error = self.log.read().decode()
        self.log.close()
        self.tmp.cleanup()
        self.assertEqual(self.process.returncode, 0, error)

    def request(self, path, data=None, authenticated=True, headers=None):
        values = {'X-Obj01-Token': self.token} if authenticated else {}
        values.update(headers or {})
        body = None if data is None else json.dumps(data).encode()
        if body is not None:
            values['Content-Type'] = 'application/json'
        connection = http.client.HTTPConnection(self.authority, timeout=15)
        connection.request('GET' if data is None else 'POST', path, body=body, headers=values)
        response = connection.getresponse()
        result = response.status, dict(response.getheaders()), response.read()
        connection.close()
        return result

    def state(self):
        status, _, body = self.request('/api/state')
        self.assertEqual(status, 200)
        return json.loads(body)['state']

    def test_token_and_origin_guards_preserve_document(self):
        self.assertEqual(self.request('/api/state', authenticated=False)[0], 403)
        self.assertEqual(self.request('/api/state', headers={'Origin':'https://example.invalid'})[0], 403)
        self.assertEqual(self.request('/api/state', headers={'Host':'outside.invalid'})[0], 403)
        self.assertEqual(self.state()['revision'], 0)

    def test_static_interface_and_safety_headers(self):
        status, headers, body = self.request('/', authenticated=False)
        self.assertEqual(status, 200)
        self.assertIn(b'Inserir figura', body)
        self.assertIn("frame-ancestors 'none'", headers['Content-Security-Policy'])
        self.assertEqual(headers['X-Content-Type-Options'], 'nosniff')
        self.assertEqual(headers['Referrer-Policy'], 'no-referrer')
        self.assertEqual(self.request('/../../origem-documento.json')[0], 404)

    def test_real_import_save_reopen_and_resource_bytes(self):
        initial = self.state()
        status, _, image = self.request('/sample.png', authenticated=False)
        self.assertEqual(status, 200)
        request = {'op':'import','expected':initial['revision'],'data':base64.b64encode(image).decode(),
                   'before':initial['paragraphs'][1]['id'],'caption':'<img src=x onerror=alert(1)>',
                   'alt':'Amostra declarada','decorative':False}
        status, _, raw = self.request('/api/request',request)
        self.assertEqual(status, 200)
        result = json.loads(raw)
        self.assertTrue(result['accepted'], result)
        state = result['state']
        figure = state['figures'][0]
        self.assertEqual(figure['caption'], request['caption'])
        status, headers, saved_image = self.request('/api/image/' + figure['resource'])
        self.assertEqual((status,headers['Content-Type']), (200,'image/png'))
        self.assertEqual(saved_image,image)
        _, _, raw = self.request('/api/request', {'op':'save','expected':state['revision']})
        saved = json.loads(raw)
        self.assertTrue(saved['accepted'], saved)
        self.assertFalse(saved['state']['dirty'])
        _, _, raw = self.request('/api/request', {'op':'reopen','expected':saved['state']['revision'],'discard_unsaved':False})
        reopened = json.loads(raw)
        self.assertTrue(reopened['accepted'],reopened)
        self.assertEqual(reopened['state']['figures'],state['figures'])

    def test_invalid_request_has_no_effect(self):
        initial = self.state()
        status, _, _ = self.request('/api/request', {'op':'unknown','expected':0})
        self.assertEqual(status,400)
        status, _, _ = self.request('/api/request', {'op':'save','expected':0}, authenticated=False)
        self.assertEqual(status,403)
        connection = http.client.HTTPConnection(self.authority,timeout=15)
        connection.request('POST','/api/request',body=b'',headers={'X-Obj01-Token':self.token,'Content-Length':str(13*1024*1024),'Content-Type':'application/json'})
        response=connection.getresponse()
        self.assertEqual(response.status,413)
        response.read(); connection.close()
        self.assertEqual(self.state(),initial)


if __name__ == '__main__':
    unittest.main(verbosity=2)
