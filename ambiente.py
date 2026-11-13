"""Ambiente do INDE separado do ambiente do navegador do usuário."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sysconfig

KEYS = ('XDG_CONFIG_HOME', 'GTK_IM_MODULE', 'GTK_THEME', 'GSK_RENDERER', 'GDK_DEBUG', 'GDK_DISABLE', 'FONTCONFIG_FILE', 'FONTCONFIG_PATH')
KEY = 'INDE_PUBLICACAO_AMBIENTE_DESKTOP'


def desktop_environment(environment=None):
    env = dict(os.environ if environment is None else environment)
    saved = json.loads(env.pop(KEY)) if KEY in env else None
    if saved is not None:
        if set(saved) != set(KEYS) or any(v is not None and not isinstance(v, str) for v in saved.values()):
            raise ValueError('Registro de ambiente do desktop inválido.')
        for key, value in saved.items():
            if value is None:
                env.pop(key, None)
            else:
                env[key] = value
    env.pop('INDE_PUBLICACAO_RECURSOS', None)
    return env


def ibus_available(roots=None):
    if roots is None:
        roots = [Path('/usr/lib'), Path('/usr/lib64'), Path('/usr/local/lib')]
        multiarch = sysconfig.get_config_var('MULTIARCH')
        if multiarch:
            roots.append(Path('/usr/lib') / multiarch)
        roots.extend(Path(p) for p in os.environ.get('GTK_PATH', '').split(':') if p)
    return any(any(root.glob(pattern)) for root in roots for pattern in ('gtk-4.0/*/immodules/*ibus*.so', '*/immodules/*ibus*.so', 'immodules/*ibus*.so'))


def native_environment(profile, resources, environment=None, has_ibus=None):
    env = dict(os.environ if environment is None else environment)
    if KEY not in env:
        env[KEY] = json.dumps({key: env.get(key) for key in KEYS})
    env['XDG_CONFIG_HOME'] = str(Path(profile).resolve())
    env['INDE_PUBLICACAO_RECURSOS'] = str(Path(resources).resolve())
    if env.get('GTK_IM_MODULE') == 'ibus' and not (ibus_available() if has_ibus is None else has_ibus):
        # Let GTK select a built-in or platform context when no external IBus module exists.
        env.pop('GTK_IM_MODULE', None)
    return env


def open_browser(url, log, environment=None):
    env = desktop_environment(environment)
    log = Path(log)
    log.parent.mkdir(parents=True, exist_ok=True)
    opener = shutil.which('xdg-open', path=env.get('PATH'))
    if not opener:
        raise RuntimeError('xdg-open não está disponível. Abra o endereço local no navegador: ' + url)
    with os.fdopen(os.open(log, os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0o600), 'ab') as stream:
        process = subprocess.Popen([opener, url], env=env, stdout=stream, stderr=stream, start_new_session=True)
    # The opener may stay alive with the browser; never kill it on timeout.
    try:
        code = process.wait(timeout=2)
    except subprocess.TimeoutExpired:
        return
    if code:
        raise RuntimeError('O navegador não confirmou a abertura. Endereço: ' + url + '. Diagnóstico: ' + str(log))


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--perfil', required=True)
    parser.add_argument('--recursos', required=True)
    parser.add_argument('programa')
    parser.add_argument('argumentos', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    Path(args.perfil).mkdir(parents=True, exist_ok=True)
    os.execve(args.programa, [args.programa, *args.argumentos], native_environment(args.perfil, args.recursos))
