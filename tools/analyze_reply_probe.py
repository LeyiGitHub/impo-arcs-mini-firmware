#!/usr/bin/env python3
"""Fixed-field analysis of observed /chat/subscribe events; never exports text.

An earlier matching sentence is a measured scheduling opportunity, not proof
that future deltas cannot be revised or a measured playback speedup.
"""
import argparse
import json
from pathlib import Path
import re
from statistics import median

ANSI = re.compile(r'\x1b\[[0-?]*[ -/]*[@-~]')


def public_image(metadata):
    return {k: metadata[k] for k in ['image_sha256', 'image_bytes', 'product',
            'listenai_sdk_version', 'wake_bundle'] if k in metadata}


def analyze(directory):
    directory = Path(directory)
    manifest = json.loads((directory / 'manifest.json').read_text())
    source = json.loads((directory / 'trials.json').read_text())
    rows = []
    patterns = [
        (r'voice_delta epoch=(\d+) start=(\d+) append=(\d+) done=(\d+) full=(\d+) other=(\d+)',
         ['starts', 'appends', 'dones', 'fulls', 'other_deltas']),
        (r'voice_delta_time epoch=(\d+) first=(\d+) sentence=(\d+) final=(\d+)',
         ['first_append_ms', 'first_sentence_ms', 'final_ms']),
        (r'voice_delta_match epoch=(\d+) bytes=(\d+) sentence=(\d+) prefix=(\d+) equal=(\d+) errors=(\d+)',
         ['append_bytes', 'sentence_bytes', 'prefix_matches', 'equals_final', 'errors']),
    ]
    for turn in source:
        row = {k: turn[k] for k in ['index', 'cycle', 'mode', 'endpoint_ms', 'ok', 'voice_ok', 'failed'] if k in turn}
        row['bench_error'] = bool(turn.get('bench_error') or turn.get('error'))
        row['probe_complete'] = False
        path = directory / ('trial-%03d-%s-%d.log' % (turn['index'], turn['mode'], turn['endpoint_ms']))
        if path.is_file() and 'epoch' in turn:
            text = ANSI.sub('', path.read_text(errors='replace'))
            values = {}
            for pattern, keys in patterns:
                for m in re.finditer(pattern, text):
                    if int(m[1]) == turn['epoch']:
                        values.update({k: int(v) for k, v in zip(keys, m.groups()[1:])})
            row['probe_complete'] = len(values) == 13
            # Export only durations relative to the matched ACK, not boot IDs.
            ack = turn.get('ack', 0)
            for k, v in values.items():
                if k.endswith('_ms'):
                    row[k.replace('_ms', '_after_ack_ms')] = v - ack if v and ack else None
                else:
                    row[k] = v
            if row['probe_complete'] and ack and values['final_ms']:
                sentence, final = values['first_sentence_ms'], values['final_ms']
                row['observed_sentence_lead_ms'] = final - max(ack, sentence) if sentence else None
                row['potential_early_submission_ms'] = max(0, final - max(ack, sentence)) if sentence and values['prefix_matches'] and not values['errors'] else 0
        rows.append(row)
    valid = [r for r in rows if r.get('probe_complete') and r.get('voice_ok')]
    gains = [r['potential_early_submission_ms'] for r in valid if 'potential_early_submission_ms' in r]
    return {
        'schema_version': 1,
        'metric': 'observed matched first-sentence scheduling opportunity, not measured playback gain',
        'installed_image': public_image(manifest.get('installed_image', {})),
        'stimulus_sha256': manifest['stimulus_sha256'],
        'summary': {'attempted': len(rows), 'complete_successful_probes': len(valid),
                    'positive_opportunity_trials': sum(v > 0 for v in gains),
                    'opportunity_median_ms': median(gains) if gains else None},
        'trials': rows,
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.write_text(json.dumps(analyze(args.directory), ensure_ascii=False, indent=2) + '\n')
