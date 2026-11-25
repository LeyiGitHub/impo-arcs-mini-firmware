import json
from pathlib import Path
import sys
import tempfile
import unittest
import wave
from unittest.mock import patch
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from playback_bench import snapshot
from analyze_playback_bench import acoustic, export

class PlaybackBenchTests(unittest.TestCase):
    def test_snapshot_requires_complete_lines(self):
        text='''playback gen=7 active=0 failed=0 cancelled=0 adaptive=1 sentence=1
playback samples played=160000 queued=0 underruns=1 gap_ms=200 prefill_ms=1000
playback ingress high=8192 overflow=0 bytes=480000 stack=2300 segments=2/2
playback timing request=100 body=120 pcm=130 first=180 last=12000'''
        row=snapshot(text)
        self.assertEqual(row['segments_requested'],2)
        self.assertEqual(row['underrun_ms'],200)
        self.assertEqual(row['first_write_ms'],180)
        with self.assertRaises(ValueError):snapshot(text.split('playback timing')[0])

    def test_public_export_retains_failure_and_excludes_private_fields(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            (root/'manifest.json').write_text(json.dumps({'text':'private answer',
                'boot':'private boot','test_volume':20,'prefill_ms':1200,
                'modes':['adaptive'],'image':{'image_sha256':'abc','serial':'private serial'}}))
            (root/'trials.json').write_text(json.dumps([
                {'index':1,'mode':'adaptive','ok':True,'underruns':0,'error':'',
                 'command_host_mono':123,'token':'private token'},
                {'index':2,'mode':'adaptive','ok':False,'error':'private error'}]))
            with patch('analyze_playback_bench.acoustic',return_value={'valid':False}):
                result=export(root)
        self.assertEqual(result['summary']['adaptive']['attempts'],2)
        self.assertEqual(result['summary']['adaptive']['failed_or_unknown'],1)
        self.assertEqual(result['configured_prefill_ms'],1200)
        self.assertTrue(result['trials'][1]['bench_error'])
        self.assertNotIn('private',json.dumps(result))
        self.assertNotIn('command_host_mono',result['trials'][0])

    def test_noise_before_driver_start_is_ignored(self):
        rate=8000
        data=np.full(rate*6,20,dtype=np.int16)
        data[rate:rate+800]=2000 # unrelated noise before playback
        data[int(4.2*rate):int(4.8*rate)]=3000
        row={'command_host_mono':.5,'record_host_mono':0,'first_write_host_mono':4.0}
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'test.wav'
            with wave.open(str(path),'wb') as w:
                w.setnchannels(1);w.setsampwidth(2);w.setframerate(rate);w.writeframes(data.tobytes())
            result=acoustic(row,path)
        self.assertTrue(result['valid'])
        self.assertEqual(result['command_to_sound_ms'],3700)

if __name__=='__main__':unittest.main()
