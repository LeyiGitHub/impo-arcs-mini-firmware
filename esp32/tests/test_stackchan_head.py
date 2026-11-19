# Copyright (c) Impo contributors.
# SPDX-License-Identifier: Apache-2.0
"""The StackChan head's hardware-free parts (components/impo/boards/stackchan_logic.h)
compiled on the host: Feetech SCS packets, angles to servo steps, head-touch
gestures and colours. Plus: the manifest names only commands the firmware has."""
from pathlib import Path
import json
import os
import shlex
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

HARNESS = r'''
#include <assert.h>
#include <stdio.h>
#include "stackchan_logic.h"

static touch_gesture_t feed(touch_state_t *s, uint8_t out) { return touch_update(s, out); }

int main(void)
{
    /* Read 2 bytes at 56 from servo 1: FF FF 01 04 02 38 02 ~(1+4+2+0x38+2). */
    uint8_t pkt[16];
    const uint8_t read[] = { SCS_POSITION, 2 };
    assert(scs_packet(pkt, 1, SCS_READ, read, 2) == 8);
    const uint8_t want[] = { 0xFF, 0xFF, 0x01, 0x04, 0x02, 0x38, 0x02, 0xBE };
    assert(!memcmp(pkt, want, sizeof(want)));
    /* Goal 620 for servo 2 in 20 ms: position big-endian, as SCSCL writes it. */
    const uint8_t goal[] = { SCS_GOAL, 620 >> 8, 620 & 0xFF, 0, 20, 0, 0 };
    assert(scs_packet(pkt, 2, SCS_WRITE, goal, 7) == 13);
    assert(pkt[3] == 9 && pkt[4] == SCS_WRITE && pkt[6] == 0x02 && pkt[7] == 0x6C);
    uint8_t sum = 0;
    for (int i = 2; i < 12; i++) sum += pkt[i];
    assert((uint8_t)~sum == pkt[12]);
    assert(scs_packet(pkt, 1, SCS_WRITE, goal, SCS_MAX_PARAMS + 1) == 0);
    /* A write's acknowledgement FF FF 01 02 00 FC. */
    assert(scs_reply_sum(1, 2, 0, NULL, 0) == 0xFC);

    /* 0.3125 degrees a step from the zero position, clamped to 0..1000. */
    assert(scs_angle_to_raw(460, 0) == 460);
    assert(scs_angle_to_raw(460, 1000) == 780);     /* 100 degrees */
    assert(scs_angle_to_raw(460, -1000) == 140);
    assert(scs_angle_to_raw(620, 3000) == 1000);
    assert(scs_raw_to_angle(460, 780) == 1000);
    assert(scs_raw_to_angle(620, 620) == 0);
    assert(ease(0) == 0 && ease(1) == 1 && ease(0.5f) == 0.5f && ease(2) == 1);

    /* Touch: a press needs two touched readings; release ends it. */
    touch_state_t s = { 0 };
    assert(feed(&s, 0x04) == TOUCH_NONE);           /* middle zone, once */
    assert(feed(&s, 0x00) == TOUCH_NONE);           /* noise: no press, no release */
    assert(feed(&s, 0x04) == TOUCH_NONE);
    assert(feed(&s, 0x04) == TOUCH_PRESS && !s.stroked);
    assert(feed(&s, 0x00) == TOUCH_RELEASE);
    /* A stroke: pressed on the back zone (0, position -100), then the hand
     * moves to the front zone (2, position 100). */
    assert(feed(&s, 0x03) == TOUCH_NONE);
    assert(feed(&s, 0x03) == TOUCH_PRESS);
    assert(feed(&s, 0x07) == TOUCH_NONE);           /* back 3, middle 1: -75, only 25 along */
    assert(feed(&s, 0x30) == TOUCH_STROKE && s.stroked);
    assert(feed(&s, 0x30) == TOUCH_NONE);           /* once per touch */
    assert(feed(&s, 0x00) == TOUCH_RELEASE);

    /* Colours. */
    uint32_t rgb;
    assert(colour_parse("Pink", &rgb) && rgb == 0xFF3070);
    assert(colour_parse("#00ff80", &rgb) && rgb == 0x00FF80);
    assert(colour_parse("102030", &rgb) && rgb == 0x102030);
    assert(!colour_parse("#12345", &rgb) && !colour_parse("mauve", &rgb) && !colour_parse("#12345g", &rgb));
    assert(colour_565(0xFFFFFF, 100) == 0xFFFF);
    assert(colour_565(0xFF0000, 100) == 0xF800);
    assert(colour_565(0xFFFFFF, 0) == 0);
    puts("ok");
    return 0;
}
'''


class StackChanHead(unittest.TestCase):
    def test_logic(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "harness.c"
            exe = Path(tmp) / "harness"
            src.write_text(HARNESS)
            cc = shlex.split(os.environ.get("CC", "cc"))
            subprocess.run(cc + ["-std=c11", "-Wall", "-Werror", "-I", str(ROOT / "components/impo/boards"),
                                 str(src), "-o", str(exe)], check=True)
            self.assertEqual(subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout, "ok\n")

    def test_manifest_matches_the_firmware(self):
        made = subprocess.run([sys.executable, str(ROOT / "tools/stackchan_manifest.py")],
                              capture_output=True, text=True)
        self.assertEqual(made.returncode, 0, made.stderr)
        manifest = json.loads(made.stdout)
        self.assertLessEqual(len(manifest["systemPrompt"]), 4000)
        names = [c["name"] for c in manifest["commands"]]
        for name in ("head.look", "head.nod", "lights.set", "speaker.say", "device.health"):
            self.assertIn(name, names)
        self.assertEqual([e["name"] for e in manifest["events"]], ["head_touched"])
        source = (ROOT / "components/impo/boards/stackchan_head.c").read_text()
        self.assertIn('impo_link_send_event("head_touched"', source)


if __name__ == "__main__":
    unittest.main()
