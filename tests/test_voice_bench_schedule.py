"""A failed/unknown request must not be silently resubmitted during a bench resume."""
import sys
from pathlib import Path
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from voice_latency_bench import pending_trials,parse_snapshot


class ScheduleTests(unittest.TestCase):
    def test_short_snapshot_does_not_require_long_turn_log(self):
        text="""voice_turn boot=1234 epoch=134 session=3 (truncated)
voice phase=0 boot=1234 epoch=134 session=3 overlap=0 valid=87 failed=0
voice bytes pcm=235520 queued=0 backlog=235564 dropped=0 upload_stack_free=2776
voice stage=0 mono_ms=2228746
voice stage=1 mono_ms=2229945
voice stage=7 mono_ms=2229990
"""
        row=parse_snapshot(text)
        self.assertEqual(row["capture"],2229945)
        self.assertEqual(row["stages"]["0"],2228746)
        self.assertEqual(row["valid"],"87")

    def test_resume_skips_all_attempts_including_failed_slot(self):
        modes=[("batch",1200),("overlap",1200)]
        planned=list(pending_trials(modes,30,set()))
        attempted=set(planned[:19])  # Last attempt failed during upload.
        remaining=list(pending_trials(modes,30,attempted))
        self.assertEqual(remaining[0],(10,"batch",1200))
        self.assertEqual(len(remaining),41)
        self.assertFalse(attempted.intersection(remaining))
        self.assertEqual(len(set(planned)),60)

    def test_every_cycle_includes_each_mode_once(self):
        modes=[("batch",1200),("overlap",1200),("overlap",800)]
        rows=list(pending_trials(modes,6,set()))
        for cycle in range(1,7):
            self.assertEqual({(m,p) for c,m,p in rows if c==cycle},set(modes))
        self.assertEqual(len({tuple((m,p) for c,m,p in rows if c==cycle) for cycle in range(1,7)}),6)


if __name__=="__main__":unittest.main()
