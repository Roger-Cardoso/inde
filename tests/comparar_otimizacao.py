"""Run paired benchmarks; compare semantic fingerprints before timing."""
import argparse
import json
from pathlib import Path
import statistics
import subprocess


def run(binary):
    return subprocess.check_output([str(binary)], text=True)


def parse(output):
    result = {}
    for line in output.splitlines():
        fields = line.split()
        if len(fields) < 2:
            continue
        result[fields[0]] = {key: float(value) for key, value in
                             (field.split('=', 1) for field in fields[1:])}
    return result


def compare(before, after, output):
    output.mkdir(parents=True, exist_ok=False)
    signatures = [run(folder / 'equivalence').strip() for folder in (before, after)]
    if signatures[0] != signatures[1]:
        raise RuntimeError('Behavior changed: semantic fingerprints differ')
    samples = {'before': [], 'after': []}
    for iteration in range(3):
        for label, folder in [('before', before), ('after', after)]:
            text = run(folder / 'benchmark')
            (output / f'{label}-{iteration + 1}.txt').write_text(text)
            samples[label].append(parse(text))
    report = {'semantic_fingerprint': signatures[0], 'paired_runs': 3,
              'iterations_per_run': 5, 'scenarios': {}}
    for case in samples['before'][0]:
        pair = {label: {metric: statistics.median(sample[case][metric] for sample in rows)
                        for metric in rows[0][case]} for label, rows in samples.items()}
        if pair['before']['result'] != pair['after']['result']:
            raise RuntimeError('Benchmark result changed: ' + case)
        pair['time_reduction_percent'] = 100 * (1 - pair['after']['median_ms'] / pair['before']['median_ms'])
        report['scenarios'][case] = pair
    (output / 'resultado.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('before', type=Path, help='Folder with benchmark and equivalence executables')
    parser.add_argument('after', type=Path)
    parser.add_argument('output', type=Path, help='New results folder')
    args = parser.parse_args()
    compare(args.before.resolve(), args.after.resolve(), args.output.resolve())
