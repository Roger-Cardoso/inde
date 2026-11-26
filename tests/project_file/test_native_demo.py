"""Exercise the actual demo launcher in a NEW private X11/D-Bus session.

No widget signals, app test mode, or application files are used to close GTK.
The launcher creates its own synthetic project. Preserve evidence on failure.
"""
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time

BASE = Path(__file__).resolve().parents[2]
root = Path(sys.argv[1]).absolute()
root.mkdir(parents=True, exist_ok=False)
env = dict(os.environ, GSK_RENDERER='cairo', GTK_A11Y='none',
           GTK_USE_PORTAL='0', G_DEBUG='fatal-criticals', GDK_BACKEND='x11',
           XDG_CONFIG_HOME=str(root / 'config'), XDG_DATA_HOME=str(root / 'data'),
           XDG_CACHE_HOME=str(root / 'cache'))
if not env.get('DISPLAY'):
    raise SystemExit('Use um display X11 privado, por exemplo xvfb-run com dbus-run-session.')
manager = process = None
with (root / 'window-manager.log').open('w') as wm_log, (root / 'launcher.log').open('w') as log:
    try:
        manager = subprocess.Popen(['xfwm4', '--compositor=off'], env=env,
                                   stdout=wm_log, stderr=wm_log)
        time.sleep(.8)
        if manager.poll() is not None:
            raise RuntimeError('O gerenciador do display privado não iniciou.')
        process = subprocess.Popen(['bash', str(BASE / 'scripts/demos/Abrir-demo-arquivo-unico.sh')],
                                   cwd=BASE, env=env, stdout=log, stderr=log,
                                   start_new_session=True)
        deadline = time.monotonic() + 180
        packet = window = None
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError('O demonstrador encerrou antes da interação.')
            output = (root / 'launcher.log').read_text()
            match = re.search(r'^Arquivo único: (.+)$', output, re.MULTILINE)
            if match:
                packet = Path(match[1])
                found = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name', 'INDE'],
                                       env=env, capture_output=True, text=True)
                if found.returncode == 0 and found.stdout.strip():
                    window = found.stdout.splitlines()[-1]
                    break
            time.sleep(.2)
        if not window or not packet:
            raise RuntimeError('A janela nativa da demonstração não apareceu.')
        subprocess.run(['xdotool', 'windowactivate', '--sync', window], env=env, check=True)
        time.sleep(1)
        subprocess.run(['xdotool', 'key', '--clearmodifiers', 'ctrl+s'], env=env, check=True)
        time.sleep(.8)
        subprocess.run(['xdotool', 'key', '--clearmodifiers', 'ctrl+q'], env=env, check=True)
        if process.wait(timeout=40) != 0:
            raise RuntimeError('Salvar/encerrar/capturar retornou erro; consulte launcher.log.')
        folder = packet.parent
        original = json.loads((folder / 'verificacao.json').read_text())
        final = json.loads((folder / 'integridade-final.json').read_text())
        history = json.loads((folder / 'historico-final.json').read_text())
        if not original['original_preserved'] or not final['valid'] or len(history) < 4:
            raise RuntimeError('A conferência final do arquivo único não passou.')
        if any(word in (root / 'launcher.log').read_text()
               for word in ['Gtk-CRITICAL', 'Gdk-CRITICAL', 'GLib-GObject-CRITICAL']):
            raise RuntimeError('A demonstração emitiu erro crítico GTK.')
        (root / 'resultado.json').write_text(json.dumps(dict(
            demo=str(folder), native_window=True, keyboard_save=True,
            keyboard_quit=True, launcher_capture=True,
            verified_single_file=True, states=len(history)), indent=2) + '\n')
        print('P5 GTK demo: actual launcher, keyboard Save/Quit, capture and verification OK')
    finally:
        if process and process.poll() is None:
            os.killpg(process.pid, signal.SIGTERM)
            process.wait(timeout=10)
        if manager and manager.poll() is None:
            manager.terminate()
            manager.wait(timeout=10)
