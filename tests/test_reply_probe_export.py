import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from analyze_reply_probe import analyze
from analyze_reply_stream import export


class ReplyProbeExport(unittest.TestCase):
    def test_timing_privacy_and_partial_records(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)
            (p/'manifest.json').write_text(json.dumps({'stimulus_sha256':'fixture',
                'installed_image':{'image_sha256':'abc','serial':'private serial'}}))
            turn={'index':1,'cycle':1,'mode':'batch','endpoint_ms':1200,'epoch':7,
                  'ack':1000,'voice_ok':True,'ok':True,'boot':'private','transcript':'private',
                  'error':'private error'}
            (p/'trials.json').write_text(json.dumps([turn]))
            log=p/'trial-001-batch-1200.log'
            lines=('voice_delta epoch=7 start=1 append=2 done=1 full=0 other=0\n'
                   'voice_delta_time epoch=7 first=1100 sentence=1200 final=1800\n'
                   'voice_delta_match epoch=7 bytes=100 sentence=40 prefix=1 equal=1 errors=0\n')
            log.write_text(lines)
            data=analyze(p)
            self.assertEqual(data['summary']['opportunity_median_ms'],600)
            self.assertNotIn('private',json.dumps(data))
            self.assertTrue(data['trials'][0]['bench_error'])
            log.write_text('\x1b[32m'+lines+'\x1b[0m')
            self.assertTrue(analyze(p)['trials'][0]['probe_complete'])
            log.write_text(lines.replace('prefix=1','prefix=0'))
            self.assertEqual(analyze(p)['summary']['opportunity_median_ms'],0)
            log.write_text(lines.splitlines()[0]+'\n')
            self.assertFalse(analyze(p)['trials'][0]['probe_complete'])
            self.assertIsNone(analyze(p)['summary']['opportunity_median_ms'])
            log.write_text(lines);turn['voice_ok']=False
            (p/'trials.json').write_text(json.dumps([turn]))
            self.assertEqual(analyze(p)['summary']['complete_successful_probes'],0)
            turn.update({'reply_mode':'incremental','text':1800,'tts':1400,
                         'pcm':480000,'stages':{'0':500}})
            (p/'trials.json').write_text(json.dumps([turn]))
            (p/'analysis.json').write_text(json.dumps({'trials':[{'index':1,
                'acoustic':{'acoustic_valid':False,'error':'private acoustic error'}}]}))
            log.write_text(lines+'voice_reply_mode epoch=7 incremental=1 segments=2 mismatches=0\n'
                           'voice_reply_mode epoch=8 incremental=0 segments=0 mismatches=9\n')
            data=export([p])
            self.assertEqual(data['summary']['attempts'],1)
            self.assertEqual(data['summary']['completed'],0)
            self.assertEqual(data['summary']['acoustic_valid'],0)
            self.assertEqual(data['trials'][0]['tts_submission_before_final_ms'],400)
            self.assertEqual(data['trials'][0]['reply_segments'],2)
            self.assertEqual(data['trials'][0]['reply_mismatches'],0)
            self.assertNotIn('private',json.dumps(data))


if __name__=='__main__':unittest.main()
