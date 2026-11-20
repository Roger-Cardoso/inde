"""Run existing document GTK drivers against the Special's compiled UI objects.
Manual display test; only new synthetic fixtures/profiles, never real projects.
Compile with preparar_drivers_integrados.py before calling this runner.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
DRIVERS = ['document_paragraphs', 'document_alternatives', 'document_reading_order',
           'editor_specializations']


def run(destination, include_historical_visibility=False):
    drivers = list(DRIVERS)
    if include_historical_visibility:
        # P03b expects a separate lens widget removed by P03c. Forensic use only.
        drivers.insert(-1, 'document_visibility')
    binaries = {name: ROOT / ('build-nativo/inde_integrated_' + name + '_smoke')
                for name in drivers}
    for path in binaries.values():
        if not path.is_file():
            raise FileNotFoundError('Compile the existing driver first: ' + str(path))
    destination.mkdir(parents=True, exist_ok=False)
    results = []
    for name, binary in binaries.items():
        for theme in ['light', 'dark']:
            fixture = destination / (name + '-' + theme)
            for phase in [theme, '--verify-restart']:
                env = dict(os.environ)
                env['INDE_PUBLICACAO_RECURSOS'] = str(ROOT)
                # The driver creates this directory and sets its own isolated XDG profile.
                label = name + '-' + theme + ('-restart' if phase.startswith('--') else '')
                try:
                    result = subprocess.run([binary, fixture, phase], env=env,
                                            capture_output=True, text=True, timeout=120)
                    (destination / (label + '.log')).write_text(result.stdout + result.stderr)
                    record = {'driver': name, 'theme': theme, 'restart': phase.startswith('--'),
                              'returncode': result.returncode}
                except subprocess.TimeoutExpired as error:
                    (destination / (label + '.log')).write_bytes((error.stdout or b'') + (error.stderr or b''))
                    record = {'driver': name, 'theme': theme, 'restart': phase.startswith('--'),
                              'returncode': 'timeout'}
                results.append(record)
                (destination / 'resultados.json').write_text(json.dumps(results, indent=2) + '\n')
                print(json.dumps(record), flush=True)
                if record['returncode'] != 0:
                    # Do not reuse a failed fixture as if it were a successful restart.
                    break
    return all(record['returncode'] == 0 for record in results) and len(results) == len(drivers) * 4


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path)
    parser.add_argument('--include-historical-visibility', action='store_true',
                        help='Also exercise the obsolete P03b two-editor UI assertions')
    args = parser.parse_args()
    raise SystemExit(0 if run(args.destination.resolve(), args.include_historical_visibility) else 1)
