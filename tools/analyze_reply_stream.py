#!/usr/bin/env python3
"""Export every supplied reply-stream trial using fixed, content-free fields.

Arguments are private bench directories in experiment order. Requires the same
image and stimulus; does not calculate a speedup from rejected acoustic samples.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import wave

from analyze_reply_probe import ANSI, analyze, public_image


def export(directories, stimulus=None):
    rows = []
    images, stimuli, boots = [], [], set()
    conditions = []
    for run, directory in enumerate(map(Path, directories), 1):
        manifest = json.loads((directory / 'manifest.json').read_text())
        images.append(public_image(manifest.get('installed_image', {})))
        stimuli.append(manifest['stimulus_sha256'])
        conditions.append({k: manifest[k] for k in ['speaker_volume_percent',
            'mac_output_volume_percent', 'afplay_volume'] if k in manifest})
        probes = analyze(directory)['trials']
        source = json.loads((directory / 'trials.json').read_text())
        acoustic_path = directory / 'analysis.json'
        acoustic_rows = json.loads(acoustic_path.read_text())['trials'] if acoustic_path.is_file() else []
        for turn, probe in zip(source, probes):
            reply_mode = turn.get('reply_mode')
            if reply_mode not in ('full', 'incremental'):
                raise ValueError('Missing explicit reply mode')
            boots.add(turn.get('boot'))
            row = {'run': run, 'index': turn['index'], 'reply_mode': reply_mode,
                   'endpoint_ms': turn.get('endpoint_ms'),
                   'ok': bool(turn.get('ok')), 'voice_ok': bool(turn.get('voice_ok')),
                   'bench_error': bool(turn.get('error')),
                   'audio_timing_generation_matches': bool(turn.get('audio_timing_generation_matches'))}
            if 'early_capture_snapshot_matched' in turn:
                row['early_capture_snapshot_matched'] = turn['early_capture_snapshot_matched']
            for key in ['pcm', 'dropped', 'failed', 'chat_requests', 'tts_requests', 'answer_segments_decoded']:
                if key in turn:
                    row[key] = turn[key]
            row['capture_pcm_ms'] = turn['pcm'] / 32 if 'pcm' in turn else None
            log_path = directory / ('trial-%03d-%s-%d.log' % (turn['index'], turn['mode'], turn['endpoint_ms']))
            log = ANSI.sub('', log_path.read_text(errors='replace')) if log_path.is_file() else ''
            for mode in re.finditer(r'voice_reply_mode epoch=(\d+) incremental=(\d+) segments=(\d+) mismatches=(\d+)', log):
                if int(mode[1]) == turn.get('epoch'):
                    row.update(runtime_incremental=bool(int(mode[2])),
                               reply_segments=int(mode[3]), reply_mismatches=int(mode[4]))
            capture = re.search(r'listened (\d+) ms in (\d+) ms .*?ended by (pause|max length);', log)
            row['capture_end_reason'] = capture[3] if capture else None
            stage = turn.get('stages', {})
            last_voice = stage.get('0', stage.get(0))
            row['capture_time_available'] = bool(last_voice)
            pb = turn.get('playback', {})
            row['playback'] = {k: pb[k] for k in ['played_samples', 'queued_samples',
                'underruns', 'underrun_ms', 'prefill_ms'] if k in pb}
            bounds = {
                'last_voice_to_capture_ms': (last_voice, turn.get('capture')),
                'capture_to_queued_ms': (turn.get('capture'), turn.get('upload')),
                'queued_to_ack_ms': (turn.get('upload'), turn.get('ack')),
                'ack_to_final_ms': (turn.get('ack'), turn.get('text')),
                'tts_submit_to_body_ms': (turn.get('tts'), pb.get('first_body_ms')),
                'body_to_pcm_ms': (pb.get('first_body_ms'), pb.get('first_pcm_ms')),
                'pcm_to_driver_ms': (pb.get('first_pcm_ms'), pb.get('first_write_ms')),
                'last_voice_to_driver_ms': (last_voice, pb.get('first_write_ms')),
                'driver_first_to_last_write_ms': (pb.get('first_write_ms'), pb.get('last_write_ms')),
            }
            row['device_intervals'] = {k: b-a if a and b else None for k, (a,b) in bounds.items()}
            # Positive means the first TTS submission preceded the final text.
            row['tts_submission_before_final_ms'] = turn['text']-turn['tts'] if turn.get('text') and turn.get('tts') else None
            row['probe'] = {k: v for k,v in probe.items() if k not in
                ['index', 'cycle', 'mode', 'endpoint_ms', 'ok', 'voice_ok', 'failed', 'bench_error']}
            acoustic = next((a.get('acoustic', {}) for a in acoustic_rows if a['index']==turn['index']), {})
            row['acoustic'] = {'valid': bool(acoustic.get('acoustic_valid'))}
            if row['acoustic']['valid']:
                row['acoustic'].update({k: acoustic[k] for k in ['acoustic_gap_ms',
                    'reference_correlation', 'vad_last_voice_relative_to_stimulus_end_ms'] if k in acoustic})
            rows.append(row)
    if not rows or any(v != images[0] for v in images) or any(v != stimuli[0] for v in stimuli):
        raise ValueError('Supply nonempty trials from one image and one stimulus')
    duration = None
    if stimulus:
        stimulus = Path(stimulus)
        if hashlib.sha256(stimulus.read_bytes()).hexdigest() != stimuli[0]:
            raise ValueError('Stimulus hash mismatch')
        with wave.open(str(stimulus), 'rb') as wav:
            duration = round(1000 * wav.getnframes() / wav.getframerate(), 3)
    return {
        'schema_version': 1,
        'metric': 'exploration of early TTS submission and queue continuity; no validated end-to-end speedup',
        'installed_image': images[0], 'stimulus_sha256': stimuli[0],
        'stimulus_duration_ms': duration, 'same_boot': len(boots)==1 and None not in boots,
        'common_conditions': conditions[0] if all(c==conditions[0] for c in conditions) else None,
        'summary': {'attempts': len(rows), 'completed': sum(r['voice_ok'] for r in rows),
            'acoustic_valid': sum(r['acoustic']['valid'] for r in rows),
            'complete_probe_records': sum(r['probe']['probe_complete'] for r in rows),
            'capture_max_length': sum(r['capture_end_reason']=='max length' for r in rows),
            'underruns': sum(r['playback'].get('underruns', 0) for r in rows)},
        'trials': rows,
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directories', nargs='+', type=Path)
    parser.add_argument('--stimulus', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    args.output.write_text(json.dumps(export(args.directories, args.stimulus), ensure_ascii=False, indent=2)+'\n')
