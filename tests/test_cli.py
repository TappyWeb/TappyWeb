"""Integration checks against the real C++ binary and HTTP runtime; no test dependencies."""
import html
import http.client
import os
from pathlib import Path
import re
import socket
import subprocess
import tempfile
import time
import unittest
import urllib.parse

ROOT = Path(__file__).resolve().parent.parent
BIN = Path(os.environ.get('TAPPY_TEST_BINARY', str(ROOT / 'bin/tappy')))

class LanguageTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)
        self.server = None
    def tearDown(self):
        if self.server:
            self.server.terminate()
            self.server.wait(timeout=5)
            self.server.stdout.close()
        self.tmp.cleanup()
    def cli(self, *args):
        return subprocess.run([str(BIN), *map(str, args)], capture_output=True, text=True)
    def source(self, code):
        p = self.dir / 'app.tappy'
        p.write_text(code)
        return p
    def start(self, path, direct=False):
        with socket.socket() as s:
            s.bind(('127.0.0.1', 0))
            self.port = s.getsockname()[1]
        args = [str(BIN), str(path)] if direct else [str(BIN), 'serve', str(path)]
        self.server = subprocess.Popen([*args, '--port', str(self.port)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        for _ in range(100):
            try:
                self.request('GET', '/')
                return
            except (ConnectionError, OSError):
                if self.server.poll() is not None:
                    self.fail(self.server.stdout.read())
                time.sleep(.03)
        self.fail('Server did not start')
    def request(self, method, path, data=None, headers=None):
        conn = http.client.HTTPConnection('127.0.0.1', self.port, timeout=5)
        conn.request(method, path, data, headers or {})
        r = conn.getresponse()
        body = r.read().decode()
        result = r.status, dict(r.getheaders()), body
        conn.close()
        return result
    def page(self, route='/'):
        status, headers, body = self.request('GET', route)
        self.assertEqual(status, 200)
        cookie = headers['Set-Cookie'].split(';')[0]
        csrf = re.search(r'X-Tappy-CSRF&quot;:&quot;([a-f0-9]+)', body)[1]
        return cookie, csrf, body
    def action(self, body, index=0):
        return re.findall(r'hx-post="([^"]+)"', body)[index]
    def post(self, cookie, csrf, action, **values):
        return self.request('POST', action, urllib.parse.urlencode(values), {'Cookie': cookie, 'X-Tappy-CSRF': csrf, 'Content-Type': 'application/x-www-form-urlencoded', 'HX-Request': 'true'})
    def test_direct_file_command(self):
        path = self.source('page "Direct" /\n    heading "Running directly"\n')
        self.start(path, direct=True)
        _, _, body = self.page()
        self.assertIn('Running directly', body)
        bad = self.cli(path, '--port', 'not-a-port')
        self.assertNotEqual(bad.returncode, 0)
        self.assertIn('Port must be', bad.stderr)

    def test_examples(self):
        for path in (ROOT / 'examples').glob('*.tappy'):
            r = self.cli('check', path)
            self.assertEqual(r.returncode, 0, r.stderr)
    def test_diagnostics_and_dormant_branches(self):
        cases = [
            ('page "Home" /\n  text "x"', 'four spaces'),
            ('page "Home" /\n    if false:\n        mystery "x"', 'Unknown page command'),
            ('state tasks as []\npage "Home" /\n    for each task in tasks:\n        text missing', "don't know 'missing'"),
            ('page "Home" /\n    button "x":\n        on click:\n            add "x" to missing', 'declared list'),
            ('page "Home" /\n    text "unterminated', 'double quote'),
            ('page "Home" /../escape\n    text "x"', 'route like'),
        ]
        for code, expected in cases:
            r = self.cli('check', self.source(code))
            self.assertNotEqual(r.returncode, 0)
            self.assertIn(expected, r.stderr)
            self.assertRegex(r.stderr, r'app.tappy:\d+:')
    def test_static_export_and_dynamic_rejection(self):
        out = self.dir / 'site'
        r = self.cli('build', ROOT/'examples/landing.tappy', '--out', out)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('Grow a little curiosity', (out/'index.html').read_text())
        self.assertNotIn('/_tappy/reload', (out/'index.html').read_text())
        self.assertTrue((out/'story/index.html').exists())
        self.assertGreater((out/'_tappy/htmx.min.js').stat().st_size, 40000)
        rejected = self.dir/'rejected'
        r = self.cli('build', ROOT/'examples/tasks.tappy', '--out', rejected)
        self.assertNotEqual(r.returncode, 0)
        self.assertFalse(rejected.exists())
    def test_tasks_persistence_escaping_csrf_and_stale_actions(self):
        path = self.source((ROOT/'examples/tasks.tappy').read_text())
        self.start(path)
        cookie, csrf, body = self.page('/tasks')
        self.assertIn('Room for a new idea', body)
        action = self.action(body)
        status, _, _ = self.post(cookie, 'wrong', action, draft='bad')
        self.assertEqual(status, 403)
        status, _, body = self.post(cookie, csrf, action, draft='<img src=x onerror=evil()>')
        self.assertEqual(status, 200)
        self.assertNotIn('<!doctype', body)
        self.assertIn('&lt;img src=x onerror=evil()&gt;', body)
        self.assertIn('1 tasks', body)
        # Replaying a completed mutation cannot append the item twice.
        self.assertEqual(self.post(cookie, csrf, action, draft='duplicate')[0], 409)
        done = self.action(body, 1)
        cookie2, csrf2, other = self.page('/tasks')
        self.post(cookie2, csrf2, self.action(other), draft='Another task')
        _, _, stale = self.post(cookie, csrf, done)
        self.assertIn('shared data changed', stale)
        self.assertIn('2 tasks', stale)
        _, _, deleted = self.post(cookie, csrf, self.action(stale, 1))
        self.assertIn('1 tasks', deleted)
        self.assertNotIn('&lt;img', deleted)
        self.server.terminate(); self.server.wait(timeout=5); self.server.stdout.close(); self.server = None
        self.start(path)
        _, _, body = self.page('/tasks')
        self.assertIn('Another task', body)
        self.assertNotIn('&lt;img', body)
        self.assertEqual(self.request('GET', '/missing')[0], 404)
        self.assertIn('htmx', self.request('GET', '/_tappy/htmx.min.js')[2])
    def test_feedback_validation_and_transaction_rollback(self):
        path = self.source((ROOT/'examples/feedback.tappy').read_text())
        self.start(path)
        cookie, csrf, body = self.page()
        _, _, invalid = self.post(cookie, csrf, self.action(body), name='A', email='bad', message='hello')
        self.assertIn('Enter an email', invalid)
        self.assertIn('Be the first', invalid)
        _, _, valid = self.post(cookie, csrf, self.action(invalid), name='Ada', email='ada@example.test', message='Nice!')
        self.assertIn('Thanks, Ada!', valid)
        self.assertIn('Ada: Nice!', valid)
    def test_arithmetic_conditions_and_atomic_action_failure(self):
        path = self.source('''state divisor as 1
state count as 1
page "Test" /
    text "Count: {count}"
    button "Fail":
        on click:
            set count to count + 1
            set divisor to 0
            set count to count / divisor
''')
        self.start(path)
        cookie, csrf, body = self.page()
        _, _, body = self.post(cookie, csrf, self.action(body))
        self.assertIn('Cannot divide by zero', body)
        self.assertIn('Count: 1', body)
    def test_live_reload_preserves_state_and_invalidates_old_actions(self):
        original = '''remember count as 1
page "Before" /
    text "Before {count}"
    button "Increment":
        on click:
            set count to count + 1
'''
        path = self.source(original)
        self.start(path, direct=True)
        cookie, csrf, body = self.page()
        version = re.search(r'/_tappy/reload\?version=([a-f0-9]+)', body)[1]
        _, _, changed = self.post(cookie, csrf, self.action(body))
        stale_action = self.action(changed)
        # Simulate editors which save with atomic rename, not in-place writes.
        replacement = self.dir / 'replacement.tappy'
        replacement.write_text(original.replace('Before', 'After').replace('as 1', 'as 99') + '\npage "New route" /new\n    text "New route works"\n')
        replacement.replace(path)
        status, headers, _ = self.request('GET', '/_tappy/reload?version=' + version)
        self.assertEqual(status, 204)
        self.assertEqual(headers['HX-Refresh'], 'true')
        _, _, new_body = self.request('GET', '/', headers={'Cookie': cookie})
        self.assertIn('After 2', new_body)
        self.assertNotIn('After 99', new_body)
        new_version = re.search(r'/_tappy/reload\?version=([a-f0-9]+)', new_body)[1]
        self.assertNotEqual(new_version, version)
        self.assertNotIn('HX-Refresh', self.request('GET', '/_tappy/reload?version=' + new_version)[1])
        self.assertIn('New route works', self.request('GET', '/new')[2])
        status, headers, _ = self.post(cookie, csrf, stale_action)
        self.assertEqual(status, 409)
        self.assertEqual(headers['HX-Refresh'], 'true')
        _, _, incremented = self.post(cookie, csrf, self.action(new_body))
        self.assertIn('After 3', incremented)

    def test_invalid_reload_keeps_working_version_then_recovers(self):
        path = self.source('page "Working" /\n    text "Still working"\n')
        self.start(path)
        _, _, body = self.page()
        version = re.search(r'/_tappy/reload\?version=([a-f0-9]+)', body)[1]
        path.write_text('page "Broken" /\n    mystery "bad"\n')
        self.assertNotIn('HX-Refresh', self.request('GET', '/_tappy/reload?version=' + version)[1])
        self.assertIn('Still working', self.request('GET', '/')[2])
        path.unlink()
        self.assertIn('Still working', self.request('GET', '/')[2])
        path.write_text('page "Fixed" /\n    text "Fixed without restart"\n')
        self.assertEqual(self.request('GET', '/_tappy/reload?version=' + version)[1]['HX-Refresh'], 'true')
        self.assertIn('Fixed without restart', self.request('GET', '/')[2])

    def test_native_htmx_fragments(self):
        path = self.source((ROOT/'examples/fragments.tappy').read_text())
        self.start(path)
        _, _, body = self.page()
        self.assertIn('hx-trigger="load, every 5s"', body)
        self.assertIn('id="stats"', body)
        status, _, fragment = self.request('GET', '/stats')
        self.assertEqual(status, 200)
        self.assertNotIn('<main', fragment)
        self.assertIn('7 tasks completed', fragment)

    def test_htmx_navigation_and_redirect(self):
        path=self.source((ROOT/'examples/tasks.tappy').read_text())
        self.start(path)
        cookie, csrf, body = self.page()
        status, headers, body = self.post(cookie, csrf, self.action(body))
        self.assertEqual(status, 200)
        self.assertEqual(headers['HX-Push-Url'], '/tasks')
        self.assertIn('One thing at a time', body)
        status, _, body = self.request('GET','/about',headers={'HX-Request':'true'})
        self.assertNotIn('<!doctype',body)
        self.assertIn('Code you can explain',body)

if __name__ == '__main__':
    unittest.main(verbosity=2)
