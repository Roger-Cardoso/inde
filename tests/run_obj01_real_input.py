#!/usr/bin/env python3
"""Run input tests in a private X11 display, window manager and D-Bus session."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)
if sys.argv[1:] == ['--owned-session']:
    if os.environ.get('INDE_REAL_INPUT_OWN_DISPLAY') != '1':
        raise SystemExit('A sessão privada deve ser iniciada pelo próprio runner.')
    with tempfile.TemporaryDirectory(prefix='inde-input-wm-') as folder:
        env = dict(os.environ, XDG_CONFIG_HOME=folder + '/config', XDG_CACHE_HOME=folder + '/cache',
                   GTK_A11Y='none')
        with open(folder + '/wm.log', 'w+') as log:
            manager = subprocess.Popen(['xfwm4', '--compositor=off'], env=env, stdout=log, stderr=log)
            try:
                time.sleep(.8)
                if manager.poll() is not None:
                    log.seek(0)
                    raise SystemExit('O gerenciador de janelas do teste não iniciou: ' + log.read())
                # Keep the WM connected before setting XKB: Xvfb resets its
                # keymap when its last client disconnects.
                subprocess.run(['setxkbmap', '-layout', 'us', '-variant', 'intl'], env=env, check=True)
                command = [sys.executable, '-m', 'unittest', 'discover', '-s', 'tests', '-v']
                if env.get('INDE_REAL_INPUT_FULL_SUITE') != '1':
                    command += ['-p', 'test_obj01_real_input.py']
                result = subprocess.run(command, env=env)
            finally:
                if manager.poll() is None:
                    manager.terminate()
                    manager.wait(timeout=10)
        raise SystemExit(result.returncode)
for command in ['xvfb-run', 'dbus-run-session', 'xfwm4', 'xdotool', 'xclip', 'setxkbmap']:
    if not shutil.which(command):
        raise SystemExit('Dependência do teste ausente: ' + command)
env = dict(os.environ, INDE_REAL_INPUT_OWN_DISPLAY='1', GDK_BACKEND='x11')
result = subprocess.run(['xvfb-run', '-a', '-s', '-screen 0 1280x1024x24',
                         'dbus-run-session', '--', sys.executable, __file__, '--owned-session'], env=env)
raise SystemExit(result.returncode)
