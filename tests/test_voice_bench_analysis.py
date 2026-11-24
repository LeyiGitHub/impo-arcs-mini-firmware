"""Optional bench-analysis checks (numpy), with independently timed synthetic audio."""
import sys
from pathlib import Path
import unittest
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from analyze_voice_bench import match_reference,measure

class AcousticTests(unittest.TestCase):
    def test_offset_with_gain_noise_and_room_echo(self):
        rng=np.random.default_rng(38)
        ref=rng.normal(0,1000,8000)
        recording=rng.normal(0,15,30000)
        recording[5300:13300]+=ref*.3
        recording[5380:13380]+=ref*.1
        offset,score=match_reference(recording,ref)
        self.assertEqual(offset,5300)
        self.assertGreater(score,.8)

    def test_silent_reference_is_rejected(self):
        with self.assertRaises(ValueError):
            match_reference(np.ones(1000),np.zeros(100))

    def test_gap_and_truncated_capture(self):
        rng=np.random.default_rng(5)
        rate=8000
        ref=rng.normal(0,1000,rate)
        x=rng.normal(0,10,rate*8)
        x[rate:rate*2]+=ref
        # A loud endpoint beep at 3 seconds must not count as the answer.
        x[rate*3:rate*3+800]+=1000*np.sin(np.arange(800)*.4)
        x[rate*5:rate*5+4000]+=1000*np.sin(np.arange(4000)*.2)
        row={"record_host_mono":100.,"stimulus_host_mono":101.,"ready_host_mono":105.,
             "capture_host_mono":103.2,"capture":4200,"stages":{"0":3000}}
        r=measure(row,x,rate,ref,rate)
        self.assertTrue(r["acoustic_valid"])
        self.assertLess(abs(r["acoustic_gap_ms"]-3000),15)
        shifted=dict(row)
        for k in ["record_host_mono","stimulus_host_mono","ready_host_mono","capture_host_mono"]:
            shifted[k]+=12345.
        self.assertEqual(measure(shifted,x,rate,ref,rate)["acoustic_gap_ms"],r["acoustic_gap_ms"])
        endpoint=dict(row,endpoint_only=True)
        self.assertTrue(measure(endpoint,x,rate,ref,rate)["stimulus_tail_seen"])
        row["stages"]["0"]=2000
        self.assertFalse(measure(row,x,rate,ref,rate)["acoustic_valid"])
        self.assertFalse(measure(dict(row,endpoint_only=True),x,rate,ref,rate)["stimulus_tail_seen"])

if __name__=="__main__":unittest.main()
